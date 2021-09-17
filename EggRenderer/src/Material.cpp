#include "Resources.h"

namespace egg
{
    Material::Material(const MaterialCreateInfo& a_Info)
    {
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
        m_AlbedoFactor = a_Factor;
    }

    glm::vec3 Material::GetEmissiveFactor() const
    {
        return m_EmissiveFactor;
    }

    void Material::SetEmissiveFactor(const glm::vec3& a_Factor)
    {
        m_EmissiveFactor = a_Factor;
    }

    float Material::GetMetallicFactor() const
    {
        return m_MetallicFactor;
    }

    void Material::SetMetallicFactor(const float a_Factor)
    {
        m_MetallicFactor = a_Factor;
    }

    float Material::GetRoughnessFactor() const
    {
        return m_RoughnessFactor;
    }

    void Material::SetRoughnessFactor(const float a_Factor)
    {
        m_RoughnessFactor = a_Factor;
    }

    std::shared_ptr<EggMaterialTextures> Material::GetMaterialTextures() const
    {
        return m_Textures;
    }

    void Material::SetMaterialTextures(const std::shared_ptr<EggMaterialTextures>& a_Texture)
    {
        m_Textures = a_Texture;
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
}
