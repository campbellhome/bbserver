// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct sdict_s sdict_t;

typedef struct tag_filterToken {
	sb_t category;
	sb_t text;
	b32 prohibited;
	b32 required;
	b32 exact;
	u8 pad[4];
} filterToken;

typedef struct tag_filterTokens {
	u32 count;
	u32 allocated;
	filterToken *data;
} filterTokens;

void reset_filter_tokens(filterTokens *tokens);
filterToken *add_filter_token(filterTokens *tokens, const char *category, const char *text);
void build_filter_tokens(filterTokens *tokens, const char *src);
b32 passes_filter_tokens(filterTokens *tokens, sdict_t *sd, const char **keys, u32 numKeys);

#if defined(__cplusplus)
}
#endif
