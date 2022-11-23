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
		return false;
	}

	Window::~Window()
	{
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