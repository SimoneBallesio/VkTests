#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace VKP
{

	struct Buffer;

	struct Texture
	{
		std::string Path;
		VkImage ImageHandle = VK_NULL_HANDLE;
		VkImageView ViewHandle = VK_NULL_HANDLE;
		VkSampler SamplerHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
		uint32_t MipLevels = 1;
	};

	class TextureCache final
	{
	public:
		TextureCache(TextureCache&) = delete;
		~TextureCache();

		Texture* Create(const std::string& name);
		void Destroy(Texture* texture);

		TextureCache& operator=(TextureCache&) = delete;

		static TextureCache* Create();
		static TextureCache& Get();

	private:
		static TextureCache* s_Instance;
		static std::unordered_map<std::string, Texture*> s_ResourceMap;

		TextureCache() = default;
	};

}

namespace VKP::Impl
{

	struct State;

	bool CreateImage(State* s, Texture* texture, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
	bool PopulateImage(State* s, Texture* texture, Buffer* staging, uint32_t width, uint32_t height);
	bool CreateImageView(State* s, Texture* texture, VkFormat format, VkImageAspectFlags aspectFlags);
	bool CreateImageSampler(State* s, Texture* texture);
	void DestroyTexture(State* s, Texture* texture);

}