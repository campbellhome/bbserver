// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "filter.h"
#include "bb_array.h"
#include "bb_string.h"
#include "sdict.h"
#include "tokenize.h"

void reset_filter_tokens(filterTokens* tokens)
{
	for (u32 i = 0; i < tokens->count; ++i)
	{
		filterToken* t = tokens->data + i;
		sb_reset(&t->category);
		sb_reset(&t->text);
	}
	bba_free(*tokens);
}

filterToken* add_filter_token(filterTokens* tokens, const char* category, const char* text)
{
	if (!text || !*text)
	{
		return NULL;
	}
	else if (!bba_add(*tokens, 1))
	{
		return NULL;
	}
	else
	{
		filterToken* t = &bba_last(*tokens);
		if (category)
		{
			sb_append(&t->category, category);
		}
		if (*text == '+')
		{
			t->required = true;
			++text;
		}
		if (*text == '-')
		{
			t->prohibited = true;
			++text;
		}
		sb_append(&t->text, text);
		return t;
	}
}

void build_filter_tokens(filterTokens* tokens, const char* src)
{
	reset_filter_tokens(tokens);
	const char* cursor = src;
	span_t token = tokenize(&cursor, " ");
	while (token.start)
	{
		filterToken t = { BB_EMPTY_INITIALIZER };
		if (*token.start == '-')
		{
			t.prohibited = true;
			++token.start;
		}
		if (*token.start == '+')
		{
			t.required = true;
			++token.start;
		}
		const char* categoryEnd = token.start;
		while (categoryEnd < token.end && *categoryEnd != ':')
		{
			++categoryEnd;
		}
		if (categoryEnd < token.end)
		{
			sb_va(&t.category, "%.*s", categoryEnd - token.start, token.start);
			sb_va(&t.text, "%.*s", token.end - categoryEnd - 1, categoryEnd + 1);
		}
		else
		{
			sb_va(&t.text, "%.*s", token.end - token.start, token.start);
		}
		if (bba_add_noclear(*tokens, 1))
		{
			bba_last(*tokens) = t;
		}
		else
		{
			sb_reset(&t.category);
			sb_reset(&t.text);
		}
		token = tokenize(&cursor, " ");
	}
}

b32 passes_filter_tokens(filterTokens* tokens, sdict_t* sd, const char** keys, u32 numKeys)
{
	b32 ok = false;
	u32 numRequired = 0;
	u32 numProhibited = 0;
	u32 numAllowed = 0;
	for (u32 i = 0; i < tokens->count; ++i)
	{
		filterToken* token = tokens->data + i;
		numRequired += token->required;
		numProhibited += token->prohibited;
		numAllowed += (!token->required && !token->prohibited);
		b32 found = false;
		const char* key = sb_get(&token->category);
		if (*key)
		{
			const char* value = sdict_find_safe(sd, key);
			if (token->exact)
			{
				found = bb_stricmp(value, sb_get(&token->text)) == 0;
			}
			else
			{
				found = bb_stristr(value, sb_get(&token->text)) != NULL;
			}
		}
		else if (keys)
		{
			for (u32 keyIdx = 0; keyIdx < numKeys; ++keyIdx)
			{
				key = keys[keyIdx];
				const char* value = sdict_find_safe(sd, key);
				if (token->exact)
				{
					found = bb_stricmp(value, sb_get(&token->text)) == 0;
				}
				else
				{
					found = bb_stristr(value, sb_get(&token->text)) != NULL;
				}
				if (found)
				{
					break;
				}
			}
		}
		else
		{
			for (u32 sdIdx = 0; sdIdx < sd->count; ++sdIdx)
			{
				sdictEntry_t* e = sd->data + sdIdx;
				found = bb_stristr(sb_get(&e->value), sb_get(&token->text)) != NULL;
				if (found)
				{
					break;
				}
			}
		}
		if ((found && token->prohibited) || (!found && token->required))
		{
			return false;
			break;
		}
		else if (found)
		{
			ok = true;
		}
	}
	if (!ok && numAllowed)
	{
		return false;
	}
	return true;
}
