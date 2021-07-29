#include "api/DrawData.h"
#include "Resources.h"

namespace egg
{
    DrawData& DrawData::SetCamera(const Camera& a_Camera)
    {
        m_Camera = a_Camera;
        return *this;
    }

    DrawData& DrawData::AddDrawCall(const DynamicDrawCall& a_DrawCall)
    {
        m_DynamicDrawCalls.push_back(a_DrawCall);
        return *this;
    }

    void DrawData::Reset()
    {
        m_Camera = Camera();
        m_DynamicDrawCalls.clear();
    }
}
