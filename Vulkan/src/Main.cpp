#include "Pch.hpp"

#include "Core/Application.hpp"

#ifdef VKP_DEBUG
#include "Core/Log.hpp"
#endif

int main()
{
#ifdef VKP_DEBUG
	VKP::Log::Init();
#endif

	VKP::Application* app = VKP::Application::Create();
	app->Run();

	delete app;
	return 0;
}