#pragma once

#include "Core/UID.hpp"

#include "Rendering/Pipeline.hpp"
#include "Rendering/Texture.hpp"

#include <vulkan/vulkan.h>

namespace VKP
{

	struct Material
	{
		UID Uid;
		std::string Path = "";
		Pipeline* Template = nullptr;
		VkDescriptorSet TextureSet = VK_NULL_HANDLE;

		inline operator const uint64_t& () const { return (const uint64_t&)Uid; }
	};

	class MaterialCache final
	{
	public:
		MaterialCache(MaterialCache&) = delete;
		~MaterialCache();

		MaterialCache& operator=(MaterialCache&) = delete;

		Material* Create(const std::string& name, const std::vector<Texture*>& textures);

		static MaterialCache* Create(VkDevice device);
		static MaterialCache& Get();

	private:
		VkDevice m_Device;

		Pipeline m_DefaultTemplate = {};

		static MaterialCache* s_Instance;
		static std::unordered_map<std::string, Material*> s_ResourceMap;

		MaterialCache(VkDevice device);
	};

}