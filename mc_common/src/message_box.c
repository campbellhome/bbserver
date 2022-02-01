// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "message_box.h"
#include "bb_array.h"

static messageBoxes s_mb;

void mb_reset(messageBox *mb)
{
	sdict_reset(&mb->data);
}

void mb_queue(messageBox mb, messageBoxes *boxes)
{
	if(!boxes) {
		boxes = &s_mb;
	}
	if(bba_add_noclear(*boxes, 1)) {
		bba_last(*boxes) = mb;
	} else {
		mb_reset(&mb);
	}
}

messageBox *mb_get_active(messageBoxes *boxes)
{
	if(!boxes) {
		boxes = &s_mb;
	}
	if(boxes->count) {
		return boxes->data;
	} else {
		return NULL;
	}
}

void mb_remove_active(messageBoxes *boxes)
{
	if(!boxes) {
		boxes = &s_mb;
	}
	if(boxes->count) {
		mb_reset(boxes->data);
		bba_erase(*boxes, 0);
	}
}

void mb_shutdown(messageBoxes *boxes)
{
	if(!boxes) {
		boxes = &s_mb;
	}
	for(u32 i = 0; i < boxes->count; ++i) {
		mb_reset(boxes->data + i);
	}
	bba_free(*boxes);
}

messageBoxes *mb_get_queue(void)
{
	return &s_mb;
}
