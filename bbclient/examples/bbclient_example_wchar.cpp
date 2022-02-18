// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#define BB_WIDECHAR 1

#include "bb.h"
#include "bbclient/bb_common.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"

BB_WARNING_DISABLE(4514) // unreferenced inline function has been removed
BB_WARNING_DISABLE(4710) // function not inlined

#include "bbclient/bb_wrap_stdio.h"
#if defined(_MSC_VER)
#include <windows.h>
#else
#include <stdint.h>
#endif

// Thread code taken from bb_thread.h/.c in mc_common

#if BB_USING(BB_COMPILER_MSVC)

typedef uintptr_t bb_thread_handle_t;
typedef unsigned bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void *args);
#define bb_thread_local __declspec(thread)
#define bb_thread_exit(ret) \
	{                       \
		_endthreadex(ret);  \
		return ret;         \
	}

#include <stdlib.h>

bb_thread_handle_t bbthread_create(bb_thread_func func, void *arg)
{
	bb_thread_handle_t thread = _beginthreadex(
	    NULL, // security,
	    0,    // stack_size,
	    func, // start_address
	    arg,  // arglist
	    0,    // initflag - CREATE_SUSPENDED waits for ResumeThread
	    NULL  // thrdaddr
	);
	BB_ASSERT_MSG(thread != 0, "failed to spawn thread");
	return thread;
}
#else
#include <pthread.h>

typedef pthread_t bb_thread_handle_t;
typedef void *bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void *args);
#define bb_thread_local __thread
#define bb_thread_exit(ret) \
	{                       \
		return ret;         \
	}

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

bb_thread_handle_t bbthread_create(bb_thread_func func, void *arg)
{
	bb_thread_handle_t thread = 0;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(
	    &thread, // thread
	    &attr,   // attr
	    func,    // start_routine
	    arg      // arg
	);
	return thread;
}
#endif

static const wchar_t *s_categoryNames[] = {
	L"dynamic::category::thing",
	L"dynamic::monkey::banana",
};

static const char *s_pathNames[] = {
	"C:\\Windows\\System32\\user32.dll",
	"C:\\Windows\\Fonts\\Consola.ttf",
	"D:\\bin\\wsl-terminal\\vim.exe",
};

static b32 s_bQuit = false;

//enum { InitialBufferLen = 1 };
enum { InitialBufferLen = 1024 * 1024 };
static uint8_t s_initialBuffer[InitialBufferLen];

static void incoming_packet_handler(const bb_decoded_packet_t *decoded, void *context)
{
	(void)context;
	if(decoded->type == kBBPacketType_ConsoleCommand) {
		const char *text = decoded->packet.consoleCommand.text;
		BB_LOG_A("Console", "^:] %s\n", text);
		if(!bb_stricmp(text, "quit")) {
			s_bQuit = true;
		}
	}
}

static bb_thread_return_t before_connect_thread_proc(void *)
{
	BB_THREAD_SET_NAME(L"before_connect_thread_proc");

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 1000000;
	while(now < end && !s_bQuit) {
		BB_TRACE_A(kBBLogLevel_Verbose, "Thread::before_connect_thread_proc", "before_connect_thread_proc %llu dt %llu ms", ++count, now - start);
		bb_sleep_ms(1000);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

static bb_thread_return_t after_connect_thread_proc(void *)
{
	BB_THREAD_SET_NAME(L"after_connect_thread_proc");

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 1000000;
	while(now < end && !s_bQuit) {
		BB_TRACE_A(kBBLogLevel_Verbose, "Thread::after_connect_thread_proc", "after_connect_thread_proc %llu dt %llu ms", ++count, now - start);
		bb_sleep_ms(1000);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

int main(int argc, const char **argv)
{
	uint64_t start = bb_current_time_ms();
	uint32_t categoryIndex = 0;
	uint32_t pathIndex = 0;
	(void)argc;
	(void)argv;

	bb_init_critical_sections();

	bbthread_create(before_connect_thread_proc, nullptr);
	bb_sleep_ms(1000);

	bb_set_initial_buffer(s_initialBuffer, InitialBufferLen);
	//bb_enable_stored_thread_ids(false);

	//bb_init_file_w(L"bbclient.bbox");
	//BB_INIT(L"bbclient: matt");
	BB_INIT_WITH_FLAGS(L"bbclient: matt", kBBInitFlag_ConsoleCommands | kBBInitFlag_DebugInit);
	//BB_INIT_WITH_FLAGS(L"bbclient: matt (no view)", kBBInitFlag_NoOpenView | kBBInitFlag_DebugInit);
	BB_THREAD_START(L"main thread!");
	BB_LOG(L"startup", L"bbclient init took %llu ms", bb_current_time_ms() - start);

	BB_SET_INCOMING_PACKET_HANDLER(&incoming_packet_handler, NULL);

	BB_LOG(L"test::LongLine::Wide::Formatted", L"Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, 0, L"test::LongLine::Wide::Preformatted", L"Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_A("test::LongLine::Utf8::Formatted", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	bb_trace_dynamic_preformatted(__FILE__, (u32)__LINE__, "test::LongLine::Utf8::Preformatted", kBBLogLevel_Log, 0, "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");

	BB_LOG(L"test::category::deep", L"this is a __%s__ test at time %zu", L"simple", bb_current_time_ms());
	BB_WARNING(L"test::other", L"monkey");
	BB_LOG(L"test::a", L"fred");
	BB_LOG(L"testa::bob", L"george");
	BB_ERROR(L"testa", L"chuck");
	BB_LOG(L"standalone::nested::category", L"standalone::nested::category");
	BB_LOG(L"test::unicode::wchar_t", L"Sofía");
	BB_LOG(L"test::unicode::wchar_t", L"(╯°□°）╯︵ ┻━┻");
	BB_LOG(L"test::unicode::wchar_t", L"✨✔");
	BB_LOG(L"test::unicode::utf16", (const bb_wchar_t *)u"Sofía");
	BB_LOG(L"test::unicode::utf16", (const bb_wchar_t *)u"(╯°□°）╯︵ ┻━┻");
	BB_LOG(L"test::unicode::utf16", (const bb_wchar_t *)u"✨✔");
	BB_LOG_A("test::unicode::utf8", u8"Sofía");
	BB_LOG_A("test::unicode::utf8", u8"(╯°□°）╯︵ ┻━┻");
	BB_LOG_A("test::unicode::utf8", u8"✨✔");
	BB_LOG_A("test::unicode::utf8", u8"™");
	BB_LOG_A("test::unicode::utf8", u8"\u2728\u2714"); // ✨✔
	BB_LOG_A("test::unicode::utf8", u8"\uff09");       // ）
	BB_LOG_A("test::unicode::utf8", u8"\ufe35");       // ︵
	BB_LOG_A("test::unicode::utf8", u8"\u2728");       // ✨
	BB_LOG_A("test::unicode::utf8", u8"\u2122");       // ™

	start = bb_current_time_ms();
	while(BB_IS_CONNECTED()) {
		bb_sleep_ms(160);
		BB_TICK();

		BB_LOG(L"test::category", L"new frame");

		BB_LOG_DYNAMIC(s_pathNames[pathIndex++], 1001, s_categoryNames[categoryIndex++], L"This is a %s test!\n", L"**DYNAMIC**");
		if(pathIndex >= BB_ARRAYSIZE(s_pathNames)) {
			pathIndex = 0;
		}
		if(categoryIndex >= BB_ARRAYSIZE(s_categoryNames)) {
			categoryIndex = 0;
		}

		if(bb_current_time_ms() - start > 300) {
			break;
		}
	}

	s_bQuit = true;
	while(BB_IS_CONNECTED() && !s_bQuit) {
		BB_TICK();
		bb_sleep_ms(50);
	}

	s_bQuit = false;
	printf("Here we go...\n");
	bb_disconnect();
	bb_sleep_ms(1000);
	bb_connect(127 << 24 | 1, 0);
	bbthread_create(after_connect_thread_proc, nullptr);

	while(BB_IS_CONNECTED() && !s_bQuit) {
		BB_TICK();
		BB_LOG(L"test::frame", L"-----------------------------------------------------");
		BB_LOG(L"test::spam1", L"some data");
		BB_LOG(L"test::spam1", L"or other");
		BB_LOG(L"test::spam1", L"spamming");
		BB_LOG(L"test::spam1", L"many times");
		BB_LOG(L"test::spam1", L"in the");
		BB_LOG(L"test::spam1", L"same frame");
		BB_LOG(L"test::spam1", L"over and");
		BB_LOG(L"test::spam1", L"over.");
		BB_LOG(L"test::spam2", L"some data");
		BB_LOG(L"test::spam2", L"or other");
		BB_LOG(L"test::spam2", L"spamming");
		BB_LOG(L"test::spam2", L"many times");
		BB_LOG(L"test::spam2", L"in the");
		BB_LOG(L"test::spam2", L"same frame");
		BB_LOG(L"test::spam2", L"over and");
		BB_LOG(L"test::spam2", L"over.");
		BB_LOG(L"test::spam3", L"some data");
		BB_LOG(L"test::spam3", L"or other");
		BB_LOG(L"test::spam3", L"spamming");
		BB_LOG(L"test::spam3", L"many times");
		BB_LOG(L"test::spam3", L"in the");
		BB_LOG(L"test::spam3", L"same frame");
		BB_LOG(L"test::spam3", L"over and");
		BB_LOG(L"test::spam3", L"over.");
		bb_sleep_ms(50);
		//if(bb_current_time_ms() - start > 1000) {
		//	break;
		//}
	}

	BB_SHUTDOWN();
	return 0;
}
