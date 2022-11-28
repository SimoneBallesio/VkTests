#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>

#define MAX_CONCURRENT_FRAMES 2

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
		vkDeviceWaitIdle(m_Device);

		if (m_DescPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(m_Device, m_DescPool, nullptr);

		if (m_DescSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_Device, m_DescSetLayout, nullptr);

		for (auto& b : m_UniformBuffers)
		{
			if (b.BufferHandle != VK_NULL_HANDLE)
				vmaDestroyBuffer(m_Allocator, b.BufferHandle, b.MemoryHandle);
		}

		if (m_VertexBuffer.BufferHandle != VK_NULL_HANDLE)
			vmaDestroyBuffer(m_Allocator, m_VertexBuffer.BufferHandle, m_VertexBuffer.MemoryHandle);

		if (m_IndexBuffer.BufferHandle != VK_NULL_HANDLE)
			vmaDestroyBuffer(m_Allocator, m_IndexBuffer.BufferHandle, m_IndexBuffer.MemoryHandle);

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			vkDestroySemaphore(m_Device, m_CanAcquireImage[i], nullptr);
			vkDestroySemaphore(m_Device, m_CanPresentImage[i], nullptr);
			vkDestroyFence(m_Device, m_PrevFrameRenderEnded[i], nullptr);
		}

		if (m_TransferPool != m_CmdPool)
			vkDestroyCommandPool(m_Device, m_TransferPool, nullptr);

		if (m_CmdPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);

		DestroySwapchain();

		if (m_DefaultPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(m_Device, m_DefaultPipeline, nullptr);

		if (m_DefaultPipeLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_Device, m_DefaultPipeLayout, nullptr);

		if (m_DefaultRenderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(m_Device, m_DefaultRenderPass, nullptr);

		if (m_Allocator != VK_NULL_HANDLE)
			vmaDestroyAllocator(m_Allocator);

		if (m_Device != VK_NULL_HANDLE)
			vkDestroyDevice(m_Device, nullptr);

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
		if (success) success = CreateRenderPass();
		if (success) success = CreateFramebuffers();
		if (success) success = CreateUniformBuffers();
		if (success) success = CreateDescriptorSetLayout();
		if (success) success = CreateDescriptorPool();
		if (success) success = AllocateDescriptorSets();
		if (success) success = CreatePipeline();
		if (success) success = CreateCommandPool();
		if (success) success = AllocateCommandBuffer();
		if (success) success = CreateSyncObjects();

		if (success) success = CreateVertexBuffer();
		if (success) success = CreateIndexBuffer();

		return success;
	}

	void Context::SwapBuffers()
	{
		vkWaitForFences(m_Device, 1, &m_PrevFrameRenderEnded[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageId;
		VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_CanAcquireImage[m_CurrentFrame], VK_NULL_HANDLE, &imageId);

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

		vkResetFences(m_Device, 1, &m_PrevFrameRenderEnded[m_CurrentFrame]);

		vkResetCommandBuffer(m_CmdBuffer[m_CurrentFrame], 0);

		RecordCommandBuffer(m_CmdBuffer[m_CurrentFrame], imageId);

		glm::mat4 M = glm::mat4(1.0f);

		void* data = nullptr;
		vmaMapMemory(m_Allocator, m_UniformBuffers[m_CurrentFrame].MemoryHandle, &data);
		memcpy(data, &M[0][0], sizeof(glm::mat4));
		vmaUnmapMemory(m_Allocator, m_UniformBuffers[m_CurrentFrame].MemoryHandle);

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

		if (vkCreateDevice(m_PhysDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan device");
			return false;
		}

		vkGetDeviceQueue(m_Device, m_QueueIndices.Graphics, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_QueueIndices.Presentation, 0, &m_PresentQueue);
		vkGetDeviceQueue(m_Device, m_QueueIndices.Transfer, 0, &m_TransferQueue);

		return true;
	}

	bool Context::CreateMemoryAllocator()
	{
		VmaAllocatorCreateInfo allocInfo = {};
		allocInfo.instance = m_Instance;
		allocInfo.physicalDevice = m_PhysDevice;
		allocInfo.device = m_Device;
		allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

		if (vmaCreateAllocator(&allocInfo, &m_Allocator) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create memory allocator");
			return false;
		}

		return true;
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

		if (indices[0] != indices[1])
		{
			swapInfo.pQueueFamilyIndices = indices;
			swapInfo.queueFamilyIndexCount = 2;
			swapInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		}

		else
		{
			swapInfo.pQueueFamilyIndices = nullptr;
			swapInfo.queueFamilyIndexCount = 0;
			swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		if (vkCreateSwapchainKHR(m_Device, &swapInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
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
		vkDeviceWaitIdle(m_Device);

		m_SwapchainData.PrevFormat = m_SwapchainData.Format;

		DestroySwapchain();

		bool valid = CreateSwapchain();
		if (valid) valid = CreateImageViews();

		if (valid && (m_SwapchainData.PrevFormat.colorSpace != m_SwapchainData.Format.colorSpace || m_SwapchainData.PrevFormat.format != m_SwapchainData.Format.format))
		{
			VKP_WARN("Mismatch between previous and current presentation formats");

			vkDestroyRenderPass(m_Device, m_DefaultRenderPass, nullptr);
			valid = CreateRenderPass();
		}

		m_Viewport.width = (float)m_SwapchainData.CurrentExtent.width;
		m_Viewport.height = (float)m_SwapchainData.CurrentExtent.height;
		m_Scissor.extent = m_SwapchainData.CurrentExtent;
		m_Scissor.offset = { 0, 0 };

		if (valid) valid = CreateFramebuffers();

		return valid;
	}

	void Context::DestroySwapchain()
	{
		for (auto& f : m_DefaultFramebuffers)
			vkDestroyFramebuffer(m_Device, f, nullptr);

		for (auto& i : m_SwapImageViews)
			vkDestroyImageView(m_Device, i, nullptr);

		if (m_Swapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
	}

	bool Context::CreateImageViews()
	{
		uint32_t count = 0;
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, nullptr);

		m_SwapImages.resize(count);
		m_SwapImageViews.resize(count);

		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, m_SwapImages.data());

		std::vector<VkImageViewCreateInfo> viewInfos(count);

		size_t i = 0;

		for (auto& viewInfo : viewInfos)
		{
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.format = m_SwapchainData.Format.format;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.image = m_SwapImages[i];

			if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapImageViews[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create Vulkan swapchain image view #{}", i);
				return false;
			}

			i++;
		}

		return true;
	}

	bool Context::CreateRenderPass()
	{
		VkAttachmentDescription colorPass = {};
		colorPass.samples = VK_SAMPLE_COUNT_1_BIT;
		colorPass.format = m_SwapchainData.Format.format;
		colorPass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorPass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorPass.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = &colorRef;
		subpass.colorAttachmentCount = 1;

		VkSubpassDependency subd = {};
		subd.srcSubpass = VK_SUBPASS_EXTERNAL;
		subd.dstSubpass = 0;
		subd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subd.srcAccessMask = 0;
		subd.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subd.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.pSubpasses = &subpass;
		passInfo.subpassCount = 1;
		passInfo.pAttachments = &colorPass;
		passInfo.attachmentCount = 1;
		passInfo.pDependencies = &subd;
		passInfo.dependencyCount = 1;

		if (vkCreateRenderPass(m_Device, &passInfo, nullptr, &m_DefaultRenderPass) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default render pass");
			return false;
		}

		return true;
	}

	bool Context::CreateFramebuffers()
	{
		m_DefaultFramebuffers.resize(m_SwapImageViews.size());

		for (size_t i = 0; i < m_DefaultFramebuffers.size(); i++)
		{
			VkFramebufferCreateInfo frameInfo = {};
			frameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameInfo.pAttachments = &m_SwapImageViews[i];
			frameInfo.attachmentCount = 1;
			frameInfo.width = m_SwapchainData.CurrentExtent.width;
			frameInfo.height = m_SwapchainData.CurrentExtent.height;
			frameInfo.renderPass = m_DefaultRenderPass;
			frameInfo.layers = 1;

			if (vkCreateFramebuffer(m_Device, &frameInfo, nullptr, &m_DefaultFramebuffers[i]) != VK_SUCCESS)
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
		VkDescriptorSetLayoutBinding binding = {};
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		binding.binding = 0;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pBindings = &binding;
		layoutInfo.bindingCount = 1;

		if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescSetLayout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor set layout");
			return false;
		}

		return true;
	}

	bool Context::CreateDescriptorPool()
	{
		VkDescriptorPoolSize size = {};
		size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		size.descriptorCount = MAX_CONCURRENT_FRAMES;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pPoolSizes = &size;
		poolInfo.poolSizeCount = 1;
		poolInfo.maxSets = MAX_CONCURRENT_FRAMES;

		if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescPool) != VK_SUCCESS)
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

		if (vkAllocateDescriptorSets(m_Device, &setInfo, m_DescSets.data()) != VK_SUCCESS)
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

			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.pBufferInfo = &bufInfo;
			writeInfo.dstSet = m_DescSets[i];
			writeInfo.dstBinding = 0;
			writeInfo.dstArrayElement = 0;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeInfo.descriptorCount = 1;

			vkUpdateDescriptorSets(m_Device, 1, &writeInfo, 0, nullptr);
		}

		return true;
	}

	bool Context::CreatePipeline()
	{
		std::string moduleNames[] = {
			"shaders/base.vert.spv",
			"shaders/base.frag.spv",
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

			if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &modules[i]) != VK_SUCCESS)
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
		vertBind.stride = 5 * sizeof(float);

		std::vector<VkVertexInputAttributeDescription> descriptions;
		descriptions.reserve(2);

		auto& posDesc = descriptions.emplace_back();
		posDesc.format = VK_FORMAT_R32G32_SFLOAT;
		posDesc.location = 0;
		posDesc.binding = 0;
		posDesc.offset = 0;

		auto& colDesc = descriptions.emplace_back();
		colDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		colDesc.location = 1;
		colDesc.binding = 0;
		colDesc.offset = 2 * sizeof(float);

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
		m_Viewport.y = 0.0f;
		m_Viewport.width = (float)m_SwapchainData.CurrentExtent.width;
		m_Viewport.height = (float)m_SwapchainData.CurrentExtent.height;
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
		rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

		if (vkCreatePipelineLayout(m_Device, &pipeLayoutInfo, nullptr, &m_DefaultPipeLayout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default pipeline layout");

			for (auto &m : modules)
				vkDestroyShaderModule(m_Device, m, nullptr);

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

		if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_DefaultPipeline) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default pipeline");

			for (auto &m : modules)
				vkDestroyShaderModule(m_Device, m, nullptr);

			return false;
		}

		for (auto& m : modules)
			vkDestroyShaderModule(m_Device, m, nullptr);

		return true;
	}

	bool Context::CreateVertexBuffer()
	{
		const std::vector<float> vertices = {
			-0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f, 0.5f, 1.0f, 1.0f, 1.0f,
		};

		Buffer staging = {};

		if (!CreateBuffer(staging, vertices.size() * sizeof(float), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(m_Allocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, vertices.data(), vertices.size() * sizeof(float));

		vmaUnmapMemory(m_Allocator, staging.MemoryHandle);

		if (!CreateBuffer(m_VertexBuffer, vertices.size() * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(staging, m_VertexBuffer, vertices.size() * sizeof(float));
		vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);

		return success;
	}

	bool Context::CreateIndexBuffer()
	{
		std::vector<uint32_t> indices = {
			0, 1, 2, 2, 3, 0,
		};

		Buffer staging = {};

		if (!CreateBuffer(staging, indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT))
			return false;

		void* data;
		if (vmaMapMemory(m_Allocator, staging.MemoryHandle, &data) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to map staging buffer for initial copy");
			vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));

		vmaUnmapMemory(m_Allocator, staging.MemoryHandle);

		if (!CreateBuffer(m_IndexBuffer, indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT))
		{
			vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);
			return false;
		}

		bool success = CopyBuffer(staging, m_IndexBuffer, indices.size() * sizeof(uint32_t));
		vmaDestroyBuffer(m_Allocator, staging.BufferHandle, staging.MemoryHandle);

		return success;
	}

	bool Context::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = m_QueueIndices.Graphics;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CmdPool) != VK_SUCCESS)
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

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_TransferPool) != VK_SUCCESS)
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

		if (vkAllocateCommandBuffers(m_Device, &cmdInfo, m_CmdBuffer.data()) != VK_SUCCESS)
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
			if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_CanAcquireImage[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_CanPresentImage[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			if (vkCreateFence(m_Device, &fenceInfo, nullptr, &m_PrevFrameRenderEnded[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create fence");
				return false;
			}
		}

		return true;
	}

	bool Context::RecordCommandBuffer(const VkCommandBuffer& buffer, size_t imageId)
	{
		VkCommandBufferBeginInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(buffer, &cmdInfo) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to begin command buffer");
			return false;
		}

		VkClearValue clearColor = {{{ 0.1f, 0.1f, 0.1f, 1.0f }}};

		VkRenderPassBeginInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passInfo.renderArea.offset = {0, 0};
		passInfo.renderArea.extent = m_SwapchainData.CurrentExtent;
		passInfo.framebuffer = m_DefaultFramebuffers[imageId];
		passInfo.renderPass = m_DefaultRenderPass;
		passInfo.pClearValues = &clearColor;
		passInfo.clearValueCount = 1;

		vkCmdBeginRenderPass(buffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(buffer, 0, 1, &m_Viewport);
		vkCmdSetScissor(buffer, 0, 1, &m_Scissor);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(buffer, 0, 1, &m_VertexBuffer.BufferHandle, &offset);

		vkCmdBindIndexBuffer(buffer, m_IndexBuffer.BufferHandle, offset, VK_INDEX_TYPE_UINT32);

		vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline);

		glm::mat4 vp = glm::perspective(glm::radians(70.0f), m_AspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));

		vkCmdPushConstants(buffer, m_DefaultPipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &vp[0][0]);
		vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeLayout, 0, 1, &m_DescSets[m_CurrentFrame], 0, nullptr);

		vkCmdDrawIndexed(buffer, 6, 1, 0, 0, 0);

		vkCmdEndRenderPass(buffer);

		if (vkEndCommandBuffer(buffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to end command buffer");
			return false;
		}

		return true;
	}

	bool Context::CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags)
	{
		std::set<uint32_t> indexSet = { m_QueueIndices.Graphics, m_QueueIndices.Transfer };
		std::vector<uint32_t> indices(indexSet.begin(), indexSet.end());

		VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = bufferUsage;

		if ((bufferUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) && indices.size() > 1)
		{
			bufInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			bufInfo.pQueueFamilyIndices = indices.data();
			bufInfo.queueFamilyIndexCount = indices.size();
		}

		else
		{
			bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bufInfo.pQueueFamilyIndices = nullptr;
			bufInfo.queueFamilyIndexCount = 0;
		}

		VmaAllocationCreateInfo memInfo = {};
		memInfo.usage = VMA_MEMORY_USAGE_AUTO;
		memInfo.flags = memoryFlags;

		if (vmaCreateBuffer(m_Allocator, &bufInfo, &memInfo, &buffer.BufferHandle, &buffer.MemoryHandle, nullptr) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create buffer");
			return false;
		}

		return true;
	}

	bool Context::CopyBuffer(const Buffer& src, const Buffer& dst, VkDeviceSize size)
	{
		VkCommandBuffer copyBuffer = VK_NULL_HANDLE;

		VkCommandBufferAllocateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufInfo.commandPool = m_TransferPool;
		bufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		bufInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(m_Device, &bufInfo, &copyBuffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to allocate buffer transfer command pool");
			return false;
		}

		VkCommandBufferBeginInfo begInfo = {};
		begInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(copyBuffer, &begInfo) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to begin copy command buffer");
			return false;
		}

		VkBufferCopy region = {};
		region.srcOffset = 0;
		region.dstOffset = 0;
		region.size = size;

		vkCmdCopyBuffer(copyBuffer, src.BufferHandle, dst.BufferHandle, 1, &region);

		if (vkEndCommandBuffer(copyBuffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to end copy command buffer");
			return false;
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyBuffer;

		if (vkQueueSubmit(m_TransferQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to submit copy buffer commands to transfer queue");
			return false;
		}

		vkQueueWaitIdle(m_TransferQueue);

		vkFreeCommandBuffers(m_Device, m_TransferPool, 1, &copyBuffer);

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

		return m_SwapchainData.SurfaceFormats[0];
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