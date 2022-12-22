#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/State.hpp"
#include "Rendering/Texture.hpp"

namespace VKP::Impl
{

	State* State::Data = nullptr;

	bool QueueIndices::AreValid() const
	{
		return Graphics != UINT32_MAX && Presentation != UINT32_MAX && Transfer != UINT32_MAX;
	}

	bool SwapchainData::IsValid() const
	{
		return !PresentModes.empty() && !SurfaceFormats.empty();
	}

	bool SubmitTransfer(State* s, const std::function<void(VkCommandBuffer)>& fn)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;

		VkCommandBufferAllocateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufInfo.commandPool = s->Frames[s->CurrentFrame].TransferPool;
		bufInfo.commandBufferCount = 1;
		bufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkResult result = vkAllocateCommandBuffers(s->Device, &bufInfo, &cmdBuffer);
		VK_CHECK_RESULT(result);

		if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to allocate transfer command buffer");
			return false;
		}

		VkCommandBufferBeginInfo begInfo = {};
		begInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		result = vkBeginCommandBuffer(cmdBuffer, &begInfo);
		VK_CHECK_RESULT(result);

		if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to begin transfer command recording");
			vkFreeCommandBuffers(s->Device, s->Frames[s->CurrentFrame].TransferPool, 1, &cmdBuffer);
			return false;
		}

		fn(cmdBuffer);

		result = vkEndCommandBuffer(cmdBuffer);
		VK_CHECK_RESULT(result);

		if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to end transfer command buffer");
			return false;
		}

		VkSubmitInfo endInfo = {};
		endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		endInfo.pCommandBuffers = &cmdBuffer;
		endInfo.commandBufferCount = 1;

		result = vkQueueSubmit(s->TransferQueue, 1, &endInfo, VK_NULL_HANDLE);
		VK_CHECK_RESULT(result);

		if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to submit transfer command buffer to queue");
			return false;
		}

		vkQueueWaitIdle(s->TransferQueue); // TODO: Fence

		return true;
	}

	void GetTransferQueueData(State* s, VkSharingMode* mode, uint32_t* numQueues, const uint32_t** queuesPtr)
	{
		if (!s->Indices.DedicatedTransferQueue)
		{
			*mode = VK_SHARING_MODE_EXCLUSIVE;
			return;
		}

		*mode = VK_SHARING_MODE_CONCURRENT;
		*numQueues = s->Indices.ConcurrentQueues.size();
		*queuesPtr = s->Indices.ConcurrentQueues.data();
	}

	uint32_t GetAlignedSize(uint32_t size, uint32_t minAlignment)
	{
		if (minAlignment == 0) return size;
		return (size + minAlignment - 1) & ~(minAlignment - 1);
	}

	VkSampleCountFlagBits GetMsaaMaxSamples(const VkPhysicalDeviceProperties& props)
	{
		if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_64_BIT)
			return VK_SAMPLE_COUNT_64_BIT;

		else if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_32_BIT)
			return VK_SAMPLE_COUNT_32_BIT;

		else if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_16_BIT)
			return VK_SAMPLE_COUNT_16_BIT;

		else if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_8_BIT)
			return VK_SAMPLE_COUNT_8_BIT;

		else if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT)
			return VK_SAMPLE_COUNT_4_BIT;

		else if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_2_BIT)
			return VK_SAMPLE_COUNT_2_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}

}