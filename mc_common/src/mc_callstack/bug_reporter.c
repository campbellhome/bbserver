// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "mc_callstack/bug_reporter.h"
#include "appdata.h"
#include "bb_assert.h"
#include "bb_connection.h"
#include "bb_file.h"
#include "bb_malloc.h"
#include "bb_serialize.h"
#include "bb_thread.h"
#include "bb_wrap_process.h"
#include "cmdline.h"
#include "path_utils.h"
#include "uuid_rfc4122/uuid.h"
#include <stdlib.h>
#include <string.h>

static sb_t s_project;
static sb_t s_assignee;
static bb_critical_section s_bugReport_cs;
static bb_connection_t s_bugReport_con;

void bug_reporter_init(const char* project, const char* assignee)
{
	sb_reset(&s_project);
	sb_append(&s_project, project);
	sb_reset(&s_assignee);
	sb_append(&s_assignee, assignee);
	bb_critical_section_init(&s_bugReport_cs);
}

void bug_reporter_shutdown(void)
{
	sb_reset(&s_project);
	sb_reset(&s_assignee);
	bb_critical_section_shutdown(&s_bugReport_cs);
}

static void bug_report_reset(bugReport* report)
{
	sb_reset(&report->guid);
	sb_reset(&report->dir);
	sb_reset(&report->title);
	sb_reset(&report->desc);
	sb_reset(&report->version);
	bb_free(report);
}

bugReport* bug_report_init(void)
{
	bugReport* report = bb_malloc(sizeof(bugReport));
	if (report)
	{
		memset(report, 0, sizeof(bugReport));
		if (s_project.data && *s_project.data)
		{
			rfc_uuid uuid = { BB_EMPTY_INITIALIZER };
			if (uuid_create(&uuid))
			{
				char uuidBuffer[64];
				format_uuid(&uuid, uuidBuffer, sizeof(uuidBuffer));
				sb_append(&report->guid, uuidBuffer);
			}
			sb_t appdata = appdata_get(sb_get(&s_project));
			sb_va(&report->dir, "%s/bugs/%s", sb_get(&appdata), sb_get(&report->guid));
			path_resolve_inplace(&report->dir);
			path_mkdir(sb_get(&report->dir));
			sb_reset(&appdata);
		}
		if (report->guid.count < 1 || report->dir.count < 1)
		{
			bug_report_reset(report);
			report = NULL;
		}
	}
	return report;
}

static void bug_report_write_string_data(bb_serialize_t* ser, const char* str)
{
	while (*str)
	{
		char c = *str++;
		bbserialize_s8(ser, (s8*)&c);
	}
}

static void bug_report_write_sb(bb_serialize_t* ser, const char* prefix, sb_t suffix)
{
	u16 len = (u16)(strlen(prefix) + sb_len(&suffix));
	bbserialize_u16(ser, &len);
	bug_report_write_string_data(ser, prefix);
	bug_report_write_string_data(ser, sb_get(&suffix));
}

static void bug_report_write_string(bb_serialize_t* ser, const char* str)
{
	u16 len = (u16)(strlen(str));
	bbserialize_u16(ser, &len);
	bug_report_write_string_data(ser, str);
}

static u16 bug_report_serialize(bugReport* source, s8* buffer, u16 len)
{
	u32 packetType = 1;

	u32 numStrings = 4;
	if (sb_len(&s_assignee) > 0)
	{
		++numStrings;
	}
	if (sb_len(&source->title) > 0)
	{
		++numStrings;
	}
	if (sb_len(&source->desc) > 0)
	{
		++numStrings;
	}
	if (source->bSilent)
	{
		++numStrings;
	}

	bb_serialize_t ser;
	bbserialize_init_write(&ser, buffer, len);

	bbserialize_u32(&ser, &packetType);
	bbserialize_u32(&ser, &numStrings);
	switch (source->type)
	{
	case kBugType_Bug:
		bug_report_write_string(&ser, "-Type=LocalBug");
		break;
	case kBugType_Assert:
		bug_report_write_string(&ser, "-Type=LocalEnsure");
		break;
	case kBugType_Crash:
		bug_report_write_string(&ser, "-Type=LocalCrash");
		break;
	default:
		return 0;
	}
	bug_report_write_sb(&ser, "-Project=", s_project);
	bug_report_write_sb(&ser, "-BugGuid=", source->guid);
	bug_report_write_sb(&ser, "-BugDir=", source->dir);
	if (sb_len(&s_assignee) > 0)
	{
		bug_report_write_sb(&ser, "-Assignee=", s_assignee);
	}
	if (sb_len(&source->title) > 0)
	{
		bug_report_write_sb(&ser, "-Title=", source->title);
	}
	if (sb_len(&source->desc) > 0)
	{
		bug_report_write_sb(&ser, "-Desc=", source->desc);
	}
	if (source->bSilent)
	{
		bug_report_write_string(&ser, "-Silent");
	}

	if (ser.state == kBBSerialize_Ok)
	{
		return (u16)ser.nCursorBytes;
	}
	else
	{
		return 0;
	}
}

void bug_report_write_runtime_xml(const bugReport* report)
{
	sb_t contents = { BB_EMPTY_INITIALIZER };

	sb_append(&contents, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	sb_append(&contents, "<FGenericCrashContext>\n");
	sb_append(&contents, "<RuntimeProperties>\n");
	sb_va(&contents, "<CrashGUID>%s</CrashGUID>\n", sb_get(&report->guid));
	switch (report->type)
	{
	case kBugType_Bug:
		sb_append(&contents, "<CrashType>Bug</CrashType>\n");
		break;
	case kBugType_Assert:
		sb_append(&contents, "<CrashType>Assert</CrashType>\n");
		break;
	case kBugType_Crash:
		sb_append(&contents, "<CrashType>Crash</CrashType>\n");
		break;
	default:
		BB_BREAK();
	}
	sb_append(&contents, "<GameName>Blackbox</GameName>\n");
	sb_append(&contents, "<ExecutableName>bb</ExecutableName>\n");
	sb_va(&contents, "<EngineVersion>%s</EngineVersion>\n", sb_get(&report->version));
	sb_va(&contents, "<BuildVersion>%s</BuildVersion>\n", sb_get(&report->version));
	sb_va(&contents, "<CommandLine>%s</CommandLine>\n", cmdline_get_full());
	sb_va(&contents, "<Misc.OSVersionMajor>%s</Misc.OSVersionMajor>\n", bb_platform_name(bb_platform()));
	sb_append(&contents, "<Misc.OSVersionMinor></Misc.OSVersionMinor>\n");
	sb_append(&contents, "</RuntimeProperties>\n");
	sb_append(&contents, "</FGenericCrashContext>\n");

	if (contents.count > 1)
	{
		sb_t path = { BB_EMPTY_INITIALIZER };
		sb_va(&path, "%s\\CrashContext.runtime-xml", sb_get(&report->dir));
		path_resolve_inplace(&path);
		bb_file_handle_t fp = bb_file_open_for_write(sb_get(&path));
		if (fp != BB_INVALID_FILE_HANDLE)
		{
			bb_file_write(fp, contents.data, contents.count - 1);
			bb_file_close(fp);
		}
		sb_reset(&path);
	}
	sb_reset(&contents);
}

void bug_report_dispatch_sync(bugReport* report)
{
	if (sb_len(&report->guid))
	{
		s8 buf[4096];
		u16 serializedLen = bug_report_serialize(report, buf, sizeof(buf));
		if (serializedLen)
		{
			bug_report_write_runtime_xml(report);
			bb_critical_section_lock(&s_bugReport_cs);
			bbcon_init(&s_bugReport_con);

			if (bbcon_connect_client(&s_bugReport_con, report->addr, (u16)report->port, 5))
			{
				bbcon_send_raw(&s_bugReport_con, buf, serializedLen);
				bbcon_flush(&s_bugReport_con);
				bbcon_disconnect(&s_bugReport_con);
			}

			bbcon_shutdown(&s_bugReport_con);
			bb_critical_section_unlock(&s_bugReport_cs);
		}
	}
	bug_report_reset(report);
}

static bb_thread_return_t bug_reporter_thread_func(void* args)
{
	bbthread_set_name("bug_reporter_thread_func");
	bugReport* report = args;
	bug_report_dispatch_sync(report);
	bb_thread_exit(0);
}

void bug_report_dispatch_async(bugReport* report)
{
	bbthread_create(bug_reporter_thread_func, report);
}

void bug_report_abandon(bugReport* report)
{
	bug_report_reset(report);
}
