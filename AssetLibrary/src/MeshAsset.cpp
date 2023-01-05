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
			std::vector<uint8_t> data(info->VertexBufferSize + info->IndexBufferSize);

			LZ4_decompress_safe((const char*)src, (char*)data.data(), (int)srcSize, (int)data.size());

			memcpy(dstVbo, data.data(), info->VertexBufferSize);
			memcpy(dstIbo, data.data() + info->VertexBufferSize, info->IndexBufferSize);

			return;
		}

		memcpy(dstVbo, src, info->VertexBufferSize);
		memcpy(dstIbo, src + info->VertexBufferSize, info->IndexBufferSize);
	}

	Asset PackMesh(MeshAssetInfo* info, void* vbo, void* ibo)
	{
		nlohmann::json metadata;

		metadata["compression"] = "LZ4";
		metadata["format"] = "PosColNorUV";
		metadata["name"] = info->Name;
		metadata["vbosize"] = info->VertexBufferSize;
		metadata["ibosize"] = info->IndexBufferSize;

		const std::string jsonString = metadata.dump();

		Asset file = {};

		file.Type[0] = 'M'; file.Type[1] = 'E';
		file.Type[2] = 'S'; file.Type[3] = 'H';

		file.Json = std::move(jsonString);

		std::vector<uint8_t> mergedBuffer(info->VertexBufferSize + info->IndexBufferSize);

		memcpy(mergedBuffer.data(), vbo, info->VertexBufferSize);
		memcpy(mergedBuffer.data() + info->VertexBufferSize, ibo, info->IndexBufferSize);

		int stagingSize = LZ4_compressBound(mergedBuffer.size());
		file.Binary.resize(stagingSize);

		int compressedSize = LZ4_compress_default((const char*)mergedBuffer.data(), (char*)file.Binary.data(), mergedBuffer.size(), stagingSize);
		file.Binary.resize(compressedSize);

		return file;
	}

}