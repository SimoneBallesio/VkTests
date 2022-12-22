#pragma once

#include "Core/FunctionQueue.hpp"

#include "Rendering/Descriptors.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Profiler.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define MAX_CONCURRENT_FRAMES 2

namespace VKP::Impl
{

	struct QueueIndices
	{
		uint32_t Graphics = UINT32_MAX;
		uint32_t Presentation = UINT32_MAX;
		uint32_t Transfer = UINT32_MAX;

		std::vector<uint32_t> ConcurrentQueues;
		bool DedicatedTransferQueue = false;

		bool AreValid() const;
	};

	struct SwapchainData
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkPresentModeKHR> PresentModes;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		VkExtent2D CurrentExtent;
		VkSurfaceFormatKHR Format;
		VkSurfaceFormatKHR PrevFormat;
		VkSampleCountFlagBits NumSamples = VK_SAMPLE_COUNT_1_BIT;
		uint32_t CurrentImageId;

		bool IsValid() const;
	};

	struct FrameData
	{
		VkSemaphore CanAcquireImage = VK_NULL_HANDLE;
		VkSemaphore CanPresentImage = VK_NULL_HANDLE;
		VkFence PrevFrameRenderEnded = VK_NULL_HANDLE;

		DescriptorSetAllocator* DynDescriptorSetAlloc = nullptr;

		VkCommandPool CmdPool = VK_NULL_HANDLE;
		VkCommandBuffer CmdBuffer = VK_NULL_HANDLE;

		VkCommandPool TransferPool = VK_NULL_HANDLE;

		FunctionQueue DeletionQueue = {};
	};

	struct State
	{
		VkInstance Instance = VK_NULL_HANDLE;
		VkPhysicalDevice PhysDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties PhysDeviceProperties = {};

#ifdef VKP_DEBUG

		VkDebugUtilsMessengerEXT Debug = VK_NULL_HANDLE;
		VulkanProfiler* Profiler = nullptr;
		VulkanScopeTimer* FrameTimer = nullptr;

#endif

		VkDevice Device = VK_NULL_HANDLE;
		VmaAllocator MemAllocator = VK_NULL_HANDLE;

		VkSurfaceKHR Surface = VK_NULL_HANDLE;
		uint32_t SurfaceWidth = 0;
		uint32_t SurfaceHeight = 0;

		VkViewport Viewport = {};
		VkRect2D Scissor = {};

		VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
		SwapchainData SwcData = {};

		std::vector<Texture> SwcImages = {};
		Texture SwcDepthTexture = {};
		Texture SwcMsaaTexture = {};

		DescriptorSetAllocator* DescriptorSetAlloc = nullptr;

		DescriptorSetLayoutCache* DescriptorSetLayouts = nullptr;
		ShaderModuleCache* ShaderModules = nullptr;
		PipelineLayoutCache* PipeLayouts = nullptr;

		QueueIndices Indices = {};

		VkQueue GraphicsQueue = VK_NULL_HANDLE;
		VkQueue PresentQueue = VK_NULL_HANDLE;
		VkQueue TransferQueue = VK_NULL_HANDLE;

		FrameData Frames[MAX_CONCURRENT_FRAMES] = {};
		uint32_t CurrentFrame = 0;
		VkCommandBuffer CurrentCmdBuffer = VK_NULL_HANDLE;

		FunctionQueue DeletionQueue = {};

		static State* Data;
	};

	bool SubmitTransfer(State* s, const std::function<void(VkCommandBuffer)>& fn);
	void GetTransferQueueData(State* s, VkSharingMode* mode, uint32_t* numQueues, const uint32_t** queuesPtr);

	uint32_t GetAlignedSize(uint32_t size, uint32_t minAlignment);

	VkSampleCountFlagBits GetMsaaMaxSamples(const VkPhysicalDeviceProperties& props);

}