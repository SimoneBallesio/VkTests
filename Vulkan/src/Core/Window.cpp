#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include <SDL2/SDL.h>

namespace VKP
{

	Window::Window(uint32_t width, uint32_t height, const std::string& title)
	{
		m_Data.Width = width;
		m_Data.Height = height;
		m_Data.Title = title;
	}

	bool Window::Init()
	{
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
			return false;

		uint32_t flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN;
		m_Window = SDL_CreateWindow(m_Data.Title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_Data.Width, m_Data.Height, flags);

		SDL_GetWindowSize(m_Window, (int*)&m_Data.Width, (int*)&m_Data.Height);

		return m_Window != nullptr;
	}

	Window::~Window()
	{
		if (m_Window != nullptr)
			SDL_DestroyWindow(m_Window);

		SDL_Quit();
	}

	uint32_t Window::GetWidth() const
	{
		return m_Data.Width;
	}

	uint32_t Window::GetHeight() const
	{
		return m_Data.Height;
	}

	const char* Window::GetTitle() const
	{
		return m_Data.Title.c_str();
	}

	void Window::PollEvents() const
	{
	}

	void* Window::GetNativeHandle() const
	{
		return (void*)m_Window;
	}

	Window* Window::Create(uint32_t width, uint32_t height, const std::string& title)
	{
		if (s_Instance == nullptr)
			s_Instance = new Window(width, height, title);

		return s_Instance;
	}

	Window& Window::Get()
	{
		return *s_Instance;
	}

}