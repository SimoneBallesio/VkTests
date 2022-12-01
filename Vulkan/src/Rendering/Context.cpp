#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <fstream>

#define MAX_CONCURRENT_FRAMES 2

#define OBJ_TEXTURE_PATH "assets/models/viking_room.png"
#define OBJ_MODEL_PATH "assets/models/viking_room.obj"

namespace VKP
{

	bool QueueIndices::AreValid() const
	{
		return Graphics != UINT32_MAX && Presentation != UINT32_MAX && Transfer != UINT32_MAX;
	}

	bool SwapchainData::IsValid() const
	{
		return !PresentModes.empty() && !SurfaceFormats.empty();
	}

	Context::~Context()
	{
		vkDeviceWaitIdle(s_Device);

		if (m_ObjTexture.ImageHandle != VK_NULL_HANDLE)
		{
			vkDestroySampler(s_Device, m_ObjTexture.SamplerHandle, nullptr);
			vkDestroyImageView(s_Device, m_ObjTexture.ViewHandle, nullptr);
			vmaDestroyImage(s_Allocator, m_ObjTexture.ImageHandle, m_ObjTexture.MemoryHandle);
		}

		if (m_DescPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(s_Device, m_DescPool, nullptr);

		if (m_DescSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(s_Device, m_DescSetLayout, nullptr);

		for (auto& b : m_UniformBuffers)
		{
			if (b.BufferHandle != VK_NULL_HANDLE)
				vmaDestroyBuffer(s_Allocator, b.BufferHandle, b.MemoryHandle);
		}

		if (m_ObjVBO.BufferHandle != VK_NULL_HANDLE)
			vmaDestroyBuffer(s_Allocator, m_ObjVBO.BufferHandle, m_ObjVBO.MemoryHandle);

		if (m_ObjIBO.BufferHandle != VK_NULL_HANDLE)
			vmaDestroyBuffer(s_Allocator, m_ObjIBO.BufferHandle, m_ObjIBO.MemoryHandle);

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			vkDestroySemaphore(s_Device, m_CanAcquireImage[i], nullptr);
			vkDestroySemaphore(s_Device, m_CanPresentImage[i], nullptr);
			vkDestroyFence(s_Device, m_PrevFrameRenderEnded[i], nullptr);
		}

		if (m_TransferPool != m_CmdPool)
			vkDestroyCommandPool(s_Device, m_TransferPool, nullptr);

		if (m_CmdPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(s_Device, m_CmdPool, nullptr);

		DestroySwapchain();

		if (m_DefaultPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(s_Device, m_DefaultPipeline, nullptr);

		if (m_DefaultPipeLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(s_Device, m_DefaultPipeLayout, nullptr);

		if (m_DefaultRenderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(s_Device, m_DefaultRenderPass, nullptr);

		if (s_Allocator != VK_NULL_HANDLE)
			vmaDestroyAllocator(s_Allocator);

		if (s_Device != VK_NULL_HANDLE)
			vkDestroyDevice(s_Device, nullptr);

		if (m_Surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		if (m_Debug != VK_NULL_HANDLE)
			DestroyDebugUtilsMessengerEXT(m_Instance, m_Debug, nullptr);

		if (m_Instance != VK_NULL_HANDLE)
			vkDestroyInstance(m_Instance, nullptr);
	}

	bool Context::BeforeWindowCreation()
	{
		bool success = CreateInstance();
		return success;
	}

	bool Context::AfterWindowCreation()
	{
		bool success = CreateSurface();
		if (success) success = ChoosePhysicalDevice();
		if (success) success = CreateDevice();
		if (success) success = CreateMemoryAllocator();
		if (success) success = CreateSwapchain();
		if (success) success = CreateImageViews();
		if (success) success = CreateDepthResources();
		if (success) success = CreateRenderPass();
		if (success) success = CreateFramebuffers();
		if (success) success = CreateCommandPool();
		if (success) success = AllocateCommandBuffer();
		if (success) success = CreateSyncObjects();

		if (success) success = LoadObjModel();
		if (success) success = LoadObjTexture();

		if (success) success = CreateUniformBuffers();

		if (success) success = CreateDescriptorSetLayout();
		if (success) success = CreateDescriptorPool();
		if (success) success = AllocateDescriptorSets();
		if (success) success = CreatePipeline();

		return success;
	}

	bool Context::CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags)
	{
		VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = bufferUsage;

		GetTransferQueueData(&bufInfo.sharingMode, &bufInfo.queueFamilyIndexCount, &bufInfo.pQueueFamilyIndices);

		VmaAllocationCreateInfo memInfo = {};
		memInfo.usage = VMA_MEMORY_USAGE_AUTO;
		memInfo.flags = memoryFlags;

		if (vmaCreateBuffer(s_Allocator, &bufInfo, &memInfo, &buffer.BufferHandle, &buffer.MemoryHandle, nullptr) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create buffer");
			return false;
		}

		return true;
	}

	bool Context::CopyBuffer(const Buffer& src, const Buffer& dst, VkDeviceSize size)
	{
		const std::function<void(VkCommandBuffer)> fn = [&](VkCommandBuffer cmdBuffer)
		{
			VkBufferCopy region = {};
			region.srcOffset = 0;
			region.dstOffset = 0;
			region.size = size;

			vkCmdCopyBuffer(cmdBuffer, src.BufferHandle, dst.BufferHandle, 1, &region);
		};

		return SubmitTransfer(fn);
	}

	bool Context::CreateImage(Texture& texture, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
	{
		VkImageCreateInfo imgInfo = {};
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.arrayLayers = 1;
		imgInfo.extent = {width, height, 1};
		imgInfo.format = format;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imgInfo.mipLevels = 1;
		imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imgInfo.tiling = tiling;
		imgInfo.usage = usage;

		GetTransferQueueData(&imgInfo.sharingMode, &imgInfo.queueFamilyIndexCount, &imgInfo.pQueueFamilyIndices);

		VmaAllocationCreateInfo memInfo = {};
		memInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		memInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		VkResult result = vmaCreateImage(s_Allocator, &imgInfo, &memInfo, &texture.ImageHandle, &texture.MemoryHandle, nullptr);
		VK_CHECK_RESULT(result);

		return VK_SUCCESS == result;
	}

	bool Context::PopulateImage(Texture& texture, Buffer& staging, uint32_t width, uint32_t height)
	{
		const std::function<void(VkCommandBuffer)> fn = [&](VkCommandBuffer cmdBuffer)
		{
			VkBufferImageCopy region = {};
			region.bufferImageHeight = 0;
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.imageExtent = {width, height, 1};
			region.imageOffset = {0, 0, 0};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.layerCount = 1;

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = texture.ImageHandle;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			vkCmdCopyBufferToImage(cmdBuffer, staging.BufferHandle, texture.ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		};

		return SubmitTransfer(fn);
	}

	bool Context::CreateImageView(Texture& texture, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture.ImageHandle;
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
		viewInfo.subresourceRange.levelCount = 1;

		VkResult result = vkCreateImageView(s_Device, &viewInfo, nullptr, &texture.ViewHandle);
		VK_CHECK_RESULT(result);

		return VK_SUCCESS == result;
	}

	bool Context::CreateImageSampler(Texture& texture)
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
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(s_Device, &samplerInfo, nullptr, &texture.SamplerHandle) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create sampler for test texture");
			return false;
		}

		return true;
	}

	bool Context::SubmitTransfer(const std::function<void(VkCommandBuffer)>& fn)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;

		VkCommandBufferAllocateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufInfo.commandPool = m_TransferPool;
		bufInfo.commandBufferCount = 1;
		bufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkResult result = vkAllocateCommandBuffers(s_Device, &bufInfo, &cmdBuffer);
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
			vkFreeCommandBuffers(s_Device, m_TransferPool, 1, &cmdBuffer);
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

		result = vkQueueSubmit(m_TransferQueue, 1, &endInfo, VK_NULL_HANDLE);
		VK_CHECK_RESULT(result);

		if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to submit transfer command buffer to queue");
			return false;
		}

		vkQueueWaitIdle(m_TransferQueue); // TODO: Fence

		return true;
	}

	void Context::SwapBuffers()
	{
		vkWaitForFences(s_Device, 1, &m_PrevFrameRenderEnded[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageId;
		VkResult result = vkAcquireNextImageKHR(s_Device, m_Swapchain, UINT64_MAX, m_CanAcquireImage[m_CurrentFrame], VK_NULL_HANDLE, &imageId);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return;
		}

		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			VKP_ERROR("Unable to acquire swapchain image");
			return;
		}

		vkResetFences(s_Device, 1, &m_PrevFrameRenderEnded[m_CurrentFrame]);

		vkResetCommandBuffer(m_CmdBuffer[m_CurrentFrame], 0);

		RecordCommandBuffer(m_CmdBuffer[m_CurrentFrame], imageId);

		const VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &m_CmdBuffer[m_CurrentFrame];
		submitInfo.commandBufferCount = 1;
		submitInfo.pWaitSemaphores = &m_CanAcquireImage[m_CurrentFrame]; // Wait what?
		submitInfo.pWaitDstStageMask = stages; // To do what?
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_CanPresentImage[m_CurrentFrame];
		submitInfo.signalSemaphoreCount = 1;

		if (vkQueueSubmit(m_PresentQueue, 1, &submitInfo, m_PrevFrameRenderEnded[m_CurrentFrame]) != VK_SUCCESS)
		{
			VKP_ASSERT(false, "Unable to submit queue for rendering");
			return;
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pSwapchains = &m_Swapchain;
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &m_CanPresentImage[m_CurrentFrame];
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &imageId;

		result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Resized)
		{
			m_Resized = false;
			RecreateSwapchain();
		}

		else if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to present image to the swapchain");
			return;
		}

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_CONCURRENT_FRAMES;
	}

	void Context::OnResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		m_AspectRatio = (float)width / (float)height;
		m_Resized = true;
	}

	void Context::GetTransferQueueData(VkSharingMode* mode, uint32_t* numQueues, const uint32_t** queuesPtr)
	{
		if (!m_QueueIndices.DedicatedTransferQueue)
		{
			*mode = VK_SHARING_MODE_EXCLUSIVE;
			return;
		}

		*mode = VK_SHARING_MODE_CONCURRENT;
		*numQueues = m_QueueIndices.ConcurrentQueues.size();
		*queuesPtr = m_QueueIndices.ConcurrentQueues.data();
	}

	Context* Context::Create()
	{
		if (s_Context != nullptr)
			return s_Context;

		s_Context = new Context();
		return s_Context;
	}

	Context& Context::Get()
	{
		return *s_Context;
	}

	bool Context::CreateInstance()
	{
		VkApplicationInfo appInfo = {};
		appInfo.pApplicationName = "VkTests";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Faber";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

#ifdef VKP_DEBUG

		VkDebugUtilsMessengerCreateInfoEXT dbgInfo = {};
		dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbgInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		dbgInfo.pfnUserCallback = &Context::DebugUtilsMessengerCallbackEXT;

		const char* layerName = "VK_LAYER_KHRONOS_validation";

#endif

		std::vector<const char*> extensionNames =
		{
			VK_KHR_SURFACE_EXTENSION_NAME,

#if defined(VKP_PLATFORM_WIN32)
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,

#elif defined(VKP_PLATFORM_APPLE)
			VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
			VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
			VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
#endif

#if defined(VKP_DEBUG)
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		};

		VkInstanceCreateInfo instInfo = {};
		instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instInfo.pApplicationInfo = &appInfo;
		instInfo.ppEnabledExtensionNames = extensionNames.data();
		instInfo.enabledExtensionCount = extensionNames.size();

#ifdef VKP_PLATFORM_APPLE
		instInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

#ifdef VKP_DEBUG
		instInfo.ppEnabledLayerNames = &layerName;
		instInfo.enabledLayerCount = 1;
		instInfo.pNext = &dbgInfo;

#else
		instInfo.ppEnabledLayerNames = nullptr;
		instInfo.enabledLayerCount = 0;
#endif

		if (vkCreateInstance(&instInfo, nullptr, &m_Instance) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan instance");
			return false;
		}

#ifdef VKP_DEBUG
		if (CreateDebugUtilsMessengerEXT(m_Instance, &dbgInfo, nullptr, &m_Debug) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan debug messenger");
			return false;
		}
#endif

		return true;
	}

	bool Context::CreateSurface()
	{
		SDL_Window* window = (SDL_Window*)Window::Get().GetNativeHandle();

		if (SDL_Vulkan_CreateSurface(window, m_Instance, &m_Surface) == SDL_FALSE)
		{
			VKP_ERROR("Unable to create Vulkan surface");
			return false;
		}

		return true;
	}

	bool Context::ChoosePhysicalDevice()
	{
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);

		if (count == 0)
		{
			VKP_ERROR("Unable to find any suitable physical device for Vulkan");
			return false;
		}

		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

		for (const auto& d : devices)
		{
			uint32_t queueCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(d, &queueCount, nullptr);

			if (queueCount == 0) continue;

			std::vector<VkQueueFamilyProperties> queues(queueCount);
			vkGetPhysicalDeviceQueueFamilyProperties(d, &queueCount, queues.data());

			size_t i = 0;

			for (const auto& q : queues)
			{
				if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					m_QueueIndices.Graphics = i;
					m_QueueIndices.Transfer = i;
				}

				else if (q.queueFlags & VK_QUEUE_TRANSFER_BIT)
					m_QueueIndices.Transfer = i;

				VkBool32 supported = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(d, i, m_Surface, &supported);

				if (supported == VK_TRUE)
					m_QueueIndices.Presentation = i;

				if (m_QueueIndices.Graphics != m_QueueIndices.Transfer && m_QueueIndices.AreValid()) break;
			}

			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(d, m_Surface, &formatCount, nullptr);

			if (formatCount == 0) continue;

			m_SwapchainData.SurfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(d, m_Surface, &formatCount, m_SwapchainData.SurfaceFormats.data());

			uint32_t presentCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(d, m_Surface, &presentCount, nullptr);

			if (presentCount == 0) continue;

			m_SwapchainData.PresentModes.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(d, m_Surface, &presentCount, m_SwapchainData.PresentModes.data());

			std::set<std::string> requiredExtensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,

#ifdef VKP_PLATFORM_APPLE
				"VK_KHR_portability_subset"
#endif
			};

			uint32_t extensionCount = 0;
			vkEnumerateDeviceExtensionProperties(d, nullptr, &extensionCount, nullptr);

			if (extensionCount == 0) continue;

			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(d, nullptr, &extensionCount, extensions.data());

			for (const auto& e : extensions)
			{
				requiredExtensions.erase(e.extensionName);
				if (requiredExtensions.empty()) break;
			}

			if (m_QueueIndices.AreValid() && m_SwapchainData.IsValid() && requiredExtensions.empty())
			{
				std::set<uint32_t> indices = { m_QueueIndices.Graphics, m_QueueIndices.Transfer };

				m_QueueIndices.ConcurrentQueues = { indices.begin(), indices.end() };
				m_QueueIndices.DedicatedTransferQueue = m_QueueIndices.ConcurrentQueues.size() > 1;

				m_PhysDevice = d;

				return true;
			}
		}

		VKP_ERROR("Unable to find any suitable physical device for Vulkan");
		return false;
	}

	bool Context::CreateDevice()
	{
		std::set<uint32_t> indicesSet = { m_QueueIndices.Graphics, m_QueueIndices.Presentation, m_QueueIndices.Transfer };
		std::vector<uint32_t> indices(indicesSet.begin(), indicesSet.end());
		std::vector<VkDeviceQueueCreateInfo> queueInfos(indices.size());

		const float priority = 1.0f;
		size_t i = 0;

		for (auto& queueInfo : queueInfos)
		{
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = indices[i];
			queueInfo.pQueuePriorities = &priority;
			queueInfo.queueCount = 1;

			i++;
		}

		std::vector<const char*> extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,

#ifdef VKP_PLATFORM_APPLE
			"VK_KHR_portability_subset"
#endif
		};

		VkPhysicalDeviceFeatures features = {};
		memset(&features, 0, sizeof(VkPhysicalDeviceFeatures));

		VkDeviceCreateInfo deviceInfo = {};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pQueueCreateInfos = queueInfos.data();
		deviceInfo.queueCreateInfoCount = queueInfos.size();
		deviceInfo.ppEnabledLayerNames = nullptr;
		deviceInfo.enabledLayerCount = 0;
		deviceInfo.ppEnabledExtensionNames = extensions.data();
		deviceInfo.enabledExtensionCount = extensions.size();
		deviceInfo.pEnabledFeatures = &features;

		if (vkCreateDevice(m_PhysDevice, &deviceInfo, nullptr, &s_Device) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan device");
			return false;
		}

		vkGetDeviceQueue(s_Device, m_QueueIndices.Graphics, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(s_Device, m_QueueIndices.Presentation, 0, &m_PresentQueue);
		vkGetDeviceQueue(s_Device, m_QueueIndices.Transfer, 0, &m_TransferQueue);

		return true;
	}

	bool Context::CreateMemoryAllocator()
	{
		VmaAllocatorCreateInfo allocInfo = {};
		allocInfo.instance = m_Instance;
		allocInfo.physicalDevice = m_PhysDevice;
		allocInfo.device = s_Device;
		allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

		if (vmaCreateAllocator(&allocInfo, &s_Allocator) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create memory allocator");
			return false;
		}

		return true;
	}

	bool Context::CreateDepthResources()
	{
		bool success = CreateImage(m_SwapDepth, m_SwapchainData.CurrentExtent.width, m_SwapchainData.CurrentExtent.height, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		if (success) success = CreateImageView(m_SwapDepth, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

		return success;
	}

	bool Context::CreateSwapchain()
	{
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysDevice, m_Surface, &m_SwapchainData.Capabilities);

		const auto& capabilities = m_SwapchainData.Capabilities;
		const VkExtent2D extents = ChooseExtents();
		const VkSurfaceFormatKHR format = ChooseSurfaceFormat();
		const VkPresentModeKHR presentMode = ChoosePresentMode();
		const uint32_t numImages = std::max(capabilities.minImageCount, std::min(capabilities.minImageCount + 1, capabilities.maxImageCount));
		const uint32_t indices[] = { m_QueueIndices.Graphics, m_QueueIndices.Presentation };

		VkSwapchainCreateInfoKHR swapInfo = {};
		swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapInfo.surface = m_Surface;
		swapInfo.minImageCount = numImages;
		swapInfo.imageFormat = format.format;
		swapInfo.imageColorSpace = format.colorSpace;
		swapInfo.imageExtent = extents;
		swapInfo.imageArrayLayers = 1;
		swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapInfo.preTransform = capabilities.currentTransform;
		swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapInfo.presentMode = presentMode;
		swapInfo.oldSwapchain = VK_NULL_HANDLE;
		swapInfo.clipped = VK_TRUE;
		swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (indices[0] != indices[1])
		{
			swapInfo.pQueueFamilyIndices = indices;
			swapInfo.queueFamilyIndexCount = 2;
			swapInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		}

		if (vkCreateSwapchainKHR(s_Device, &swapInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan swapchain");
			return false;
		}

		m_SwapchainData.CurrentExtent = extents;
		m_SwapchainData.Format = format;

		return true;
	}

	bool Context::RecreateSwapchain()
	{
		vkDeviceWaitIdle(s_Device);

		m_SwapchainData.PrevFormat = m_SwapchainData.Format;

		DestroySwapchain();

		bool valid = CreateSwapchain();
		if (valid) valid = CreateImageViews();

		m_Viewport.y = static_cast<float>(m_SwapchainData.CurrentExtent.height);
		m_Viewport.width = static_cast<float>(m_SwapchainData.CurrentExtent.width);
		m_Viewport.height = static_cast<float>(m_SwapchainData.CurrentExtent.height) * -1.0f;
		m_Scissor.extent = m_SwapchainData.CurrentExtent;
		m_Scissor.offset = { 0, 0 };

		if (valid) valid = CreateDepthResources();
		if (valid) valid = CreateFramebuffers();

		if (valid && (m_SwapchainData.PrevFormat.colorSpace != m_SwapchainData.Format.colorSpace || m_SwapchainData.PrevFormat.format != m_SwapchainData.Format.format))
		{
			VKP_WARN("Mismatch between previous and current presentation formats");

			vkDestroyRenderPass(s_Device, m_DefaultRenderPass, nullptr);
			valid = CreateRenderPass();
		}

		return valid;
	}

	void Context::DestroySwapchain()
	{
		if (m_SwapDepth.ViewHandle != VK_NULL_HANDLE)
			vkDestroyImageView(s_Device, m_SwapDepth.ViewHandle, nullptr);

		if (m_SwapDepth.ImageHandle != VK_NULL_HANDLE)
			vmaDestroyImage(s_Allocator, m_SwapDepth.ImageHandle, m_SwapDepth.MemoryHandle);

		for (auto& f : m_DefaultFramebuffers)
			vkDestroyFramebuffer(s_Device, f, nullptr);

		for (auto& i : m_SwapImages)
			vkDestroyImageView(s_Device, i.ViewHandle, nullptr);

		if (m_Swapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(s_Device, m_Swapchain, nullptr);
	}

	bool Context::CreateImageViews()
	{
		uint32_t count = 0;
		vkGetSwapchainImagesKHR(s_Device, m_Swapchain, &count, nullptr);

		std::vector<VkImage> images(count);
		m_SwapImages.resize(count);

		vkGetSwapchainImagesKHR(s_Device, m_Swapchain, &count, images.data());

		std::vector<VkImageViewCreateInfo> viewInfos(count);

		for (size_t i = 0; i < count; i++)
		{
			auto& t = m_SwapImages[i];
			t.ImageHandle = images[i];

			if (!CreateImageView(t, m_SwapchainData.Format.format, VK_IMAGE_ASPECT_COLOR_BIT))
			{
				VKP_ERROR("Unable to create Vulkan swapchain image view #{}", i);
				return false;
			}
		}

		return true;
	}

	bool Context::CreateRenderPass()
	{
		std::vector<VkAttachmentDescription> attachments;
		attachments.reserve(2);

		auto& colorPass = attachments.emplace_back();
		colorPass.samples = VK_SAMPLE_COUNT_1_BIT;
		colorPass.format = m_SwapchainData.Format.format;
		colorPass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorPass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorPass.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		auto& depthPass = attachments.emplace_back();
		depthPass.samples = VK_SAMPLE_COUNT_1_BIT;
		depthPass.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		depthPass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthPass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthPass.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthRef = {};
		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = &colorRef;
		subpass.colorAttachmentCount = 1;
		subpass.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency subd = {};
		subd.srcSubpass = VK_SUBPASS_EXTERNAL;
		subd.dstSubpass = 0;
		subd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		subd.srcAccessMask = 0;
		subd.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		subd.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.pSubpasses = &subpass;
		passInfo.subpassCount = 1;
		passInfo.pAttachments = attachments.data();
		passInfo.attachmentCount = attachments.size();
		passInfo.pDependencies = &subd;
		passInfo.dependencyCount = 1;

		if (vkCreateRenderPass(s_Device, &passInfo, nullptr, &m_DefaultRenderPass) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default render pass");
			return false;
		}

		return true;
	}

	bool Context::CreateFramebuffers()
	{
		m_DefaultFramebuffers.resize(m_SwapImages.size());

		for (size_t i = 0; i < m_DefaultFramebuffers.size(); i++)
		{
			std::vector<VkImageView> views = { m_SwapImages[i].ViewHandle, m_SwapDepth.ViewHandle };
			VkFramebufferCreateInfo frameInfo = {};
			frameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameInfo.pAttachments = views.data();
			frameInfo.attachmentCount = views.size();
			frameInfo.width = m_SwapchainData.CurrentExtent.width;
			frameInfo.height = m_SwapchainData.CurrentExtent.height;
			frameInfo.renderPass = m_DefaultRenderPass;
			frameInfo.layers = 1;

			if (vkCreateFramebuffer(s_Device, &frameInfo, nullptr, &m_DefaultFramebuffers[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create framebuffer for Vulkan swapchain image view #{}", i);
				return false;
			}
		}

		return true;
	}

	bool Context::CreateUniformBuffers()
	{
		m_UniformBuffers.reserve(MAX_CONCURRENT_FRAMES);

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			auto& b = m_UniformBuffers.emplace_back();

			if (!CreateBuffer(b, sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			{
				VKP_ERROR("Unable to create uniform buffer for in-flight frame {}", i);
				return false;
			}
		}

		return true;
	}

	bool Context::CreateDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings(2);
		
		auto& binding = bindings[0];
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		binding.binding = 0;

		auto& samplerBinding = bindings[1];
		samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerBinding.descriptorCount = 1;
		samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerBinding.binding = 1;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pBindings = bindings.data();
		layoutInfo.bindingCount = bindings.size();

		if (vkCreateDescriptorSetLayout(s_Device, &layoutInfo, nullptr, &m_DescSetLayout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor set layout");
			return false;
		}

		return true;
	}

	bool Context::CreateDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> sizes(2);
		
		auto& size = sizes[0];
		size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		size.descriptorCount = MAX_CONCURRENT_FRAMES; // 2 descriptors, 1 x  concurrent frame

		auto& samplerSize = sizes[1];
		samplerSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerSize.descriptorCount = MAX_CONCURRENT_FRAMES;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pPoolSizes = sizes.data();
		poolInfo.poolSizeCount = sizes.size();
		poolInfo.maxSets = MAX_CONCURRENT_FRAMES; // 1 set x concurrent frame

		if (vkCreateDescriptorPool(s_Device, &poolInfo, nullptr, &m_DescPool) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor pool");
			return false;
		}

		return true;
	}

	bool Context::AllocateDescriptorSets()
	{
		const std::vector<VkDescriptorSetLayout> layouts(MAX_CONCURRENT_FRAMES, m_DescSetLayout);

		VkDescriptorSetAllocateInfo setInfo = {};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		setInfo.descriptorPool = m_DescPool;
		setInfo.descriptorSetCount = MAX_CONCURRENT_FRAMES;
		setInfo.pSetLayouts = layouts.data();

		m_DescSets.resize(MAX_CONCURRENT_FRAMES);

		if (vkAllocateDescriptorSets(s_Device, &setInfo, m_DescSets.data()) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to allocate descriptor sets");
			return false;
		}

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			const auto& b = m_UniformBuffers[i];

			VkDescriptorBufferInfo bufInfo = {};
			bufInfo.buffer = b.BufferHandle;
			bufInfo.offset = 0;
			bufInfo.range = VK_WHOLE_SIZE;

			VkDescriptorImageInfo imgInfo = {};
			imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imgInfo.imageView = m_ObjTexture.ViewHandle;
			imgInfo.sampler = m_ObjTexture.SamplerHandle;

			std::vector<VkWriteDescriptorSet> writeInfos(2);

			auto& bufWriteInfo = writeInfos[0];
			bufWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			bufWriteInfo.pBufferInfo = &bufInfo;
			bufWriteInfo.dstSet = m_DescSets[i];
			bufWriteInfo.dstBinding = 0;
			bufWriteInfo.dstArrayElement = 0;
			bufWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			bufWriteInfo.descriptorCount = 1;

			auto& imgWriteInfo = writeInfos[1];
			imgWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imgWriteInfo.pImageInfo = &imgInfo;
			imgWriteInfo.dstSet = m_DescSets[i];
			imgWriteInfo.dstBinding = 1;
			imgWriteInfo.dstArrayElement = 0;
			imgWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			imgWriteInfo.descriptorCount = 1;

			vkUpdateDescriptorSets(s_Device, writeInfos.size(), writeInfos.data(), 0, nullptr);
		}

		return true;
	}

	bool Context::CreatePipeline()
	{
		std::string moduleNames[] = {
			"assets/shaders/base.vert.spv",
			"assets/shaders/base.frag.spv",
		};

		const VkShaderStageFlagBits moduleStages[] = {
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		std::vector<VkShaderModuleCreateInfo> moduleInfos(2);
		std::vector<VkShaderModule> modules(2);

		std::vector<VkPipelineShaderStageCreateInfo> stageInfos(2);

		for (size_t i = 0; i < 2; i++)
		{
			std::ifstream file(moduleNames[i], std::ios::ate | std::ios::binary);

			if (!file.is_open())
			{
				VKP_ERROR("Unable to locate shader module file: {}", moduleNames[i]);
				return false;
			}

			const size_t size = file.tellg();
			std::vector<char> fileBuffer(size);

			file.seekg(0);
			file.read(fileBuffer.data(), size);

			file.close();

			auto& moduleInfo = moduleInfos[i];
			moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleInfo.pCode = reinterpret_cast<uint32_t*>(fileBuffer.data());
			moduleInfo.codeSize = size;

			if (vkCreateShaderModule(s_Device, &moduleInfo, nullptr, &modules[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create shader module: {}", moduleNames[i]);
				return false;
			}

			auto &stageInfo = stageInfos[i];
			stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo.module = modules[i];
			stageInfo.pName = "main";
			stageInfo.stage = moduleStages[i];
		}

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynInfo = {};
		dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynInfo.pDynamicStates = dynamicStates.data();
		dynInfo.dynamicStateCount = dynamicStates.size();

		VkVertexInputBindingDescription vertBind = {};
		vertBind.binding = 0;
		vertBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		vertBind.stride = sizeof(Vertex);

		std::vector<VkVertexInputAttributeDescription> descriptions;
		descriptions.reserve(3);

		auto& posDesc = descriptions.emplace_back();
		posDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		posDesc.location = 0;
		posDesc.binding = 0;
		posDesc.offset = offsetof(Vertex, Position);

		auto& colDesc = descriptions.emplace_back();
		colDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		colDesc.location = 1;
		colDesc.binding = 0;
		colDesc.offset = offsetof(Vertex, Color);

		auto& texDesc = descriptions.emplace_back();
		texDesc.format = VK_FORMAT_R32G32_SFLOAT;
		texDesc.location = 2;
		texDesc.binding = 0;
		texDesc.offset = offsetof(Vertex, TexCoord);

		VkPipelineVertexInputStateCreateInfo vertInfo = {};
		vertInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertInfo.pVertexBindingDescriptions = &vertBind;
		vertInfo.vertexAttributeDescriptionCount = descriptions.size();
		vertInfo.pVertexAttributeDescriptions = descriptions.data();
		vertInfo.vertexBindingDescriptionCount = 1;

		VkPipelineInputAssemblyStateCreateInfo inputInfo = {};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputInfo.primitiveRestartEnable = VK_FALSE;

		m_Viewport.x = 0.0f;
		m_Viewport.y = static_cast<float>(m_SwapchainData.CurrentExtent.height);
		m_Viewport.width = static_cast<float>(m_SwapchainData.CurrentExtent.width);
		m_Viewport.height = static_cast<float>(m_SwapchainData.CurrentExtent.height) * -1.0f;
		m_Viewport.minDepth = 0.0f;
		m_Viewport.maxDepth = 1.0f;

		m_Scissor.extent = m_SwapchainData.CurrentExtent;
		m_Scissor.offset = { 0, 0 };

		VkPipelineViewportStateCreateInfo viewportInfo = {};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterInfo = {};
		rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo sampleInfo = {};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		sampleInfo.sampleShadingEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState blendState = {};
		blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo blendInfo = {};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.logicOpEnable = VK_FALSE;
		blendInfo.pAttachments = &blendState;
		blendInfo.attachmentCount = 1;

		VkPipelineDepthStencilStateCreateInfo depthInfo = {};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = VK_TRUE;
		depthInfo.depthWriteEnable = VK_TRUE;
		depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		depthInfo.depthBoundsTestEnable = VK_FALSE;
		depthInfo.stencilTestEnable = VK_FALSE;

		VkPushConstantRange vpRange = {};
		vpRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vpRange.size = sizeof(glm::mat4);
		vpRange.offset = 0;

		VkPipelineLayoutCreateInfo pipeLayoutInfo = {};
		pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeLayoutInfo.pPushConstantRanges = &vpRange;
		pipeLayoutInfo.pushConstantRangeCount = 1;
		pipeLayoutInfo.pSetLayouts = &m_DescSetLayout;
		pipeLayoutInfo.setLayoutCount = 1;

		if (vkCreatePipelineLayout(s_Device, &pipeLayoutInfo, nullptr, &m_DefaultPipeLayout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default pipeline layout");

			for (auto &m : modules)
				vkDestroyShaderModule(s_Device, m, nullptr);

			return false;
		}

		VkGraphicsPipelineCreateInfo pipeInfo = {};
		pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeInfo.pStages = stageInfos.data();
		pipeInfo.stageCount = stageInfos.size();
		pipeInfo.renderPass = m_DefaultRenderPass;
		pipeInfo.subpass = 0;
		pipeInfo.layout = m_DefaultPipeLayout;
		pipeInfo.pDynamicState = &dynInfo;
		pipeInfo.pVertexInputState = &vertInfo;
		pipeInfo.pInputAssemblyState = &inputInfo;
		pipeInfo.pViewportState = &viewportInfo;
		pipeInfo.pRasterizationState = &rasterInfo;
		pipeInfo.pMultisampleState = &sampleInfo;
		pipeInfo.pColorBlendState = &blendInfo;
		pipeInfo.pDepthStencilState = &depthInfo;

		if (vkCreateGraphicsPipelines(s_Device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_DefaultPipeline) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default pipeline");

			for (auto &m : modules)
				vkDestroyShaderModule(s_Device, m, nullptr);

			return false;
		}

		for (auto& m : modules)
			vkDestroyShaderModule(s_Device, m, nullptr);

		return true;
	}

	bool Context::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = m_QueueIndices.Graphics;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(s_Device, &poolInfo, nullptr, &m_CmdPool) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create presentation command pool");
			return false;
		}

		if (m_QueueIndices.Presentation == m_QueueIndices.Graphics)
		{
			m_TransferPool = m_CmdPool;
			return true;
		}

		poolInfo.queueFamilyIndex = m_QueueIndices.Transfer;

		if (vkCreateCommandPool(s_Device, &poolInfo, nullptr, &m_TransferPool) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create presentation command pool");
			return false;
		}

		return true;
	}

	bool Context::AllocateCommandBuffer()
	{
		VkCommandBufferAllocateInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandPool = m_CmdPool;
		cmdInfo.commandBufferCount = MAX_CONCURRENT_FRAMES;

		m_CmdBuffer.resize(MAX_CONCURRENT_FRAMES, VK_NULL_HANDLE);

		if (vkAllocateCommandBuffers(s_Device, &cmdInfo, m_CmdBuffer.data()) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to allocate presentation command buffer");
			return false;
		}

		return true;
	}

	bool Context::CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_CanAcquireImage.resize(MAX_CONCURRENT_FRAMES, VK_NULL_HANDLE);
		m_CanPresentImage.resize(MAX_CONCURRENT_FRAMES, VK_NULL_HANDLE);
		m_PrevFrameRenderEnded.resize(MAX_CONCURRENT_FRAMES, VK_NULL_HANDLE);

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			if (vkCreateSemaphore(s_Device, &semInfo, nullptr, &m_CanAcquireImage[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			if (vkCreateSemaphore(s_Device, &semInfo, nullptr, &m_CanPresentImage[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			if (vkCreateFence(s_Device, &fenceInfo, nullptr, &m_PrevFrameRenderEnded[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create fence");
				return false;
			}
		}

		return true;
	}

	bool Context::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		Buffer staging = {};
		const VkDeviceSize size = vertices.size() * sizeof(Vertex);

		if (!CreateBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(s_Allocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, vertices.data(), size);

		vmaUnmapMemory(s_Allocator, staging.MemoryHandle);

		if (!CreateBuffer(m_ObjVBO, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(staging, m_ObjVBO, size);
		vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);

		return success;
	}

	bool Context::CreateIndexBuffer(const std::vector<uint32_t>& indices)
	{
		Buffer staging = {};
		const VkDeviceSize size = indices.size() * sizeof(uint32_t);

		if (!CreateBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(s_Allocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, indices.data(), size);

		vmaUnmapMemory(s_Allocator, staging.MemoryHandle);

		if (!CreateBuffer(m_ObjIBO, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(staging, m_ObjIBO, size);
		vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);

		return success;
	}

	bool Context::LoadObjModel()
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warnings, errors;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warnings, &errors, OBJ_MODEL_PATH))
		{
			VKP_ERROR("Unable to load OBJ model file {}", OBJ_MODEL_PATH);
			return false;
		}

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::unordered_map<Vertex, uint32_t> uniqueVertices;

		for (const auto& s : shapes)
		{
			for (const auto& i : s.mesh.indices)
			{
				Vertex v{};

				v.Position = {
					attrib.vertices[3 * i.vertex_index + 0],
					attrib.vertices[3 * i.vertex_index + 1],
					attrib.vertices[3 * i.vertex_index + 2]
				};

				v.TexCoord = {
					attrib.texcoords[2 * i.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * i.texcoord_index + 1]
				};

				v.Color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(v) == 0) {
					uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(v);
				}

				indices.push_back(uniqueVertices[v]);
			}
		}

		bool success = CreateVertexBuffer(vertices);
		if (success) success = CreateIndexBuffer(indices);
		if (success) m_NumIndices = indices.size();

		return success;
	}

	bool Context::LoadObjTexture()
	{
		int w = 0, h = 0, nrChannels = 0;
		uint8_t* data = stbi_load(OBJ_TEXTURE_PATH, &w, &h, &nrChannels, STBI_rgb_alpha);

		if (data == nullptr)
		{
			VKP_ERROR("Unable to locate texture file");
			return false;
		}

		Buffer staging = {};

		if (!CreateBuffer(staging, w * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
		{
			VKP_ERROR("Unable to create staging buffer for texture upload");
			stbi_image_free(data);
			return false;
		}

		void* bufData = nullptr;
		vmaMapMemory(s_Allocator, staging.MemoryHandle, &bufData);
		memcpy(bufData, data, w * h * 4);
		vmaUnmapMemory(s_Allocator, staging.MemoryHandle);

		stbi_image_free(data);

		if (!CreateImage(m_ObjTexture, w, h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
		{
			VKP_ERROR("Unable to create image object");
			vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = PopulateImage(m_ObjTexture, staging, w, h);

		vmaDestroyBuffer(s_Allocator, staging.BufferHandle, staging.MemoryHandle);

		if (success) success = CreateImageView(m_ObjTexture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		if (success) success = CreateImageSampler(m_ObjTexture);

		return success;
	}

	bool Context::RecordCommandBuffer(VkCommandBuffer buffer, size_t imageId)
	{
		// static auto startTime = std::chrono::high_resolution_clock::now();
		// auto currentTime = std::chrono::high_resolution_clock::now();
		// float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float time = 0.0f;

		glm::mat4 M = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
									glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		void *data = nullptr;
		vmaMapMemory(s_Allocator, m_UniformBuffers[m_CurrentFrame].MemoryHandle, &data);
		memcpy(data, &M[0][0], sizeof(glm::mat4));
		vmaUnmapMemory(s_Allocator, m_UniformBuffers[m_CurrentFrame].MemoryHandle);

		VkCommandBufferBeginInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(buffer, &cmdInfo) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to begin command buffer");
			return false;
		}

		std::vector<VkClearValue> clearColors(2);

		clearColors[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
		clearColors[1].depthStencil = {1.0f, 0};

		VkRenderPassBeginInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passInfo.renderArea.offset = {0, 0};
		passInfo.renderArea.extent = m_SwapchainData.CurrentExtent;
		passInfo.framebuffer = m_DefaultFramebuffers[imageId];
		passInfo.renderPass = m_DefaultRenderPass;
		passInfo.pClearValues = clearColors.data();
		passInfo.clearValueCount = clearColors.size();

		vkCmdBeginRenderPass(buffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(buffer, 0, 1, &m_Viewport);
		vkCmdSetScissor(buffer, 0, 1, &m_Scissor);

		vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(buffer, 0, 1, &m_ObjVBO.BufferHandle, &offset);

		vkCmdBindIndexBuffer(buffer, m_ObjIBO.BufferHandle, offset, VK_INDEX_TYPE_UINT32);

		glm::mat4 vp = glm::perspective(glm::radians(45.0f), m_AspectRatio, 0.1f, 100.0f);

		vkCmdPushConstants(buffer, m_DefaultPipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &vp[0][0]);
		vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeLayout, 0, 1, &m_DescSets[m_CurrentFrame], 0, nullptr);

		vkCmdDrawIndexed(buffer, m_NumIndices, 1, 0, 0, 0);

		vkCmdEndRenderPass(buffer);

		if (vkEndCommandBuffer(buffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to end command buffer");
			return false;
		}

		return true;
	}

	VkExtent2D Context::ChooseExtents() const
	{
		const auto& capabilities = m_SwapchainData.Capabilities;

		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		VkExtent2D extent = { Window::Get().GetWidth(), Window::Get().GetHeight() };

		extent.width = std::max(capabilities.minImageExtent.width, std::min(extent.width, capabilities.maxImageExtent.width));
		extent.height = std::max(capabilities.minImageExtent.height, std::min(extent.height, capabilities.maxImageExtent.height));

		return extent;
	}

	VkSurfaceFormatKHR Context::ChooseSurfaceFormat() const
	{
		for (const auto& f : m_SwapchainData.SurfaceFormats)
		{
			if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == VK_FORMAT_R8G8B8A8_SRGB)
				return f;
		}

		return m_SwapchainData.SurfaceFormats[1];
	}

	VkPresentModeKHR Context::ChoosePresentMode() const
	{
		for (const auto& m : m_SwapchainData.PresentModes)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

#ifdef VKP_DEBUG

	VKAPI_ATTR VkResult VKAPI_CALL Context::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
	{
		auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

		if (fn != nullptr)
			return fn(instance, pCreateInfo, pAllocator, pMessenger);

		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	VKAPI_ATTR void VKAPI_CALL Context::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
	{
		auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

		if (fn != nullptr)
		{
			fn(instance, messenger, pAllocator);
			return;
		}

		VKP_ERROR("Debug messenger extension is not present");
	}

	VkBool32 Context::DebugUtilsMessengerCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			VKP_WARN(pCallbackData->pMessage);
			return VK_FALSE;
		}

		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			VKP_ERROR(pCallbackData->pMessage);
			return VK_FALSE;
		}

		VKP_INFO(pCallbackData->pMessage);
		return VK_FALSE;
	}

#endif

}

#undef MAX_CONCURRENT_FRAMES