#pragma once
#include <cstdint>

namespace egg
{
    /*
     * Format types for the renderer.
     * Formats underlying value is equal to their vulkan equivalent.
     */
    enum class TextureFormat
    {
        FORMAT_R8_G8_B8_SRGB = 50,		//Non linear RGB format (used for surfaces etc).
        FORMAT_R8_G8_B8_A8_UNORM = 37,	//Linear RGBA format.
        FORMAT_DEPTH_32 = 126			//32 Bit depth format.
    };

    /*
     * Information required to create a texture.
     */
    struct TextureCreateInfo
    {
        TextureFormat m_Format = TextureFormat::FORMAT_R8_G8_B8_A8_UNORM;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        void* m_Data = nullptr;
    };

    /*
     * A texture loaded into GPU memory.
     */
    class EggTexture
    {
    public:
        virtual ~EggTexture() = default;
    };
}
