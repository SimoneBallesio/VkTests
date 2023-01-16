#include <AssetLibrary.hpp>

#include <stb_image.h>

#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef TINYGLTF_IMPLEMENTATION

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

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

bool ParseObj(const std::filesystem::path& in, const std::filesystem::path& out)
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

void UnpackGltfBuffer(tinygltf::Model& model, tinygltf::Accessor& accessor, std::vector<uint8_t>& out)
{
	tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
	tinygltf::Buffer& data = model.buffers[view.buffer];

	const uint32_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);
	const uint32_t elementSize = tinygltf::GetComponentSizeInBytes(accessor.componentType) * numComponents;

	size_t stride = view.byteStride;
	if (stride == 0) stride = elementSize;

	out.resize(accessor.count * elementSize);

	uint8_t* src = data.data.data() + view.byteOffset + accessor.byteOffset;
	uint8_t* dst = out.data();

	for (size_t i = 0; i < accessor.count; i++)
	{
		memcpy(dst, src, elementSize);

		src += stride;
		dst += elementSize;
	}
}

std::string ParseGltfMeshName(const tinygltf::Model& model, int meshIndex, int primitiveIndex)
{
	char meshIdBuf[50];
	snprintf(meshIdBuf, 10, "%d", meshIndex);

	std::string name = "Mesh_" + std::string{ meshIdBuf } + "_" + model.meshes[meshIndex].name;

	if (model.meshes[meshIndex].primitives.size() > 1)
	{
		char primIdBuf[50];
		snprintf(primIdBuf, 10, "%d", primitiveIndex);

		name += "_Primitive_" + std::string{ primIdBuf };
	}

	return name;
}

std::string ParseGltfMaterialName(const tinygltf::Model& model, int materialIndex)
{
	char matIdBuf[50];
	snprintf(matIdBuf, 10, "%d", materialIndex);

	std::string name = "Mat_" + std::string{ matIdBuf } + '_' + model.materials[materialIndex].name;

	return name;
}

bool ParseGltfVertices(tinygltf::Model& model, tinygltf::Primitive& primitive, std::vector<Assets::VertexPosColNorUV>& verts)
{
	tinygltf::Accessor& posAccess = model.accessors[primitive.attributes["POSITION"]];

	if (posAccess.type != TINYGLTF_TYPE_VEC3)
	{
		std::cout << "GLTF Error: Unsupported vector format for Position data\n";
		return false;
	}

	if (posAccess.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		std::cout << "GLTF Error: Unsupported data type for Position data\n";
		return false;
	}

	verts.resize(posAccess.count);

	std::vector<uint8_t> posData = {};
	UnpackGltfBuffer(model, posAccess, posData);

	for (size_t i = 0; i < verts.size(); i++)
	{
		float* floatPtr = (float*)posData.data();

		verts[i].Position[0] = *(floatPtr + (i * 3) + 0);
		verts[i].Position[1] = *(floatPtr + (i * 3) + 1);
		verts[i].Position[2] = *(floatPtr + (i * 3) + 2);
	}

	tinygltf::Accessor& normAccess = model.accessors[primitive.attributes["NORMAL"]];

	if (normAccess.type != TINYGLTF_TYPE_VEC3)
	{
		std::cout << "GLTF Error: Unsupported vector format for Normal data\n";
		return false;
	}

	if (normAccess.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		std::cout << "GLTF Error: Unsupported data type for Normal data\n";
		return false;
	}

	std::vector<uint8_t> normData = {};
	UnpackGltfBuffer(model, normAccess, normData);

	for (size_t i = 0; i < verts.size(); i++)
	{
		float* floatPtr = (float*)normData.data();

		verts[i].Normal[0] = *(floatPtr + (i * 3) + 0);
		verts[i].Normal[1] = *(floatPtr + (i * 3) + 1);
		verts[i].Normal[2] = *(floatPtr + (i * 3) + 2);

		verts[i].Color[0] = verts[i].Normal[0];
		verts[i].Color[1] = verts[i].Normal[1];
		verts[i].Color[2] = verts[i].Normal[2];
	}

	tinygltf::Accessor& uvAccess = model.accessors[primitive.attributes["TEXCOORD_0"]];

	if (uvAccess.type != TINYGLTF_TYPE_VEC2)
	{
		std::cout << "GLTF Error: Unsupported vector format for UV data\n";
		return false;
	}

	if (uvAccess.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		std::cout << "GLTF Error: Unsupported data type for UV data\n";
		return false;
	}

	std::vector<uint8_t> uvData = {};
	UnpackGltfBuffer(model, uvAccess, uvData);

	for (size_t i = 0; i < verts.size(); i++)
	{
		float* floatPtr = (float*)uvData.data();

		verts[i].UV[0] = *(floatPtr + (i * 2) + 0);
		verts[i].UV[1] = *(floatPtr + (i * 2) + 1);
	}

	return true;
}

bool ParseGltfIndices(tinygltf::Model& model, tinygltf::Primitive& primitive, std::vector<uint32_t>& indices)
{
	tinygltf::Accessor& idxAccess = model.accessors[primitive.indices];

	std::vector<uint8_t> idxData = {};
	UnpackGltfBuffer(model, idxAccess, idxData);

	for (size_t i = 0; i < idxAccess.count; i++)
	{
		uint32_t index;

		switch (idxAccess.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				uint16_t* ushortPtr = (uint16_t*)idxData.data();
				index = *(ushortPtr + i);
				break;
			}

			case TINYGLTF_COMPONENT_TYPE_SHORT:
			{
				int16_t* shortPtr = (int16_t*)idxData.data();
				index = *(shortPtr + i);
				break;
			}

			default:
			{
				std::cout << "GLTF Error: Unsupported data type for indices data\n";
				return false;
			}
		}

		indices.push_back(index);
	}

	return true;
}

bool ParseGltfMeshes(tinygltf::Model& model, const std::filesystem::path& out)
{
	for (size_t i = 0; i < model.meshes.size(); i++)
	{
		auto& m = model.meshes[i];

		std::vector<Assets::VertexPosColNorUV> vertices = {};
		std::vector<uint32_t> indices = {};

		for (size_t j = 0; j < m.primitives.size(); j++)
		{
			vertices.clear();
			indices.clear();

			bool success = ParseGltfVertices(model, m.primitives[j], vertices);
			if (success) success = ParseGltfIndices(model, m.primitives[j], indices);

			if (!success)
			{
				std::cout << "GLTF Error: Unable to parse mesh vertices or indices\n";
				return false;
			}

			Assets::MeshAssetInfo info = {};
			info.Name = ParseGltfMeshName(model, i, j);
			info.Format = Assets::VertexFormat::PosColNorUV;
			info.Compression = Assets::CompressionMode::LZ4;
			info.VertexBufferSize = vertices.size() * sizeof(Assets::VertexPosColNorUV);
			info.IndexBufferSize = indices.size() * sizeof(uint32_t);

			Assets::Asset file = Assets::PackMesh(&info, vertices.data(), indices.data());
			const std::filesystem::path outPath = out / (info.Name + ".mesh");

			Assets::SaveBinary(outPath.c_str(), file);
		}
	}

	return true;
}

bool ParseGltfMaterials(const tinygltf::Model& model, const std::filesystem::path& out)
{
	for (size_t i = 0; i < model.materials.size(); i++)
	{
		const auto& material = model.materials[i];
		const auto& pbr = material.pbrMetallicRoughness;

		Assets::MaterialAssetInfo info = {};

		if (strcmp("BLEND", material.alphaMode.c_str()) == 0)
			info.Transparency = Assets::TransparencyMode::Transparent;

		if (pbr.baseColorTexture.index >= 0)
		{
			const tinygltf::Texture& texture = model.textures[pbr.baseColorTexture.index];
			const tinygltf::Image& image = model.images[texture.source];

			std::filesystem::path texPath = out.parent_path() / image.uri;
			texPath.replace_extension(".texi");

			info.Textures["diffuse"] = texPath.string();
		}

		const std::string matName = ParseGltfMaterialName(model, i);
		std::filesystem::path outPath = out / (matName + ".matx");

		Assets::Asset file = Assets::PackMaterial(&info);
		Assets::SaveBinary(outPath.c_str(), file);
	}

	return true;
}

bool ParseGltfNodes(const tinygltf::Model& model, const std::filesystem::path& in, const std::filesystem::path& out)
{
	Assets::PrefabAssetInfo info = {};
	std::vector<uint64_t> meshNodes;

	for (size_t i = 0; i < model.nodes.size(); i++)
	{
		const auto& n = model.nodes[i];
		info.NodeNames[i] = std::move(n.name);

		std::cout << "   -- Node name: \"" << n.name << "\"\n";

		std::array<float, 16> matrix;

		if (n.matrix.size() > 0)
		{
			for (size_t j = 0; j < n.matrix.size(); j++)
				matrix[j] = (float)matrix[j];
		}

		else
		{
			glm::mat4 translation(1.0f);
			glm::mat4 rotation(1.0f);
			glm::mat4 scale(1.0f);

			if (n.translation.size() > 0)
				translation = glm::translate(translation, { (float)n.translation[0], (float)n.translation[1], (float)n.translation[2] });

			if (n.rotation.size() > 0)
			{
				glm::quat orientation = { (float)n.rotation[3], (float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2] };
				rotation = glm::mat4_cast(orientation);
			}

			if (n.scale.size() > 0)
				scale = glm::scale(scale, { (float)n.scale[0], (float)n.scale[1], (float)n.scale[2] });

			glm::mat4 model = scale * rotation * translation;
			memcpy(matrix.data(), &model[0][0], 16 * sizeof(float));
		}

		info.NodeMatrices[i] = info.Matrices.size();
		info.Matrices.push_back(std::move(matrix));

		if (n.mesh >= 0)
		{
			const auto& mesh = model.meshes[n.mesh];

			if (mesh.primitives.size() > 1)
				meshNodes.push_back(i);

			else
			{
				auto& primitive = mesh.primitives[0];

				std::string meshName = ParseGltfMeshName(model, n.mesh, 0);
				std::filesystem::path meshPath = out / (meshName + ".mesh");

				std::string matName = ParseGltfMaterialName(model, primitive.material);
				std::filesystem::path matPath = out / (matName + ".matx");

				Assets::MeshNode meshNode = { matPath.string(), meshPath.string() };
				info.NodeMeshes[i] = std::move(meshNode);
			}
		}
	}

	for (size_t i = 0; i < model.nodes.size(); i++)
	{
		for (auto& c : model.nodes[i].children)
			info.NodeParents[c] = i;
	}

	int nodeIdx = model.nodes.size();

	for (int i = 0; i < meshNodes.size(); i++)
	{
		auto& node = model.nodes[i];

		if (node.mesh < 0) break;

		auto& mesh = model.meshes[node.mesh];

		for (size_t j = 0; j < mesh.primitives.size(); j++)
		{
			auto& primitive = mesh.primitives[j];
			size_t nextIdx = nodeIdx++;

			char nameBuf[50];
			snprintf(nameBuf, 10, "%zu", j);

			info.NodeNames[nextIdx] = info.NodeNames[i] + "_Primitive_" + std::string{ nameBuf };

			std::string meshName = ParseGltfMeshName(model, node.mesh, j);
			std::filesystem::path meshPath = out / (meshName + ".mesh");

			std::string matName = ParseGltfMaterialName(model, primitive.material);
			std::filesystem::path matPath = out / (matName + ".matx");

			Assets::MeshNode meshNode = { matPath.string(), meshPath.string() };
			info.NodeMeshes[nextIdx] = std::move(meshNode);

			glm::mat4 identity(1.0f);
			auto& m = info.Matrices.emplace_back();
			memcpy(m.data(), &identity[0][0], 16 * sizeof(float));

			info.NodeMatrices[nextIdx] = info.Matrices.size();
		}
	}

	Assets::Asset file = Assets::PackPrefabAsset(&info);

	std::filesystem::path prefabPath = (out.parent_path()) / in.stem();
	prefabPath.replace_extension(".prfb");

	Assets::SaveBinary(prefabPath.c_str(), file);

	return true;
}

bool ParseGltf(const std::filesystem::path& in)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string warn, err;

	bool success = loader.LoadASCIIFromFile(&model, &err, &warn, in.c_str());

	if (!warn.empty())
		std::cout << "GLTF Warning: " << warn << '\n';

	if (!err.empty())
		std::cout << "GLTF Error: " << err << '\n';

	const std::filesystem::path out = in.parent_path() / (in.stem().string() + "_GLTF");
	std::filesystem::create_directory(out);

	if (success) success = ParseGltfMeshes(model, out);
	if (success) success = ParseGltfMaterials(model, out);
	if (success) success = ParseGltfNodes(model, in, out);

	return success;
}

int main(int argc, char** argv)
{
	std::filesystem::path p(argv[1]);

	for (auto& it : std::filesystem::directory_iterator(p))
	{
		if (it.path().extension() == ".png" || it.path().extension() == ".jpg")
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

			std::cout << "-- Parsing mesh (OBJ) " << it.path().c_str() << " >> " << out.c_str() << '\n';

			if (!ParseObj(it.path(), out))
			{
				std::cout << "-- Unable to parse mesh file --\n";
				return 1;
			}

			continue;
		}

		if (it.path().extension() == ".gltf")
		{
			std::cout << "-- Parsing mesh (GLTF) " << it.path().c_str() << '\n';

			if (!ParseGltf(it.path()))
			{
				std::cout << "-- Unable to parse mesh file --\n";
				return 1;
			}

			continue;
		}
	}

	return 0;
}