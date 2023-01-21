// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "appdata.h"
#include "bb.h"
#include "bb_array.h"
#include "bb_malloc.h"
#include "cmdline.h"
#include "crt_leak_check.h"
#include "mc_updater/mc_updater.h"
#include "path_utils.h"
#include "sb.h"

#include "bb_wrap_windows.h"

int main(int argc, const char** argv)
{
	crt_leak_check_init();
	cmdline_init(argc, argv);

	mc_updater_globals globals = { BB_EMPTY_INITIALIZER };
	globals.appName = "bb.exe";
	globals.appdataName = "bb";
	globals.currentVersionJsonFilename = "bb_current_version.json";
	globals.p4VersionDir = "..\\..";
	globals.manifestFilename = "bb_build_manifest.json";

	if (cmdline_find("-nolog") <= 0)
	{
		sb_t logPath = appdata_get(globals.appdataName);
		sb_append(&logPath, "/bbupdater");
#if BB_USING(BB_PLATFORM_WINDOWS)
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		u64 timeVal = li.QuadPart;
		sb_va(&logPath, "/{%llu}bbupdater.bbox", timeVal);
#else
		sb_append(&logPath, "/bbupdater.bbox");
#endif
		path_mkdir(path_get_dir(sb_get(&logPath)));
		path_resolve_inplace(&logPath);
		bb_init_file(sb_get(&logPath));
		sb_reset(&logPath);
	}

	bb_set_send_callback(&bb_echo_to_stdout, NULL);
	BB_INIT_WITH_FLAGS("mc_updater", cmdline_find("-bb") > 0 ? kBBInitFlag_None : kBBInitFlag_NoDiscovery);
	BB_THREAD_START("main");
	BB_TRACE(kBBLogLevel_Verbose, "Startup", "Command line: %s", cmdline_get_full());

	bba_push(globals.contentsFilenames, "bb.exe");
	bba_push(globals.contentsFilenames, "bb.pdb");
	bba_push(globals.contentsFilenames, "bboxtolog.exe");
	bba_push(globals.contentsFilenames, "bboxtolog.pdb");
	bba_push(globals.contentsFilenames, "bbupdater.exe");
	bba_push(globals.contentsFilenames, "bbupdater.pdb");
	bba_push(globals.contentsFilenames, "bb_site_config.json");
	bba_push(globals.contentsFilenames, "freetype.dll");

	b32 success = mc_updater_main(&globals);

	bb_free((void*)globals.contentsFilenames.data);

	cmdline_shutdown();
	BB_SHUTDOWN();
	return success ? 0 : 1;
}
