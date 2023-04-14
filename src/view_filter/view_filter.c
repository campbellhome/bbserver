// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "view_filter.h"
#include "bb_array.h"
#include "bb_assert.h"
#include "bb_json_generated.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "recorded_session.h"
#include "site_config.h"
#include "str.h"
#include "va.h"
#include "view.h"
#include "view_filter_legacy.h"

static span_t view_filter_tokenize_string(span_t* out, vfilter_t* filter, const char* line)
{
	span_t ret = { BB_EMPTY_INITIALIZER };
	if (!out)
		return ret;

	span_t remaining = *out;
	if (span_is_empty(remaining))
		return ret;

	char quoteChar = span_peek(remaining);
	b32 bQuoted = (quoteChar == '\'' || quoteChar == '\"');
	if (bQuoted)
	{
		remaining = span_pop_front(remaining);
	}
	ret.start = remaining.start;

	char next = span_peek(remaining);
	while (next != '\0')
	{
		// look for end of string
		if (bQuoted)
		{
			if (next == quoteChar)
			{
				ret.end = remaining.start;
				remaining = span_pop_front(remaining);
				break;
			}
		}
		else
		{
			if (next == ' ' || next == ')' || next == '>' || next == '<')
			{
				ret.end = remaining.start;
				break;
			}
			else if (next == '!' || next == '=')
			{
				char second = span_peek(span_pop_front(remaining));
				if (second == '=')
				{
					remaining = span_pop_front(remaining);
					ret.end = remaining.start;
					break;
				}
			}
		}

		if (next == '\\')
		{
			remaining = span_pop_front(remaining);
			char second = span_peek(remaining);
			switch (second)
			{
			case '\\':
			case '\'':
			case '\"':
			case '(':
			case ')':
				remaining = span_pop_front(remaining);
				break;
			default:
				// invalid!
				if (filter && filter->valid)
				{
					filter->valid = false;
					if (line)
					{
						filter->error.column = (s32)(remaining.start - line);
					}
					sb_reset(&filter->error.text);
					sb_va(&filter->error.text, "Invalid character '%c' after \\ in column %d", second, filter->error.column);
				}
				break;
			}
		}
		else if (next == '\0')
		{
			span_t empty = { BB_EMPTY_INITIALIZER };
			ret.end = remaining.start;
			remaining = empty;
			break;
		}
		else
		{
			remaining = span_pop_front(remaining);
		}
		next = span_peek(remaining);
	}

	if (!ret.end && !bQuoted && remaining.end >= ret.start)
	{
		ret.end = remaining.end;
	}

	*out = remaining;
	return ret;
}

static vfilter_token_t view_filter_tokenize(span_t* out, vfilter_t* filter, const char* line)
{
	vfilter_token_t ret = { BB_EMPTY_INITIALIZER };
	if (!out)
		return ret;

	span_t remaining = *out;
	if (span_is_empty(remaining))
		return ret;

	char next = span_peek(remaining);
	while (next == ' ' || next == '\t')
	{
		remaining = span_pop_front(remaining);
		next = span_peek(remaining);
	}
	char nextnext = span_peek(span_pop_front(remaining));
	if (next == '(')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_OpenParen;
		remaining = span_pop_front(remaining);
	}
	else if (next == ')')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_CloseParen;
		remaining = span_pop_front(remaining);
	}
	else if (next == '>' && nextnext == '=')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_GreaterThanEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	}
	else if (next == '<' && nextnext == '=')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_LessThanEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	}
	else if (next == '=' && nextnext == '=')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_Equals;
		remaining = span_pop_front(span_pop_front(remaining));
	}
	else if (next == '!' && nextnext == '=')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_NotEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	}
	else if (next == '>')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_GreaterThan;
		remaining = span_pop_front(remaining);
	}
	else if (next == '<')
	{
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_LessThan;
		remaining = span_pop_front(remaining);
	}
	else if (next == '\"')
	{
		ret.span = view_filter_tokenize_string(&remaining, filter, line);
		ret.type = kVFT_String;
	}
	else if (next == '\'')
	{
		ret.span = view_filter_tokenize_string(&remaining, filter, line);
		ret.type = kVFT_String;
	}
	else
	{
		ret.span = view_filter_tokenize_string(&remaining, filter, line);
		ret.type = kVFT_String;
	}

	if (!filter->valid)
	{
		vfilter_token_t empty = { BB_EMPTY_INITIALIZER };
		ret = empty;
		remaining = ret.span;
	}

	*out = remaining;
	return ret;
}

static u32 view_filter_validate_error(vfilter_t* filter, u32 index, const char* input, const char* message)
{
	filter->valid = false;
	if (filter->error.text.data == NULL)
	{
		if (index >= filter->tokens.count)
		{
			vfilter_token_t* token = filter->tokens.data + filter->tokens.count - 1;
			filter->error.column = (s32)(token->span.end - input + 1);
		}
		else
		{
			vfilter_token_t* token = filter->tokens.data + index;
			filter->error.column = (s32)(token->span.start - input);
		}
		BB_ASSERT(filter->error.column >= 0 && filter->error.column < 4096);
		sb_reset(&filter->error.text);
		sb_va(&filter->error.text, "%s in column %d", message, filter->error.column);
	}
	return filter->tokens.count;
}

typedef enum
{
	kVFVS_Empty,
	kVFVS_HasLeft,
	kVFVS_HasOperator,
	kVFVS_HasRight,
	kVFVS_HasConjunction,
} vfilter_validate_state_e;

u32 view_filter_validate_tokens(vfilter_t* filter, u32 index, int* parenDepth, const char* input)
{
	if (index >= filter->tokens.count)
	{
		return filter->tokens.count;
	}

	vfilter_validate_state_e state = kVFVS_Empty;

	b32 bLeftTokenIsNumeric = false;
	b32 bLeftTokenIsVerbosity = false;
	b32 bHasNot = false;

	while (index < filter->tokens.count)
	{
		bHasNot = false;
		vfilter_token_t* token = filter->tokens.data + index;
		BB_WARNING_PUSH(4061);
		switch (token->type)
		{
		case kVFT_OpenParen:
			if (state == kVFVS_Empty || state == kVFVS_HasConjunction)
			{
				++*parenDepth;
				index = view_filter_validate_tokens(filter, index + 1, parenDepth, input);
				state = kVFVS_HasRight;
			}
			else
			{
				return view_filter_validate_error(filter, index, input, "Unexpected (");
			}
			break;
		case kVFT_CloseParen:
			if (state == kVFVS_HasRight)
			{
				--*parenDepth;
				if (parenDepth >= 0)
				{
					return index + 1;
				}
				else
				{
					return view_filter_validate_error(filter, index, input, "Too many )'s");
				}
			}
			else
			{
				return view_filter_validate_error(filter, index, input, "Unexpected )");
			}
		case kVFT_LessThan:
		case kVFT_LessThanEquals:
		case kVFT_Equals:
		case kVFT_NotEquals:
		case kVFT_GreaterThan:
		case kVFT_GreaterThanEquals:
			if (state == kVFVS_HasLeft && (bLeftTokenIsNumeric || bLeftTokenIsVerbosity))
			{
				// can only follow kVFT_DeltaMillisecondsAbsolute, kVFT_DeltaMillisecondsViewRelative, kVFT_PIEInstance, or kVFT_Verbosity
				state = kVFVS_HasOperator;
				++index;
			}
			else
			{
				return view_filter_validate_error(filter, index, input, "Unexpected numeric comparator");
			}
			break;
		case kVFT_String:
			if (state == kVFVS_Empty || state == kVFVS_HasConjunction)
			{
				if (!span_stricmp(token->span, span_from_string("not")))
				{
					token->type = kVFT_Not;
				}
				else if (span_starts_with(token->span, "@", kSpanCaseInsentitive) && span_length(token->span) > 1)
				{
					token->type = kVFT_NamedFilter;
					state = kVFVS_HasRight;
				}
				else
				{
					state = kVFVS_HasLeft;
					bLeftTokenIsNumeric = false;
					bLeftTokenIsVerbosity = false;
					if (!span_stricmp(token->span, span_from_string("absms")))
					{
						token->type = kVFT_DeltaMillisecondsAbsolute;
						bLeftTokenIsNumeric = true;
					}
					else if (!span_stricmp(token->span, span_from_string("relms")))
					{
						token->type = kVFT_DeltaMillisecondsViewRelative;
						bLeftTokenIsNumeric = true;
					}
					else if (!span_stricmp(token->span, span_from_string("filename")))
					{
						token->type = kVFT_Filename;
					}
					else if (!span_stricmp(token->span, span_from_string("thread")))
					{
						token->type = kVFT_Thread;
					}
					else if (!span_stricmp(token->span, span_from_string("pieinstance")))
					{
						token->type = kVFT_PIEInstance;
					}
					else if (!span_stricmp(token->span, span_from_string("category")))
					{
						token->type = kVFT_Category;
					}
					else if (!span_stricmp(token->span, span_from_string("verbosity")))
					{
						token->type = kVFT_Verbosity;
						bLeftTokenIsVerbosity = true;
					}
					else if (!span_stricmp(token->span, span_from_string("text")))
					{
						token->type = kVFT_Text;
					}
					else
					{
						return view_filter_validate_error(filter, index, input, "Unknown left hand side");
					}
				}
			}
			else if (state == kVFVS_HasLeft)
			{
				state = kVFVS_HasOperator;
				if (!bHasNot && !span_stricmp(token->span, span_from_string("not")))
				{
					token->type = kVFT_Not;
					bHasNot = true;
					state = kVFVS_HasLeft;
				}
				else if (!span_stricmp(token->span, span_from_string("is")) || !span_stricmp(token->span, span_from_string("matches")))
				{
					token->type = kVFT_Matches;
				}
				else if (!span_stricmp(token->span, span_from_string("contains")))
				{
					token->type = kVFT_Contains;
				}
				else if (!span_stricmp(token->span, span_from_string("startswith")))
				{
					token->type = kVFT_StartsWith;
				}
				else if (!span_stricmp(token->span, span_from_string("endswith")))
				{
					token->type = kVFT_EndsWith;
				}
				else
				{
					return view_filter_validate_error(filter, index, input, "Unknown operator");
				}
			}
			else if (state == kVFVS_HasOperator)
			{
				state = kVFVS_HasRight;
				if (bLeftTokenIsNumeric)
				{
					token->type = kVFT_Number;
					const char* str = va("%.*s", span_length(token->span), token->span.start);
					token->number = strtou32(str);
				}
				else if (bLeftTokenIsVerbosity)
				{
					if (!span_stricmp(token->span, span_from_string("log")))
					{
						token->number = kBBLogLevel_Log;
					}
					else if (!span_stricmp(token->span, span_from_string("warning")))
					{
						token->number = kBBLogLevel_Warning;
					}
					else if (!span_stricmp(token->span, span_from_string("error")))
					{
						token->number = kBBLogLevel_Error;
					}
					else if (!span_stricmp(token->span, span_from_string("display")))
					{
						token->number = kBBLogLevel_Display;
					}
					else if (!span_stricmp(token->span, span_from_string("setcolor")))
					{
						token->number = kBBLogLevel_SetColor;
					}
					else if (!span_stricmp(token->span, span_from_string("veryverbose")))
					{
						token->number = kBBLogLevel_VeryVerbose;
					}
					else if (!span_stricmp(token->span, span_from_string("verbose")))
					{
						token->number = kBBLogLevel_Verbose;
					}
					else if (!span_stricmp(token->span, span_from_string("fatal")))
					{
						token->number = kBBLogLevel_Fatal;
					}
					else
					{
						return view_filter_validate_error(filter, index, input, "Unknown verbosity");
					}
				}
			}
			else if (state == kVFVS_HasRight)
			{
				if (!span_stricmp(token->span, span_from_string("and")))
				{
					token->type = kVFT_And;
					state = kVFVS_HasConjunction;
				}
				else if (!span_stricmp(token->span, span_from_string("or")))
				{
					token->type = kVFT_Or;
					state = kVFVS_HasConjunction;
				}
			}
			else
			{
				return view_filter_validate_error(filter, index, input, "Unexpected token");
			}
			++index;
			break;
		default:
			return view_filter_validate_error(filter, index, input, "Unexpected token");
		}
		BB_WARNING_POP;
	}

	if (state != kVFVS_HasRight)
	{
		return view_filter_validate_error(filter, index, input, "Incomplete filter");
	}
	return filter->tokens.count;
}

typedef enum vfilter_token_classification_e
{
	kVFC_Operator,
	kVFC_Operand,
	kVFC_OpenParen,
	kVFC_CloseParen,
	kVFC_Count
} vfilter_token_classification_e;

static vfilter_token_classification_e view_filter_token_classify(vfilter_token_t token)
{
	switch (token.type)
	{
	case kVFT_OpenParen:
		return kVFC_OpenParen;
	case kVFT_CloseParen:
		return kVFC_CloseParen;
	case kVFT_LessThan:
	case kVFT_LessThanEquals:
	case kVFT_Equals:
	case kVFT_NotEquals:
	case kVFT_GreaterThan:
	case kVFT_GreaterThanEquals:
	case kVFT_Matches:
	case kVFT_And:
	case kVFT_Or:
	case kVFT_Not:
	case kVFT_Contains:
	case kVFT_StartsWith:
	case kVFT_EndsWith:
		return kVFC_Operator;
	case kVFT_DeltaMillisecondsAbsolute:
	case kVFT_DeltaMillisecondsViewRelative:
	case kVFT_Filename:
	case kVFT_Thread:
	case kVFT_PIEInstance:
	case kVFT_Category:
	case kVFT_Verbosity:
	case kVFT_Text:
	case kVFT_Number:
	case kVFT_String:
	case kVFT_NamedFilter:
		return kVFC_Operand;
	case kVFT_Invalid:
	case kVFT_Count:
	default:
		return kVFC_Count;
	}
}

static u32 view_filter_token_priority(vfilter_token_t token)
{
	if (token.type == kVFT_And)
	{
		return 2;
	}
	if (token.type == kVFT_Or)
	{
		return 1;
	}
	if (token.type == kVFT_Not)
	{
		return 3;
	}
	switch (view_filter_token_classify(token))
	{
	case kVFC_Operator:
		return 4;
	case kVFC_OpenParen:
	case kVFC_CloseParen:
	case kVFC_Operand:
	case kVFC_Count:
	default:
		return 0;
	}
}

static void view_filter_convert_tokens_to_rpn(vfilter_t* filter)
{
	vfilter_tokens_t stack = { BB_EMPTY_INITIALIZER };
	vfilter_tokens_reset(&filter->rpn_tokens);

	for (u32 i = 0; i < filter->tokens.count; ++i)
	{
		vfilter_token_t* token = filter->tokens.data + i;
		vfilter_token_classification_e classification = view_filter_token_classify(*token);
		switch (classification)
		{
		case kVFC_OpenParen:
			bba_push(stack, *token);
			break;
		case kVFC_CloseParen:
			while (stack.count > 1 && stack.data[stack.count - 1].type != kVFT_OpenParen)
			{
				bba_push(filter->rpn_tokens, stack.data[stack.count - 1]);
				--stack.count;
			}
			--stack.count;
			break;
		case kVFC_Operator:
			while (stack.count > 0 && view_filter_token_priority(stack.data[stack.count - 1]) >= view_filter_token_priority(*token))
			{
				bba_push(filter->rpn_tokens, stack.data[stack.count - 1]);
				--stack.count;
			}
			bba_push(stack, *token);
			break;
		case kVFC_Operand:
			bba_push(filter->rpn_tokens, *token);
			break;
		case kVFC_Count:
		default:
			break;
		}
	}
	while (stack.count > 0)
	{
		bba_push(filter->rpn_tokens, stack.data[stack.count - 1]);
		--stack.count;
	}

	vfilter_tokens_reset(&stack);
}

static vfilter_t view_filter_parse_single(const char* name, const char* input)
{
	BB_LOG("filter", "parse: [%s] %s", name, input);

	vfilter_t filter = { BB_EMPTY_INITIALIZER };
	filter.name = sb_from_c_string(name);
	filter.input = sb_from_c_string(input);
	filter.tokenstream = sb_from_c_string(input);
	filter.valid = true;

	// parse the raw tokens
	b32 bAllString = true;
	span_t remaining = span_from_string(filter.tokenstream.data);
	while (!span_is_empty(remaining))
	{
		vfilter_token_t token = view_filter_tokenize(&remaining, &filter, filter.tokenstream.data);
		BB_LOG("filter", "token type:%d contents:%.*s", token.type, span_length(token.span), token.span.start);
		bba_push(filter.tokens, token);
		if (token.type != kVFT_String)
		{
			bAllString = false;
		}
	}

	if (bAllString)
	{
		if (filter.tokens.count >= 3)
		{
			if (!span_stricmp(filter.tokens.data[0].span, span_from_string("text")) ||
			    !span_stricmp(filter.tokens.data[0].span, span_from_string("category")))
			{
				if (!span_stricmp(filter.tokens.data[1].span, span_from_string("is")) ||
				    !span_stricmp(filter.tokens.data[1].span, span_from_string("matches")) ||
				    !span_stricmp(filter.tokens.data[1].span, span_from_string("contains")) ||
				    !span_stricmp(filter.tokens.data[1].span, span_from_string("startswith")) ||
				    !span_stricmp(filter.tokens.data[1].span, span_from_string("endswith")))
				{
					bAllString = false;
				}
			}
		}
	}
	if (bAllString)
	{
		if (filter.tokens.count == 1 || filter.tokens.count >= 3)
		{
			if (span_starts_with(filter.tokens.data[0].span, "@", kSpanCaseInsentitive))
			{
				if (span_length(filter.tokens.data[0].span) > 1)
				{
					bAllString = false;
				}
			}
		}
	}

	if (bAllString)
	{
		filter.type = kVF_Legacy;
		filter.valid = true;
		BB_LOG("filter", "Legacy: %s", filter.tokenstream.data);
	}
	else if (filter.tokens.count > 1 && (!span_stricmp(filter.tokens.data[0].span, span_from_string("WHERE"))))
	{
		filter.type = kVF_SQL;
		filter.valid = true;
		BB_LOG("filter", "SQL: %s", sb_get(&filter.input));
	}
	else
	{
		filter.type = kVF_Standard;
		filter.valid = true;

		// walk the stack and convert the string tokens to more specific category, like, etc tokens
		int parenDepth = 0;
		view_filter_validate_tokens(&filter, 0, &parenDepth, filter.tokenstream.data);

		view_filter_convert_tokens_to_rpn(&filter);

		if (parenDepth > 0)
		{
			view_filter_validate_error(&filter, filter.tokens.count - 1, filter.tokenstream.data, "missing )");
		}
		else if (parenDepth < 0)
		{
			view_filter_validate_error(&filter, filter.tokens.count - 1, filter.tokenstream.data, "extra )");
		}

		sb_t out = { BB_EMPTY_INITIALIZER };
		for (u32 i = 0; i < filter.tokens.count; ++i)
		{
			if (i)
			{
				sb_append(&out, " ");
			}
			vfilter_token_t* token = filter.tokens.data + i;
			if (token->type == kVFT_String)
			{
				sb_va(&out, "\"%.*s\"", span_length(token->span), token->span.start);
			}
			else if (token->type == kVFT_Number)
			{
				sb_va(&out, "%u", token->number);
			}
			else if (token->type == kVFT_Verbosity)
			{
				sb_va(&out, "%s", bb_get_log_level_name((bb_log_level_e)token->number, "invalidVerbosity"));
			}
			else
			{
				sb_va(&out, "%s", string_from_vfilter_token_type_e(token->type));
			}
		}
		BB_LOG("filter", "Standard: %s", sb_get(&out));
		sb_reset(&out);

		for (u32 i = 0; i < filter.rpn_tokens.count; ++i)
		{
			if (i)
			{
				sb_append(&out, " ");
			}
			vfilter_token_t* token = filter.rpn_tokens.data + i;
			if (token->type == kVFT_String)
			{
				sb_va(&out, "\"%.*s\"", span_length(token->span), token->span.start);
			}
			else if (token->type == kVFT_Number)
			{
				sb_va(&out, "%u", token->number);
			}
			else if (token->type == kVFT_Verbosity)
			{
				sb_va(&out, "%s", bb_get_log_level_name((bb_log_level_e)token->number, "invalidVerbosity"));
			}
			else
			{
				sb_va(&out, "%s", string_from_vfilter_token_type_e(token->type));
			}
		}
		BB_LOG("filter", "RPN: %s", sb_get(&out));
		sb_reset(&out);
	}

	for (u32 i = 0; i < filter.tokens.count; ++i)
	{
		vfilter_token_t* token = filter.tokens.data + i;
		if (token->span.end)
		{
			*(char*)(token->span.end) = '\0';
		}
	}

	if (!filter.valid)
	{
		const char* error = view_filter_get_error_string(&filter);
		if (error && *error)
		{
			BB_ERROR("filter", "%s", error);
		}
	}

	return filter;
}

static const char* view_filter_get_named_filter(span_t searchName)
{
#if defined(BB_STANDALONE)
	BB_UNUSED(searchName);
#else
	if (*searchName.start == '@')
	{
		searchName.start++; // ignore the initial '@'
	}
	for (u32 i = 0; i < g_config.namedFilters.count; ++i)
	{
		const char* filterName = sb_get(&g_config.namedFilters.data[i].name);
		if (!span_stricmp(searchName, span_from_string(filterName)))
		{
			const char* filterText = sb_get(&g_config.namedFilters.data[i].text);
			return filterText;
		}
	}
	for (u32 i = 0; i < g_site_config.namedFilters.count; ++i)
	{
		const char* filterName = sb_get(&g_site_config.namedFilters.data[i].name);
		if (!span_stricmp(searchName, span_from_string(filterName)))
		{
			const char* filterText = sb_get(&g_site_config.namedFilters.data[i].text);
			return filterText;
		}
	}
#endif
	return NULL;
}

static vfilter_t* named_filter_find(named_vfilters_t* namedFilters, const span_t searchName)
{
	for (u32 i = 0; i < namedFilters->count; ++i)
	{
		const char* filterName = sb_get(&namedFilters->data[i].name);
		if (!span_stricmp(searchName, span_from_string(filterName)))
		{
			return namedFilters->data + i;
		}
	}
	return NULL;
}

static const char* get_vfilter_token_text(vfilter_token_t* token)
{
	switch (token->type)
	{
	case kVFT_Invalid: return "Invalid"; break;
	case kVFT_OpenParen: return "("; break;
	case kVFT_CloseParen: return ")"; break;
	case kVFT_String: return va(" \"%.*s\"", span_length(token->span), token->span.start); break;
	case kVFT_LessThan: return "<"; break;
	case kVFT_LessThanEquals: return "<="; break;
	case kVFT_Equals: return "=="; break;
	case kVFT_NotEquals: return "!="; break;
	case kVFT_GreaterThan: return ">"; break;
	case kVFT_GreaterThanEquals: return ">="; break;
	case kVFT_Matches: return "Matches"; break;
	case kVFT_And: return "And"; break;
	case kVFT_Or: return "Or"; break;
	case kVFT_Not: return "Not"; break;
	case kVFT_Contains: return "Contains"; break;
	case kVFT_StartsWith: return "StartsWith"; break;
	case kVFT_EndsWith: return "EndsWith"; break;
	case kVFT_DeltaMillisecondsAbsolute: return va("%u", token->number); break;
	case kVFT_DeltaMillisecondsViewRelative: return va("%u", token->number); break;
	case kVFT_Filename: return "Filename"; break;
	case kVFT_Thread: return "Thread"; break;
	case kVFT_PIEInstance: return "PIEInstance"; break;
	case kVFT_Category: return "Category"; break;
	case kVFT_Verbosity: return "Verbosity"; break;
	case kVFT_Text: return "Text"; break;
	case kVFT_Number: return va("%u", token->number); break;
	case kVFT_NamedFilter: return va(" \"@%.*s\"", span_length(token->span), token->span.start); break;
	case kVFT_Count: return "Count"; break;
	}
	return "";
}

static sb_t view_filter_to_text(vfilter_t filter, named_vfilters_t* namedFilters)
{
	sb_t out = { BB_EMPTY_INITIALIZER };
	for (u32 i = 0; i < filter.tokens.count; ++i)
	{
		if (i)
		{
			sb_append(&out, " ");
		}
		vfilter_token_t* token = filter.tokens.data + i;
		if (filter.type == kVF_Standard)
		{
			if (token->type == kVFT_NamedFilter)
			{
				vfilter_t* namedFilter = named_filter_find(namedFilters, token->span);
				if (namedFilter)
				{
					sb_t namedText = view_filter_to_text(*namedFilter, namedFilters);
					sb_append(&out, sb_get(&namedText));
					sb_reset(&namedText);
				}
				else
				{
					sb_va(&out, "%s", get_vfilter_token_text(token));
				}
			}
			else
			{
				sb_va(&out, "%s", get_vfilter_token_text(token));
			}
		}
		else
		{
			sb_va(&out, " \"%.*s\"", span_length(token->span), token->span.start);
		}
	}
	return out;
}

static vfilter_t view_filter_parse_with_named_filters(const char* name, const char* input, named_vfilters_t* namedFilters)
{
	vfilter_t filter = view_filter_parse_single(name, input);

	if (filter.valid && filter.type == kVF_Standard)
	{
		// first, resolve all named filters recursively
		for (u32 i = 0; i < filter.tokens.count; ++i)
		{
			vfilter_token_t* token = filter.tokens.data + i;
			if (token->type == kVFT_NamedFilter)
			{
				vfilter_t* existing = named_filter_find(namedFilters, token->span);
				if (!existing)
				{
					// put a placeholder into namedFilters with the correct name to prevent stack overflow on recursive named filters
					vfilter_t placeholder = { BB_EMPTY_INITIALIZER };
					placeholder.name = sb_from_span(token->span);
					bba_push(*namedFilters, placeholder);

					const char* text = view_filter_get_named_filter(token->span);
					vfilter_t namedFilter = view_filter_parse_with_named_filters(sb_get(&placeholder.name), text ? text : "", namedFilters);

					vfilter_reset(&placeholder);
					bba_last(*namedFilters) = namedFilter;
				}
			}
		}

		// after named filters are resolved, build a composite filter string and parse that
		sb_t composite = view_filter_to_text(filter, namedFilters);
		vfilter_reset(&filter);
		filter = view_filter_parse_single(name, sb_get(&composite));
		sb_reset(&composite);
	}

	return filter;
}

vfilter_t view_filter_parse(const char* name, const char* input)
{
	named_vfilters_t namedFilters = { BB_EMPTY_INITIALIZER };
	vfilter_t filter = view_filter_parse_with_named_filters(name, input, &namedFilters);
	named_vfilters_reset(&namedFilters);

	return filter;
}

const char* view_filter_get_error_string(vfilter_t* filter)
{
	if (!filter || sb_len(&filter->error.text) == 0)
		return "";
	const char* format = filter->error.column > 0 ? va("%%s\n%%s\n%%%ds^", filter->error.column) : "%s\n%s\n^";
	return va(format, sb_get(&filter->error.text), filter->input.data, " ");
}

static u32 view_filter_get_millis(view_t* view, recorded_log_t* log, b32 relative)
{
	recorded_session_t* session = view->session;
	recorded_log_t* lastLog = (relative) ? (view->lastVisibleSessionLogIndex < session->logs.count ? session->logs.data[view->lastVisibleSessionLogIndex] : NULL) : (view->lastSessionLogIndex < session->logs.count ? session->logs.data[view->lastSessionLogIndex] : NULL);
	bb_decoded_packet_t* decoded = &log->packet;

	s64 prevElapsedTicks = (lastLog) ? (s64)lastLog->packet.header.timestamp - (s64)session->appInfo.header.timestamp : 0;
	s64 elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
	double deltaMillis = (elapsedTicks - prevElapsedTicks) * session->appInfo.packet.appInfo.millisPerTick;
	return (u32)deltaMillis;
}

static void view_filter_evaluate_number(view_t* view, recorded_log_t* log, u32 operatorIndex)
{
	if (operatorIndex < 2)
	{
		BB_ASSERT(false);
		return;
	}

	vfilter_token_t* left = view->vfilter.rpn_tokens.data + operatorIndex - 2;
	vfilter_token_t* right = view->vfilter.rpn_tokens.data + operatorIndex - 1;
	vfilter_token_t* comparison = view->vfilter.rpn_tokens.data + operatorIndex;

	u32 lhs = 0;
	BB_WARNING_PUSH(4062);
	switch (left->type)
	{
	case kVFT_DeltaMillisecondsAbsolute:
		lhs = view_filter_get_millis(view, log, false);
		break;
	case kVFT_DeltaMillisecondsViewRelative:
		lhs = view_filter_get_millis(view, log, true);
		break;
	case kVFT_PIEInstance:
		lhs = log->packet.packet.logText.pieInstance;
		break;
	case kVFT_Verbosity:
		lhs = 0; // TODO
		break;
	}
	BB_WARNING_POP;
	u32 rhs = right->number;

	vfilter_result_t result = { false };
	BB_WARNING_PUSH(4062);
	switch (comparison->type)
	{
	case kVFT_LessThan:
		result.value = lhs < rhs;
		break;
	case kVFT_LessThanEquals:
		result.value = lhs <= rhs;
		break;
	case kVFT_Equals:
		result.value = lhs == rhs;
		break;
	case kVFT_NotEquals:
		result.value = lhs != rhs;
		break;
	case kVFT_GreaterThan:
		result.value = lhs > rhs;
		break;
	case kVFT_GreaterThanEquals:
		result.value = lhs >= rhs;
		break;
	}
	BB_WARNING_POP;

	bba_push(view->vfilter.results, result);
}

static void view_filter_evaluate_string(view_t* view, recorded_log_t* log, u32 operatorIndex)
{
	vfilter_token_t* left = view->vfilter.rpn_tokens.data + operatorIndex - 2;
	vfilter_token_t* right = view->vfilter.rpn_tokens.data + operatorIndex - 1;
	vfilter_token_t* comparison = view->vfilter.rpn_tokens.data + operatorIndex;
	vfilter_result_t result = { false };

	const char* lhs = "";
	BB_WARNING_PUSH(4062);
	switch (left->type)
	{
	case kVFT_Filename:
		lhs = recorded_session_get_filename(view->session, log->packet.header.fileId);
		break;
	case kVFT_Thread:
		lhs = recorded_session_get_thread_name(view->session, log->packet.header.threadId);
		break;
	case kVFT_Category:
		lhs = recorded_session_get_category_name(view->session, log->packet.packet.logText.categoryId);
		break;
	case kVFT_Text:
		lhs = log->packet.packet.logText.text;
		break;
	}
	BB_WARNING_POP;

	const char* rhs = right->span.start;

	BB_WARNING_PUSH(4062);
	switch (comparison->type)
	{
	case kVFT_Matches:
		result.value = !bb_stricmp(lhs, rhs);
		break;
	case kVFT_Contains:
		result.value = bb_stristr(lhs, rhs) != NULL;
		break;
	case kVFT_StartsWith:
		result.value = !bb_strnicmp(lhs, rhs, strlen(rhs));
		break;
	case kVFT_EndsWith:
	{
		size_t lhs_len = strlen(lhs);
		size_t rhs_len = strlen(rhs);
		while (lhs_len > 0 && lhs[lhs_len - 1] == '\n')
		{
			--lhs_len;
		}
		if (lhs_len >= rhs_len)
		{
			size_t offset = lhs_len - rhs_len;
			result.value = !bb_strnicmp(lhs + offset, rhs, rhs_len);
		}
		else
		{
			result.value = false;
		}
		break;
	}
	}
	BB_WARNING_POP;

	bba_push(view->vfilter.results, result);
}

static void view_filter_evaluate_and_or(view_t* view, u32 operatorIndex)
{
	if (view->vfilter.results.count < 2)
	{
		BB_ASSERT(false);
		return;
	}

	b32 lhs = view->vfilter.results.data[view->vfilter.results.count - 2].value;
	b32 rhs = view->vfilter.results.data[view->vfilter.results.count - 1].value;
	view->vfilter.results.count -= 2;

	vfilter_result_t result = { false };
	vfilter_token_t* comparison = view->vfilter.rpn_tokens.data + operatorIndex;
	if (comparison->type == kVFT_And)
	{
		result.value = lhs && rhs;
	}
	else
	{
		result.value = lhs || rhs;
	}

	bba_push(view->vfilter.results, result);
}

static void view_filter_evaluate_not(view_t* view)
{
	if (view->vfilter.results.count < 1)
	{
		BB_ASSERT(false);
		return;
	}

	view->vfilter.results.data[view->vfilter.results.count - 1].value = !view->vfilter.results.data[view->vfilter.results.count - 1].value;
}

static void view_filter_evaluate_named_filter(view_t* view, recorded_log_t* log, u32 operatorIndex)
{
	BB_UNUSED(log);
	BB_UNUSED(operatorIndex);
	vfilter_result_t result = { false };
	bba_push(view->vfilter.results, result);
}

static b32 view_filter_visible_standard(view_t* view, recorded_log_t* log)
{
	view->vfilter.results.count = 0;

	for (u32 i = 0; i < view->vfilter.rpn_tokens.count; ++i)
	{
		vfilter_token_t* token = view->vfilter.rpn_tokens.data + i;
		switch (token->type)
		{
		case kVFT_LessThan:
		case kVFT_LessThanEquals:
		case kVFT_Equals:
		case kVFT_NotEquals:
		case kVFT_GreaterThan:
		case kVFT_GreaterThanEquals:
			view_filter_evaluate_number(view, log, i);
			break;

		case kVFT_Matches:
		case kVFT_Contains:
		case kVFT_StartsWith:
		case kVFT_EndsWith:
			view_filter_evaluate_string(view, log, i);
			break;

		case kVFT_And:
		case kVFT_Or:
			view_filter_evaluate_and_or(view, i);
			break;

		case kVFT_Not:
			view_filter_evaluate_not(view);
			break;

		case kVFT_NamedFilter:
			view_filter_evaluate_named_filter(view, log, i);
			break;

		case kVFT_String:
		case kVFT_DeltaMillisecondsAbsolute:
		case kVFT_DeltaMillisecondsViewRelative:
		case kVFT_Filename:
		case kVFT_Thread:
		case kVFT_PIEInstance:
		case kVFT_Category:
		case kVFT_Verbosity:
		case kVFT_Text:
		case kVFT_Number:
		case kVFT_Invalid:
		case kVFT_OpenParen:
		case kVFT_CloseParen:
		case kVFT_Count:
		default:
			break;
		}
	}

	if (view->vfilter.results.count == 1)
	{
		return view->vfilter.results.data[0].value;
	}
	else
	{
		BB_ASSERT(false);
		return true;
	}
}

b32 view_filter_visible(view_t* view, recorded_log_t* log)
{
	if (!view->config.filterActive || !view->vfilter.valid || !view->vfilter.tokens.count)
		return true;
	switch (view->vfilter.type)
	{
	case kVF_Standard:
		return view_filter_visible_standard(view, log);
	case kVF_SQL:
		return true;
	case kVF_Legacy:
		return view_filter_visible_legacy(view, log);
	case kVF_Count:
	default:
		return true;
	}
}
