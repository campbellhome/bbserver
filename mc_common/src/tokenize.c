// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "tokenize.h"
#include "common.h"

#include <string.h>

span_t tokenize(const char **bufferCursor, const char *delimiters)
{
	span_t ret = { BB_EMPTY_INITIALIZER };

	if(!delimiters) {
		delimiters = " \t\r\n";
	}

	const char *buffer = *bufferCursor;

	if(!buffer || !*buffer) {
		return ret;
	}

	// skip whitespace
	while(*buffer && strchr(delimiters, *buffer)) {
		++buffer;
	}

	if(!*buffer) {
		return ret;
	}

	// look for '\"'
	b32 isQuotedString = (*buffer == '\"');
	if(isQuotedString) {
		++buffer;
	}

	// step to end of string
	const char *start = buffer;
	b32 bBackslash = false;
	if(isQuotedString) {
		b32 bMultilineDelimiter = strchr(delimiters, '\n') != NULL;
		while(*buffer) {
			if(bMultilineDelimiter && *buffer == '\n')
				break;

			if(bBackslash) {
				bBackslash = false;
			} else {
				if(*buffer == '\\') {
					bBackslash = true;
				} else if(*buffer == '\"') {
					break;
				}
			}
			++buffer;
		}
	} else {
		while(*buffer && !strchr(delimiters, *buffer)) {
			++buffer;
		}
	}

	// remove trailing '\"'
	const char *end = buffer;
	if(isQuotedString && *end == '\"') {
		buffer++;
	}

	ret.start = start;
	ret.end = end;

	*bufferCursor = buffer;

	return ret;
}

span_t tokenizeLine(span_t *cursor)
{
	span_t ret = { BB_EMPTY_INITIALIZER };
	if(cursor && cursor->start) {
		ret.start = cursor->start;
		ret.end = ret.start;
		while(ret.end <= cursor->end) {
			if(*ret.end == '\n') {
				break;
			} else if(*ret.end == '\r' && !(ret.end < cursor->end && ret.end[1] == '\n')) {
				break;
			} else {
				++ret.end;
			}
		}

		if(ret.end < cursor->end) {
			cursor->start = ret.end + 1;
		} else {
			cursor->start = NULL;
		}
		if(!cursor->start && ret.end - ret.start == 1) {
			ret.start = ret.end = NULL;
		}
		if(ret.end > ret.start && ret.end[-1] == '\r') {
			--ret.end;
		}
	}
	return ret;
}

span_t tokenizeNthLine(span_t span, u32 lineIndex)
{
	span_t subLineSpan = { BB_EMPTY_INITIALIZER };
	u32 subLineIndex = 0;
	for(span_t line = tokenizeLine(&span); line.start; line = tokenizeLine(&span)) {
		if(subLineIndex == lineIndex) {
			subLineSpan = line;
			break;
		}
		++subLineIndex;
	}
	return subLineSpan;
}
