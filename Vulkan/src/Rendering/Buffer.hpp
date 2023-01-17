#pragma once

#include "Rendering/VertexData.hpp"

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

namespace VKP::Impl
{

	struct State;

	bool CreateBuffer(State* s, Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags);
	bool CopyBuffer(State* s, const Buffer* src, const Buffer* dst, VkDeviceSize size);
	bool CreateVertexBuffer(State* s, Buffer* vbo, const std::vector<Vertex>& vertices);
	bool CreateIndexBuffer(State* s, Buffer* ibo, const std::vector<uint32_t>& indices);
	bool CreateUniformBuffer(State* s, Buffer* ubo, VkDeviceSize size);
	bool CreateStorageBuffer(State* s, Buffer* ssbo, VkDeviceSize size);
	void DestroyBuffer(State* s, Buffer* buffer);
	
}