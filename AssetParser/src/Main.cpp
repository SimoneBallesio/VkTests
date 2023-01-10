#include <AssetLibrary.hpp>

#include <stb_image.h>

#include <tiny_obj_loader.h>

#include <filesystem>
#include <iostream>
#include <unordered_map>

bool ParseTexture(const std::filesystem::path& in, const std::filesystem::path& out)
{
	int width, height, nrChannels;
	void* data = stbi_load(in.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);

	if (data == nullptr) return false;

	Assets::TextureAssetInfo info = {};
	info.Name = in.string();
	info.FileSize = width * height * STBI_rgb_alpha;
	info.Format = Assets::TextureFormat::RGBA8;
	info.Compression = Assets::CompressionMode::LZ4;
	info.PixelSize[0] = width;
	info.PixelSize[1] = height;
	info.PixelSize[2] = 1;

	Assets::Asset file = Assets::PackTexture(&info, data);
	Assets::SaveBinary(out.c_str(), file);

	stbi_image_free(data);

	return true;
}

bool ParseMesh(const std::filesystem::path& in, const std::filesystem::path& out)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warnings, errors;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warnings, &errors, in.c_str()))
		return false;

	std::vector<Assets::VertexPosColNorUV> vertices;
	std::vector<uint32_t> indices;
	std::unordered_map<Assets::VertexPosColNorUV, uint32_t> uniqueVertices;

	for (const auto& s : shapes)
	{
		for (const auto& i : s.mesh.indices)
		{
			Assets::VertexPosColNorUV v{};

			v.Position[0] = attrib.vertices[3 * i.vertex_index + 0];
			v.Position[1] = attrib.vertices[3 * i.vertex_index + 1];
			v.Position[2] = attrib.vertices[3 * i.vertex_index + 2];

			v.UV[0] = attrib.texcoords[2 * i.texcoord_index + 0];
			v.UV[1] = 1.0f - attrib.texcoords[2 * i.texcoord_index + 1];

			v.Normal[0] = attrib.normals[3 * i.normal_index + 0];
			v.Normal[1] = attrib.normals[3 * i.normal_index + 1];
			v.Normal[2] = attrib.normals[3 * i.normal_index + 2];

			v.Color[0] = 1.0f; v.Color[1] = 1.0f; v.Color[2] = 1.0f;

			if (uniqueVertices.count(v) == 0)
			{
				uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(v);
			}

			indices.push_back(uniqueVertices[v]);
		}
	}

	Assets::MeshAssetInfo info = {};

	info.Name = in.string();
	info.Format = Assets::VertexFormat::PosColNorUV;
	info.Compression = Assets::CompressionMode::LZ4;
	info.VertexBufferSize = vertices.size() * sizeof(Assets::VertexPosColNorUV);
	info.IndexBufferSize = indices.size() * sizeof(uint32_t);

	Assets::Asset file = Assets::PackMesh(&info, vertices.data(), indices.data());
	Assets::SaveBinary(out.c_str(), file);

	return true;
}

int main(int argc, char** argv)
{
	std::filesystem::path p(argv[1]);

	for (auto& it : std::filesystem::directory_iterator(p))
	{
		if (it.path().extension() == ".png")
		{
			auto out = it.path();
			out.replace_extension(".texi");

			std::cout << "-- Parsing texture " << it.path().c_str() << " >> " << out.c_str() << '\n';

			if (!ParseTexture(it.path(), out))
			{
				std::cout << "-- Unable to parse texture file --\n";
				return 1;
			}

			continue;
		}

		if (it.path().extension() == ".obj")
		{
			auto out = it.path();
			out.replace_extension(".mesh");

			std::cout << "-- Parsing mesh " << it.path().c_str() << " >> " << out.c_str() << '\n';

			if (!ParseMesh(it.path(), out))
			{
				std::cout << "-- Unable to parse mesh file --\n";
				return 1;
			}

			continue;
		}
	}

	return 0;
}