#include "DrawData.h"
#include "Resources.h"

namespace egg
{
	void DrawData::SetCamera(const Camera& a_Camera)
	{
		m_Camera = a_Camera;
	}

    LightHandle DrawData::AddLight(const DirectionalLight& a_Light)
    {
		m_PackedLightData.emplace_back(PackedLightData{ 
			{a_Light.m_Direction[0], a_Light.m_Direction[1], a_Light.m_Direction[2], 0.f},
			{a_Light.m_Radiance[0], a_Light.m_Radiance[1], a_Light.m_Radiance[2], 0.f}
		});
		return static_cast<LightHandle>(m_PackedLightData.size() - 1);
    }

    LightHandle DrawData::AddLight(const SphereLight& a_Light)
    {
        m_PackedLightData.emplace_back(PackedLightData{
    {a_Light.m_Position[0], a_Light.m_Position[1], a_Light.m_Position[2], a_Light.m_Radius},
    {a_Light.m_Radiance[0], a_Light.m_Radiance[1], a_Light.m_Radiance[2], 0.f}
            });
        return static_cast<LightHandle>(m_PackedLightData.size() - 1);
    }

    MaterialHandle DrawData::AddMaterial(const std::shared_ptr<EggMaterial>& a_Material)
    {
        //Keep a reference alive and also tightly pack the data for uploading.
        m_Materials.push_back(a_Material);
        m_PackedMaterialData.push_back(std::static_pointer_cast<Material>(a_Material)->PackMaterialData());

        return static_cast<MaterialHandle>(m_PackedMaterialData.size() - 1);
    }

    MeshHandle DrawData::AddMesh(const std::shared_ptr<EggMesh>& a_Mesh)
    {
        m_Meshes.push_back(a_Mesh);
        return static_cast<MeshHandle>(m_Meshes.size() - 1);
    }

    InstanceDataHandle DrawData::AddInstance(const glm::mat4& a_Transform, const MaterialHandle a_MaterialHandle,
        const uint32_t a_CustomId)
    {
        //Ensure that the material handle is valid.
        assert(static_cast<uint32_t>(a_MaterialHandle) < m_PackedMaterialData.size() && "Material handle referes to a material that was not added!");

        auto& instance = m_PackedInstanceData.emplace_back();

        instance.m_Transform = a_Transform;
        instance.m_MaterialId = static_cast<uint32_t>(a_MaterialHandle);
        instance.m_CustomId = a_CustomId;
        
        return static_cast<InstanceDataHandle>(m_PackedInstanceData.size() - 1);
    }

    DrawCallHandle DrawData::AddDrawCall(MeshHandle a_MeshHandle, const InstanceDataHandle* a_Instances,
        uint32_t a_InstanceCount)
    {
#ifndef NDEBUG
        //Ensure that the mesh is valid.
        assert(static_cast<uint32_t>(a_MeshHandle) < m_Meshes.size() && "Invalid mesh provided!");

        //Ensure that every instance provided is also valid.
        for(uint32_t i = 0; i < a_InstanceCount; ++i)
        {
            assert(static_cast<uint32_t>(a_Instances[i]) < m_PackedInstanceData.size() && "Invalid instance provided!");
        }
#endif

        //Create a draw call after adding the instance data indices to the indirection buffer.
        const uint32_t indirectionBufferOffset = static_cast<uint32_t>(m_IndirectionBuffer.size());
        m_IndirectionBuffer.insert(m_IndirectionBuffer.end(), reinterpret_cast<const uint32_t*>(&a_Instances[0]), reinterpret_cast<const uint32_t*> (&a_Instances[a_InstanceCount]));
        m_drawCalls.push_back(DrawCall{static_cast<uint32_t>(a_MeshHandle), indirectionBufferOffset, a_InstanceCount});
        return static_cast<DrawCallHandle>(m_drawCalls.size() - 1);
    }

    DrawPassHandle DrawData::AddDeferredShadingDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls)
    {
#ifndef NDEBUG
        for (uint32_t i = 0; i < a_NumDrawCalls; ++i)
        {
            assert(static_cast<uint32_t>(a_DrawCalls[i]) < m_drawCalls.size() && "Invalid draw call provided!");
        }
#endif

        //Create a new draw pass.
        auto& pass = m_DrawPasses.emplace_back();
        pass.m_Type = DrawPassType::STATIC_DEFERRED_SHADING;
        pass.m_DrawCalls.insert(pass.m_DrawCalls.end(), reinterpret_cast<const uint32_t*>(&a_DrawCalls[0]), reinterpret_cast<const uint32_t*>(&a_DrawCalls[a_NumDrawCalls]));

        return static_cast<DrawPassHandle>(m_DrawPasses.size() - 1);
    }

    DrawPassHandle DrawData::AddShadowDrawPass(const DrawCallHandle* a_DrawCalls, uint32_t a_NumDrawCalls,
        const LightHandle a_LightHandle)
    {
#ifndef NDEBUG
        //Verify handles correctness
        assert(static_cast<uint32_t>(a_LightHandle) < m_PackedLightData.size() && "Invalid light handle provided!");

        for (uint32_t i = 0; i < a_NumDrawCalls; ++i)
        {
            assert(static_cast<uint32_t>(a_DrawCalls[i]) < m_drawCalls.size() && "Invalid draw call provided!");
        }
#endif

        //Create a new draw pass.
        auto& pass = m_DrawPasses.emplace_back();
        pass.m_Type = DrawPassType::SHADOW_GENERATION;
        pass.m_DrawCalls.insert(pass.m_DrawCalls.end(), reinterpret_cast<const uint32_t*>(&a_DrawCalls[0]), reinterpret_cast<const uint32_t*>(&a_DrawCalls[a_NumDrawCalls]));
        pass.m_LightHandle = static_cast<uint32_t>(a_LightHandle);
        return static_cast<DrawPassHandle>(m_DrawPasses.size() - 1);
    }

    uint32_t DrawData::GetInstanceCount() const
    {
        return static_cast<uint32_t>(m_PackedInstanceData.size());
    }

    uint32_t DrawData::GetDrawPassCount() const
    {
        return static_cast<uint32_t>(m_DrawPasses.size());
    }

    uint32_t DrawData::GetDrawCallCount() const
    {
        return static_cast<uint32_t>(m_drawCalls.size());
    }

    uint32_t DrawData::GetMaterialCount() const
    {
        return static_cast<uint32_t>(m_Materials.size());
    }

    uint32_t DrawData::GetMeshCount() const
    {
		return static_cast<uint32_t>(m_Meshes.size());
    }

    uint32_t DrawData::GetLightCount() const
    {
		return static_cast<uint32_t>(m_PackedLightData.size());
    }
}
