#pragma once

#include "Rendering/Buffer.hpp"

namespace VKP
{

	struct Mesh
	{
		std::string Path = "";
		Buffer VBO = {};
		Buffer IBO = {};
		uint32_t NumIndices = 0;
	};

	class MeshCache final
	{
	public:
		MeshCache(MeshCache&) = delete;
		~MeshCache();

		Mesh* Create(const std::string& name);

		static MeshCache* Create();
		static MeshCache& Get();

	private:
		static MeshCache* s_Instance;
		static std::unordered_map<std::string, Mesh*> s_ResourceMap;

		MeshCache() = default;
	};

}