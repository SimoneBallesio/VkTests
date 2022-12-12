#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/VertexData.hpp"

#include <tiny_obj_loader.h>

namespace VKP
{
	
	std::unordered_map<std::string, Mesh*> Mesh::s_ResourceMap = {};

	Mesh* Mesh::Create(const std::string& name)
	{
		auto it = s_ResourceMap.find(name);

		if (it != s_ResourceMap.end())
			return it->second;

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warnings, errors;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warnings, &errors, name.c_str()))
		{
			VKP_ERROR("Unable to load OBJ model file {}", name);
			return nullptr;
		}

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::unordered_map<Vertex, uint32_t> uniqueVertices;

		for (const auto& s : shapes)
		{
			for (const auto& i : s.mesh.indices)
			{
				Vertex v{};

				v.Position = {
					attrib.vertices[3 * i.vertex_index + 0],
					attrib.vertices[3 * i.vertex_index + 1],
					attrib.vertices[3 * i.vertex_index + 2]
				};

				v.TexCoord = {
					attrib.texcoords[2 * i.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * i.texcoord_index + 1]
				};

				v.Color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(v) == 0) {
					uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(v);
				}

				indices.push_back(uniqueVertices[v]);
			}
		}

		auto mesh = new Mesh();

		bool success = Context::Get().CreateVertexBuffer(mesh->VBO, vertices);
		if (success) success = Context::Get().CreateIndexBuffer(mesh->IBO, indices);
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