// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "tokenize.h"
#include "common.h"

#include <string.h>

span_t tokenize(const char** bufferCursor, const char* delimiters)
{
	span_t ret = { BB_EMPTY_INITIALIZER };

	if (!delimiters)
	{
		delimiters = " \t\r\n";
	}

	const char* buffer = *bufferCursor;

	if (!buffer || !*buffer)
	{
		return ret;
	}

	// skip whitespace
	while (*buffer && strchr(delimiters, *buffer))
	{
		++buffer;
	}

	if (!*buffer)
	{
		return ret;
	}

	// look for '\"'
	b32 isQuotedString = (*buffer == '\"');
	if (isQuotedString)
	{
		++buffer;
	}

	// step to end of string
	const char* start = buffer;
	b32 bBackslash = false;
	if (isQuotedString)
	{
		b32 bMultilineDelimiter = strchr(delimiters, '\n') != NULL;
		while (*buffer)
		{
			if (bMultilineDelimiter && *buffer == '\n')
				break;

			if (bBackslash)
			{
				bBackslash = false;
			}
			else
			{
				if (*buffer == '\\')
				{
					bBackslash = true;
				}
				else if (*buffer == '\"')
				{
					break;
				}
			}
			++buffer;
		}
	}
	else
	{
		while (*buffer && !strchr(delimiters, *buffer))
		{
			++buffer;
		}
	}

	// remove trailing '\"'
	const char* end = buffer;
	if (isQuotedString && *end == '\"')
	{
		buffer++;
	}

	ret.start = start;
	ret.end = end;

	*bufferCursor = buffer;

	return ret;
}

span_t tokenizeLine(span_t* cursor)
{
	span_t ret = { BB_EMPTY_INITIALIZER };
	if (cursor && cursor->start)
	{
		ret.start = cursor->start;
		ret.end = ret.start;
		while (ret.end <= cursor->end)
		{
			if (*ret.end == '\n')
			{
				break;
			}
			else if (*ret.end == '\r' && !(ret.end < cursor->end && ret.end[1] == '\n'))
			{
				break;
			}
			else
			{
				++ret.end;
			}
		}

		if (ret.end < cursor->end)
		{
			cursor->start = ret.end + 1;
		}
		else
		{
			cursor->start = NULL;
		}
		if (!cursor->start && ret.end - ret.start == 1)
		{
			ret.start = ret.end = NULL;
		}
		if (ret.end > ret.start && ret.end[-1] == '\r')
		{
			--ret.end;
		}
	}
	return ret;
}

span_t tokenizeNthLine(span_t span, u32 lineIndex)
{
	span_t subLineSpan = { BB_EMPTY_INITIALIZER };
	u32 subLineIndex = 0;
	for (span_t line = tokenizeLine(&span); line.start; line = tokenizeLine(&span))
	{
		if (subLineIndex == lineIndex)
		{
			subLineSpan = line;
			break;
		}
		++subLineIndex;
	}
	return subLineSpan;
}

#if defined(MC_COMMON_TESTS) && MC_COMMON_TESTS

#include "bb_string.h"
#include "output.h"
#include "va.h"

typedef struct tokenize_test_data_t {
	const char* name;
	const char* source;
	const char** outputLines;
	int expectedOutputLines;
	u32 optionalSourceLen;
} tokenize_test_data_t;

static b32 report_test(const tokenize_test_data_t* testData, b32 bSuccess)
{
	if (bSuccess)
	{
		output_log("[%s] pass", testData->name);
		return true;
	}
	else
	{
		output_error("[%s] FAIL", testData->name);
		return false;
	}
}

static b32 test_tokenize_lines_from_span(const tokenize_test_data_t* testData, span_t cursor)
{
	b32 pass = true;
	int numLines = 0;
	span_t token = tokenizeLine(&cursor);
	while (token.start)
	{
		sb_t saw_sb = sb_from_span(token);
		char* saw = va("%.*s", token.end - token.start, token.start);
		if (bb_stricmp(sb_get(&saw_sb), saw) != 0)
		{
			output_warning("[%s] lines[%d] mismatch: saw [%s], sb_from_span [%s]", testData->name, numLines, saw, sb_get(&saw_sb));
			pass = false;
		}
		else if (numLines < testData->expectedOutputLines)
		{
			const char* expected = testData->outputLines[numLines];
			if (bb_stricmp(saw, expected) != 0)
			{
				output_warning("[%s] lines[%d] mismatch: saw [%s], expected [%s]", testData->name, numLines, saw, expected);
				pass = false;
			}
		}
		else
		{
			pass = false;
		}
		sb_reset(&saw_sb);

		++numLines;
		token = tokenizeLine(&cursor);
	}

	if (numLines != testData->expectedOutputLines)
	{
		output_warning("[%s] mismatched number of lines: saw %d, expected %d", testData->name, numLines, testData->expectedOutputLines);
		pass = false;
	}
	return report_test(testData, pass);
}

static b32 test_tokenize_lines(const tokenize_test_data_t* testData)
{
	span_t cursor = span_from_string(testData->source);
	if (testData->optionalSourceLen > 0)
	{
		cursor.end = testData->source + testData->optionalSourceLen;
	}
	return test_tokenize_lines_from_span(testData, cursor);
}

static tokenize_test_data_t g_emptyTestData = {
	"tokenize empty string",
	"",
	NULL,
	0
};

static const char* g_singleLineTestResults[] = {
	"This is a single line"
};
static tokenize_test_data_t g_singleLineTestData = {
	"tokenize single line",
	"This is a single line",
	g_singleLineTestResults,
	1
};

static tokenize_test_data_t g_singleLineCRTestData = {
	"tokenize single line ending in CR",
	"This is a single line\r",
	g_singleLineTestResults,
	1
};

static tokenize_test_data_t g_singleLineLFTestData = {
	"tokenize single line ending in LF",
	"This is a single line\r",
	g_singleLineTestResults,
	1
};

static tokenize_test_data_t g_singleLineCRLFTestData = {
	"tokenize single line ending in CRLF",
	"This is a single line\r\n",
	g_singleLineTestResults,
	1
};

static const char* g_doubleLineTestResults[] = {
	"This is the first line",
	"This is the second."
};
static tokenize_test_data_t g_doubleLineTestData = {
	"tokenize two lines",
	"This is the first line\nThis is the second.",
	g_doubleLineTestResults,
	2
};

static tokenize_test_data_t g_doubleLineLFTestData = {
	"tokenize two lines ending in LF",
	"This is the first line\nThis is the second.\n",
	g_doubleLineTestResults,
	2
};

static tokenize_test_data_t g_doubleLineNullTestData = {
	"tokenize two lines ending in Null (\\0)",
	"This is the first line\nThis is the second.\0",
	g_doubleLineTestResults,
	2,
	44
};

b32 test_tokenize(void)
{
	if (!test_tokenize_lines(&g_emptyTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_singleLineTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_singleLineCRTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_singleLineLFTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_singleLineCRLFTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_doubleLineTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_doubleLineLFTestData))
	{
		return false;
	}
	if (!test_tokenize_lines(&g_doubleLineNullTestData))
	{
		return false;
	}
	return true;
}

#endif // #if defined(MC_COMMON_TESTS) && MC_COMMON_TESTS
