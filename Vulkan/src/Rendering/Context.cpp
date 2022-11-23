#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Context.hpp"

namespace VKP
{

	Context::~Context()
	{
		if (m_Device != VK_NULL_HANDLE)
			vkDestroyDevice(m_Device, nullptr);

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
		bool success = ChoosePhysicalDevice();

		if (success) success = CreateDevice();

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
			VkPhysicalDeviceFeatures feats = {};
			vkGetPhysicalDeviceFeatures(d, &feats);

			if (feats.multiDrawIndirect)
			{
				m_PhysDevice = d;
				return true;
			}
		}

		return false;
	}

	bool Context::CreateDevice()
	{
		VkDeviceCreateInfo deviceInfo = {};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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