// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "devkit_autodetect.h"
#include "bb.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "bb_time.h"
#include "config.h"
#include "env_utils.h"
#include "file_utils.h"
#include "process_task.h"
#include "sb.h"
#include "site_config.h"
#include "system_error_utils.h"
#include "tasks.h"
#include "tokenize.h"
#include "va.h"

BB_WARNING_PUSH(4820, 4668)
#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
BB_WARNING_POP

typedef struct devkit_s {
	sb_t addr;
	sb_t comment;
	b32 bPreferred;
	u8 pad[4];
} devkit_t;
typedef struct devkits_s {
	u32 count;
	u32 allocated;
	devkit_t *data;
} devkits_t;

static const u64 kDevkitAutodetectIntervalMillis = 5 * 60 * 1000;
static u64 s_lastDevkitAutodetectMillis;
static b32 s_devkitAutodetectActive;
static devkits_t s_devkits;
static HANDLE s_interfaceChangeHandle;
static b32 s_interfaceChanged;

static void InterfaceChanged(IN PVOID CallerContext, IN PMIB_IPFORWARD_ROW2 Row OPTIONAL, IN MIB_NOTIFICATION_TYPE NotificationType)
{
	BB_UNUSED(CallerContext);
	BB_UNUSED(Row);
	BB_UNUSED(NotificationType);
	if(!s_devkitAutodetectActive) {
		s_lastDevkitAutodetectMillis = 0;
	}
	s_interfaceChanged = true;
}

static void RegisterForInterfaceChanges(void)
{
	int ret = NotifyRouteChange2(AF_INET, &InterfaceChanged, NULL, FALSE, &s_interfaceChangeHandle);
	if(ret != NO_ERROR) {
		system_error_to_log(ret, "Devkit", "NotifyRouteChange2");
	}
}

static void UnregisterForInterfaceChanges(void)
{
	if(s_interfaceChangeHandle) {
		CancelMibChangeNotify2(s_interfaceChangeHandle);
		s_interfaceChangeHandle = NULL;
	}
}

static void devkits_reset(devkits_t *devkits)
{
	for(u32 i = 0; i < devkits->count; ++i) {
		devkit_t *devkit = devkits->data + i;
		sb_reset(&devkit->addr);
		sb_reset(&devkit->comment);
	}
	bba_free(*devkits);
}

void devkit_autodetect_shutdown(void)
{
	devkits_reset(&s_devkits);
	UnregisterForInterfaceChanges();
}

static void devkit_autodetect_finish(void)
{
	b32 changes = false;
	for(u32 whitelistIndex = 0; whitelistIndex < g_config.whitelist.count; ++whitelistIndex) {
		b32 valid = true;
		configWhitelistEntry_t *entry = g_config.whitelist.data + whitelistIndex;
		if(entry->autodetectedDevkit) {
			valid = false;
			for(u32 devkitIndex = 0; devkitIndex < s_devkits.count; ++devkitIndex) {
				devkit_t *devkit = s_devkits.data + devkitIndex;
				if(!strcmp(sb_get(&devkit->addr), sb_get(&entry->addressPlusMask))) {
					valid = true;
					if(devkit->bPreferred && entry->delay) {
						entry->delay = 0;
						changes = true;
					} else if(!devkit->bPreferred && entry->delay != 50) {
						entry->delay = 50;
						changes = true;
					}
					break;
				}
			}
		}
		if(!valid) {
			if(entry->delay != 75) {
				entry->delay = 75;
				changes = true;
			}
		}
	}
	for(u32 devkitIndex = 0; devkitIndex < s_devkits.count; ++devkitIndex) {
		b32 exists = false;
		devkit_t *devkit = s_devkits.data + devkitIndex;
		for(u32 whitelistIndex = 0; whitelistIndex < g_config.whitelist.count; ++whitelistIndex) {
			configWhitelistEntry_t *entry = g_config.whitelist.data + whitelistIndex;
			if(entry->autodetectedDevkit && !strcmp(sb_get(&devkit->addr), sb_get(&entry->addressPlusMask))) {
				exists = true;
				break;
			}
		}
		if(!exists) {
			BB_LOG("Devkit", "Adding autodetected devkit %s %s", sb_get(&devkit->addr), sb_get(&devkit->comment));
			configWhitelistEntry_t *entry = bba_add(g_config.whitelist, 1);
			if(entry) {
				entry->allow = true;
				entry->autodetectedDevkit = true;
				sb_append(&entry->addressPlusMask, sb_get(&devkit->addr));
				sb_append(&entry->comment, sb_get(&devkit->comment));
				if(!devkit->bPreferred) {
					entry->delay = 50;
				}
				changes = true;
			}
		}
	}
	if(changes || s_interfaceChanged) {
		s_interfaceChanged = false;
		config_push_whitelist(&g_config.whitelist);
	}
	s_devkitAutodetectActive = false;
	s_lastDevkitAutodetectMillis = bb_current_time_ms();
	devkits_reset(&s_devkits);
	BB_TRACE(kBBLogLevel_Verbose, "Devkit", "Devkit autodetect END");
}

static void devkit_autodetect_add(span_t addr, const char *platformName, const char *name)
{
	if(addr.start && *addr.start > '0' && *addr.start <= '9') {
		int numDots = 0;
		b32 validChars = true;
		s64 len = addr.end - addr.start;
		for(s64 i = 0; i < len; ++i) {
			if(addr.start[i] == '.') {
				++numDots;
			} else if(addr.start[i] < '0' || addr.start[i] > '9') {
				validChars = false;
				break;
			}
		}
		if(numDots == 3 && validChars) {
			devkit_t tmp = { BB_EMPTY_INITIALIZER };
			sb_append_range(&tmp.addr, addr.start, addr.end);
			if(name && *name) {
				sb_va(&tmp.comment, "%s (%s)", name, platformName);
			} else {
				sb_append(&tmp.comment, platformName);
			}
			sb_append(&tmp.comment, " - autodetected");
			bba_push(s_devkits, tmp);
		} else {
			BB_WARNING("Devkit", "Ignored devkit with invalid addr '%.*s'", addr.end - addr.start, addr.start);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

static void task_orbis_info_statechanged(task *t)
{
	task_process_statechanged(t);
	if(t->state == kTaskState_Succeeded) {
		task_process *p = t->taskData;
		if(p->process) {
			const char *cursor = p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "";
			span_t line = tokenize(&cursor, "\r\n");
			sb_t devkitName = { BB_EMPTY_INITIALIZER };
			sb_t gameAddr = { BB_EMPTY_INITIALIZER };
			b32 bDefault = false;
			b32 bConnected = false;
			b32 bInUse = false;
			while(line.start) {
				const char *lineCursor = line.start;
				span_t key = tokenize(&lineCursor, " \r\n");
				if(!bb_strnicmp(key.start, "CachedName:", key.end - key.start)) {
					span_t val = tokenize(&lineCursor, " \r\n");
					if(val.start) {
						sb_reset(&devkitName);
						sb_append_range(&devkitName, val.start, val.end);
					}
				} else if(!bb_strnicmp(key.start, "GameLanIpAddress:", key.end - key.start)) {
					span_t val = tokenize(&lineCursor, " \r\n");
					if(val.start) {
						sb_reset(&gameAddr);
						sb_append_range(&gameAddr, val.start, val.end);
					}
				} else if(!bb_strnicmp(key.start, "default:", key.end - key.start)) {
					span_t val = tokenize(&lineCursor, " \r\n");
					if(val.start) {
						if(!span_strcmp(val, span_from_string("True"))) {
							bDefault = true;
						}
					}
				} else if(!bb_strnicmp(key.start, "ConnectionState:", key.end - key.start)) {
					span_t val = tokenize(&lineCursor, " \r\n");
					if(val.start) {
						if(!span_strcmp(val, span_from_string("CONNECTION_CONNECTED"))) {
							bConnected = true;
						} else if(!span_strcmp(val, span_from_string("CONNECTION_IN_USE"))) {
							bInUse = true;
						}
					}
				}
				line = tokenize(&cursor, "\r\n");
			}
			if(gameAddr.data) {
				b32 bAdd = (bDefault || bConnected) && !bInUse;
				BB_LOG("Devkit", "Orbis Game Addr: %s (added:%d default:%d connected:%d inuse:%d)",
				       gameAddr.data, bAdd, bDefault, bConnected, bInUse);
				if(bAdd) {
					devkit_autodetect_add(span_from_string(gameAddr.data), bb_platform_name(kBBPlatform_Orbis), sb_get(&devkitName));
				}
			}
			sb_reset(&devkitName);
			sb_reset(&gameAddr);
		}
	}
}

static void task_orbis_list_statechanged(task *t)
{
	task_process_statechanged(t);
	if(t->state == kTaskState_Succeeded) {
		task_process *p = t->taskData;
		sb_t dir = env_resolve("%SCE_ROOT_DIR%\\ORBIS\\Tools\\Target Manager Server\\bin");
		task *groupTask = t->parent;
		if(p->process) {
			const char *cursor = p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "";
			span_t line = tokenize(&cursor, "\r\n");
			while(line.start) {
				const char *lineCursor = line.start;
				//BB_TRACE(kBBLogLevel_Verbose, "Devkit", "%.*s", line.end - line.start, line.start);
				span_t key = tokenize(&lineCursor, " ");
				if(!bb_strnicmp(key.start, "Host:", key.end - key.start)) {
					span_t val = tokenize(&lineCursor, " \r\n");
					if(val.start) {
						BB_LOG("Devkit", "Orbis Devkit Addr: %.*s", val.end - val.start, val.start);
						task subtask = process_task_create(
						    va("orbis_info %.*s", val.end - val.start, val.start),
						    kProcessSpawn_Tracked, sb_get(&dir), "\"%s\\orbis-ctrl.exe\" info %.*s",
						    sb_get(&dir), val.end - val.start, val.start);
						subtask.stateChanged = task_orbis_info_statechanged;
						task_queue_subtask(groupTask, subtask);
					}
				}
				line = tokenize(&cursor, "\r\n");
			}
		}
		sb_reset(&dir);
	}
}

static void devkit_autodetect_orbis_queue(task *groupTask)
{
	sb_t dir = env_resolve("%SCE_ROOT_DIR%\\ORBIS\\Tools\\Target Manager Server\\bin");
	sb_t path = { BB_EMPTY_INITIALIZER };
	sb_va(&path, "%s\\orbis-ctrl.exe", sb_get(&dir));
	if(file_readable(sb_get(&path))) {
		task orbisGroupTask = { BB_EMPTY_INITIALIZER };
		orbisGroupTask.tick = task_tick_subtasks;
		orbisGroupTask.parallel = true;
		sb_append(&orbisGroupTask.name, "orbis_autodetect_group");
		task orbisTask = process_task_create("orbis_list", kProcessSpawn_Tracked, sb_get(&dir), "\"%s\" list", sb_get(&path));
		orbisTask.stateChanged = task_orbis_list_statechanged;
		task_queue_subtask(&orbisGroupTask, orbisTask);
		task_queue_subtask(groupTask, orbisGroupTask);
	}
	sb_reset(&dir);
	sb_reset(&path);
}

//////////////////////////////////////////////////////////////////////////

static void task_xbconfig_statechanged(task *t)
{
	task_process_statechanged(t);
	if(t->state == kTaskState_Succeeded) {
		const char *key = sdict_find(&t->extraData, "key");
		task_process *p = t->taskData;
		if(p->process && key) {
			const char *cursor = p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "";
			span_t line = tokenize(&cursor, " \r\n");
			if(line.start) {
				const char *lineCursor = line.start;
				span_t keyToken = tokenize(&lineCursor, " :\r\n");
				span_t valToken = tokenize(&lineCursor, " :\r\n");
				size_t keyTokenLen = keyToken.end - keyToken.start;
				size_t valTokenLen = valToken.end - valToken.start;
				if(keyToken.start && valToken.start && keyTokenLen == strlen(key) && !strncmp(key, keyToken.start, keyTokenLen)) {
					BB_LOG("Devkit", "Durango %s: %.*s", key, valTokenLen, valToken.start);
					sdict_add_raw(&t->parent->extraData, key, va("%.*s", valTokenLen, valToken.start));
				}
			}
		}
	}
}

static void task_xbconnect_statechanged(task *t)
{
	task_process_statechanged(t);
	if(t->state == kTaskState_Succeeded) {
		task_process *p = t->taskData;
		if(p->process) {
			const char *cursor = p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "";
			span_t token = tokenize(&cursor, " \r\n");
			if(token.start && *token.start > '0' && *token.start <= '9') {
				BB_LOG("Devkit", "Durango Game Addr: %.*s", token.end - token.start, token.start);
				sdict_add_raw(&t->parent->extraData, "GameAddr", va("%.*s", token.end - token.start, token.start));

				sb_t dir = env_resolve("%XboxOneXDKLatest%..\\bin");
				sb_t path = { BB_EMPTY_INITIALIZER };
				sb_va(&path, "%s\\xbconfig.exe", sb_get(&dir));

				task subtask = process_task_create("xbconfig ConsoleType", kProcessSpawn_Tracked, sb_get(&dir), "\"%s\" ConsoleType", sb_get(&path));
				sdict_add_raw(&subtask.extraData, "key", "ConsoleType");
				subtask.stateChanged = task_xbconfig_statechanged;
				task_queue_subtask(t->parent, subtask);

				subtask = process_task_create("xbconfig HostName", kProcessSpawn_Tracked, sb_get(&dir), "\"%s\" HostName", sb_get(&path));
				sdict_add_raw(&subtask.extraData, "key", "HostName");
				subtask.stateChanged = task_xbconfig_statechanged;
				task_queue_subtask(t->parent, subtask);

				sb_reset(&dir);
				sb_reset(&path);
			}
		}
	}
}

static void task_durango_autodetect_group_statechanged(task *t)
{
	if(task_done(t)) {
		const char *gameAddr = sdict_find(&t->extraData, "GameAddr");
		const char *consoleType = sdict_find(&t->extraData, "ConsoleType");
		const char *hostName = sdict_find(&t->extraData, "HostName");
		if(gameAddr) {
			if(!consoleType) {
				consoleType = bb_platform_name(kBBPlatform_Durango);
			}
			devkit_autodetect_add(span_from_string(gameAddr), consoleType, hostName);
		}
	}
}

static void devkit_autodetect_durango_queue(task *groupTask)
{
	sb_t dir = env_resolve("%XboxOneXDKLatest%..\\bin");
	sb_t path = { BB_EMPTY_INITIALIZER };
	sb_va(&path, "%s\\xbconnect.exe", sb_get(&dir));

	if(file_readable(sb_get(&path))) {
		task durangoGroupTask = { BB_EMPTY_INITIALIZER };
		durangoGroupTask.parallel = true;
		durangoGroupTask.tick = task_tick_subtasks;
		durangoGroupTask.stateChanged = task_durango_autodetect_group_statechanged;
		sb_append(&durangoGroupTask.name, "durango_autodetect_group");

		task t = process_task_create("xbconnect", kProcessSpawn_Tracked, sb_get(&dir), "\"%s\" /S", sb_get(&path));
		t.stateChanged = task_xbconnect_statechanged;
		task_queue_subtask(&durangoGroupTask, t);
		task_queue_subtask(groupTask, durangoGroupTask);
	}

	sb_reset(&dir);
	sb_reset(&path);
}

//////////////////////////////////////////////////////////////////////////

static void task_devkit_autodetect_group_statechanged(task *t)
{
	if(task_done(t)) {
		devkit_autodetect_finish();
	}
}

void devkit_autodetect_tick(void)
{
	if(s_devkitAutodetectActive || s_lastDevkitAutodetectMillis && bb_current_time_ms() < s_lastDevkitAutodetectMillis + kDevkitAutodetectIntervalMillis)
		return;

	if(!g_site_config.autodetectDevkits)
		return;

	BB_TRACE(kBBLogLevel_Verbose, "Devkit", "Devkit autodetect BEGIN");

	if(!s_interfaceChangeHandle) {
		RegisterForInterfaceChanges();
	}

	task groupTask = { BB_EMPTY_INITIALIZER };
	groupTask.tick = task_tick_subtasks;
	sb_append(&groupTask.name, "devkit_autodetect_group");
	groupTask.parallel = true;

	s_devkitAutodetectActive = true;

	devkit_autodetect_orbis_queue(&groupTask);
	devkit_autodetect_durango_queue(&groupTask);

	groupTask.stateChanged = task_devkit_autodetect_group_statechanged;
	if(groupTask.subtasks.count) {
		task_queue(groupTask);
	} else {
		task_discard(groupTask);
		devkit_autodetect_finish();
	}
}
