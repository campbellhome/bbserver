// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

typedef struct view_s view_t;

void UITags_Update(view_t* view);
void UITags_Shutdown();

void UITags_Category_ClearSelection(view_t* view);
void UITags_Category_SetSelected(view_t* view, u32 viewCategoryIndex, b32 selected);
