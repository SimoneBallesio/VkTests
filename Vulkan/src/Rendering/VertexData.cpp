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
		posAttr.binding = 0;
		posAttr.location = 1;
		posAttr.offset = offsetof(Vertex, Color);

		auto& texAttr = attributes.emplace_back();
		texAttr.format = VK_FORMAT_R32G32_SFLOAT;
		texAttr.binding = 0;
		texAttr.location = 2;
		texAttr.offset = offsetof(Vertex, TexCoord);
	}

	bool Vertex::operator==(const Vertex& v) const
	{
		return Position == v.Position && Color == v.Color && TexCoord == v.TexCoord;
	}

}