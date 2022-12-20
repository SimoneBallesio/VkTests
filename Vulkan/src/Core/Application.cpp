#include "Pch.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Shader.hpp"

namespace VKP
{

	Application::~Application()
	{
		if (m_Model.Model != nullptr)
			delete m_Model.Model;

		if (m_Diffuse != nullptr)
			Context::Get().DestroyTexture(*m_Diffuse);

		delete m_Context;
		delete m_Window;

		s_Instance = nullptr;
	}

	void Application::Init()
	{
		m_Diffuse = Texture::Load("assets/models/viking_room.png");
		m_Model.Model = Mesh::Create("assets/models/viking_room.obj");
		m_Model.Mat = MaterialCache::Get().Create("default", { m_Diffuse });
	}

	void Application::Run()
	{
		m_Context = Context::Create();
		m_Window = Window::Create(1280, 720);

		bool valid = m_Context->BeforeWindowCreation();
		if (valid) valid = m_Window->Init();
		if (valid) valid = m_Context->AfterWindowCreation();

		if (!valid) return;

		Init();

		while (m_Running)
		{
			Draw();
			m_Context->SwapBuffers();
			m_Window->PollEvents();
		}
	}

	void Application::Draw()
	{
		Context::Get().SubmitRenderable(&m_Model);
	}

	void Application::Stop()
	{
		m_Running = false;
	}

	Application* Application::Create()
	{
		if (s_Instance == nullptr)
			s_Instance = new Application();

		return s_Instance;
	}

	Application& Application::Get()
	{
		return *s_Instance;
	}

}