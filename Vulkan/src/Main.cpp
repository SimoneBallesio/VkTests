#include "Pch.hpp"

#include "Core/Log.hpp"
#include "Core/Definitions.hpp"
#include "Core/Window.hpp"

#include "Rendering/Context.hpp"

int main(int argc, char* argv[])
{
	VKP::Log::Init();

	auto context = VKP::Context::Create();
	auto window = VKP::Window::Create(1280, 720);

	bool success = context->BeforeWindowCreation();
	if (success) success = window->Init();
	if (success) success = context->AfterWindowCreation();

	delete window;
	delete context;

	return 0;
}