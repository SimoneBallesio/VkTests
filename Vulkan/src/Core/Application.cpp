#include "Pch.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Shader.hpp"

namespace VKP
{

	Application::~Application()
	{
		if (m_Model.Mat != nullptr)
			delete m_Model.Mat;

		if (m_Model.Model != nullptr)
			delete m_Model.Model;

		delete m_Context;
		delete m_Window;
		s_Instance = nullptr;
	}

	void Application::Init()
	{
		// m_Model.Mat = Material::Create("assets/shaders/base");
		// m_Model.Model = Mesh::Create("assets/models/viking_room.obj");

		auto cache = ShaderModuleCache::Create(Context::GetDevice());

		auto baseVert = cache->Create("assets/shaders/base.vert.spv");
		auto baseFrag = cache->Create("assets/shaders/base.frag.spv");

		ShaderEffect effect = {};
		effect.AddStage(baseVert, VK_SHADER_STAGE_VERTEX_BIT);
		effect.AddStage(baseFrag, VK_SHADER_STAGE_FRAGMENT_BIT);
		effect.Reflect(Context::GetDevice());

		ShaderModuleCache::Destroy();
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