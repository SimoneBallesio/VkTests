#pragma once

#include "Rendering/Renderable.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/Camera.hpp"

namespace VKP
{

	class Window;
	class Context;

	class Application final
	{
	public:
		Application(Application&) = delete;
		~Application();

		void Init();

		void Run();
		void Draw();

		void Stop();

		Application& operator=(Application&) = delete;

		static Application* Create();
		static Application& Get();

	private:
		Context* m_Context = nullptr;
		Window* m_Window = nullptr;

		Camera m_Camera = {};

		Renderable m_Model = {};
		Texture* m_Diffuse = nullptr;

		bool m_Running = true;

		Application() = default;

		inline static Application* s_Instance = nullptr;
	};

}