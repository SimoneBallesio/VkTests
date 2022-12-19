#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Texture.hpp"

#include <stb_image.h>

namespace VKP
{

	std::unordered_map<std::string, Texture*> Texture::s_ResourceMap = {};

	Texture* Texture::Load(const std::string& path)
	{
		auto it = s_ResourceMap.find(path);

		if (it != s_ResourceMap.end())
			return (*it).second;

		int w = 0, h = 0, nrChannels = 0;
		uint8_t* data = stbi_load(path.c_str(), &w, &h, &nrChannels, STBI_rgb_alpha);

		if (data == nullptr)
		{
			VKP_ERROR("Unable to locate texture file");
			return nullptr;
		}

		auto alloc = Context::GetMemoryAllocator();
		Buffer staging = {};
		Texture* tex = new Texture(path);

		if (!Context::Get().CreateBuffer(staging, w * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
		{
			VKP_ERROR("Unable to create staging buffer for texture upload");
			stbi_image_free(data);
			return nullptr;
		}

		void* bufData = nullptr;
		vmaMapMemory(alloc, staging.MemoryHandle, &bufData);
		memcpy(bufData, data, w * h * 4);
		vmaUnmapMemory(alloc, staging.MemoryHandle);

		stbi_image_free(data);

		if (!Context::Get().CreateImage(*tex, w, h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
		{
			VKP_ERROR("Unable to create image object");
			vmaDestroyBuffer(alloc, staging.BufferHandle, staging.MemoryHandle);
			delete tex;
			return nullptr;
		}

		bool success = Context::Get().PopulateImage(*tex, staging, w, h);

		vmaDestroyBuffer(alloc, staging.BufferHandle, staging.MemoryHandle);

		if (success) success = Context::Get().CreateImageView(*tex, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		if (success) success = Context::Get().CreateImageSampler(*tex);

		if (!success)
		{
			if (tex->ImageHandle != VK_NULL_HANDLE)
				vmaDestroyImage(alloc, tex->ImageHandle, tex->MemoryHandle);

			if (tex->ViewHandle != VK_NULL_HANDLE)
				vkDestroyImageView(Context::GetDevice(), tex->ViewHandle, nullptr);

			if (tex->SamplerHandle != VK_NULL_HANDLE)
				vkDestroySampler(Context::GetDevice(), tex->SamplerHandle, nullptr);

			delete tex;
			return nullptr;
		}

		s_ResourceMap[path] = tex;
		return tex;
	}

	void Texture::Destroy(Texture* texture)
	{
		Context::Get().DestroyTexture(*texture);
		s_ResourceMap.erase(texture->Path);
		delete texture;
	}

}