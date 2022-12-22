#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace VKP
{

	struct Camera
	{
		glm::vec3 Position = glm::vec3(0.0f, 1.7f, 5.0f);
		glm::quat Orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		glm::vec3 Forward() const;
		glm::vec3 Right() const;
		glm::vec3 Up() const;

		glm::mat4 ViewMatrix() const;
	};

}