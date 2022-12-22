#pragma once

#ifdef VKP_DEBUG

#include <vulkan/vulkan.h>

namespace VKP
{
	struct ScopeTimer
	{
		uint32_t StartTimestamp;
		uint32_t EndTimestamp;
		std::string Name;
	};

	struct StatRecorder
	{
		uint32_t Query;
		std::string Name;
	};

	struct QueryFrameState
	{
		std::vector<ScopeTimer> FrameTimers;
		VkQueryPool TimerPool;
		uint32_t TimerLast;

		std::vector<StatRecorder> StatRecorders;
		VkQueryPool StatPool;
		uint32_t StatLast;
	};

	class VulkanProfiler
	{
	public:
		bool Init(VkDevice device, float timestampPeriod, uint32_t poolsSize = 100);
		void Cleanup();

		void ParseQueries(VkCommandBuffer cmdBuffer);

		double GetTiming(const std::string& name) const;
		int32_t GetStat(const std::string& name) const;

		VkQueryPool GetTimerPool() const;
		VkQueryPool GetStatPool() const;

		void AddTimer(ScopeTimer& timer);
		void AddStat(StatRecorder& stat);

		uint32_t GetTimestampId();
		uint32_t GetStatId();

	private:
		VkDevice m_Device;

		std::array<QueryFrameState, 3> m_QueryFrames;

		std::unordered_map<std::string, double> m_Timings;
		std::unordered_map<std::string, int32_t> m_Stats;

		uint32_t m_CurrentFrame;
		float m_Period;
	};

	class VulkanScopeTimer
	{
	public:
		VulkanScopeTimer(VkCommandBuffer cmdBuffer, VulkanProfiler* profiler, const char* name);
		~VulkanScopeTimer();

	private:
		VulkanProfiler* m_Profiler = nullptr;
		VkCommandBuffer m_CmdBuffer = VK_NULL_HANDLE;
		ScopeTimer m_Timer = {};
	};

	class VulkanPipelineStatRecorder
	{
	public:
		VulkanPipelineStatRecorder(VkCommandBuffer cmdBuffer, VulkanProfiler* profiler, const char* name);
		~VulkanPipelineStatRecorder();

	private:
		VulkanProfiler* m_Profiler = nullptr;
		VkCommandBuffer m_CmdBuffer = VK_NULL_HANDLE;
		StatRecorder m_Timer = {};
	};

}

#endif