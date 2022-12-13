#pragma once

#include "Rendering/Buffer.hpp"

namespace VKP
{

	class Mesh
	{
	public:
		std::string Path = "";
		Buffer VBO = {};
		Buffer IBO = {};
		uint32_t NumIndices = 0;

		Mesh() = default;
		~Mesh();

		static Mesh* Create(const std::string& path);

	private:
		static std::unordered_map<std::string, Mesh*> s_ResourceMap;
	};

}