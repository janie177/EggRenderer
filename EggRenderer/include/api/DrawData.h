#pragma once
#include <memory>
#include <set>
#include <vector>
#include <glm/glm/glm.hpp>

#include "Camera.h"
#include "EggMaterial.h"
#include "EggMesh.h"
#include "DrawDataBuilder.h"

namespace egg
{
	struct DrawCall;
	struct DrawData;

	
	//TODO actually create this type.
	class EggLight;
	
	/*
     * Instance data that is packed and aligned correctly.
     * Custom data can be stored in the last row of the matrix.
     */
	union PackedInstanceData
	{
		//Everything packed into a matrix.
		glm::mat4 m_Matrix;

		//Normally GLM matrices are column major, but I'll interpret them as row major instead.
		struct
		{
			glm::vec4 m_TransformRow1;
			glm::vec4 m_TransformRow2;
			glm::vec4 m_TransformRow3;
			union
			{
				glm::vec4 m_CustomData;
				struct
				{
					float m_CustomData1;
					float m_CustomData2;
					float m_CustomData3;
					float m_CustomData4;
				};
			};
		};
	};

	/*
	 * Light data ready to be uploaded to the GPU.
	 * This struct can contain position, direction, radiance, angle, radius etc.
	 */
	struct PackedLightData
	{
		glm::vec4 m_Data1;
		glm::vec4 m_Data2;
		glm::vec4 m_Data3;
	};
	
	/*
	 * Information about when and how a draw call should be used.
	 * Multiple flags can be combined to have a draw call be used in multiple render stages.
	 */
	enum class DrawFlags
	{
		STATIC_DEFERRED_SHADING,	//This draw call will draw static meshes in a deferred pass.
		STATIC_FORWARD_SHADING,		//This draw call will draw static meshes in a forward pass.
		SHADOW_CASTER				//This draw call will affect shadow map generation (cast shadows).
	};

	inline DrawFlags operator |(DrawFlags& a_Lhs, DrawFlags& a_Rhs) { return static_cast<DrawFlags>(static_cast<int>(a_Lhs) | static_cast<int>(a_Rhs)); }
	inline DrawFlags operator &(DrawFlags& a_Lhs, DrawFlags& a_Rhs) { return static_cast<DrawFlags>(static_cast<int>(a_Lhs) & static_cast<int>(a_Rhs)); }

	/*
	 * A draw call represents an action to be performed by the GPU.
	 */
	struct DrawCall
	{
		DrawFlags m_DrawMask;
		uint32_t m_InstanceOffset;
		uint32_t m_NumInstances;
		uint32_t m_MeshIndex;
	};
	
	/*
	 * DrawData is provided to the Renderer.
	 * It contains all information for a single frame to be drawn.
	 */
	struct DrawData
	{
		friend class Renderer;
		friend class RenderStage_Deferred;
	public:
		
		/*
		 * Set the camera to be used for this frame.
		 */
		void SetCamera(const Camera& a_Camera);
		
		/*
		 * Add a material to be used during this frame.
		 * Returns the index of the material within the draw data.
		 * This index can be passed to the AddInstance function.
		 */
		uint32_t AddMaterial(const std::shared_ptr<EggMaterial>& a_Material);

		/*
		 * Add a mesh to be used during this frame.
		 * Returns the index of the mesh within the draw data.
		 * This index can be passed to the AddInstance function.
		 */
		uint32_t AddMesh(const std::shared_ptr<EggMesh>& a_Mesh);

		/*
		 * Add a light to the scene for this frame.
		 * The index of the light is returned.
		 */
		uint32_t AddLight(const std::shared_ptr<EggLight>& a_Light);

		/*
		 * Add instance data to be used during this frame.
		 * a_Transform is the world space rotation, translation and scale.
		 * a_MaterialIndex is the index of the material to use to draw this object.
		 * a_CustomPtr is a user defined pointer that can be queried for a position on the screen.
		 */
		uint32_t AddInstance(const glm::mat4& a_Transform, const uint32_t a_MaterialIndex, const void* a_CustomPtr);

		/*
		 * Get the amount of instances currently in this draw data object.
		 */
		uint32_t GetInstanceCount() const;

		/*
		 * Add instance data to be used during this frame.
		 * a_Transform is the world space rotation, translation and scale.
		 * a_MaterialIndex is the index of the material to use to draw this object.
		 * a_CustomPtr is a user defined pointer that can be queried for a position on the screen.
		 * a_AnimationFrameIndex is the frame of the animation used in case this is a skeletal animated mesh.
		 *
		 * The index of the instance added is returned. This can be passed to the AddDrawCall function.
		 */
		uint32_t AddInstance(const glm::mat4& a_Transform, const uint32_t a_MaterialIndex, const void* a_CustomPtr, const uint32_t a_AnimationFrameIndex);

		/*
		 * Add a draw call to the draw data for this frame.
		 * a_MeshIndex is the index of the mesh to use for this draw call.
		 * a_InstanceOffset is the offset into the instance data buffer to start drawing.
		 * The AddInstance function returns the index of each added instance.
		 * a_NumInstances is the amount of instances to consecutively draw.
		 * a_DrawFlags specifies how this draw call will be executed (deferred, static, skeletal, shadowmapping etc.)
		 * Multiple draw flags can be combined using the | operator.
		 */
		void AddDrawCall(uint32_t a_MeshIndex, uint32_t a_InstanceOffset, uint32_t a_NumInstances, const DrawFlags a_DrawFlags);
	
	private:
		Camera m_Camera;
		std::vector<std::shared_ptr<EggMaterial>> m_Materials;
		std::vector<std::shared_ptr<EggMesh>> m_Meshes;
		std::vector<PackedInstanceData> m_PackedInstanceData;
		std::vector<PackedLightData> m_PackedLightData;
		std::vector<DrawCall> m_drawCalls;
	};
}