#ifndef PLATFORM_NATIVE_INPUT_H
#define PLATFORM_NATIVE_INPUT_H

#include <macros.h>

#define PLATFORM_INPUT_PAD_COUNT 4

enum PlatformInputKeyboardAction
{
	PLATFORM_INPUT_KEYBOARD_UP = 0,
	PLATFORM_INPUT_KEYBOARD_DOWN,
	PLATFORM_INPUT_KEYBOARD_LEFT,
	PLATFORM_INPUT_KEYBOARD_RIGHT,
	PLATFORM_INPUT_KEYBOARD_CROSS,
	PLATFORM_INPUT_KEYBOARD_SQUARE,
	PLATFORM_INPUT_KEYBOARD_CIRCLE,
	PLATFORM_INPUT_KEYBOARD_TRIANGLE,
	PLATFORM_INPUT_KEYBOARD_L1,
	PLATFORM_INPUT_KEYBOARD_R1,
	PLATFORM_INPUT_KEYBOARD_L2,
	PLATFORM_INPUT_KEYBOARD_R2,
	PLATFORM_INPUT_KEYBOARD_L3,
	PLATFORM_INPUT_KEYBOARD_R3,
	PLATFORM_INPUT_KEYBOARD_START,
	PLATFORM_INPUT_KEYBOARD_SELECT,
	PLATFORM_INPUT_KEYBOARD_ACTION_COUNT,
};

#define PLATFORM_INPUT_KEYBOARD_BINDING_COUNT 2

enum PlatformInputTouchButton
{
	PLATFORM_INPUT_TOUCH_SELECT = 0x0001,
	PLATFORM_INPUT_TOUCH_L3 = 0x0002,
	PLATFORM_INPUT_TOUCH_R3 = 0x0004,
	PLATFORM_INPUT_TOUCH_START = 0x0008,
	PLATFORM_INPUT_TOUCH_UP = 0x0010,
	PLATFORM_INPUT_TOUCH_RIGHT = 0x0020,
	PLATFORM_INPUT_TOUCH_DOWN = 0x0040,
	PLATFORM_INPUT_TOUCH_LEFT = 0x0080,
	PLATFORM_INPUT_TOUCH_L2 = 0x0100,
	PLATFORM_INPUT_TOUCH_R2 = 0x0200,
	PLATFORM_INPUT_TOUCH_L1 = 0x0400,
	PLATFORM_INPUT_TOUCH_R1 = 0x0800,
	PLATFORM_INPUT_TOUCH_TRIANGLE = 0x1000,
	PLATFORM_INPUT_TOUCH_CIRCLE = 0x2000,
	PLATFORM_INPUT_TOUCH_CROSS = 0x4000,
	PLATFORM_INPUT_TOUCH_SQUARE = 0x8000,
};

struct PlatformInputPadSnapshot
{
	u8 status;
	u8 id;
	u8 buttons[2];
	u8 analog[4];
	u8 connected;
	u8 reserved[3];
};

int Platform_InputInit(void);
void Platform_InputShutdown(void);
void Platform_InputUpdate(void);
void Platform_InputSuspend(void);
void Platform_InputResume(void);
void Platform_InputKeyboardEvent(int key, int down);
int Platform_InputGetKeyboardBinding(int action, int binding);
int Platform_InputSetKeyboardBinding(int action, int binding, int key);
void Platform_InputResetKeyboardBindings(void);
void Platform_InputSetKeyboardSuppressed(int suppressed);
void Platform_InputMouseButtonEvent(int button, int down);
void Platform_InputTouchSetEnabled(int enabled);
void Platform_InputTouchButton(unsigned int buttonMask, int down);
void Platform_InputTouchLeftStick(int x, int y, int active);
void Platform_InputTouchReset(void);
// A physical controller owns the primary pad, which the touch controls also drive.
// Controllers on later pads, and SDL virtual joysticks, do not count.
int Platform_InputTouchPlayerHasController(void);
void Platform_InputControllerAdded(int deviceIndex);
void Platform_InputControllerRemoved(int instanceId);
int Platform_InputCycleKeyboardController(void);
int Platform_InputCycleGamepadController(void);

void Platform_InputPadInit(int slot, unsigned char *padData);
int Platform_InputPadGetState(int port);
void Platform_InputPadVibrate(int port, unsigned char *table, int len);
int Platform_InputCapturePadSnapshots(struct PlatformInputPadSnapshot *dst, int count);
int Platform_InputInstallPadSnapshots(const struct PlatformInputPadSnapshot *src, int count);
void Platform_InputClearInstalledPadSnapshots(void);
int Platform_InputUpgradeLegacySubmitNameSnapshots(struct PlatformInputPadSnapshot *snapshots, int count);
void Platform_InputSetSubmitNameKey(int key, int down);
int Platform_InputGetSubmitNameKey(void);
int Platform_InputRunSelfTest(void);
int Platform_InputGetStateSize(void);
int Platform_InputCaptureState(void *dst, int dstSize);
int Platform_InputRestoreState(const void *src, int srcSize);

#endif
