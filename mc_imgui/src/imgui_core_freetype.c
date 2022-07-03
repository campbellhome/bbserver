// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "imgui_core_freetype.h"

#if BB_USING(FEATURE_FREETYPE)

#include "bb_wrap_windows.h"

BB_WARNING_PUSH(4820)

#include <ft2build.h>

#include FT_FREETYPE_H  // <freetype/freetype.h>
#include FT_GLYPH_H     // <freetype/ftglyph.h>
#include FT_MODULE_H    // <freetype/ftmodapi.h>
#include FT_SYNTHESIS_H // <freetype/ftsynth.h>

BB_WARNING_POP

//#pragma comment(lib, "freetype.lib")

// freetype functions in use:
// FT_New_Memory_Face
// FT_Done_Face
// FT_Request_Size
// FT_Load_Glyph
// FT_Render_Glyph
// FT_Select_Charmap
// FT_Get_Char_Index
// FT_New_Library
// FT_Done_Library
// FT_Add_Default_Modules
// FT_GlyphSlot_Embolden
// FT_GlyphSlot_Oblique

// freetype function signatures:
// FT_Error FT_New_Memory_Face(FT_Library library, const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface);
// FT_Error FT_Done_Face(FT_Face face);
// FT_Error FT_Request_Size(FT_Face face, FT_Size_Request req);
// FT_Error FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags);
// FT_Error FT_Render_Glyph(FT_GlyphSlot slot, FT_Render_Mode render_mode);
// FT_Error FT_Select_Charmap(FT_Face face, FT_Encoding encoding);
// FT_UInt FT_Get_Char_Index(FT_Face face, FT_ULong charcode);
// FT_Error FT_New_Library(FT_Memory memory, FT_Library *alibrary);
// FT_Error FT_Done_Library(FT_Library library);
// void FT_Add_Default_Modules(FT_Library library);
// void FT_GlyphSlot_Embolden(FT_GlyphSlot slot);
// void FT_GlyphSlot_Oblique(FT_GlyphSlot slot);

typedef FT_Error(FT_New_Memory_Face_Proc)(FT_Library library, const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface);
typedef FT_Error(FT_Done_Face_Proc)(FT_Face face);
typedef FT_Error(FT_Request_Size_Proc)(FT_Face face, FT_Size_Request req);
typedef FT_Error(FT_Load_Glyph_Proc)(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags);
typedef FT_Error(FT_Render_Glyph_Proc)(FT_GlyphSlot slot, FT_Render_Mode render_mode);
typedef FT_Error(FT_Select_Charmap_Proc)(FT_Face face, FT_Encoding encoding);
typedef FT_UInt(FT_Get_Char_Index_Proc)(FT_Face face, FT_ULong charcode);
typedef FT_Error(FT_New_Library_Proc)(FT_Memory memory, FT_Library *alibrary);
typedef FT_Error(FT_Done_Library_Proc)(FT_Library library);
typedef void(FT_Add_Default_Modules_Proc)(FT_Library library);
typedef void(FT_GlyphSlot_Embolden_Proc)(FT_GlyphSlot slot);
typedef void(FT_GlyphSlot_Oblique_Proc)(FT_GlyphSlot slot);

static HMODULE g_hFreetypeModule;
static b32 g_freetypeValid;
static FT_New_Memory_Face_Proc *g_FT_New_Memory_Face;
static FT_Done_Face_Proc *g_FT_Done_Face;
static FT_Request_Size_Proc *g_FT_Request_Size;
static FT_Load_Glyph_Proc *g_FT_Load_Glyph;
static FT_Render_Glyph_Proc *g_FT_Render_Glyph;
static FT_Select_Charmap_Proc *g_FT_Select_Charmap;
static FT_Get_Char_Index_Proc *g_FT_Get_Char_Index;
static FT_New_Library_Proc *g_FT_New_Library;
static FT_Done_Library_Proc *g_FT_Done_Library;
static FT_Add_Default_Modules_Proc *g_FT_Add_Default_Modules;
static FT_GlyphSlot_Embolden_Proc *g_FT_GlyphSlot_Embolden;
static FT_GlyphSlot_Oblique_Proc *g_FT_GlyphSlot_Oblique;

FT_Error FT_New_Memory_Face(FT_Library library, const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface)
{
	return (*g_FT_New_Memory_Face)(library, file_base, file_size, face_index, aface);
}
FT_Error FT_Done_Face(FT_Face face)
{
	return (*g_FT_Done_Face)(face);
}
FT_Error FT_Request_Size(FT_Face face, FT_Size_Request req)
{
	return (*g_FT_Request_Size)(face, req);
}
FT_Error FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags)
{
	return (*g_FT_Load_Glyph)(face, glyph_index, load_flags);
}
FT_Error FT_Render_Glyph(FT_GlyphSlot slot, FT_Render_Mode render_mode)
{
	return (*g_FT_Render_Glyph)(slot, render_mode);
}
FT_Error FT_Select_Charmap(FT_Face face, FT_Encoding encoding)
{
	return (*g_FT_Select_Charmap)(face, encoding);
}
FT_UInt FT_Get_Char_Index(FT_Face face, FT_ULong charcode)
{
	return (*g_FT_Get_Char_Index)(face, charcode);
}
FT_Error FT_New_Library(FT_Memory memory, FT_Library *alibrary)
{
	return (*g_FT_New_Library)(memory, alibrary);
}
FT_Error FT_Done_Library(FT_Library library)
{
	return (*g_FT_Done_Library)(library);
}
void FT_Add_Default_Modules(FT_Library library)
{
	(*g_FT_Add_Default_Modules)(library);
}
void FT_GlyphSlot_Embolden(FT_GlyphSlot slot)
{
	(*g_FT_GlyphSlot_Embolden)(slot);
}
void FT_GlyphSlot_Oblique(FT_GlyphSlot slot)
{
	(*g_FT_GlyphSlot_Oblique)(slot);
}

void Imgui_Core_Freetype_Init(void)
{
	g_hFreetypeModule = LoadLibraryA("freetype.dll");
	if(g_hFreetypeModule) {
		g_FT_New_Memory_Face = (FT_New_Memory_Face_Proc *)GetProcAddress(g_hFreetypeModule, "FT_New_Memory_Face");
		g_FT_Done_Face = (FT_Done_Face_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Done_Face");
		g_FT_Request_Size = (FT_Request_Size_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Request_Size");
		g_FT_Load_Glyph = (FT_Load_Glyph_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Load_Glyph");
		g_FT_Render_Glyph = (FT_Render_Glyph_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Render_Glyph");
		g_FT_Select_Charmap = (FT_Select_Charmap_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Select_Charmap");
		g_FT_Get_Char_Index = (FT_Get_Char_Index_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Get_Char_Index");
		g_FT_New_Library = (FT_New_Library_Proc *)GetProcAddress(g_hFreetypeModule, "FT_New_Library");
		g_FT_Done_Library = (FT_Done_Library_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Done_Library");
		g_FT_Add_Default_Modules = (FT_Add_Default_Modules_Proc *)GetProcAddress(g_hFreetypeModule, "FT_Add_Default_Modules");
		g_FT_GlyphSlot_Embolden = (FT_GlyphSlot_Embolden_Proc *)GetProcAddress(g_hFreetypeModule, "FT_GlyphSlot_Embolden");
		g_FT_GlyphSlot_Oblique = (FT_GlyphSlot_Oblique_Proc *)GetProcAddress(g_hFreetypeModule, "FT_GlyphSlot_Oblique");

		g_freetypeValid = g_FT_New_Memory_Face &&
		                  g_FT_Done_Face &&
		                  g_FT_Request_Size &&
		                  g_FT_Load_Glyph &&
		                  g_FT_Render_Glyph &&
		                  g_FT_Select_Charmap &&
		                  g_FT_Get_Char_Index &&
		                  g_FT_New_Library &&
		                  g_FT_Done_Library &&
		                  g_FT_Add_Default_Modules &&
		                  g_FT_GlyphSlot_Embolden &&
		                  g_FT_GlyphSlot_Oblique;
	}
}

void Imgui_Core_Freetype_Shutdown(void)
{
	if(g_hFreetypeModule) {
		FreeLibrary(g_hFreetypeModule);
		g_hFreetypeModule = NULL;
		g_freetypeValid = false;
	}
}

b32 Imgui_Core_Freetype_Valid(void)
{
	return g_freetypeValid;
}

#else // #if BB_USING(FEATURE_FREETYPE)

void Imgui_Core_Freetype_Init(void)
{
}

void Imgui_Core_Freetype_Shutdown(void)
{
}

b32 Imgui_Core_Freetype_Valid(void)
{
	return false;
}

#endif // #else // #if BB_USING(FEATURE_FREETYPE)
