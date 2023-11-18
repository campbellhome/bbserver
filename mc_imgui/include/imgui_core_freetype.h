// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#define FEATURE_FREETYPE BB_ON

#if defined(__cplusplus)
extern "C" {
#endif

void Imgui_Core_Freetype_Init(void);
void Imgui_Core_Freetype_Shutdown(void);
b32 Imgui_Core_Freetype_Valid(void);

#if defined(__cplusplus)
}
#endif
