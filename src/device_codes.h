// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb.h"
#include "bb_criticalsection.h"
#include "sb.h"

AUTOJSON typedef struct deviceCodes_s
{
	sbs_t deviceCodes;
} deviceCodes_t;

const sbs_t* deviceCodes_lock(void);
void deviceCodes_unlock(void);

void deviceCodes_init(void);
void deviceCodes_shutdown(void);

LRESULT WINAPI deviceCodes_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#if defined(__cplusplus)
}
#endif
