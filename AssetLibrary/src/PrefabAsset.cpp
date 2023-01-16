#include "Asset.hpp"
#include "PrefabAsset.hpp"

#include <nlohmann/json.hpp>

#include <lz4.h>

namespace Assets
{

	PrefabAssetInfo ParsePrefabAssetInfo(Asset* file)
	{
		nlohmann::json metadata = nlohmann::json::parse(file->Json);

		PrefabAssetInfo info = {};

		for (const auto& p : metadata["nodenames"].items())
		{
			auto& value = p.value();
			info.NodeNames[value[0]] = value[1];
		}

		for (const auto& p : metadata["nodeparents"].items())
		{
			auto& value = p.value();
			info.NodeParents[value[0]] = value[1];
		}

		for (const auto& p : metadata["nodematrices"].items())
		{
			auto& value = p.value();
			info.NodeMatrices[value[0]] = value[1];
		}

		const std::unordered_map<uint64_t, nlohmann::json> meshNodes = metadata["nodemeshes"];

		for (const auto& p : meshNodes)
		{
			MeshNode node;

			node.MeshPath = p.second["meshpath"];
			node.MaterialPath = p.second["matpath"];

			info.NodeMeshes[p.first] = std::move(node);
		}

		const uint32_t matricesSize = metadata["matricessize"];
		info.Matrices.resize(matricesSize / (16 * sizeof(float)));

		LZ4_decompress_safe((const char*)file->Binary.data(), (char*)info.Matrices.data(), file->Binary.size(), matricesSize);

		return info;
	}

	Asset PackPrefabAsset(PrefabAssetInfo* info)
	{
		nlohmann::json metadata;
		const uint32_t matricesSize = info->Matrices.size() * 16 * sizeof(float);

		metadata["nodenames"] = info->NodeNames;
		metadata["nodeparents"] = info->NodeParents;
		metadata["nodematrices"] = info->NodeMatrices;
		metadata["matricessize"] = matricesSize;
		metadata["compression"] = "LZ4";

		std::unordered_map<uint64_t, nlohmann::json> meshData;

		for (const auto& [id, mesh] : info->NodeMeshes)
		{
			nlohmann::json meshNode;

			meshNode["meshpath"] = mesh.MeshPath;
			meshNode["matpath"] = mesh.MaterialPath;

			meshData[id] = meshNode;
		}

		metadata["nodemeshes"] = meshData;

		const std::string jsonString = metadata.dump();

		Asset file = {};

		file.Type[0] = 'P'; file.Type[1] = 'R';
		file.Type[2] = 'F'; file.Type[3] = 'B';

		file.Json = std::move(jsonString);

		int stagingSize = LZ4_compressBound(matricesSize);
		file.Binary.resize(stagingSize);

		int compressedSize = LZ4_compress_default((const char*)info->Matrices.data(), (char*)file.Binary.data(), matricesSize, stagingSize);
		file.Binary.resize(compressedSize);

		return file;
	}
	
}