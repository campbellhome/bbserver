// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#include "fonts.h"
#include "bb_array.h"
#include "common.h"
#include "imgui_core.h"
#include "imgui_utils.h"
#include "json_utils.h"

#include "bb_wrap_windows.h"
#include <ShlObj.h>
#include <parson/parson.h>

extern "C" void fontConfig_reset(fontConfig_t* val)
{
	if (val)
	{
		sb_reset(&val->path);
	}
}
extern "C" fontConfig_t fontConfig_clone(const fontConfig_t* src)
{
	fontConfig_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		dst.enabled = src->enabled;
		dst.size = src->size;
		dst.path = sb_clone(&src->path);
	}
	return dst;
}

extern "C" void fontConfigs_reset(fontConfigs_t* val)
{
	if (val)
	{
		for (u32 i = 0; i < val->count; ++i)
		{
			fontConfig_reset(val->data + i);
		}
		bba_free(*val);
	}
}
extern "C" fontConfigs_t fontConfigs_clone(const fontConfigs_t* src)
{
	fontConfigs_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		for (u32 i = 0; i < src->count; ++i)
		{
			if (bba_add_noclear(dst, 1))
			{
				bba_last(dst) = fontConfig_clone(src->data + i);
			}
		}
	}
	return dst;
}

fontConfig_t json_deserialize_fontConfig_t(JSON_Value* src)
{
	fontConfig_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Object* obj = json_value_get_object(src);
		if (obj)
		{
			dst.enabled = json_object_get_boolean_safe(obj, "enabled");
			dst.size = (u32)json_object_get_number(obj, "size");
			dst.path = json_deserialize_sb_t(json_object_get_value(obj, "path"));
		}
	}
	return dst;
}

fontConfigs_t json_deserialize_fontConfigs_t(JSON_Value* src)
{
	fontConfigs_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Array* arr = json_value_get_array(src);
		if (arr)
		{
			for (u32 i = 0; i < json_array_get_count(arr); ++i)
			{
				bba_push(dst, json_deserialize_fontConfig_t(json_array_get_value(arr, i)));
			}
		}
	}
	return dst;
}

JSON_Value* json_serialize_fontConfig_t(const fontConfig_t* src)
{
	JSON_Value* val = json_value_init_object();
	JSON_Object* obj = json_value_get_object(val);
	if (obj)
	{
		json_object_set_boolean(obj, "enabled", src->enabled);
		json_object_set_number(obj, "size", src->size);
		json_object_set_value(obj, "path", json_serialize_sb_t(&src->path));
	}
	return val;
}

JSON_Value* json_serialize_fontConfigs_t(const fontConfigs_t* src)
{
	JSON_Value* val = json_value_init_array();
	JSON_Array* arr = json_value_get_array(val);
	if (arr)
	{
		for (u32 i = 0; i < src->count; ++i)
		{
			JSON_Value* child = json_serialize_fontConfig_t(src->data + i);
			if (child)
			{
				json_array_append_value(arr, child);
			}
		}
	}
	return val;
}

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

struct fontBuilder
{
#if BB_USING(FEATURE_FREETYPE)
	bool useFreeType = true;
#else  // #if BB_USING(FEATURE_FREETYPE)
	bool useFreeType = false;
#endif // #else // #if BB_USING(FEATURE_FREETYPE)
	bool rebuild = true;

	// Call _BEFORE_ NewFrame()
	bool UpdateRebuild()
	{
		if (!rebuild)
			return false;
		ImGuiIO& io = ImGui::GetIO();
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
// ImGui::Checkbox("DEBUG Text Shadows", &g_config.textShadows);
#if BB_USING(FEATURE_FREETYPE)
	if (Imgui_Core_Freetype_Valid())
	{
		if (ImGui::Checkbox("DEBUG Use FreeType", &s_fonts.useFreeType))
		{
			Fonts_MarkAtlasForRebuild();
		}
	}
#endif // #if BB_USING(FEATURE_FREETYPE)
}

static ImFontGlyphRangesBuilder s_glyphs;

static bool Glyphs_CacheText(ImFontGlyphRangesBuilder* glyphs, const char* text, const char* text_end = nullptr)
{
	bool result = false;
	while (text_end ? (text < text_end) : *text)
	{
		unsigned int c = 0;
		int c_len = ImTextCharFromUtf8(&c, text, text_end);
		text += c_len;
		if (c_len == 0)
			break;
		if (c <= IM_UNICODE_CODEPOINT_MAX)
		{
			if (!glyphs->GetBit((ImWchar)c))
			{
				glyphs->SetBit((ImWchar)c);
				result = true;
			}
		}
	}
	return result;
}

extern "C" void Fonts_CacheGlyphs(const char* text)
{
	if (Glyphs_CacheText(&s_glyphs, text))
	{
		Imgui_Core_QueueUpdateDpiDependentResources();
	}
}

void Fonts_GetGlyphRanges(ImVector<ImWchar>* glyphRanges)
{
	ImFontAtlas tmp;
	s_glyphs.AddRanges(tmp.GetGlyphRangesDefault());
	s_glyphs.BuildRanges(glyphRanges);
}

const ImWchar forkAwesomeRanges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 }; // must be static - from imgui: THE ARRAY DATA NEEDS TO PERSIST AS LONG AS THE FONT IS ALIVE.

static void Fonts_MergeIconFont(float fontSize)
{
	// merge in icons from Fork Awesome
	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig config;
	config.MergeMode = true;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryCompressedTTF(ForkAwesome_compressed_data, ForkAwesome_compressed_size, fontSize, &config, forkAwesomeRanges);
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

// const ImWchar fallbackRanges[] = { 0x2700, 0x2800, 0 };
// const ImWchar fallbackRanges[] = { 0x100, 0xFFFF, 0 };

// const ImWchar allRanges[] = {
//	0x0020, 0x00FF, // Basic Latin + Latin Supplement
//	0x0370, 0x03FF, // Greek and Coptic
//	0x2000, 0x206F, // General Punctuation
//	0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
//	0x3131, 0x3163, // Korean alphabets
//	0x31F0, 0x31FF, // Katakana Phonetic Extensions
//	0x4e00, 0x9FAF, // CJK Ideograms
//	0xAC00, 0xD7A3, // Korean characters
//	0xFF00, 0xFFEF, // Half-width characters
//	0xFFFD, 0xFFFD, // Invalid
//	0
// };

// const ImWchar fallbackRanges[] = {
//	0x0100, 0x036F, // between Basic Latin + Latin Supplement and Greek and Coptic
//	0x0400, 0x1FFF, // between Greek and Coptic and General Punctuation
//	0x2070, 0x2FFF, // between General Punctuation and CJK Symbols and Punctuations, Hiragana, Katakana
//	0x3100, 0x3130, // between CJK Symbols and Punctuations, Hiragana, Katakana and Korean alphabets
//	0x3164, 0x31EF, // between Korean alphabets and Katakana Phonetic Extensions
//	0x3200, 0x4DFF, // between Katakana Phonetic Extensions and CJK Ideograms
//	0x9FB0, 0xABFF, // between CJK Ideograms and Korean characters
//	0xD7A2, 0xFEFF, // between Korean characters and Half-width characters
//	0xFFEE, 0xFFFC, // between Half-width characters and Invalid
//	0
// };

void Fonts_InitFonts(void)
{
	static ImVector<ImWchar> glyphRanges;
	glyphRanges.clear();
	Fonts_GetGlyphRanges(&glyphRanges);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	if (s_fontConfigs.count < 1)
	{
		io.Fonts->AddFontDefault();
		Fonts_MergeIconFont(12.0f);
	}
	else
	{
		float dpiScale = Imgui_Core_GetDpiScale();
		for (u32 i = 0; i < s_fontConfigs.count; ++i)
		{
			fontConfig_t* fontConfig = s_fontConfigs.data + i;
			if (fontConfig->enabled && fontConfig->size > 0 && *sb_get(&fontConfig->path))
			{
				float fontSize = (float)fontConfig->size * dpiScale;
				io.Fonts->AddFontFromFileTTF(sb_get(&fontConfig->path), fontSize, nullptr, glyphRanges.Data);
				Fonts_MergeIconFont(fontSize);

				// io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\L_10646.ttf", fontSize, &config, fallbackRanges);
				// io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\SEGUIEMJ.ttf", fontSize, &mergeConfig, fallbackRanges);
			}
			else
			{
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
	if (SHGetKnownFolderPath(FOLDERID_Fonts, 0, NULL, &wpath) == S_OK)
	{
		sb_reserve(&dir, (u32)(wcslen(wpath) * 2));
		if (dir.data)
		{
			size_t numCharsConverted = 0;
			wcstombs_s(&numCharsConverted, dir.data, dir.allocated, wpath, _TRUNCATE);
			dir.count = (u32)strlen(dir.data) + 1;
		}
	}
	return dir;
}
