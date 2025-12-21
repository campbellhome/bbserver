// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

typedef struct view_s view_t;

void UIRecordedView_ShutdownFilter(void);
void UIRecordedView_ShowFilterTooltip(view_t* view);
bool UIRecordedView_UpdateFilter(view_t* view);
