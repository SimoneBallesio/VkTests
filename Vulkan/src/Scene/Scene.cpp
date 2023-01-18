#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Material.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/Texture.hpp"

#include "Scene/Scene.hpp"

#include <AssetLibrary.hpp>

namespace VKP
{

	bool Scene::LoadFromPrefab(const char* path)
	{
		Assets::Asset file = {};
		bool success = Assets::LoadBinary(path, file);

		if (!success)
		{
			VKP_ERROR("Unable to load prefab asset ({})", path);
			return false;
		}

		const auto info = Assets::ParsePrefabAssetInfo(&file);
		m_Renderables.reserve(info.NodeMeshes.size());

		std::unordered_map<uint64_t, glm::mat4> worldMatrices;
		std::vector<std::pair<uint64_t, glm::mat4>> pendingNodes;

		for (const auto& [k, v] : info.NodeMatrices)
		{
			glm::mat4 mat;
			memcpy(&mat[0][0], info.Matrices[v].data(), sizeof(glm::mat4));

			if (info.NodeParents.find(k) == info.NodeParents.end())
			{
				worldMatrices[k] = mat;
				continue;
			}

			pendingNodes.push_back({ k, mat });
		}

		while (pendingNodes.size() > 0)
		{
			for (size_t i = 0; i < pendingNodes.size(); i++)
			{
				uint64_t node = pendingNodes[i].first;
				uint64_t parent = info.NodeParents.at(node);
				auto it = worldMatrices.find(parent);

				if (it != worldMatrices.end())
				{
					glm::mat4 mat = it->second * pendingNodes[i].second;
					worldMatrices[node] = mat;

					pendingNodes[i] = pendingNodes.back();
					pendingNodes.pop_back();
					i--;
				}
			}
		}

		for (const auto& [k, m] : info.NodeMeshes)
		{
			auto& r = m_Renderables.emplace_back();
			r.Model = MeshCache::Get().Create(m.MeshPath);

			Assets::Asset matFile = {};

			if (!Assets::LoadBinary(m.MaterialPath.c_str(), matFile))
			{
				VKP_ERROR("Unable to locate material file ({})", m.MaterialPath);
				return false;
			}

			const auto matInfo = Assets::ParseMaterialAssetInfo(&matFile);
			std::vector<Texture*> textures = { TextureCache::Get().Create(matInfo.Textures.at("diffuse")) };

			r.Mat = MaterialCache::Get().Create(m.MaterialPath, std::move(textures));

			if (worldMatrices.find(k) != worldMatrices.end())
				r.Matrix = worldMatrices.at(k);
		}

		return true;
	}

	std::vector<Renderable>::iterator Scene::Begin()
	{
		return m_Renderables.begin();
	}

	std::vector<Renderable>::iterator Scene::End()
	{
		return m_Renderables.end();
	}

	std::vector<Renderable>::const_iterator Scene::Begin() const
	{
		return m_Renderables.begin();
	}

	std::vector<Renderable>::const_iterator Scene::End() const
	{
		return m_Renderables.end();
	}

	Scene* Scene::Create()
	{
		return new Scene();
	}

}