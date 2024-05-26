// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#include "config_whitelist_push.h"
#include "bb_array.h"
#include "bb_assert.h"
#include "bb_log.h"
#include "bb_string.h"
#include "config.h"
#include "dns_task.h"
#include "sdict.h"
#include "va.h"
#include <stdlib.h>

#include "bb_wrap_windows.h"
#include "bb_wrap_winsock2.h"

static s32 s_lastWhitelistId = 0;

void discovery_push_whitelist(resolved_whitelist_t* resolvedWhitelist);

static void resolved_whitelist_add_entry(resolved_whitelist_t* whitelist, resolved_whitelist_entry_t* entry)
{
	for (u32 i = 0; i < whitelist->count; ++i)
	{
		resolved_whitelist_entry_t* existing = whitelist->data + i;
		if (memcmp(existing, entry, sizeof(resolved_whitelist_entry_t)) == 0)
			return;
	}

	bba_push(*whitelist, *entry);
}

static void config_push_whitelist_task_statechanged(task* t)
{
	if (task_done(t))
	{
		resolved_whitelist_t resolvedWhitelist = { BB_EMPTY_INITIALIZER };
		for (u32 taskIndex = 0; taskIndex < t->subtasks.count; ++taskIndex)
		{
			task* subtask = t->subtasks.data + taskIndex;
			dns_task_result* result = dns_task_get_result(subtask);
			if (!result)
				continue;

			sdict_t* sd = &subtask->extraData;
			BB_LOG("whitelist", "lookup %s has %u results:", sb_get(&result->name), result->addrs.count);
			if (result->addrs.count)
			{
				s32 subnetMask = atoi(sdict_find_safe(sd, "subnetMask"));
				for (u32 i = 0; i < result->addrs.count; ++i)
				{
					resolved_whitelist_entry_t entry = { BB_EMPTY_INITIALIZER };
					entry.allow = atoi(sdict_find_safe(sd, "allow"));
					bb_strncpy(entry.applicationName, sdict_find_safe(sd, "applicationName"), sizeof(entry.applicationName));

					char buf[64];
					BB_LOG("whitelist", "  %s", bb_format_addr(buf, sizeof(buf), (const struct sockaddr*)&result->addrs.data[i], sizeof(result->addrs.data[i]), false));
					entry.addr = result->addrs.data[i];

					if (entry.addr.ss_family == AF_INET6)
					{
						struct sockaddr_in6* addr = (struct sockaddr_in6*)&entry.addr;
						struct sockaddr_in6* mask = (struct sockaddr_in6*)&entry.subnetMask;

						mask->sin6_family = AF_INET6;

						s32 localSubnetMask = subnetMask;
						if (bbnet_socket_is6to4((const struct sockaddr*)addr))
						{
							if (subnetMask <= 32) // IPv4 mapped to IPv6, treat the mask as /32
							{
								localSubnetMask += 12 * 8;
							}
						}

						for (int byteIndex = 0; byteIndex < 16 && localSubnetMask; ++byteIndex)
						{
							for (int bitIndex = 7; bitIndex >= 0 && localSubnetMask; --bitIndex)
							{
								u8 byteMask = 1u << bitIndex;
								mask->sin6_addr.s6_addr[byteIndex] |= byteMask;
								if (!--localSubnetMask)
									break;
							}
						}
					}
					else if (entry.addr.ss_family == AF_INET)
					{
						entry.subnetMask = entry.addr;
						struct sockaddr_in* mask = (struct sockaddr_in*)&entry.subnetMask;
						if (subnetMask >=0 && subnetMask < 32)
						{
							BB_S_ADDR_UNION(*mask) = (1u << subnetMask) - 1u;
						}
						else
						{
							BB_S_ADDR_UNION(*mask) = ~0u;
						}
					}
					else
					{
						BB_ASSERT(false);
					}

					resolved_whitelist_add_entry(&resolvedWhitelist, &entry);
				}
			}
		}

		s32 whitelistId = atoi(sdict_find_safe(&t->extraData, "whitelistId"));
		if (whitelistId == s_lastWhitelistId)
		{
			discovery_push_whitelist(&resolvedWhitelist);
		}
		else
		{
			BB_WARNING("whitelist", "ignored whitelist %d in favor of pending whitelist %d", whitelistId, s_lastWhitelistId);
			bba_free(resolvedWhitelist);
		}
	}
}

static void dns_task_finished(task* t, dns_task_result* results)
{
	BB_UNUSED(t);
	BB_UNUSED(results);
}

static void queue_dns_task(task* groupTask,
                           b32 allow,
                           const char* hostname, s32 mask,
                           const char* applicationName)
{
	task subtask = dns_task_create(hostname, dns_task_finished);
	sdict_add_raw(&subtask.extraData, "allow", allow ? "1" : "0");
	sdict_add_raw(&subtask.extraData, "applicationName", applicationName);
	sdict_add_raw(&subtask.extraData, "subnetMask", va("%d", mask));
	task_queue_subtask(groupTask, subtask);

	if (!strcmp(hostname, "localhost"))
	{
		char localhostHostname[256];
		gethostname(localhostHostname, sizeof(localhostHostname));
		queue_dns_task(groupTask, allow, localhostHostname, mask, applicationName);
	}
}

void config_push_whitelist(configWhitelist_t* configWhitelist)
{
	task groupTask = { BB_EMPTY_INITIALIZER };
	groupTask.tick = task_tick_subtasks;
	sb_append(&groupTask.name, "config_push_whitelist");
	groupTask.stateChanged = config_push_whitelist_task_statechanged;
	for (u32 i = 0; i < configWhitelist->count; ++i)
	{
		s32 mask = 128;
		configWhitelistEntry_t* entry = configWhitelist->data + i;
		char* sep = strchr(sb_get(&entry->addressPlusMask), '/');
		if (sep)
		{
			*sep = '\0';
			mask = atoi(sep + 1);
			mask = (mask < 0) ? 0 : ((mask > 128) ? 128 : mask);
			queue_dns_task(&groupTask, entry->allow, sb_get(&entry->addressPlusMask), mask, sb_get(&entry->applicationName));
			*sep = '/';
		}
		else
		{
			queue_dns_task(&groupTask, entry->allow, sb_get(&entry->addressPlusMask), mask, sb_get(&entry->applicationName));
		}
	}
	sdict_add_raw(&groupTask.extraData, "whitelistId", va("%d", ++s_lastWhitelistId));
	task_queue(groupTask);
}
