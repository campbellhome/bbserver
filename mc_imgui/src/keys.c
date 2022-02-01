// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "keys.h"
#include "time_utils.h"

typedef struct key_data_s {
	b32 pressed;
	u8 pad[4];
	double time;
} key_data_t;

static key_data_t s_keys[Key_Count];

void key_clear_all(void)
{
	double now = Time_GetCurrentTime();
	int i;
	for(i = 0; i < Key_Count; ++i) {
		if(s_keys[i].pressed) {
			s_keys[i].pressed = false;
			s_keys[i].time = now;
		}
	}
}

void key_on_pressed(key_e key)
{
	if(!s_keys[key].pressed) {
		s_keys[key].pressed = true;
		s_keys[key].time = Time_GetCurrentTime();
	}
}

void key_on_released(key_e key)
{
	if(s_keys[key].pressed) {
		s_keys[key].pressed = false;
		s_keys[key].time = Time_GetCurrentTime();
	}
}

b32 key_is_pressed_this_frame(key_e key)
{
	return s_keys[key].pressed && s_keys[key].time == Time_GetCurrentTime();
}

b32 key_is_down(key_e key)
{
	return s_keys[key].pressed;
}

b32 key_is_released_this_frame(key_e key)
{
	return !s_keys[key].pressed && s_keys[key].time == Time_GetCurrentTime();
}

b32 key_is_any_down_or_released_this_frame(void)
{
	double now = Time_GetCurrentTime();
	int i;
	for(i = 0; i < Key_Count; ++i) {
		if(s_keys[i].pressed || s_keys[i].time == now) {
			return true;
		}
	}
	return false;
}
