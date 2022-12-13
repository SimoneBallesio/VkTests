#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace VKP
{

	struct Buffer
	{
		VkBuffer BufferHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
		uint32_t Size = 0;
		uint32_t AlignedSize = 0;
	};

}