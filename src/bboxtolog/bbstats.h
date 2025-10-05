// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_packet.h"
#include "bboxtolog_utils.h"

#include "common.h"
#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

int bbstats_main(int argc, char** argv);

AUTOSTRUCT AUTOFROMLOC typedef struct bbstats_bucket_s
{
	sb_t key;
	u64 elapsedMillis;
	s64 totalLines;
	s64 totalBytes;
} bbstats_bucket_t;

AUTOSTRUCT typedef struct bbstats_buckets_s
{
	u32 count;
	u32 allocated;
	bbstats_bucket_t* data;
} bbstats_buckets_t;

void bbstats_buckets_reset(bbstats_buckets_t *val);

AUTOSTRUCT AUTOFROMLOC typedef struct bbstats_fileHashEntry
{
	sb_t key;
	u64 startMillis;
	u64 endMillis;
	s64 totalLines;
	s64 totalBytes;
} bbstats_fileHashEntry;

AUTOSTRUCT AUTOFROMLOC typedef struct bbstats_fileHashChain
{
	u32 count;
	u32 allocated;
	bbstats_fileHashEntry* data;
} bbstats_fileHashChain;

AUTOSTRUCT AUTOFROMLOC AUTOSTRINGHASH typedef struct bbstats_fileHashTable
{
	u32 count;
	u32 allocated;
	bbstats_fileHashChain* data;
} bbstats_fileHashTable;

bbstats_fileHashEntry *bbstats_fileHashTable_find(bbstats_fileHashTable *table, const char *name);
bbstats_fileHashEntry *bbstats_fileHashTable_insert(bbstats_fileHashTable *table, const bbstats_fileHashEntry *entry);

AUTOSTRUCT typedef struct bbstats_process_file_data_s {
	sb_t path;
	bb_decoded_packet_t appInfo;
	u64 elapsedMillis;
	id_names_t categories;
	id_names_t filenames;
	bbstats_fileHashTable locationHashTable;
	bbstats_fileHashTable categoryHashTable;
} bbstats_process_file_data_t;

void bbstats_process_file_data_reset(bbstats_process_file_data_t *val);

AUTOSTRUCT typedef struct bbstats_data_s {
	sbs_t applicationNames;

	u32 allocated;
	u32 count;
	bbstats_process_file_data_t* data;
} bbstats_data_t;

void bbstats_data_reset(bbstats_data_t *val);

#if defined(__cplusplus)
} // extern "C"
#endif
