// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "fonts.h"
#include "bb_array.h"
#include "common.h"
#include "imgui_core.h"
#include "imgui_utils.h"

#include "bb_wrap_windows.h"
#include <ShlObj.h>

// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4365: '=': conversion from 'ImGuiTabItemFlags' to 'ImGuiID', signed/unsigned mismatch
BB_WARNING_PUSH(4820, 4365)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
BB_WARNING_POP

extern "C" fontConfig_t fontConfig_clone(fontConfig_t *src);
extern "C" void fontConfigs_reset(fontConfigs_t *val);

static fontConfigs_t s_fontConfigs;

#include "forkawesome-webfont.inc"

#if BB_USING(FEATURE_FREETYPE)

// warning C4548: expression before comma has no effect; expected expression with side-effect
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
// warning C4668: '_WIN32_WINNT_WINTHRESHOLD' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
// warning C4574: 'INCL_WINSOCK_API_TYPEDEFS' is defined to be '0': did you mean to use '#if INCL_WINSOCK_API_TYPEDEFS'?
// warning C4365: 'return': conversion from 'bool' to 'BOOLEAN', signed/unsigned mismatch
BB_WARNING_PUSH(4820 4255 4668 4574 4365 4619 5219)
#include "misc/freetype/imgui_freetype.cpp"
BB_WARNING_POP

#endif // #if BB_USING(FEATURE_FREETYPE)

extern "C" void Fonts_Shutdown(void)
{
	fontConfigs_reset(&s_fontConfigs);
}

struct fontBuilder {
#if BB_USING(FEATURE_FREETYPE)
	bool useFreeType = true;
#else // #if BB_USING(FEATURE_FREETYPE)
	bool useFreeType = false;
#endif // #else // #if BB_USING(FEATURE_FREETYPE)
	bool rebuild = true;

	// Call _BEFORE_ NewFrame()
	bool UpdateRebuild()
	{
		if(!rebuild)
			return false;
		ImGuiIO &io = ImGui::GetIO();
		io.Fonts->Build();
		rebuild = false;
		return true;
	}
};

static fontBuilder s_fonts;
void Fonts_MarkAtlasForRebuild(void)
{
	s_fonts.rebuild = true;
}

bool Fonts_UpdateAtlas(void)
{
	return s_fonts.UpdateRebuild();
}

void Fonts_Menu(void)
{
//ImGui::Checkbox("DEBUG Text Shadows", &g_config.textShadows);
#if BB_USING(FEATURE_FREETYPE)
	if(Imgui_Core_Freetype_Valid()) {
		if(ImGui::Checkbox("DEBUG Use FreeType", &s_fonts.useFreeType)) {
			Fonts_MarkAtlasForRebuild();
		}
	}
#endif // #if BB_USING(FEATURE_FREETYPE)
}

static ImFontGlyphRangesBuilder s_glyphs;

static bool Glyphs_CacheText(ImFontGlyphRangesBuilder *glyphs, const char *text, const char *text_end = nullptr)
{
	bool result = false;
	while(text_end ? (text < text_end) : *text) {
		unsigned int c = 0;
		int c_len = ImTextCharFromUtf8(&c, text, text_end);
		text += c_len;
		if(c_len == 0)
			break;
		if(c <= IM_UNICODE_CODEPOINT_MAX) {
			if(!glyphs->GetBit((ImWchar)c)) {
				glyphs->SetBit((ImWchar)c);
				result = true;
			}
		}
	}
	return result;
}

extern "C" void Fonts_CacheGlyphs(const char *text)
{
	if(Glyphs_CacheText(&s_glyphs, text)) {
		Imgui_Core_QueueUpdateDpiDependentResources();
	}
}

void Fonts_GetGlyphRanges(ImVector< ImWchar > *glyphRanges)
{
	ImFontAtlas tmp;
	s_glyphs.AddRanges(tmp.GetGlyphRangesDefault());
	s_glyphs.BuildRanges(glyphRanges);
}

static void Fonts_MergeIconFont(float fontSize)
{
	// merge in icons from Fork Awesome
	ImGuiIO &io = ImGui::GetIO();
	static const ImWchar ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
	ImFontConfig config;
	config.MergeMode = true;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryCompressedTTF(ForkAwesome_compressed_data, ForkAwesome_compressed_size,
	                                         fontSize, &config, ranges);
	//io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, 16.0f, &icons_config, icons_ranges);
}

extern "C" void Fonts_ClearFonts(void)
{
	fontConfigs_reset(&s_fontConfigs);
	Imgui_Core_QueueUpdateDpiDependentResources();
}

extern "C" void Fonts_AddFont(fontConfig_t font)
{
	bba_push(s_fontConfigs, fontConfig_clone(&font));
	Imgui_Core_QueueUpdateDpiDependentResources();
	Imgui_Core_QueueUpdateDpiDependentResources();
}

void Fonts_Init(void)
{
#if BB_USING(FEATURE_FREETYPE)
	s_fonts.useFreeType = Imgui_Core_Freetype_Valid() != 0;
#endif
}

void Fonts_InitFonts(void)
{
	static ImVector< ImWchar > glyphRanges;
	glyphRanges.clear();
	Fonts_GetGlyphRanges(&glyphRanges);

	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();

	if(s_fontConfigs.count < 1) {
		io.Fonts->AddFontDefault();
		Fonts_MergeIconFont(12.0f);
	} else {
		float dpiScale = Imgui_Core_GetDpiScale();
		for(u32 i = 0; i < s_fontConfigs.count; ++i) {
			fontConfig_t *fontConfig = s_fontConfigs.data + i;
			if(fontConfig->enabled && fontConfig->size > 0 && *sb_get(&fontConfig->path)) {
				io.Fonts->AddFontFromFileTTF(sb_get(&fontConfig->path), (float)fontConfig->size * dpiScale, nullptr, glyphRanges.Data);
				Fonts_MergeIconFont((float)fontConfig->size * dpiScale);
			} else {
				io.Fonts->AddFontDefault();
				Fonts_MergeIconFont(12.0f);
			}
		}
	}

	Fonts_MarkAtlasForRebuild();
}

extern "C" sb_t Fonts_GetSystemFontDir(void)
{
	sb_t dir = { BB_EMPTY_INITIALIZER };
	PWSTR wpath;
	if(SHGetKnownFolderPath(FOLDERID_Fonts, 0, NULL, &wpath) == S_OK) {
		sb_reserve(&dir, (u32)(wcslen(wpath) * 2));
		if(dir.data) {
			size_t numCharsConverted = 0;
			wcstombs_s(&numCharsConverted, dir.data, dir.allocated, wpath, _TRUNCATE);
			dir.count = (u32)strlen(dir.data) + 1;
		}
	}
	return dir;
}
