// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

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

static const char *s_categoryNames[] = {
	"dynamic::category::thing",
	"dynamic::monkey::banana",
};

static const char *s_pathNames[] = {
	"C:\\Windows\\System32\\user32.dll",
	"C:\\Windows\\Fonts\\Consola.ttf",
	"D:\\bin\\wsl-terminal\\vim.exe",
};

static b32 s_bQuit = false;

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

int main(int argc, const char **argv)
{
	uint64_t start = bb_current_time_ms();
	uint32_t categoryIndex = 0;
	uint32_t pathIndex = 0;
	(void)argc;
	(void)argv;

	//bb_init_file_w("bbclient.bbox");
	//BB_INIT(L"bbclient: matt");
	BB_INIT_WITH_FLAGS("bbclient: matt", kBBInitFlag_ConsoleCommands | kBBInitFlag_DebugInit);
	//BB_INIT_WITH_FLAGS("bbclient: matt (no view)", kBBInitFlag_NoOpenView | kBBInitFlag_DebugInit);
	BB_THREAD_START("main thread!");
	BB_LOG("startup", "bbclient init took %llu ms", bb_current_time_ms() - start);

	BB_SET_INCOMING_PACKET_HANDLER(&incoming_packet_handler, NULL);

	BB_LOG("test::LongLine::Wide::Formatted", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, 0, "test::LongLine::Wide::Preformatted", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	BB_LOG_A("test::LongLine::Utf8::Formatted", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
	bb_trace_dynamic_preformatted(__FILE__, (u32)__LINE__, "test::LongLine::Utf8::Preformatted", kBBLogLevel_Log, 0, "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");

	BB_LOG("test::category::deep", "this is a __%s__ test at time %zu", L"simple", bb_current_time_ms());
	BB_WARNING("test::other", "monkey");
	BB_LOG("test::a", "fred");
	BB_LOG("testa::bob", "george");
	BB_ERROR("testa", "chuck");
	BB_LOG("standalone::nested::category", "standalone::nested::category");
	BB_LOG("test::unicode::wchar_t", "Sofía");
	BB_LOG("test::unicode::wchar_t", "(╯°□°）╯︵ ┻━┻");
	BB_LOG("test::unicode::wchar_t", "✨✔");
	//BB_LOG("test::unicode::utf16", u"Sofía");
	//BB_LOG("test::unicode::utf16", u"(╯°□°）╯︵ ┻━┻");
	//BB_LOG("test::unicode::utf16", u"✨✔");
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

		BB_LOG("test::category", "new frame");

		BB_LOG_DYNAMIC(s_pathNames[pathIndex++], 1001, s_categoryNames[categoryIndex++], "This is a %s test!\n", "**DYNAMIC**");
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
	bb_connect(0, 0);

	while(BB_IS_CONNECTED() && !s_bQuit) {
		BB_TICK();
		BB_LOG("test::frame", "-----------------------------------------------------");
		BB_LOG("test::spam1", "some data");
		BB_LOG("test::spam1", "or other");
		BB_LOG("test::spam1", "spamming");
		BB_LOG("test::spam1", "many times");
		BB_LOG("test::spam1", "in the");
		BB_LOG("test::spam1", "same frame");
		BB_LOG("test::spam1", "over and");
		BB_LOG("test::spam1", "over.");
		BB_LOG("test::spam2", "some data");
		BB_LOG("test::spam2", "or other");
		BB_LOG("test::spam2", "spamming");
		BB_LOG("test::spam2", "many times");
		BB_LOG("test::spam2", "in the");
		BB_LOG("test::spam2", "same frame");
		BB_LOG("test::spam2", "over and");
		BB_LOG("test::spam2", "over.");
		BB_LOG("test::spam3", "some data");
		BB_LOG("test::spam3", "or other");
		BB_LOG("test::spam3", "spamming");
		BB_LOG("test::spam3", "many times");
		BB_LOG("test::spam3", "in the");
		BB_LOG("test::spam3", "same frame");
		BB_LOG("test::spam3", "over and");
		BB_LOG("test::spam3", "over.");
		bb_sleep_ms(50);
		//if(bb_current_time_ms() - start > 1000) {
		//	break;
		//}
	}

	BB_SHUTDOWN();
	return 0;
}
