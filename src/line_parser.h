// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct line_parser_s {
	const char *source;
	char *buffer;
	char *tokenCursor;
	u32 bufferSize;
	u32 lineIndex;
	char commentCharacter;
	u8 pad[7];
} line_parser_t;

typedef struct parser_token_s {
	char *text;
	u32 length;
	u8 pad[4];
} parser_token_t;

void line_parser_init(line_parser_t *parser, const char *source, char *buffer);
u32 line_parser_read_version(line_parser_t *parser);
char *line_parser_next_line(line_parser_t *parser);
char *line_parser_next_token(line_parser_t *parser);
b32 line_parser_error(line_parser_t *parser, const char *reason);

#if defined(__cplusplus)
}
#endif
