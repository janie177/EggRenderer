#include "api/DrawData.h"
#include "Resources.h"

namespace egg
{
	void DrawData::SetCamera(const Camera& a_Camera)
	{
		m_Camera = a_Camera;
	}

	uint32_t DrawData::AddMaterial(const std::shared_ptr<EggMaterial>& a_Material)
	{
		m_Materials.push_back(a_Material);
		return static_cast<uint32_t>(m_Materials.size()) - 1;
	}

	uint32_t DrawData::AddMesh(const std::shared_ptr<EggMesh>& a_Mesh)
	{
		m_Meshes.push_back(a_Mesh);
		return static_cast<uint32_t>(m_Meshes.size()) - 1;
	}

	uint32_t DrawData::AddLight(const std::shared_ptr<EggLight>& a_Light)
	{
		//TODO implement
		return static_cast<uint32_t>(m_PackedLightData.size()) - 1;
	}

	uint32_t DrawData::AddInstance(const glm::mat4& a_Transform, const uint32_t a_MaterialIndex,
		const void* a_CustomPtr)
	{
		//TODO implement
		return static_cast<uint32_t>(m_PackedInstanceData.size()) - 1;
	}

	uint32_t DrawData::GetInstanceCount() const
	{
		return static_cast<uint32_t>(m_PackedInstanceData.size());
	}

	uint32_t DrawData::AddInstance(const glm::mat4& a_Transform, const uint32_t a_MaterialIndex,
	                               const void* a_CustomPtr, const uint32_t a_AnimationFrameIndex)
	{
		//TODO
		return static_cast<uint32_t>(m_PackedInstanceData.size()) - 1;
	}

	void DrawData::AddDrawCall(uint32_t a_MeshIndex, uint32_t a_InstanceOffset, uint32_t a_NumInstances,
		const DrawFlags a_DrawFlags)
	{
		m_drawCalls.emplace_back(DrawCall{ a_DrawFlags, a_InstanceOffset, a_NumInstances, a_MeshIndex });
	}
}
