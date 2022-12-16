#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Shader.hpp"

#include <spirv_reflect.h>

#include <fstream>

namespace VKP
{

	ShaderModuleCache* ShaderModuleCache::s_Instance = nullptr;
	std::unordered_map<std::string, ShaderModule*> ShaderModuleCache::s_ResourceMap = {};

	// PipelineLayoutCache* PipelineLayoutCache::s_Instance = nullptr;
	// std::unordered_map<size_t, VkPipelineLayout> PipelineLayoutCache::s_ResourceMap = {};

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

	// PipelineLayoutCache::~PipelineLayoutCache()
	// {
	// 	for (auto it : s_ResourceMap)
	// 		vkDestroyPipelineLayout(m_Device, it.second, nullptr);

	// 	s_ResourceMap.clear();
	// }

	// VkPipelineLayout PipelineLayoutCache::Create(const VkPipelineLayoutCreateInfo& info)
	// {
	// 	const size_t hash = Hash(info);
	// 	auto it = s_ResourceMap.find(hash);

	// 	if (it != s_ResourceMap.end())
	// 		return (*it).second;

	// 	VkPipelineLayout layout;

	// 	if (vkCreatePipelineLayout(m_Device, &info, nullptr, &layout) != VK_SUCCESS)
	// 	{
	// 		VKP_ERROR("Unable to create pipeline layout");
	// 		return VK_NULL_HANDLE;
	// 	}

	// 	s_ResourceMap[hash] = layout;
	// 	return layout;
	// }

	void ShaderEffect::AddStage(ShaderModule* module, VkShaderStageFlagBits stage)
	{
		m_Stages.push_back({ module, stage });
	}

	void ShaderEffect::Reflect(VkDevice device)
	{
		for (const auto& s : m_Stages)
		{
			SpvReflectShaderModule module;
			SpvReflectResult result = spvReflectCreateShaderModule(s.Module->ByteCode.size() * sizeof(uint8_t), s.Module->ByteCode.data(), &module);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
				return;

			uint32_t count = 0;
			result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
				return;

			std::vector<SpvReflectDescriptorSet*> descSets(count);
			spvReflectEnumerateDescriptorSets(&module, &count, descSets.data());

			m_DescriptorList.reserve(count);

			for (size_t i = 0; i < descSets.size(); i++)
			{
				const auto& set = *descSets[i];
				auto& list = m_DescriptorList.emplace_back();

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
			}

			spvReflectDestroyShaderModule(&module);
		}
	}

}