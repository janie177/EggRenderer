#include "api/EggLight.h"

#include <cassert>

namespace egg
{
    DirectionalLight::DirectionalLight() : m_Direction{ 0.f, -1.f, 0.f }, m_Radiance{ 1.f, 1.f, 1.f }
    {

    }

    void DirectionalLight::SetDirection(float a_X, float a_Y, float a_Z)
    {
        //Ensure direction is normalized.
        assert(fabsf(sqrtf((a_X * a_X) + (a_Y * a_Y) + (a_Z * a_Z)) - 1.f) < 0.0001f && "Direction must be normalized!");
        m_Direction[0] = a_X;
        m_Direction[1] = a_Y;
        m_Direction[2] = a_Z;
    }

    void DirectionalLight::SetRadiance(float a_R, float a_G, float a_B)
    {
        m_Radiance[0] = a_R;
        m_Radiance[1] = a_G;
        m_Radiance[2] = a_B;
    }

    void DirectionalLight::GetDirection(float& a_X, float& a_Y, float& a_Z) const
    {
        a_X = m_Direction[0];
        a_Y = m_Direction[1];
        a_Z = m_Direction[2];
    }

    void DirectionalLight::GetRadiance(float& a_R, float& a_G, float& a_B) const
    {
        a_R = m_Radiance[0];
        a_G = m_Radiance[1];
        a_B = m_Radiance[2];
    }

    SphereLight::SphereLight() : m_Position{ 0.f, 0.f, 0.f }, m_Radiance{ 1.f, 1.f, 1.f }, m_Radius(1.f)
    {
    }

    void SphereLight::SetPosition(float a_X, float a_Y, float a_Z)
    {
        m_Position[0] = a_X;
        m_Position[1] = a_Y;
        m_Position[2] = a_Z;
    }

    void SphereLight::SetRadiance(float a_R, float a_G, float a_B)
    {
        m_Radiance[0] = a_R;
        m_Radiance[1] = a_G;
        m_Radiance[2] = a_B;
    }

    void SphereLight::SetRadius(float a_Radius)
    {
    }

    void SphereLight::GetPosition(float& a_X, float& a_Y, float& a_Z) const
    {
        a_X = m_Position[0];
        a_Y = m_Position[1];
        a_Z = m_Position[2];
    }

    void SphereLight::GetRadiance(float& a_R, float& a_G, float& a_B) const
    {
        a_R = m_Radiance[0];
        a_G = m_Radiance[1];
        a_B = m_Radiance[2];
    }

    void SphereLight::GetRadius(float& a_Radius) const
    {
        a_Radius = m_Radius;
    }
}
