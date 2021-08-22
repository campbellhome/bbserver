// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#include "view_filter.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "str.h"
#include "va.h"

static span_t view_filter_tokenize_string(span_t *out, vfilter_error_t *error, const char *line)
{
	span_t ret = { BB_EMPTY_INITIALIZER };
	if(!out)
		return ret;

	span_t remaining = *out;
	if(span_is_empty(remaining))
		return ret;

	char quoteChar = span_peek(remaining);
	b32 bQuoted = (quoteChar == '\'' || quoteChar == '\"');
	if(bQuoted) {
		remaining = span_pop_front(remaining);
	}
	ret.start = remaining.start;

	char next = span_peek(remaining);
	while(next != '\0') {
		// look for end of string
		if(bQuoted) {
			if(next == quoteChar) {
				ret.end = remaining.start;
				remaining = span_pop_front(remaining);
				break;
			}
		} else {
			if(next == ' ' || next == ')' || next == '>' || next == '<') {
				ret.end = remaining.start;
				break;
			} else if(next == '!' || next == '=') {
				char second = span_peek(span_pop_front(remaining));
				if(second == '=') {
					remaining = span_pop_front(remaining);
					ret.end = remaining.start;
					break;
				}
			}
		}

		if(next == '\\') {
			remaining = span_pop_front(remaining);
			char second = span_peek(remaining);
			switch(second) {
			case '\\':
			case '\'':
			case '\"':
			case '(':
			case ')':
				remaining = span_pop_front(remaining);
				break;
			default:
				// invalid!
				if(error) {
					if(line) {
						error->column = (s32)(remaining.start - line);
					}
					sb_reset(&error->text);
					sb_va(&error->text, "Invalid character '%c' after \\ in column %d", second, error->column);
				}
				break;
			}
		} else if(next == '\0') {
			span_t empty = { BB_EMPTY_INITIALIZER };
			ret.end = remaining.start;
			remaining = empty;
			break;
		} else {
			remaining = span_pop_front(remaining);
		}
		next = span_peek(remaining);
	}

	if(!ret.end && !bQuoted && remaining.end >= ret.start) {
		ret.end = remaining.end;
	}

	*out = remaining;
	return ret;
}

static vfilter_token_t view_filter_tokenize(span_t *out, vfilter_error_t *error, const char *line)
{
	vfilter_token_t ret = { BB_EMPTY_INITIALIZER };
	if(!out)
		return ret;

	span_t remaining = *out;
	if(span_is_empty(remaining))
		return ret;

	char next = span_peek(remaining);
	while(next == ' ' || next == '\t') {
		remaining = span_pop_front(remaining);
		next = span_peek(remaining);
	}
	char nextnext = span_peek(span_pop_front(remaining));
	if(next == '(') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_OpenParen;
		remaining = span_pop_front(remaining);
	} else if(next == ')') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_CloseParen;
		remaining = span_pop_front(remaining);
	} else if(next == '>' && nextnext == '=') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_GreaterThanEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	} else if(next == '<' && nextnext == '=') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_LessThanEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	} else if(next == '=' && nextnext == '=') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_Equals;
		remaining = span_pop_front(span_pop_front(remaining));
	} else if(next == '!' && nextnext == '=') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 2;
		ret.type = kVFT_NotEquals;
		remaining = span_pop_front(span_pop_front(remaining));
	} else if(next == '>') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_GreaterThan;
		remaining = span_pop_front(remaining);
	} else if(next == '<') {
		ret.span.start = remaining.start;
		ret.span.end = remaining.start + 1;
		ret.type = kVFT_LessThan;
		remaining = span_pop_front(remaining);
	} else if(next == '\"') {
		ret.span = view_filter_tokenize_string(&remaining, error, line);
		ret.type = kVFT_String;
	} else if(next == '\'') {
		ret.span = view_filter_tokenize_string(&remaining, error, line);
		ret.type = kVFT_String;
	} else {
		ret.span = view_filter_tokenize_string(&remaining, error, line);
		ret.type = kVFT_String;
	}

	if(sb_len(&error->text) > 0) {
		vfilter_token_t empty = { BB_EMPTY_INITIALIZER };
		ret = empty;
		remaining = ret.span;
	}

	*out = remaining;
	return ret;
}

static u32 view_filter_validate_error(vfilter_t *filter, u32 index, const char *input, const char *message)
{
	if(filter->error.text.data == NULL) {
		vfilter_token_t *token = filter->tokens.data + index;
		filter->error.column = (s32)(token->span.start - input);
		sb_reset(&filter->error.text);
		sb_va(&filter->error.text, "%s in column %d", message, filter->error.column);
	}
	return filter->tokens.count;
}

typedef enum {
	kVFVS_Empty,
	kVFVS_HasLeft,
	kVFVS_HasOperator,
	kVFVS_HasRight,
	kVFVS_HasConjunction,
} vfilter_validate_state_e;

u32 view_filter_validate_tokens(vfilter_t *filter, u32 index, int *parenDepth, const char *input)
{
	if(index >= filter->tokens.count) {
		return filter->tokens.count;
	}

	vfilter_validate_state_e state = kVFVS_Empty;

	b32 bLeftTokenIsNumeric = false;
	b32 bLeftTokenIsVerbosity = false;
	b32 bHasNot = false;

	while(index < filter->tokens.count) {
		bHasNot = false;
		vfilter_token_t *token = filter->tokens.data + index;
		BB_WARNING_PUSH(4061);
		switch(token->type) {
		case kVFT_OpenParen:
			if(state == kVFVS_Empty || state == kVFVS_HasConjunction) {
				++*parenDepth;
				index = view_filter_validate_tokens(filter, index + 1, parenDepth, input);
				state = kVFVS_HasRight;
			} else {
				return view_filter_validate_error(filter, index, input, "Unexpected (");
			}
			break;
		case kVFT_CloseParen:
			if(state == kVFVS_HasRight) {
				--*parenDepth;
				if(parenDepth >= 0) {
					return index + 1;
				} else {
					return view_filter_validate_error(filter, index, input, "Too many )'s");
				}
			} else {
				return view_filter_validate_error(filter, index, input, "Unexpected )");
			}
		case kVFT_LessThan:
		case kVFT_LessThanEquals:
		case kVFT_Equals:
		case kVFT_NotEquals:
		case kVFT_GreaterThan:
		case kVFT_GreaterThanEquals:
			if(state == kVFVS_HasLeft && (bLeftTokenIsNumeric || bLeftTokenIsVerbosity)) {
				// can only follow kVFT_DeltaMillisecondsAbsolute, kVFT_DeltaMillisecondsViewRelative, kVFT_PIEInstance, or kVFT_Verbosity
				state = kVFVS_HasOperator;
				++index;
			} else {
				return view_filter_validate_error(filter, index, input, "Unexpected numeric comparator");
			}
			break;
		case kVFT_String:
			if(state == kVFVS_Empty || state == kVFVS_HasConjunction) {
				state = kVFVS_HasLeft;
				bLeftTokenIsNumeric = false;
				bLeftTokenIsVerbosity = false;
				if(!span_stricmp(token->span, span_from_string("absms"))) {
					token->type = kVFT_DeltaMillisecondsAbsolute;
					bLeftTokenIsNumeric = true;
				} else if(!span_stricmp(token->span, span_from_string("relms"))) {
					token->type = kVFT_DeltaMillisecondsViewRelative;
					bLeftTokenIsNumeric = true;
				} else if(!span_stricmp(token->span, span_from_string("filename"))) {
					token->type = kVFT_Filename;
				} else if(!span_stricmp(token->span, span_from_string("thread"))) {
					token->type = kVFT_Thread;
				} else if(!span_stricmp(token->span, span_from_string("pieinstance"))) {
					token->type = kVFT_PIEInstance;
				} else if(!span_stricmp(token->span, span_from_string("category"))) {
					token->type = kVFT_Category;
				} else if(!span_stricmp(token->span, span_from_string("verbosity"))) {
					token->type = kVFT_Verbosity;
					bLeftTokenIsVerbosity = true;
				} else if(!span_stricmp(token->span, span_from_string("text"))) {
					token->type = kVFT_Text;
				} else {
					return view_filter_validate_error(filter, index, input, "Unknown left hand side");
				}
			} else if(state == kVFVS_HasLeft) {
				state = kVFVS_HasOperator;
				if(!bHasNot && !span_stricmp(token->span, span_from_string("not"))) {
					token->type = kVFT_Not;
					bHasNot = true;
					state = kVFVS_HasLeft;
				} else if(!span_stricmp(token->span, span_from_string("like"))) {
					token->type = kVFT_Like;
				} else if(!span_stricmp(token->span, span_from_string("contains"))) {
					token->type = kVFT_Contains;
				} else if(!span_stricmp(token->span, span_from_string("startswith"))) {
					token->type = kVFT_StartsWith;
				} else if(!span_stricmp(token->span, span_from_string("endswith"))) {
					token->type = kVFT_EndsWith;
				} else {
					return view_filter_validate_error(filter, index, input, "Unknown operator");
				}
			} else if(state == kVFVS_HasOperator) {
				state = kVFVS_HasRight;
				if(bLeftTokenIsNumeric) {
					token->type = kVFT_Number;
					const char *str = va("%.*s", span_length(token->span), token->span.start);
					token->number = strtou32(str);
				} else if(bLeftTokenIsVerbosity) {
					if(!span_stricmp(token->span, span_from_string("log"))) {
						token->number = kBBLogLevel_Log;
					} else if(!span_stricmp(token->span, span_from_string("warning"))) {
						token->number = kBBLogLevel_Warning;
					} else if(!span_stricmp(token->span, span_from_string("error"))) {
						token->number = kBBLogLevel_Error;
					} else if(!span_stricmp(token->span, span_from_string("display"))) {
						token->number = kBBLogLevel_Display;
					} else if(!span_stricmp(token->span, span_from_string("setcolor"))) {
						token->number = kBBLogLevel_SetColor;
					} else if(!span_stricmp(token->span, span_from_string("veryverbose"))) {
						token->number = kBBLogLevel_VeryVerbose;
					} else if(!span_stricmp(token->span, span_from_string("verbose"))) {
						token->number = kBBLogLevel_Verbose;
					} else if(!span_stricmp(token->span, span_from_string("fatal"))) {
						token->number = kBBLogLevel_Fatal;
					} else {
						return view_filter_validate_error(filter, index, input, "Unknown verbosity");
					}
				}
			} else if(state == kVFVS_HasRight) {
				if(!span_stricmp(token->span, span_from_string("and"))) {
					token->type = kVFT_And;
					state = kVFVS_HasConjunction;
				} else if(!span_stricmp(token->span, span_from_string("or"))) {
					token->type = kVFT_Or;
					state = kVFVS_HasConjunction;
				}
			} else {
				return view_filter_validate_error(filter, index, input, "Unexpected token");
			}
			++index;
			break;
		default:
			return view_filter_validate_error(filter, index, input, "Unexpected token");
		}
		BB_WARNING_POP;
	}

	if(state != kVFVS_HasRight) {
		return view_filter_validate_error(filter, index, input, "Incomplete filter");
	}
	return filter->tokens.count;
}

void view_filter_parse(const char *input)
{
	BB_LOG("filter", "parse: %s", input);

	vfilter_t filter = { BB_EMPTY_INITIALIZER };

	// parse the raw tokens
	b32 bAllString = true;
	span_t remaining = span_from_string(input);
	while(!span_is_empty(remaining)) {
		vfilter_token_t token = view_filter_tokenize(&remaining, &filter.error, input);
		BB_LOG("filter", "token type:%d contents:%.*s", token.type, span_length(token.span), token.span.start);
		bba_push(filter.tokens, token);
		if(token.type != kVFT_String) {
			bAllString = false;
		}
	}

	if(bAllString) {
		filter.type = kVF_Legacy;
		BB_LOG("filter", "Legacy: %s", input);
	} else if(filter.tokens.count > 1 && (!span_stricmp(filter.tokens.data[0].span, span_from_string("SQL")) || !span_stricmp(filter.tokens.data[0].span, span_from_string("SQL:")))) {
		filter.type = kVF_SQL;
		BB_LOG("filter", "SQL: %s", filter.tokens.data[1].span.start);
	} else {
		filter.type = kVF_Standard;

		// walk the stack and convert the string tokens to more specific category, like, etc tokens
		int parenDepth = 0;
		view_filter_validate_tokens(&filter, 0, &parenDepth, input);

		if(parenDepth > 0) {
			view_filter_validate_error(&filter, filter.tokens.count - 1, input, "missing )");
		} else if(parenDepth < 0) {
			view_filter_validate_error(&filter, filter.tokens.count - 1, input, "extra )");
		}

		sb_t out = { BB_EMPTY_INITIALIZER };
		for(u32 i = 0; i < filter.tokens.count; ++i) {
			if(i) {
				sb_append(&out, " ");
			}
			vfilter_token_t *token = filter.tokens.data + i;
			if(token->type == kVFT_String) {
				sb_va(&out, "\"%.*s\"", span_length(token->span), token->span.start);
			} else if(token->type == kVFT_Number) {
				sb_va(&out, "%u", token->number);
			} else if(token->type == kVFT_Verbosity) {
				sb_va(&out, "%s", bb_get_log_level_name((bb_log_level_e)token->number, "invalidVerbosity"));
			} else {
				sb_va(&out, "%s", string_from_vfilter_token_type_e(token->type));
			}
		}
		BB_LOG("filter", "Standard: %s", sb_get(&out));
		sb_reset(&out);

		if(sb_len(&filter.error.text) > 0) {
			const char *format = filter.error.column > 0 ? va("%%s\n%%s\n%%%ds^", filter.error.column) : "%s\n%s\n^";
			BB_ERROR("filter", format, sb_get(&filter.error.text), input, " ");
		}
	}

	vfilter_reset(&filter);
}
