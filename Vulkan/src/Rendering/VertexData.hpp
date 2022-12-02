#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>

namespace VKP
{

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Color;
		glm::vec2 TexCoord;

		Vertex() = default;
		~Vertex() = default;

		bool operator==(const Vertex& v) const;

		static void PopulateBindingDescription(std::vector<VkVertexInputBindingDescription>& bindings, std::vector<VkVertexInputAttributeDescription>& attributes);
	};

}

namespace std
{

	template<>
	struct hash<VKP::Vertex>
	{
		size_t operator()(VKP::Vertex const& v) const
		{
			return ((hash<glm::vec3>()(v.Position) ^ (hash<glm::vec3>()(v.Color) << 1)) >> 1) ^ (hash<glm::vec2>()(v.TexCoord) << 1);
		}
	};

}