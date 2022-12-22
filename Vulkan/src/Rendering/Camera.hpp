#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace VKP
{

	struct Camera
	{
		glm::vec3 Position = glm::vec3(0.0f, 0.0f, -3.0f);
		float Pitch = 0.0f;
		float Yaw = 0.0f;

		glm::mat4 VP;

		glm::quat Orientation() const;
		glm::mat4 ViewMatrix() const;
	};

}