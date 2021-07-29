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
		friend class RenderStage_Deferred;
	private:
		DynamicDrawCall() = default;

		/*
		 * Update the draw call.
		 * Called by the renderer.
		 *
		 * The last frame that materials changed, and the current frame are required.
		 */
		void Update(uint32_t a_LastMaterialUploadFrame, uint32_t a_CurrentFrameIndex);

	private:
		struct MaterialData
		{
			std::shared_ptr<EggMaterial> m_Material;
			uint32_t m_LastUpdatedFrameId;
		};

	private:
		/*
		 * The mesh to draw.
		 */
		std::shared_ptr<EggMesh> m_Mesh;

		/*
		 * The instance data tightly packed and ready for uploading to the GPU.
		 */
		std::vector<PackedInstanceData> m_InstanceData;

		/*
		 * The indices of the materials per instance data.
		 * Used to dynamically update if materials have changed.
		 */
		std::vector<uint32_t> m_MaterialIndices;

		/*
		 * The materials used by this draw call.
		 */
		std::vector<MaterialData> m_MaterialData;

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

	private:

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
