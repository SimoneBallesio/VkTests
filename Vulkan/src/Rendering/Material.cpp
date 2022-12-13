#include "Pch.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Material.hpp"

namespace VKP
{

	std::unordered_map<std::string, Material*> Material::s_ResourceMap = {};

	Material::~Material()
	{
		Context::Get().DestroyMaterial(*this);
		s_ResourceMap.erase(this->Path);
	}

	Material* Material::Create(const std::string& name)
	{
		auto it = s_ResourceMap.find(name);

		if (it != s_ResourceMap.end())
			return it->second;

		auto mat = new Material();
		mat->Path = name;

		if (!Context::Get().CreatePipelineLayout(&mat->PipeLayout))
		{
			delete mat;
			return nullptr;
		}

		if (!Context::Get().CreatePipeline(*mat))
		{
			delete mat;
			return nullptr;
		}

		s_ResourceMap[name] = mat;

		return mat;
	}

}