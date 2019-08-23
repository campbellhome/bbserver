// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "config_whitelist_push.h"
#include "bb_array.h"
#include "bb_log.h"
#include "bb_string.h"
#include "config.h"
#include "dns_task.h"
#include "sdict.h"
#include "va.h"
#include <stdlib.h>

static s32 s_lastWhitelistId = 0;

void discovery_push_whitelist(resolved_whitelist_t *resolvedWhitelist);

static void resolved_whitelist_add_entry(resolved_whitelist_t *whitelist, resolved_whitelist_entry_t *entry)
{
	for(u32 i = 0; i < whitelist->count; ++i) {
		resolved_whitelist_entry_t *existing = whitelist->data + i;
		if(memcmp(existing, entry, sizeof(resolved_whitelist_entry_t)) == 0)
			return;
	}

	bba_push(*whitelist, *entry);
}

static void config_push_whitelist_task_statechanged(task *t)
{
	if(task_done(t)) {
		resolved_whitelist_t resolvedWhitelist = { BB_EMPTY_INITIALIZER };
		for(u32 taskIndex = 0; taskIndex < t->subtasks.count; ++taskIndex) {
			task *subtask = t->subtasks.data + taskIndex;
			dns_task_result *result = dns_task_get_result(subtask);
			if(!result)
				continue;

			sdict_t *sd = &subtask->extraData;
			BB_LOG("whitelist", "lookup %s has %u results:", sb_get(&result->name), result->addrs.count);
			if(result->addrs.count) {
				resolved_whitelist_entry_t entry = { BB_EMPTY_INITIALIZER };
				entry.allow = atoi(sdict_find_safe(sd, "allow"));
				bb_strncpy(entry.applicationName, sdict_find_safe(sd, "applicationName"), sizeof(entry.applicationName));
				entry.mask = strtoul(sdict_find_safe(sd, "mask"), NULL, 10);
				for(u32 i = 0; i < result->addrs.count; ++i) {
					char buf[32];
					BB_LOG("whitelist", "  %s", bb_format_ip(buf, sizeof(buf), result->addrs.data[i]));
					entry.ip = result->addrs.data[i];
					resolved_whitelist_add_entry(&resolvedWhitelist, &entry);
				}
			}
		}

		s32 whitelistId = atoi(sdict_find_safe(&t->extraData, "whitelistId"));
		if(whitelistId == s_lastWhitelistId) {
			discovery_push_whitelist(&resolvedWhitelist);
		} else {
			BB_WARNING("whitelist", "ignored whitelist %d in favor of pending whitelist %d", whitelistId, s_lastWhitelistId);
			bba_free(resolvedWhitelist);
		}
	}
}

static void dns_task_finished(task *t, dns_task_result *results)
{
	BB_UNUSED(t);
	BB_UNUSED(results);
}

static void queue_dns_task(task *groupTask,
                           b32 allow,
                           const char *hostname, u32 mask,
                           const char *applicationName)
{
	task subtask = dns_task_create(hostname, dns_task_finished);
	sdict_add_raw(&subtask.extraData, "allow", allow ? "1" : "0");
	sdict_add_raw(&subtask.extraData, "applicationName", applicationName);
	sdict_add_raw(&subtask.extraData, "mask", va("%u", mask));
	task_queue_subtask(groupTask, subtask);

	if(!strcmp(hostname, "localhost")) {
		char localhostHostname[256];
		gethostname(localhostHostname, sizeof(localhostHostname));
		queue_dns_task(groupTask, allow, localhostHostname, mask, applicationName);
	}
}

void config_push_whitelist(configWhitelist_t *configWhitelist)
{
	task groupTask = { BB_EMPTY_INITIALIZER };
	groupTask.tick = task_tick_subtasks;
	sb_append(&groupTask.name, "config_push_whitelist");
	groupTask.stateChanged = config_push_whitelist_task_statechanged;
	for(u32 i = 0; i < configWhitelist->count; ++i) {
		u32 mask = ~0U;
		configWhitelistEntry_t *entry = configWhitelist->data + i;
		char *sep = strchr(sb_get(&entry->addressPlusMask), '/');
		if(sep) {
			int val;
			u32 zeros;
			*sep = '\0';
			val = atoi(sep + 1);
			zeros = (val < 0) ? 0 : (val > 32) ? 32 : val;
			if(zeros == 32) {
				mask = 0;
			} else if(zeros > 0) {
				mask = mask << zeros;
			}
			queue_dns_task(&groupTask, entry->allow, sb_get(&entry->addressPlusMask), mask, sb_get(&entry->applicationName));
			*sep = '/';
		} else {
			queue_dns_task(&groupTask, entry->allow, sb_get(&entry->addressPlusMask), mask, sb_get(&entry->applicationName));
		}
	}
	sdict_add_raw(&groupTask.extraData, "whitelistId", va("%d", ++s_lastWhitelistId));
	task_queue(groupTask);
}
