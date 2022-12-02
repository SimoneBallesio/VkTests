#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace VKP
{

	struct Texture
	{
		VkImage ImageHandle = VK_NULL_HANDLE;
		VkImageView ViewHandle = VK_NULL_HANDLE;
		VkSampler SamplerHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
		uint32_t MipLevels = 1;
	};
	
}