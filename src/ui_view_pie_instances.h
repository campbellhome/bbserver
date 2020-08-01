// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

typedef struct view_s view_t;

void UIViewPieInstances_Update(view_t *view);
void UIViewPieInstances_Shutdown();
