#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Buffer.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/VertexData.hpp"
#include "Rendering/State.hpp"

#include <AssetLibrary.hpp>

namespace VKP
{
	
	std::unordered_map<std::string, Mesh*> Mesh::s_ResourceMap = {};

	Mesh::~Mesh()
	{
		Impl::DestroyBuffer(Impl::State::Data, &VBO);
		Impl::DestroyBuffer(Impl::State::Data, &IBO);

		s_ResourceMap.erase(Path);
	}

	Mesh* Mesh::Create(const std::string& name)
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
			delete mesh;
			return nullptr;
		}

		s_ResourceMap[name] = mesh;

		return mesh;
	}

}