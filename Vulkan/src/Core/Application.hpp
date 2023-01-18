#pragma once

#include "Rendering/Renderable.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/Camera.hpp"

#include <SDL2/SDL.h>

namespace VKP
{

	class Window;
	class Context;
	class Scene;

#ifdef VKP_DEBUG	

	struct Stats
	{
		float FrameTime = 0.0f;
	};

#endif

	class Application final
	{
	public:
		Application(Application&) = delete;
		~Application();

		void Init();

		void Run();
		void Draw();

		void Stop();

		void OnMouseDown();
		void OnMouseUp();
		void OnMouseMove(double x, double y);
		void OnKeyDown(SDL_Keycode key);
		void OnKeyUp(SDL_Keycode key);

		Application& operator=(Application&) = delete;

		static Application* Create();
		static Application& Get();

	private:
		Context* m_Context = nullptr;
		Window* m_Window = nullptr;

		Scene* m_Scene = nullptr;
		Camera m_Camera = {};

#ifdef VKP_DEBUG

		Stats m_Stats = {};

#endif

		bool m_Running = true;

		bool m_MouseDown = false;
		bool m_Forward = false;
		bool m_Backward = false;
		bool m_Right = false;
		bool m_Left = false;

		Application() = default;

		inline static Application* s_Instance = nullptr;
	};

}