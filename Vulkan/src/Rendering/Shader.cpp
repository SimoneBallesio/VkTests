#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Shader.hpp"

#include <spirv_reflect.h>

#include <fstream>

namespace VKP
{

	ShaderModuleCache* ShaderModuleCache::s_Instance = nullptr;
	std::unordered_map<std::string, ShaderModule*> ShaderModuleCache::s_ResourceMap = {};

	PipelineLayoutCache* PipelineLayoutCache::s_Instance = nullptr;
	std::unordered_map<size_t, VkPipelineLayout> PipelineLayoutCache::s_ResourceMap = {};

	static constexpr uint32_t FNV1A32(char const* s, std::size_t count)
	{
		return ((count ? FNV1A32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	ShaderModuleCache::~ShaderModuleCache()
	{
		for (auto m : s_ResourceMap)
		{
			vkDestroyShaderModule(m_Device, m.second->ModuleHandle, nullptr);
			delete m.second;
		}

		s_ResourceMap.clear();
	}

	ShaderModule* ShaderModuleCache::Create(const std::string& path)
	{
		auto it = s_ResourceMap.find(path);

		if (it != s_ResourceMap.end())
			return (*it).second;

		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			VKP_ERROR("Unable to locate shader module file: {}", path);
			return nullptr;
		}

		ShaderModule* mod = new ShaderModule();
		const size_t size = file.tellg();
		mod->ByteCode.resize(size);
		
		file.seekg(0);
		file.read((char*)mod->ByteCode.data(), size);

		file.close();

		VkShaderModuleCreateInfo moduleInfo = {};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.pCode = reinterpret_cast<uint32_t*>(mod->ByteCode.data());
		moduleInfo.codeSize = size;

		if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &mod->ModuleHandle) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create shader module: {}", path);
			delete mod;
			return nullptr;
		}

		s_ResourceMap[path] = mod;
		return mod;
	}

	ShaderModuleCache* ShaderModuleCache::Create(VkDevice device)
	{
		if (s_Instance == nullptr)
			s_Instance = new ShaderModuleCache(device);

		return s_Instance;
	}

	void ShaderModuleCache::Destroy()
	{
		if (s_Instance == nullptr) return;
		delete s_Instance;
		s_Instance = nullptr;
	}

	PipelineLayoutCache::~PipelineLayoutCache()
	{
		for (auto it : s_ResourceMap)
			vkDestroyPipelineLayout(m_Device, it.second, nullptr);

		s_ResourceMap.clear();
	}

	VkPipelineLayout PipelineLayoutCache::Create(const VkPipelineLayoutCreateInfo& info)
	{
		const size_t hash = Hash(info);
		auto it = s_ResourceMap.find(hash);

		if (it != s_ResourceMap.end())
			return (*it).second;

		VkPipelineLayout layout;

		if (vkCreatePipelineLayout(m_Device, &info, nullptr, &layout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create pipeline layout");
			return VK_NULL_HANDLE;
		}

		s_ResourceMap[hash] = layout;
		return layout;
	}

	PipelineLayoutCache* PipelineLayoutCache::Create(VkDevice device)
	{
		if (s_Instance == nullptr)
			s_Instance = new PipelineLayoutCache(device);

		return s_Instance;
	}

	void PipelineLayoutCache::Destroy()
	{
		if (s_Instance == nullptr) return;
		delete s_Instance;
		s_Instance = nullptr;
	}

	size_t PipelineLayoutCache::Hash(const VkPipelineLayoutCreateInfo& info) const
	{
		std::stringstream ss = {};
		ss << info.setLayoutCount;
		ss << info.pushConstantRangeCount;

		for (size_t i = 0; i < info.setLayoutCount; i++)
			ss << info.pSetLayouts[i];

		for (size_t i = 0; i < info.pushConstantRangeCount; i++)
		{
			const auto& r = info.pPushConstantRanges[i];
			ss << r.offset;
			ss << r.size;
			ss << r.stageFlags;
		}

		const auto s = ss.str();

		return FNV1A32(s.c_str(), s.size());
	}

	void ShaderEffect::AddStage(ShaderModule* module, VkShaderStageFlagBits stage)
	{
		m_Stages.push_back({ module, stage });
	}

	void ShaderEffect::Reflect(DescriptorSetLayoutCache* setLayoutCache, PipelineLayoutCache* pipeLayoutCache)
	{
		std::vector<std::tuple<uint32_t, VkShaderStageFlagBits, DescriptorList>> descSetList = {};
		std::vector<VkPushConstantRange> constantRanges = {};

		for (const auto& s : m_Stages)
		{
			SpvReflectShaderModule module;
			SpvReflectResult result = spvReflectCreateShaderModule(s.Module->ByteCode.size() * sizeof(uint8_t), s.Module->ByteCode.data(), &module);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				VKP_ERROR("Unabel to reflect shader module");
				return;
			}

			uint32_t count = 0;
			result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				VKP_ERROR("Unabel to reflect shader module descriptors");
				return;
			}

			std::vector<SpvReflectDescriptorSet*> descSets(count);
			spvReflectEnumerateDescriptorSets(&module, &count, descSets.data());

			for (size_t i = 0; i < descSets.size(); i++)
			{
				const auto& set = *descSets[i];
				DescriptorList list = {};

				list.Bindings.reserve(set.binding_count);

				for (size_t j = 0; j < set.binding_count; j++)
				{
					const auto& b = *set.bindings[j];
					auto& binding = list.Bindings.emplace_back();

					binding.binding = b.binding;
					binding.descriptorType = static_cast<VkDescriptorType>(b.descriptor_type);
					binding.descriptorCount = 1;
					binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);

					for (size_t n = 0; n < b.array.dims_count; n++)
						binding.descriptorCount *= b.array.dims[n];
				}

				descSetList.push_back({ set.set, static_cast<VkShaderStageFlagBits>(module.shader_stage), std::move(list) });
			}

			result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				VKP_ERROR("Unabel to reflect shader module push constants");
				return;
			}

			if (count == 0)
			{
				spvReflectDestroyShaderModule(&module);
				continue;
			}

			std::vector<SpvReflectBlockVariable*> pushConstants(count);
			spvReflectEnumeratePushConstantBlocks(&module, &count, pushConstants.data());

			for (const auto p : pushConstants)
			{
				auto& range = constantRanges.emplace_back();
				range.offset = p->offset;
				range.size = p->size;
				range.stageFlags = module.shader_stage;
			}

			spvReflectDestroyShaderModule(&module);
		}

		for (size_t i = 0; i < 4; i++)
		{
			for (const auto& [set, stage, list] : descSetList)
			{
				if (set == i)
				{
					if (i >= m_DescriptorLists.size())
					{
						m_DescriptorLists.push_back(std::move(list));
						continue;
					}

					for (auto& b : m_DescriptorLists[i].Bindings)
						b.stageFlags |= stage;
				}
			}
		}

		VKP_ASSERT(setLayoutCache != nullptr, "Null pointer passed as DescriptorSetLayoutCache");

		m_DescriptorSetLayouts.reserve(m_DescriptorLists.size());

		for (auto& l : m_DescriptorLists)
			m_DescriptorSetLayouts.emplace_back(setLayoutCache->Allocate(l));

		VKP_ASSERT(pipeLayoutCache != nullptr, "Null pointer passed as PipelineLayoutCache");

		VkPipelineLayoutCreateInfo pipeInfo = {};
		pipeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeInfo.pSetLayouts = m_DescriptorSetLayouts.data();
		pipeInfo.setLayoutCount = m_DescriptorSetLayouts.size();
		pipeInfo.pPushConstantRanges = constantRanges.data();
		pipeInfo.pushConstantRangeCount = constantRanges.size();

		m_PipeLayout = pipeLayoutCache->Create(pipeInfo);
	}

}