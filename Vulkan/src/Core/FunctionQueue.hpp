#pragma once

#include <deque>

namespace VKP
{

	struct FunctionQueue
	{
		std::deque<std::function<void()>> Queue;

		void Push(std::function<void()>&& fn);
		void Flush();
	};
	
}