#pragma once
#include "api/EggDrawData.h"

namespace egg
{
	struct PackedLightData;
	union PackedInstanceData;
	union PackedMaterialData;
	
	class DrawData : public EggDrawData
	{
		friend class Renderer;
		friend class RenderStage_Deferred;
	public:
		void SetCamera(const Camera& a_Camera) override;
		LightHandle AddLight(const DirectionalLight& a_Light) override;
		LightHandle AddLight(const SphereLight& a_Light) override;
		MaterialHandle AddMaterial(const std::shared_ptr<EggMaterial>& a_Material) override;
		MeshHandle AddMesh(const std::shared_ptr<EggMesh>& a_Mesh) override;
		InstanceDataHandle AddInstance(const glm::mat4& a_Transform, const MaterialHandle a_MaterialHandle,
			const uint32_t a_CustomId) override;
		DrawCallHandle AddDrawCall(MeshHandle a_MeshHandle, const InstanceDataHandle* a_Instances,
			uint32_t a_InstanceCount) override;
		DrawPassHandle AddDeferredShadingDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls) override;
		DrawPassHandle AddShadowDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls,
			const LightHandle a_LightHandle) override;
		uint32_t GetInstanceCount() const override;
		uint32_t GetDrawPassCount() const override;
		uint32_t GetDrawCallCount() const override;
		uint32_t GetMaterialCount() const override;
		uint32_t GetMeshCount() const override;
		uint32_t GetLightCount() const override;

	private:
		Camera m_Camera;											//Camera for this frame.
		std::vector<std::shared_ptr<EggMaterial>> m_Materials;		//Material handles used during this frame.
		std::vector<PackedMaterialData> m_PackedMaterialData;		//All materials used during this frame.
		std::vector<PackedLightData> m_PackedLightData;				//Lights used during this frame.
		std::vector<std::shared_ptr<EggMesh>> m_Meshes;				//All meshes used during this frame.
		std::vector<PackedInstanceData> m_PackedInstanceData;		//Buffer of instance data, ready for upload.
		std::vector<uint32_t> m_IndirectionBuffer;					//Indirection buffer, contains indices into instance data.
		std::vector<DrawCall> m_drawCalls;							//Draw calls for this frame.
		std::vector<DrawPass> m_DrawPasses;							//Draw passes referring to the draw calls.
	};
}
