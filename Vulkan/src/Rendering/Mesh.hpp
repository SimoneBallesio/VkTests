#pragma once

#include "Core/UID.hpp"

#include "Rendering/Buffer.hpp"

namespace VKP
{

	struct Mesh
	{
		UID Uid;
		std::string Path = "";
		Buffer VBO = {};
		Buffer IBO = {};
		uint32_t NumIndices = 0;
		uint32_t NumVertices = 0;

		inline operator const uint64_t& () const { return (const uint64_t&)Uid; }
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