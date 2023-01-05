#pragma once

#include <string>

namespace Assets
{

	enum class VertexFormat
	{
		None = 0,
		PosColNorUV,
		PosNorUv,
	};

	struct VertexPosColNorUV
	{
		float Position[3];
		float Color[3];
		float Normal[3];
		float UV[2];
	};

	struct VertexPosNorUV
	{
		float Position[3];
		float Normal[3];
		float UV[2];
	};

	enum class CompressionMode;

	struct MeshAssetInfo
	{
		std::string Name = "";
		CompressionMode Compression;
		VertexFormat Format;
		uint32_t VertexBufferSize = 0;
		uint32_t IndexBufferSize = 0;
	};

	struct Asset;

	MeshAssetInfo ParseMeshAssetInfo(Asset* file);

	void UnpackMesh(MeshAssetInfo* info, const uint8_t* src, size_t srcSize, uint8_t* dstVbo, uint8_t* dstIbo);
	Asset PackMesh(MeshAssetInfo* info, void* vbo, void* ibo);

}