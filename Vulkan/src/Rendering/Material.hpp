#pragma once

#include <vulkan/vulkan.h>

namespace VKP
{

	class Material
	{
	public:
		std::string Path = "";
		VkPipeline Pipe = VK_NULL_HANDLE;
		VkPipelineLayout PipeLayout = VK_NULL_HANDLE;

		Material() = default;
		~Material() = default;

		static Material* Create(const std::string& name);

	private:
		static std::unordered_map<std::string, Material*> s_ResourceMap;
	};

}