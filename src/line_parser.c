// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "line_parser.h"
#include "va.h"

#include "bb.h"
#include "bbclient/bb_string.h"

#include <stdlib.h>

static char *next_token_inplace(char **bufferCursor, const char *delimiters, u32 *lineIndex, char commentCharacter)
{
	char *buffer = *bufferCursor;
	char *original = buffer;
	char *start, *end;
	b32 isPastComments = false;
	b32 isQuotedString;
	while(!isPastComments) {
		if(!buffer) {
			return NULL;
		}

		// skip whitespace
		while(*buffer && strchr(delimiters, *buffer)) {
			++buffer;
		}

		if(!*buffer) {
			return NULL;
		}

		// skip comments
		if(*buffer == commentCharacter) {
			buffer = strchr(buffer, '\n');
			if(buffer) {
				++buffer;
			}
		} else {
			isPastComments = true;
		}
	}

	// look for '\"'
	isQuotedString = (*buffer == '\"');
	if(isQuotedString) {
		++buffer;
	}

	// step to end of string
	start = buffer;
	if(isQuotedString) {
		while(*buffer && *buffer != '\"' && *buffer != '\n') {
			++buffer;
		}
	} else {
		while(*buffer && !strchr(delimiters, *buffer)) {
			++buffer;
		}
	}

	// remove trailing '\"'
	end = buffer;
	if(isQuotedString && *end == '\"') {
		buffer++;
	}

	if(lineIndex) {
		while(original <= buffer) {
			if(*original++ == '\n') {
				++*lineIndex;
			}
		}
	}

	*bufferCursor = (*buffer) ? buffer + 1 : buffer;
	*end = '\0';
	return start;
}

char *line_parser_next_line(line_parser_t *parser)
{
	char *line = next_token_inplace(&parser->buffer, "\r\n", &parser->lineIndex, parser->commentCharacter);
	parser->tokenCursor = line;
	return line;
}

char *line_parser_next_token(line_parser_t *parser)
{
	return next_token_inplace(&parser->tokenCursor, " \t", NULL, parser->commentCharacter);
}

b32 line_parser_error(line_parser_t *parser, const char *reason)
{
	BB_ERROR("Config::Parser", "%s:%u: %s\n", parser->source, parser->lineIndex, reason);
	return 0;
}

void line_parser_init(line_parser_t *parser, const char *source, char *buffer)
{
	memset(parser, 0, sizeof(*parser));
	parser->source = source;
	parser->buffer = buffer;
	parser->commentCharacter = '#';
}

u32 line_parser_read_version(line_parser_t *parser)
{
	char *token;
	u32 version;
	line_parser_next_line(parser);
	token = line_parser_next_token(parser);
	if(!token)
		return line_parser_error(parser, "missing initial version");
	if(*token != '[') {
		return line_parser_error(parser, va("expected initial version - instead saw '%s'", token));
	} else {
		char *end = token + 1;
		while(*end && *end != ']') {
			if(*end < '0' || *end > '9') {
				return line_parser_error(parser, va("initial version format is [int] - instead saw '%s'", token));
			}
			++end;
		}
		if(*end != ']' || *(end + 1)) {
			return line_parser_error(parser, va("initial version format is [int] - instead saw '%s'", token));
		}
	}
	version = (u32)atoi(token + 1);
	token = line_parser_next_token(parser);
	if(token) {
		return line_parser_error(parser, va("extra characters after version: '%s'", token));
	}
	return version;
}
