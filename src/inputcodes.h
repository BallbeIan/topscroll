#pragma once

// The Scroller speaks the Linux evdev event vocabulary. On Linux that comes
// straight from the kernel headers; elsewhere (Windows) the touchpad backend
// translates into the same codes, so the values the Scroller needs are
// defined here with the kernel's numbers.

#ifdef __linux__
#include <linux/input.h>
#else

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_MSC 0x04

#define SYN_REPORT 0
#define SYN_DROPPED 3

#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_TOOL_FINGER 0x145
#define BTN_TOOL_QUINTTAP 0x148
#define BTN_TOUCH 0x14a
#define BTN_TOOL_DOUBLETAP 0x14d
#define BTN_TOOL_TRIPLETAP 0x14e
#define BTN_TOOL_QUADTAP 0x14f

#define REL_HWHEEL 0x06
#define REL_WHEEL 0x08
#define REL_WHEEL_HI_RES 0x0b
#define REL_HWHEEL_HI_RES 0x0c

#define ABS_X 0x00
#define ABS_Y 0x01
#define ABS_MT_SLOT 0x2f
#define ABS_MT_TOUCH_MAJOR 0x30
#define ABS_MT_POSITION_X 0x35
#define ABS_MT_POSITION_Y 0x36
#define ABS_MT_TRACKING_ID 0x39
#define ABS_MT_PRESSURE 0x3a

#endif
