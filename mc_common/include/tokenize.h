// Copyright (c) Matt Campbell
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

#if defined(MC_COMMON_TESTS) && MC_COMMON_TESTS
b32 test_tokenize(void);
#endif

#if defined(__cplusplus)
}
#endif
