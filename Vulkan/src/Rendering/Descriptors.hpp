#pragma once

#include <vulkan/vulkan.h>

namespace VKP
{

	struct DescriptorPoolSizes
	{
		std::pair<VkDescriptorType, float> Sizes[11] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.0f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.0f },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f },
		};
	};

	class DescriptorAllocator final
	{
		friend class DescriptorCache;

	public:
		DescriptorAllocator(DescriptorAllocator&) = delete;
		~DescriptorAllocator();

		void ResetPools();

		bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

		DescriptorAllocator& operator=(DescriptorAllocator&) = delete;

		static DescriptorAllocator* Create(VkDevice device);

	private:
		VkDevice m_Device = VK_NULL_HANDLE;

		DescriptorPoolSizes m_Sizes = {};

		std::vector<VkDescriptorPool> m_FreePools = {};
		std::vector<VkDescriptorPool> m_UsedPools = {};

		VkDescriptorPool m_CurrentPool = VK_NULL_HANDLE;

		DescriptorAllocator(VkDevice device) : m_Device(device) {}

		VkDescriptorPool GrabFreePool();
	};

	struct DescriptorLayoutInfo
	{
		std::vector<VkDescriptorSetLayoutBinding> Bindings;
		size_t Hash() const;
		bool operator==(const DescriptorLayoutInfo& other) const;
	};

	struct DescriptorLayoutHash
	{
		inline size_t operator()(const DescriptorLayoutInfo& info) const { return info.Hash(); }
	};

	class DescriptorLayoutCache final
	{
		friend class DescriptorCache;

	public:
		DescriptorLayoutCache(DescriptorLayoutCache&) = delete;
		~DescriptorLayoutCache();

		VkDescriptorSetLayout Allocate(VkDescriptorSetLayoutCreateInfo* info);

		DescriptorLayoutCache& operator=(DescriptorLayoutCache&) = delete;

		static DescriptorLayoutCache* Create(VkDevice device);

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_ResourceMap = {};

		DescriptorLayoutCache(VkDevice device) : m_Device(device) {}
	};

	class DescriptorCache final
	{
	public:
		DescriptorCache(DescriptorCache&) = delete;
		~DescriptorCache();

		DescriptorCache& BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorCache& BindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

		bool Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);

		DescriptorCache& operator=(DescriptorCache&) = delete;

		static DescriptorCache* Create(DescriptorAllocator* allocator, DescriptorLayoutCache* layoutCache);

	private:
		DescriptorAllocator* m_Allocator = nullptr;
		DescriptorLayoutCache* m_LayoutCache = nullptr;

		std::vector<VkWriteDescriptorSet> m_WriteDescriptors = {};
		std::vector<VkDescriptorSetLayoutBinding> m_Bindings = {};

		DescriptorCache(DescriptorAllocator* allocator, DescriptorLayoutCache* layoutCache) : m_Allocator(allocator), m_LayoutCache(layoutCache) {}
	};

}