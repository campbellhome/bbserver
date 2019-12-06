// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_messageBoxes messageBoxes;
extern messageBoxes g_messageboxes;
extern float g_messageboxHeight;

#if defined(__cplusplus)
}
#endif

float UIBlackboxMessageBox_Update(messageBoxes *boxes);
