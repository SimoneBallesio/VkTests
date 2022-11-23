#pragma once

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