#ifndef PLATFORM_NATIVE_IOS_TOUCH_H
#define PLATFORM_NATIVE_IOS_TOUCH_H

typedef void (*NativeIOSTouchDiscReselectionCallback)(void *userdata);

int NativeIOSTouch_Begin(NativeIOSTouchDiscReselectionCallback discReselectionCallback, void *userdata);
void NativeIOSTouch_End(void);
int NativeIOSTouch_IsConfiguring(void);

#endif
