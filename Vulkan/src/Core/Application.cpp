#include "Pch.hpp"

#include "Core/Definitions.hpp"
#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Mesh.hpp"

#include "Scene/Scene.hpp"

#include <AssetLibrary.hpp>

#include <filesystem>

namespace VKP
{

	Application::~Application()
	{
		Impl::State::Data->DeletionQueue.Push([&]() { Renderer3D::Destroy(); });

		delete m_Context;
		delete m_Window;
		delete m_Scene;

		s_Instance = nullptr;
	}

	void Application::Init()
	{
		m_Scene->LoadFromPrefab("assets/models/BatchingTest/BatchingTest.prfb");
		// ^ should register objects with renderer3D
		// ^ Renderer3D should split them into passes, then push them to large VBO/IBO
		// ^ then, Renderer3D should build initial batches
	}

	void Application::Run()
	{
		m_Context = Context::Create();
		m_Window = Window::Create(1280, 720);
		m_Scene = Scene::Create();

		bool valid = m_Context->BeforeWindowCreation();
		if (valid) valid = m_Window->Init();
		if (valid) valid = m_Context->AfterWindowCreation();
		if (valid) valid = Renderer3D::Init();

		if (!valid) return;

		Init();

#ifdef VKP_DEBUG

		std::chrono::time_point<std::chrono::system_clock> start, end;

		start = std::chrono::system_clock::now();
		end = std::chrono::system_clock::now();

#endif

		while (m_Running)
		{

#ifdef VKP_DEBUG

			end = std::chrono::system_clock::now();
			std::chrono::duration<float> delta = end - start;

			m_Stats.FrameTime = delta.count() * 1000.0f;
			start = std::chrono::system_clock::now();

#endif

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