// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sdict.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_messageBox messageBox;
typedef void(messageBoxFunc)(messageBox* mb, const char* action);

typedef struct tag_messageBox
{
	sdict_t data;
	messageBoxFunc* callback;
	void* userData;
} messageBox;

typedef struct tag_messageBoxes
{
	u32 count;
	u32 allocated;
	messageBox* data;
	u32 bgColor[2];
	b32 modal;
	b32 manualUpdate;
} messageBoxes;

void mb_queue(messageBox mb, messageBoxes* boxes);
messageBox* mb_get_active(messageBoxes* boxes);
void mb_remove_active(messageBoxes* boxes);
void mb_shutdown(messageBoxes* boxes);
messageBoxes* mb_get_queue(void);

#if defined(__cplusplus)
}
#endif
