#pragma once

#include "Rendering/Renderable.hpp"

namespace VKP
{

	class Scene final
	{
	public:
		Scene(Scene&) = delete;
		~Scene() = default;

		bool LoadFromPrefab(const char* path);

		inline std::vector<Renderable>::iterator Begin() { return m_Renderables.begin(); }
		inline std::vector<Renderable>::iterator End() { return m_Renderables.end(); }

		inline std::vector<Renderable>::const_iterator Begin() const { return m_Renderables.begin(); }
		inline std::vector<Renderable>::const_iterator End() const { return m_Renderables.end(); }

		Scene& operator=(Scene&) = delete;

		static Scene* Create();

	private:
		std::vector<Renderable> m_Renderables = {};

		Scene() = default;
	};

}