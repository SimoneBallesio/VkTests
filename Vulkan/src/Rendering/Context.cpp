#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Renderable.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/State.hpp"
#include "Rendering/Renderer.hpp"

namespace VKP
{

	Context::~Context()
	{
		vkDeviceWaitIdle(Impl::State::Data->Device);

		Impl::State::Data->DeletionQueue.Flush();

		if (Impl::State::Data->MemAllocator != VK_NULL_HANDLE)
			vmaDestroyAllocator(Impl::State::Data->MemAllocator);

		if (Impl::State::Data->Device != VK_NULL_HANDLE)
			vkDestroyDevice(Impl::State::Data->Device, nullptr);

		if (Impl::State::Data->Surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(Impl::State::Data->Instance, Impl::State::Data->Surface, nullptr);

#ifdef VKP_DEBUG

		if (Impl::State::Data->Debug != VK_NULL_HANDLE)
			Impl::DestroyDebugUtilsMessengerEXT(Impl::State::Data->Instance, Impl::State::Data->Debug, nullptr);

#endif

		if (Impl::State::Data->Instance != VK_NULL_HANDLE)
			vkDestroyInstance(Impl::State::Data->Instance, nullptr);

		delete Impl::State::Data;
	}

	bool Context::BeforeWindowCreation()
	{
		Impl::State::Data = new Impl::State();

		bool success = Impl::CreateInstance(Impl::State::Data);
		return success;
	}

	bool Context::AfterWindowCreation()
	{
		Impl::State::Data->SurfaceWidth = Window::Get().GetWidth();
		Impl::State::Data->SurfaceHeight = Window::Get().GetHeight();

		bool success = Impl::CreateSurface(Impl::State::Data, Window::Get().GetNativeHandle());
		if (success) success = Impl::ChoosePhysicalDevice(Impl::State::Data);
		if (success) success = Impl::CreateDevice(Impl::State::Data);
		if (success) success = Impl::CreateMemoryAllocator(Impl::State::Data);

		if (success) success = Impl::CreateSwapchain(Impl::State::Data);
		if (success) success = Impl::CreateImageViews(Impl::State::Data);
		if (success) success = Impl::CreateMsaaTexture(Impl::State::Data);
		if (success) success = Impl::CreateDepthTexture(Impl::State::Data);

		if (success) Impl::State::Data->DeletionQueue.Push([&]() { Impl::DestroySwapchain(Impl::State::Data); });

		if (success) success = Impl::CreateCommandPools(Impl::State::Data);
		if (success) success = Impl::AllocateCommandBuffers(Impl::State::Data);
		if (success) success = Impl::CreateSyncObjects(Impl::State::Data);
		if (success) success = Impl::CreateDescriptorSetAllocators(Impl::State::Data);
		if (success) success = Impl::CreateResourceCaches(Impl::State::Data);

		return success;
	}

	void Context::BeginFrame()
	{
		vkWaitForFences(Impl::State::Data->Device, 1, &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].PrevFrameRenderEnded, VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(Impl::State::Data->Device, Impl::State::Data->Swapchain, UINT64_MAX, Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CanAcquireImage, VK_NULL_HANDLE, &Impl::State::Data->SwcData.CurrentImageId);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			Impl::RecreateSwapchain(Impl::State::Data);
			return;
		}

		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			VKP_ERROR("Unable to acquire swapchain image");
			return;
		}

		vkResetFences(Impl::State::Data->Device, 1, &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].PrevFrameRenderEnded);

		Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].DynDescriptorSetAlloc->Reset();

		vkResetCommandBuffer(Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CmdBuffer, 0);

		VkCommandBufferBeginInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CmdBuffer, &cmdInfo) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to begin command buffer");
			return;
		}

		Impl::State::Data->CurrentCmdBuffer = Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CmdBuffer;


#if defined(VKP_DEBUG) && !defined(VKP_PLATFORM_APPLE)

		Impl::State::Data->Profiler->ParseQueries(Impl::State::Data->CurrentCmdBuffer);
		Impl::State::Data->FrameTimer = new VulkanScopeTimer(Impl::State::Data->CurrentCmdBuffer, Impl::State::Data->Profiler, "Frame");

#endif

	}

	void Context::EndFrame()
	{

#if defined(VKP_DEBUG) && !defined(VKP_PLATFORM_APPLE)

		delete Impl::State::Data->FrameTimer;

#endif

		if (vkEndCommandBuffer(Impl::State::Data->CurrentCmdBuffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to end command buffer");
			return;
		}

		const VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CmdBuffer;
		submitInfo.commandBufferCount = 1;
		submitInfo.pWaitSemaphores = &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CanAcquireImage; // Wait what?
		submitInfo.pWaitDstStageMask = stages; // To do what?
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CanPresentImage;
		submitInfo.signalSemaphoreCount = 1;

		if (vkQueueSubmit(Impl::State::Data->PresentQueue, 1, &submitInfo, Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].PrevFrameRenderEnded) != VK_SUCCESS)
		{
			VKP_ASSERT(false, "Unable to submit queue for rendering");
			return;
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pSwapchains = &Impl::State::Data->Swapchain;
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].CanPresentImage;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &Impl::State::Data->SwcData.CurrentImageId;

		VkResult result = vkQueuePresentKHR(Impl::State::Data->PresentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Resized)
		{
			m_Resized = false;
			Impl::RecreateSwapchain(Impl::State::Data);
		}

		else if (result != VK_SUCCESS)
		{
			VKP_ERROR("Unable to present image to the swapchain");
			return;
		}

		Impl::State::Data->CurrentFrame = (Impl::State::Data->CurrentFrame + 1) % MAX_CONCURRENT_FRAMES;
	}

	void Context::OnResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		Impl::State::Data->SurfaceWidth = width;
		Impl::State::Data->SurfaceHeight = height;

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

}

namespace VKP::Impl
{

	bool CreateInstance(State* s)
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
		dbgInfo.pfnUserCallback = &DebugUtilsMessengerCallbackEXT;

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

		if (vkCreateInstance(&instInfo, nullptr, &s->Instance) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan instance");
			return false;
		}

#ifdef VKP_DEBUG
		if (CreateDebugUtilsMessengerEXT(s->Instance, &dbgInfo, nullptr, &s->Debug) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan debug messenger");
			return false;
		}
#endif

		return true;
	}

	bool CreateSurface(State* s, void* windowHandle)
	{
		SDL_Window* window = (SDL_Window*)windowHandle;

		if (SDL_Vulkan_CreateSurface(window, s->Instance, &s->Surface) == SDL_FALSE)
		{
			VKP_ERROR("Unable to create Vulkan surface");
			return false;
		}

		return true;
	}

	bool ChoosePhysicalDevice(State* s)
	{
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(s->Instance, &count, nullptr);

		if (count == 0)
		{
			VKP_ERROR("Unable to find any suitable physical device for Vulkan");
			return false;
		}

		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(s->Instance, &count, devices.data());

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
					s->Indices.Graphics = i;
					s->Indices.Transfer = i;
				}

				else if (q.queueFlags & VK_QUEUE_TRANSFER_BIT)
					s->Indices.Transfer = i;

				VkBool32 supported = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(d, i, s->Surface, &supported);

				if (supported == VK_TRUE)
					s->Indices.Presentation = i;

				if (s->Indices.Graphics != s->Indices.Transfer && s->Indices.AreValid()) break;
			}

			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(d, s->Surface, &formatCount, nullptr);

			if (formatCount == 0) continue;

			s->SwcData.SurfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(d, s->Surface, &formatCount, s->SwcData.SurfaceFormats.data());

			uint32_t presentCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(d, s->Surface, &presentCount, nullptr);

			if (presentCount == 0) continue;

			s->SwcData.PresentModes.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(d, s->Surface, &presentCount, s->SwcData.PresentModes.data());

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

			if (s->Indices.AreValid() && s->SwcData.IsValid() && requiredExtensions.empty())
			{
				std::set<uint32_t> indices = { s->Indices.Graphics, s->Indices.Transfer };

				s->Indices.ConcurrentQueues = { indices.begin(), indices.end() };
				s->Indices.DedicatedTransferQueue = s->Indices.ConcurrentQueues.size() > 1;

				s->PhysDevice = d;

				vkGetPhysicalDeviceProperties(d, &s->PhysDeviceProperties);

				VkSampleCountFlagBits maxSamples = GetMsaaMaxSamples(s->PhysDeviceProperties);
				s->SwcData.NumSamples = std::min(VK_SAMPLE_COUNT_8_BIT, maxSamples);

				return true;
			}
		}

		VKP_ERROR("Unable to find any suitable physical device for Vulkan");
		return false;
	}

	bool CreateDevice(State* s)
	{
		std::set<uint32_t> indicesSet = { s->Indices.Graphics, s->Indices.Presentation, s->Indices.Transfer };
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

#if defined(VKP_DEBUG) && !defined(VKP_PLATFORM_APPLE)

		features.pipelineStatisticsQuery = VK_TRUE;

#endif

		VkPhysicalDeviceShaderDrawParametersFeatures drawParamFeatures = {};
		drawParamFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;
		drawParamFeatures.shaderDrawParameters = VK_TRUE;

		VkDeviceCreateInfo deviceInfo = {};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pQueueCreateInfos = queueInfos.data();
		deviceInfo.queueCreateInfoCount = queueInfos.size();
		deviceInfo.ppEnabledLayerNames = nullptr;
		deviceInfo.enabledLayerCount = 0;
		deviceInfo.ppEnabledExtensionNames = extensions.data();
		deviceInfo.enabledExtensionCount = extensions.size();
		deviceInfo.pEnabledFeatures = &features;
		deviceInfo.pNext = &drawParamFeatures;

		if (vkCreateDevice(s->PhysDevice, &deviceInfo, nullptr, &s->Device) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan device");
			return false;
		}

		vkGetDeviceQueue(s->Device, s->Indices.Graphics, 0, &s->GraphicsQueue);
		vkGetDeviceQueue(s->Device, s->Indices.Presentation, 0, &s->PresentQueue);
		vkGetDeviceQueue(s->Device, s->Indices.Transfer, 0, &s->TransferQueue);

		return true;
	}

	bool CreateMemoryAllocator(State* s)
	{
		VmaAllocatorCreateInfo allocInfo = {};
		allocInfo.instance = s->Instance;
		allocInfo.physicalDevice = s->PhysDevice;
		allocInfo.device = s->Device;
		allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

		if (vmaCreateAllocator(&allocInfo, &s->MemAllocator) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create memory allocator");
			return false;
		}

		return true;
	}

	bool CreateMsaaTexture(State* s)
	{
		bool success = CreateImage(s, &s->SwcMsaaTexture, s->SwcData.CurrentExtent.width, s->SwcData.CurrentExtent.height, s->SwcData.Format.format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, s->SwcData.NumSamples);
		if (success) success = CreateImageView(s, &s->SwcMsaaTexture, s->SwcData.Format.format, VK_IMAGE_ASPECT_COLOR_BIT);

		return success;
	}

	bool CreateDepthTexture(State* s)
	{
		bool success = CreateImage(s, &s->SwcDepthTexture, s->SwcData.CurrentExtent.width, s->SwcData.CurrentExtent.height, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, s->SwcData.NumSamples);
		if (success) success = CreateImageView(s, &s->SwcDepthTexture, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

		return success;
	}

	bool CreateSwapchain(State* s)
	{
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->PhysDevice, s->Surface, &s->SwcData.Capabilities);

		const auto& capabilities = s->SwcData.Capabilities;
		const VkExtent2D extents = ChooseExtents(s);
		const VkSurfaceFormatKHR format = ChooseSurfaceFormat(s);
		const VkPresentModeKHR presentMode = ChoosePresentMode(s);
		const uint32_t numImages = std::max(capabilities.minImageCount, std::min(capabilities.minImageCount + 1, capabilities.maxImageCount));
		const uint32_t indices[] = { s->Indices.Graphics, s->Indices.Presentation };

		VkSwapchainCreateInfoKHR swapInfo = {};
		swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapInfo.surface = s->Surface;
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

		if (vkCreateSwapchainKHR(s->Device, &swapInfo, nullptr, &s->Swapchain) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create Vulkan swapchain");
			return false;
		}

		s->SwcData.CurrentExtent = extents;
		s->SwcData.Format = format;

		s->Viewport.x = 0.0f;
		s->Viewport.y = static_cast<float>(s->SwcData.CurrentExtent.height);
		s->Viewport.width = static_cast<float>(s->SwcData.CurrentExtent.width);
		s->Viewport.height = static_cast<float>(s->SwcData.CurrentExtent.height) * -1.0f;
		s->Viewport.minDepth = 0.0f;
		s->Viewport.maxDepth = 1.0f;

		s->Scissor.extent = s->SwcData.CurrentExtent;
		s->Scissor.offset = { 0, 0 };

		return true;
	}

	bool RecreateSwapchain(State* s)
	{
		vkDeviceWaitIdle(s->Device);

		s->SwcData.PrevFormat = s->SwcData.Format;

		DestroySwapchain(s);

		bool valid = CreateSwapchain(s);
		if (valid) valid = CreateImageViews(s);
		if (valid) valid = CreateMsaaTexture(s);
		if (valid) valid = CreateDepthTexture(s);
		if (valid) valid = Renderer3D::OnResize(s->SurfaceWidth, s->SurfaceHeight);

		return valid;
	}

	void DestroySwapchain(State* s)
	{
		if (s->SwcDepthTexture.ViewHandle != VK_NULL_HANDLE)
			vkDestroyImageView(s->Device, s->SwcDepthTexture.ViewHandle, nullptr);

		if (s->SwcDepthTexture.ImageHandle != VK_NULL_HANDLE)
			vmaDestroyImage(s->MemAllocator, s->SwcDepthTexture.ImageHandle, s->SwcDepthTexture.MemoryHandle);

		if (s->SwcMsaaTexture.ViewHandle != VK_NULL_HANDLE)
			vkDestroyImageView(s->Device, s->SwcMsaaTexture.ViewHandle, nullptr);

		if (s->SwcMsaaTexture.ImageHandle != VK_NULL_HANDLE)
			vmaDestroyImage(s->MemAllocator, s->SwcMsaaTexture.ImageHandle, s->SwcMsaaTexture.MemoryHandle);

		for (auto& i : s->SwcImages)
			vkDestroyImageView(s->Device, i.ViewHandle, nullptr);

		if (s->Swapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(s->Device, s->Swapchain, nullptr);
	}

	bool CreateImageViews(State* s)
	{
		uint32_t count = 0;
		vkGetSwapchainImagesKHR(s->Device, s->Swapchain, &count, nullptr);

		std::vector<VkImage> images(count);
		s->SwcImages.resize(count);

		vkGetSwapchainImagesKHR(s->Device, s->Swapchain, &count, images.data());

		std::vector<VkImageViewCreateInfo> viewInfos(count);

		for (size_t i = 0; i < count; i++)
		{
			auto& t = s->SwcImages[i];
			t.ImageHandle = images[i];

			if (!CreateImageView(s, &t, s->SwcData.Format.format, VK_IMAGE_ASPECT_COLOR_BIT))
			{
				VKP_ERROR("Unable to create Vulkan swapchain image view #{}", i);
				return false;
			}
		}

		return true;
	}

	bool CreateCommandPools(State* s)
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = s->Indices.Graphics;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			if (vkCreateCommandPool(s->Device, &poolInfo, nullptr, &s->Frames[i].CmdPool) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create graphics/presentation command pool");
				return false;
			}

			s->DeletionQueue.Push([=]() { vkDestroyCommandPool(s->Device, s->Frames[i].CmdPool, nullptr); });

			if (!s->Indices.DedicatedTransferQueue)
			{
				s->Frames[i].TransferPool = s->Frames[i].CmdPool;
				continue;
			}

			poolInfo.queueFamilyIndex = s->Indices.Transfer;

			if (vkCreateCommandPool(s->Device, &poolInfo, nullptr, &s->Frames[i].TransferPool) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create presentation command pool");
				return false;
			}

			s->DeletionQueue.Push([=]() { vkDestroyCommandPool(s->Device, s->Frames[i].TransferPool, nullptr); });
		}

		return true;
	}

	bool AllocateCommandBuffers(State* s)
	{
		VkCommandBufferAllocateInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandBufferCount = 1;

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			cmdInfo.commandPool = s->Frames[i].CmdPool;

			if (vkAllocateCommandBuffers(s->Device, &cmdInfo, &s->Frames[i].CmdBuffer) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to allocate presentation command buffer");
				return false;
			}
		}

		return true;
	}

	bool CreateSyncObjects(State* s)
	{
		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			if (vkCreateSemaphore(s->Device, &semInfo, nullptr, &s->Frames[i].CanAcquireImage) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			s->DeletionQueue.Push([=]() { vkDestroySemaphore(s->Device, s->Frames[i].CanAcquireImage, nullptr); });

			if (vkCreateSemaphore(s->Device, &semInfo, nullptr, &s->Frames[i].CanPresentImage) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create semaphore");
				return false;
			}

			s->DeletionQueue.Push([=]() { vkDestroySemaphore(s->Device, s->Frames[i].CanPresentImage, nullptr); });

			if (vkCreateFence(s->Device, &fenceInfo, nullptr, &s->Frames[i].PrevFrameRenderEnded) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create fence");
				return false;
			}

			s->DeletionQueue.Push([=]() { vkDestroyFence(s->Device, s->Frames[i].PrevFrameRenderEnded, nullptr); });
		}

		fenceInfo.flags = 0;

		if (vkCreateFence(s->Device, &fenceInfo, nullptr, &s->TransferCompleted))
		{
			VKP_ERROR("Unable to create transfer fence");
			return false;
		}

		s->DeletionQueue.Push([=]() { vkDestroyFence(s->Device, s->TransferCompleted, nullptr); });

		return true;
	}

	bool CreateDescriptorSetAllocators(State* s)
	{
		s->DescriptorSetAlloc = DescriptorSetAllocator::Create(s->Device);

		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
			s->Frames[i].DynDescriptorSetAlloc = DescriptorSetAllocator::Create(s->Device);

		s->DeletionQueue.Push([=]() {
			for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
				delete s->Frames[i].DynDescriptorSetAlloc;

			delete s->DescriptorSetAlloc;
		});

		return true;
	}

	bool CreateResourceCaches(State* s)
	{
		s->DescriptorSetLayouts = DescriptorSetLayoutCache::Create(s->Device);
		s->ShaderModules = ShaderModuleCache::Create(s->Device);
		s->PipeLayouts = PipelineLayoutCache::Create(s->Device);

#if defined(VKP_DEBUG) && !defined(VKP_PLATFORM_APPLE)

		s->Profiler = new VulkanProfiler();
		s->Profiler->Init(s->Device, s->PhysDeviceProperties.limits.timestampPeriod);

#endif

		s->DeletionQueue.Push([=]() {

#if defined(VKP_DEBUG) && !defined(VKP_PLATFORM_APPLE)
			delete s->Profiler;
#endif

			delete s->DescriptorSetLayouts;
			delete s->ShaderModules;
			delete s->PipeLayouts;
		});

		return true;
	}

	VkExtent2D ChooseExtents(State* s)
	{
		const auto& capabilities = s->SwcData.Capabilities;

		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		VkExtent2D extent = { s->SurfaceWidth, s->SurfaceHeight };

		extent.width = std::max(capabilities.minImageExtent.width, std::min(extent.width, capabilities.maxImageExtent.width));
		extent.height = std::max(capabilities.minImageExtent.height, std::min(extent.height, capabilities.maxImageExtent.height));

		return extent;
	}

	VkSurfaceFormatKHR ChooseSurfaceFormat(State* s)
	{
		for (const auto& f : s->SwcData.SurfaceFormats)
		{
			if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				if (f.format == VK_FORMAT_R8G8B8A8_SRGB || f.format == VK_FORMAT_B8G8R8A8_SRGB)
					return f;
			}
		}

		return s->SwcData.SurfaceFormats[0];
	}

	VkPresentModeKHR ChoosePresentMode(State* s)
	{
		for (const auto& m : s->SwcData.PresentModes)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

#ifdef VKP_DEBUG

	VKAPI_ATTR VkResult VKAPI_CALL CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
	{
		auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

		if (fn != nullptr)
			return fn(instance, pCreateInfo, pAllocator, pMessenger);

		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	VKAPI_ATTR void VKAPI_CALL DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
	{
		auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

		if (fn != nullptr)
		{
			fn(instance, messenger, pAllocator);
			return;
		}

		VKP_ERROR("Debug messenger extension is not present");
	}

	VkBool32 DebugUtilsMessengerCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
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