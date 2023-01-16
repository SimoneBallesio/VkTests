#pragma once

#include "Compression.hpp"

#include <unordered_map>

namespace Assets
{

	struct MeshNode
	{
		std::string MaterialPath;
		std::string MeshPath;
	};

	struct PrefabAssetInfo
	{
		std::unordered_map<uint64_t, std::string> NodeNames;
		std::unordered_map<uint64_t, uint64_t> NodeParents;
		std::unordered_map<uint64_t, MeshNode> NodeMeshes;
		std::unordered_map<uint64_t, uint64_t> NodeMatrices;
		std::vector<std::array<float, 16>> Matrices;

		CompressionMode Compression = CompressionMode::LZ4;
	};

	struct Asset;

	PrefabAssetInfo ParsePrefabAssetInfo(Asset* file);

	Asset PackPrefabAsset(PrefabAssetInfo* info);

}