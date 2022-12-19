#pragma once

#include "Rendering/Descriptors.hpp"

#include <vulkan/vulkan.h>

namespace VKP
{

	struct ShaderModule
	{
		VkShaderModule ModuleHandle = VK_NULL_HANDLE;
		std::vector<uint8_t> ByteCode = {};
	};

	struct ShaderStage
	{
		ShaderModule* Module = nullptr;
		VkShaderStageFlagBits Stage;
	};

	class ShaderModuleCache final
	{
	public:
		ShaderModuleCache(ShaderModuleCache&) = delete;
		~ShaderModuleCache();

		ShaderModule* Create(const std::string& path);

		ShaderModuleCache& operator=(ShaderModuleCache&) = delete;

		static ShaderModuleCache* Create(VkDevice device);
		static void Destroy();

	private:
		VkDevice m_Device;

		static ShaderModuleCache* s_Instance;
		static std::unordered_map<std::string, ShaderModule*> s_ResourceMap;

		ShaderModuleCache(VkDevice device): m_Device(device) {}
	};

	class PipelineLayoutCache final
	{
	public:
		PipelineLayoutCache(PipelineLayoutCache&) = delete;
		~PipelineLayoutCache();

		VkPipelineLayout Create(const VkPipelineLayoutCreateInfo& info);

		PipelineLayoutCache& operator=(PipelineLayoutCache&) = delete;

		static PipelineLayoutCache* Create(VkDevice device);
		static void Destroy();

	private:
		VkDevice m_Device;

		static PipelineLayoutCache* s_Instance;
		static std::unordered_map<size_t, VkPipelineLayout> s_ResourceMap;

		PipelineLayoutCache(VkDevice device) : m_Device(device) {}

		size_t Hash(const VkPipelineLayoutCreateInfo& info) const;
	};

	class ShaderEffect final
	{
	public:
		ShaderEffect() = default;
		ShaderEffect(ShaderEffect&) = delete;

		void AddStage(ShaderModule* module, VkShaderStageFlagBits stage);
		void Reflect(DescriptorSetLayoutCache* setLayoutCache, PipelineLayoutCache* pipeLayoutCache);

		~ShaderEffect() = default;

	private:
		std::vector<ShaderStage> m_Stages = {};
		std::vector<DescriptorList> m_DescriptorLists = {};

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts = {};
		VkPipelineLayout m_PipeLayout = VK_NULL_HANDLE;
	};

}