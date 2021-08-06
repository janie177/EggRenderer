#include "api/DrawData.h"
#include "Resources.h"

namespace egg
{
    void DynamicDrawCall::Update(uint32_t a_LastMaterialUploadFrame, uint32_t a_CurrentFrameIndex)
    {
        const auto size = m_MaterialData.size();

        //Mark the mesh as in-use in case it gets deleted while in-flight.
        std::static_pointer_cast<Mesh>(m_Mesh)->m_LastUsedFrameId = a_CurrentFrameIndex;

    	//Mark the materials as used for this frame.
    	for(auto& material : m_MaterialData)
    	{
            std::static_pointer_cast<Material>(material.m_Material)->SetLastUsedFrame(a_CurrentFrameIndex);
    	}

        //When not using the default material, check for potential changes.
        if (!m_MaterialData.empty())
        {
            std::vector<uint32_t> currentMaterialIds;
            std::vector<bool> needsUpdate;
            currentMaterialIds.reserve(size);
            needsUpdate.reserve(size);    //Contains true if the material needs an update.

            for (auto& materialData : m_MaterialData)
            {
                //To ensure that the material does not go out of scope, mark it as used.
                auto mat = std::static_pointer_cast<Material>(materialData.m_Material);
                mat->m_LastUsedFrameId = a_CurrentFrameIndex;

                //Check if the material is up to date for this draw call.
                uint32_t lastUpdated;
                uint32_t index;
                mat->GetCurrentlyUsedGpuIndex(index, lastUpdated);
                currentMaterialIds.push_back(index);
                needsUpdate.push_back((lastUpdated > materialData.m_LastUpdatedFrameId));
            }

            //Update the texture IDs in the packed draw call data.
            for (uint32_t matId = 0; matId < static_cast<uint32_t>(currentMaterialIds.size()); ++matId)
            {
                //Update the instance data for this material if it changed.
                if (needsUpdate[matId])
                {
                    for (uint32_t i = 0; i < static_cast<uint32_t>(m_InstanceData.size()); ++i)
                    {
                        const auto index = m_MaterialIndices[i];
                        if (index == matId)
                        {
                            *reinterpret_cast<uint32_t*>(&(m_InstanceData[i].m_Transform[0][3])) = currentMaterialIds[index];
                        }
                    }

                    //Mark the material in this draw call as updated for this specific frame.
                    //The frame index corresponds to a frame when an upload was performed.
                    //This effectively allows me to see when a material is outdated.
                    m_MaterialData[matId].m_LastUpdatedFrameId = a_LastMaterialUploadFrame;
                }
            }
        }
    }

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
