#pragma once
#include <memory>
#include <vector>
#include <glm/glm/glm.hpp>

#include "Camera.h"
#include "EggMaterial.h"
#include "EggMesh.h"

namespace egg
{
	/*
     * Instance data that is packed and aligned correctly.
     * The material ID and used pointer are stored in the empty matrix members (material, uPtr32, 0, 1) bottom row.
     */
	struct PackedInstanceData
	{
		glm::mat4 m_Transform;
	};

	/*
	 * A single dynamic draw call.
	 * This type of draw call re-uploads all data every frame.
	 */
	struct DynamicDrawCall
	{
		friend class Renderer;
	private:
		DynamicDrawCall() = default;

	public:
		/*
		 * The mesh to draw.
		 */
		std::shared_ptr<EggMesh> m_Mesh;

		/*
		 * The instance data tightly packed and ready for uploading to the GPU.
		 */
		std::vector<PackedInstanceData> m_InstanceData;

		/*
		 * The materials used by this draw call.
		 */
		std::vector<std::shared_ptr<EggMaterial>> m_Materials;

		/*
		 * If true, this geometry is drawn in a separate forward pass after the deferred stage ends.
		 */
		 //TODO
		bool m_Transparent = false;
	};

	/*
	 * All information needed to draw a single frame with the renderer.
	 * All data is copied when this is passed to the DrawFrame function of Renderer.
	 */
	struct DrawData
	{
		friend class Renderer;
		friend class RenderStage_Deferred;

		/*
		 * Set the camera for this draw data.
		 */
		DrawData& SetCamera(const Camera& a_Camera);

		/*
		 * Add a draw call to this draw data.
		 */
		DrawData& AddDrawCall(const DynamicDrawCall& a_DrawCall);

		/*
		 * Reset this draw data object back to it's original state.
		 */
		void Reset();

	public:

		/*
		 * A pointer to the camera to use.
		 */
		Camera m_Camera;

		/*
		 * All draw calls for the frame to be renderer.
		 */
		std::vector<DynamicDrawCall> m_DynamicDrawCalls;

		/*
		 * All the materials used by the draw calls.
		 */
		std::vector<std::shared_ptr<EggMaterial>> m_Materials;
	};
}
