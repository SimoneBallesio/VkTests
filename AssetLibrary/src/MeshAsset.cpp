#include "Asset.hpp"
#include "MeshAsset.hpp"
#include "Compression.hpp"

#include <nlohmann/json.hpp>

#include <lz4.h>

namespace Assets
{

	static VertexFormat ParseVertexFormat(const char* format)
	{
		if (strcmp("PosColNorUV", format) == 0)
			return VertexFormat::PosColNorUV;

		if (strcmp("PosNorUV", format) == 0)
			return VertexFormat::PosNorUv;

		return VertexFormat::None;
	}

	MeshAssetInfo ParseMeshAssetInfo(Asset* file)
	{
		MeshAssetInfo info = {};
		nlohmann::json metadata = nlohmann::json::parse(file->Json);

		info.Name = metadata["name"];
		info.FileSize = metadata["filesize"];
		info.VertexBufferSize = metadata["vbosize"];
		info.IndexBufferSize = metadata["ibosize"];

		const std::string compression = metadata["compression"];
		info.Compression = ParseCompressionMode(compression.c_str());

		const std::string format = metadata["format"];
		info.Format = ParseVertexFormat(format.c_str());

		return info;
	}

	void UnpackMesh(MeshAssetInfo* info, const uint8_t* src, size_t srcSize, uint8_t* dstVbo, uint8_t* dstIbo)
	{
		if (info->Compression == CompressionMode::LZ4)
		{
			return;
		}

		memcpy(dstVbo, src, info->VertexBufferSize);
		memcpy(dstIbo, src + info->VertexBufferSize, info->IndexBufferSize);
	}

	Asset PackMesh(MeshAssetInfo* info, void* vbo, void* ibo)
	{
		Asset a;
		return a;
	}

}