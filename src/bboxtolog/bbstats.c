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
#include "tokenize.h"
#include "va.h"

#include "bb_wrap_dirent.h"
#include "bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include <stdlib.h>
#include <string.h>

#if BB_USING(BB_PLATFORM_WINDOWS)
#include <conio.h>
#endif

static bbstats_data_t s_data;
static b32 s_bSortByBytes;

static bbstats_fileHashEntry* bbstats_hashTable_find_or_add(bbstats_fileHashTable* table, const char* key)
{
	bbstats_fileHashEntry* entry = bbstats_fileHashTable_find(table, key);
	if (entry)
	{
		return entry;
	}

	bbstats_fileHashEntry newEntry = { BB_EMPTY_INITIALIZER };
	newEntry.key = sb_from_c_string_no_alloc(key);
	entry = bbstats_fileHashTable_insert(table, &newEntry);
	return entry;
}

static void bbstats_process_non_log_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	bbstats_process_file_data_t* userdata = process_file_data->userdata;

	if (bbpacket_is_app_info_type(decoded->type))
	{
		userdata->appInfo = *decoded;
	}
	else if (decoded->type == kBBPacketType_CategoryId)
	{
		id_names_find_or_add(&userdata->categories, decoded->packet.categoryId.id, decoded->packet.categoryId.name);
	}
	else if (decoded->type == kBBPacketType_FileId)
	{
		id_names_find_or_add(&userdata->filenames, decoded->packet.fileId.id, decoded->packet.fileId.name);
	}
}

static bbstats_bucket_t* bbstats_bucket_find_or_add(bbstats_buckets_t *buckets, const char* key)
{
	for (u32 i = 0; i < buckets->count; ++i)
	{
		bbstats_bucket_t* bucket = buckets->data + i;
		if (!bb_stricmp(sb_get(&bucket->key), key))
		{
			return bucket;
		}
	}

	bbstats_bucket_t* bucket = bba_add(*buckets, 1);
	bucket->key = sb_from_c_string(key);
	return bucket;
}

static void bbstats_accumulate_bucket(bbstats_buckets_t* buckets, bbstats_fileHashEntry* entry)
{
	bbstats_bucket_t* bucket = bbstats_bucket_find_or_add(buckets, sb_get(&entry->key));
	bucket->elapsedMillis += entry->endMillis - entry->startMillis;
	bucket->totalLines += entry->totalLines;
	bucket->totalBytes += entry->totalBytes;
}

static void bbstats_accumulate_hashTable(bbstats_buckets_t* buckets, bbstats_fileHashTable* table)
{
	for (u32 chainIndex = 0; chainIndex < table->count; ++chainIndex)
	{
		bbstats_fileHashChain* chain = table->data + chainIndex;
		for (u32 entryIndex = 0; entryIndex < chain->count; ++entryIndex)
		{
			bbstats_fileHashEntry* entry = chain->data + entryIndex;
			bbstats_accumulate_bucket(buckets, entry);
		}
	}
}

static int bbstats_bucket_sort(const void* _a, const void* _b)
{
	const bbstats_bucket_t* a = (const bbstats_bucket_t*)_a;
	const bbstats_bucket_t* b = (const bbstats_bucket_t*)_b;
	if (s_bSortByBytes)
	{
		if (a->totalBytes != b->totalBytes)
		{
			return (int)(b->totalBytes - a->totalBytes);
		}
	}
	else
	{
		if (a->totalLines != b->totalLines)
		{
			return (int)(b->totalLines - a->totalLines);
		}
	}

	return bb_stricmp(sb_get(&a->key), sb_get(&b->key));
}

static void bbstats_sort_buckets(bbstats_buckets_t* buckets)
{
	qsort(buckets->data, buckets->count, sizeof(buckets->data[0]), bbstats_bucket_sort);
}

static void bbstats_print_buckets(bbstats_buckets_t* buckets, const char *key)
{
	if (s_bSortByBytes)
	{
		print_stdout(va("bytes,lines,bytes/line,elapsed,bytes/ms,lines/ms,%s\n", key));
	}
	else
	{
		print_stdout(va("lines,bytes,bytes/line,elapsed,lines/ms,bytes/ms,%s\n", key));
	}
	for (u32 i = 0; i < buckets->count; ++i)
	{
		bbstats_bucket_t *bucket = buckets->data + i;

		double elapsedMillis = (bucket->elapsedMillis) ? (double)bucket->elapsedMillis : 1.0;
		double bytesRate = (bucket->elapsedMillis) ? bucket->totalBytes / elapsedMillis : 0.0;
		double linesRate = (bucket->elapsedMillis) ? bucket->totalLines / elapsedMillis : 0.0;
		double bytesPerLine = bucket->totalBytes / (double)bucket->totalLines;
		if (s_bSortByBytes)
		{
			print_stdout(va("%lld,%lld,%.1f,%llu,%f,%f,%s\n", bucket->totalBytes, bucket->totalLines, bytesPerLine, bucket->elapsedMillis, bytesRate, linesRate, sb_get(&bucket->key)));
		}
		else
		{
			print_stdout(va("%lld,%lld,%.1f,%llu,%f,%f,%s\n", bucket->totalLines, bucket->totalBytes, bytesPerLine, bucket->elapsedMillis, linesRate, bytesRate, sb_get(&bucket->key)));
		}
	}
}

static void bbstats_accumulate_log_packet(bbstats_fileHashEntry* entry, process_packet_data_t* packetData, u32 numLines)
{
	if (!entry->startMillis)
	{
		entry->startMillis = packetData->milliseconds;
	}
	entry->endMillis = packetData->milliseconds;

	entry->totalLines += numLines;
	entry->totalBytes += sb_len(&packetData->lines);
}

static void bbstats_process_log_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	bbstats_process_file_data_t* userdata = process_file_data->userdata;
	process_packet_data_t packetData = build_packet_data(decoded);

	userdata->elapsedMillis = packetData.milliseconds;

	id_name_t categoryIdName = id_names_find_non_null(&userdata->categories, decoded->packet.logText.categoryId);
	id_name_t filenameIdName = id_names_find_non_null(&userdata->filenames, decoded->header.fileId);

	const char* category = sb_get(&categoryIdName.name);
	const char* filename = sb_get(&filenameIdName.name);

	bbstats_fileHashEntry* locationEntry = bbstats_hashTable_find_or_add(&userdata->locationHashTable, va("%s:%d", filename, decoded->header.line));
	bbstats_fileHashEntry* categoryEntry = bbstats_hashTable_find_or_add(&userdata->categoryHashTable, category);

	u32 numLines = 0;
	span_t cursor = { packetData.lines.data, packetData.lines.data + packetData.lines.count };
	for (span_t line = tokenizeLine(&cursor); line.start; line = tokenizeLine(&cursor))
	{
		++numLines;
	}

	bbstats_accumulate_log_packet(locationEntry, &packetData, numLines);
	bbstats_accumulate_log_packet(categoryEntry, &packetData, numLines);

	reset_packet_data(&packetData);
}

static void bbstats_process_file(const sb_t* path)
{
	process_file_data_t process_file_data = { BB_EMPTY_INITIALIZER };

	bbstats_process_file_data_t* userdata = bba_add(s_data, 1);
	userdata->path = sb_clone(path);

	bba_add(userdata->locationHashTable, 256);
	bba_add(userdata->categoryHashTable, 256);

	process_file_data.source = sb_get(path);
	process_file_data.log_packet_func = &bbstats_process_log_packet;
	process_file_data.non_log_packet_func = &bbstats_process_non_log_packet;
	process_file_data.userdata = userdata;

	process_file(&process_file_data);

	sbs_add_unique(&s_data.applicationNames, sb_from_c_string_no_alloc(userdata->appInfo.packet.appInfo.applicationName));
}

static void bbstats_print_overall(const char* applicationName, bb_platform_e platform)
{
	u32 count = 0;
	u64 elapsedMillis = 0;
	for (u32 i = 0; i < s_data.count; ++i)
	{
		bbstats_process_file_data_t* fileData = s_data.data + i;
		if (applicationName && *applicationName && bb_stricmp(applicationName, fileData->appInfo.packet.appInfo.applicationName) != 0)
		{
			continue;
		}

		if (platform != kBBPlatform_Count && platform != (bb_platform_e)fileData->appInfo.packet.appInfo.platform)
		{
			continue;
		}

		++count;
		elapsedMillis += fileData->elapsedMillis;
	}

	if (!count)
	{
		return;
	}

	const char* platformName = (platform == kBBPlatform_Count) ? NULL : bb_platform_name(platform);
	if (applicationName && *applicationName && platformName)
	{
		print_stdout(va("======== %s / %s: %u/%u files ========\n", applicationName, platformName, count, s_data.count));
	}
	else if (applicationName && *applicationName)
	{
		print_stdout(va("======== %s: %u/%u files ========\n", applicationName, count, s_data.count));
	}
	else if (platformName)
	{
		print_stdout(va("======== %s: %u/%u files ========\n", platformName, count, s_data.count));
	}
	else
	{
		print_stdout(va("======== Overall: %u files ========\n", count));
	}

	print_stdout(va("Duration: %llu millis\n", elapsedMillis));

	bbstats_buckets_t locationBuckets = { BB_EMPTY_INITIALIZER };
	bbstats_buckets_t categoryBuckets = { BB_EMPTY_INITIALIZER };

	for (u32 i = 0; i < s_data.count; ++i)
	{
		bbstats_process_file_data_t* fileData = s_data.data + i;
		if (applicationName && *applicationName && bb_stricmp(applicationName, fileData->appInfo.packet.appInfo.applicationName) != 0)
		{
			continue;
		}

		if (platform != kBBPlatform_Count && platform != (bb_platform_e)fileData->appInfo.packet.appInfo.platform)
		{
			continue;
		}

		print_stdout(va("Path: %s\n", sb_get(&fileData->path)));

		bbstats_accumulate_hashTable(&locationBuckets, &fileData->locationHashTable);
		bbstats_accumulate_hashTable(&categoryBuckets, &fileData->categoryHashTable);
	}

	bbstats_sort_buckets(&locationBuckets);
	bbstats_sort_buckets(&categoryBuckets);

	print_stdout("\n");

	bbstats_print_buckets(&locationBuckets, "location");

	print_stdout("\n");

	bbstats_print_buckets(&categoryBuckets, "category");

	print_stdout("\n\n\n");

	bbstats_buckets_reset(&categoryBuckets);
	bbstats_buckets_reset(&locationBuckets);
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
	b32 bPerApp = false;
	b32 bPerPlatform = false;
	b32 bOverall = false;
	b32 bPause = false;

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
			if (!bb_stricmp(arg, "-bbstats") || !bb_stricmp(arg, "--bbstats"))
			{
				// do nothing
			}
			else if (!bb_stricmp(arg, "-r"))
			{
				bRecursive = true;
			}
			else if (!bb_stricmp(arg, "-app") || !bb_stricmp(arg, "--app"))
			{
				bPerApp = true;
			}
			else if (!bb_stricmp(arg, "-platform") || !bb_stricmp(arg, "--platform"))
			{
				bPerPlatform = true;
			}
			else if (!bb_stricmp(arg, "-overall") || !bb_stricmp(arg, "--overall"))
			{
				bOverall = true;
			}
			else if (!bb_stricmp(arg, "-bytes") || !bb_stricmp(arg, "--bytes"))
			{
				s_bSortByBytes = true;
			}
			else if (!bb_stricmp(arg, "-lines") || !bb_stricmp(arg, "--lines"))
			{
				s_bSortByBytes = false;
			}
			else if (!bb_stricmp(arg, "-pause") || !bb_stricmp(arg, "--pause"))
			{
				bPause = true;
			}
			else
			{
				print_stderr("Usage: bboxtolog.exe --bbstats [-r] [--app] [--platform] [--overall] [--bytes] [--lines] [--pause] <filename.bbox or dir>\n");
				return 1;
			}
		}
		else
		{
			bbstats_process(arg, bRecursive);
		}
	}

	if (!bPerApp && !bPerPlatform)
	{
		bOverall = true;
	}

	if (bPerApp && bPerPlatform)
	{
		for (u32 appIndex = 0; appIndex < s_data.applicationNames.count; ++appIndex)
		{
			for (u32 platformIndex = 0; platformIndex < kBBPlatform_Count; ++platformIndex)
			{
				bbstats_print_overall(sb_get(s_data.applicationNames.data + appIndex), (bb_platform_e)platformIndex);
			}
		}
	}
	else if (bPerApp)
	{
		for (u32 appIndex = 0; appIndex < s_data.applicationNames.count; ++appIndex)
		{
			bbstats_print_overall(sb_get(s_data.applicationNames.data + appIndex), kBBPlatform_Count);
		}
	}
	else if (bPerPlatform)
	{
		for (u32 platformIndex = 0; platformIndex < kBBPlatform_Count; ++platformIndex)
		{
			bbstats_print_overall(NULL, (bb_platform_e)platformIndex);
		}
	}

	if (bOverall)
	{
		bbstats_print_overall(NULL, kBBPlatform_Count);
	}

	bbstats_data_reset(&s_data);

	if (bPause)
	{
#if BB_USING(BB_PLATFORM_WINDOWS)
		print_stderr("Press any key to continue . . . ");
		_getch();
#else
		print_stderr("Press Enter to continue.");
		getchar();
#endif
	}

	return ret;
}
