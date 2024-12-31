// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "mc_updater/mc_updater.h"
#include "appdata.h"
#include "bb.h"
#include "bb_array.h"
#include "bb_defines.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_time.h"
#include "cmdline.h"
#include "file_utils.h"
#include "path_utils.h"
#include "system_error_utils.h"
#include "update_utils.h"
#include "va.h"

#if BB_USING(BB_PLATFORM_WINDOWS)
#include "bb_thread.h"
#include "process_utils.h"
#include "py_parser.h"
#include "sdict.h"
#endif

#include <stdarg.h>

// thirdparty

#include "bb_wrap_dirent.h"
#include "parson/parson.h"

BB_WARNING_PUSH(4061 4100 4127 4232 4242 4244 4255 4468 4548 4668 4820 5045)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#pragma-messages"
#include "miniz/miniz.c"
#pragma clang diagnostic pop
#else
#include "miniz/miniz.c"
#endif
BB_WARNING_POP
#if defined(_MSC_VER)
#include <Psapi.h>
#endif

static mc_updater_globals s_globals;

int mc_updater_do_update(const char* version, const char* sourcePath, const char* targetDir)
{
	if (!path_mkdir(targetDir))
	{
		BB_ERROR("Extract", "Failed to create dir %s", targetDir);
		return false;
	}

	mz_zip_archive sourceZipArchive = { BB_EMPTY_INITIALIZER };
	mz_bool status = mz_zip_reader_init_file(&sourceZipArchive, sourcePath, 0);
	if (!status)
	{
		BB_ERROR("Read", "Failed to read %s", sourcePath);
		return false;
	}

	// clean up old files
	sbs_t filesToRemove = { BB_EMPTY_INITIALIZER };
	DIR* d = opendir(targetDir);
	if (d)
	{
		struct dirent* entry;
		while ((entry = readdir(d)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				sb_t* val = bba_add(filesToRemove, 1);
				if (val)
				{
					sb_append(val, entry->d_name);
				}
			}
		}
		closedir(d);
	}
	for (u32 i = 0; i < filesToRemove.count; ++i)
	{
		sb_t path = { BB_EMPTY_INITIALIZER };
		sb_va(&path, "%s/%s", targetDir, sb_get(filesToRemove.data + i));
		path_resolve_inplace(&path);
		const char* pathToDelete = sb_get(&path);
		if (file_readable(pathToDelete))
		{
			if (file_delete(pathToDelete))
			{
				BB_LOG("Cleanup", "deleted %s", pathToDelete);
			}
			else
			{
				BB_ERROR("Cleanup", "Failed to delete %s", pathToDelete);
			}
		}
		else
		{
			BB_TRACE(kBBLogLevel_Verbose, "Cleanup", "Skipping delete of %s", pathToDelete);
		}
		sb_reset(&path);
	}
	sbs_reset(&filesToRemove);

	// extract new files
	b32 success = true;
	mz_uint numFiles = mz_zip_reader_get_num_files(&sourceZipArchive);
	for (mz_uint i = 0; i < numFiles; ++i)
	{
		mz_zip_archive_file_stat fileStat;
		if (mz_zip_reader_file_stat(&sourceZipArchive, i, &fileStat))
		{
			mz_bool bDirectory = mz_zip_reader_is_file_a_directory(&sourceZipArchive, i);
			if (!bDirectory)
			{
				sb_t path = { BB_EMPTY_INITIALIZER };
				sb_va(&path, "%s/%s", targetDir, fileStat.m_filename);
				path_resolve_inplace(&path);
				sb_t tmpPath = { BB_EMPTY_INITIALIZER };
				sb_va(&tmpPath, "%s.tmp", sb_get(&path));
				const char* pathToExtract = sb_get(&path);
				size_t uncompressedSize = 0;
				void* uncompressedData = mz_zip_reader_extract_file_to_heap(&sourceZipArchive, fileStat.m_filename, &uncompressedSize, 0);
				if (uncompressedData && uncompressedSize == fileStat.m_uncomp_size)
				{
					fileData_t fileData = { BB_EMPTY_INITIALIZER };
					fileData.buffer = uncompressedData;
					fileData.bufferSize = (u32)uncompressedSize;
					sb_t dir = { BB_EMPTY_INITIALIZER };
					sb_append(&dir, pathToExtract);
					path_remove_filename(&dir);
					if (dir.data && *dir.data)
					{
						path_mkdir(dir.data); // no need to error check - file write will fail also
					}
					sb_reset(&dir);
					if (fileData.bufferSize != uncompressedSize ||
					    fileData_writeIfChanged(pathToExtract, sb_get(&tmpPath), fileData))
					{
						BB_LOG("Extract", "Extracted %zu bytes as %s", uncompressedSize, pathToExtract);
					}
					else
					{
						success = false;
						BB_ERROR("Extract", "Failed to write %s", pathToExtract);
					}
				}
				else
				{
					success = false;
					BB_ERROR("Extract", "Failed to extract %s", fileStat.m_filename);
				}
				if (uncompressedData)
				{
					mz_free(uncompressedData);
				}
				sb_reset(&tmpPath);
				sb_reset(&path);
			}
		}
	}

	if (success)
	{
		sb_t currentVersionPath = appdata_get(s_globals.appdataName);
		sb_va(&currentVersionPath, "/%s", s_globals.currentVersionJsonFilename);
		path_resolve_inplace(&currentVersionPath);
		sb_t currentVersionContents = { BB_EMPTY_INITIALIZER };
		sb_va(&currentVersionContents, "{\"name\":\"%s\"}", version);
		fileData_t fileData = { BB_EMPTY_INITIALIZER };
		fileData.buffer = currentVersionContents.data;
		fileData.bufferSize = sb_len(&currentVersionContents);
		if (fileData_writeIfChanged(sb_get(&currentVersionPath), NULL, fileData))
		{
			BB_LOG("Version", "Updated %s", sb_get(&currentVersionPath));
		}
		else
		{
			BB_ERROR("Version", "Failed to write %s", sb_get(&currentVersionPath));
		}
		sb_reset(&currentVersionContents);
		sb_reset(&currentVersionPath);
	}

	mz_zip_reader_end(&sourceZipArchive);
	return success;
}

#if BB_USING(BB_PLATFORM_WINDOWS)
static u32 get_p4_version(void)
{
	u32 version = 0;
	process_init();
	const char* cmdline = "p4.exe -G filelog -m 1 ...#have";
	processSpawnResult_t spawnRes = process_spawn(s_globals.p4VersionDir, cmdline, kProcessSpawn_Tracked, kProcessLog_All);
	if (spawnRes.success)
	{
		processTickResult_t tickRes = { BB_EMPTY_INITIALIZER };
		while (!tickRes.done)
		{
			tickRes = process_tick(spawnRes.process);
		}
		sdicts dicts = { BB_EMPTY_INITIALIZER };
		pyParser parser = { BB_EMPTY_INITIALIZER };
		parser.data = spawnRes.process->stdoutBuffer.data;
		parser.count = spawnRes.process->stdoutBuffer.count;
		while (py_parser_tick(&parser, &dicts, false))
		{
			// do nothing
		}

		for (u32 dictIndex = 0; dictIndex < dicts.count; ++dictIndex)
		{
			sdict_t* dict = dicts.data + dictIndex;
			const char* val = sdict_find(dict, "change0");
			u32 nval = (val && *val) ? strtoul(val, NULL, 10) : 0;
			version = (version >= nval) ? version : nval;
		}

		// don't bba_free(parser) because the memory is borrowed from spawnRes.process->stdoutBuffer
		sdicts_reset(&dicts);
		process_free(spawnRes.process);
	}
	process_shutdown();
	return version;
}
#endif

b32 mc_updater_create_build(const char* manifestDir)
{
	b32 success = true;
	updateManifest_t originalManifest = { BB_EMPTY_INITIALIZER };
	updateManifest_t updatedManifest = { BB_EMPTY_INITIALIZER };
	sb_t manifestPath = { BB_EMPTY_INITIALIZER };
	sb_va(&manifestPath, "%s/%s", manifestDir, s_globals.manifestFilename);
	path_resolve_inplace(&manifestPath);
	JSON_Value* val = json_parse_file(sb_get(&manifestPath));
	if (val)
	{
		originalManifest = json_deserialize_updateManifest_t(val);
		json_value_free(val);
	}
	u32 latest = strtoul(sb_get(&originalManifest.latest), NULL, 10);
	u32 newVersion = latest + 1;
#if BB_USING(BB_PLATFORM_WINDOWS)
	if (cmdline_find("-p4") > 0)
	{
		newVersion = get_p4_version();
	}
#endif
	if (latest && newVersion)
	{
		BB_LOG("CreateBuild", "Creating version %u", newVersion);

		mz_zip_archive zip_archive;
		memset(&zip_archive, 0, sizeof(zip_archive)); // mz_zip_archive contains a bunch of pointers. set all to nullptr
		mz_bool status = mz_zip_writer_init_heap(&zip_archive, 0, 0);
		mz_uint levelAndFlags = (mz_uint)MZ_DEFAULT_COMPRESSION;

		for (u32 i = 0; i < s_globals.contentsFilenames.count; ++i)
		{
			const char** filename = s_globals.contentsFilenames.data + i;
			status = mz_zip_writer_add_file(&zip_archive, *filename, *filename, NULL, 0, levelAndFlags) || status;
		}

		status = mz_zip_writer_finalize_archive(&zip_archive) || status;

		fileData_t fileData = { BB_EMPTY_INITIALIZER };
		fileData.buffer = zip_archive.m_pState->m_pMem;
		fileData.bufferSize = (u32)zip_archive.m_pState->m_mem_size;
		if (status)
		{
			path_mkdir(va("%s/%u", manifestDir, newVersion));
			const char* archiveName = va("%s/%u/%s_%u.zip", manifestDir, newVersion, s_globals.appdataName, newVersion);
			if (fileData_writeIfChanged(archiveName, NULL, fileData))
			{
				updatedManifest = updateManifest_build(manifestDir, s_globals.appdataName);
				sb_append(&updatedManifest.stable, sb_get(&originalManifest.stable));
				if (updatedManifest.versions.count == originalManifest.versions.count + 1)
				{
					val = json_serialize_updateManifest_t(&updatedManifest);
					if (val)
					{
						FILE* fp = fopen(sb_get(&manifestPath), "wb");
						if (fp)
						{
							char* serialized_string = json_serialize_to_string_pretty(val);
							fputs(serialized_string, fp);
							fclose(fp);
							json_free_serialized_string(serialized_string);
						}
						else
						{
							BB_ERROR("CreateBuild", "Failed to write %s", sb_get(&manifestPath));
							success = false;
						}
						sb_reset(&manifestPath);
					}
					else
					{
						BB_ERROR("CreateBuild", "Failed to serialize manifest json");
						success = false;
					}
					json_value_free(val);
				}
				else
				{
					BB_ERROR("CreateBuild", "Failed to build manifest");
					success = false;
				}
			}
			else
			{
				BB_ERROR("CreateBuild", "Failed to write %s", archiveName);
				success = false;
			}
		}
		else
		{
			BB_ERROR("CreateBuild", "Failed to build .zip data");
			success = false;
		}

		status = mz_zip_writer_end(&zip_archive);
		if (!status)
			return false;
	}
	else
	{
		BB_ERROR("CreateBuild", "Failed to find latest version in manifest");
		success = false;
	}
	updateManifest_reset(&originalManifest);
	updateManifest_reset(&updatedManifest);
	sb_reset(&manifestPath);
	return success;
}

static void shutdown_pause(b32 success)
{
	if ((success && (cmdline_find("-pauseSuccess") > 0)) ||
	    (!success && (cmdline_find("-pauseFailed") > 0)) ||
	    (cmdline_find("-pause") > 0))
	{
		printf("Press Enter to continue.");
		int val = getc(stdin);
		BB_UNUSED(val);
	}
}

#if BB_USING(BB_PLATFORM_WINDOWS)
typedef struct tag_processIds
{
	u32 count;
	u32 allocated;
	DWORD* data;
} processIds;
static u32 s_numProcessesNeeded = 16 * 1024;
#endif

static b32 mc_updater_app_is_running(void)
{
	b32 ret = false;
#if BB_USING(BB_PLATFORM_WINDOWS)
	processIds ids = { BB_EMPTY_INITIALIZER };
	bba_add(ids, s_numProcessesNeeded);
	if (ids.data)
	{
		DWORD bytesUsed = 0;
		u32 bytesNeeded = ids.count * sizeof(DWORD);
		if (EnumProcesses(ids.data, bytesNeeded, &bytesUsed))
		{
			char moduleName[_MAX_PATH];
			for (u32 i = 0; i < bytesUsed / sizeof(DWORD); ++i)
			{
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ids.data[i]);
				if (hProcess)
				{
					HMODULE hModule;
					DWORD unused;
					if (EnumProcessModules(hProcess, &hModule, sizeof(hModule), &unused))
					{
						if (GetModuleBaseNameA(hProcess, hModule, moduleName, sizeof(moduleName)))
						{
							if (!bb_stricmp(s_globals.appName, moduleName))
							{
								ret = true;
								break;
							}
						}
					}
				}
			}
		}
		bba_free(ids);
	}
#endif
	return ret;
}

b32 mc_updater_main(mc_updater_globals* globals)
{
	b32 success = false;
	s_globals = *globals;

#if BB_USING(BB_PLATFORM_WINDOWS)
	if (cmdline_find("-debugger") > 0)
	{
		while (!IsDebuggerPresent())
		{
			Sleep(1000);
		}
	}
#endif

	BB_TRACE(kBBLogLevel_Verbose, "Startup", "Miniz version: %s", MZ_VERSION);

	if (mc_updater_app_is_running() && cmdline_find("-nowait") < 0)
	{
		BB_LOG("WaitForExit", "Waiting for %s to exit...", s_globals.appName);
		while (mc_updater_app_is_running())
		{
			bb_sleep_ms(100);
		}
		BB_LOG("WaitForExit", "Done.");
	}

	sb_t targetDir = { BB_EMPTY_INITIALIZER };
	const char* version = cmdline_find_prefix("-version=");
	const char* sourcePath = cmdline_find_prefix("-source=");
	sb_append(&targetDir, cmdline_find_prefix("-target="));
	path_resolve_inplace(&targetDir);

	updateManifest_t manifest = { BB_EMPTY_INITIALIZER };
	const char* sourceDir = cmdline_find_prefix("-sourceDir=");
	if (sourceDir)
	{
		sb_t manifestPath = { BB_EMPTY_INITIALIZER };
		sb_va(&manifestPath, "%s/%s", sourceDir, s_globals.manifestFilename);
		path_resolve_inplace(&manifestPath);
		JSON_Value* val = json_parse_file(sb_get(&manifestPath));
		if (val)
		{
			manifest = json_deserialize_updateManifest_t(val);
			json_value_free(val);
		}
		sb_reset(&manifestPath);
		version = update_resolve_version(&manifest, version);
	}

	sb_t sbSourcePath = { BB_EMPTY_INITIALIZER };
	if (version && sourceDir && !sourcePath)
	{
		sbSourcePath = update_get_archive_name(sourceDir, s_globals.appdataName, version);
		sourcePath = sbSourcePath.data;
	}

	BB_TRACE(kBBLogLevel_Verbose, "Startup", "Version: %s", version ? version : "(none)");
	BB_TRACE(kBBLogLevel_Verbose, "Startup", "Source zip: %s", sourcePath ? sourcePath : "(none)");
	BB_TRACE(kBBLogLevel_Verbose, "Startup", "Target dir: %s", targetDir.data ? targetDir.data : "(none)");

	if (version && *version && sourcePath && *sourcePath && targetDir.data && *targetDir.data)
	{
		success = mc_updater_do_update(version, sourcePath, targetDir.data);
	}
	else
	{
		if (!version && !sourcePath && sourceDir && cmdline_find("-create") > 0)
		{
			success = mc_updater_create_build(sourceDir);
		}
		else
		{
			success = false;
			BB_ERROR("Usage", "usage: %s -version=sourceVersion -source=/path/to/source.zip -target=/path/to/install/dir", cmdline_get_exe_filename());
		}
	}

	if (success)
	{
		BB_TRACE(kBBLogLevel_Display, "Shutdown", "Success.");
		shutdown_pause(true);
	}
	else
	{
		BB_WARNING("Shutdown", "Encountered error.");
		shutdown_pause(false);
	}

	const char* relaunchCommand = cmdline_find_prefix("-relaunch=");
	if (targetDir.data && *targetDir.data && relaunchCommand && *relaunchCommand)
	{
#if BB_USING(BB_PLATFORM_WINDOWS)
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		char* cmdline = va("%s", relaunchCommand);
		BOOL res = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, targetDir.data, &si, &pi);

		if (res)
		{
			BB_LOG("Restart", "Created process: %s", relaunchCommand);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		else
		{
			system_error_to_log(GetLastError(), "Restart", va("Failed to create process:\n  %s", cmdline));
		}
#endif
	}

	updateManifest_reset(&manifest);
	sb_reset(&sbSourcePath);
	sb_reset(&targetDir);
	return success;
}
