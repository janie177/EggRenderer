#pragma once
#include <glm/glm/glm.hpp>
#include <glm/glm/ext/matrix_clip_space.hpp>

#include "Transform.h"

namespace egg
{
	/*
     * The camera containing the FOV and such.
     */
	class Camera
	{
	public:
		Camera()
		{
			UpdateProjection(90.f, 0.1f, 1000.f, 1.f);
		}

		/*
		 * Update the camera settings.
		 * Fov is provided in degrees.
		 */
		void UpdateProjection(float a_Fov, float a_NearPlane, float a_FarPlane, float a_AspectRatio)
		{
			m_Fov = a_Fov;
			m_NearPlane = a_NearPlane;
			m_FarPlane = a_FarPlane;
			m_AspectRatio = a_AspectRatio;


			m_ProjectionMatrix = glm::perspective(glm::radians(a_Fov), a_AspectRatio, a_NearPlane, a_FarPlane);
		}

		/*
		 * Get a reference to the cameras transform.
		 * Modifications automatically apply to the camera when the matrices are re-retrieved.
		 */
		Transform& GetTransform()
		{
			return m_Transform;
		}

		/*
		 * Calculate the view projection matrices combined.
		 */
		glm::mat4 CalculateVPMatrix() const
		{
			return GetProjectionMatrix() * GetViewMatrix();
		}

		/*
		 * Calculate the camera's view matrix.
		 */
		glm::mat4 GetViewMatrix() const
		{
			return glm::inverse(m_Transform.GetTransformation());
		}

		glm::mat4 GetProjectionMatrix() const
		{
			return m_ProjectionMatrix;
		}

	private:
		float m_Fov;
		float m_NearPlane;
		float m_FarPlane;
		float m_AspectRatio;

		Transform m_Transform;
		glm::mat4 m_ProjectionMatrix;
	};
}
