// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bboxtolog_utils.h"

#include "bb_string.h"
#include "span.h"

void separateFilename(const char* filename, sb_t* base, sb_t* ext)
{
	const char* separator = strrchr(filename, '.');
	if (separator)
	{
		span_t baseSpan = { filename, separator };
		*base = sb_from_span(baseSpan);
		*ext = sb_from_c_string(separator + 1);
	}
	else
	{
		*base = sb_from_c_string(filename);
	}
}

b32 wildcardMatch(const char* pattern, const char* input)
{
	// skip UE backups for now
	if (bb_stristr(input, "-backup-") != NULL)
		return false;

	// only support * or full match for now
	if (!*pattern || bb_stricmp(pattern, "*") == 0)
		return true;

	return bb_stristr(input, pattern) != NULL;
}
