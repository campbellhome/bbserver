// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"
#include "span.h"

#if defined(__cplusplus)
extern "C" {
#endif

span_t tokenize(const char** bufferCursor, const char* delimiters);
span_t tokenizeLine(span_t* cursor);
span_t tokenizeNthLine(span_t span, u32 lineIndex);

#if defined(__cplusplus)
}
#endif
