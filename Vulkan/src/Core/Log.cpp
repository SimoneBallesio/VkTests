#include "Pch.hpp"

#ifdef VKP_DEBUG

#include "Core/Log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace VKP
{

	std::shared_ptr<spdlog::logger> Log::s_Logger;

	void Log::Init()
	{
		spdlog::set_pattern("%^[%T] %n: %v%$");

		s_Logger = spdlog::stdout_color_mt("Engine");
		s_Logger->set_level(spdlog::level::trace);
	}

	std::shared_ptr<spdlog::logger>& Log::GetLogger()
	{
		return s_Logger;
	}

}

#endif