// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#define BB_WIDECHAR 1

#include "bb.h"
#include "bbclient/bb_assert.h"
#include "bbclient/bb_common.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"

BB_WARNING_DISABLE(4514) // unreferenced inline function has been removed
BB_WARNING_DISABLE(4710) // function not inlined

#include "bbclient/bb_wrap_process.h"
#include "bbclient/bb_wrap_stdio.h"
#include "bbclient/bb_wrap_windows.h"
#if !defined(_MSC_VER)
#include <stdint.h>
#endif

#include <locale.h>

// Thread code taken from bb_thread.h/.c in mc_common

#if BB_USING(BB_COMPILER_MSVC)

typedef uintptr_t bb_thread_handle_t;
typedef unsigned bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void* args);
#define bb_thread_local __declspec(thread)
#define bb_thread_exit(ret) \
	{                       \
		_endthreadex(ret);  \
		return ret;         \
	}

#include <stdlib.h>

bb_thread_handle_t bbthread_create(bb_thread_func func, void* arg)
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
void bbthread_join(bb_thread_handle_t threadHandle)
{
	WaitForSingleObject((HANDLE)threadHandle, INFINITE);
	CloseHandle((HANDLE)threadHandle);
}
#else
#include <pthread.h>

typedef pthread_t bb_thread_handle_t;
typedef void* bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void* args);
#define bb_thread_local __thread
#define bb_thread_exit(ret) \
	{                       \
		return ret;         \
	}

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

bb_thread_handle_t bbthread_create(bb_thread_func func, void* arg)
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
void bbthread_join(bb_thread_handle_t threadHandle)
{
	pthread_join(threadHandle, NULL);
}
#endif

static const wchar_t* s_categoryNames[] = {
	L"dynamic::category::thing",
	L"dynamic::monkey::banana",
};

static const char* s_pathNames[] = {
	"C:\\Windows\\System32\\user32.dll",
	"C:\\Windows\\Fonts\\Consola.ttf",
	"D:\\bin\\wsl-terminal\\vim.exe",
};

static b32 s_bQuit = false;
static u32 s_NumContinues = 0;

// enum { InitialBufferLen = 1 };
enum
{
	InitialBufferLen = 1024 * 1024
};
static uint8_t s_initialBuffer[InitialBufferLen];

struct console_autocomplete_entry
{
	const char* command;
	const char* description;
	b32 bCommand;
	u32 pad;
};

static console_autocomplete_entry s_autocompleteEntries[] = {
	{ "continue", "continue to next section of test app", true, 0 },
	{ "quit", "quit the application", true, 0 },
	{ "jumbolog", "extra large log", true, 0 },
	{ "log LogTemp", "set log category", true, 0 },
	{ "log LogConsole", "set log category", true, 0 },
	{ "log LogMatchmaking", "set log category", true, 0 },
	{ "con.autocomplete", "a cvar!", false, 0 },
	{ "map Evansburgh_A", "load a map", true, 0 },
	{ "map Evansburgh_B", "load a map", true, 0 },
	{ "map Evansburgh_C", "load a map", true, 0 },
	{ "map Evansburgh_D", "load a map", true, 0 },
	{ "map Evansburgh_E", "load a map", true, 0 },
	{ "map BlueDog_A", "load a map", true, 0 },
	{ "map BlueDog_B", "load a map", true, 0 },
	{ "map BlueDog_C", "load a map", true, 0 },
	{ "map BlueDog_D", "load a map", true, 0 },
	{ "map BlueDog_E", "load a map", true, 0 },
	{ "map TestMap_A", "load a map", true, 0 },
	{ "map TestMap_B", "load a map", true, 0 },
	{ "map TestMap_C", "load a map", true, 0 },
	{ "map TestMap_D", "load a map", true, 0 },
	{ "map TestMap_E", "load a map", true, 0 },
	{ "map TestMap_F", "load a map", true, 0 },
	{ "map TestMap_G", "load a map", true, 0 },
	{ "map TestMap_H", "load a map", true, 0 },
	{ "map TestMap_I", "load a map", true, 0 },
	{ "map TestMap_J", "load a map", true, 0 },
	{ "map TestMap_K", "load a map", true, 0 },
	{ "map TestMap_L", "load a map", true, 0 },
	{ "map TestMap_M", "load a map", true, 0 },
	{ "map TestMap_N", "load a map", true, 0 },
	{ "map TestMap_O", "load a map", true, 0 },
	{ "map TestMap_P", "load a map", true, 0 },
	{ "map TestMap_Q", "load a map", true, 0 },
	{ "map TestMap_R", "load a map", true, 0 },
	{ "map TestMap_S", "load a map", true, 0 },
	{ "map TestMap_T", "load a map", true, 0 },
	{ "map TestMap_U", "load a map", true, 0 },
	{ "map TestMap_V", "load a map", true, 0 },
	{ "map TestMap_W", "load a map", true, 0 },
	{ "map TestMap_X", "load a map", true, 0 },
	{ "map TestMap_Y", "load a map", true, 0 },
	{ "map TestMap_Z", "load a map", true, 0 },
};

static void incoming_packet_handler(const bb_decoded_packet_t* decoded, void* context)
{
	(void)context;
	if (decoded->type == kBBPacketType_ConsoleCommand)
	{
		const char* text = decoded->packet.consoleCommand.text;
		BB_LOG_A("Console", "^:] %s\n", text);
		if (!bb_stricmp(text, "quit"))
		{
			s_bQuit = true;
			s_NumContinues = INT_MAX;
		}
		else if (!bb_stricmp(text, "continue"))
		{
			++s_NumContinues;
		}
		else if (!bb_stricmp(text, "jumbolog"))
		{
			size_t largeLen = 1024ull * 1024ull * 1024ull;
			char* large = (char*)malloc(largeLen + 1);
			if (large)
			{
				for (size_t i = 0; i < largeLen; ++i)
				{
					large[i] = '0' + (char)(i % 10);
				}
				large[largeLen] = '\0';
				bb_trace_dynamic_preformatted(__FILE__, __LINE__, "test::LongLine::ExtraLongLine", kBBLogLevel_Log, -1, large);
				free(large);
			}
		}
	}
	else if (decoded->type == kBBPacketType_ConsoleAutocompleteRequest)
	{
		size_t len = strlen(decoded->packet.consoleAutocompleteRequest.text);
		u32 total = 0;
		if (len)
		{
			for (u32 i = 0; i < BB_ARRAYSIZE(s_autocompleteEntries); ++i)
			{
				if (!bb_strnicmp(s_autocompleteEntries[i].command, decoded->packet.consoleAutocompleteRequest.text, len))
				{
					++total;
				}
			}
		}

		BB_LOG_A("Console", "^:autocomplete %u (%u)] %s\n", decoded->packet.consoleAutocompleteRequest.id, total, decoded->packet.consoleAutocompleteRequest.text);

		bb_decoded_packet_t out = { BB_EMPTY_INITIALIZER };
		out.type = kBBPacketType_ConsoleAutocompleteResponseHeader;
		out.packet.consoleAutocompleteResponseHeader.id = decoded->packet.consoleAutocompleteRequest.id;
		out.packet.consoleAutocompleteResponseHeader.total = total;
		out.packet.consoleAutocompleteResponseHeader.reuse = false;
		bb_send_raw_packet(&out);

		if (total)
		{
			out.type = kBBPacketType_ConsoleAutocompleteResponseEntry;
			out.packet.consoleAutocompleteResponseEntry.id = decoded->packet.consoleAutocompleteRequest.id;
			for (u32 i = 0; i < BB_ARRAYSIZE(s_autocompleteEntries); ++i)
			{
				if (!bb_strnicmp(s_autocompleteEntries[i].command, decoded->packet.consoleAutocompleteRequest.text, len))
				{
					out.packet.consoleAutocompleteResponseEntry.command = s_autocompleteEntries[i].bCommand;
					bb_strncpy(out.packet.consoleAutocompleteResponseEntry.text, s_autocompleteEntries[i].command, sizeof(out.packet.consoleAutocompleteResponseEntry.text));
					bb_strncpy(out.packet.consoleAutocompleteResponseEntry.description, s_autocompleteEntries[i].description, sizeof(out.packet.consoleAutocompleteResponseEntry.description));
					bb_send_raw_packet(&out);
				}
			}
		}
	}
}

static bb_thread_return_t before_connect_thread_proc(void*)
{
	BB_THREAD_SET_NAME(L"before_connect_thread_proc");
	BB_TRACE_A(kBBLogLevel_Verbose, "Thread::before_connect_thread_proc", "before_connect_thread_proc started");

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 1000000;
	while (now < end && !s_bQuit)
	{
		BB_TRACE_A(kBBLogLevel_Verbose, "Thread::before_connect_thread_proc", "before_connect_thread_proc %llu dt %llu ms", ++count, now - start);
		bb_sleep_ms(1000);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

static bb_thread_return_t also_before_connect_thread_proc(void*)
{
	BB_THREAD_SET_NAME(L"also_before_connect_thread_proc");
	BB_TRACE_A(kBBLogLevel_Verbose, "Thread::also_before_connect_thread_proc", "also_before_connect_thread_proc started");

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 1000000;
	while (now < end && !s_bQuit)
	{
		BB_TRACE_A(kBBLogLevel_Verbose, "Thread::also_before_connect_thread_proc", "also_before_connect_thread_proc %llu dt %llu ms", ++count, now - start);
		bb_sleep_ms(1000);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

static bb_thread_return_t after_connect_thread_proc(void*)
{
	BB_THREAD_SET_NAME(L"after_connect_thread_proc");
	BB_TRACE_A(kBBLogLevel_Verbose, "Thread::after_connect_thread_proc", "after_connect_thread_proc started");

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 1000000;
	while (now < end && !s_bQuit)
	{
		BB_TRACE_A(kBBLogLevel_Verbose, "Thread::after_connect_thread_proc", "after_connect_thread_proc %llu dt %llu ms", ++count, now - start);
		bb_sleep_ms(1000);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

int main(int argc, const char** argv)
{
	setlocale(LC_ALL, ".UTF8");

	uint64_t start = bb_current_time_ms();
	uint32_t categoryIndex = 0;
	uint32_t pathIndex = 0;
	uint64_t frameNumber = 0;
	(void)argc;
	(void)argv;

	//bb_init_critical_sections();

	bb_thread_handle_t before_connect_thread_handle = bbthread_create(before_connect_thread_proc, nullptr);
	bb_thread_handle_t also_before_connect_thread_handle = bbthread_create(also_before_connect_thread_proc, nullptr);
	bb_sleep_ms(1000);

	bb_set_initial_buffer(s_initialBuffer, InitialBufferLen);
	// bb_enable_stored_thread_ids(false);

	bb_init_file_w(L"bbclient.bbox");
	// BB_INIT(L"bbclient: matt");
	BB_INIT_WITH_FLAGS(L"bbclient: matt", kBBInitFlag_ConsoleCommands | kBBInitFlag_DebugInit | kBBInitFlag_ConsoleAutocomplete);
	// BB_INIT_WITH_FLAGS(L"bbclient: matt (no view)", kBBInitFlag_NoOpenView | kBBInitFlag_DebugInit);
	BB_THREAD_START(L"main thread!");
	BB_LOG(L"startup", L"bbclient init took %llu ms", bb_current_time_ms() - start);

	BB_START_FRAME_NUMBER(++frameNumber);
	BB_LOG(L"frameNumber", L"frameNumber: %llu", frameNumber);
	BB_START_FRAME_NUMBER(++frameNumber);
	BB_LOG(L"frameNumber", L"frameNumber: %llu", frameNumber);
	BB_START_FRAME_NUMBER(++frameNumber);
	BB_LOG(L"frameNumber", L"frameNumber: %llu", frameNumber);
	BB_START_FRAME_NUMBER(++frameNumber);

	BB_SET_INCOMING_PACKET_HANDLER(&incoming_packet_handler, NULL);

	BB_LOG(L"test::LongLine::Wide::Formatted", L"Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, 0, L"test::LongLine::Wide::Preformatted", L"Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_A("test::LongLine::Utf8::Formatted", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	bb_trace_dynamic_preformatted(__FILE__, (u32)__LINE__, "test::LongLine::Utf8::Preformatted", kBBLogLevel_Log, 0, "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");

	bb_trace_dynamic_preformatted(nullptr, 0u, nullptr, kBBLogLevel_Log, 0, "NULL file and category");

	BB_LOG(L"test::category::deep", L"this is a __%s__ test at time %zu", L"simple", bb_current_time_ms());
	BB_WARNING(L"test::other", L"monkey");
	BB_LOG(L"test::a", L"fred");
	BB_LOG(L"testa::bob", L"george");
	BB_ERROR(L"testa", L"chuck");
	BB_LOG(L"standalone::nested::category", L"standalone::nested::category");

	// repeating connection test for iteration on bbserver console input
	while (s_NumContinues < 1)
	{
		BB_LOG(L"test::unicode::wchar_t", L"Sofía");
		BB_LOG(L"test::unicode::wchar_t", L"(╯°□°）╯︵ ┻━┻");
		BB_LOG(L"test::unicode::wchar_t", L"✨✔");
		BB_LOG(L"test::unicode::utf16", (const bb_wchar_t*)u"Sofía");
		BB_LOG(L"test::unicode::utf16", (const bb_wchar_t*)u"(╯°□°）╯︵ ┻━┻");
		BB_LOG(L"test::unicode::utf16", (const bb_wchar_t*)u"✨✔");
		BB_LOG_A("test::unicode::utf8", u8"Sofía");
		BB_LOG_A("test::unicode::utf8", u8"(╯°□°）╯︵ ┻━┻");
		BB_LOG_A("test::unicode::utf8", u8"✨✔");
		BB_LOG_A("test::unicode::utf8", u8"™");
		BB_LOG_A("test::unicode::utf8", u8"\u2728\u2714"); // ✨✔
		BB_LOG_A("test::unicode::utf8", u8"\uff09");       // ）
		BB_LOG_A("test::unicode::utf8", u8"\ufe35");       // ︵
		BB_LOG_A("test::unicode::utf8", u8"\u2728");       // ✨
		BB_LOG_A("test::unicode::utf8", u8"\u2122");       // ™

		while (BB_IS_CONNECTED() && s_NumContinues < 1)
		{
			bb_sleep_ms(100);
			BB_START_FRAME_NUMBER(++frameNumber);
			BB_TICK();
		}

		if (!BB_IS_CONNECTED())
		{
			bb_sleep_ms(1000);
			bb_connect(127 << 24 | 1, 0);
		}
	}

	start = bb_current_time_ms();
	while (BB_IS_CONNECTED() && s_NumContinues < 2)
	{
		bb_sleep_ms(160);
		BB_START_FRAME_NUMBER(++frameNumber);
		BB_TICK();

		BB_LOG(L"test::category", L"new frame");

		BB_LOG_DYNAMIC(s_pathNames[pathIndex++], 1001, s_categoryNames[categoryIndex++], L"This is a %s test!\n", L"**DYNAMIC**");
		if (pathIndex >= BB_ARRAYSIZE(s_pathNames))
		{
			pathIndex = 0;
		}
		if (categoryIndex >= BB_ARRAYSIZE(s_categoryNames))
		{
			categoryIndex = 0;
		}
	}

	s_bQuit = true;
	while (BB_IS_CONNECTED() && !s_bQuit)
	{
		BB_TICK();
		bb_sleep_ms(50);
	}

	s_bQuit = false;
	printf("Here we go...\n");
	bb_disconnect();
	bb_sleep_ms(1000);
	bb_connect_str("127.0.0.1", 0);
	bb_thread_handle_t after_connect_thread_handle = bbthread_create(after_connect_thread_proc, nullptr);

	while (BB_IS_CONNECTED() && !s_bQuit && s_NumContinues < 3)
	{
		BB_START_FRAME_NUMBER(++frameNumber);
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
		// if(bb_current_time_ms() - start > 1000) {
		//	break;
		// }
	}

	s_bQuit = true;
	bbthread_join(before_connect_thread_handle);
	bbthread_join(also_before_connect_thread_handle);
	bbthread_join(after_connect_thread_handle);
	BB_SHUTDOWN();
	return 0;
}
