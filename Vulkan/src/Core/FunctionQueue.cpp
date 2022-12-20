#include "Pch.hpp"

#include "Core/FunctionQueue.hpp"

namespace VKP
{

	void FunctionQueue::Push(std::function<void()>&& fn)
	{
		Queue.push_back(fn);
	}

	void FunctionQueue::Flush()
	{
		for (auto it = Queue.rbegin(); it != Queue.rend(); it++) (*it)();
		Queue.clear();
	}

}