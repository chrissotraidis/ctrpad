#include <platform.h>

#include <macros.h>

#include "platform/native_audio.h"
#include "platform/native_glad.h"
#include "platform/native_gpu.h"
#include "platform/native_input.h"
#include "platform/native_log.h"
#if defined(CTR_NATIVE_MACOS_BUNDLE)
#include "platform/native_macos_controls.h"
#endif
#include "platform/native_perf.h"
#include "platform/native_renderer.h"
#include "platform/native_replay_scheduler.h"
#include "platform/native_savestate.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Window *g_window = NULL;
int g_dbg_polygonSelected = 0;

extern int g_cfg_bilinearFiltering;
extern int g_dbg_emulatorPaused;
extern int g_dbg_texturelessMode;
extern int g_dbg_wireframeMode;
extern int g_windowHeight;
extern int g_windowWidth;

#define HOST_ALT_LEFT  (1 << 0)
#define HOST_ALT_RIGHT (1 << 1)
global_variable int s_hostAltKeyState = 0;
global_variable int s_platformInitialized = 0;
global_variable int s_platformBeginScene = 0;
global_variable int s_pinnedVramDisplayFrames = 0;
global_variable int s_pinnedVramDisplayCustomRect = 0;
global_variable int s_pinnedVramDisplayX = 0;
global_variable int s_pinnedVramDisplayY = 0;
global_variable int s_pinnedVramDisplayW = 0;
global_variable int s_pinnedVramDisplayH = 0;
#if defined(SDL_PLATFORM_IOS)
// Keep Simulator/device cadence observable during short lifecycle runs.
#define NATIVE_FPS_REPORT_FRAME_WINDOW 120
#else
#define NATIVE_FPS_REPORT_FRAME_WINDOW 2000
#endif
global_variable int s_fpsFrameCount = 0;
global_variable u64 s_fpsLastCounter = 0;
global_variable u64 s_fpsPreviousCounter = 0;
global_variable u64 s_fpsFrameDurations[NATIVE_FPS_REPORT_FRAME_WINDOW];

struct NativeFrameStats
{
	f64 meanMilliseconds;
	f64 medianMilliseconds;
	f64 p95Milliseconds;
	f64 p99Milliseconds;
	f64 maxMilliseconds;
	f64 framesPerSecond;
};

enum NativeLifecyclePhase
{
	NATIVE_LIFECYCLE_ACTIVE = 0,
	NATIVE_LIFECYCLE_WILL_ENTER_BACKGROUND,
	NATIVE_LIFECYCLE_BACKGROUND,
	NATIVE_LIFECYCLE_WILL_ENTER_FOREGROUND,
	NATIVE_LIFECYCLE_TERMINATING,
};

struct NativeLifecycleStatus
{
	enum NativeLifecyclePhase phase;
	int outputSuspended;
	int quitRequested;
};

struct NativeLifecycleActions
{
	int suspendOutput;
	int resumeOutput;
	int resetInput;
	int resumeInput;
	int rebaseVBlankClock;
	int flushLog;
};

global_variable struct NativeLifecycleStatus s_lifecycleStatus = {NATIVE_LIFECYCLE_ACTIVE, 0, 0};
global_variable int s_lifecycleEventWatchInstalled = 0;

static int s_uiOverlayPaused;
internal void Native_RebaseVBlankClock(void);

internal void NativeFrameStats_Reset(void)
{
	s_fpsFrameCount = 0;
	s_fpsLastCounter = 0;
	s_fpsPreviousCounter = 0;
}

internal int NativeFrameStats_CompareCounters(const void *left, const void *right)
{
	const u64 leftValue = *(const u64 *)left;
	const u64 rightValue = *(const u64 *)right;

	return (leftValue > rightValue) - (leftValue < rightValue);
}

internal int NativeFrameStats_PercentileIndex(int sampleCount, int percentile)
{
	return (((sampleCount * percentile) + 99) / 100) - 1;
}

internal int NativeFrameStats_Calculate(const u64 *durations, int sampleCount, u64 frequency, struct NativeFrameStats *stats)
{
	u64 sortedDurations[NATIVE_FPS_REPORT_FRAME_WINDOW];
	f64 totalTicks = 0.0;
	f64 medianTicks;
	f64 millisecondsPerTick;

	if ((durations == NULL) || (stats == NULL) || (sampleCount <= 0) ||
	    (sampleCount > NATIVE_FPS_REPORT_FRAME_WINDOW) || (frequency == 0))
	{
		return 0;
	}

	memcpy(sortedDurations, durations, (size_t)sampleCount * sizeof(sortedDurations[0]));
	qsort(sortedDurations, (size_t)sampleCount, sizeof(sortedDurations[0]), NativeFrameStats_CompareCounters);
	for (int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
	{
		totalTicks += (f64)sortedDurations[sampleIndex];
	}

	if ((sampleCount & 1) != 0)
	{
		medianTicks = (f64)sortedDurations[sampleCount / 2];
	}
	else
	{
		medianTicks = ((f64)sortedDurations[(sampleCount / 2) - 1] + (f64)sortedDurations[sampleCount / 2]) * 0.5;
	}

	millisecondsPerTick = 1000.0 / (f64)frequency;
	stats->meanMilliseconds = (totalTicks / (f64)sampleCount) * millisecondsPerTick;
	stats->medianMilliseconds = medianTicks * millisecondsPerTick;
	stats->p95Milliseconds = (f64)sortedDurations[NativeFrameStats_PercentileIndex(sampleCount, 95)] * millisecondsPerTick;
	stats->p99Milliseconds = (f64)sortedDurations[NativeFrameStats_PercentileIndex(sampleCount, 99)] * millisecondsPerTick;
	stats->maxMilliseconds = (f64)sortedDurations[sampleCount - 1] * millisecondsPerTick;
	stats->framesPerSecond = stats->meanMilliseconds > 0.0 ? 1000.0 / stats->meanMilliseconds : 0.0;
	return 1;
}

internal const char *NativeLifecycle_PhaseName(enum NativeLifecyclePhase phase)
{
	switch (phase)
	{
	case NATIVE_LIFECYCLE_ACTIVE:
		return "active";
	case NATIVE_LIFECYCLE_WILL_ENTER_BACKGROUND:
		return "will-background";
	case NATIVE_LIFECYCLE_BACKGROUND:
		return "background";
	case NATIVE_LIFECYCLE_WILL_ENTER_FOREGROUND:
		return "will-foreground";
	case NATIVE_LIFECYCLE_TERMINATING:
		return "terminating";
	}

	return "unknown";
}

internal const char *NativeLifecycle_EventName(Uint32 eventType)
{
	switch (eventType)
	{
	case SDL_EVENT_WILL_ENTER_BACKGROUND:
		return "will-enter-background";
	case SDL_EVENT_DID_ENTER_BACKGROUND:
		return "did-enter-background";
	case SDL_EVENT_WILL_ENTER_FOREGROUND:
		return "will-enter-foreground";
	case SDL_EVENT_DID_ENTER_FOREGROUND:
		return "did-enter-foreground";
	case SDL_EVENT_TERMINATING:
		return "terminating";
	case SDL_EVENT_LOW_MEMORY:
		return "low-memory";
	default:
		return "other";
	}
}

internal struct NativeLifecycleActions NativeLifecycle_Reduce(struct NativeLifecycleStatus *status, Uint32 eventType)
{
	struct NativeLifecycleActions actions;

	memset(&actions, 0, sizeof(actions));
	if (status == NULL)
	{
		return actions;
	}

	switch (eventType)
	{
	case SDL_EVENT_WILL_ENTER_BACKGROUND:
		if (status->phase == NATIVE_LIFECYCLE_TERMINATING)
		{
			break;
		}
		status->phase = NATIVE_LIFECYCLE_WILL_ENTER_BACKGROUND;
		if (status->outputSuspended == 0)
		{
			status->outputSuspended = 1;
			actions.suspendOutput = 1;
			actions.resetInput = 1;
		}
		actions.flushLog = 1;
		break;

	case SDL_EVENT_DID_ENTER_BACKGROUND:
		if (status->phase == NATIVE_LIFECYCLE_TERMINATING)
		{
			break;
		}
		status->phase = NATIVE_LIFECYCLE_BACKGROUND;
		if (status->outputSuspended == 0)
		{
			status->outputSuspended = 1;
			actions.suspendOutput = 1;
			actions.resetInput = 1;
		}
		actions.flushLog = 1;
		break;

	case SDL_EVENT_WILL_ENTER_FOREGROUND:
		if (status->phase != NATIVE_LIFECYCLE_TERMINATING)
		{
			status->phase = NATIVE_LIFECYCLE_WILL_ENTER_FOREGROUND;
		}
		break;

	case SDL_EVENT_DID_ENTER_FOREGROUND:
		if ((status->phase != NATIVE_LIFECYCLE_TERMINATING) &&
		    ((status->phase != NATIVE_LIFECYCLE_ACTIVE) || (status->outputSuspended != 0)))
		{
			status->phase = NATIVE_LIFECYCLE_ACTIVE;
			status->outputSuspended = 0;
			actions.resumeInput = 1;
			actions.rebaseVBlankClock = 1;
			actions.resumeOutput = 1;
		}
		break;

	case SDL_EVENT_TERMINATING:
		status->phase = NATIVE_LIFECYCLE_TERMINATING;
		status->quitRequested = 1;
		if (status->outputSuspended == 0)
		{
			status->outputSuspended = 1;
			actions.suspendOutput = 1;
			actions.resetInput = 1;
		}
		actions.flushLog = 1;
		break;

	case SDL_EVENT_LOW_MEMORY:
		actions.flushLog = 1;
		break;

	default:
		break;
	}

	return actions;
}

internal void NativeLifecycle_ApplyEvent(Uint32 eventType)
{
	const enum NativeLifecyclePhase previousPhase = s_lifecycleStatus.phase;
	const struct NativeLifecycleActions actions = NativeLifecycle_Reduce(&s_lifecycleStatus, eventType);

	if (actions.resetInput != 0)
	{
		s_hostAltKeyState = 0;
		Platform_InputSuspend();
	}
	if ((actions.suspendOutput != 0) && !NativeAudio_SuspendOutput())
	{
		Platform_LogError("[CTR Lifecycle] failed to suspend audio: %s\n", SDL_GetError());
	}
	if ((actions.resumeInput != 0) && !s_uiOverlayPaused)
	{
		Platform_InputResume();
	}
	if (actions.rebaseVBlankClock != 0)
	{
		Native_RebaseVBlankClock();
		NativeFrameStats_Reset();
	}
	if ((actions.resumeOutput != 0) && !s_uiOverlayPaused && !NativeAudio_ResumeOutput())
	{
		Platform_LogError("[CTR Lifecycle] failed to resume audio: %s\n", SDL_GetError());
	}
	if ((previousPhase != s_lifecycleStatus.phase) || (eventType == SDL_EVENT_LOW_MEMORY))
	{
		Platform_Log("[CTR Lifecycle] event=%s phase=%s audio=%s quit=%d\n", NativeLifecycle_EventName(eventType),
		             NativeLifecycle_PhaseName(s_lifecycleStatus.phase), s_lifecycleStatus.outputSuspended ? "suspended" : "active",
		             s_lifecycleStatus.quitRequested);
	}
	if (actions.flushLog != 0)
	{
		Platform_LogFlush();
	}
}

internal bool SDLCALL NativeLifecycle_EventWatch(void *userdata, SDL_Event *event)
{
	(void)userdata;

	if (event != NULL)
	{
		NativeLifecycle_ApplyEvent(event->type);
	}
	return true;
}

internal void Platform_CalcFPS(void)
{
#if defined(CTR_INTERNAL)
	const u64 freq = SDL_GetPerformanceFrequency();
	const u64 now = SDL_GetPerformanceCounter();

	if (freq == 0)
	{
		return;
	}

	if (s_fpsLastCounter == 0)
	{
		s_fpsLastCounter = now;
		s_fpsPreviousCounter = now;
		s_fpsFrameCount = 0;
		return;
	}

	if (now <= s_fpsPreviousCounter)
	{
		NativeFrameStats_Reset();
		s_fpsLastCounter = now;
		s_fpsPreviousCounter = now;
		return;
	}

	s_fpsFrameDurations[s_fpsFrameCount] = now - s_fpsPreviousCounter;
	s_fpsPreviousCounter = now;
	s_fpsFrameCount++;
	if (s_fpsFrameCount < NATIVE_FPS_REPORT_FRAME_WINDOW)
	{
		return;
	}

	if (now > s_fpsLastCounter)
	{
		struct NativeFrameStats stats;
		const f64 elapsedSeconds = (f64)(now - s_fpsLastCounter) / (f64)freq;
		const f64 fps = (f64)s_fpsFrameCount / elapsedSeconds;

		Platform_Log("[CTR Native] FPS: %.2f (last %d frames)\n", fps, s_fpsFrameCount);
		if (NativeFrameStats_Calculate(s_fpsFrameDurations, s_fpsFrameCount, freq, &stats))
		{
			Platform_Log("[CTR FrameStats] frames=%d mean_ms=%.3f median_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f fps=%.2f\n",
			             s_fpsFrameCount, stats.meanMilliseconds, stats.medianMilliseconds, stats.p95Milliseconds,
			             stats.p99Milliseconds, stats.maxMilliseconds, stats.framesPerSecond);
		}
	}

	NativeFrameStats_Reset();
	s_fpsLastCounter = now;
	s_fpsPreviousCounter = now;
#endif
}

internal void Platform_GetWindowName(const char *appName, char *buffer, size_t bufferSize)
{
#ifdef CTR_INTERNAL
	snprintf(buffer, bufferSize, "%s | Internal", appName);
#else
	snprintf(buffer, bufferSize, "%s", appName);
#endif
}

internal void Platform_HandleWindowResize(int width, int height)
{
	int pixelWidth = width;
	int pixelHeight = height;

	if ((g_window != NULL) && !SDL_GetWindowSizeInPixels(g_window, &pixelWidth, &pixelHeight))
	{
		pixelWidth = width;
		pixelHeight = height;
	}

	g_windowWidth = pixelWidth;
	g_windowHeight = pixelHeight;
	NativeRenderer_ResetDevice();
}

internal void Platform_UpdateCursorVisibility(void)
{
	if (g_window == NULL)
	{
		return;
	}

	if ((SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0)
	{
		SDL_HideCursor();
	}
	else
	{
		SDL_ShowCursor();
	}
}

internal void Platform_HandleFullscreenToggle(void)
{
	int fullscreen = (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0;

	SDL_SetWindowFullscreen(g_window, fullscreen == 0);
	if (!SDL_GetWindowSizeInPixels(g_window, &g_windowWidth, &g_windowHeight))
	{
		SDL_GetWindowSize(g_window, &g_windowWidth, &g_windowHeight);
	}
	Platform_UpdateCursorVisibility();
	NativeRenderer_ResetDevice();
}

internal void Platform_UpdateHostAltKeyState(const s32 key, const s8 down)
{
	s32 altKeyBit = 0;

	if (key == SDL_SCANCODE_LALT)
	{
		altKeyBit = HOST_ALT_LEFT;
	}
	else if (key == SDL_SCANCODE_RALT)
	{
		altKeyBit = HOST_ALT_RIGHT;
	}

	if (altKeyBit == 0)
	{
		return;
	}

	if (down != 0)
	{
		s_hostAltKeyState |= altKeyBit;
	}
	else
	{
		s_hostAltKeyState &= ~altKeyBit;
	}
}

#if defined(CTR_INTERNAL)
internal void Platform_TakeScreenshot(void)
{
	u8 *pixels = (u8 *)malloc(g_windowWidth * g_windowHeight * 4);

	glReadPixels(0, 0, g_windowWidth, g_windowHeight, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

	SDL_Surface *surface = SDL_CreateSurfaceFrom(g_windowWidth, g_windowHeight, SDL_PIXELFORMAT_BGRA8888, pixels, g_windowWidth * 4);

	SDL_SaveBMP(surface, "SCREENSHOT.BMP");
	SDL_DestroySurface(surface);

	free(pixels);
}
#endif

internal void Platform_HandleKey(int key, char down)
{
	if (down == 0)
	{
		SubmitName_UseKeyboard(0);
	}
	else
	{
		SubmitName_UseKeyboard(key);
	}

#ifdef CTR_INTERNAL
	if (!down)
	{
		switch (key)
		{
		case SDL_SCANCODE_F1:
			g_dbg_wireframeMode ^= 1;
			Platform_LogWarn("[CTR Native] wireframe mode: %d\n", g_dbg_wireframeMode);
			break;

		case SDL_SCANCODE_F2:
			g_dbg_texturelessMode ^= 1;
			Platform_LogWarn("[CTR Native] textureless mode: %d\n", g_dbg_texturelessMode);
			break;
		case SDL_SCANCODE_UP:
		case SDL_SCANCODE_DOWN:
			if (g_dbg_emulatorPaused)
			{
				g_dbg_polygonSelected += (key == SDL_SCANCODE_UP) ? 3 : -3;
			}
			break;
		case SDL_SCANCODE_F9:
			if (NativeReplayScheduler_RequestStart() != 0)
			{
				break;
			}
			break;
		case SDL_SCANCODE_F10:
			NativeReplayScheduler_RequestStop();
			break;
		case SDL_SCANCODE_F7:
			Platform_LogWarn("[CTR Native] saving VRAM.TGA\n");
			NativeRenderer_SaveVRAM("VRAM.TGA", 0, 0, VRAM_WIDTH, VRAM_HEIGHT, 1);
			break;
		case SDL_SCANCODE_F12:
			Platform_LogWarn("[CTR Native] Saving screenshot...\n");
			Platform_TakeScreenshot();
			break;
		case SDL_SCANCODE_F3:
			g_cfg_bilinearFiltering ^= 1;
			Platform_LogWarn("[CTR Native] filtering mode: %d\n", g_cfg_bilinearFiltering);
			break;
		case SDL_SCANCODE_F5:
			NativeSaveState_RequestSave();
			break;
		case SDL_SCANCODE_F8:
			NativeSaveState_RequestLoad();
			break;
		}
	}
#endif
}

void Platform_Init(const char *title, int width, int height)
{
	char windowName[128];

	s_lifecycleStatus.phase = NATIVE_LIFECYCLE_ACTIVE;
	s_lifecycleStatus.outputSuspended = 0;
	s_lifecycleStatus.quitRequested = 0;
	s_lifecycleEventWatchInstalled = 0;

	Platform_LogInit(title);
	Platform_GetWindowName(title, windowName, sizeof(windowName));

	Platform_Log("[CTR Native] Initialising platform\n");

#if defined(SDL_PLATFORM_IOS)
	// UIKit intersects this hint with the target-specific Info.plist mask.
	// iPhone therefore remains landscape-only, while iPad can honor its declared
	// portrait and dynamically resized scenes instead of clipping a landscape
	// controller view inside portrait window bounds.
	SDL_SetHint(SDL_HINT_ORIENTATIONS,
	            "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight");
#endif

	if (SDL_Init(SDL_INIT_VIDEO) == 0)
	{
		Platform_LogError("[CTR Native] Failed to initialise SDL: %s\n", SDL_GetError());
		Platform_LogShutdown();
		return;
	}

	s_platformInitialized = 1;
	s_uiOverlayPaused = 0;
	if (!SDL_AddEventWatch(NativeLifecycle_EventWatch, NULL))
	{
		Platform_LogError("[CTR Native] Failed to install lifecycle event watch: %s\n", SDL_GetError());
		Platform_Shutdown();
		return;
	}
	s_lifecycleEventWatchInstalled = 1;

	if (!NativeRenderer_InitialiseRender(windowName, width, height, 0))
	{
		Platform_LogError("[CTR Native] Failed to initialise window\n");
		Platform_Shutdown();
		return;
	}

	if (!NativeRenderer_InitialisePSX())
	{
		Platform_LogError("[CTR Native] Failed to initialise PSX renderer state\n");
		Platform_Shutdown();
		return;
	}

	atexit(Platform_Shutdown);
	Platform_UpdateCursorVisibility();
	Platform_InputInit();
#if defined(CTR_NATIVE_MACOS_BUNDLE)
	NativeMacOSControls_Install(g_window);
#endif
}

int Platform_IsInitialized(void)
{
	return s_platformInitialized;
}

int Platform_IsHostActive(void)
{
	return s_lifecycleStatus.phase == NATIVE_LIFECYCLE_ACTIVE;
}

void Platform_SetUIOverlayPaused(int paused)
{
    paused = paused != 0;
    if (paused == s_uiOverlayPaused) return;
    s_uiOverlayPaused = paused;
    if (paused)
    {
        Platform_InputSuspend();
        if (!NativeAudio_SuspendOutput()) Platform_LogWarn("[CTR Options] audio suspension failed\n");
    }
    else
    {
        Native_RebaseVBlankClock();
        NativeFrameStats_Reset();
        if (Platform_IsHostActive())
        {
            Platform_InputResume();
            if (!NativeAudio_ResumeOutput()) Platform_LogWarn("[CTR Options] audio resume failed\n");
        }
    }
    Platform_Log("[CTR Options] runtime_paused=%d\n", paused);
}

int Platform_ShouldQuit(void)
{
	return s_lifecycleStatus.quitRequested;
}

void Platform_Shutdown(void)
{
	if (s_platformInitialized == 0)
	{
		return;
	}

	s_platformInitialized = 0;
	if (s_lifecycleEventWatchInstalled != 0)
	{
		SDL_RemoveEventWatch(NativeLifecycle_EventWatch, NULL);
		s_lifecycleEventWatchInstalled = 0;
	}
#if defined(CTR_INTERNAL)
	NativeRenderer_FinishGpuMeasurements();
	NativePerf_Shutdown();
	NativeReplayScheduler_Shutdown();
#endif
#if defined(CTR_NATIVE_MACOS_BUNDLE)
	NativeMacOSControls_Shutdown();
#endif
	Platform_InputShutdown();
	NativeAudio_Shutdown();
	NativeRenderer_Shutdown();

	if (g_window != NULL)
	{
		SDL_DestroyWindow(g_window);
		g_window = NULL;
	}

	SDL_Quit();

	Platform_LogShutdown();
}

void Platform_BeginFrame(void)
{
	// NOTE(aalhendi): Normal rendering begins from DrawOTag after the current
	// draw env is installed. Starting a host scene here clears the previous env
	// and can force the host GL driver to block before the retail render-submit path.
}

int Platform_BeginScene(void)
{
	if (s_platformBeginScene)
	{
		return 0;
	}

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_PLATFORM_BEGIN_SCENE);
	// NOTE(aalhendi): CTR already throttles through the retail VSync/draw-sync
	// path. Do not add a second SDL swap wait; some GL drivers charge that wait
	// to the next frame's first clear instead of SDL_GL_SwapWindow.
	NativeRenderer_UpdateSwapIntervalState(0);

	NativeRenderer_BeginScene();

	if (activeDrawEnv.isbg)
	{
		const RECT16 clipenv = activeDrawEnv.clip;
		const u8 r = activeDrawEnv.r0;
		const u8 g = activeDrawEnv.g0;
		const u8 b = activeDrawEnv.b0;

		NativeRenderer_Clear(clipenv.x, clipenv.y, clipenv.w, clipenv.h, r, g, b);
	}

	s_platformBeginScene = 1;

	Platform_LogFlush();

	NativePerf_EndScope(NATIVE_PERF_BUCKET_PLATFORM_BEGIN_SCENE);
	return 1;
}

void Platform_EndScene(void)
{
	if (!s_platformBeginScene)
	{
		return;
	}

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_PLATFORM_END_SCENE);
	s_platformBeginScene = 0;

	NativeRenderer_EndScene();

	if (s_pinnedVramDisplayFrames > 0)
	{
		if (s_pinnedVramDisplayCustomRect)
		{
			NativeRenderer_PresentVRAMRect(s_pinnedVramDisplayX, s_pinnedVramDisplayY, s_pinnedVramDisplayW, s_pinnedVramDisplayH);
		}
		else
		{
			NativeRenderer_PresentVRAMDisplay();
		}
		NativeRenderer_EndGpuFrame();
		NativeRenderer_SwapWindow();
		s_pinnedVramDisplayFrames--;
		if (s_pinnedVramDisplayFrames <= 0)
		{
			s_pinnedVramDisplayCustomRect = 0;
		}
		NativePerf_EndScope(NATIVE_PERF_BUCKET_PLATFORM_END_SCENE);
		return;
	}

	// NOTE(aalhendi): Keep the displayed VRAM region current for screen-copy
	// effects without forcing a CPU readback.
	NativeRenderer_StoreFrameBuffer(activeDispEnv.disp.x, activeDispEnv.disp.y, activeDispEnv.disp.w, activeDispEnv.disp.h);
	NativeRenderer_PresentMainRenderTarget();
	NativeRenderer_EndGpuFrame();
	NativeRenderer_SwapWindow();
	NativePerf_EndScope(NATIVE_PERF_BUCKET_PLATFORM_END_SCENE);
}

// NOTE(aalhendi): Frame timing is handled by VSync() in the platform layer,
// matching PS1 hardware behavior. Platform_EndFrame only does buffer swap + FPS.
void Platform_EndFrame(void)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_PLATFORM_END_FRAME);
	Platform_EndScene();
	Platform_CalcFPS();
	NativePerf_EndScope(NATIVE_PERF_BUCKET_PLATFORM_END_FRAME);
}

void Platform_PresentVRAMDisplay(void)
{
	Platform_PinVRAMDisplayFrames(1);
	Platform_BeginScene();
	Platform_EndFrame();
}

void Platform_PinVRAMDisplayFrames(int frameCount)
{
	if (frameCount > s_pinnedVramDisplayFrames)
	{
		s_pinnedVramDisplayFrames = frameCount;
		s_pinnedVramDisplayCustomRect = 0;
	}
}

void Platform_PinVRAMDisplayRect(int x, int y, int w, int h, int frameCount)
{
	if ((frameCount <= 0) || (w <= 0) || (h <= 0))
	{
		return;
	}

	s_pinnedVramDisplayX = x;
	s_pinnedVramDisplayY = y;
	s_pinnedVramDisplayW = w;
	s_pinnedVramDisplayH = h;
	s_pinnedVramDisplayFrames = frameCount;
	s_pinnedVramDisplayCustomRect = 1;
}

void Platform_PollHostEvents(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_EVENT_GAMEPAD_ADDED:
			Platform_InputControllerAdded(event.gdevice.which);
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			Platform_InputControllerRemoved(event.gdevice.which);
			break;
		case SDL_EVENT_QUIT:
			s_lifecycleStatus.quitRequested = 1;
			Platform_Log("[CTR Lifecycle] cooperative quit requested by SDL\n");
			break;
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			Platform_HandleWindowResize(event.window.data1, event.window.data2);
			break;
		case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
		case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
			Platform_UpdateCursorVisibility();
			break;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			s_lifecycleStatus.quitRequested = 1;
			Platform_Log("[CTR Lifecycle] cooperative quit requested by window\n");
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			Platform_InputMouseButtonEvent(event.button.button,
			                                   event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1 : 0);
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			int key = event.key.scancode;
			char down = (event.type == SDL_EVENT_KEY_UP) ? 0 : 1;

			Platform_UpdateHostAltKeyState(key, down);

			if (key == SDL_SCANCODE_F11)
			{
				if ((down != 0) && (event.key.repeat == 0))
				{
					Platform_HandleFullscreenToggle();
				}
				break;
			}

			// Preserve a complete quick key tap until the next retail pad
			// snapshot. Host Alt shortcuts must never leak into game input.
			if (s_hostAltKeyState == 0)
			{
				Platform_InputKeyboardEvent(key, down);
			}

			if (key == SDL_SCANCODE_RETURN)
			{
				if ((s_hostAltKeyState != 0) && (down != 0) && (event.key.repeat == 0))
				{
					Platform_HandleFullscreenToggle();
				}
				break;
			}

			if (key == SDL_SCANCODE_RSHIFT)
			{
				key = SDL_SCANCODE_LSHIFT;
			}
			else if (key == SDL_SCANCODE_RCTRL)
			{
				key = SDL_SCANCODE_LCTRL;
			}
			else if (key == SDL_SCANCODE_RALT)
			{
				key = SDL_SCANCODE_LALT;
			}

			if ((key == SDL_SCANCODE_F4) && (down == 0))
			{
#ifdef CTR_INTERNAL
				Platform_LogWarn("[CTR Native] Keyboard assigned to player %d\n", Platform_InputCycleKeyboardController());
#endif
				break;
			}

			if ((key == SDL_SCANCODE_F6) && (down == 0))
			{
#ifdef CTR_INTERNAL
				int player = Platform_InputCycleGamepadController();
				if (player == 0)
				{
					Platform_LogWarn("[CTR Native] No gamepad connected\n");
				}
				else
				{
					Platform_LogWarn("[CTR Native] Gamepad assigned to player %d\n", player);
				}
#endif
				break;
			}

			Platform_HandleKey(key, down);
			break;
		}
		}
	}
}

int Platform_PollInput(void)
{
	Platform_PollHostEvents();
	Platform_InputUpdate();
	return 1;
}

int Platform_StartDisplayLoop(void (*callback)(void *), void *userdata)
{
#if defined(SDL_PLATFORM_IOS)
	if ((g_window == NULL) || (callback == NULL))
	{
		return 0;
	}
	return SDL_SetiOSAnimationCallback(g_window, 1, callback, userdata) ? 1 : 0;
#else
	(void)callback;
	(void)userdata;
	return 0;
#endif
}

void Platform_StopDisplayLoop(void)
{
#if defined(SDL_PLATFORM_IOS)
	if (g_window != NULL)
	{
		SDL_SetiOSAnimationCallback(g_window, 1, NULL, NULL);
	}
#endif
}

int NikoGetEnterKey(void)
{
	return Platform_InputGetSubmitNameKey() == SDL_SCANCODE_RETURN;
}

// NOTE(aalhendi): VSyncCallback uses the PSX facade, but native owns the VBlank
// clock that emits the registered callback.
// NOTE(aalhendi): Native paces VBlank from PS1 NTSC video timing instead of
// rounded 60Hz. PSX-SPX lists NTSC as 263 scanlines/frame and about 3413 video
// cycles/scanline. With the NTSC GPU clock used here, this is ~59.817Hz, making
// VSync(2) roughly 29.909 FPS. This affects host wall pacing; game state still
// advances from emitted VBlank counts and retail RCNT1 ticks.
#define NATIVE_VBLANK_GPU_CYCLES 897619ull // 3413 * 263
#define NATIVE_GPU_CLOCK_HZ      53693175ull
#define NATIVE_VSYNC_CATCHUP_MAX 8
// NOTE(aalhendi): Desktop uses SDL_DelayPrecise plus a final bounded spin.
// UIKit uses a fully yielding sleep because CADisplayLink owns its main thread.
#if defined(SDL_PLATFORM_IOS)
// CADisplayLink owns the outer iOS loop. Do not burn the final 200 us of each
// synthetic NTSC VBlank on the UIKit main thread; an absolute deadline still
// prevents drift when SDL_DelayPrecise wakes late.
#define NATIVE_VSYNC_SPIN_US 0
#else
#define NATIVE_VSYNC_SPIN_US 200
#endif

global_variable u64 s_nextVBlankCounter = 0;
global_variable u64 s_vblankRemainder = 0;
global_variable int s_nativeVBlankCount = 0;

internal void Native_RebaseVBlankClock(void)
{
	// Preserve the game-visible count and RCNT1 state. Only discard the host
	// deadline that became stale while UIKit withheld CPU time.
	s_nextVBlankCounter = 0;
	s_vblankRemainder = 0;
}

internal u64 Native_CounterFromMicroseconds(u64 freq, u64 microseconds)
{
	return (freq * microseconds) / 1000000;
}

internal void Native_AdvanceVBlankTarget(void)
{
	const u64 freq = SDL_GetPerformanceFrequency();
	// counter ticks per vblank = freq * (897619 / 53693175) sec, kept exact with a
	// running remainder. freq*897619 fits u64 for any realistic QPC frequency.
	const u64 numer = freq * NATIVE_VBLANK_GPU_CYCLES;

	s_nextVBlankCounter += numer / NATIVE_GPU_CLOCK_HZ;
	s_vblankRemainder += numer % NATIVE_GPU_CLOCK_HZ;
	if (s_vblankRemainder >= NATIVE_GPU_CLOCK_HZ)
	{
		s_nextVBlankCounter++;
		s_vblankRemainder -= NATIVE_GPU_CLOCK_HZ;
	}
}

internal void Native_EnsureVBlankTarget(void)
{
	const u64 now = SDL_GetPerformanceCounter();

	if (s_nextVBlankCounter == 0)
	{
		s_nextVBlankCounter = now;
		s_vblankRemainder = 0;
		Native_AdvanceVBlankTarget();
	}
}

internal void Native_WaitUntilVBlankTarget(void)
{
	const u64 freq = SDL_GetPerformanceFrequency();
	const u64 spinWindow = Native_CounterFromMicroseconds(freq, NATIVE_VSYNC_SPIN_US);

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_VSYNC_WAIT);
	while (1)
	{
		const u64 now = SDL_GetPerformanceCounter();
		u64 remaining;
		u64 sleepUs;

		if (now >= s_nextVBlankCounter)
		{
			NativePerf_EndScope(NATIVE_PERF_BUCKET_VSYNC_WAIT);
			return;
		}

		remaining = s_nextVBlankCounter - now;
		if (remaining <= spinWindow)
		{
			// NOTE(penta3): OS sleeps can wake late. Sleep while safely far from
			// the VBlank target (high-res waitable timer), then spin only this
			// final small window so the native VBlank emitter is paced by our
			// clock, not the OS scheduler.
			while (SDL_GetPerformanceCounter() < s_nextVBlankCounter)
			{
			}

			NativePerf_EndScope(NATIVE_PERF_BUCKET_VSYNC_WAIT);
			return;
		}

		sleepUs = ((remaining - spinWindow) * 1000000) / freq;
		if (sleepUs > 0)
		{
			// UIKit owns the outer display loop. SDL_DelayPrecise still spins its
			// final sub-millisecond interval, so iOS uses the fully yielding system
			// sleep. Waking slightly late is safe: the target is absolute and the
			// catch-up limiter handles elapsed VBlanks without accumulating drift.
#if defined(SDL_PLATFORM_IOS)
			SDL_DelayNS(sleepUs * 1000ull);
#else
			// Other hosts retain the established high-resolution pacing path used
			// by the accepted desktop cadence evidence.
			SDL_DelayPrecise(sleepUs * 1000ull);
#endif
		}
	}
}

internal void Native_EmitVBlank(void)
{
	NativeRCnt_EmitVBlank();

	if (vsync_callback != NULL)
	{
		vsync_callback();
	}

	NativeAudio_StepVBlank();
	s_nativeVBlankCount++;
}

internal int Native_CatchUpDueVBlanks(void)
{
	int emittedVBlanks = 0;

	Native_EnsureVBlankTarget();

	// NOTE(aalhendi): Native host stalls can be much longer than retail frame
	// stalls, for example during window dragging or a debugger break. Replay a few
	// late VBlanks normally, but rebase pathological stalls instead of bursting
	// many callbacks into one host frame.
	{
		const u64 now = SDL_GetPerformanceCounter();

		if (now >= s_nextVBlankCounter)
		{
			const u64 freq = SDL_GetPerformanceFrequency();
			const u64 step = (freq * NATIVE_VBLANK_GPU_CYCLES) / NATIVE_GPU_CLOCK_HZ;
			const u64 dueApprox = ((now - s_nextVBlankCounter) / step) + 1;

			if (dueApprox > NATIVE_VSYNC_CATCHUP_MAX)
			{
				s_nextVBlankCounter = now;
				s_vblankRemainder = 0;
				Native_AdvanceVBlankTarget();
				return 0;
			}
		}
	}

	while (SDL_GetPerformanceCounter() >= s_nextVBlankCounter)
	{
		const u64 now = SDL_GetPerformanceCounter();

		Native_EmitVBlank();
		emittedVBlanks++;

		if (emittedVBlanks >= NATIVE_VSYNC_CATCHUP_MAX)
		{
			// NOTE(aalhendi): Keep normal late frames faithful, but rebase if the
			// due count grew past the cap while we were replaying.
			s_nextVBlankCounter = now;
			s_vblankRemainder = 0;
			Native_AdvanceVBlankTarget();
			break;
		}

		Native_AdvanceVBlankTarget();
	}

	return emittedVBlanks;
}

internal void Native_WaitAndEmitVBlank(void)
{
	Native_EnsureVBlankTarget();
	Native_WaitUntilVBlankTarget();
	Native_EmitVBlank();
	Native_AdvanceVBlankTarget();
}

int VSync(int mode)
{
	int requestedVBlanks;
	int emittedVBlanks;

	if (mode < 0)
	{
		return s_nativeVBlankCount;
	}

	requestedVBlanks = (mode == 0) ? 1 : mode;
	emittedVBlanks = 0;

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_ConsumeVSyncPacket(requestedVBlanks, &emittedVBlanks))
	{
		for (s32 i = 0; i < emittedVBlanks; i++)
		{
			Native_WaitAndEmitVBlank();
		}

		return s_nativeVBlankCount;
	}
#endif

	emittedVBlanks += Native_CatchUpDueVBlanks();

	for (s32 i = 0; i < requestedVBlanks; i++)
	{
		Native_WaitAndEmitVBlank();
		emittedVBlanks++;
	}

#if defined(CTR_INTERNAL)
	NativeReplayScheduler_RecordVSyncPacket(emittedVBlanks);
#endif

	return s_nativeVBlankCount;
}

int Platform_GetVBlankCount(void)
{
	return s_nativeVBlankCount;
}

void Platform_WaitUntilVBlank(int targetVBlank)
{
	int emittedVBlanks = 0;
	int requestedVBlanks = targetVBlank - s_nativeVBlankCount;

	if (requestedVBlanks <= 0)
	{
		return;
	}

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_ConsumeVSyncPacket(requestedVBlanks, &emittedVBlanks))
	{
		for (s32 i = 0; i < emittedVBlanks; i++)
		{
			Native_WaitAndEmitVBlank();
		}

		return;
	}
#endif

	emittedVBlanks += Native_CatchUpDueVBlanks();

	while (s_nativeVBlankCount < targetVBlank)
	{
		Native_WaitAndEmitVBlank();
		emittedVBlanks++;
	}

#if defined(CTR_INTERNAL)
	NativeReplayScheduler_RecordVSyncPacket(emittedVBlanks);
#endif
}

int Platform_RunLifecycleSelfTest(void)
{
	struct NativeLifecycleStatus status = {NATIVE_LIFECYCLE_ACTIVE, 0, 0};
	struct NativeLifecycleActions actions;
	const int savedVBlankCount = s_nativeVBlankCount;
	const u64 savedNextVBlankCounter = s_nextVBlankCounter;
	const u64 savedVBlankRemainder = s_vblankRemainder;

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_WILL_ENTER_BACKGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_WILL_ENTER_BACKGROUND) || (status.outputSuspended != 1) ||
	    (actions.suspendOutput != 1) || (actions.resetInput != 1) || (actions.flushLog != 1))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: will-background\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_WILL_ENTER_BACKGROUND);
	if ((actions.suspendOutput != 0) || (actions.resetInput != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: duplicate suspend\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_BACKGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_BACKGROUND) || (actions.suspendOutput != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: did-background\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_WILL_ENTER_FOREGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_WILL_ENTER_FOREGROUND) || (actions.resumeOutput != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: will-foreground\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_FOREGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_ACTIVE) || (status.outputSuspended != 0) || (actions.resumeInput != 1) ||
	    (actions.rebaseVBlankClock != 1) || (actions.resumeOutput != 1))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: did-foreground\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_FOREGROUND);
	if ((actions.resumeInput != 0) || (actions.rebaseVBlankClock != 0) || (actions.resumeOutput != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: duplicate resume\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_BACKGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_BACKGROUND) || (actions.suspendOutput != 1) || (actions.resetInput != 1))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: direct background\n");
		return 1;
	}
	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_FOREGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_ACTIVE) || (actions.resumeOutput != 1) || (actions.rebaseVBlankClock != 1))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: direct foreground\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_LOW_MEMORY);
	if ((status.phase != NATIVE_LIFECYCLE_ACTIVE) || (actions.flushLog != 1) || (actions.suspendOutput != 0) ||
	    (actions.resumeOutput != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: low-memory\n");
		return 1;
	}

	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_TERMINATING);
	if ((status.phase != NATIVE_LIFECYCLE_TERMINATING) || (status.quitRequested != 1) || (status.outputSuspended != 1) ||
	    (actions.suspendOutput != 1) || (actions.resetInput != 1) || (actions.flushLog != 1))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: terminating\n");
		return 1;
	}
	actions = NativeLifecycle_Reduce(&status, SDL_EVENT_DID_ENTER_FOREGROUND);
	if ((status.phase != NATIVE_LIFECYCLE_TERMINATING) || (actions.resumeOutput != 0) || (actions.rebaseVBlankClock != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: terminate is final\n");
		return 1;
	}

	s_nativeVBlankCount = 77;
	s_nextVBlankCounter = 123;
	s_vblankRemainder = 45;
	Native_RebaseVBlankClock();
	if ((s_nativeVBlankCount != 77) || (s_nextVBlankCounter != 0) || (s_vblankRemainder != 0))
	{
		fprintf(stderr, "[CTR Lifecycle] self-test failed: vblank rebase\n");
		s_nativeVBlankCount = savedVBlankCount;
		s_nextVBlankCounter = savedNextVBlankCounter;
		s_vblankRemainder = savedVBlankRemainder;
		return 1;
	}

	s_nativeVBlankCount = savedVBlankCount;
	s_nextVBlankCounter = savedNextVBlankCounter;
	s_vblankRemainder = savedVBlankRemainder;
	printf("[CTR Lifecycle] self-test passed: background=idempotent foreground=rebase quit=cooperative audio=paired low-memory=flush\n");
	return 0;
}

int Platform_RunFrameStatsSelfTest(void)
{
	const u64 oddDurations[] = {1, 2, 3, 4, 100};
	const u64 evenDurations[] = {4, 1, 3, 2};
	struct NativeFrameStats stats;

	if (!NativeFrameStats_Calculate(oddDurations, 5, 1000, &stats) || (stats.meanMilliseconds != 22.0) ||
	    (stats.medianMilliseconds != 3.0) || (stats.p95Milliseconds != 100.0) || (stats.p99Milliseconds != 100.0) ||
	    (stats.maxMilliseconds != 100.0) || (oddDurations[0] != 1) || (oddDurations[4] != 100))
	{
		fprintf(stderr, "[CTR FrameStats] self-test failed: odd nearest-rank percentiles\n");
		return 1;
	}

	if (!NativeFrameStats_Calculate(evenDurations, 4, 1000, &stats) || (stats.meanMilliseconds != 2.5) ||
	    (stats.medianMilliseconds != 2.5) || (stats.p95Milliseconds != 4.0) || (stats.p99Milliseconds != 4.0) ||
	    (stats.maxMilliseconds != 4.0))
	{
		fprintf(stderr, "[CTR FrameStats] self-test failed: even median\n");
		return 1;
	}

	if (NativeFrameStats_Calculate(NULL, 4, 1000, &stats) || NativeFrameStats_Calculate(evenDurations, 0, 1000, &stats) ||
	    NativeFrameStats_Calculate(evenDurations, 4, 0, &stats))
	{
		fprintf(stderr, "[CTR FrameStats] self-test failed: invalid input\n");
		return 1;
	}

	printf("[CTR FrameStats] self-test passed: mean=checked median=odd+even p95=nearest-rank p99=nearest-rank input=immutable\n");
	return 0;
}
