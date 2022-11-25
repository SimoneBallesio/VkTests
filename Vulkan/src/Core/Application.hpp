#pragma once

namespace VKP
{

	class Window;
	class Context;

	class Application final
	{
	public:
		Application(Application&) = delete;
		~Application();

		void Run();
		void Stop();

		Application& operator=(Application&) = delete;

		static Application* Create();
		static Application& Get();

	private:
		Context* m_Context = nullptr;
		Window* m_Window = nullptr;

		bool m_Running = true;

		Application() = default;

		inline static Application* s_Instance = nullptr;
	};

}