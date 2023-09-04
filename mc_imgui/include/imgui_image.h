// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_dx9.h"
#include "wrap_imgui.h"

struct IDirect3DDevice9;

struct UserImageId
{
	u32 id = 0;
	bool operator==(const UserImageId& other) const { return id == other.id; }
};

struct UserImageData
{
	const u8* pixelData;
	LPDIRECT3DTEXTURE9 texture;
	int width;
	int height;
	UserImageId userId;
	u32 flags;
};

bool ImGui_Image_Init(IDirect3DDevice9* device);
void ImGui_Image_Shutdown();
void ImGui_Image_InvalidateDeviceObjects();
void ImGui_Image_NewFrame();

UserImageData ImGui_Image_Get(UserImageId userId);
ImVec2 ImGui_Image_Constrain(const UserImageData& image, ImVec2 available);
UserImageId ImGui_Image_CreateFromFile(const char* path);
UserImageId ImGui_Image_Create(const u8* pixelData, int width, int height, u32 extraFlags = 0);
void ImGui_Image_MarkForDestroy(UserImageId userId);
void ImGui_Image_Modify(UserImageId userId, const u8* pixelData, int width, int height);
