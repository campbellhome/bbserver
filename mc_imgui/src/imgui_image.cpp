// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#define STB_IMAGE_IMPLEMENTATION
#include "imgui_image.h"
#include "bb_array.h"
#include "bb_malloc.h"
BB_WARNING_PUSH(4365 4820 4296 4619 5219)
#include "stb/stb_image.h"
BB_WARNING_POP
#include "wrap_dx11.h"

enum ImGui_Image_Flag : u32
{
	kImGui_Image_Dirty = 1,
	kImGui_Image_PendingDestroy = 2,
	kImGui_Image_PixelDataOwnership = 4,
};

struct UserImages
{
	u32 count;
	u32 allocated;
	UserImageData* data;
};

static ID3D11Device* g_pImageDevice;
static UserImages s_userImages;
static u32 s_lastUserId;

ImVec2 ImGui_Image_Constrain(const UserImageData& image, ImVec2 available)
{
	float availableRatio = available.x / available.y;
	float imageRatio = (float)image.width / (float)image.height;
	if (availableRatio > imageRatio)
	{
		// available space is wider, so we have extra space on the sides
		return ImVec2(imageRatio * available.y, available.y);
	}
	else if (availableRatio < imageRatio)
	{
		// available space is taller, so we have extra space on the top
		return ImVec2(available.x, available.x / imageRatio);
	}
	else
	{
		return available;
	}
}

UserImageData ImGui_Image_Get(UserImageId userId)
{
	for (u32 i = 0; i < s_userImages.count; ++i)
	{
		UserImageData* data = s_userImages.data + i;
		if (data->userId == userId)
		{
			return *data;
		}
	}
	UserImageData empty = { BB_EMPTY_INITIALIZER };
	return empty;
}

static UserImageData* ImGui_Image_Find(UserImageId userId)
{
	for (u32 i = 0; i < s_userImages.count; ++i)
	{
		UserImageData* data = s_userImages.data + i;
		if (data->userId == userId)
		{
			return data;
		}
	}
	return nullptr;
}

UserImageId ImGui_Image_CreateFromFile(const char* path)
{
	int width = 0;
	int height = 0;
	int channelsInFile = 0;
	u8* pixelData = stbi_load(path, &width, &height, &channelsInFile, 4);
	bb_log_external_alloc(pixelData);
	BB_LOG("Image", "Image %u %s %dx%d", s_lastUserId + 1, path, width, height);
	return ImGui_Image_Create(pixelData, width, height, kImGui_Image_PixelDataOwnership);
}

UserImageId ImGui_Image_Create(const u8* pixelData, int width, int height, u32 extraFlags)
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

void ImGui_Image_Modify(UserImageId userId, const u8* pixelData, int width, int height)
{
	UserImageData* data = ImGui_Image_Find(userId);
	if (data)
	{
		if (data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0)
		{
			bb_free((void*)data->pixelData);
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
	UserImageData* data = ImGui_Image_Find(userId);
	if (data)
	{
		if (data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0)
		{
			bb_free((void*)data->pixelData);
			data->flags &= (~kImGui_Image_PixelDataOwnership);
		}
		data->pixelData = nullptr;
		data->width = 0;
		data->height = 0;
		data->flags |= kImGui_Image_PendingDestroy;
	}
}

void ImGui_Image_InvalidateDeviceObject(UserImageData* data)
{
	if (!g_pImageDevice)
		return;

	if (data->texture)
	{
		data->texture->Release();
		data->texture = nullptr;
	}
}

void ImGui_Image_InvalidateDeviceObjects()
{
	for (u32 i = 0; i < s_userImages.count; ++i)
	{
		UserImageData* data = s_userImages.data + i;
		ImGui_Image_InvalidateDeviceObject(data);
	}
}

void ImGui_Image_CreateDeviceObject(UserImageData* data)
{
	if (data->texture)
	{
		ImGui_Image_InvalidateDeviceObject(data);
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = (UINT)data->width;
	desc.Height = (UINT)data->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = data->pixelData;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	g_pImageDevice->CreateTexture2D(&desc, &subResource, &pTexture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	if (g_pImageDevice->CreateShaderResourceView(pTexture, &srvDesc, &data->texture) >= 0)
	{
		data->flags &= ~kImGui_Image_Dirty;
	}
	pTexture->Release();
}

void ImGui_Image_CreateDeviceObjects()
{
	for (u32 i = 0; i < s_userImages.count; ++i)
	{
		UserImageData* data = s_userImages.data + i;
		if ((!data->texture || (data->flags & kImGui_Image_Dirty) != 0) && data->width && data->height)
		{
			ImGui_Image_CreateDeviceObject(data);
		}
	}
}

void ImGui_Image_NewFrame()
{
	for (u32 i = 0; i < s_userImages.count;)
	{
		UserImageData* data = s_userImages.data + i;
		if ((data->flags & kImGui_Image_PendingDestroy) == 0)
		{
			++i;
		}
		else
		{
			ImGui_Image_InvalidateDeviceObject(data);
			bba_erase(s_userImages, i);
		}
	}
	ImGui_Image_CreateDeviceObjects();
}

bool ImGui_Image_Init(ID3D11Device* device)
{
	ImGui_Image_InvalidateDeviceObjects();
	g_pImageDevice = device;
	return true;
}

void ImGui_Image_Shutdown()
{
	ImGui_Image_InvalidateDeviceObjects();
	for (u32 i = 0; i < s_userImages.count; ++i)
	{
		UserImageData* data = s_userImages.data + i;
		if (data->pixelData && (data->flags & kImGui_Image_PixelDataOwnership) != 0)
		{
			bb_free((void*)data->pixelData);
			data->flags &= (~kImGui_Image_PixelDataOwnership);
		}
		data->pixelData = nullptr;
	}
	bba_free(s_userImages);
	g_pImageDevice = nullptr;
}
