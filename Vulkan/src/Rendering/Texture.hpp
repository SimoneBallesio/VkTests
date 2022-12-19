#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace VKP
{

	class Texture
	{
	public:
		std::string Path;
		VkImage ImageHandle = VK_NULL_HANDLE;
		VkImageView ViewHandle = VK_NULL_HANDLE;
		VkSampler SamplerHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
		uint32_t MipLevels = 1;

		Texture() = default;
		~Texture() = default;

		static Texture* Load(const std::string& path);
		static void Destroy(Texture* texture);

	private:
		Texture(const std::string& path): Path(std::move(path)) {}

		static std::unordered_map<std::string, Texture*> s_ResourceMap;
	};
	
}