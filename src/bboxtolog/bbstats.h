// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

int bbstats_main(int argc, char** argv);

AUTOSTRUCT typedef struct bbstats_process_file_data_s {
	u64 elapsedMillis;
} bbstats_process_file_data_t;

void bbstats_process_file_data_reset(bbstats_process_file_data_t *val);

#if defined(__cplusplus)
} // extern "C"
#endif
