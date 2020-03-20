// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bbserver_asserts.h"
#include "common.h"

#define FEATURE_CALLSTACKS BB_ON

#if BB_USING(FEATURE_CALLSTACKS)

#include "app_update.h"
#include "bb_file.h"
#include "bb_thread.h"
#include "cmdline.h"
#include "config.h"
#include "mc_callstack/bug_reporter.h"
#include "mc_callstack/callstack_utils.h"
#include "mc_callstack/exception_handler.h"
#include "sb.h"
#include "site_config.h"
#include "va.h"

#include "bb_wrap_stdio.h"

static const char *s_logPath;

static bbassert_action_e App_AssertHandler(const char *condition, const char *message, const char *file, const int line)
{
	// static buffers so we can safely assert in a memory allocator :)
	static bb_thread_local char output[2048];
	int len;
	BB_WARNING_PUSH(4996);
	if(message && *message) {
		len = bb_snprintf(output, sizeof(output), "Assert Failure: '%s': %s [%s:%d]", condition, message, file, line);
	} else {
		len = bb_snprintf(output, sizeof(output), "Assert Failure: '%s' [%s:%d]", condition, file, line);
	}
	len = (len > 0 && len < (int)sizeof(output)) ? len : (int)sizeof(output) - 1;
	output[len] = '\0';
	BB_WARNING_POP;

	sb_t callstack = callstack_generate_sb(2); // skip App_AssertHandler and bbassert_dispatch
	if(message && *message) {
		BB_ERROR("Assert", "Assert Failure: '%s': %s\n\n%s", condition, message, sb_get(&callstack));
	} else {
		BB_ERROR("Assert", "Assert Failure: '%s'\n\n%s", condition, sb_get(&callstack));
	}

	bbassert_action_e action = kBBAssertAction_Break;
	if(sb_len(&g_site_config.bugProject)) {
		bugReport *report = bug_report_init();
		if(report) {
			report->type = kBugType_Assert;
			report->bSilent = true;
			report->addr = 127 << 24 | 1;
			report->port = g_site_config.bugPort;
			sb_append(&report->version, Update_GetCurrentVersion());
			sb_append(&report->title, output);
			sb_t target = { BB_EMPTY_INITIALIZER };
			if(s_logPath && *s_logPath) {
				BB_FLUSH();
				sb_va(&target, "%s\\bb.bbox", sb_get(&report->dir));
				CopyFileA(s_logPath, sb_get(&target), false);
				sb_reset(&target);
			}
			if(callstack.count > 1) {
				sb_va(&target, "%s\\Callstack.txt", sb_get(&report->dir));
				bb_file_handle_t fp = bb_file_open_for_write(sb_get(&target));
				if(fp != BB_INVALID_FILE_HANDLE) {
					bb_file_write(fp, callstack.data, callstack.count - 1);
					bb_file_close(fp);
				}
				sb_reset(&target);
			}
			bug_report_dispatch_async(report);
		}
	}
	BB_TRACE(kBBLogLevel_Verbose, "Assert", "Finished with assert handler");
	BB_FLUSH();
	if(g_config.assertMessageBox) {
		const char *buttonDesc = "\nBreak in the debugger?";
		if(message && *message) {
			len = bb_snprintf(output, sizeof(output), "%s(%d):\n\nAssert Failure: '%s': %s\n%s\n\n%s", file, line, condition, message, buttonDesc, sb_get(&callstack));
		} else {
			len = bb_snprintf(output, sizeof(output), "%s(%d):\n\nAssert Failure: '%s'\n%s\n\n%s", file, line, condition, buttonDesc, sb_get(&callstack));
		}
		len = (len > 0 && len < (int)sizeof(output)) ? len : (int)sizeof(output) - 1;
		output[len] = '\0';

		if(MessageBoxA(NULL, output, "Blackbox Assert Failure", MB_YESNO) == IDYES) {
			action = kBBAssertAction_Break;
		} else {
			action = kBBAssertAction_Continue;
		}
	}
	sb_reset(&callstack);
	return action;
}

static sb_t App_BuildExceptionInfo(EXCEPTION_POINTERS *pExPtrs)
{
	sb_t out = { BB_EMPTY_INITIALIZER };
	sb_append(&out, "Blackbox Unhandled Exception: ");
	EXCEPTION_RECORD *record = pExPtrs->ExceptionRecord;
	switch(record->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION: {
		sb_append(&out, "EXCEPTION_ACCESS_VIOLATION ");
		if(record->ExceptionInformation[0]) {
			sb_append(&out, "writing");
		} else {
			sb_append(&out, "reading");
		}
		sb_va(&out, " address 0x%08X", record->ExceptionInformation[1]);
		break;
	}
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: sb_append(&out, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"); break;
	case EXCEPTION_BREAKPOINT: sb_append(&out, "EXCEPTION_BREAKPOINT"); break;
	case EXCEPTION_DATATYPE_MISALIGNMENT: sb_append(&out, "EXCEPTION_DATATYPE_MISALIGNMENT"); break;
	case EXCEPTION_FLT_DENORMAL_OPERAND: sb_append(&out, "EXCEPTION_FLT_DENORMAL_OPERAND"); break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: sb_append(&out, "EXCEPTION_FLT_DIVIDE_BY_ZERO"); break;
	case EXCEPTION_FLT_INEXACT_RESULT: sb_append(&out, "EXCEPTION_FLT_INEXACT_RESULT"); break;
	case EXCEPTION_FLT_INVALID_OPERATION: sb_append(&out, "EXCEPTION_FLT_INVALID_OPERATION"); break;
	case EXCEPTION_FLT_OVERFLOW: sb_append(&out, "EXCEPTION_FLT_OVERFLOW"); break;
	case EXCEPTION_FLT_STACK_CHECK: sb_append(&out, "EXCEPTION_FLT_STACK_CHECK"); break;
	case EXCEPTION_FLT_UNDERFLOW: sb_append(&out, "EXCEPTION_FLT_UNDERFLOW"); break;
	case EXCEPTION_ILLEGAL_INSTRUCTION: sb_append(&out, "EXCEPTION_ILLEGAL_INSTRUCTION"); break;
	case EXCEPTION_IN_PAGE_ERROR: sb_append(&out, "EXCEPTION_IN_PAGE_ERROR"); break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO: sb_append(&out, "EXCEPTION_INT_DIVIDE_BY_ZERO"); break;
	case EXCEPTION_INT_OVERFLOW: sb_append(&out, "EXCEPTION_INT_OVERFLOW"); break;
	case EXCEPTION_INVALID_DISPOSITION: sb_append(&out, "EXCEPTION_INVALID_DISPOSITION"); break;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: sb_append(&out, "EXCEPTION_NONCONTINUABLE_EXCEPTION"); break;
	case EXCEPTION_PRIV_INSTRUCTION: sb_append(&out, "EXCEPTION_PRIV_INSTRUCTION"); break;
	case EXCEPTION_SINGLE_STEP: sb_append(&out, "EXCEPTION_SINGLE_STEP"); break;
	case EXCEPTION_STACK_OVERFLOW: sb_append(&out, "EXCEPTION_STACK_OVERFLOW"); break;
	default:
		sb_va(&out, "0x%8.8X", record->ExceptionCode);
		break;
	}
	return out;
}

static void App_ExceptionHandler(EXCEPTION_POINTERS *pExPtrs)
{
	//volatile bool s_done = false;
	//while(!s_done) {
	//	bb_sleep_ms(1000);
	//}

	sb_t title = App_BuildExceptionInfo(pExPtrs);
	BB_ERROR("Crash", "%s", sb_get(&title));
	sb_t callstack = callstack_generate_crash_sb();
	BB_ERROR("Crash", "%s", sb_get(&callstack));

	bugReport *report = bug_report_init();
	if(report) {
		report->type = kBugType_Crash;
		report->bSilent = false;
		report->addr = 127 << 24 | 1;
		report->port = g_site_config.bugPort;
		sb_append(&report->version, Update_GetCurrentVersion());
		report->title = sb_clone(&title);
		sb_t target = { BB_EMPTY_INITIALIZER };
		if(s_logPath && *s_logPath) {
			BB_FLUSH();
			sb_va(&target, "%s\\bb.bbox", sb_get(&report->dir));
			CopyFileA(s_logPath, sb_get(&target), false);
			sb_reset(&target);
		}
		if(callstack.count > 1) {
			sb_va(&target, "%s\\Callstack.txt", sb_get(&report->dir));
			bb_file_handle_t fp = bb_file_open_for_write(sb_get(&target));
			if(fp != BB_INVALID_FILE_HANDLE) {
				bb_file_write(fp, callstack.data, callstack.count - 1);
				bb_file_close(fp);
			}
			sb_reset(&target);
		}
		bug_report_dispatch_sync(report);
	} else {
		MessageBoxA(0, va("%s\n\n%s", sb_get(&title), sb_get(&callstack)), "Blackbox Unhandled Exception", MB_OK);
	}
	sb_reset(&callstack);
	sb_reset(&title);
}

#endif // FEATURE_CALLSTACKS

void BBServer_InitAsserts(const char *logPath)
{
	BB_UNUSED(logPath);
#if BB_USING(FEATURE_CALLSTACKS)
	s_logPath = logPath;
	callstack_init();
	if(sb_len(&g_site_config.bugProject)) {
		bug_reporter_init(sb_get(&g_site_config.bugProject), sb_get(&g_site_config.bugAssignee));
	}

	install_unhandled_exception_handler(&App_ExceptionHandler);
	bbassert_set_handler(&App_AssertHandler);
#endif
}

void BBServer_ShutdownAsserts(void)
{
#if BB_USING(FEATURE_CALLSTACKS)
	bug_reporter_shutdown();
	callstack_shutdown();
#endif
}
