#pragma once

#include "Core/FunctionQueue.hpp"

#include "Rendering/Buffer.hpp"
#include "Rendering/Descriptors.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/VertexData.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKP
{

	class Context final
	{
	public:
		Context(Context&) = delete;

		~Context();

		bool BeforeWindowCreation();
		bool AfterWindowCreation();

		void BeginFrame();
		void EndFrame();

		void OnResize(uint32_t width, uint32_t height);

		Context& operator=(Context&) = delete;

		static Context* Create();
		static Context& Get();

	private:
		bool m_Resized = false;

		static inline Context* s_Context = nullptr;

		Context() = default;
	};

}

namespace VKP::Impl
{

	bool CreateInstance(State* s);

	bool CreateSurface(State* s, void* windowHandle);

	bool ChoosePhysicalDevice(State* s);
	bool CreateDevice(State* s);

	bool CreateMemoryAllocator(State* s);

	bool CreateMsaaTexture(State* s);
	bool CreateDepthTexture(State* s);

	bool CreateSwapchain(State* s);
	bool RecreateSwapchain(State* s);
	void DestroySwapchain(State* s);

	bool CreateImageViews(State* s);

	bool CreateCommandPools(State* s);
	bool AllocateCommandBuffers(State* s);

	bool CreateSyncObjects(State* s);

	bool CreateDescriptorSetAllocators(State* s);
	bool CreateResourceCaches(State* s);

	VkExtent2D ChooseExtents(State* s);
	VkSurfaceFormatKHR ChooseSurfaceFormat(State* s);
	VkPresentModeKHR ChoosePresentMode(State* s);

#ifdef VKP_DEBUG

	VKAPI_ATTR VkResult VKAPI_CALL CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pMessenger);

	VKAPI_ATTR void VKAPI_CALL DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT messenger,
		const VkAllocationCallbacks* pAllocator);

	VkBool32 DebugUtilsMessengerCallbackEXT(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

#endif

}