#include "Pch.hpp"

#include "Rendering/VertexData.hpp"

namespace VKP
{

	void Vertex::PopulateBindingDescription(std::vector<VkVertexInputBindingDescription>& bindings, std::vector<VkVertexInputAttributeDescription>& attributes)
	{
		bindings.reserve(1);
		attributes.reserve(3);

		auto& bind = bindings.emplace_back();
		bind.binding = 0;
		bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bind.stride = sizeof(Vertex);

		auto& posAttr = attributes.emplace_back();
		posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
		posAttr.binding = 0;
		posAttr.location = 0;
		posAttr.offset = offsetof(Vertex, Position);

		auto& colAttr = attributes.emplace_back();
		colAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
		colAttr.binding = 0;
		colAttr.location = 1;
		colAttr.offset = offsetof(Vertex, Color);

		auto& norAttr = attributes.emplace_back();
		norAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
		norAttr.binding = 0;
		norAttr.location = 2;
		norAttr.offset = offsetof(Vertex, Normal);

		auto& texAttr = attributes.emplace_back();
		texAttr.format = VK_FORMAT_R32G32_SFLOAT;
		texAttr.binding = 0;
		texAttr.location = 3;
		texAttr.offset = offsetof(Vertex, TexCoord);
	}

	bool Vertex::operator==(const Vertex& v) const
	{
		return Position == v.Position && Color == v.Color && Normal == v.Normal && TexCoord == v.TexCoord;
	}

}