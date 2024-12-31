// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "tasks.h"
#include "bb_sockets.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_dns_task_result dns_task_result;
typedef void(DnsTask_Finished)(task* t, dns_task_result* results);

typedef struct tag_dns_addresses
{
	u32 count;
	u32 allocated;
	struct sockaddr_storage* data;
} dns_addresses;

typedef struct tag_dns_task_result
{
	sb_t name;
	dns_addresses addrs;
} dns_task_result;

task dns_task_create(const char* name, DnsTask_Finished* finished);
task dns_task_create_addrfamily(const char* name, DnsTask_Finished* finished, const int addrFamily); // use AF_INET to force IPv4, AF_INET6 to force IPv6, AF_UNSPEC to get both
dns_task_result* dns_task_get_result(task* t);

#if defined(__cplusplus)
}
#endif
