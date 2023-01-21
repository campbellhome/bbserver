// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "sb.h"

typedef enum tag_bugType
{
	kBugType_Bug,
	kBugType_Assert,
	kBugType_Crash,
} bugType;

typedef struct tag_bugReport
{
	b32 bSilent;
	bugType type;
	sb_t guid;
	sb_t dir;
	sb_t title;
	sb_t desc;
	sb_t version;
	u32 addr;
	u16 port;
	u8 pad[2];
} bugReport;

void bug_reporter_init(const char* project, const char* assignee);
void bug_reporter_shutdown(void);

bugReport* bug_report_init(void);
void bug_report_dispatch_sync(bugReport* report);
void bug_report_dispatch_async(bugReport* report);
void bug_report_abandon(bugReport* report);

#if defined(__cplusplus)
}
#endif
