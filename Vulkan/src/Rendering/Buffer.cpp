#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Buffer.hpp"
#include "Rendering/State.hpp"

namespace VKP::Impl
{

	bool CreateBuffer(State* s, Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags)
	{
		VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = bufferUsage;

		GetTransferQueueData(s, &bufInfo.sharingMode, &bufInfo.queueFamilyIndexCount, &bufInfo.pQueueFamilyIndices);

		VmaAllocationCreateInfo memInfo = {};
		memInfo.usage = VMA_MEMORY_USAGE_AUTO;
		memInfo.flags = memoryFlags;

		if (vmaCreateBuffer(s->MemAllocator, &bufInfo, &memInfo, &buffer->BufferHandle, &buffer->MemoryHandle, nullptr) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create buffer");
			return false;
		}

		return true;
	}

	bool CopyBuffer(State* s, const Buffer* src, const Buffer* dst, VkDeviceSize size)
	{
		const std::function<void(VkCommandBuffer)> fn = [&](VkCommandBuffer cmdBuffer)
		{
			VkBufferCopy region = {};
			region.srcOffset = 0;
			region.dstOffset = 0;
			region.size = size;

			vkCmdCopyBuffer(cmdBuffer, src->BufferHandle, dst->BufferHandle, 1, &region);
		};

		return SubmitTransfer(s, fn);
	}

	bool CreateVertexBuffer(State* s, Buffer* vbo, const std::vector<Vertex>& vertices)
	{
		Buffer staging = {};
		const VkDeviceSize size = vertices.size() * sizeof(Vertex);

		if (!CreateBuffer(s, &staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(s->MemAllocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, vertices.data(), size);

		vmaUnmapMemory(s->MemAllocator, staging.MemoryHandle);

		if (!CreateBuffer(s, vbo, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(s, &staging, vbo, size);
		vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);

		vbo->Size = (uint32_t)size;

		return success;
	}

	bool CreateIndexBuffer(State* s, Buffer* ibo, const std::vector<uint32_t>& indices)
	{
		Buffer staging = {};
		const VkDeviceSize size = indices.size() * sizeof(uint32_t);

		if (!CreateBuffer(s, &staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(s->MemAllocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, indices.data(), size);

		vmaUnmapMemory(s->MemAllocator, staging.MemoryHandle);

		if (!CreateBuffer(s, ibo, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(s, &staging, ibo, size);
		vmaDestroyBuffer(s->MemAllocator, staging.BufferHandle, staging.MemoryHandle);

		ibo->Size = (uint32_t)size;

		return success;
	}

	bool CreateUniformBuffer(State* s, Buffer* ubo, VkDeviceSize size)
	{
		const VkDeviceSize alignedSize = GetAlignedSize(size, s->PhysDeviceProperties.limits.minUniformBufferOffsetAlignment);

		if (!CreateBuffer(s, ubo, alignedSize * MAX_CONCURRENT_FRAMES, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
		{
			VKP_ERROR("Unable to create uniform buffer");
			return false;
		}

		ubo->Size = size;
		ubo->AlignedSize = alignedSize;

		return true;
	}

	bool CreateStorageBuffer(State* s, Buffer* ssbo, VkDeviceSize size)
	{
		if (!CreateBuffer(s, ssbo, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
		{
			VKP_ERROR("Unable to create storage buffer");
			return false;
		}

		ssbo->Size = (uint32_t)size;

		return true;
	}

	void DestroyBuffer(State* s, Buffer* buffer)
	{
		if (buffer->BufferHandle != VK_NULL_HANDLE)
			vmaDestroyBuffer(s->MemAllocator, buffer->BufferHandle, buffer->MemoryHandle);
	}

}