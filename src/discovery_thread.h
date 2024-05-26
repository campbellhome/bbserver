// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

int discovery_thread_init(const int addrFamily); // AF_INET or AF_INET6, or AF_UNSPEC to listen to both on separate sockets
void discovery_thread_shutdown(void);

#if defined(__cplusplus)
}
#endif
