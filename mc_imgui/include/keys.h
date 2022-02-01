// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif                             // #if defined( _MSC_VER )

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,
	Key_Tilde,
	Key_Count,
} key_e;

void key_clear_all(void);
void key_on_pressed(key_e key);
void key_on_released(key_e key);
b32 key_is_pressed_this_frame(key_e key);
b32 key_is_down(key_e key);
b32 key_is_released_this_frame(key_e key);
b32 key_is_any_down_or_released_this_frame(void);

#if defined(__cplusplus)
}
#endif
