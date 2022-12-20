#pragma once

#include <vulkan/vulkan.h>

namespace VKP
{

	struct DescriptorList
	{
		std::vector<VkDescriptorSetLayoutBinding> Bindings = {};
		size_t Hash() const;
		bool operator==(const DescriptorList& other) const;
	};

	struct DescriptorListHash
	{
		size_t operator()(const DescriptorList& list) const;
	};

	class DescriptorSetLayoutCache final
	{
	public:
		DescriptorSetLayoutCache(DescriptorSetLayoutCache&) = delete;
		~DescriptorSetLayoutCache();

		VkDescriptorSetLayout Allocate(DescriptorList& list);

		DescriptorSetLayoutCache& operator=(DescriptorSetLayoutCache&) = delete;

		static DescriptorSetLayoutCache* Create(VkDevice device);
		static void Destroy();

		static DescriptorSetLayoutCache& Get();

	private:
		VkDevice m_Device;

		static DescriptorSetLayoutCache* s_Instance;
		static std::unordered_map<DescriptorList, VkDescriptorSetLayout, DescriptorListHash> s_ResourceMap;

		DescriptorSetLayoutCache(VkDevice device) : m_Device(device) {}
	};

	class DescriptorSetAllocator final
	{
	public:
		DescriptorSetAllocator(DescriptorSetAllocator&) = delete;
		~DescriptorSetAllocator();

		bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
		void Reset();

		DescriptorSetAllocator& operator=(DescriptorSetAllocator&) = delete;

		static DescriptorSetAllocator* Create(VkDevice device);

	private:
		VkDevice m_Device;

		std::vector<VkDescriptorPool> m_FreePools = {};
		std::vector<VkDescriptorPool> m_UsedPools = {};

		VkDescriptorPool m_CurrentPool = VK_NULL_HANDLE;

		DescriptorSetAllocator(VkDevice device) : m_Device(device) {}

		VkDescriptorPool GetOrAllocatePool();
	};

	class DescriptorSetFactory final
	{
	public:
		DescriptorSetFactory(VkDevice device, DescriptorSetLayoutCache* cache, DescriptorSetAllocator* alloc)
			: m_Device(device), m_LayoutCache(cache), m_Allocator(alloc) {}

		DescriptorSetFactory(DescriptorSetFactory&) = delete;

		~DescriptorSetFactory() = default;

		DescriptorSetFactory& BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufInfo, VkDescriptorType type, VkShaderStageFlags flags);
		DescriptorSetFactory& BindImage(uint32_t binding, VkDescriptorImageInfo* imgInfo, VkDescriptorType type, VkShaderStageFlags flags);

		bool Build(VkDescriptorSet& set);

		DescriptorSetFactory& operator=(DescriptorSetFactory&) = delete;

	private:
		VkDevice m_Device;

		DescriptorSetLayoutCache* m_LayoutCache;
		DescriptorSetAllocator* m_Allocator;

		DescriptorList m_Bindings = {};
		std::vector<VkWriteDescriptorSet> m_Writes = {};
	};

}