// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_defines.h"
#include "bbclient/bb_malloc.h"
#include "bbclient/bb_string.h"

#include <stdlib.h> // for malloc
#if defined(WIN32)
#include <stdint.h>
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

BB_WARNING_DISABLE(4711); // warning C4711: function 'bb_tolower' selected for automatic inline expansion

size_t bb_strncpy(char* dest, const char* src, size_t destSize)
{
	//size_t len = strlen(src);
	const char* s = src;
	char* c;
	size_t len = 0;
	size_t n;
	while (*s++)
	{
		++len;
	}

	len = (len < destSize) ? len : destSize - 1;

	//memcpy(dest, src, len);
	n = len;
	c = dest;
	s = src;
	while (n--)
	{
		*c++ = *s++;
	}

	dest[len] = '\0';
	return len;
}

#if BB_COMPILE_WIDECHAR
size_t bb_wstrncpy(bb_wchar_t* dest, const bb_wchar_t* src, size_t destSize)
{
	//size_t len = strlen(src);
	const bb_wchar_t* s = src;
	bb_wchar_t* c;
	size_t len = 0;
	size_t n;
	while (*s++)
	{
		++len;
	}

	len = (len < destSize) ? len : destSize - 1;

	//memcpy(dest, src, len);
	n = len;
	c = dest;
	s = src;
	while (n--)
	{
		*c++ = *s++;
	}

	dest[len] = '\0';
	return len;
}
#endif // #if BB_COMPILE_WIDECHAR

char bb_tolower(char c)
{
	return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

char* bb_stristr(const char* src, const char* pattern)
{
	if (!src)
		return NULL;

	while (*src)
	{
		const char* left = src;
		const char* right = pattern;

		while (*left && bb_tolower(*left) == bb_tolower(*right))
		{
			++left;
			++right;
		}

		if (!*right)
		{
			return (char*)src; // match!
		}

		++src;
	}

	return NULL;
}

int bb_strnicmp(const char* s1, const char* s2, size_t len)
{
	if (!s1 || !s2)
	{
		if (!s1 && !s2)
			return 0;
		return (s1) ? 1 : -1;
	}

	while (len && *s1 && (bb_tolower(*s1) == bb_tolower(*s2)))
	{
		++s1;
		++s2;
		--len;
	}
	if (len == 0)
	{
		return 0;
	}
	else
	{
		return (bb_tolower(*s1) - bb_tolower(*s2));
	}
}

int bb_stricmp(const char* s1, const char* s2)
{
	if (!s1 || !s2)
	{
		if (!s1 && !s2)
			return 0;
		return (s1) ? 1 : -1;
	}

	while (*s1 && (bb_tolower(*s1) == bb_tolower(*s2)))
	{
		++s1;
		++s2;
	}
	if (!*s1 && !*s2)
	{
		return 0;
	}
	else
	{
		return (bb_tolower(*s1) - bb_tolower(*s2));
	}
}

char* bb_strdup_loc(const char* file, int line, const char* s)
{
	size_t len = s ? strlen(s) : 0u;
	size_t size = len + 1;
	char* out = (size) ? (char*)bb_malloc_loc(file, line, size) : 0;
	if (out)
	{
		if (size)
		{
			memcpy(out, s, len);
		}
		out[len] = '\0';
	}

	return out;
}

#endif // #if BB_ENABLED
