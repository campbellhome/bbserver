// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"
#include "sb.h"
#include "site_config.h"

#include "bb_wrap_windows.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#if !(defined(_MSC_VER) && _MSC_VER)
typedef struct HWND__* HWND;
#endif

// mirror Windows POINT
AUTOJSON typedef struct configPoint_t {
	long x;
	long y;
} configPoint_t;

// mirror Windows RECT
AUTOJSON typedef struct configRect_t
{
	long left;
	long top;
	long right;
	long bottom;
} configRect_t;

// mirror Windows WINDOWPLACEMENT
AUTOJSON typedef struct configWindowplacement_t
{
	unsigned int length;
	unsigned int flags;
	unsigned int showCmd;
	configPoint_t ptMinPosition;
	configPoint_t ptMaxPosition;
	configRect_t rcNormalPosition;
} configWindowplacement_t;

AUTOJSON typedef struct configFont_t
{
	b32 enabled;
	u32 size;
	sb_t path;
} configFont_t;

AUTOJSON typedef struct configWhitelistEntry_s
{
	b32 allow;
	b32 autodetectedDevkit;
	u32 delay;
	u8 pad[4];
	sb_t addressPlusMask;
	sb_t applicationName;
	sb_t comment;
} configWhitelistEntry_t;
AUTOJSON typedef struct configWhitelist_s
{
	u32 count;
	u32 allocated;
	configWhitelistEntry_t* data;
} configWhitelist_t;

AUTOJSON typedef struct openTargetEntry_s
{
	sb_t displayName;
	sb_t commandLine;
} openTargetEntry_t;
AUTOJSON typedef struct openTargetList_s
{
	u32 count;
	u32 allocated;
	openTargetEntry_t* data;
} openTargetList_t;

AUTOJSON typedef struct pathFixupEntry_s
{
	sb_t src;
	sb_t dst;
} pathFixupEntry_t;
AUTOJSON typedef struct pathFixupList_s
{
	u32 count;
	u32 allocated;
	pathFixupEntry_t* data;
} pathFixupList_t;

AUTOJSON AUTODEFAULT(kConfigColors_Full) typedef enum tag_configColorUsage {
	kConfigColors_Full,
	kConfigColors_BgAsFg,
	kConfigColors_NoBg,
	kConfigColors_None,
	kConfigColors_Count
} configColorUsage;

AUTOJSON typedef struct tag_tooltipConfig
{
	b32 enabled;
	b32 overText;
	b32 overMisc;
	b32 onlyOverSelected;
	float delay;
	u8 pad[4];
} tooltipConfig;

AUTOJSON typedef struct tag_sizeConfig
{
	s32 resizeBarSize;
	s32 scrollbarSize;
} sizeConfig;

AUTOJSON AUTODEFAULT(kViewTileMode_Auto) typedef enum viewTileMode_t {
	kViewTileMode_Auto,
	kViewTileMode_PreferColumns,
	kViewTileMode_PreferRows,
	kViewTileMode_Columns,
	kViewTileMode_Rows,
	kViewTileMode_None,
	kViewTileMode_Count,
} viewTileMode_t;

AUTOJSON typedef struct config_named_filter_t
{
	sb_t name;
	sb_t text;
} config_named_filter_t;

AUTOJSON typedef struct config_named_filters_t
{
	u32 count;
	u32 allocated;
	config_named_filter_t* data;
} config_named_filters_t;

AUTOJSON typedef struct config_max_recordings_entry_t
{
	sb_t filter;
	u32 allowed;
	u8 pad[4];
} config_max_recordings_entry_t;

AUTOJSON typedef struct config_max_recordings_t
{
	u32 count;
	u32 allocated;
	config_max_recordings_entry_t* data;
} config_max_recordings_t;

AUTOJSON typedef struct config_s
{
	configWhitelist_t whitelist;
	openTargetList_t openTargets;
	pathFixupList_t pathFixups;
	config_named_filters_t namedFilters;
	config_max_recordings_t maxRecordings;
	configFont_t logFontConfig;
	configFont_t uiFontConfig;
	sb_t colorscheme;
	configWindowplacement_t wp;
	u32 version;
	viewTileMode_t viewTileMode;
	b32 alternateRowBackground;
	b32 textShadows;
	configColorUsage logColorUsage;
	tooltipConfig tooltips;
	sizeConfig sizes;
	b32 dpiAware;
	b32 dpiScrollwheel;
	u32 autoDeleteAfterDays;
	b32 autoCloseAll;
	b32 autoCloseManual;
	b32 updateManagement;
	float doubleClickSeconds;
	float dpiScale;
	b32 updateWaitForDebugger;
	b32 updatePauseAfterSuccessfulUpdate;
	b32 updatePauseAfterFailedUpdate;
	b32 assertMessageBox;
	b32 showDebugMenu;
	b32 showEmptyCategories;
	configListenProtocol_t listenProtocol;
	u8 pad[4];
} config_t;

enum
{
	kConfigVersion = 8
};

extern config_t g_config;

b32 config_read(config_t* config);
b32 config_write(config_t* config);
config_t config_clone(const config_t* config);
void config_reset(config_t* config);
void config_free(config_t* config);
void config_push_whitelist(configWhitelist_t* whitelist);
void whitelist_move_entry(configWhitelist_t* whitelist, u32 indexA, u32 indexB);
void config_validate_whitelist(configWhitelist_t* whitelist);
void open_target_move_entry(openTargetList_t* openTargets, u32 indexA, u32 indexB);
void config_validate_open_targets(openTargetList_t* openTargets);
void path_fixup_move_entry(pathFixupList_t* pathFixups, u32 indexA, u32 indexB);
void config_max_recordings_move_entry(config_max_recordings_t* arr, u32 indexA, u32 indexB);
u32 config_getwindowplacement(HWND hwnd);
float config_get_resizeBarSize(config_t* config);

#if defined(__cplusplus)
}
#endif
