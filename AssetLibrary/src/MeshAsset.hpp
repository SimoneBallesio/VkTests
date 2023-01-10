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

		bool operator==(const VertexPosColNorUV& v) const;
	};

	struct VertexPosNorUV
	{
		float Position[3];
		float Normal[3];
		float UV[2];

		bool operator==(const VertexPosNorUV& v) const;
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

namespace std
{

	template<>
	struct hash<Assets::VertexPosNorUV>
	{
		size_t operator()(const Assets::VertexPosNorUV& v) const
		{
			return ((hash<float>()(v.Position[0] + v.Position[1] + v.Position[2]) ^ (hash<float>()(v.Normal[0] + v.Normal[1] + v.Normal[2]) << 1)) >> 1) ^ (hash<float>()(v.UV[0] + v.UV[1]) << 1);
		}
	};

	template<>
	struct hash<Assets::VertexPosColNorUV>
	{
		size_t operator()(const Assets::VertexPosColNorUV& v) const
		{
			return ((hash<float>()(v.Position[0] + v.Position[1] + v.Position[2]) ^ (hash<float>()(v.Normal[0] + v.Normal[1] + v.Normal[2]) << 1) ^ (hash<float>()(v.Color[0] + v.Color[1] + v.Color[2]) << 2)) >> 1) ^ (hash<float>()(v.UV[0] + v.UV[1]) << 1);
		}
	};

}