#include "Resources.h"
#include "MaterialManager.h"

namespace egg
{
    Material::Material(const MaterialCreateInfo& a_Info, MaterialManager& a_Manager) : m_DirtyFlag(false)
    {
        m_Manager = &a_Manager;
        m_PreviousAllocation = a_Manager.GetDefaultAllocation();
        m_CurrentAllocation = a_Manager.GetDefaultAllocation();
        m_MetallicFactor = a_Info.m_MetallicFactor;
        m_RoughnessFactor = a_Info.m_RoughnessFactor;
        m_EmissiveFactor = a_Info.m_EmissiveFactor;
        m_AlbedoFactor = a_Info.m_AlbedoFactor;
        m_Textures = a_Info.m_MaterialTextures;
    }

    glm::vec3 Material::GetAlbedoFactor() const
    {
        return m_AlbedoFactor;
    }

    void Material::SetAlbedoFactor(const glm::vec3& a_Factor)
    {
        std::lock_guard lock(m_DirtyFlagMutex);
        if(m_AlbedoFactor != a_Factor)
        {
            m_AlbedoFactor = a_Factor;
            MarkAsDirty();
        }
    }

    glm::vec3 Material::GetEmissiveFactor() const
    {
        return m_EmissiveFactor;
    }

    void Material::SetEmissiveFactor(const glm::vec3& a_Factor)
    {
        std::lock_guard lock(m_DirtyFlagMutex);
        if (m_EmissiveFactor != a_Factor)
        {
            m_EmissiveFactor = a_Factor;
            MarkAsDirty();
        }
    }

    float Material::GetMetallicFactor() const
    {
        return m_MetallicFactor;
    }

    void Material::SetMetallicFactor(const float a_Factor)
    {
        std::lock_guard lock(m_DirtyFlagMutex);
        if (m_MetallicFactor != a_Factor)
        {
            m_MetallicFactor = a_Factor;
            MarkAsDirty();
        }
    }

    float Material::GetRoughnessFactor() const
    {
        return m_RoughnessFactor;
    }

    void Material::SetRoughnessFactor(const float a_Factor)
    {
        std::lock_guard lock(m_DirtyFlagMutex);
        if (m_RoughnessFactor != a_Factor)
        {
            m_RoughnessFactor = a_Factor;
            MarkAsDirty();
        }
    }

    std::shared_ptr<EggMaterialTextures> Material::GetMaterialTextures() const
    {
        return m_Textures;
    }

    void Material::SetMaterialTextures(const std::shared_ptr<EggMaterialTextures>& a_Texture)
    {
        std::lock_guard<std::mutex> lock(m_DirtyFlagMutex);
        if (m_Textures != a_Texture)
        {
            m_Textures = a_Texture;
            MarkAsDirty();
        }
    }

    PackedMaterialData Material::PackMaterialData() const
    {
        PackedMaterialData data;

        //Pack metallic roughness into X.
        data.m_Data.x = glm::packUnorm2x16(glm::vec2(m_MetallicFactor, m_RoughnessFactor));

        //TODO: pack the texture ID into Y.
        data.m_Data.y = 69;

        //Albedo Z channel.
        data.m_Data.z = glm::packUnorm4x8(glm::vec4(m_AlbedoFactor, 0.f));

        //Emissive W channel.
        data.m_Data.w = glm::packUnorm4x8(glm::vec4(m_EmissiveFactor, 0.f));

        return data;
    }

    void Material::GetCurrentlyUsedGpuIndex(uint32_t& a_Index, uint32_t& a_LastUpdatedFrame) const
    {
        if(m_CurrentAllocation->m_Uploaded)
        {
            a_Index = m_CurrentAllocation->m_Index;
            a_LastUpdatedFrame = m_CurrentAllocation->m_UpdatedFrame;
            return;
        }
        a_Index =  m_PreviousAllocation->m_Index;
        a_LastUpdatedFrame = m_PreviousAllocation->m_UpdatedFrame;
    }

    bool Material::IsDirty() const
    {
        std::lock_guard<std::mutex> lock(m_DirtyFlagMutex);
        return m_DirtyFlag;
    }

    void Material::MarkAsDirty()
    {
        //Only mark if not already dirty.
        if (!m_DirtyFlag)
        {
            //Retrieve self-containing pointer and mark as dirty with the manager.
            m_DirtyFlag = true;
            auto ptr = shared_from_this();
            m_Manager->RegisterDirtyMaterial(ptr);
        }
    }
}
