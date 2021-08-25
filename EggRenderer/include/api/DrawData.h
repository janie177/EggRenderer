#pragma once
#include <memory>
#include <set>
#include <vector>
#include <glm/glm/glm.hpp>

#include "Camera.h"
#include "EggMaterial.h"
#include "EggLight.h"
#include "EggMesh.h"
#include "DrawDataBuilder.h"

namespace egg
{
	class DrawData;
	struct DrawCall;
	struct PackedLightData;
	union PackedInstanceData;
	union PackedMaterialData;
	
	//Opaque handle types.
	enum class MaterialHandle : uint32_t {};
	enum class MeshHandle : uint32_t {};
	enum class InstanceDataHandle : uint32_t {};
	enum class LightHandle : uint32_t {};
	enum class DrawCallHandle : uint32_t {};
	enum class DrawPassHandle : uint32_t {};
	
	/*
	 * The type indicating in which stage of rendering a draw call should be executed.
	 */
	enum class DrawPassType
	{
		STATIC_DEFERRED_SHADING,	//This draw pass will draw static meshes in a deferred pass.
		STATIC_FORWARD_SHADING,		//This draw pass will draw static meshes in a forward pass.
		SHADOW_GENERATION			//This draw pass will affect shadow map generation (cast shadows).
	};

	inline DrawPassType operator |(DrawPassType& a_Lhs, DrawPassType& a_Rhs) { return static_cast<DrawPassType>(static_cast<int>(a_Lhs) | static_cast<int>(a_Rhs)); }
	inline DrawPassType operator &(DrawPassType& a_Lhs, DrawPassType& a_Rhs) { return static_cast<DrawPassType>(static_cast<int>(a_Lhs) & static_cast<int>(a_Rhs)); }

	/*
	 * A draw call represents an action to be performed by the GPU.
	 */
	struct DrawCall
	{
		uint32_t m_MeshIndex;					//Index into the mesh array in the draw data.
		uint32_t m_IndirectionBufferOffset;		//Where in the indirection buffer the indices for this draw call start.
		uint32_t m_NumInstances;				//How many instances to draw.
	};

	/*
	 * A draw pass has a type which indicates how draw calls should be used.
	 * It contains one or more draw calls.
	 */
	struct DrawPass
	{
		DrawPassType m_Type;						//The type of draw pass.
		uint32_t m_LightHandle;					//If this is a shadow generation pass, this is the light it is generated for.
		std::vector<uint32_t> m_DrawCalls;	//The handles to the draw calls used by this draw pass.
	};
	
	/*
	 * DrawData is provided to the Renderer.
	 * It contains all information for a single frame to be drawn.
	 * When passed to the renderer, all contained state is consumed.
	 */
	struct DrawData
	{
		friend class Renderer;
		friend class RenderStage_Deferred;

	public:
		/*
		 * Set the camera used for this frame.
		 */
		void SetCamera(const Camera& a_Camera);

		/*
		 * Add a directional light to the scene in this frame.
		 * Returns a handle to the light.
		 */
		LightHandle AddLight(const DirectionalLight& a_Light);

		/*
		 * Add a spherical light to the scene in this frame.
		 * Returns a handle to the light.
		 */
		LightHandle AddLight(const SphereLight& a_Light);

		/*
		 * Add a material to be used during this frame.
		 * Returns a handle to the material that can be specified when adding instance data.
		 */
		MaterialHandle AddMaterial(const std::shared_ptr<EggMaterial>& a_Material);

		/*
		 * Add a mesh to be used during this frame.
		 * Returns a handle to the mesh that can be specified when creating draw calls.
		 */
		MeshHandle AddMesh(const std::shared_ptr<EggMesh>& a_Mesh);

		/*
		 * Add an instance's data to this frame.
		 *
		 * a_Transform represents a mat4x4 consisting of 16 32-bit floats in column-major order.
		 * a_MaterialHandle is the handle to a material previously added to this DrawData using AddMaterial().
		 * a_CustomId is an identifier that can be queried for a location on the screen after drawing.
		 *
		 * Returns a handle that can be provided to the AddDrawCall() function.
		 */
		InstanceDataHandle AddInstance(const glm::mat4& a_Transform, const MaterialHandle a_MaterialHandle, const uint32_t a_CustomId);

		/*
		 * Add a draw call to this frame.
		 * A draw call represents a drawing operation involving geometry and instance data.
		 * A single draw call can be used by one or more draw passes.
		 *
		 * a_MeshHandle is the handle of the mesh to use for the geometry.
		 * a_Instances is a collection of instance data handles (returned by the AddInstance() function).
		 * a_InstanceCount is the amount of instances in the a_Instances collection.
		 *
		 * Returns a handle to the newly created draw call, which can be passed to the functions that add draw passes.
		 */
		DrawCallHandle AddDrawCall(MeshHandle a_MeshHandle, const InstanceDataHandle* a_Instances, uint32_t a_InstanceCount);

		/*
		 * Add a deferred shading draw pass.
		 * All draw calls in this pass will be shaded and output to the window.
		 * Transparency is not supported.
		 * 
		 * a_DrawCalls is a collection of draw calls that will be used for this pass.
		 * a_NumDrawCalls is the amount of draw calls in the collection.
		 *
		 * Returns a handle to the draw pass created.
		 */
		DrawPassHandle AddDeferredShadingDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls);

		/*
		 * Add a shadow map generation draw pass.
		 * All draw calls in this pass will affect the shadows that are cast from light a_LightHandle.
		 *
		 * a_DrawCalls is a collection of draw calls that will be used for this pass.
		 * a_NumDrawCalls is the amount of draw calls in the collection.
		 * a_LightHandle is the handle of the light that shadows will be generated for.
		 *
		 * Returns a handle to the draw pass created.
		 */
		DrawPassHandle AddShadowDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls, const LightHandle a_LightHandle);

		/*
		 * Get the amount of instances that have been added for this frame.
		 */
		uint32_t GetInstanceCount() const;

		/*
		 * Get the amount of draw passes that have been added for this frame.
		 */
		uint32_t GetDrawPassCount() const;

		/*
		 * Get the amount of draw calls that have been added for this frame.
		 */
		uint32_t GetDrawCallCount() const;

		/*
		 * Get the amount of materials used by this frame.
		 */
		uint32_t GetMaterialCount() const;

		/*
		 * Get the amount of meshes used by this frame.
		 */
		uint32_t GetMeshCount() const;

		/*
		 * Get the amount of lights used by this frame.
		 */
	    uint32_t GetLightCount() const;

	private:
		Camera m_Camera;											//Camera for this frame.
		std::vector<std::shared_ptr<EggMaterial>> m_Materials;		//All materials used during this frame.
		std::vector<uint32_t> m_MaterialGpuIndices;					//Material indices on the GPU corresponding to m_Materials.
		std::vector<PackedLightData> m_PackedLightData;				//Lights used during this frame.
		std::vector<std::shared_ptr<EggMesh>> m_Meshes;				//All meshes used during this frame.
		std::vector<PackedInstanceData> m_PackedInstanceData;		//Buffer of instance data, ready for upload.
		std::vector<uint32_t> m_IndirectionBuffer;					//Indirection buffer, contains indices into instance data.

		std::vector<DrawCall> m_drawCalls;							//Draw calls for this frame.
		std::vector<DrawPass> m_DrawPasses;							//Draw passes referring to the draw calls.
	};



	void example_function()
	{
		//There's multiple types of light, each with a very simple implementation.
		DirectionalLight light;

		struct SceneNode
		{
			std::shared_ptr<EggMesh> mesh;
			std::shared_ptr<EggMaterial> material;
			glm::mat4 transform;
			uint32_t id = 0;
		} my_scene_object;

		//Draw data to be consumed by a frame.
		DrawData drawData;

		/*
		 * Add the individual resources and retrieve handles for them.
		 * Note: Manually avoid duplicate adding of the same resource for better performance.
		 */
		const auto lightRef = drawData.AddLight(light);
		const auto meshRef = drawData.AddMesh(my_scene_object.mesh);
		const auto materialRef = drawData.AddMaterial(my_scene_object.material);
		const auto instanceRef = drawData.AddInstance(my_scene_object.transform, materialRef, my_scene_object.id);

		/*
		 * Create a draw call that draws this one instance of the mesh.
		 */
		const auto drawCallRef = drawData.AddDrawCall(meshRef, &instanceRef, 1);

		/*
		 * Create a deferred draw pass that executes the draw call and shades it.
		 * Also create a shadow pass for the same draw call which fills the shadow map for the light provided.
		 */
	    drawData.AddDeferredShadingDrawPass(&drawCallRef, 1);
		drawData.AddShadowDrawPass(&drawCallRef, 1, lightRef);
	}
}