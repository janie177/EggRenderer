#pragma once
#include <memory>
#include <glm/glm/glm.hpp>

namespace egg
{
    /*
     * Information used to create a set of textures on the GPU.
     * The created texture can then be set for a material.
     */
    struct MaterialTexturesCreateInfo
    {
        //TODO pass texture data, maybe optional scaling factor and the format.
    };

    /*
     * Handle to a set of textures on the GPU used for one or more materials.
     */
    class EggMaterialTextures
    {
    public:
        virtual ~EggMaterialTextures() = default;
    };

    /*
     * Information to create a new material.
     */
    struct MaterialCreateInfo
    {
        //The albedo factor to use for this texture.
        glm::vec3 m_AlbedoFactor = glm::vec3(1.f, 1.f, 1.f);

        //How much light this material emits in the R, G and B color channels.
        glm::vec3 m_EmissiveFactor = glm::vec3(0.f, 0.f, 0.f);

        //Metallic scaling factor for the material.
        float m_MetallicFactor = 1.f;

        //Roughness scaling factor for the material.
        float m_RoughnessFactor = 1.f;

        //The textures to use for this material.
        //If not set, default textures will be used.
        std::shared_ptr<EggMaterialTextures> m_MaterialTextures;
    };

    /*
     * Materials can be applied to meshes.
     * They contain a few tweak-able constant values, along with a combined texture.
     * This combined texture tightly packs all shading-required data.
     */
    class EggMaterial
    {
    public:
        virtual ~EggMaterial() = default;

        /*
         * Get the albedo color factor for this material.
         */
        virtual glm::vec3 GetAlbedoFactor() const = 0;

        /*
         * Set the albedo color factor of the material.
         * This is multiplied with the albedo texture color.
         */
        virtual void SetAlbedoFactor(const glm::vec3& a_Factor) = 0;

        /*
         * Fet the emissive scaling factor.
         */
        virtual glm::vec3 GetEmissiveFactor() const = 0;

        /*
         * Set the emissive factor for this material.
         * This factor is multiplied with the emissive texture of the material.
         */
        virtual void SetEmissiveFactor(const glm::vec3& a_Factor) = 0;

        /*
         * Get the metallic scaling factor for this material.
         */
        virtual float GetMetallicFactor() const = 0;

        /*
         * Set the metallic scaling factor for this material.
         * This factor is multiplied with the material texture.
         */
        virtual void SetMetallicFactor(const float a_Factor) = 0;

        /*
         * Get the roughness scaling factor.
         */
        virtual float GetRoughnessFactor() const = 0;

        /*
         * Set the roughness scaling factor.
         * This is multiplied with the texture value before being applied.
         */
        virtual void SetRoughnessFactor(const float a_Factor) = 0;

        /*
         * Get the textures used by this material.
         */
        virtual std::shared_ptr<EggMaterialTextures> GetMaterialTextures() const = 0;

        /*
         * Set the textures used by this material.
         */
        virtual void SetMaterialTextures(const std::shared_ptr<EggMaterialTextures>& a_Texture) = 0;
    };
}
