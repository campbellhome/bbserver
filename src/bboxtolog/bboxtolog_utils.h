// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_packet.h"

#include "sb.h"

#include "bb_wrap_stdio.h"

#if defined(__cplusplus)
extern "C" {
#endif

void print_stderr(const char* text);
void print_stdout(const char* text);
void print_fp(const char* text, FILE *fp);
	
void separateFilename(const char* filename, sb_t* base, sb_t* ext);
b32 wildcardMatch(const char* pattern, const char* input);

typedef struct process_file_data_s process_file_data_t;
typedef void(tail_catchup_func_t)(process_file_data_t* process_file_data);
typedef void(packet_func_t)(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data);

typedef struct process_file_data_s
{
	const char* source;
	tail_catchup_func_t* tail_catchup_func;
	packet_func_t* packet_func;
	void* userdata;
} process_file_data_t;

int process_file(process_file_data_t* process_file_data);

typedef struct process_packet_data_s
{
	sb_t lines;
	u64 threadId;
	s64 milliseconds;
	u32 numPartialLogsUsed;
	u32 pad;
} process_packet_data_t;

process_packet_data_t build_packet_data(const bb_decoded_packet_t* decoded);
void reset_packet_data(process_packet_data_t *data);

#if defined(__cplusplus)
} // extern "C"
#endif
