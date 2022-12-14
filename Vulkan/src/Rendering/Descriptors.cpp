#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Descriptors.hpp"

namespace VKP
{

	static VkDescriptorPool CreatePool(VkDevice device, const DescriptorPoolSizes& sizes, uint32_t descriptorCount, VkDescriptorPoolCreateFlags flags)
	{
		std::array<VkDescriptorPoolSize, 11> descriptorSizes;

		for (size_t i = 0; i < descriptorSizes.size(); i++)
		{
			descriptorSizes[i].type = sizes.Sizes[i].first;
			descriptorSizes[i].descriptorCount = uint32_t(descriptorCount * sizes.Sizes[i].second);
		}

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pPoolSizes = descriptorSizes.data();
		poolInfo.poolSizeCount = descriptorSizes.size();
		poolInfo.maxSets = descriptorCount;
		poolInfo.flags = flags;

		VkDescriptorPool pool = VK_NULL_HANDLE;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor pool");
			return VK_NULL_HANDLE;
		}

		return pool;
	}

	DescriptorAllocator::~DescriptorAllocator()
	{
		for (auto p : m_UsedPools)
			vkDestroyDescriptorPool(m_Device, p, nullptr);

		for (auto p : m_FreePools)
			vkDestroyDescriptorPool(m_Device, p, nullptr);
	}

	void DescriptorAllocator::ResetPools()
	{
		for (auto p : m_UsedPools)
			vkResetDescriptorPool(m_Device, p, 0);

		m_FreePools.reserve(m_FreePools.size() + m_UsedPools.size());
		m_FreePools.insert(m_FreePools.end(), m_UsedPools.begin(), m_UsedPools.end());

		m_UsedPools.clear();
		m_CurrentPool = VK_NULL_HANDLE;
	}

	bool DescriptorAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		if (m_CurrentPool == VK_NULL_HANDLE)
		{
			m_CurrentPool = GrabFreePool();
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

		m_CurrentPool = GrabFreePool();
		m_UsedPools.push_back(m_CurrentPool);

		result = vkAllocateDescriptorSets(m_Device, &allocInfo, set);

		return result == VK_SUCCESS;
	}

	DescriptorAllocator* DescriptorAllocator::Create(VkDevice device)
	{
		return new DescriptorAllocator(device);
	}

	VkDescriptorPool DescriptorAllocator::GrabFreePool()
	{
		if (m_FreePools.size() > 0)
		{
			VkDescriptorPool pool = m_FreePools.back();
			m_FreePools.pop_back();

			return pool;
		}

		return CreatePool(m_Device, m_Sizes, 1000, 0);
	}

	size_t DescriptorLayoutInfo::Hash() const
	{
		size_t result = std::hash<size_t>()(Bindings.size());

		for (const VkDescriptorSetLayoutBinding& b : Bindings)
		{
			size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
			result ^= std::hash<size_t>()(binding_hash);
		}

		return result;
	}

	bool DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
	{
		if (Bindings.size() != other.Bindings.size())
			return false;

		for (size_t i = 0; i < Bindings.size(); i++)
		{
			if (other.Bindings[i].binding != Bindings[i].binding)
				return false;

			else if (other.Bindings[i].descriptorType != Bindings[i].descriptorType)
				return false;

			else if (other.Bindings[i].descriptorCount != Bindings[i].descriptorCount)
				return false;

			else if (other.Bindings[i].stageFlags != Bindings[i].stageFlags)
				return false;
		}

		return true;
	}

	DescriptorLayoutCache::~DescriptorLayoutCache()
	{
		for (auto p : m_ResourceMap)
			vkDestroyDescriptorSetLayout(m_Device, p.second, nullptr);
	}

	VkDescriptorSetLayout DescriptorLayoutCache::Allocate(VkDescriptorSetLayoutCreateInfo* info)
	{
		DescriptorLayoutInfo layoutInfo;
		bool sorted = true;
		int32_t lastBinding = -1;

		for (size_t i = 0; i < info->bindingCount; i++)
		{
			layoutInfo.Bindings.push_back(info->pBindings[i]);

			if (info->pBindings[i].binding > lastBinding)
			{
				lastBinding = info->pBindings[i].binding;
				continue;
			}

			sorted = false;
		}

		if (!sorted) std::sort(layoutInfo.Bindings.begin(), layoutInfo.Bindings.end(), [](auto& a, auto& b) { return a.binding < b.binding; });

		auto it = m_ResourceMap.find(layoutInfo);

		if (it != m_ResourceMap.end())
			return (*it).second;

		VkDescriptorSetLayout layout;

		if (vkCreateDescriptorSetLayout(m_Device, info, nullptr, &layout) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create descriptor set layout");
			return VK_NULL_HANDLE;
		}

		m_ResourceMap[layoutInfo] = layout;

		return layout;
	}

	DescriptorLayoutCache* DescriptorLayoutCache::Create(VkDevice device)
	{
		return new DescriptorLayoutCache(device);
	}

	DescriptorCache::~DescriptorCache()
	{
		
	}

	DescriptorCache& DescriptorCache::BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayoutBinding bind = {};

		bind.descriptorCount = 1;
		bind.descriptorType = type;
		bind.stageFlags = stageFlags;
		bind.binding = binding;

		m_Bindings.push_back(bind);

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.dstBinding = binding;

		m_WriteDescriptors.push_back(write);

		return *this;
	}

	DescriptorCache& DescriptorCache::BindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		return *this;
	}

	bool DescriptorCache::Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
	{
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pBindings = m_Bindings.data();
		layoutInfo.bindingCount = m_Bindings.size();

		layout = m_LayoutCache->Allocate(&layoutInfo);

		if (layout == VK_NULL_HANDLE)
			return false;

		if (!m_Allocator->Allocate(&set, layout))
			return false;

		for (auto& w : m_WriteDescriptors)
			w.dstSet = set;

		vkUpdateDescriptorSets(m_Allocator->m_Device, m_WriteDescriptors.size(), m_WriteDescriptors.data(), 0, nullptr);

		return true;
	}

	DescriptorCache* DescriptorCache::Create(DescriptorAllocator* allocator, DescriptorLayoutCache* layoutCache)
	{
		return new DescriptorCache(allocator, layoutCache);
	}

}