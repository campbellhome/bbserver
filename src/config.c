// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#include "config.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_malloc.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "common.h"
#include "env_utils.h"
#include "file_utils.h"
#include "fonts.h"
#include "line_parser.h"
#include "process_task.h"
#include "sb.h"
#include "site_config.h"
#include "span.h"
#include "tokenize.h"
#include "va.h"
#include <stdlib.h>

BB_CTASSERT(sizeof(WINDOWPLACEMENT) == sizeof(configWindowplacement_t));

config_t g_config;

u32 config_getwindowplacement(HWND hwnd)
{
	UINT oldShowCmd = g_config.wp.showCmd;
	memset(&g_config.wp, 0, sizeof(g_config.wp));
	g_config.wp.length = sizeof(g_config.wp);
	GetWindowPlacement(hwnd, (WINDOWPLACEMENT*)&g_config.wp);
	if (g_config.wp.showCmd == SW_SHOWMINIMIZED)
	{
		g_config.wp.showCmd = oldShowCmd;
		return SW_SHOWMINIMIZED;
	}
	else
	{
		return g_config.wp.showCmd;
	}
}

void config_free(config_t* config)
{
	config_reset(config);
	bb_free(config);
}

static sb_t config_get_path(const char* appName)
{
	sb_t s = appdata_get(appName);
	sb_append(&s, "\\bb_config.json");
	return s;
}

b32 config_read(config_t* config)
{
	b32 ret = false;
	sb_t path = config_get_path("bb");
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		*config = json_deserialize_config_t(val);
		json_value_free(val);
		ret = true;
	}
	sb_reset(&path);

	if (config->wp.showCmd == SW_SHOWMINIMIZED ||
	    config->wp.showCmd == SW_MINIMIZE ||
	    config->wp.showCmd == SW_SHOWMINNOACTIVE ||
	    config->wp.showCmd == SW_FORCEMINIMIZE)
	{
		config->wp.showCmd = SW_NORMAL;
	}
	if (config->version == 0)
	{
		config->alternateRowBackground = true;
		config->logColorUsage = kConfigColors_Full;
		config->autoDeleteAfterDays = 14;
		config->doubleClickSeconds = 0.3f;
		config->dpiAware = true;
		config->dpiScale = 1.0f;
		config->uiFontConfig.enabled = true;
		config->uiFontConfig.size = 16;
		config->uiFontConfig.path = Fonts_GetSystemFontDir();
		sb_append(&config->uiFontConfig.path, "\\verdana.ttf");
		config->logFontConfig.enabled = true;
		config->logFontConfig.size = 14;
		config->logFontConfig.path = Fonts_GetSystemFontDir();
		sb_append(&config->logFontConfig.path, "\\consola.ttf");
		sb_append(&config->colorscheme, "ImGui Dark");
	}
	if (config->version <= 1)
	{
		config->tooltips.enabled = true;
		config->tooltips.onlyOverSelected = false;
		config->tooltips.delay = 0.3f;
	}
	if (config->version <= 2)
	{
		config->updatePauseAfterFailedUpdate = true;
	}
	if (config->version <= 3)
	{
		config->logFontConfig.enabled = true;
	}
	if (config->version <= 4)
	{
		config->textShadows = true;
	}
	if (config->version <= 5)
	{
		config->sizes.resizeBarSize = 0;
		config->sizes.scrollbarSize = 0;
	}
	if (config->version <= 6)
	{
		config->tooltips.overText = false;
		config->tooltips.overMisc = true;
	}
	if (config->version <= 7)
	{
		config->dpiScrollwheel = true;
	}
	config->version = kConfigVersion;

	for (u32 entryIndex = 0; entryIndex < config->whitelist.count;)
	{
		configWhitelistEntry_t* entry = config->whitelist.data + entryIndex;
		b32 bRemove = false;
		if (entry->autodetectedDevkit)
		{
			int numDots = 0;
			int numColons = 0;
			b32 validChars = true;
			size_t len = strlen(sb_get(&entry->addressPlusMask));
			for (size_t i = 0; i < len; ++i)
			{
				if (entry->addressPlusMask.data[i] == '.')
				{
					++numDots;
				}
				else if (entry->addressPlusMask.data[i] == ':')
				{
					++numColons;
				}
				else if ((entry->addressPlusMask.data[i] < '0' || entry->addressPlusMask.data[i] > '9') &&
				         (entry->addressPlusMask.data[i] < 'a' || entry->addressPlusMask.data[i] > 'f'))
				{
					validChars = false;
					break;
				}
			}
			if ((numDots != 3 && numColons < 2) || !validChars)
			{
				bRemove = true;
			}
			else if (!g_site_config.autodetectDevkits)
			{
				bRemove = true;
			}
		}
		if (bRemove)
		{
			BB_WARNING("config::read", "removing invalid devkit '%s' '%s'", sb_get(&entry->addressPlusMask), sb_get(&entry->comment));
			configWhitelistEntry_reset(config->whitelist.data + entryIndex);
			bba_erase(config->whitelist, entryIndex);
		}
		else
		{
			++entryIndex;
		}
	}

	return ret;
}

b32 config_write(config_t* config)
{
	b32 result = false;
	JSON_Value* val = json_serialize_config_t(config);
	if (val)
	{
		sb_t path = config_get_path("bb");
		FILE* fp = fopen(sb_get(&path), "wb");
		if (fp)
		{
			char* serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
			result = true;
		}
		sb_reset(&path);
	}
	json_value_free(val);
	return result;
}

void whitelist_move_entry(configWhitelist_t* whitelist, u32 indexA, u32 indexB)
{
	if (indexA < whitelist->count &&
	    indexB < whitelist->count &&
	    indexA != indexB)
	{
		configWhitelistEntry_t* entryA = whitelist->data + indexA;
		configWhitelistEntry_t* entryB = whitelist->data + indexB;
		configWhitelistEntry_t tmp = *entryA;
		*entryA = *entryB;
		*entryB = tmp;
	}
}

void config_validate_whitelist(configWhitelist_t* whitelist)
{
	if (!whitelist->count)
	{
		configWhitelistEntry_t* entry = bba_add(*whitelist, 1);
		if (entry)
		{
			entry->allow = true;
			sb_append(&entry->addressPlusMask, "localhost");
		}
	}
}

void open_target_move_entry(openTargetList_t* openTargets, u32 indexA, u32 indexB)
{
	if (indexA < openTargets->count &&
	    indexB < openTargets->count &&
	    indexA != indexB)
	{
		openTargetEntry_t* entryA = openTargets->data + indexA;
		openTargetEntry_t* entryB = openTargets->data + indexB;
		openTargetEntry_t tmp = *entryA;
		*entryA = *entryB;
		*entryB = tmp;
	}
}

void task_vswhere_statechanged(task* t)
{
	task_process_statechanged(t);
	if (t->state == kTaskState_Succeeded)
	{
		task_process* p = t->taskData;
		if (p->process)
		{
			const char* cursor = p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "";
			span_t path = tokenize(&cursor, "\r\n");
			if (path.start)
			{
				openTargetEntry_t* entry = bba_add(g_config.openTargets, 1);
				if (entry)
				{
					sb_append(&entry->displayName, "Open in Visual Studio");
					sb_va(&entry->commandLine, "\"%.*s\" /edit \"{File}\" /command \"edit.goto {Line}\"", (path.end - path.start), path.start);
					BB_LOG("config::vswhere", "Added 'Open in Visual Studio' entry:\n%s", sb_get(&entry->commandLine));
				}
			}
		}
	}
}

void config_validate_open_targets(openTargetList_t* openTargets)
{
	if (!openTargets->count)
	{
		sb_t cmdline = env_resolve("\"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -nologo -latest -property productPath");
		task t = process_task_create("vswhere", kProcessSpawn_Tracked, "C:\\", "%s", sb_get(&cmdline));
		t.stateChanged = task_vswhere_statechanged;
		task_queue(t);
		sb_reset(&cmdline);
	}
}

void path_fixup_move_entry(pathFixupList_t* pathFixups, u32 indexA, u32 indexB)
{
	if (indexA < pathFixups->count &&
	    indexB < pathFixups->count &&
	    indexA != indexB)
	{
		pathFixupEntry_t* entryA = pathFixups->data + indexA;
		pathFixupEntry_t* entryB = pathFixups->data + indexB;
		pathFixupEntry_t tmp = *entryA;
		*entryA = *entryB;
		*entryB = tmp;
	}
}

float config_get_resizeBarSize(config_t* config)
{
	return ((config->sizes.resizeBarSize > 0) ? config->sizes.resizeBarSize : 3) * config->dpiScale;
}
