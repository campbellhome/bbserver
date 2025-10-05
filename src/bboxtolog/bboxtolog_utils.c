// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bboxtolog_utils.h"

#include "bb_array.h"
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

id_name_t id_names_find_non_null(id_names_t* names, u32 id)
{
	id_name_t* id_name = id_names_find(names, id);
	if (id_name)
	{
		return *id_name;
	}

	id_name_t empty = { BB_EMPTY_INITIALIZER };
	return empty;
}

id_name_t* id_names_find(id_names_t* names, u32 id)
{
	if (!names)
	{
		return NULL;
	}

	for (u32 i = 0; i < names->count; ++i)
	{
		id_name_t* id_name = names->data + i;
		if (id_name->id == id)
		{
			return id_name;
		}
	}

	return NULL;
}

id_name_t* id_names_find_or_add(id_names_t* names, u32 id, const char* name)
{
	if (!names)
	{
		return NULL;
	}

	id_name_t* id_name = id_names_find(names, id);
	if (id_name)
	{
		return id_name;
	}

	id_name = bba_add(*names, 1);
	if (id_name)
	{
		id_name->id = id;
		id_name->name = sb_from_c_string(name);
	}

	return id_name;
}
