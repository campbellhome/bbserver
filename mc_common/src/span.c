// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "span.h"
#include "bb_string.h"
#include <string.h>

span_t span_from_string(const char* str)
{
	span_t ret;
	ret.start = ret.end = str;
	if (str)
	{
		while (*ret.end)
		{
			++ret.end;
		}
	}
	return ret;
}

int span_strcmp(span_t a, span_t b)
{
	size_t alen = a.end - a.start;
	size_t blen = b.end - b.start;
	size_t len = (alen < blen) ? alen : blen;
	int cmp = strncmp(a.start, b.start, len);
	if (cmp)
	{
		return cmp;
	}
	if (alen == blen)
	{
		return 0;
	}
	else
	{
		return blen > alen;
	}
}

int span_stricmp(span_t a, span_t b)
{
	size_t alen = a.end - a.start;
	size_t blen = b.end - b.start;
	size_t len = (alen < blen) ? alen : blen;
	int cmp = bb_strnicmp(a.start, b.start, len);
	if (cmp)
	{
		return cmp;
	}
	if (alen == blen)
	{
		return 0;
	}
	else
	{
		return blen > alen;
	}
}

int span_starts_with(span_t span, const char* str, span_case_e spanCase)
{
	size_t spanLen = span.end - span.start;
	size_t strLen = strlen(str);
	return (spanLen >= strLen && (spanCase == kSpanCaseSensitive ? !strncmp(span.start, str, strLen) : !bb_strnicmp(span.start, str, strLen)));
}

int span_ends_with(span_t span, const char* str, span_case_e spanCase)
{
	size_t spanLen = span.end - span.start;
	size_t strLen = strlen(str);
	const char* spanStart = span.start + (spanLen - strLen);
	return (spanLen >= strLen && (spanCase == kSpanCaseSensitive ? !strncmp(spanStart, str, strLen) : !bb_strnicmp(spanStart, str, strLen)));
}

int span_is_empty(span_t span)
{
	return span.start >= span.end || span.start == NULL;
}

char span_peek(span_t span)
{
	if (span.start == NULL)
	{
		return '\0';
	}
	else
	{
		return *span.start;
	}
}

span_t span_pop_front(span_t span)
{
	span_t out = { span.start + 1, span.end };
	if ((out.start >= out.end || out.start == NULL) && out.start != out.end)
	{
		out.start = out.end = NULL;
	}
	return out;
}

u64 span_length(span_t span)
{
	if (span.start >= span.end || span.start == NULL)
	{
		return 0;
	}
	return span.end - span.start;
}
