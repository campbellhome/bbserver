// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#include "bb_time.h"
#include "cmdline.h"
#include "crt_leak_check.h"
#include "env_utils.h"
#include "file_utils.h"
#include "mc_build/mc_build_commands.h"
#include "mc_build/mc_build_dependencies.h"
#include "mc_build/mc_build_structs_generated.h"
#include "mc_build/mc_build_utils.h"
#include "path_utils.h"
#include "process_task.h"
#include "str.h"
#include "va.h"
#include <bb_wrap_windows.h>
#include <stdio.h>

typedef enum CrossCompiler_e
{
	kCrossCompiler_wsl,
	kCrossCompiler_zig,
} CrossCompiler_t;

int main(int argc, const char** argv)
{
	int ret = 0;
	crt_leak_check_init();
	// bba_set_logging(true, true);
	cmdline_init(argc, argv);

#if BB_USING(BB_PLATFORM_WINDOWS)
	if (cmdline_find("-debugger") > 0)
	{
		while (!IsDebuggerPresent())
		{
			Sleep(1000);
		}
	}
#endif

	const char* exeDir = cmdline_get_exe_dir();
	sb_t rootDir = sb_from_va("%s\\..\\..", exeDir);
	path_resolve_inplace(&rootDir);
#if BB_USING(BB_PLATFORM_WINDOWS)
	if (!SetCurrentDirectoryA(sb_get(&rootDir)))
	{
		++ret;
	}
#endif

	b32 bb = (cmdline_find("-bb") > 0);

	bb_set_send_callback(&bb_echo_to_stdout, NULL);
	u32 bbFlags = bb ? 0 : kBBInitFlag_NoOpenView | kBBInitFlag_NoDiscovery;
	BB_INIT_WITH_FLAGS("mc_build", bbFlags);
	BB_THREAD_START("main");
	// BB_TRACE(kBBLogLevel_Verbose, "Startup", "Command line: %s", cmdline_get_full());

	u64 start = bb_current_time_ms();

	if (!file_readable("vs/bb_build_linux.sln"))
	{
		++ret;
		BB_WARNING("startup", "bb_build_linux needs to be run from a blackbox bin dir");
	}

	buildDepRebuild rebuild = (cmdline_find("-rebuild") > 0) ? kBuildDep_Rebuild : kBuildDep_NoRebuild;
	b32 bStopOnErrors = !(cmdline_find("-nostop") > 0);
	buildDepDebug debug = (cmdline_find("-debug") > 0) ? kBuildDep_Debug : kBuildDep_NoDebug;
	if (cmdline_find("-showreasons") > 0)
	{
		debug = kBuildDep_Reasons;
	}
	b32 bShowCommands = (cmdline_find("-showcommands") > 0);
	u32 concurrency = 16;
	const char* concurrencyStr = cmdline_find_prefix("-j=");
	if (concurrencyStr)
	{
		concurrency = strtou32(concurrencyStr);
		if (concurrency == 0)
		{
			concurrency = 1;
			BB_WARNING("startup", "Invalid concurrency '%s' - clamping to 1", concurrencyStr);
		}
	}

	b32 bSqlite = true;
	if (cmdline_find("-nosql") > 0 || cmdline_find("-nosqlite") > 0)
	{
		bSqlite = false;
	}

	CrossCompiler_t crossCompiler = kCrossCompiler_wsl;
	if (cmdline_find("-wsl") > 0)
	{
		crossCompiler = kCrossCompiler_wsl;
	}
	else if (cmdline_find("-zig") > 0 || cmdline_find("-zigcc") > 0)
	{
		crossCompiler = kCrossCompiler_zig;
	}

	tasks_startup();
	process_init();

	buildDependencyTable deps = buildDependencyTable_init(2048);
	sourceTimestampTable times = sourceTimestampTable_init(2048);

	const char* objDir = NULL;
	const char* binDir = NULL;
	switch (crossCompiler)
	{
	case kCrossCompiler_wsl:
		objDir = "obj/wsl/linux";
		binDir = "bin/wsl/linux";
		break;
	case kCrossCompiler_zig:
		objDir = "obj/zig/linux";
		binDir = "bin/zig/linux";
		break;
	}

	path_mkdir(objDir);
	path_mkdir(binDir);

	sbs_t bbclient_c = { BB_EMPTY_INITIALIZER };
	sbs_t mc_common_c = { BB_EMPTY_INITIALIZER };
	sbs_t thirdparty_c = { BB_EMPTY_INITIALIZER };
	sbs_t bb_example_c = { BB_EMPTY_INITIALIZER };
	sbs_t bb_example_cpp = { BB_EMPTY_INITIALIZER };
	sbs_t bboxtolog_c = { BB_EMPTY_INITIALIZER };

	buildDependencyTable_insertDir(&deps, &times, &bbclient_c, "bbclient/src", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "mc_common/src", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "mc_common/src/uuid_rfc4122", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "mc_common/src/md5_rfc1321", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &mc_common_c, "mc_common/src/mc_callstack", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	buildDependencyTable_insertDir(&deps, &times, &thirdparty_c, "thirdparty/parson", objDir, kBuildDep_NoRecurse, kBuildDep_NoSourceFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertFile(&deps, &times, &thirdparty_c, "thirdparty/parson/parson.c", objDir, kBuildDep_NoDebug);

	if (bSqlite)
	{
		buildDependencyTable_insertDir(&deps, &times, &thirdparty_c, "thirdparty/sqlite", objDir, kBuildDep_NoRecurse, kBuildDep_NoSourceFiles, kBuildDep_NoDebug);
		buildDependencyTable_insertFile(&deps, &times, &thirdparty_c, "thirdparty/sqlite/sqlite3.c", objDir, kBuildDep_NoDebug);
	}

	buildDependencyTable_insertDir(NULL, &times, NULL, "obj/linux", NULL, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(NULL, &times, NULL, "bbclient/include", NULL, kBuildDep_Recurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(NULL, &times, NULL, "mc_common/include", NULL, kBuildDep_Recurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	buildDependencyTable_insertDir(&deps, &times, &bb_example_c, "bbclient/examples", objDir, kBuildDep_NoRecurse, kBuildDep_CSourceFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &bb_example_cpp, "bbclient/examples", objDir, kBuildDep_NoRecurse, kBuildDep_CppSourceFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &bboxtolog_c, "src", objDir, kBuildDep_NoRecurse, kBuildDep_NoSourceFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &bboxtolog_c, "src/bboxtolog", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);
	buildDependencyTable_insertDir(&deps, &times, &bboxtolog_c, "src/view_filter", objDir, kBuildDep_NoRecurse, kBuildDep_AllFiles, kBuildDep_NoDebug);

	sb_t bbclient_example = sb_from_va("%s/bbclient_example", binDir);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example), &bbclient_c);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example), &bb_example_c);

	sb_t bbclient_example_wchar = sb_from_va("%s/bbclient_example_wchar", binDir);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example_wchar), &bbclient_c);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bbclient_example_wchar), &bb_example_cpp);

	sb_t bboxtolog = sb_from_va("%s/bboxtolog", binDir);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bboxtolog), &bbclient_c);
	buildDependencyTable_addDeps(&deps, &times, sb_get(&bboxtolog), &bboxtolog_c);

	if (debug == kBuildDep_Debug)
	{
		buildDependencyTable_dump(&deps);
		sourceTimestampTable_dump(&times);
		buildSources_dump(&bbclient_c, "bbclient_c");
		buildSources_dump(&mc_common_c, "mc_common_c");
		buildSources_dump(&thirdparty_c, "thirdparty_c");
		buildSources_dump(&bb_example_c, "bb_example_c");
		buildSources_dump(&bb_example_cpp, "bb_example_cpp");
		buildSources_dump(&bboxtolog_c, "bboxtolog_c");
	}

	buildCommands_t commands = { BB_EMPTY_INITIALIZER };

	sb_t cCompiler = { BB_EMPTY_INITIALIZER };
	sb_t cppCompiler = { BB_EMPTY_INITIALIZER };
	switch (crossCompiler)
	{
	case kCrossCompiler_wsl:
	{
		sb_t wslPath = env_resolve("\"%windir%\\System32\\wsl.exe\"");
		cCompiler = sb_from_va("%s clang", sb_get(&wslPath));
		cppCompiler = sb_from_va("%s clang++", sb_get(&wslPath));
		sb_reset(&wslPath);
		break;
	}
	case kCrossCompiler_zig:
	{
		const char* target = "x86_64-linux-gnu.2.28";
		cCompiler = sb_from_va("zig.exe cc -target %s", target);
		cppCompiler = sb_from_va("zig.exe c++ -target %s", target);
		break;
	}
	}

	u32 bbclientCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bbclient_c, objDir, debug, rebuild, ".", va("%s -MMD -c -g -Werror -Wall -Wextra -Ibbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cCompiler)));
	u32 mcCommonCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &mc_common_c, objDir, debug, rebuild, ".", va("%s -MMD -c -g -Werror -Wall -Wextra -Ibbclient/include -Ibbclient/include/bbclient -Imc_common/include -Ithirdparty {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cCompiler)));
	u32 thirdpartyCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &thirdparty_c, objDir, debug, rebuild, ".", va("%s -MMD -c -g -Werror -Ithirdparty {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cCompiler)));
	u32 bbExampleCCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bb_example_c, objDir, debug, rebuild, ".", va("%s -MMD -c -g -Werror -Wall -Wextra -Ibbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cCompiler)));
	u32 bbExampleCPPCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bb_example_cpp, objDir, debug, rebuild, ".", va("%s -MMD -c -g -Werror -Wall -Wextra -Ibbclient/include {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cppCompiler)));
	u32 bboxtologCommands = buildDependencyTable_queueCommands(&commands, &deps, &times, &bboxtolog_c, objDir, debug, rebuild, ".", va("%s -DBB_STANDALONE -MMD -c -g -Werror -Wall -Wextra -Ibbclient/include -Ibbclient/include/bbclient -Imc_common/include -Ithirdparty -Isrc -Isrc/view_filter {SOURCE_PATH} -o {OBJECT_PATH}", sb_get(&cCompiler)));

	buildCommandsState_t dispatchState = buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
	ret += dispatchState.errorCount;
	buildCommands_reset(&commands);

	if (dispatchState.errorCount == 0)
	{
		b32 bUpToDate = true;

		bUpToDate = buildDependencyTable_checkDeps(&deps, &times, sb_get(&bbclient_example), debug);
		if (bbclientCommands || mcCommonCommands || thirdpartyCommands || bbExampleCCommands || !bUpToDate)
		{
			sb_t cmd = sb_from_va("%s -MMD -g -o %s", sb_get(&cCompiler), sb_get(&bbclient_example));
			buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
			buildUtils_appendObjects(objDir, &mc_common_c, &cmd);
			buildUtils_appendObjects(objDir, &thirdparty_c, &cmd);
			buildUtils_appendObjects(objDir, &bb_example_c, &cmd);
			sb_append(&cmd, " -lpthread -ldl");
			buildCommands_push(&commands, "Linking bbclient_example", ".", sb_get(&cmd));
			sb_reset(&cmd);
		}

		bUpToDate = buildDependencyTable_checkDeps(&deps, &times, sb_get(&bbclient_example_wchar), debug);
		if (bbclientCommands || mcCommonCommands || thirdpartyCommands || bbExampleCPPCommands || !bUpToDate)
		{
			sb_t cmd = sb_from_va("%s -MMD -g -o %s", sb_get(&cppCompiler), sb_get(&bbclient_example_wchar));
			buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
			buildUtils_appendObjects(objDir, &mc_common_c, &cmd);
			buildUtils_appendObjects(objDir, &thirdparty_c, &cmd);
			buildUtils_appendObjects(objDir, &bb_example_cpp, &cmd);
			sb_append(&cmd, " -lpthread -ldl");
			buildCommands_push(&commands, "Linking bbclient_example_wchar", ".", sb_get(&cmd));
			sb_reset(&cmd);
		}

		bUpToDate = buildDependencyTable_checkDeps(&deps, &times, sb_get(&bboxtolog), debug);
		if (bbclientCommands || mcCommonCommands || thirdpartyCommands || bboxtologCommands || !bUpToDate)
		{
			sb_t cmd = sb_from_va("%s -MMD -g -o %s", sb_get(&cCompiler), sb_get(&bboxtolog));
			buildUtils_appendObjects(objDir, &bbclient_c, &cmd);
			buildUtils_appendObjects(objDir, &mc_common_c, &cmd);
			buildUtils_appendObjects(objDir, &thirdparty_c, &cmd);
			buildUtils_appendObjects(objDir, &bboxtolog_c, &cmd);
			sb_append(&cmd, " -lpthread -ldl");
			buildCommands_push(&commands, "Linking bboxtolog", ".", sb_get(&cmd));
			sb_reset(&cmd);
		}

		dispatchState = buildCommands_dispatch(&commands, concurrency, bStopOnErrors, bShowCommands);
		ret += dispatchState.errorCount;
		buildCommands_reset(&commands);
	}

	sb_reset(&cCompiler);
	sb_reset(&cppCompiler);

	sbs_reset(&bbclient_c);
	sbs_reset(&mc_common_c);
	sbs_reset(&thirdparty_c);
	sbs_reset(&bb_example_c);
	sbs_reset(&bb_example_cpp);
	sbs_reset(&bboxtolog_c);
	buildDependencyTable_reset(&deps);
	sourceTimestampTable_reset(&times);

	sb_reset(&bbclient_example);
	sb_reset(&bbclient_example_wchar);
	sb_reset(&bboxtolog);

	sb_reset(&rootDir);

	u64 end = bb_current_time_ms();
	BB_LOG("Timings", "Finished in %f seconds.", (end - start) * 0.001);

	tasks_shutdown();
	cmdline_shutdown();
	BB_FLUSH();
	BB_SHUTDOWN();
	return ret;
}
