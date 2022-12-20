#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Shader.hpp"

namespace VKP
{

	MaterialCache* MaterialCache::s_Instance = nullptr;
	std::unordered_map<std::string, Material*> MaterialCache::s_ResourceMap = {};

	MaterialCache::MaterialCache(VkDevice device) : m_Device(device)
	{
		auto& shaderCache = ShaderModuleCache::Get();
		auto& descSetLayoutCache = DescriptorSetLayoutCache::Get();
		auto& pipeLayoutCache = PipelineLayoutCache::Get();

		auto baseVert = shaderCache.Create("assets/shaders/base.vert.spv");
		auto baseFrag = shaderCache.Create("assets/shaders/base.frag.spv");

		ShaderEffect effect = {};
		effect.AddStage(baseVert, VK_SHADER_STAGE_VERTEX_BIT);
		effect.AddStage(baseFrag, VK_SHADER_STAGE_FRAGMENT_BIT);
		effect.Reflect(&descSetLayoutCache, &pipeLayoutCache);

		GraphicsPipelineFactory f = {};
		m_DefaultTemplate.Pipe = f.Build(Context::Get().GetDefaultRenderPass(), &effect);
		m_DefaultTemplate.PipeLayout = effect.m_PipeLayout;

		VKP_ASSERT(m_DefaultTemplate.Pipe != VK_NULL_HANDLE, "Fatal: Unable to create default pipeline");
	}

	MaterialCache::~MaterialCache()
	{
		for (auto& p : s_ResourceMap)
			delete p.second;

		vkDestroyPipeline(m_Device, m_DefaultTemplate.Pipe, nullptr);

		s_ResourceMap.clear();
	}

	Material* MaterialCache::Create(const std::string& name, const std::vector<Texture*> textures)
	{
		auto it = s_ResourceMap.find(name);

		if (it != s_ResourceMap.end())
			return (*it).second;

		Material* mat = new Material();
		mat->Path = std::move(name);
		mat->Template = &m_DefaultTemplate;

		DescriptorSetFactory builder = DescriptorSetFactory(m_Device, &DescriptorSetLayoutCache::Get(), Context::Get().GetDescriptorSetAllocator());

		for (size_t i = 0; i < textures.size(); i++)
		{
			VkDescriptorImageInfo info = {};
			info.imageView = textures[i]->ViewHandle;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.sampler = textures[i]->SamplerHandle;

			builder.BindImage(i, &info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		}

		builder.Build(mat->TextureSet);

		if (mat->TextureSet == VK_NULL_HANDLE)
		{
			VKP_ERROR("Unable to create descriptor set for specified textures");
			delete mat;
			return nullptr;
		}

		s_ResourceMap[mat->Path] = mat;
		return mat;
	}

	MaterialCache* MaterialCache::Create(VkDevice device)
	{
		if (s_Instance == nullptr)
			s_Instance = new MaterialCache(device);

		return s_Instance;
	}

	void MaterialCache::Destroy()
	{
		delete s_Instance;
		s_Instance = nullptr;
	}

	MaterialCache& MaterialCache::Get()
	{
		return *s_Instance;
	}

}