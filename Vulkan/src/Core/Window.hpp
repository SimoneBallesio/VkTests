#pragma once

#include <SDL2/SDL.h>

#include <cstdint>

struct SDL_Window;

namespace VKP
{

	struct WindowData
	{
		uint32_t Width = 1280;
		uint32_t Height = 720;

		std::string Title = "Engine 3D";
	};

	class Window
	{
	public:
		Window(Window&) = delete;
		~Window();

		bool Init();

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

		const char* GetTitle() const;

		double GetTime() const;

		void PollEvents();

		void* GetNativeHandle() const;

		Window& operator=(Window&) = delete;

		static Window* Create(uint32_t width, uint32_t height, const std::string& title = "Engine 3D");
		static Window& Get();

	private:
		WindowData m_Data = {};
		SDL_Window* m_Window = nullptr;

		Window(uint32_t width, uint32_t height, const std::string& title);

		inline static Window* s_Instance = nullptr;
	};

}