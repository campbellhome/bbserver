// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

void cmdline_init_composite(const char* cmdline);
void cmdline_init(int argc, const char** argv);
void cmdline_shutdown(void);

int cmdline_argc(void);
const char* cmdline_argv(int index);
int cmdline_find(const char* arg);
const char* cmdline_find_prefix(const char* prefix);
const char* cmdline_get_exe_dir(void);
const char* cmdline_get_exe_filename(void);
const char* cmdline_get_full(void);

#if defined(__cplusplus)
}
#endif
