#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Buffer.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/VertexData.hpp"
#include "Rendering/State.hpp"

#include <AssetLibrary.hpp>

namespace VKP
{

	MeshCache* MeshCache::s_Instance = nullptr;
	std::unordered_map<std::string, Mesh*> MeshCache::s_ResourceMap = {};

	MeshCache::~MeshCache()
	{
		for (auto& p : s_ResourceMap)
		{
			Impl::DestroyBuffer(Impl::State::Data, &p.second->VBO);
			Impl::DestroyBuffer(Impl::State::Data, &p.second->IBO);

			delete p.second;
		}

		s_ResourceMap.clear();
	}

	Mesh* MeshCache::Create(const std::string& name)
	{
		auto it = s_ResourceMap.find(name);

		if (it != s_ResourceMap.end())
			return it->second;

		Assets::Asset file;

		if (!Assets::LoadBinary(name.c_str(), file))
		{
			VKP_ERROR("Unable to load model file {}", name);
			return nullptr;
		}

		auto mesh = new Mesh();
		mesh->Path = name;

		auto info = Assets::ParseMeshAssetInfo(&file);

		std::vector<Vertex> vertices(info.VertexBufferSize / sizeof(Vertex));
		std::vector<uint32_t> indices(info.IndexBufferSize / sizeof(uint32_t));

		Assets::UnpackMesh(&info, file.Binary.data(), file.Binary.size(), (uint8_t*)vertices.data(), (uint8_t*)indices.data());

		bool success = Impl::CreateVertexBuffer(Impl::State::Data, &mesh->VBO, vertices);
		if (success) success = Impl::CreateIndexBuffer(Impl::State::Data, &mesh->IBO, indices);
		if (success) mesh->NumIndices = indices.size();

		else
		{
			Impl::DestroyBuffer(Impl::State::Data, &mesh->VBO);
			Impl::DestroyBuffer(Impl::State::Data, &mesh->IBO);

			delete mesh;
			return nullptr;
		}

		s_ResourceMap[name] = mesh;

		return mesh;
	}

	MeshCache* MeshCache::Create()
	{
		if (s_Instance == nullptr)
			s_Instance = new MeshCache();

		return s_Instance;
	}

	MeshCache& MeshCache::Get()
	{
		return *s_Instance;
	}

}