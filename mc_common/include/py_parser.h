// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sdict.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum tag_pyParserState {
	kParser_Idle,
	kParser_Dict,
	kParser_Error,
} pyParserState;

typedef struct tag_pyParser {
	u32 count;
	u32 allocated;
	s8 *data;
	const char *cmdline;
	u32 consumed;
	pyParserState state;
	sdict_t dict;
} pyParser;

b32 py_parser_tick(pyParser *parser, sdicts *dicts, b32 bDebug);

typedef struct tag_pyWriter {
	u32 count;
	u32 allocated;
	s8 *data;
} pyWriter;

b32 py_write_sdict(pyWriter *fd, sdict_t *sd);
b32 py_write_sdicts(pyWriter *fd, sdicts *dicts);

#if defined(__cplusplus)
}
#endif
