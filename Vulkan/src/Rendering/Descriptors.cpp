#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Descriptors.hpp"

namespace VKP
{

	DescriptorSetLayoutCache* DescriptorSetLayoutCache::s_Instance = nullptr;
	std::unordered_map<DescriptorList, VkDescriptorSetLayout, DescriptorListHash> DescriptorSetLayoutCache::s_ResourceMap = {};

	size_t DescriptorList::Hash() const
	{
		size_t result = std::hash<size_t>()(Bindings.size());

		for (const auto& b : Bindings)
		{
			size_t hash = b.binding | (b.descriptorType << 8) | (b.descriptorCount << 16) | (b.stageFlags << 24);
			result ^= std::hash<size_t>()(hash);
		}

		return result;
	}

	bool DescriptorList::operator==(const DescriptorList& other) const
	{
		if (other.Bindings.size() != Bindings.size())
			return false;

		for (size_t i = 0; i < other.Bindings.size(); i++)
		{
			if (other.Bindings[i].binding != Bindings[i].binding)
				return false;

			if (other.Bindings[i].descriptorType != Bindings[i].descriptorType)
				return false;

			if (other.Bindings[i].descriptorCount != Bindings[i].descriptorCount)
				return false;

			if (other.Bindings[i].stageFlags != Bindings[i].stageFlags)
				return false;
		}

		return true;
	}

	size_t DescriptorListHash::operator()(const DescriptorList& list) const
	{
		return list.Hash();
	}

	DescriptorSetLayoutCache::~DescriptorSetLayoutCache()
	{
		for (auto p : s_ResourceMap)
			vkDestroyDescriptorSetLayout(m_Device, p.second, nullptr);

		s_ResourceMap.clear();
	}

	VkDescriptorSetLayout DescriptorSetLayoutCache::Allocate(DescriptorList& list)
	{
		int32_t lastBinding = -1;
		bool sorted = true;

		for (const auto& b : list.Bindings)
		{
			if (b.binding > lastBinding)
			{
				lastBinding = b.binding;
				continue;
			}

			sorted = false;
			break;
		}

		if (!sorted) std::sort(list.Bindings.begin(), list.Bindings.end(), [](const auto& a, const auto& b) { return a.binding < b.binding; });

		auto it = s_ResourceMap.find(list);

		if (it != s_ResourceMap.end())
			return (*it).second;

		VkDescriptorSetLayoutCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pBindings = list.Bindings.data();
		createInfo.bindingCount = list.Bindings.size();

		VkDescriptorSetLayout layout;

		if (vkCreateDescriptorSetLayout(m_Device, &createInfo, nullptr, &layout) != VK_SUCCESS)
			return VK_NULL_HANDLE;

		s_ResourceMap[list] = layout;

		return layout;
	}

	DescriptorSetLayoutCache* DescriptorSetLayoutCache::Create(VkDevice device)
	{
		if (s_Instance == nullptr)
			s_Instance = new DescriptorSetLayoutCache(device);

		return s_Instance;
	}

	void DescriptorSetLayoutCache::Destroy()
	{
		if (s_Instance == nullptr) return;
		delete s_Instance;
		s_Instance = nullptr;
	}

	DescriptorSetLayoutCache& DescriptorSetLayoutCache::Get()
	{
		return *s_Instance;
	}

	DescriptorSetAllocator::~DescriptorSetAllocator()
	{
		for (auto p : m_UsedPools)
			vkDestroyDescriptorPool(m_Device, p, nullptr);

		for (auto p : m_FreePools)
			vkDestroyDescriptorPool(m_Device, p, nullptr);
	}

	bool DescriptorSetAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		if (m_CurrentPool == VK_NULL_HANDLE)
		{
			m_CurrentPool = GetOrAllocatePool();
			m_UsedPools.push_back(m_CurrentPool);
		}

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_CurrentPool;
		allocInfo.pSetLayouts = &layout;
		allocInfo.descriptorSetCount = 1;

		VkResult result = vkAllocateDescriptorSets(m_Device, &allocInfo, set);

		switch (result)
		{
			case VK_SUCCESS:
				return true;

			case VK_ERROR_FRAGMENTED_POOL:
			case VK_ERROR_OUT_OF_POOL_MEMORY:
				break;

			default:
				return false;
		}

		m_CurrentPool = GetOrAllocatePool();
		m_UsedPools.push_back(m_CurrentPool);

		allocInfo.descriptorPool = m_CurrentPool;

		result = vkAllocateDescriptorSets(m_Device, &allocInfo, set);

		return VK_SUCCESS == result;
	}

	void DescriptorSetAllocator::Reset()
	{
		for (auto p : m_UsedPools)
			vkResetDescriptorPool(m_Device, p, 0);

		m_FreePools.reserve(m_FreePools.size() + m_UsedPools.size());
		m_FreePools.insert(m_FreePools.end(), m_UsedPools.begin(), m_UsedPools.end());

		m_UsedPools.clear();
		m_CurrentPool = VK_NULL_HANDLE;
	}

	DescriptorSetAllocator* DescriptorSetAllocator::Create(VkDevice device)
	{
		return new DescriptorSetAllocator(device);
	}

	VkDescriptorPool DescriptorSetAllocator::GetOrAllocatePool()
	{
		if (m_FreePools.size() > 0)
		{
			VkDescriptorPool pool = m_FreePools.back();
			m_FreePools.pop_back();

			return pool;
		}

		static constexpr VkDescriptorPoolSize sizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 500 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 500 },
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pPoolSizes = sizes;
		poolInfo.poolSizeCount = 11;
		poolInfo.maxSets = 1000;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		VkDescriptorPool pool;

		if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor pool");
			return VK_NULL_HANDLE;
		}

		return pool;
	}

	DescriptorSetFactory& DescriptorSetFactory::BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufInfo, VkDescriptorType type, VkShaderStageFlags flags)
	{
		auto& bind = m_Bindings.Bindings.emplace_back();
		bind.binding = binding;
		bind.descriptorType = type;
		bind.descriptorCount = 1;
		bind.stageFlags = flags;

		auto& write = m_Writes.emplace_back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pBufferInfo = bufInfo;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.dstBinding = binding;

		return *this;
	}

	DescriptorSetFactory& DescriptorSetFactory::BindImage(uint32_t binding, VkDescriptorImageInfo* imgInfo, VkDescriptorType type, VkShaderStageFlags flags)
	{
		auto& bind = m_Bindings.Bindings.emplace_back();
		bind.binding = binding;
		bind.descriptorType = type;
		bind.descriptorCount = 1;
		bind.stageFlags = flags;

		auto& write = m_Writes.emplace_back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pImageInfo = imgInfo;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.dstBinding = binding;

		return *this;
	}

	bool DescriptorSetFactory::Build(VkDescriptorSet& set)
	{
		VkDescriptorSetLayout layout = m_LayoutCache->Allocate(m_Bindings);

		if (layout == VK_NULL_HANDLE) return false;
		if (!m_Allocator->Allocate(&set, layout)) return false;

		for (auto& w : m_Writes)
			w.dstSet = set;

		vkUpdateDescriptorSets(m_Device, m_Writes.size(), m_Writes.data(), 0, nullptr);

		m_Bindings.Bindings.clear();
		m_Writes.clear();

		return true;
	}

}