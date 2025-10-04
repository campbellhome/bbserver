// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bbstats.h"

#include "bb.h"
#include "bb_array.h"
#include "bb_common.h"
#include "bb_malloc.h"
#include "bb_string.h"
#include "bboxtolog_utils.h"
#include "path_utils.h"
#include "sb.h"
#include "span.h"
#include "va.h"

#include "bb_wrap_dirent.h"
#include "bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include <stdlib.h>
#include <string.h>

static void bbstats_process_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	process_packet_data_t packetData = build_packet_data(decoded);

	bbstats_process_file_data_t* userdata = process_file_data->userdata;
	userdata->elapsedMillis = packetData.milliseconds;

	reset_packet_data(&packetData);
}

static void bbstats_process_file(const sb_t* path)
{
	process_file_data_t process_file_data = { BB_EMPTY_INITIALIZER };
	bbstats_process_file_data_t userdata = { BB_EMPTY_INITIALIZER };

	process_file_data.source = sb_get(path);
	process_file_data.packet_func = &bbstats_process_packet;
	process_file_data.userdata = &userdata;

	print_stdout(va("Processing %s...\n", process_file_data.source));
	process_file(&process_file_data);
	print_stdout(va("  Duration: %llu millis\n", userdata.elapsedMillis));

	bbstats_process_file_data_reset(&userdata);
}

void bbstats_process_recursive(sb_t* dirName, const char* pattern, b32 bRecursive)
{
	sbs_t files = { BB_EMPTY_INITIALIZER };
	sbs_t dirs = { BB_EMPTY_INITIALIZER };
	DIR* d = opendir(sb_get(dirName));
	if (d)
	{
		sb_t patternFilename = { BB_EMPTY_INITIALIZER };
		sb_t patternExtension = { BB_EMPTY_INITIALIZER };
		separateFilename(pattern, &patternFilename, &patternExtension);

		struct dirent* entry;
		while ((entry = readdir(d)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				sb_t entryFilename = { BB_EMPTY_INITIALIZER };
				sb_t entryExtension = { BB_EMPTY_INITIALIZER };
				separateFilename(entry->d_name, &entryFilename, &entryExtension);

				// only support * or full match for now
				if (wildcardMatch(sb_get(&patternFilename), sb_get(&entryFilename)))
				{
					if (wildcardMatch(sb_get(&patternExtension), sb_get(&entryExtension)))
					{
						sb_t* val = bba_add(files, 1);
						if (val)
						{
							*val = sb_from_va("%s\\%s", sb_get(dirName), entry->d_name);
						}
					}
				}

				sb_reset(&entryFilename);
				sb_reset(&entryExtension);
			}
		}
		closedir(d);

		sb_reset(&patternFilename);
		sb_reset(&patternExtension);
	}

	for (u32 i = 0; i < files.count; ++i)
	{
		bbstats_process_file(files.data + i);
	}

	if (bRecursive)
	{
		d = opendir(sb_get(dirName));
		if (d)
		{
			struct dirent* entry;
			while ((entry = readdir(d)) != NULL)
			{
				if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				{
					sb_t* val = bba_add(dirs, 1);
					if (val)
					{
						*val = sb_from_va("%s\\%s", sb_get(dirName), entry->d_name);
					}
				}
			}
			closedir(d);
		}

		for (u32 i = 0; i < dirs.count; ++i)
		{
			sb_t* subdir = dirs.data + i;
			bbstats_process_recursive(subdir, pattern, bRecursive);
		}
	}

	sbs_reset(&files);
	sbs_reset(&dirs);
}

void bbstats_process(const char* target, b32 bRecursive)
{
	const char* filename = "*.bbox";
	sb_t dir = sb_from_c_string(target);

	DIR* d = opendir(target);
	b32 isdir = d != NULL;
	if (d)
	{
		closedir(d);
	}

	if (!isdir)
	{
		filename = path_get_filename(target);
		path_remove_filename(&dir);
	}
	if (sb_len(&dir) == 0)
	{
		sb_append(&dir, ".");
	}
	bbstats_process_recursive(&dir, filename, bRecursive);
	sb_reset(&dir);
}

int bbstats_main(int argc, char** argv)
{
	int ret = 0;
	b32 bPastSwitches = false;
	b32 bRecursive = false;

	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (!bb_stricmp(arg, "--"))
		{
			bPastSwitches = true;
			continue;
		}

		if (!bPastSwitches && *arg == '-')
		{
			if (!bb_stricmp(arg, "-bbstats"))
			{
				// do nothing
			}
			else if (!bb_stricmp(arg, "-r"))
			{
				bRecursive = true;
			}
			else
			{
				return 1; // TODO: print usage
			}
		}
		else
		{
			bbstats_process(arg, bRecursive);
		}
	}

	return ret;
}
