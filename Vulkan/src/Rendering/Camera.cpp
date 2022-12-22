#include "Pch.hpp"

#include "Rendering/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace VKP
{
	
	glm::quat Camera::Orientation() const
	{
		return glm::quat(glm::vec3(-Pitch, -Yaw, 0.0f));
	}

	glm::mat4 Camera::ViewMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), Position) * glm::toMat4(Orientation());
	}

}