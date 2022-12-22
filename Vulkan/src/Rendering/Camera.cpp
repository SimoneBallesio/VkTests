#include "Pch.hpp"

#include "Rendering/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace VKP
{

	glm::vec3 Camera::Forward() const
	{
		return glm::vec3(0.0f, 0.0f, -1.0f) * glm::conjugate(Orientation);
	}

	glm::vec3 Camera::Right() const
	{
		return glm::vec3(1.0f, 0.0f, 0.0f) * glm::conjugate(Orientation);
	}

	glm::vec3 Camera::Up() const
	{
		return glm::vec3(0.0f, 1.0f, 0.0f) * glm::conjugate(Orientation);
	}

	glm::mat4 Camera::ViewMatrix() const
	{
		return glm::mat4_cast(glm::conjugate(Orientation)) * glm::translate(glm::mat4(1.0f), -Position);
	}

}