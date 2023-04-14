// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"

#if defined(__cplusplus)

void Fonts_MarkAtlasForRebuild(void);
bool Fonts_UpdateAtlas(void);
void Fonts_Menu(void);
void Fonts_InitFonts(void);

extern "C"
{
#endif

typedef struct json_value_t JSON_Value;

AUTOJSON typedef struct fontConfig_s
{
	b32 enabled;
	u32 size;
	sb_t path;
} fontConfig_t;

AUTOJSON typedef struct fontConfigs_s
{
	u32 count;
	u32 allocated;
	fontConfig_t* data;
} fontConfigs_t;
void fontConfig_reset(fontConfig_t* val);
fontConfig_t fontConfig_clone(const fontConfig_t* src);
void fontConfigs_reset(fontConfigs_t* val);
fontConfigs_t fontConfigs_clone(const fontConfigs_t* src);
fontConfig_t json_deserialize_fontConfig_t(JSON_Value* src);
fontConfigs_t json_deserialize_fontConfigs_t(JSON_Value* src);
JSON_Value* json_serialize_fontConfig_t(const fontConfig_t* src);
JSON_Value* json_serialize_fontConfigs_t(const fontConfigs_t* src);

void Fonts_ClearFonts(void);
void Fonts_AddFont(fontConfig_t font);

void Fonts_CacheGlyphs(const char* text);

sb_t Fonts_GetSystemFontDir(void);

void Fonts_Init(void);
void Fonts_Shutdown(void);

#if defined(__cplusplus)
}
#endif
