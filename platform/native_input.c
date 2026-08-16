#include <platform/native_input.h>

#include <macros.h>
#include "psx/libpad.h"
#include "platform/native_log.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_INPUT_MAX_CONTROLLERS       PLATFORM_INPUT_PAD_COUNT
#define NATIVE_INPUT_PHYSICAL_SLOT_COUNT   2
#define NATIVE_INPUT_PAD_PACKET_BYTES      8
#define NATIVE_INPUT_MULTITAP_HEADER       2
#define NATIVE_INPUT_PAD_DIGITAL           0x41
#define NATIVE_INPUT_PAD_ANALOG            0x73
#define NATIVE_INPUT_PAD_MULTITAP          0x80
#define NATIVE_INPUT_PAD_DISCONNECT        0xff
#define NATIVE_INPUT_AXIS_DEADZONE         500
#define NATIVE_INPUT_MAP_FLAG_AXIS         0x4000
#define NATIVE_INPUT_MAP_FLAG_INVERSE      0x8000
#define NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT 0
#define NATIVE_INPUT_SUBMIT_NAME_KEY_MARKER 0x53
// NOTE(aalhendi): Little-endian tag `CTRI` = CTR native Input snapshot.
#define NATIVE_INPUT_STATE_MAGIC           0x49525443
#define NATIVE_INPUT_STATE_VERSION         1

// NOTE(aalhendi): Native input preserves behavior from PsyCross's
// MIT-licensed pad implementation while moving host ownership into ctr-native.
// See THIRD_PARTY_NOTICES.md.

struct NativeInputControllerMapping
{
	s32 id;

	s32 gc_square, gc_circle, gc_triangle, gc_cross;

	s32 gc_l1, gc_l2, gc_l3;
	s32 gc_r1, gc_r2, gc_r3;

	s32 gc_start, gc_select;

	s32 gc_dpad_left, gc_dpad_right, gc_dpad_up, gc_dpad_down;

	s32 gc_axis_left_x, gc_axis_left_y;
	s32 gc_axis_right_x, gc_axis_right_y;
};

struct NativeInputController
{
	SDL_JoystickID instanceId;
	SDL_Gamepad *controller;
	s32 analogEnabled;
	s32 switchingAnalog;
	struct PlatformInputPadSnapshot snapshot;
};

struct NativeInputControllerStateSnapshot
{
	struct PlatformInputPadSnapshot snapshot;
	s32 analogEnabled;
	s32 switchingAnalog;
	s32 controllerToSlotMapping;
};

struct NativeInputStateSnapshot
{
	u32 magic;
	u32 version;
	u32 size;
	s32 keyboardControllerSlot;
	s32 lastActiveControllerSlot;
	s32 installedSnapshotsActive;
	struct PlatformInputPadSnapshot installedSnapshots[NATIVE_INPUT_MAX_CONTROLLERS];
	struct NativeInputControllerStateSnapshot controllers[NATIVE_INPUT_MAX_CONTROLLERS];
};

struct NativeInputTouchState
{
	u16 heldButtons;
	u16 latchedButtons;
	s16 leftX;
	s16 leftY;
	s32 leftStickActive;
	s32 enabled;
};

global_variable struct NativeInputControllerMapping s_controllerMapping;
global_variable s32 s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_ACTION_COUNT][PLATFORM_INPUT_KEYBOARD_BINDING_COUNT];
global_variable s32 s_controllerToSlotMapping[NATIVE_INPUT_MAX_CONTROLLERS] = {-1, -1, -1, -1};

global_variable struct NativeInputController s_controllers[NATIVE_INPUT_MAX_CONTROLLERS];
global_variable struct PlatformInputPadSnapshot s_installedSnapshots[NATIVE_INPUT_MAX_CONTROLLERS];
global_variable u8 *s_padSlotData[NATIVE_INPUT_PHYSICAL_SLOT_COUNT];
global_variable const bool *s_keyboardState;
global_variable s32 s_inputInitialized;
global_variable s32 s_installedSnapshotsActive;
global_variable s32 s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;
global_variable s32 s_lastActiveControllerSlot = -1;
global_variable s32 s_submitNameKey;
global_variable s32 s_keyboardSuppressed;
// SDL can deliver key-down and key-up while the VSync callback continues to
// refresh pad packets faster than slow game logic consumes them. Keep each
// active-low edge until GAMEPAD_ProcessHold acknowledges its retail poll.
// This host transport state is intentionally not serialized.
global_variable u16 s_keyboardLatchedButtons = 0xffff;
// Desktop mouse buttons are optional peer inputs for the keyboard player.
// Keep fast clicks until the retail poll, while held buttons remain live.
global_variable u16 s_mouseHeldButtons;
global_variable u16 s_mouseLatchedButtons;
// UIKit touch contacts are host transport state. Button-down edges are
// likewise retained until the retail consumer polls; held contacts and the
// analog stick remain live until UIKit releases them.
global_variable struct NativeInputTouchState s_touchState;

extern s32 g_padCommEnable;

internal u16 NativeInput_GetSnapshotButtons(const struct PlatformInputPadSnapshot *snapshot)
{
	return (u16)(snapshot->buttons[0] | (snapshot->buttons[1] << 8));
}

internal void NativeInput_SetSnapshotButtons(struct PlatformInputPadSnapshot *snapshot, u16 buttons)
{
	snapshot->buttons[0] = (u8)(buttons & 0xff);
	snapshot->buttons[1] = (u8)(buttons >> 8);
}

internal void NativeInput_SetSnapshotSubmitNameKey(struct PlatformInputPadSnapshot *snapshot, s32 key)
{
	if (snapshot == NULL)
	{
		return;
	}

	snapshot->reserved[0] = NATIVE_INPUT_SUBMIT_NAME_KEY_MARKER;
	snapshot->reserved[1] = (u8)(key & 0xff);
	snapshot->reserved[2] = (u8)((u32)key >> 8);
}

internal s32 NativeInput_GetSnapshotSubmitNameKey(const struct PlatformInputPadSnapshot *snapshot)
{
	if ((snapshot == NULL) || (snapshot->reserved[0] != NATIVE_INPUT_SUBMIT_NAME_KEY_MARKER))
	{
		return 0;
	}

	return (s32)(snapshot->reserved[1] | ((u32)snapshot->reserved[2] << 8));
}

internal void NativeInput_ResetSnapshot(s32 slot)
{
	struct PlatformInputPadSnapshot *snapshot = &s_controllers[slot].snapshot;

	snapshot->connected = slot == 0;
	snapshot->status = snapshot->connected ? 0 : NATIVE_INPUT_PAD_DISCONNECT;
	snapshot->id = snapshot->connected ? NATIVE_INPUT_PAD_DIGITAL : NATIVE_INPUT_PAD_DISCONNECT;
	NativeInput_SetSnapshotButtons(snapshot, 0xffff);
	snapshot->analog[0] = 0x80;
	snapshot->analog[1] = 0x80;
	snapshot->analog[2] = 0x80;
	snapshot->analog[3] = 0x80;
	memset(snapshot->reserved, 0, sizeof(snapshot->reserved));
}

internal s32 NativeInput_IsValidControllerSlot(s32 slot)
{
	return (slot >= 0) && (slot < NATIVE_INPUT_MAX_CONTROLLERS);
}

internal s32 NativeInput_NextControllerSlot(s32 slot)
{
	slot++;
	if (slot >= NATIVE_INPUT_MAX_CONTROLLERS)
	{
		slot = 0;
	}

	return slot;
}

internal s32 NativeInput_SharePrimaryInputSources(void)
{
#if defined(SDL_PLATFORM_IOS)
	// iPhone and iPad have one local player surface. Hardware-keyboard,
	// touch, and gamepad input must all compose into the primary PSX pad.
	return 1;
#else
	return 0;
#endif
}

internal void NativeInput_AssignKeyboardForController(s32 slot, s32 sharePrimaryInput)
{
	if ((sharePrimaryInput == 0) && (s_keyboardControllerSlot == slot))
	{
		s_keyboardControllerSlot = NativeInput_NextControllerSlot(s_keyboardControllerSlot);
	}
}

internal void NativeInput_MakeDisconnectedSnapshot(struct PlatformInputPadSnapshot *snapshot)
{
	if (snapshot == NULL)
	{
		return;
	}

	snapshot->connected = 0;
	snapshot->status = NATIVE_INPUT_PAD_DISCONNECT;
	snapshot->id = NATIVE_INPUT_PAD_DISCONNECT;
	NativeInput_SetSnapshotButtons(snapshot, 0xffff);
	snapshot->analog[0] = 0x80;
	snapshot->analog[1] = 0x80;
	snapshot->analog[2] = 0x80;
	snapshot->analog[3] = 0x80;
	memset(snapshot->reserved, 0, sizeof(snapshot->reserved));
}

internal void NativeInput_WritePadPacket(u8 *dst, const struct PlatformInputPadSnapshot *snapshot)
{
	if ((dst == NULL) || (snapshot == NULL))
	{
		return;
	}

	dst[0] = snapshot->status;
	dst[1] = snapshot->id;
	dst[2] = snapshot->buttons[0];
	dst[3] = snapshot->buttons[1];
	dst[4] = snapshot->analog[0];
	dst[5] = snapshot->analog[1];
	dst[6] = snapshot->analog[2];
	dst[7] = snapshot->analog[3];
}

internal s32 NativeInput_UseMultitapBus(void)
{
	s32 slot;

	for (slot = NATIVE_INPUT_PHYSICAL_SLOT_COUNT; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if (s_controllers[slot].snapshot.connected != 0)
		{
			return 1;
		}
	}

	return 0;
}

internal void NativeInput_WritePadBus(void)
{
	u8 *slot0 = s_padSlotData[0];
	u8 *slot1 = s_padSlotData[1];
	s32 useMultitap = NativeInput_UseMultitapBus();
	s32 slot;

	if (slot0 != NULL)
	{
		if (useMultitap != 0)
		{
			slot0[0] = 0;
			slot0[1] = NATIVE_INPUT_PAD_MULTITAP;
			for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
			{
				NativeInput_WritePadPacket(&slot0[NATIVE_INPUT_MULTITAP_HEADER + (slot * NATIVE_INPUT_PAD_PACKET_BYTES)], &s_controllers[slot].snapshot);
			}
		}
		else
		{
			NativeInput_WritePadPacket(slot0, &s_controllers[0].snapshot);
		}
	}

	if (slot1 != NULL)
	{
		if (useMultitap != 0)
		{
			struct PlatformInputPadSnapshot disconnected;

			NativeInput_MakeDisconnectedSnapshot(&disconnected);
			NativeInput_WritePadPacket(slot1, &disconnected);
		}
		else
		{
			NativeInput_WritePadPacket(slot1, &s_controllers[1].snapshot);
		}
	}
}

internal void NativeInput_WriteInstalledSnapshots(void)
{
	s32 slot;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		s_controllers[slot].snapshot = s_installedSnapshots[slot];
	}

	NativeInput_WritePadBus();
}

internal void NativeInput_DefaultMappings(void)
{
	memset(s_keyboardBindings, 0, sizeof(s_keyboardBindings));
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_UP][0] = SDL_SCANCODE_W;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_UP][1] = SDL_SCANCODE_UP;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_DOWN][0] = SDL_SCANCODE_S;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_DOWN][1] = SDL_SCANCODE_DOWN;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_LEFT][0] = SDL_SCANCODE_A;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_LEFT][1] = SDL_SCANCODE_LEFT;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_RIGHT][0] = SDL_SCANCODE_D;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_RIGHT][1] = SDL_SCANCODE_RIGHT;

	// Racing-first defaults. These are host aliases only; the retail game still
	// receives the same PlayStation-shaped buttons as touch and controllers.
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_CROSS][0] = SDL_SCANCODE_LSHIFT;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_CROSS][1] = SDL_SCANCODE_RSHIFT;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_SQUARE][0] = SDL_SCANCODE_E;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_SQUARE][1] = SDL_SCANCODE_X;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_CIRCLE][0] = SDL_SCANCODE_SPACE;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_CIRCLE][1] = SDL_SCANCODE_V;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_TRIANGLE][0] = SDL_SCANCODE_ESCAPE;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_TRIANGLE][1] = SDL_SCANCODE_Z;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_L1][0] = SDL_SCANCODE_Q;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_R1][0] = SDL_SCANCODE_R;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_L2][0] = SDL_SCANCODE_LCTRL;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_R2][0] = SDL_SCANCODE_RCTRL;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_L3][0] = SDL_SCANCODE_LEFTBRACKET;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_R3][0] = SDL_SCANCODE_RIGHTBRACKET;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_START][0] = SDL_SCANCODE_RETURN;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_START][1] = SDL_SCANCODE_P;
	s_keyboardBindings[PLATFORM_INPUT_KEYBOARD_SELECT][0] = SDL_SCANCODE_TAB;

	s_controllerMapping.gc_square = SDL_GAMEPAD_BUTTON_WEST;
	s_controllerMapping.gc_circle = SDL_GAMEPAD_BUTTON_EAST;
	s_controllerMapping.gc_triangle = SDL_GAMEPAD_BUTTON_NORTH;
	s_controllerMapping.gc_cross = SDL_GAMEPAD_BUTTON_SOUTH;

	s_controllerMapping.gc_l1 = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
	s_controllerMapping.gc_l2 = SDL_GAMEPAD_AXIS_LEFT_TRIGGER | NATIVE_INPUT_MAP_FLAG_AXIS;
	s_controllerMapping.gc_l3 = SDL_GAMEPAD_BUTTON_LEFT_STICK;

	s_controllerMapping.gc_r1 = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	s_controllerMapping.gc_r2 = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER | NATIVE_INPUT_MAP_FLAG_AXIS;
	s_controllerMapping.gc_r3 = SDL_GAMEPAD_BUTTON_RIGHT_STICK;

	s_controllerMapping.gc_dpad_up = SDL_GAMEPAD_BUTTON_DPAD_UP;
	s_controllerMapping.gc_dpad_down = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
	s_controllerMapping.gc_dpad_left = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
	s_controllerMapping.gc_dpad_right = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;

	s_controllerMapping.gc_select = SDL_GAMEPAD_BUTTON_BACK;
	s_controllerMapping.gc_start = SDL_GAMEPAD_BUTTON_START;

	s_controllerMapping.gc_axis_left_x = SDL_GAMEPAD_AXIS_LEFTX | NATIVE_INPUT_MAP_FLAG_AXIS;
	s_controllerMapping.gc_axis_left_y = SDL_GAMEPAD_AXIS_LEFTY | NATIVE_INPUT_MAP_FLAG_AXIS;
	s_controllerMapping.gc_axis_right_x = SDL_GAMEPAD_AXIS_RIGHTX | NATIVE_INPUT_MAP_FLAG_AXIS;
	s_controllerMapping.gc_axis_right_y = SDL_GAMEPAD_AXIS_RIGHTY | NATIVE_INPUT_MAP_FLAG_AXIS;
}

internal u16 NativeInput_KeyboardButtonBit(s32 key)
{
	static const u16 buttonBits[PLATFORM_INPUT_KEYBOARD_ACTION_COUNT] = {
	    0x10, 0x40, 0x80, 0x20, 0x4000, 0x8000, 0x2000, 0x1000,
	    0x400, 0x800, 0x100, 0x200, 0x2, 0x4, 0x8, 0x1,
	};
	u16 buttonBit = 0;
	s32 action;
	s32 binding;

	if ((key <= SDL_SCANCODE_UNKNOWN) || (key >= SDL_SCANCODE_COUNT))
	{
		return 0;
	}

	for (action = 0; action < PLATFORM_INPUT_KEYBOARD_ACTION_COUNT; action++)
	{
		for (binding = 0; binding < PLATFORM_INPUT_KEYBOARD_BINDING_COUNT; binding++)
		{
			if (key == s_keyboardBindings[action][binding])
			{
				buttonBit |= buttonBits[action];
				break;
			}
		}
	}

	return buttonBit;
}

internal void NativeInput_ClearKeyboardLatch(void)
{
	s_keyboardLatchedButtons = 0xffff;
}

internal void NativeInput_ResetMouseButtons(void)
{
	s_mouseHeldButtons = 0;
	s_mouseLatchedButtons = 0;
}

internal u16 NativeInput_MouseButtonBit(int button)
{
	switch (button)
	{
	case SDL_BUTTON_LEFT:
		return PLATFORM_INPUT_TOUCH_CROSS;
	case SDL_BUTTON_RIGHT:
		return PLATFORM_INPUT_TOUCH_SQUARE;
	case SDL_BUTTON_MIDDLE:
		return PLATFORM_INPUT_TOUCH_CIRCLE;
	case SDL_BUTTON_X1:
		return PLATFORM_INPUT_TOUCH_L1;
	case SDL_BUTTON_X2:
		return PLATFORM_INPUT_TOUCH_R1;
	default:
		return 0;
	}
}

internal void NativeInput_ResetTouchContacts(void)
{
	s_touchState.heldButtons = 0;
	s_touchState.latchedButtons = 0;
	s_touchState.leftX = 0;
	s_touchState.leftY = 0;
	s_touchState.leftStickActive = 0;
}

void Platform_InputTouchSetEnabled(int enabled)
{
	NativeInput_ResetTouchContacts();
	s_touchState.enabled = enabled != 0;
}

void Platform_InputTouchButton(unsigned int buttonMask, int down)
{
	u16 mask = (u16)buttonMask;

	if ((s_touchState.enabled == 0) || (buttonMask == 0) || (buttonMask > 0xffffu))
	{
		return;
	}
	if (Platform_LogIsOpen())
	{
		Platform_Log("[CTR Input] source=touch edge=%s mask=0x%04x held-before=0x%04x\n", down != 0 ? "down" : "up",
		             (unsigned int)mask, (unsigned int)s_touchState.heldButtons);
	}

	if (down != 0)
	{
		s_touchState.heldButtons |= mask;
		s_touchState.latchedButtons |= mask;
	}
	else
	{
		s_touchState.heldButtons &= (u16)~mask;
	}
}

void Platform_InputTouchLeftStick(int x, int y, int active)
{
	if (s_touchState.enabled == 0)
	{
		return;
	}

	if (active == 0)
	{
		s_touchState.leftX = 0;
		s_touchState.leftY = 0;
		s_touchState.leftStickActive = 0;
		return;
	}

	if (x < -32768)
		x = -32768;
	if (x > 32767)
		x = 32767;
	if (y < -32768)
		y = -32768;
	if (y > 32767)
		y = 32767;

	s_touchState.leftX = (s16)x;
	s_touchState.leftY = (s16)y;
	s_touchState.leftStickActive = 1;
}

void Platform_InputTouchReset(void)
{
	NativeInput_ResetTouchContacts();
}

void Platform_InputKeyboardEvent(int key, int down)
{
	u16 buttonBit;

	if (down == 0)
	{
		return;
	}

	buttonBit = NativeInput_KeyboardButtonBit(key);
	if (buttonBit != 0)
	{
		if (Platform_LogIsOpen())
		{
			Platform_Log("[CTR Input] source=keyboard edge=down scancode=%d mask=0x%04x\n", key, (unsigned int)buttonBit);
		}
		s_keyboardLatchedButtons &= (u16)~buttonBit;
	}
}

int Platform_InputGetKeyboardBinding(int action, int binding)
{
	if ((action < 0) || (action >= PLATFORM_INPUT_KEYBOARD_ACTION_COUNT) ||
	    (binding < 0) || (binding >= PLATFORM_INPUT_KEYBOARD_BINDING_COUNT))
	{
		return SDL_SCANCODE_UNKNOWN;
	}

	return s_keyboardBindings[action][binding];
}

int Platform_InputSetKeyboardBinding(int action, int binding, int key)
{
	if ((action < 0) || (action >= PLATFORM_INPUT_KEYBOARD_ACTION_COUNT) ||
	    (binding < 0) || (binding >= PLATFORM_INPUT_KEYBOARD_BINDING_COUNT) ||
	    (key < SDL_SCANCODE_UNKNOWN) || (key >= SDL_SCANCODE_COUNT))
	{
		return 0;
	}

	s_keyboardBindings[action][binding] = key;
	NativeInput_ClearKeyboardLatch();
	return 1;
}

void Platform_InputResetKeyboardBindings(void)
{
	NativeInput_DefaultMappings();
	NativeInput_ClearKeyboardLatch();
}

void Platform_InputSetKeyboardSuppressed(int suppressed)
{
	s_keyboardSuppressed = suppressed != 0;
	NativeInput_ClearKeyboardLatch();
}

void Platform_InputMouseButtonEvent(int button, int down)
{
	u16 buttonBit = NativeInput_MouseButtonBit(button);
	if (buttonBit == 0)
	{
		return;
	}
	if (down != 0)
	{
		s_mouseHeldButtons |= buttonBit;
		s_mouseLatchedButtons |= buttonBit;
	}
	else
	{
		s_mouseHeldButtons &= (u16)~buttonBit;
	}
	if (Platform_LogIsOpen())
	{
		Platform_Log("[CTR Input] source=mouse edge=%s button=%d mask=0x%04x\n", down != 0 ? "down" : "up",
		             button, (unsigned int)buttonBit);
	}
}

internal s32 NativeInput_ControllerButtonState(SDL_Gamepad *controller, s32 buttonOrAxis)
{
	if (controller == NULL)
	{
		return 0;
	}

	if ((buttonOrAxis & NATIVE_INPUT_MAP_FLAG_AXIS) != 0)
	{
		s32 axis = buttonOrAxis & ~(NATIVE_INPUT_MAP_FLAG_AXIS | NATIVE_INPUT_MAP_FLAG_INVERSE);
		s32 value = SDL_GetGamepadAxis(controller, (SDL_GamepadAxis)axis);

		if ((abs(value) > NATIVE_INPUT_AXIS_DEADZONE) && ((buttonOrAxis & NATIVE_INPUT_MAP_FLAG_INVERSE) != 0))
		{
			value *= -1;
		}

		return value;
	}

	if (buttonOrAxis < 0)
	{
		return 0;
	}

	return SDL_GetGamepadButton(controller, (SDL_GamepadButton)buttonOrAxis) * 32767;
}

internal u8 NativeInput_AxisToByte(s32 axis)
{
	s32 value = (axis / 256) + 128;

	if (value < 0)
	{
		return 0;
	}

	if (value > 0xff)
	{
		return 0xff;
	}

	return (u8)value;
}

internal u16 NativeInput_ConsumeTouchButtons(void)
{
	return s_touchState.heldButtons | s_touchState.latchedButtons;
}

internal u16 NativeInput_ConsumeMouseButtons(void)
{
	return s_mouseHeldButtons | s_mouseLatchedButtons;
}

internal void NativeInput_ApplyMouse(s32 slot, u16 mouseButtons)
{
	struct PlatformInputPadSnapshot *snapshot;
	u16 buttons;

	if ((slot != s_keyboardControllerSlot) || (mouseButtons == 0))
	{
		return;
	}
	snapshot = &s_controllers[slot].snapshot;
	if (snapshot->connected == 0)
	{
		snapshot->connected = 1;
		snapshot->status = 0;
		snapshot->id = NATIVE_INPUT_PAD_DIGITAL;
	}
	buttons = NativeInput_GetSnapshotButtons(snapshot);
	NativeInput_SetSnapshotButtons(snapshot, buttons & (u16)~mouseButtons);
}

internal void NativeInput_ApplyTouch(s32 slot, u16 touchButtons)
{
	struct PlatformInputPadSnapshot *snapshot;
	u16 buttons;

	if ((slot != 0) || (s_touchState.enabled == 0))
	{
		return;
	}

	snapshot = &s_controllers[slot].snapshot;
	snapshot->connected = 1;
	snapshot->status = 0;
	snapshot->id = NATIVE_INPUT_PAD_ANALOG;
	buttons = NativeInput_GetSnapshotButtons(snapshot);
	NativeInput_SetSnapshotButtons(snapshot, buttons & (u16)~touchButtons);

	if (s_touchState.leftStickActive != 0)
	{
		snapshot->analog[2] = NativeInput_AxisToByte(s_touchState.leftX);
		snapshot->analog[3] = NativeInput_AxisToByte(s_touchState.leftY);
	}
}

internal s32 NativeInput_AxisIsActive(s32 axis)
{
	return abs(axis) > NATIVE_INPUT_AXIS_DEADZONE;
}

internal void NativeInput_ApplyController(s32 slot)
{
	struct NativeInputController *nativeController = &s_controllers[slot];
	struct PlatformInputPadSnapshot *snapshot = &nativeController->snapshot;
	const struct NativeInputControllerMapping *mapping = &s_controllerMapping;
	SDL_Gamepad *controller = nativeController->controller;
	u16 buttons = 0xffff;
	s32 rightX;
	s32 rightY;
	s32 leftX;
	s32 leftY;

	if ((controller == NULL) || (SDL_GamepadConnected(controller) == 0))
	{
		return;
	}

	snapshot->connected = 1;
	snapshot->status = 0;
	snapshot->id = nativeController->analogEnabled ? NATIVE_INPUT_PAD_ANALOG : NATIVE_INPUT_PAD_DIGITAL;

	if (NativeInput_ControllerButtonState(controller, mapping->gc_square) > 16384)
	{
		buttons &= ~0x8000;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_circle) > 16384)
	{
		buttons &= ~0x2000;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_triangle) > 16384)
	{
		buttons &= ~0x1000;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_cross) > 16384)
	{
		buttons &= ~0x4000;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_l1) > 16384)
	{
		buttons &= ~0x400;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_r1) > 16384)
	{
		buttons &= ~0x800;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_l2) > 16384)
	{
		buttons &= ~0x100;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_r2) > 16384)
	{
		buttons &= ~0x200;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_dpad_up) > 16384)
	{
		buttons &= ~0x10;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_dpad_down) > 16384)
	{
		buttons &= ~0x40;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_dpad_left) > 16384)
	{
		buttons &= ~0x80;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_dpad_right) > 16384)
	{
		buttons &= ~0x20;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_l3) > 16384)
	{
		buttons &= ~0x2;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_r3) > 16384)
	{
		buttons &= ~0x4;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_select) > 16384)
	{
		buttons &= ~0x1;
	}
	if (NativeInput_ControllerButtonState(controller, mapping->gc_start) > 16384)
	{
		buttons &= ~0x8;
	}

	rightX = NativeInput_ControllerButtonState(controller, mapping->gc_axis_right_x);
	rightY = NativeInput_ControllerButtonState(controller, mapping->gc_axis_right_y);
	leftX = NativeInput_ControllerButtonState(controller, mapping->gc_axis_left_x);
	leftY = NativeInput_ControllerButtonState(controller, mapping->gc_axis_left_y);

	if ((buttons != 0xffff) || NativeInput_AxisIsActive(rightX) || NativeInput_AxisIsActive(rightY) || NativeInput_AxisIsActive(leftX) ||
	    NativeInput_AxisIsActive(leftY))
	{
		s_lastActiveControllerSlot = slot;
	}

	if (((buttons & 0x1) == 0) && ((buttons & 0x8) == 0))
	{
		buttons = 0xffff;
		if (nativeController->switchingAnalog == 0)
		{
			nativeController->analogEnabled = nativeController->analogEnabled == 0;
		}
		nativeController->switchingAnalog = 1;
	}
	else
	{
		nativeController->switchingAnalog = 0;
	}

	NativeInput_SetSnapshotButtons(snapshot, buttons);
	snapshot->analog[0] = NativeInput_AxisToByte(rightX);
	snapshot->analog[1] = NativeInput_AxisToByte(rightY);
	snapshot->analog[2] = NativeInput_AxisToByte(leftX);
	snapshot->analog[3] = NativeInput_AxisToByte(leftY);
}

internal u16 NativeInput_ReadKeyboard(void)
{
	static const u16 buttonBits[PLATFORM_INPUT_KEYBOARD_ACTION_COUNT] = {
	    0x10, 0x40, 0x80, 0x20, 0x4000, 0x8000, 0x2000, 0x1000,
	    0x400, 0x800, 0x100, 0x200, 0x2, 0x4, 0x8, 0x1,
	};
	u16 buttons = s_keyboardLatchedButtons;
	s32 action;
	s32 binding;

	if (s_keyboardState == NULL)
	{
		return buttons;
	}

	for (action = 0; action < PLATFORM_INPUT_KEYBOARD_ACTION_COUNT; action++)
	{
		for (binding = 0; binding < PLATFORM_INPUT_KEYBOARD_BINDING_COUNT; binding++)
		{
			s32 key = s_keyboardBindings[action][binding];
			if ((key > SDL_SCANCODE_UNKNOWN) && (key < SDL_SCANCODE_COUNT) && s_keyboardState[key])
			{
				buttons &= (u16)~buttonBits[action];
				break;
			}
		}
	}

	return buttons;
}

internal u16 NativeInput_ConsumeKeyboard(void)
{
	return NativeInput_ReadKeyboard();
}

void Platform_InputAcknowledgeRetailPoll(void)
{
	u16 keyboardPressed = (u16)~s_keyboardLatchedButtons;
	u16 touchPressed = s_touchState.latchedButtons;
	u16 mousePressed = s_mouseLatchedButtons;

	if (((keyboardPressed != 0) || (touchPressed != 0) || (mousePressed != 0)) && Platform_LogIsOpen())
	{
		Platform_Log("[CTR Input] source=retail-poll consumed keyboard=0x%04x mouse=0x%04x touch=0x%04x\n",
		             (unsigned int)keyboardPressed, (unsigned int)mousePressed, (unsigned int)touchPressed);
	}
	NativeInput_ClearKeyboardLatch();
	s_mouseLatchedButtons = 0;
	s_touchState.latchedButtons = 0;
}

internal s32 NativeInput_KeyboardSuppressed(void)
{
	if (s_keyboardSuppressed != 0)
	{
		return 1;
	}

	if (s_keyboardState == NULL)
	{
		return 0;
	}

	return s_keyboardState[SDL_SCANCODE_RALT] || s_keyboardState[SDL_SCANCODE_LALT];
}

internal void NativeInput_ApplyKeyboard(s32 slot, u16 keyboardButtons)
{
	struct PlatformInputPadSnapshot *snapshot = &s_controllers[slot].snapshot;
	u16 buttons;

	if (slot != s_keyboardControllerSlot)
	{
		return;
	}

	if (snapshot->connected == 0)
	{
		snapshot->connected = 1;
		snapshot->status = 0;
		snapshot->id = NATIVE_INPUT_PAD_DIGITAL;
	}

	buttons = NativeInput_GetSnapshotButtons(snapshot);
	NativeInput_SetSnapshotButtons(snapshot, buttons & keyboardButtons);
}

internal s32 NativeInput_FindActiveControllerSlot(void)
{
	s32 slot;

	if (NativeInput_IsValidControllerSlot(s_lastActiveControllerSlot) && (s_controllers[s_lastActiveControllerSlot].controller != NULL))
	{
		return s_lastActiveControllerSlot;
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if (s_controllers[slot].controller != NULL)
		{
			return slot;
		}
	}

	return -1;
}

internal void NativeInput_SwapControllerSlots(s32 slotA, s32 slotB)
{
	struct NativeInputController controller;
	s32 mapping;

	if (!NativeInput_IsValidControllerSlot(slotA) || !NativeInput_IsValidControllerSlot(slotB) || (slotA == slotB))
	{
		return;
	}

	controller = s_controllers[slotA];
	s_controllers[slotA] = s_controllers[slotB];
	s_controllers[slotB] = controller;

	mapping = s_controllerToSlotMapping[slotA];
	s_controllerToSlotMapping[slotA] = s_controllerToSlotMapping[slotB];
	s_controllerToSlotMapping[slotB] = mapping;

	if (s_lastActiveControllerSlot == slotA)
	{
		s_lastActiveControllerSlot = slotB;
	}
	else if (s_lastActiveControllerSlot == slotB)
	{
		s_lastActiveControllerSlot = slotA;
	}
}

internal s32 NativeInput_FindSlotForDeviceIndex(Sint32 deviceIndex)
{
	s32 slot;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if (s_controllerToSlotMapping[slot] == deviceIndex)
		{
			return slot;
		}
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if ((s_controllerToSlotMapping[slot] < 0) && (s_controllers[slot].controller == NULL))
		{
			return slot;
		}
	}

	return -1;
}

internal void NativeInput_CloseController(s32 slot)
{
	struct NativeInputController *controller;
	const char *name;
	s32 connected;

	if ((slot < 0) || (slot >= NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return;
	}

	controller = &s_controllers[slot];
	if (controller->controller != NULL)
	{
		if (Platform_LogIsOpen())
		{
			name = SDL_GetGamepadName(controller->controller);
			connected = SDL_GamepadConnected(controller->controller) ? 1 : 0;
			Platform_Log("[CTR Input] controller-close slot=%d instance=%u connected=%d name=%s\n", slot + 1,
			             (unsigned int)controller->instanceId, connected, name != NULL ? name : "unknown");
		}
		SDL_CloseGamepad(controller->controller);
	}

	controller->controller = NULL;
	controller->instanceId = -1;
	controller->analogEnabled = 0;
	controller->switchingAnalog = 0;
	s_controllerToSlotMapping[slot] = -1;

	if (s_lastActiveControllerSlot == slot)
	{
		s_lastActiveControllerSlot = -1;
	}
}

internal s32 NativeInput_CloseDisconnectedControllers(void)
{
	s32 closed = 0;
	s32 slot;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if ((s_controllers[slot].controller != NULL) && !SDL_GamepadConnected(s_controllers[slot].controller))
		{
			NativeInput_CloseController(slot);
			closed++;
		}
	}

	return closed;
}

internal void NativeInput_OpenController(SDL_JoystickID instanceId, s32 slot)
{
	struct NativeInputController *controller;
	SDL_Joystick *joystick;

	if ((slot < 0) || (slot >= NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return;
	}

	if (SDL_IsGamepad(instanceId) == 0)
	{
		return;
	}

	controller = &s_controllers[slot];
	if (controller->controller != NULL)
	{
		return;
	}

	controller->controller = SDL_OpenGamepad(instanceId);
	if (controller->controller == NULL)
	{
		return;
	}

	joystick = SDL_GetGamepadJoystick(controller->controller);
	controller->instanceId = joystick != NULL ? SDL_GetJoystickID(joystick) : instanceId;
	controller->analogEnabled = 1;
	controller->switchingAnalog = 0;
	s_controllerToSlotMapping[slot] = controller->instanceId;
	NativeInput_AssignKeyboardForController(slot, NativeInput_SharePrimaryInputSources());
	if (Platform_LogIsOpen())
	{
		const char *name = SDL_GetGamepadName(controller->controller);
		Platform_Log("[CTR Input] controller-open slot=%d instance=%u name=%s\n", slot + 1,
		             (unsigned int)controller->instanceId, name != NULL ? name : "unknown");
	}
}

internal void NativeInput_OpenKnownControllers(void)
{
	SDL_JoystickID *gamepads;
	s32 count = 0;
	s32 i;

	gamepads = SDL_GetGamepads(&count);
	for (i = 0; i < count; i++)
	{
		s32 slot = NativeInput_FindSlotForDeviceIndex(gamepads[i]);

		if (slot >= 0)
		{
			NativeInput_OpenController(gamepads[i], slot);
		}
	}
	SDL_free(gamepads);
}

int Platform_InputInit(void)
{
	s32 slot;

	if (s_inputInitialized != 0)
	{
		return 1;
	}

	memset(s_controllers, 0, sizeof(s_controllers));
	memset(s_padSlotData, 0, sizeof(s_padSlotData));
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		s_controllers[slot].instanceId = -1;
		NativeInput_ResetSnapshot(slot);
		s_installedSnapshots[slot] = s_controllers[slot].snapshot;
	}

	NativeInput_DefaultMappings();
	s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;
	s_lastActiveControllerSlot = -1;
	s_installedSnapshotsActive = 0;
	s_keyboardState = SDL_GetKeyboardState(NULL);
	s_submitNameKey = 0;
	s_keyboardSuppressed = 0;
	NativeInput_ClearKeyboardLatch();
	NativeInput_ResetMouseButtons();
	NativeInput_ResetTouchContacts();

	if (SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC) == 0)
	{
		fprintf(stderr, "[CTR Native] Failed to initialise SDL input subsystem: %s\n", SDL_GetError());
		return 0;
	}

	SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
	NativeInput_OpenKnownControllers();

	s_inputInitialized = 1;
	return 1;
}

void Platform_InputShutdown(void)
{
	s32 slot;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		NativeInput_CloseController(slot);
	}

	if (s_inputInitialized != 0)
	{
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC);
	}

	s_inputInitialized = 0;
	s_installedSnapshotsActive = 0;
	s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;
	s_lastActiveControllerSlot = -1;
	s_submitNameKey = 0;
	s_keyboardSuppressed = 0;
	NativeInput_ClearKeyboardLatch();
	NativeInput_ResetMouseButtons();
	Platform_InputTouchSetEnabled(0);
	memset(s_padSlotData, 0, sizeof(s_padSlotData));
	s_keyboardState = NULL;
}

void Platform_InputUpdate(void)
{
	u16 keyboardButtons;
	u16 mouseButtons;
	u16 touchButtons;
	s32 slot;

	if (s_inputInitialized == 0)
	{
		return;
	}
	NativeInput_CloseDisconnectedControllers();

	if (s_installedSnapshotsActive != 0)
	{
		// NOTE(aalhendi): replay/state installs PSX-shaped pad bytes here;
		// SDL host state is not serialized.
		NativeInput_ClearKeyboardLatch();
		NativeInput_ResetMouseButtons();
		NativeInput_ResetTouchContacts();
		NativeInput_WriteInstalledSnapshots();
		return;
	}

	if (g_padCommEnable == 0)
	{
		NativeInput_ClearKeyboardLatch();
		s_mouseLatchedButtons = 0;
		// Keep current contacts like SDL's held keyboard state, but do not
		// replay a tap edge accumulated while the retail pad bus was disabled.
		s_touchState.latchedButtons = 0;
		return;
	}

	SDL_PumpEvents();
	keyboardButtons = NativeInput_ConsumeKeyboard();
	mouseButtons = NativeInput_ConsumeMouseButtons();
	touchButtons = NativeInput_ConsumeTouchButtons();
	if (NativeInput_KeyboardSuppressed())
	{
		keyboardButtons = 0xffff;
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		NativeInput_ResetSnapshot(slot);
		NativeInput_ApplyController(slot);
		NativeInput_ApplyKeyboard(slot, keyboardButtons);
		NativeInput_ApplyMouse(slot, mouseButtons);
		NativeInput_ApplyTouch(slot, touchButtons);
		if (slot == s_keyboardControllerSlot)
		{
			NativeInput_SetSnapshotSubmitNameKey(&s_controllers[slot].snapshot, s_submitNameKey);
		}
	}
	NativeInput_WritePadBus();
}

void Platform_InputSuspend(void)
{
	s32 slot;

	NativeInput_ClearKeyboardLatch();
	NativeInput_ResetMouseButtons();
	NativeInput_ResetTouchContacts();
	s_submitNameKey = 0;

	if (s_inputInitialized == 0)
	{
		return;
	}

	// Publish a released snapshot before UIKit removes CPU time. This prevents
	// a key, button, or axis held at the resign-active boundary from sticking
	// in the PSX-shaped pad bus across foreground restoration.
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		NativeInput_ResetSnapshot(slot);
	}
	NativeInput_WritePadBus();
}

void Platform_InputResume(void)
{
	// Clear transport edges accumulated before the foreground boundary, then
	// repair any controller ownership whose disconnect arrived while suspended.
	NativeInput_ClearKeyboardLatch();
	NativeInput_ResetMouseButtons();
	NativeInput_ResetTouchContacts();
	s_submitNameKey = 0;
	if (s_inputInitialized != 0)
	{
		NativeInput_CloseDisconnectedControllers();
		NativeInput_OpenKnownControllers();
	}
}

void Platform_InputControllerAdded(int deviceIndex)
{
	s32 slot;

	if (s_inputInitialized == 0)
	{
		return;
	}

	if (Platform_LogIsOpen())
	{
		Platform_Log("[CTR Input] controller-added instance=%u\n", (unsigned int)(SDL_JoystickID)deviceIndex);
	}
	NativeInput_CloseDisconnectedControllers();
	slot = NativeInput_FindSlotForDeviceIndex(deviceIndex);
	if (slot >= 0)
	{
		NativeInput_OpenController(deviceIndex, slot);
	}
}

void Platform_InputControllerRemoved(int instanceId)
{
	s32 slot;

	if (Platform_LogIsOpen())
	{
		Platform_Log("[CTR Input] controller-removed instance=%u\n", (unsigned int)(SDL_JoystickID)instanceId);
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if (s_controllers[slot].instanceId == (SDL_JoystickID)instanceId)
		{
			NativeInput_CloseController(slot);
			return;
		}
	}
}

int Platform_InputCycleKeyboardController(void)
{
	s_keyboardControllerSlot = NativeInput_NextControllerSlot(s_keyboardControllerSlot);
	return s_keyboardControllerSlot + 1;
}

int Platform_InputCycleGamepadController(void)
{
	s32 slot = NativeInput_FindActiveControllerSlot();
	s32 nextSlot;

	if (slot < 0)
	{
		return 0;
	}

	nextSlot = NativeInput_NextControllerSlot(slot);
	NativeInput_SwapControllerSlots(slot, nextSlot);
	NativeInput_WritePadBus();
	return nextSlot + 1;
}

void Platform_InputPadInit(int slot, unsigned char *padData)
{
	if ((slot < 0) || (slot >= NATIVE_INPUT_PHYSICAL_SLOT_COUNT))
	{
		return;
	}

	s_padSlotData[slot] = padData;
	NativeInput_WritePadBus();
}

int Platform_InputPadGetState(int port)
{
	s32 physicalSlot = (port >> 4) & 1;
	s32 tap = port & 3;
	s32 slot;

	if (NativeInput_UseMultitapBus() != 0)
	{
		if (physicalSlot != 0)
		{
			return PadStateDiscon;
		}
		slot = tap;
	}
	else
	{
		if (tap != 0)
		{
			return PadStateDiscon;
		}
		slot = physicalSlot;
	}

	if ((slot < 0) || (slot >= NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return PadStateDiscon;
	}

	return s_controllers[slot].snapshot.connected ? PadStateStable : PadStateDiscon;
}

int Platform_InputCapturePadSnapshots(struct PlatformInputPadSnapshot *dst, int count)
{
	s32 slot;

	if ((dst == NULL) || (count < NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return 0;
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		dst[slot] = s_controllers[slot].snapshot;
	}

	return NATIVE_INPUT_MAX_CONTROLLERS;
}

int Platform_InputInstallPadSnapshots(const struct PlatformInputPadSnapshot *src, int count)
{
	s32 slot;

	if ((src == NULL) || (count < NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return 0;
	}

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		s_installedSnapshots[slot] = src[slot];
	}

	s_installedSnapshotsActive = 1;
	NativeInput_WriteInstalledSnapshots();
	return NATIVE_INPUT_MAX_CONTROLLERS;
}

void Platform_InputClearInstalledPadSnapshots(void)
{
	s_installedSnapshotsActive = 0;
}

int Platform_InputUpgradeLegacySubmitNameSnapshots(struct PlatformInputPadSnapshot *snapshots, int count)
{
	if ((snapshots == NULL) || (count != NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return 0;
	}

	for (s32 slot = 0; slot < count; slot++)
	{
		struct PlatformInputPadSnapshot *snapshot = &snapshots[slot];
		s32 submitNameKey = 0;

		if (snapshot->reserved[0] == NATIVE_INPUT_SUBMIT_NAME_KEY_MARKER)
		{
			continue;
		}

		// Legacy version-2 reports did not serialize host-only name-entry
		// shortcuts. Their keyboard Enter mapping is recoverable from the raw
		// Start bit; other keyboard scancodes are not.
		if ((snapshot->connected != 0) && ((NativeInput_GetSnapshotButtons(snapshot) & 0x8) == 0))
		{
			submitNameKey = SDL_SCANCODE_RETURN;
		}
		NativeInput_SetSnapshotSubmitNameKey(snapshot, submitNameKey);
	}

	return 1;
}

void Platform_InputSetSubmitNameKey(int key, int down)
{
	s_submitNameKey = (down != 0) ? key : 0;
}

int Platform_InputGetSubmitNameKey(void)
{
	s32 slot;
	s32 hasSubmitNameKeyMetadata = 0;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		const struct PlatformInputPadSnapshot *snapshot = &s_controllers[slot].snapshot;
		s32 key;

		if (snapshot->reserved[0] != NATIVE_INPUT_SUBMIT_NAME_KEY_MARKER)
		{
			continue;
		}
		hasSubmitNameKeyMetadata = 1;
		key = NativeInput_GetSnapshotSubmitNameKey(snapshot);

		if (key != 0)
		{
			return key;
		}
	}

	// Version-2 replay records predate native shortcut metadata. Enter was
	// already mapped to raw Start, so recover that one legacy shortcut only
	// while replay/state snapshots are installed. Live gamepad Start remains
	// retail input and is not treated as a host keyboard shortcut.
	if ((s_installedSnapshotsActive != 0) && (hasSubmitNameKeyMetadata == 0))
	{
		for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
		{
			const struct PlatformInputPadSnapshot *snapshot = &s_controllers[slot].snapshot;

			if ((snapshot->connected != 0) && ((NativeInput_GetSnapshotButtons(snapshot) & 0x8) == 0))
			{
				return SDL_SCANCODE_RETURN;
			}
		}
	}

	return 0;
}

struct NativeInputVirtualRumbleProbe
{
	u16 lowFrequency;
	u16 highFrequency;
	s32 callCount;
};

internal bool SDLCALL NativeInput_VirtualRumble(void *userdata, Uint16 lowFrequency, Uint16 highFrequency)
{
	struct NativeInputVirtualRumbleProbe *probe = userdata;

	if (probe != NULL)
	{
		probe->lowFrequency = lowFrequency;
		probe->highFrequency = highFrequency;
		probe->callCount++;
	}

	return true;
}

internal s32 NativeInput_RunVirtualControllerSelfTest(void)
{
	struct NativeInputVirtualRumbleProbe rumbleProbe = {0};
	SDL_VirtualJoystickDesc virtualDesc;
	SDL_JoystickID virtualId = 0;
	SDL_JoystickID reconnectedVirtualId = 0;
	SDL_JoystickID secondVirtualId = 0;
	SDL_Joystick *virtualJoystick = NULL;
	const char *failure = NULL;
	unsigned char rumbleTable[2] = {0x40, 0x80};
	u16 buttons;
	s32 openControllerCount;
	s32 slot;

	if (SDL_InitSubSystem(SDL_INIT_GAMEPAD) == 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: SDL virtual gamepad init: %s\n", SDL_GetError());
		return 0;
	}

	SDL_INIT_INTERFACE(&virtualDesc);
	virtualDesc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	virtualDesc.naxes = SDL_GAMEPAD_AXIS_COUNT;
	virtualDesc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
	virtualDesc.name = "CTR Native self-test gamepad";
	virtualDesc.userdata = &rumbleProbe;
	virtualDesc.Rumble = NativeInput_VirtualRumble;

	virtualId = SDL_AttachVirtualJoystick(&virtualDesc);
	if (virtualId == 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: attach virtual gamepad: %s\n", SDL_GetError());
		failure = "attach virtual gamepad";
		goto CLEANUP;
	}

	s_inputInitialized = 1;
	Platform_InputControllerAdded(virtualId);
	if ((s_controllers[0].controller == NULL) || (s_controllers[0].instanceId != virtualId) ||
	    ((SDL_JoystickID)s_controllerToSlotMapping[0] != virtualId) ||
	    (s_keyboardControllerSlot != (NativeInput_SharePrimaryInputSources() != 0 ? 0 : 1)))
	{
		failure = "virtual gamepad slot ownership";
		goto CLEANUP;
	}

	Platform_InputControllerAdded(virtualId);
	openControllerCount = 0;
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if (s_controllers[slot].controller != NULL)
		{
			openControllerCount++;
		}
	}
	if (openControllerCount != 1)
	{
		failure = "duplicate controller add opened a second slot";
		goto CLEANUP;
	}

	virtualJoystick = SDL_GetGamepadJoystick(s_controllers[0].controller);
	if ((virtualJoystick == NULL) ||
	    !SDL_SetJoystickVirtualButton(virtualJoystick, SDL_GAMEPAD_BUTTON_SOUTH, true) ||
	    !SDL_SetJoystickVirtualButton(virtualJoystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true) ||
	    !SDL_SetJoystickVirtualAxis(virtualJoystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 24576) ||
	    !SDL_SetJoystickVirtualAxis(virtualJoystick, SDL_GAMEPAD_AXIS_LEFTX, -32768) ||
	    !SDL_SetJoystickVirtualAxis(virtualJoystick, SDL_GAMEPAD_AXIS_RIGHTY, 32767))
	{
		failure = "set virtual gamepad state";
		goto CLEANUP;
	}

	SDL_UpdateJoysticks();
	NativeInput_ResetSnapshot(0);
	NativeInput_ApplyController(0);
	buttons = NativeInput_GetSnapshotButtons(&s_controllers[0].snapshot);
	if ((s_controllers[0].snapshot.connected == 0) || (s_controllers[0].snapshot.id != NATIVE_INPUT_PAD_ANALOG) ||
	    (buttons != 0xb5ff) || (s_controllers[0].snapshot.analog[0] != 0x80) ||
	    (s_controllers[0].snapshot.analog[1] != 0xff) || (s_controllers[0].snapshot.analog[2] != 0x00) ||
	    (s_controllers[0].snapshot.analog[3] != 0x80) || (s_lastActiveControllerSlot != 0))
	{
		failure = "virtual gamepad PS1 snapshot";
		goto CLEANUP;
	}

	Platform_InputTouchSetEnabled(1);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_SQUARE, 1);
	Platform_InputTouchLeftStick(32767, -16384, 1);
	NativeInput_ApplyTouch(0, NativeInput_ConsumeTouchButtons());
	buttons = NativeInput_GetSnapshotButtons(&s_controllers[0].snapshot);
	if ((buttons != 0x35ff) || (s_controllers[0].snapshot.analog[0] != 0x80) ||
	    (s_controllers[0].snapshot.analog[1] != 0xff) || (s_controllers[0].snapshot.analog[2] != 0xff) ||
	    (s_controllers[0].snapshot.analog[3] != 0x40))
	{
		failure = "touch and gamepad composition";
		goto CLEANUP;
	}
	Platform_InputTouchSetEnabled(0);

	Platform_InputPadVibrate(0, rumbleTable, sizeof(rumbleTable));
	if ((rumbleProbe.callCount != 1) || (rumbleProbe.lowFrequency != 32640) || (rumbleProbe.highFrequency != 16320))
	{
		failure = "virtual gamepad rumble";
		goto CLEANUP;
	}

	Platform_InputControllerRemoved(virtualId);
	if ((s_controllers[0].controller != NULL) || (s_controllerToSlotMapping[0] != -1))
	{
		failure = "virtual gamepad removal";
		goto CLEANUP;
	}

	Platform_InputControllerAdded(virtualId);
	if ((s_controllers[0].controller == NULL) || ((SDL_JoystickID)s_controllerToSlotMapping[0] != virtualId))
	{
		failure = "virtual gamepad reconnect";
		goto CLEANUP;
	}

	// A sleeping wireless controller can disappear before its queued removal
	// event reaches the host. Its replacement must reclaim the disconnected
	// slot instead of silently becoming player 2.
	if (!SDL_DetachVirtualJoystick(virtualId))
	{
		failure = "detach stale virtual gamepad";
		goto CLEANUP;
	}
	virtualId = 0;
	reconnectedVirtualId = SDL_AttachVirtualJoystick(&virtualDesc);
	if (reconnectedVirtualId == 0)
	{
		failure = "attach replacement virtual gamepad";
		goto CLEANUP;
	}
	Platform_InputControllerAdded(reconnectedVirtualId);
	if ((s_controllers[0].controller == NULL) || (s_controllers[0].instanceId != reconnectedVirtualId) ||
	    ((SDL_JoystickID)s_controllerToSlotMapping[0] != reconnectedVirtualId) || (s_controllers[1].controller != NULL))
	{
		failure = "stale virtual gamepad did not reclaim player 1";
		goto CLEANUP;
	}

	secondVirtualId = SDL_AttachVirtualJoystick(&virtualDesc);
	if (secondVirtualId == 0)
	{
		failure = "attach second virtual gamepad";
		goto CLEANUP;
	}
	Platform_InputControllerAdded(secondVirtualId);
	if ((s_controllers[0].instanceId != reconnectedVirtualId) || (s_controllers[1].instanceId != secondVirtualId))
	{
		failure = "connected player 1 was displaced by player 2";
		goto CLEANUP;
	}
	Platform_InputControllerRemoved(secondVirtualId);
	if ((s_controllers[0].instanceId != reconnectedVirtualId) || (s_controllers[1].controller != NULL))
	{
		failure = "second virtual gamepad removal changed player 1";
		goto CLEANUP;
	}

CLEANUP:
	Platform_InputTouchSetEnabled(0);
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		NativeInput_CloseController(slot);
		NativeInput_ResetSnapshot(slot);
	}
	s_inputInitialized = 0;
	s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;
	s_lastActiveControllerSlot = -1;
	if ((virtualId != 0) && !SDL_DetachVirtualJoystick(virtualId) && (failure == NULL))
	{
		failure = "detach virtual gamepad";
	}
	if ((reconnectedVirtualId != 0) && !SDL_DetachVirtualJoystick(reconnectedVirtualId) && (failure == NULL))
	{
		failure = "detach replacement virtual gamepad";
	}
	if ((secondVirtualId != 0) && !SDL_DetachVirtualJoystick(secondVirtualId) && (failure == NULL))
	{
		failure = "detach second virtual gamepad";
	}
	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);

	if (failure != NULL)
	{
		fprintf(stderr, "[CTR Input] self-test failed: %s\n", failure);
		return 0;
	}

	return 1;
}

int Platform_InputRunSelfTest(void)
{
	const struct
	{
		s32 key;
		u16 buttonBit;
	} aliasExpectations[] = {
		{SDL_SCANCODE_W, 0x10},
		{SDL_SCANCODE_UP, 0x10},
		{SDL_SCANCODE_D, 0x20},
		{SDL_SCANCODE_RIGHT, 0x20},
		{SDL_SCANCODE_LSHIFT, 0x4000},
		{SDL_SCANCODE_RSHIFT, 0x4000},
		{SDL_SCANCODE_E, 0x8000},
		{SDL_SCANCODE_X, 0x8000},
		{SDL_SCANCODE_SPACE, 0x2000},
		{SDL_SCANCODE_V, 0x2000},
		{SDL_SCANCODE_ESCAPE, 0x1000},
		{SDL_SCANCODE_Z, 0x1000},
		{SDL_SCANCODE_Q, 0x400},
		{SDL_SCANCODE_R, 0x800},
		{SDL_SCANCODE_RETURN, 0x8},
		{SDL_SCANCODE_P, 0x8},
		{SDL_SCANCODE_TAB, 0x1},
	};
	struct PlatformInputPadSnapshot migrationSnapshots[NATIVE_INPUT_MAX_CONTROLLERS];
	struct PlatformInputPadSnapshot *snapshot;
	bool keyboardState[SDL_SCANCODE_COUNT];
	u16 heldButtons;
	u16 latchedButtons;
	u16 touchButtons;
	s32 slot;

	memset(s_controllers, 0, sizeof(s_controllers));
	NativeInput_DefaultMappings();
	s_keyboardState = NULL;
	NativeInput_ClearKeyboardLatch();
	Platform_InputTouchSetEnabled(0);
	s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;
	s_installedSnapshotsActive = 0;
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		s_controllerToSlotMapping[slot] = -1;
		NativeInput_ResetSnapshot(slot);
	}

	NativeInput_AssignKeyboardForController(0, 1);
	if (s_keyboardControllerSlot != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: shared primary input assignment\n");
		return 1;
	}
	NativeInput_AssignKeyboardForController(0, 0);
	if (s_keyboardControllerSlot != 1)
	{
		fprintf(stderr, "[CTR Input] self-test failed: separate controller input assignment\n");
		return 1;
	}
	s_keyboardControllerSlot = NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT;

	snapshot = &s_controllers[NATIVE_INPUT_DEFAULT_KEYBOARD_SLOT].snapshot;
	NativeInput_SetSnapshotSubmitNameKey(snapshot, SDL_SCANCODE_A);
	if (Platform_InputGetSubmitNameKey() != SDL_SCANCODE_A)
	{
		fprintf(stderr, "[CTR Input] self-test failed: snapshot metadata key\n");
		return 1;
	}

	NativeInput_SetSnapshotSubmitNameKey(snapshot, 0);
	NativeInput_SetSnapshotButtons(snapshot, 0xfff7);
	s_installedSnapshotsActive = 1;
	if (Platform_InputGetSubmitNameKey() != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: metadata did not preserve retail Start\n");
		return 1;
	}

	memset(snapshot->reserved, 0, sizeof(snapshot->reserved));
	if (Platform_InputGetSubmitNameKey() != SDL_SCANCODE_RETURN)
	{
		fprintf(stderr, "[CTR Input] self-test failed: legacy replay Enter fallback\n");
		return 1;
	}
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		migrationSnapshots[slot] = s_controllers[slot].snapshot;
	}
	if (!Platform_InputUpgradeLegacySubmitNameSnapshots(migrationSnapshots, NATIVE_INPUT_MAX_CONTROLLERS) ||
	    (NativeInput_GetSnapshotSubmitNameKey(&migrationSnapshots[0]) != SDL_SCANCODE_RETURN))
	{
		fprintf(stderr, "[CTR Input] self-test failed: legacy replay metadata migration\n");
		return 1;
	}

	NativeInput_SetSnapshotSubmitNameKey(snapshot, 0);
	s_installedSnapshotsActive = 0;
	if (Platform_InputGetSubmitNameKey() != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: live gamepad Start became keyboard Enter\n");
		return 1;
	}

	Platform_InputKeyboardEvent(SDL_SCANCODE_LSHIFT, 1);
	Platform_InputKeyboardEvent(SDL_SCANCODE_RIGHT, 1);
	Platform_InputKeyboardEvent(SDL_SCANCODE_LSHIFT, 0);
	Platform_InputKeyboardEvent(SDL_SCANCODE_RIGHT, 0);
	latchedButtons = NativeInput_ConsumeKeyboard();
	if (((latchedButtons & 0x4000) != 0) || ((latchedButtons & 0x20) != 0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: key-down tap was not latched\n");
		return 1;
	}
	latchedButtons = NativeInput_ConsumeKeyboard();
	if (((latchedButtons & 0x4000) != 0) || ((latchedButtons & 0x20) != 0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: key-down tap did not survive until retail poll\n");
		return 1;
	}
	Platform_InputAcknowledgeRetailPoll();
	if (NativeInput_ConsumeKeyboard() != 0xffff)
	{
		fprintf(stderr, "[CTR Input] self-test failed: quick tap survived retail acknowledgement\n");
		return 1;
	}

	for (slot = 0; slot < (s32)(sizeof(aliasExpectations) / sizeof(aliasExpectations[0])); slot++)
	{
		if (NativeInput_KeyboardButtonBit(aliasExpectations[slot].key) != aliasExpectations[slot].buttonBit)
		{
			fprintf(stderr, "[CTR Input] self-test failed: keyboard alias index %d\n", slot);
			return 1;
		}
	}
	if (!Platform_InputSetKeyboardBinding(PLATFORM_INPUT_KEYBOARD_CIRCLE, 1, SDL_SCANCODE_F) ||
	    (Platform_InputGetKeyboardBinding(PLATFORM_INPUT_KEYBOARD_CIRCLE, 1) != SDL_SCANCODE_F) ||
	    (NativeInput_KeyboardButtonBit(SDL_SCANCODE_F) != 0x2000))
	{
		fprintf(stderr, "[CTR Input] self-test failed: configurable keyboard binding\n");
		return 1;
	}
	if (!Platform_InputSetKeyboardBinding(PLATFORM_INPUT_KEYBOARD_CIRCLE, 1, SDL_SCANCODE_LSHIFT) ||
	    (NativeInput_KeyboardButtonBit(SDL_SCANCODE_LSHIFT) != 0x6000))
	{
		fprintf(stderr, "[CTR Input] self-test failed: duplicate keyboard binding composition\n");
		return 1;
	}
	Platform_InputResetKeyboardBindings();
	if (NativeInput_KeyboardButtonBit(SDL_SCANCODE_F) != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: keyboard binding reset\n");
		return 1;
	}

	memset(keyboardState, 0, sizeof(keyboardState));
	keyboardState[SDL_SCANCODE_RSHIFT] = true;
	keyboardState[SDL_SCANCODE_D] = true;
	keyboardState[SDL_SCANCODE_R] = true;
	s_keyboardState = keyboardState;
	heldButtons = NativeInput_ReadKeyboard();
	s_keyboardState = NULL;
	if (((heldButtons & 0x4000) != 0) || ((heldButtons & 0x20) != 0) || ((heldButtons & 0x800) != 0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: held keyboard aliases shift+d+r\n");
		return 1;
	}

	Platform_InputKeyboardEvent(SDL_SCANCODE_RSHIFT, 1);
	Platform_InputKeyboardEvent(SDL_SCANCODE_D, 1);
	Platform_InputKeyboardEvent(SDL_SCANCODE_RSHIFT, 0);
	Platform_InputKeyboardEvent(SDL_SCANCODE_D, 0);
	latchedButtons = NativeInput_ConsumeKeyboard();
	if (((latchedButtons & 0x4000) != 0) || ((latchedButtons & 0x20) != 0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: alias key-down tap was not latched\n");
		return 1;
	}
	latchedButtons = NativeInput_ConsumeKeyboard();
	if (((latchedButtons & 0x4000) != 0) || ((latchedButtons & 0x20) != 0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: alias tap did not survive until retail poll\n");
		return 1;
	}
	Platform_InputAcknowledgeRetailPoll();
	if (NativeInput_ConsumeKeyboard() != 0xffff)
	{
		fprintf(stderr, "[CTR Input] self-test failed: alias tap survived retail acknowledgement\n");
		return 1;
	}

	Platform_InputMouseButtonEvent(SDL_BUTTON_LEFT, 1);
	Platform_InputMouseButtonEvent(SDL_BUTTON_X1, 1);
	Platform_InputMouseButtonEvent(SDL_BUTTON_LEFT, 0);
	NativeInput_ResetSnapshot(0);
	NativeInput_ApplyMouse(0, NativeInput_ConsumeMouseButtons());
	snapshot = &s_controllers[0].snapshot;
	if ((snapshot->connected == 0) || (snapshot->id != NATIVE_INPUT_PAD_DIGITAL) ||
	    (NativeInput_GetSnapshotButtons(snapshot) != 0xbbff))
	{
		fprintf(stderr, "[CTR Input] self-test failed: mouse gas and drift composition\n");
		return 1;
	}
	Platform_InputAcknowledgeRetailPoll();
	Platform_InputMouseButtonEvent(SDL_BUTTON_X1, 0);
	if (NativeInput_ConsumeMouseButtons() != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: released mouse chord survived\n");
		return 1;
	}

	Platform_InputTouchSetEnabled(1);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_CROSS, 1);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_R1, 1);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_DOWN, 1);
	Platform_InputTouchLeftStick(-32768, 16384, 1);
	touchButtons = NativeInput_ConsumeTouchButtons();
	NativeInput_ResetSnapshot(0);
	NativeInput_ApplyTouch(0, touchButtons);
	snapshot = &s_controllers[0].snapshot;
	if ((snapshot->connected == 0) || (snapshot->id != NATIVE_INPUT_PAD_ANALOG) ||
	    (NativeInput_GetSnapshotButtons(snapshot) != 0xb7bf) || (snapshot->analog[2] != 0x00) ||
	    (snapshot->analog[3] != 0xc0))
	{
		fprintf(stderr, "[CTR Input] self-test failed: touch analog chord\n");
		return 1;
	}
	touchButtons = NativeInput_ConsumeTouchButtons();
	if ((touchButtons & (PLATFORM_INPUT_TOUCH_CROSS | PLATFORM_INPUT_TOUCH_R1 | PLATFORM_INPUT_TOUCH_DOWN)) !=
	    (PLATFORM_INPUT_TOUCH_CROSS | PLATFORM_INPUT_TOUCH_R1 | PLATFORM_INPUT_TOUCH_DOWN))
	{
		fprintf(stderr, "[CTR Input] self-test failed: touch chord did not survive until retail poll\n");
		return 1;
	}
	Platform_InputAcknowledgeRetailPoll();
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_CROSS, 0);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_R1, 0);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_DOWN, 0);
	Platform_InputTouchLeftStick(0, 0, 0);
	if (NativeInput_ConsumeTouchButtons() != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: released touch chord survived\n");
		return 1;
	}
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_CIRCLE, 1);
	Platform_InputTouchButton(PLATFORM_INPUT_TOUCH_CIRCLE, 0);
	if ((NativeInput_ConsumeTouchButtons() & PLATFORM_INPUT_TOUCH_CIRCLE) == 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: touch tap was not latched\n");
		return 1;
	}
	if ((NativeInput_ConsumeTouchButtons() & PLATFORM_INPUT_TOUCH_CIRCLE) == 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: touch tap did not survive until retail poll\n");
		return 1;
	}
	Platform_InputAcknowledgeRetailPoll();
	if (NativeInput_ConsumeTouchButtons() != 0)
	{
		fprintf(stderr, "[CTR Input] self-test failed: touch tap survived retail acknowledgement\n");
		return 1;
	}
	Platform_InputTouchSetEnabled(0);
	if (!NativeInput_RunVirtualControllerSelfTest())
	{
		return 1;
	}

	printf("[CTR Input] self-test passed: metadata-key=%d legacy-enter=%d migration-enter=%d live-start=retail tap-latch=shift+right until-retail-poll aliases=17 remap+duplicate+reset=passed held=shift+d+r alias-tap=shift+d mouse=gas+brake+item+l1+r1 primary-share=keyboard+mouse+touch+gamepad virtual-gamepad=buttons+axes+rumble+hotplug stale-reclaim=player1 multiplayer=preserved\n",
	       SDL_SCANCODE_A, SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN);
	return 0;
}

int Platform_InputGetStateSize(void)
{
	return (int)sizeof(struct NativeInputStateSnapshot);
}

int Platform_InputCaptureState(void *dst, int dstSize)
{
	struct NativeInputStateSnapshot *snapshot = (struct NativeInputStateSnapshot *)dst;
	s32 slot;

	if ((dst == NULL) || (dstSize < (int)sizeof(*snapshot)))
	{
		return 0;
	}

	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->magic = NATIVE_INPUT_STATE_MAGIC;
	snapshot->version = NATIVE_INPUT_STATE_VERSION;
	snapshot->size = sizeof(*snapshot);
	snapshot->keyboardControllerSlot = s_keyboardControllerSlot;
	snapshot->lastActiveControllerSlot = s_lastActiveControllerSlot;
	snapshot->installedSnapshotsActive = s_installedSnapshotsActive;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		snapshot->installedSnapshots[slot] = s_installedSnapshots[slot];
		snapshot->controllers[slot].snapshot = s_controllers[slot].snapshot;
		snapshot->controllers[slot].analogEnabled = s_controllers[slot].analogEnabled;
		snapshot->controllers[slot].switchingAnalog = s_controllers[slot].switchingAnalog;
		snapshot->controllers[slot].controllerToSlotMapping = s_controllerToSlotMapping[slot];
	}

	return 1;
}

int Platform_InputRestoreState(const void *src, int srcSize)
{
	const struct NativeInputStateSnapshot *snapshot = (const struct NativeInputStateSnapshot *)src;
	s32 slot;

	if ((src == NULL) || (srcSize < (int)sizeof(*snapshot)))
	{
		return 0;
	}
	if ((snapshot->magic != NATIVE_INPUT_STATE_MAGIC) || (snapshot->version != NATIVE_INPUT_STATE_VERSION) || (snapshot->size != sizeof(*snapshot)))
	{
		return 0;
	}
	if (!NativeInput_IsValidControllerSlot(snapshot->keyboardControllerSlot))
	{
		return 0;
	}
	if ((snapshot->lastActiveControllerSlot < -1) || (snapshot->lastActiveControllerSlot >= NATIVE_INPUT_MAX_CONTROLLERS))
	{
		return 0;
	}
	if ((snapshot->installedSnapshotsActive < 0) || (snapshot->installedSnapshotsActive > 1))
	{
		return 0;
	}
	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		if ((snapshot->controllers[slot].analogEnabled < 0) || (snapshot->controllers[slot].analogEnabled > 1))
		{
			return 0;
		}
		if ((snapshot->controllers[slot].switchingAnalog < 0) || (snapshot->controllers[slot].switchingAnalog > 1))
		{
			return 0;
		}
		if (snapshot->controllers[slot].controllerToSlotMapping < -1)
		{
			return 0;
		}
	}

	s_keyboardControllerSlot = snapshot->keyboardControllerSlot;
	s_lastActiveControllerSlot = snapshot->lastActiveControllerSlot;
	s_installedSnapshotsActive = snapshot->installedSnapshotsActive != 0;

	for (slot = 0; slot < NATIVE_INPUT_MAX_CONTROLLERS; slot++)
	{
		s_installedSnapshots[slot] = snapshot->installedSnapshots[slot];
		s_controllers[slot].snapshot = snapshot->controllers[slot].snapshot;
		s_controllers[slot].analogEnabled = snapshot->controllers[slot].analogEnabled;
		s_controllers[slot].switchingAnalog = snapshot->controllers[slot].switchingAnalog;
		s_controllerToSlotMapping[slot] = snapshot->controllers[slot].controllerToSlotMapping;
	}
	s_submitNameKey = NativeInput_GetSnapshotSubmitNameKey(&s_controllers[s_keyboardControllerSlot].snapshot);
	NativeInput_ClearKeyboardLatch();
	NativeInput_ResetTouchContacts();
	NativeInput_WritePadBus();

	return 1;
}

void Platform_InputPadVibrate(int port, unsigned char *table, int len)
{
	s32 physicalSlot = (port >> 4) & 1;
	s32 tap = port & 3;
	s32 slot;
	struct NativeInputController *controller;
	u16 freqHigh;
	u16 freqLow;

	if (NativeInput_UseMultitapBus() != 0)
	{
		if (physicalSlot != 0)
		{
			return;
		}
		slot = tap;
	}
	else
	{
		if (tap != 0)
		{
			return;
		}
		slot = physicalSlot;
	}

	if ((slot < 0) || (slot >= NATIVE_INPUT_MAX_CONTROLLERS) || (table == NULL) || (len <= 0))
	{
		return;
	}

	controller = &s_controllers[slot];
	if (controller->controller == NULL)
	{
		return;
	}

	freqHigh = table[0] * 255;
	freqLow = len > 1 ? table[1] * 255 : 0;

	if ((freqLow != 0) && (freqLow < 4096))
	{
		freqLow = 4096;
	}

	if ((freqHigh != 0) && (freqHigh < 4096))
	{
		freqHigh = 4096;
	}

	SDL_RumbleGamepad(controller->controller, freqLow, freqHigh, 200);
}
