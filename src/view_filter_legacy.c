// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#include "view_filter_legacy.h"
#include "bb_string.h"
#include "recorded_session.h"
#include "view.h"

typedef enum view_filter_comparison_e {
	kViewFilterComparison_Equal,
	kViewFilterComparison_GreaterEqual,
	kViewFilterComparison_Greater,
	kViewFilterComparison_LessThanEqual,
	kViewFilterComparison_LessThan,
	kViewFilterComparison_NotEqual,
	kViewFilterComparison_Count
} view_filter_comparison_t;

static const char *s_view_filter_comparator[] = {
	"==",
	">=",
	">",
	"<=",
	"<",
	"!=",
};
BB_CTASSERT(BB_ARRAYSIZE(s_view_filter_comparator) == kViewFilterComparison_Count);

static view_filter_comparison_t view_parse_filter_comparator(const char **token)
{
	for(u32 i = 0; i < kViewFilterComparison_Count; ++i) {
		size_t len = strlen(s_view_filter_comparator[i]);
		if(!strncmp(*token, s_view_filter_comparator[i], len)) {
			*token += len;
			return i;
		}
	}
	return kViewFilterComparison_Count;
}

static inline b32 view_filter_compare_double(view_filter_comparison_t comp, double a, double b)
{
	switch(comp) {
	case kViewFilterComparison_Equal: return a == b;
	case kViewFilterComparison_GreaterEqual: return a >= b;
	case kViewFilterComparison_Greater: return a > b;
	case kViewFilterComparison_LessThanEqual: return a <= b;
	case kViewFilterComparison_LessThan: return a < b;
	case kViewFilterComparison_NotEqual: return a != b;
	case kViewFilterComparison_Count:
	default: return false;
	}
}

static b32 view_filter_abs_millis(view_t *view, recorded_log_t *log, view_filter_comparison_t comp, const char *token)
{
	recorded_session_t *session = view->session;
	recorded_log_t *lastLog = view->lastSessionLogIndex < session->logs.count ? session->logs.data[view->lastSessionLogIndex] : NULL;
	bb_decoded_packet_t *decoded = &log->packet;

	double thresholdMillis = atof(token);
	s64 prevElapsedTicks = (lastLog) ? (s64)lastLog->packet.header.timestamp - (s64)session->appInfo.header.timestamp : 0;
	s64 elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
	double deltaMillis = (elapsedTicks - prevElapsedTicks) * session->appInfo.packet.appInfo.millisPerTick;
	return view_filter_compare_double(comp, deltaMillis, thresholdMillis);
}

static b32 view_filter_rel_millis(view_t *view, recorded_log_t *log, view_filter_comparison_t comp, const char *token)
{
	recorded_session_t *session = view->session;
	recorded_log_t *lastLog = view->lastVisibleSessionLogIndex < session->logs.count ? session->logs.data[view->lastVisibleSessionLogIndex] : NULL;
	bb_decoded_packet_t *decoded = &log->packet;

	double thresholdMillis = atof(token);
	s64 prevElapsedTicks = (lastLog) ? (s64)lastLog->packet.header.timestamp - (s64)session->appInfo.header.timestamp : 0;
	s64 elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
	double deltaMillis = (elapsedTicks - prevElapsedTicks) * session->appInfo.packet.appInfo.millisPerTick;
	return view_filter_compare_double(comp, deltaMillis, thresholdMillis);
}

static b32 view_filter_legacy_find_token(view_t *view, recorded_log_t *log, const char *token)
{
	bb_decoded_packet_t *decoded = &log->packet;
	const char *text = decoded->packet.logText.text;
	if(!bb_strnicmp(token, "absms", 5)) {
		const char *tmp = token + 5;
		view_filter_comparison_t comp = view_parse_filter_comparator(&tmp);
		if(comp != kViewFilterComparison_Count) {
			return view_filter_abs_millis(view, log, comp, tmp);
		}
	}
	if(!bb_strnicmp(token, "relms", 5)) {
		const char *tmp = token + 5;
		view_filter_comparison_t comp = view_parse_filter_comparator(&tmp);
		if(comp != kViewFilterComparison_Count) {
			return view_filter_abs_millis(view, log, comp, tmp);
		}
	}
	return bb_stristr(text, token) != NULL;
}

b32 view_filter_visible_legacy(view_t *view, recorded_log_t *log)
{
	b32 ok = false;
	u32 numRequired = 0;
	u32 numProhibited = 0;
	u32 numAllowed = 0;
	u32 i;
	for(i = 0; i < view->vfilter.tokens.count; ++i) {
		const vfilter_token_t *token = view->vfilter.tokens.data + i;
		const char *s = token->span.start;
		b32 required = *s == '+';
		b32 prohibited = *s == '-';
		numRequired += required;
		numProhibited += prohibited;
		numAllowed += (!required && !prohibited);
		if(required || prohibited) {
			++s;
		}
		b32 found = view_filter_legacy_find_token(view, log, s);
		if(found && prohibited || !found && required) {
			return false;
			break;
		} else if(found) {
			ok = true;
		}
	}
	if(!ok && numAllowed) {
		return false;
	}
	return true;
}
