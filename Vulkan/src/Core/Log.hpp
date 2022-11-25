#pragma once

#ifdef VKP_DEBUG

#include <spdlog/spdlog.h>

namespace VKP
{

	class Log
	{
	public:
		static void Init();
		static std::shared_ptr<spdlog::logger>& GetLogger();

	private:
		static std::shared_ptr<spdlog::logger> s_Logger;
	};

}

#endif