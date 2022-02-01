// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#define STB_IMAGE_IMPLEMENTATION
#include "imgui_image.h"
#include "bb_array.h"
BB_WARNING_PUSH(4365 4820 4296 4619 5219)
#include "stb/stb_image.h"
BB_WARNING_POP
#include <d3d9.h>

enum ImGui_Image_Flag : u32 {
	kImGui_Image_Dirty = 1,
	kImGui_Image_PendingDestroy = 2,
	kImGui_Image_PixelDataOwnership = 4,
};

struct UserImages {
	u32 count;
	u32 allocated;
	UserImageData *data;
};

static LPDIRECT3DDEVICE9 g_pImageDevice;
static UserImages s_userImages;
static u32 s_lastUserId;

ImVec2 ImGui_Image_Constrain(const UserImageData &image, ImVec2 available)
{
	float availableRatio = available.x / available.y;
	float imageRatio = (float)image.width / (float)image.height;
	if(availableRatio > imageRatio) {
		// available space is wider, so we have extra space on the sides
		return ImVec2(imageRatio * available.y, available.y);
	} else if(availableRatio < imageRatio) {
		// available space is taller, so we have extra space on the top
		return ImVec2(available.x, available.x / imageRatio);
	} else {
		return available;
	}
}

UserImageData ImGui_Image_Get(UserImageId userId)
{
	for(u32 i = 0; i < s_userImages.count; ++i) {
		UserImageData *data = s_userImages.data + i;
		if(data->userId == userId) {
			return *data;
		}
	}
	UserImageData empty = { BB_EMPTY_INITIALIZER };
	return empty;
}

static UserImageData *ImGui_Image_Find(UserImageId userId)
{
	for(u32 i = 0; i < s_userImages.count; ++i) {
		UserImageData *data = s_userImages.data + i;
		if(data->userId == userId) {
			return data;
		}
	}
	return nullptr;
}

UserImageId ImGui_Image_CreateFromFile(const char *path)
{
	int width = 0;
	int height = 0;
	int channelsInFile = 0;
	u8 *pixelData = stbi_load(path, &width, &height, &channelsInFile, 4);
	if(pixelData) {
		for(s64 i = 0; i < width * height; ++i) {
			u8 *pixel = pixelData + 4 * i;
			u8 tmp = pixel[0];
			pixel[0] = pixel[2];
			pixel[2] = tmp;
		}
	}
	BB_LOG("Image", "Image %u %s %dx%d", s_lastUserId + 1, path, width, height);
	return ImGui_Image_Create(pixelData, width, height, kImGui_Image_PixelDataOwnership);
}

UserImageId ImGui_Image_Create(const u8 *pixelData, int width, int height, u32 extraFlags)
{
	UserImageId userId = { ++s_lastUserId };
	UserImageData data = { BB_EMPTY_INITIALIZER };
	data.userId = userId;
	data.width = width;
	data.height = height;
	data.pixelData = pixelData;
	data.flags = kImGui_Image_Dirty | extraFlags;
	bba_push(s_userImages, data);
	return userId;
}

void ImGui_Image_Modify(UserImageId userId, const u8 *pixelData, int width, int height)
{
	UserImageData *data = ImGui_Image_Find(userId);
	if(data) {
		if(data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0) {
			free((void *)data->pixelData);
			data->flags &= (~kImGui_Image_PixelDataOwnership);
		}
		data->pixelData = pixelData;
		data->width = width;
		data->height = height;
		data->flags |= kImGui_Image_Dirty;
	}
}

void ImGui_Image_MarkForDestroy(UserImageId userId)
{
	UserImageData *data = ImGui_Image_Find(userId);
	if(data) {
		if(data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0) {
			free((void *)data->pixelData);
			data->flags &= (~kImGui_Image_PixelDataOwnership);
		}
		data->pixelData = nullptr;
		data->width = 0;
		data->height = 0;
		data->flags |= kImGui_Image_PendingDestroy;
	}
}

void ImGui_Image_InvalidateDeviceObject(UserImageData *data)
{
	if(!g_pImageDevice)
		return;

	if(data->texture) {
		data->texture->Release();
		data->texture = nullptr;
	}
}

void ImGui_Image_InvalidateDeviceObjects()
{
	for(u32 i = 0; i < s_userImages.count; ++i) {
		UserImageData *data = s_userImages.data + i;
		ImGui_Image_InvalidateDeviceObject(data);
	}
}

void ImGui_Image_CreateDeviceObject(UserImageData *data)
{
	if(data->texture) {
		ImGui_Image_InvalidateDeviceObject(data);
	}

	if(g_pImageDevice->CreateTexture((UINT)data->width, (UINT)data->height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &data->texture, nullptr) >= 0) {
		D3DLOCKED_RECT lockedRect;
		if(data->texture->LockRect(0, &lockedRect, NULL, 0) == D3D_OK) {
			for(s64 y = 0; y < data->height; ++y) {
				memcpy((unsigned char *)lockedRect.pBits + lockedRect.Pitch * y, data->pixelData + (data->width * 4) * y, (size_t)(data->width * 4));
			}
			data->texture->UnlockRect(0);
			data->flags &= ~kImGui_Image_Dirty;
		}
	}
}

void ImGui_Image_CreateDeviceObjects()
{
	for(u32 i = 0; i < s_userImages.count; ++i) {
		UserImageData *data = s_userImages.data + i;
		if((!data->texture || (data->flags & kImGui_Image_Dirty) != 0) && data->width && data->height) {
			ImGui_Image_CreateDeviceObject(data);
		}
	}
}

void ImGui_Image_NewFrame()
{
	for(u32 i = 0; i < s_userImages.count;) {
		UserImageData *data = s_userImages.data + i;
		if((data->flags & kImGui_Image_PendingDestroy) == 0) {
			++i;
		} else {
			ImGui_Image_InvalidateDeviceObject(data);
			bba_erase(s_userImages, i);
		}
	}
	ImGui_Image_CreateDeviceObjects();
}

bool ImGui_Image_Init(IDirect3DDevice9 *device)
{
	ImGui_Image_InvalidateDeviceObjects();
	g_pImageDevice = device;
	return true;
}

void ImGui_Image_Shutdown()
{
	ImGui_Image_InvalidateDeviceObjects();
	for(u32 i = 0; i < s_userImages.count; ++i) {
		UserImageData *data = s_userImages.data + i;
		if(data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0) {
			free((void *)data->pixelData);
			data->flags &= (~kImGui_Image_PixelDataOwnership);
		}
		data->pixelData = nullptr;
	}
	bba_free(s_userImages);
	g_pImageDevice = nullptr;
}
