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

		std::vector<Renderable>::iterator Begin();
		std::vector<Renderable>::iterator End();

		std::vector<Renderable>::const_iterator Begin() const;
		std::vector<Renderable>::const_iterator End() const;

		Scene& operator=(Scene&) = delete;

		static Scene* Create();

	private:
		std::vector<Renderable> m_Renderables = {};

		Scene() = default;
	};

}