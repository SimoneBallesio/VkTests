#include "Pch.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Renderer.hpp"

namespace VKP
{

	Application::~Application()
	{
		Impl::State::Data->DeletionQueue.Push([&]() {
			if (m_Model.Model != nullptr)
			delete m_Model.Model;

			Renderer3D::Destroy();
		});

		delete m_Context;
		delete m_Window;

		s_Instance = nullptr;
	}

	void Application::Init()
	{
		m_Diffuse = TextureCache::Get().Create("assets/models/viking_room.png");
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
		if (valid) valid = Renderer3D::Init();

		if (!valid) return;

		Init();

		while (m_Running)
		{
			Draw();
			m_Window->PollEvents();
		}
	}

	void Application::Draw()
	{
		m_Context->BeginFrame();

		Renderer3D::SubmitRenderable(&m_Model);
		Renderer3D::Flush(&m_Camera);

		m_Context->EndFrame();
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