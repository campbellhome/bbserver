// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bb_array.h"
#include "bb_packet.h"
#include "bb_time.h"
#include "cmdline.h"
#include "crt_leak_check.h"
#include "env_utils.h"
#include "mc_build/mc_build_commands.h"
#include "mc_build/mc_build_dependencies.h"
#include "mc_build/mc_build_structs_generated.h"
#include "mc_build/mc_build_utils.h"
#include "process_task.h"
#include "va.h"
#include <stdio.h>

static void RedirectBlackboxToStdout(void *context, bb_decoded_packet_t *decoded)
{
	BB_UNUSED(context);
	if(decoded->type == kBBPacketType_LogText) {
		switch(decoded->packet.logText.level) {
		case kBBLogLevel_Warning:
		case kBBLogLevel_Error:
		case kBBLogLevel_Fatal:
			fputs(decoded->packet.logText.text, stderr);
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;

		case kBBLogLevel_Log:
		case kBBLogLevel_Display:
			fputs(decoded->packet.logText.text, stdout);
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;

		default:
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;
		}
	}
}

int main(int argc, const char **argv)
{
	int ret = 1;
	crt_leak_check_init();
	//bba_set_logging(true, true);
	cmdline_init(argc, argv);

#if BB_USING(BB_PLATFORM_WINDOWS)
	if(cmdline_find("-debugger") > 0) {
		while(!IsDebuggerPresent()) {
			Sleep(1000);
		}
	}
#endif

	bb_set_send_callback(&RedirectBlackboxToStdout, NULL);
	u32 bbFlags = kBBInitFlag_NoOpenView | kBBInitFlag_NoDiscovery;
	bb_init_file("mc_build.bbox");
	BB_INIT_WITH_FLAGS("mc_build", bbFlags);
	BB_THREAD_START("main");
	//BB_TRACE(kBBLogLevel_Verbose, "Startup", "Command line: %s", cmdline_get_full());

	u64 start = bb_current_time_ms();

	b32 bRebuild = false;
	b32 bStopOnErrors = true;
	b32 bDebugDeps = false;
	b32 bShowCommands = false;
	u32 concurrency = 16;

	tasks_startup();
	process_init();

	buildDependencyTable deps = buildDependencyTable_init(2048);
	sourceTimestampTable times = sourceTimestampTable_init(2048);

	const char *objDir = "../obj/linux";
	const char *binDir = "../bin/linux";

	sbs_t bbclient_c = { BB_EMPTY_INITIALIZER };
	sbs_t mc_common_c = { BB_EMPTY_INITIALIZER };
	sbs_t bb_example_cpp = { BB_EMPTY_INITIALIZER };
	sbs_t bboxtolog_c = { BB_EMPTY_INITIALIZER };

	buildDependencyTable_insertFromDir(&deps, &times, &bbclient_c, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/src", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/submodules/parson", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/uuid_rfc4122", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/md5_rfc1321", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/mc_callstack", objDir, false);
	buildDependencyTable_insertFromDir(NULL, &times, NULL, "../obj/linux", NULL, false);
	buildDependencyTable_insertFromDir(NULL, &times, NULL, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include", NULL, true);
	buildDependencyTable_insertFromDir(NULL, &times, NULL, "../submodules/mc_imgui/submodules/mc_common/include", NULL, true);

	buildDependencyTable_insertFromDir(&deps, &times, &bb_example_cpp, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/examples", objDir, false);
	buildDependencyTable_insertFromDir(&deps, &times, &bboxtolog_c, "../src/bboxtolog", objDir, false);

	//buildDependencyTable_dump(&deps);
	//sourceTimestampTable_dump(&times);

	buildCommands_t commands = { BB_EMPTY_INITIALIZER };

	sb_t wslPath = env_resolve("\"%windir%\\System32\\wsl.exe\"");
	//sb_t wslPath = env_resolve("\"%comspec%\" /C wsl");

	u32 bbclientCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bbclient_c, objDir, bDebugDeps, bRebuild, ".", va("%s clang -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 mcCommonCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &mc_common_c, objDir, bDebugDeps, bRebuild, ".", va("%s clang -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../submodules/mc_imgui/submodules/mc_common/include -I../submodules/mc_imgui/submodules/mc_common/submodules {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 bbExampleCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bb_example_cpp, objDir, bDebugDeps, bRebuild, ".", va("%s clang++ -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 bboxtologCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bboxtolog_c, objDir, bDebugDeps, bRebuild, ".", va("%s clang -DBB_WIDECHAR=0 -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));

	buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
	buildCommands_reset(&commands);

	if(bbclientCommands || bbExampleCommands) {
		sb_t cmd = sb_from_va("%s clang -MMD -g -o ../bin/linux/bbclient_example_wchar", sb_get(&wslPath));
		buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
		buildUtils_appendObjects(objDir, &bb_example_cpp, &cmd);
		buildCommands_push(&commands, "Linking bbclient_example_wchar", ".", sb_get(&cmd));
		sb_reset(&cmd);
	}

	if(bbclientCommands || bboxtologCommands) {
		sb_t cmd = sb_from_va("%s clang -MMD -g -o ../bin/linux/bboxtolog", sb_get(&wslPath));
		buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
		buildUtils_appendObjects(objDir, &bboxtolog_c, &cmd);
		buildCommands_push(&commands, "Linking bboxtolog", ".", sb_get(&cmd));
		sb_reset(&cmd);
	}

	buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
	buildCommands_reset(&commands);

	sbs_reset(&bbclient_c);
	sbs_reset(&mc_common_c);
	sbs_reset(&bb_example_cpp);
	sbs_reset(&bboxtolog_c);
	buildDependencyTable_reset(&deps);
	sourceTimestampTable_reset(&times);
	sb_reset(&wslPath);

	u64 end = bb_current_time_ms();
	BB_LOG("Timings", "Finished in %f seconds.", (end - start) * 0.001);

	tasks_shutdown();
	cmdline_shutdown();
	BB_FLUSH();
	BB_SHUTDOWN();
	return ret;
}
