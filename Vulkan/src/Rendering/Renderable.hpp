#pragma once

#include <glm/glm.hpp>

namespace VKP
{

	class Mesh;
	class Material;

	struct Renderable
	{
		Mesh* Model = nullptr;
		Material* Mat = nullptr;
		glm::mat4 Matrix = glm::mat4(1.0f);
	};

}