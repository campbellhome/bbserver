// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"
#include "span.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct recorded_log_s recorded_log_t;
typedef struct view_s view_t;

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
	kVFT_Matches,
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
	kVFT_NamedFilter,
	kVFT_Count
} vfilter_token_type_e;

AUTOSTRUCT typedef struct vfilter_token_s
{
	span_t span;
	vfilter_token_type_e type;
	u32 number;
} vfilter_token_t;

AUTOSTRUCT typedef struct vfilter_tokens_s
{
	u32 count;
	u32 allocated;
	vfilter_token_t* data;
} vfilter_tokens_t;

AUTOSTRUCT typedef struct vfilter_error_s
{
	sb_t text;
	s32 column;
	u8 pad[4];
} vfilter_error_t;

AUTOSTRUCT typedef struct vfilter_result_s
{
	b32 value;
} vfilter_result_t;

AUTOSTRUCT typedef struct vfilter_results_s
{
	u32 count;
	u32 allocated;
	vfilter_result_t* data;
} vfilter_results_t;

typedef struct vfilter_s vfilter_t;
AUTOSTRUCT typedef struct named_vfilters_s
{
	u32 count;
	u32 allocated;
	vfilter_t* data;
} named_vfilters_t;

AUTOSTRUCT typedef struct vfilter_s
{
	sb_t name;
	sb_t input;
	sb_t tokenstream;
	vfilter_tokens_t tokens;
	vfilter_tokens_t rpn_tokens;
	vfilter_error_t error;
	vfilter_results_t results;
	vfilter_type_e type;
	b32 valid;
} vfilter_t;
void vfilter_reset(vfilter_t* val);

vfilter_t view_filter_parse(const char* name, const char* input);
const char* view_filter_get_error_string(vfilter_t* filter);
b32 view_filter_visible(view_t* view, recorded_log_t* log);

#if defined(__cplusplus)
}
#endif
