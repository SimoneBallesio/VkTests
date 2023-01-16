#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Renderer.hpp"

namespace VKP
{

	Application::~Application()
	{
		Impl::State::Data->DeletionQueue.Push([&]()
		{
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
		m_Diffuse = TextureCache::Get().Create("assets/models/viking_room.texi");
		m_Model.Model = Mesh::Create("assets/models/viking_room.mesh");
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
		if (m_Forward) m_Camera.Position += m_Camera.Forward() * 0.02f;
		if (m_Backward) m_Camera.Position -= m_Camera.Forward() * 0.02f;
		if (m_Right) m_Camera.Position += m_Camera.Right() * 0.02f;
		if (m_Left) m_Camera.Position -= m_Camera.Right() * 0.02f;

		m_Context->BeginFrame();

		Renderer3D::SubmitRenderable(&m_Model);
		Renderer3D::Flush(&m_Camera);

		m_Context->EndFrame();
	}

	void Application::Stop()
	{
		m_Running = false;
	}

	void Application::OnMouseDown()
	{
		m_MouseDown = true;
	}

	void Application::OnMouseUp()
	{
		m_MouseDown = false;
	}

	void Application::OnMouseMove(double x, double y)
	{
		if (!m_MouseDown) return;
		if (x == 0.0 && y == 0.0) return;

		glm::quat pitch = glm::angleAxis((float)y * 0.005f, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw = glm::angleAxis((float)x * 0.005f, glm::vec3(0.0f, 1.0f, 0.0f));

		m_Camera.Orientation = yaw * m_Camera.Orientation * pitch;
	}

	void Application::OnKeyDown(SDL_Keycode key)
	{
		switch (key)
		{
			case SDLK_w:
				m_Forward = true;
				break;

			case SDLK_s:
				m_Backward = true;
				break;

			case SDLK_d:
				m_Right = true;
				break;

			case SDLK_a:
				m_Left = true;
				break;
		}
	}

	void Application::OnKeyUp(SDL_Keycode key)
	{
		switch (key)
		{
		case SDLK_w:
			m_Forward = false;
			break;

		case SDLK_s:
			m_Backward = false;
			break;

		case SDLK_d:
			m_Right = false;
			break;

		case SDLK_a:
			m_Left = false;
			break;
		}
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