// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bb_time.h"
#include "cmdline.h"
#include "crt_leak_check.h"
#include "env_utils.h"
#include "mc_build/mc_build_commands.h"
#include "mc_build/mc_build_dependencies.h"
#include "mc_build/mc_build_structs_generated.h"
#include "mc_build/mc_build_utils.h"
#include "process_task.h"
#include "str.h"
#include "va.h"
#include <stdio.h>

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

	b32 bb = (cmdline_find("-bb") > 0);

	bb_set_send_callback(&bb_echo_to_stdout, NULL);
	u32 bbFlags = bb ? 0 : kBBInitFlag_NoOpenView | kBBInitFlag_NoDiscovery;
	BB_INIT_WITH_FLAGS("mc_build", bbFlags);
	BB_THREAD_START("main");
	//BB_TRACE(kBBLogLevel_Verbose, "Startup", "Command line: %s", cmdline_get_full());

	u64 start = bb_current_time_ms();

	buildDepRebuild rebuild = (cmdline_find("-rebuild") > 0) ? kBuildDep_Rebuild : kBuildDep_NoRebuild;
	b32 bStopOnErrors = !(cmdline_find("-nostop") > 0);
	buildDepDebug debug = (cmdline_find("-debug") > 0) ? kBuildDep_Debug : kBuildDep_NoDebug;
	if(cmdline_find("-showreasons") > 0) {
		debug = kBuildDep_Reasons;
	}
	b32 bShowCommands = (cmdline_find("-showcommands") > 0);
	u32 concurrency = 16;
	const char *concurrencyStr = cmdline_find_prefix("-j=");
	if(concurrencyStr) {
		concurrency = strtou32(concurrencyStr);
		if(concurrency == 0) {
			concurrency = 1;
			BB_WARNING("startup", "Invalid concurrency '%s' - clamping to 1", concurrencyStr);
		}
	}

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

	buildDependencyTable_insertDir(&deps, &times, &bbclient_c, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/src", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/uuid_rfc4122", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/md5_rfc1321", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/src/mc_callstack", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/submodules/parson", objDir, kBuildDep_NoRecurse, kBuildDep_NoSourceFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertFile(&deps, &times, &mc_common_c, "../submodules/mc_imgui/submodules/mc_common/submodules/parson/parson.c", objDir, kBuildDep_NoDebug);

	buildDependencyTable_insertDir(NULL, &times, NULL, "../obj/linux", NULL, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(NULL, &times, NULL, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include", NULL, kBuildDep_Recurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(NULL, &times, NULL, "../submodules/mc_imgui/submodules/mc_common/include", NULL, kBuildDep_Recurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	buildDependencyTable_insertDir(&deps, &times, &bb_example_cpp, "../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/examples", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &bboxtolog_c, "../src/bboxtolog", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	sb_t bbclient_example_wchar = sb_from_va("%s/bbclient_example_wchar", binDir);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example_wchar), &bbclient_c);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example_wchar), &bb_example_cpp);

	sb_t bboxtolog = sb_from_va("%s/bboxtolog", binDir);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bboxtolog), &bbclient_c);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bboxtolog), &bboxtolog_c);

	if(debug == kBuildDep_Debug) {
		buildDependencyTable_dump(&deps);
		sourceTimestampTable_dump(&times);
		buildSources_dump(&bbclient_c, "bbclient_c");
		buildSources_dump(&mc_common_c, "mc_common_c");
		buildSources_dump(&bb_example_cpp, "bb_example_cpp");
		buildSources_dump(&bboxtolog_c, "bboxtolog_c");
	}

	buildCommands_t commands = { BB_EMPTY_INITIALIZER };

	sb_t wslPath = env_resolve("\"%windir%\\System32\\wsl.exe\"");
	//sb_t wslPath = env_resolve("\"%comspec%\" /C wsl");

	u32 bbclientCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bbclient_c, objDir, debug, rebuild, ".", va("%s clang -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 mcCommonCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &mc_common_c, objDir, debug, rebuild, ".", va("%s clang -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../submodules/mc_imgui/submodules/mc_common/include -I../submodules/mc_imgui/submodules/mc_common/submodules {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 bbExampleCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bb_example_cpp, objDir, debug, rebuild, ".", va("%s clang++ -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	u32 bboxtologCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bboxtolog_c, objDir, debug, rebuild, ".", va("%s clang -DBB_WIDECHAR=0 -MMD -c -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&wslPath)));
	BB_UNUSED(mcCommonCommands);

	buildCommandsState_t dispatchState = buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
	buildCommands_reset(&commands);

	if(dispatchState.errorCount == 0) {
		b32 bUpToDate = buildDependencyTable_checkDeps(&deps, &times, sb_get(&bbclient_example_wchar), debug);
		if(bbclientCommands || bbExampleCommands || !bUpToDate) {
			sb_t cmd = sb_from_va("%s clang -MMD -g -o %s", sb_get(&wslPath), sb_get(&bbclient_example_wchar));
			buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
			buildUtils_appendObjects(objDir, &bb_example_cpp, &cmd);
			buildCommands_push(&commands, "Linking bbclient_example_wchar", ".", sb_get(&cmd));
			sb_reset(&cmd);
		}

		bUpToDate = buildDependencyTable_checkDeps(&deps, &times, sb_get(&bboxtolog), debug);
		if(bbclientCommands || bboxtologCommands || !bUpToDate) {
			sb_t cmd = sb_from_va("%s clang -MMD -g -o %s", sb_get(&wslPath), sb_get(&bboxtolog));
			buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
			buildUtils_appendObjects(objDir, &bboxtolog_c, &cmd);
			sb_append(&cmd, " -lpthread");
			buildCommands_push(&commands, "Linking bboxtolog", ".", sb_get(&cmd));
			sb_reset(&cmd);
		}

		buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
		buildCommands_reset(&commands);
	}

	sbs_reset(&bbclient_c);
	sbs_reset(&mc_common_c);
	sbs_reset(&bb_example_cpp);
	sbs_reset(&bboxtolog_c);
	buildDependencyTable_reset(&deps);
	sourceTimestampTable_reset(&times);
	sb_reset(&wslPath);

	sb_reset(&bbclient_example_wchar);
	sb_reset(&bboxtolog);

	u64 end = bb_current_time_ms();
	BB_LOG("Timings", "Finished in %f seconds.", (end - start) * 0.001);

	tasks_shutdown();
	cmdline_shutdown();
	BB_FLUSH();
	BB_SHUTDOWN();
	return ret;
}
