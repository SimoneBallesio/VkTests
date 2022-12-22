#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"

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

		uint32_t flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN; // | SDL_WINDOW_RESIZABLE;
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

	double Window::GetTime() const
	{
		uint64_t steps = SDL_GetTicks64();
		return static_cast<double>(steps) / 1000.0;
	}

	void Window::PollEvents()
	{
		SDL_Event e;

		while (SDL_PollEvent(&e) != 0)
		{
			switch (e.type)
			{
				case SDL_QUIT:
				{
					Application::Get().Stop();
					return;
				}

				case SDL_WINDOWEVENT:
				{
					switch (e.window.event)
					{
						case SDL_WINDOWEVENT_SIZE_CHANGED:
						{
							VKP_INFO("Window size changed: ({}x{})", e.window.data1, e.window.data2);

							m_Data.Width = (uint32_t)e.window.data1;
							m_Data.Height = (uint32_t)e.window.data2;

							Context::Get().OnResize(m_Data.Width, m_Data.Height);

							break;
						}
					}
					break;
				}

				case SDL_MOUSEBUTTONDOWN:
				{
					Application::Get().OnMouseDown();
					break;
				}

				case SDL_MOUSEBUTTONUP:
				{
					Application::Get().OnMouseUp();
					break;
				}

				case SDL_MOUSEMOTION:
				{
					Application::Get().OnMouseMove((double)e.motion.xrel, (double)e.motion.yrel);
					break;
				}

				case SDL_KEYDOWN:
				{
					switch (e.key.keysym.sym)
					{
						case SDLK_ESCAPE:
							Application::Get().Stop();
							return;

						default:
							Application::Get().OnKeyDown(e.key.keysym.sym);
							break;
					}
					break;
				}

				case SDL_KEYUP:
				{
					Application::Get().OnKeyUp(e.key.keysym.sym);
					break;
				}
			}
		}
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