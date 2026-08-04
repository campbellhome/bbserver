// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

typedef struct view_s view_t;

bool LogTable_Update(view_t *view, b32 otherControlFocused);
void LogTable_Shutdown(void);
