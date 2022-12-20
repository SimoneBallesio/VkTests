#pragma once

#include "Rendering/Pipeline.hpp"
#include "Rendering/Texture.hpp"

#include <vulkan/vulkan.h>

namespace VKP
{

	struct Material
	{
		std::string Path = "";
		Pipeline* Template = nullptr;
		VkDescriptorSet TextureSet = VK_NULL_HANDLE;
	};

	class MaterialCache final
	{
	public:
		MaterialCache(MaterialCache&) = delete;
		~MaterialCache();

		MaterialCache& operator=(MaterialCache&) = delete;

		Material* Create(const std::string& name, const std::vector<Texture*> textures);

		static MaterialCache* Create(VkDevice device);
		static void Destroy();

		static MaterialCache& Get();

	private:
		VkDevice m_Device;

		static MaterialCache* s_Instance;
		static std::unordered_map<std::string, Material*> s_ResourceMap;

		Pipeline m_DefaultTemplate = {};

		MaterialCache(VkDevice device);
	};

}