#include "Resources.h"
#include "MaterialManager.h"

namespace egg
{
    Material::Material(const MaterialCreateInfo& a_Info, MaterialManager& a_Manager) : m_DirtyFlag(false)
    {
        m_Manager = &a_Manager;
        m_PreviousAllocation = a_Manager.GetDefaultAllocation();
        m_CurrentAllocation = a_Manager.AllocateMaterialMemory();
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
        if (m_Textures != a_Texture)
        {
            m_Textures = a_Texture;
            MarkAsDirty();
        }
    }

    PackedMaterialData Material::PackMaterialData() const
    {
        PackedMaterialData data;

        //TODO extract the texture index
        data.m_TexturesIndex = 69;

        constexpr uint32_t byteMax = std::numeric_limits<uint8_t>::max();
        constexpr uint32_t shortMax = std::numeric_limits<uint16_t>::max();

        data.m_AlbedoFactor.m_X = m_AlbedoFactor.x * byteMax;
        data.m_AlbedoFactor.m_Y = m_AlbedoFactor.y * byteMax;
        data.m_AlbedoFactor.m_Z = m_AlbedoFactor.z * byteMax;

        data.m_EmissiveFactor.m_X = m_EmissiveFactor.x * byteMax;
        data.m_EmissiveFactor.m_Y = m_EmissiveFactor.y * byteMax;
        data.m_EmissiveFactor.m_Z = m_EmissiveFactor.z * byteMax;

        data.m_RoughnessFactor = m_RoughnessFactor * shortMax;
        data.m_MetallicFactor = m_MetallicFactor * shortMax;

        return data;
    }

    uint32_t Material::GetCurrentlyUsedGpuIndex() const
    {
        if(m_CurrentAllocation->m_Uploaded)
        {
            return m_CurrentAllocation->m_Index;
        }
        return m_PreviousAllocation->m_Index;
    }

    bool Material::IsDirty() const
    {
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
