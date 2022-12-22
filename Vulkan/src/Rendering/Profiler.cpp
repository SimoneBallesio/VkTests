#include "Pch.hpp"

#ifdef VKP_DEBUG

#include "Core/Definitions.hpp"

#include "Rendering/Profiler.hpp"

namespace VKP
{

	bool VulkanProfiler::Init(VkDevice device, float period, uint32_t poolsSize)
	{
		m_Device = device;
		m_Period = period;
		m_CurrentFrame = 0;

		VkQueryPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		poolInfo.queryCount = poolsSize;

		for (size_t i = 0; i < 3; i++)
		{
			if (vkCreateQueryPool(device, &poolInfo, nullptr, &m_QueryFrames[i].TimerPool) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create timing query pool");
				return false;
			}

			m_QueryFrames[i].TimerLast = 0;
		}

		poolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;

		for (size_t i = 0; i < 3; i++)
		{
			poolInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;

			if (vkCreateQueryPool(device, &poolInfo, nullptr, &m_QueryFrames[i].StatPool) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create stats query pool");
				return false;
			}

			m_QueryFrames[i].StatLast = 0;
		}

		return true;
	}

	void VulkanProfiler::Cleanup()
	{
		for (auto& f : m_QueryFrames)
		{
			vkDestroyQueryPool(m_Device, f.TimerPool, nullptr);
			vkDestroyQueryPool(m_Device, f.StatPool, nullptr);
		}
	}

	void VulkanProfiler::ParseQueries(VkCommandBuffer cmdBuffer)
	{
		const uint32_t frame = m_CurrentFrame;
		m_CurrentFrame = (m_CurrentFrame + 1) % 3;

		vkCmdResetQueryPool(cmdBuffer, m_QueryFrames[m_CurrentFrame].TimerPool, 0, m_QueryFrames[m_CurrentFrame].TimerLast);
		
		m_QueryFrames[m_CurrentFrame].TimerLast = 0;
		m_QueryFrames[m_CurrentFrame].FrameTimers.clear();

		vkCmdResetQueryPool(cmdBuffer, m_QueryFrames[m_CurrentFrame].StatPool, 0, m_QueryFrames[m_CurrentFrame].StatLast);
		
		m_QueryFrames[m_CurrentFrame].StatLast = 0;
		m_QueryFrames[m_CurrentFrame].StatRecorders.clear();

		QueryFrameState& f = m_QueryFrames[frame];

		std::vector<uint64_t> queryState = {};

		if (f.TimerLast > 0)
		{
			queryState.resize(f.TimerLast);
			vkGetQueryPoolResults(m_Device, f.TimerPool, 0, f.TimerLast, queryState.size() * sizeof(uint64_t), queryState.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
		}

		std::vector<uint64_t> statsState = {};

		if (f.StatLast > 0)
		{
			statsState.resize(f.StatLast);
			vkGetQueryPoolResults(m_Device, f.StatPool, 0, f.StatLast, statsState.size() * sizeof(uint64_t), statsState.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
		}

		for (auto& t : f.FrameTimers)
		{
			uint64_t begin = queryState[t.StartTimestamp];
			uint64_t end = queryState[t.EndTimestamp];
			uint64_t timestamp = end - begin;

			m_Timings[t.Name] = (double(timestamp) * m_Period) / 1000000.0;
		}

		for (auto& r : f.StatRecorders)
		{
			uint64_t result = statsState[r.Query];
			m_Stats[r.Name] = static_cast<int32_t>(result);
		}
	}

	double VulkanProfiler::GetTiming(const std::string& name) const
	{
		auto it = m_Timings.find(name);

		if (it != m_Timings.end())
			return (*it).second;

		return 0;
	}

	int32_t VulkanProfiler::GetStat(const std::string& name) const
	{
		auto it = m_Stats.find(name);

		if (it != m_Stats.end())
			return (*it).second;

		return 0;
	}

	VkQueryPool VulkanProfiler::GetTimerPool() const
	{
		return m_QueryFrames[m_CurrentFrame].TimerPool;
	}


	VkQueryPool VulkanProfiler::GetStatPool() const
	{
		return m_QueryFrames[m_CurrentFrame].StatPool;
	}

	void VulkanProfiler::AddTimer(ScopeTimer& timer)
	{
		m_QueryFrames[m_CurrentFrame].FrameTimers.push_back(timer);
	}


	void VulkanProfiler::AddStat(StatRecorder& stat)
	{
		m_QueryFrames[m_CurrentFrame].StatRecorders.push_back(stat);
	}

	uint32_t VulkanProfiler::GetTimestampId()
	{
		uint32_t q = m_QueryFrames[m_CurrentFrame].TimerLast++;
		return q;
	}


	uint32_t VulkanProfiler::GetStatId()
	{
		uint32_t q = m_QueryFrames[m_CurrentFrame].StatLast++;
		return q;
	}

	VulkanScopeTimer::VulkanScopeTimer(VkCommandBuffer cmdBuffer, VulkanProfiler* profiler, const char* name)
	{
		m_CmdBuffer = cmdBuffer;
		m_Profiler = profiler;
		m_Timer.Name = name;
		m_Timer.StartTimestamp = m_Profiler->GetTimestampId();

		VkQueryPool pool = m_Profiler->GetTimerPool();
		vkCmdWriteTimestamp(m_CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, m_Timer.StartTimestamp);
	}

	VulkanScopeTimer::~VulkanScopeTimer()
	{
		m_Timer.EndTimestamp = m_Profiler->GetTimestampId();
		VkQueryPool pool = m_Profiler->GetTimerPool();
		vkCmdWriteTimestamp(m_CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, m_Timer.EndTimestamp);

		m_Profiler->AddTimer(m_Timer);
	}

	VulkanPipelineStatRecorder::VulkanPipelineStatRecorder(VkCommandBuffer cmdBuffer, VulkanProfiler* profiler, const char* name)
	{
		m_CmdBuffer = cmdBuffer;
		m_Profiler = profiler;
		m_Timer.Name = name;
		m_Timer.Query = m_Profiler->GetStatId();

		VkQueryPool pool = m_Profiler->GetStatPool();
		vkCmdBeginQuery(m_CmdBuffer, pool, m_Timer.Query, 0);
	}


	VulkanPipelineStatRecorder::~VulkanPipelineStatRecorder()
	{
		VkQueryPool pool = m_Profiler->GetStatPool();
		vkCmdEndQuery(m_CmdBuffer, pool, m_Timer.Query);

		m_Profiler->AddStat(m_Timer);
	}

}

#endif