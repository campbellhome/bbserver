// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"
#include "span.h"

#if defined(__cplusplus)
extern "C" {
#endif

AUTOJSON typedef enum vfilter_type_e {
	kVF_Standard,
	kVF_SQL,
	kVF_Legacy,
	kVF_Count
} vfilter_type_e;

AUTOJSON typedef enum vfilter_token_type_e {
	kVFT_Invalid,
	kVFT_OpenParen,
	kVFT_CloseParen,
	kVFT_String,
	kVFT_LessThan,
	kVFT_LessThanEquals,
	kVFT_Equals,
	kVFT_NotEquals,
	kVFT_GreaterThan,
	kVFT_GreaterThanEquals,
	kVFT_Like,
	kVFT_And,
	kVFT_Or,
	kVFT_Not,
	kVFT_Contains,
	kVFT_StartsWith,
	kVFT_EndsWith,
	kVFT_DeltaMillisecondsAbsolute,
	kVFT_DeltaMillisecondsViewRelative,
	kVFT_Filename,
	kVFT_Thread,
	kVFT_PIEInstance,
	kVFT_Category,
	kVFT_Verbosity,
	kVFT_Text,
	kVFT_Number,
	kVFT_Count
} vfilter_token_type_e;

AUTOSTRUCT typedef struct vfilter_token_s {
	span_t span;
	vfilter_token_type_e type;
	u32 number;
} vfilter_token_t;

AUTOSTRUCT typedef struct vfilter_tokens_s {
	u32 count;
	u32 allocated;
	vfilter_token_t *data;
} vfilter_tokens_t;

AUTOSTRUCT typedef struct vfilter_error_s {
	sb_t text;
	s32 column;
	u8 pad[4];
} vfilter_error_t;

AUTOSTRUCT typedef struct vfilter_s {
	vfilter_tokens_t tokens;
	vfilter_error_t error;
	vfilter_type_e type;
	u8 pad[4];
} vfilter_t;

#if defined(__cplusplus)
}
#endif
