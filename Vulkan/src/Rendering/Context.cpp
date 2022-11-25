#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <fstream>

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
		if (m_CmdPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);

		if (m_DefaultPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(m_Device, m_DefaultPipeline, nullptr);

		if (m_DefaultPipeLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_Device, m_DefaultPipeLayout, nullptr);

		for (auto &f : m_DefaultFramebuffers)
			vkDestroyFramebuffer(m_Device, f, nullptr);

		if (m_DefaultRenderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(m_Device, m_DefaultRenderPass, nullptr);

		for (auto& i : m_SwapImageViews)
			vkDestroyImageView(m_Device, i, nullptr);

		if (m_Swapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

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
		if (success) success = CreateSwapchain();
		if (success) success = CreateImageViews();
		if (success) success = CreateRenderPass();
		if (success) success = CreateFramebuffers();
		if (success) success = CreatePipeline();
		if (success) success = CreateCommandPool();
		if (success) success = AllocateCommandBuffer();

		return success;
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
					m_QueueIndices.Graphics = i;

				if (q.queueFlags & VK_QUEUE_TRANSFER_BIT)
					m_QueueIndices.Transfer = i;

				VkBool32 supported = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(d, i, m_Surface, &supported);

				if (supported == VK_TRUE)
					m_QueueIndices.Presentation = i;

				if (m_QueueIndices.AreValid()) break;
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

		VkRenderPassCreateInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.pSubpasses = &subpass;
		passInfo.subpassCount = 1;
		passInfo.pAttachments = &colorPass;
		passInfo.attachmentCount = 1;

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

		VkPipelineVertexInputStateCreateInfo vertInfo = {};
		vertInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertInfo.vertexAttributeDescriptionCount = 0;
		vertInfo.vertexBindingDescriptionCount = 0;

		VkPipelineInputAssemblyStateCreateInfo inputInfo = {};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputInfo.primitiveRestartEnable = VK_FALSE;

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

		VkPipelineLayoutCreateInfo pipeLayoutInfo = {};
		pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

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

	bool Context::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = m_QueueIndices.Presentation;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CmdPool) != VK_SUCCESS)
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
		cmdInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(m_Device, &cmdInfo, &m_CmdBuffer) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to allocate presentation command buffer");
			return false;
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

		VkViewport view = {};
		view.x = 0.0f;
		view.y = 0.0f;
		view.width = (float)m_SwapchainData.CurrentExtent.width;
		view.height = (float)m_SwapchainData.CurrentExtent.height;
		view.minDepth = 0.0f;
		view.maxDepth = 1.0f;

		vkCmdSetViewport(buffer, 0, 1, &view);

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapchainData.CurrentExtent;

		vkCmdSetScissor(buffer, 0, 1, &scissor);

		vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline);

		vkCmdDraw(buffer, 3, 1, 0, 0);

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