#pragma once

#include "Core/Log.hpp"

#ifdef _WIN32
#ifdef _WIN64
#define VKP_PLATFORM_WINDOWS
#endif

#elif defined(__APPLE__)
#define VKP_PLATFORM_APPLE

#else
#error "Only Windows/x64 builds are supported"

#endif

#ifdef VKP_DEBUG

#if defined(VKP_PLATFORM_WINDOWS)
#define DEBUGBREAK() { __debugbreak(); }

#elif defined(VKP_PLATFORM_APPLE)
#define DEBUGBREAK() { __builtin_trap(); }
#endif

#define VKP_TRACE(...)		{ VKP::Log::GetLogger()->trace(__VA_ARGS__); }
#define VKP_INFO(...)		{ VKP::Log::GetLogger()->info(__VA_ARGS__); }
#define VKP_WARN(...)		{ VKP::Log::GetLogger()->warn(__VA_ARGS__); }
#define VKP_ERROR(...)		{ VKP::Log::GetLogger()->error(__VA_ARGS__); }
#define VKP_ASSERT(x, ...)	{ if (!x) { VKP_ERROR(__VA_ARGS__); DEBUGBREAK(); } }

#else

#define DEBUGBREAK()

#define VKP_TRACE(...)
#define VKP_INFO(...)
#define VKP_WARN(...)
#define VKP_ERROR(...)
#define VKP_ASSERT(x, ...)

#endif

#define VK_CHECK_RESULT(f) \
{ \
	VkResult res = (f); \
	if (res != VK_SUCCESS) \
	{ \
		VKP_ERROR("VkResult unsuccessful in {0} at line {1}", __FILE__, __LINE__); \
	} \
}

#if defined(VKP_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR

#elif defined(VKP_PLATFORM_APPLE)
#define VK_USE_PLATFORM_MACOS_MVK
#endif