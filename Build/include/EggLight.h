#pragma once
#include <cstdint>

#include "Math.h"

namespace egg
{
    //The available light types.
    enum class LightType : uint32_t
    {
        DIRECTIONAL = 0,
        AREA
    };

    /*
     * A light without position or drop-off.
     */
    class DirectionalLight
    {
        friend class DrawData;
    public:
        DirectionalLight();

        void SetDirection(float a_X, float a_Y, float a_Z);
        void SetRadiance(float a_R, float a_G, float a_B);

        void GetDirection(float& a_X, float& a_Y, float& a_Z) const;
        void GetRadiance(float& a_R, float& a_G, float& a_B) const;

    private:
        float m_Direction[3];
        float m_Radiance[3];
    };

    /*
     * A light that has a position in the world.
     */
    class SphereLight
    {
        friend class DrawData;
    public:
        SphereLight();

        void SetPosition(float a_X, float a_Y, float a_Z);
        void SetRadiance(float a_R, float a_G, float a_B);
        void SetRadius(float a_Radius);

        void GetPosition(float& a_X, float& a_Y, float& a_Z) const;
        void GetRadiance(float& a_R, float& a_G, float& a_B) const;
        void GetRadius(float& a_Radius) const;

    private:
        float m_Position[3];
        float m_Radiance[3];
        float m_Radius;
    };
}