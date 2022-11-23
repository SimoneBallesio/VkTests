#pragma once

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

		Context& operator=(Context&) = delete;

		static Context* Create();
		static Context& Get();

	private:
		VkInstance m_Instance = VK_NULL_HANDLE;

		VkPhysicalDevice m_PhysDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;

#ifdef VKP_DEBUG
		VkDebugUtilsMessengerEXT m_Debug = VK_NULL_HANDLE;
#endif

		static inline Context* s_Context = nullptr;

		Context() = default;

		bool CreateInstance();
		bool ChoosePhysicalDevice();
		bool CreateDevice();

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

		static VkBool32 DebugUtilsMessengerCallbackEXT(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageTypes,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

#endif
	};

}