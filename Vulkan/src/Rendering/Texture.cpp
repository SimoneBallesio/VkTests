#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Buffer.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/State.hpp"

#include <AssetLibrary.hpp>

namespace VKP
{

	TextureCache* TextureCache::s_Instance = nullptr;
	std::unordered_map<std::string, Texture*> TextureCache::s_ResourceMap = {};

	TextureCache::~TextureCache()
	{
		for (auto& p : s_ResourceMap)
		{
			Impl::DestroyTexture(Impl::State::Data, p.second);
			delete p.second;
		}

		s_ResourceMap.clear();
	}

	Texture* TextureCache::Create(const std::string& name)
	{
		auto it = s_ResourceMap.find(name);

		if (it != s_ResourceMap.end())
			return (*it).second;

		Assets::Asset file;

		if (!Assets::LoadBinary(name.c_str(), file))
		{
			VKP_ERROR("Unable to load raw texture binary file ({})", name);
			return nullptr;
		}

		auto info = Assets::ParseTextureAssetInfo(&file);

		Buffer staging = {};
		Texture* tex = new Texture();

		if (!Impl::CreateBuffer(Impl::State::Data, &staging, info.PixelSize[0] * info.PixelSize[1] * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
		{
			VKP_ERROR("Unable to create staging buffer for texture upload");
			delete tex;
			return nullptr;
		}

		void* bufData = nullptr;
		vmaMapMemory(Impl::State::Data->MemAllocator, staging.MemoryHandle, &bufData);

		Assets::UnpackTexture(&info, file.Binary.data(), static_cast<uint8_t*>(bufData), file.Binary.size());

		vmaUnmapMemory(Impl::State::Data->MemAllocator, staging.MemoryHandle);

		if (!Impl::CreateImage(Impl::State::Data, tex, info.PixelSize[0], info.PixelSize[1], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
		{
			VKP_ERROR("Unable to create image object");
			vmaDestroyBuffer(Impl::State::Data->MemAllocator, staging.BufferHandle, staging.MemoryHandle);
			delete tex;
			return nullptr;
		}

		bool success = Impl::PopulateImage(Impl::State::Data, tex, &staging, info.PixelSize[0], info.PixelSize[1]);

		vmaDestroyBuffer(Impl::State::Data->MemAllocator, staging.BufferHandle, staging.MemoryHandle);

		if (success) success = Impl::CreateImageView(Impl::State::Data, tex, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		if (success) success = Impl::CreateImageSampler(Impl::State::Data, tex);

		if (!success)
		{
			if (tex->ImageHandle != VK_NULL_HANDLE)
				vmaDestroyImage(Impl::State::Data->MemAllocator, tex->ImageHandle, tex->MemoryHandle);

			if (tex->ViewHandle != VK_NULL_HANDLE)
				vkDestroyImageView(Impl::State::Data->Device, tex->ViewHandle, nullptr);

			if (tex->SamplerHandle != VK_NULL_HANDLE)
				vkDestroySampler(Impl::State::Data->Device, tex->SamplerHandle, nullptr);

			delete tex;
			return nullptr;
		}

		s_ResourceMap[name] = tex;
		tex->Path = std::move(name);

		return tex;
	}

	void TextureCache::Destroy(Texture* texture)
	{
		Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].DeletionQueue.Push([=]() {
			Impl::DestroyTexture(Impl::State::Data, texture);
		});

		s_ResourceMap.erase(texture->Path);
	}

	TextureCache* TextureCache::Create()
	{
		if (s_Instance == nullptr)
			s_Instance = new TextureCache();

		return s_Instance;
	}

	TextureCache& TextureCache::Get()
	{
		return *s_Instance;
	}

}

namespace VKP::Impl
{

	bool CreateImage(State* s, Texture* texture, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkSampleCountFlagBits samples)
	{
		texture->MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT || usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			texture->MipLevels = 1;

		VkImageCreateInfo imgInfo = {};
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.arrayLayers = 1;
		imgInfo.extent = { width, height, 1 };
		imgInfo.format = format;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imgInfo.mipLevels = texture->MipLevels;
		imgInfo.samples = samples;
		imgInfo.tiling = tiling;
		imgInfo.usage = usage;

		if (texture->MipLevels == 1)
			imgInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		GetTransferQueueData(s, &imgInfo.sharingMode, &imgInfo.queueFamilyIndexCount, &imgInfo.pQueueFamilyIndices);

		VmaAllocationCreateInfo memInfo = {};
		memInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		memInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		VkResult result = vmaCreateImage(s->MemAllocator, &imgInfo, &memInfo, &texture->ImageHandle, &texture->MemoryHandle, nullptr);
		VK_CHECK_RESULT(result);

		return VK_SUCCESS == result;
	}

	bool PopulateImage(State* s, Texture* texture, Buffer* staging, uint32_t width, uint32_t height)
	{
		const std::function<void(VkCommandBuffer)> fn = [&](VkCommandBuffer cmdBuffer)
		{
			VkBufferImageCopy region = {};
			region.bufferImageHeight = 0;
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.imageExtent = { width, height, 1 };
			region.imageOffset = { 0, 0, 0 };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.layerCount = 1;

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = texture->ImageHandle;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = texture->MipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			vkCmdCopyBufferToImage(cmdBuffer, staging->BufferHandle, texture->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			if (texture->MipLevels == 1)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				return;
			}

			int32_t mipW = width, mipH = height;

			for (size_t i = 1; i < texture->MipLevels; i++)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.subresourceRange.levelCount = 1; // One level at a time

				vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				VkImageBlit blit;
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipW, mipH, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.srcSubresource.mipLevel = i - 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				blit.dstSubresource.mipLevel = i;

				vkCmdBlitImage(cmdBuffer, texture->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				if (mipW > 1) mipW /= 2;
				if (mipH > 1) mipH /= 2;
			}

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.subresourceRange.baseMipLevel = texture->MipLevels - 1;

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		};

		return SubmitTransfer(s, fn);
	}

	bool CreateImageView(State* s, Texture* texture, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture->ImageHandle;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = texture->MipLevels;

		VkResult result = vkCreateImageView(s->Device, &viewInfo, nullptr, &texture->ViewHandle);
		VK_CHECK_RESULT(result);

		return VK_SUCCESS == result;
	}

	bool CreateImageSampler(State* s, Texture* texture)
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // Expands to 1000.0f

		if (vkCreateSampler(s->Device, &samplerInfo, nullptr, &texture->SamplerHandle) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create sampler for test texture");
			return false;
		}

		return true;
	}

	void DestroyTexture(State* s, Texture* texture)
	{
		if (texture->SamplerHandle != VK_NULL_HANDLE)
			vkDestroySampler(s->Device, texture->SamplerHandle, nullptr);

		if (texture->ViewHandle != VK_NULL_HANDLE)
			vkDestroyImageView(s->Device, texture->ViewHandle, nullptr);

		if (texture->ImageHandle != VK_NULL_HANDLE)
			vmaDestroyImage(s->MemAllocator, texture->ImageHandle, texture->MemoryHandle);
	}

}