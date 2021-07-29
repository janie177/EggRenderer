#include "MaterialManager.h"

#include "Renderer.h"

namespace egg
{
    bool MaterialManager::Init(const RenderData& a_RenderData)
    {
        if(m_Initialized)
        {
            printf("Trying to init material manager, but was already initialized.\n");
            return false;
        }
        m_MaxMaterials = a_RenderData.m_Settings.maxNumMaterials;
        m_IndexCounter = 0;

        //TODO init all stuff.


        //Set state to initialized. This happens before creating the default allocation because it requires this flag.
        m_Initialized = true;

        //Allocate the default memory (will be index 0).
        m_DefaultAllocation = AllocateMaterialMemory();

        //TODO upload default allocation.


        return true;
    }

    void MaterialManager::UploadData()
    {
        //Only one upload can happen at a time.
        std::lock_guard<std::mutex> lock(m_UploadOperationMutex);

        {
            //Swap the vectors while the mutex is locked.
            //Keep locked because m_DirtyMaterials is accessed below to add back some materials potentially.
            std::lock_guard<std::mutex> lock(m_DirtyMaterialMutex);
            std::vector<std::shared_ptr<Material>> dirtyMaterialVector;
            dirtyMaterialVector.swap(m_DirtyMaterials);
           
            //Mark all the dirty materials for upload.
            std::vector<std::pair<std::shared_ptr<MaterialMemoryData>, PackedMaterialData>> toUploadData;
            for (auto& dirtyMaterial : dirtyMaterialVector)
            {
                //If the current material does not have pending uploads, queue it for uploading.
                if (dirtyMaterial->m_CurrentAllocation->m_Uploaded)
                {
                    toUploadData.emplace_back(std::make_pair(dirtyMaterial->m_CurrentAllocation, dirtyMaterial->PackMaterialData()));
                }
                //The material is already waiting for an upload. If it's dirty, add it back for the next iteration.
                else if(dirtyMaterial->IsDirty())
                {
                    m_DirtyMaterials.push_back(dirtyMaterial);
                }
            }
        }


        //TODO upload all data in the copy.
        //TODO then set the bool uploaded to true when done. Wait for fence etc.

    }

    std::shared_ptr<Material> MaterialManager::CreateMaterial(const MaterialCreateInfo& a_CreateInfo)
    {
        std::shared_ptr<Material> ptr = std::make_shared<Material>(a_CreateInfo, *this);
        ptr->MarkAsDirty();
        return ptr;
    }

    std::shared_ptr<MaterialMemoryData> MaterialManager::AllocateMaterialMemory()
    {
        if (!m_Initialized)
        {
            printf("Error! Could not allocate material memory because the material manager was not initialized.\n");
            return nullptr;
        }
        //Lock from external access.
        std::lock_guard<std::mutex> lock(m_AllocationMutex);

        uint32_t index = 0;

        //Some freed memory is available, so recycle that.
        if(!m_FreedIndices.empty())
        {
            index = m_FreedIndices.front();
            m_FreedIndices.pop();
        }
        else if(m_IndexCounter < m_MaxMaterials)
        {
            index = m_IndexCounter++;
        }
        else
        {
            printf("No more space to allocate new materials in material manager!\n");
            return nullptr;
        }

        std::shared_ptr<MaterialMemoryData> ptr = std::make_shared<MaterialMemoryData>();
        ptr->m_Index = index;
        ptr->m_LastUsedFrame = 0;
        ptr->m_Uploaded = false;
        m_Data.Add(ptr);
        return ptr;
    }

    std::shared_ptr<MaterialMemoryData> MaterialManager::GetDefaultAllocation() const
    {
        return m_DefaultAllocation;
    }

    void MaterialManager::RemoveUnused(const uint32_t a_CurrentFrameIndex, const uint32_t a_SwapChainCount)
    {
        std::lock_guard<std::mutex> lock(m_AllocationMutex);
        m_Data.RemoveUnused([&](MaterialMemoryData& a_Data)
            {
                //Ensure that an index is not in flight anymore before potentially re-using it.
                if(a_Data.m_LastUsedFrame + a_SwapChainCount < a_CurrentFrameIndex)
                {
                    m_FreedIndices.push(a_Data.m_Index);    //Add the index to the list of free ones.
                    return true;
                }
                return false;
            });
    }

    void MaterialManager::RegisterDirtyMaterial(std::shared_ptr<Material>& a_Material)
    {
        std::lock_guard<std::mutex> lock(m_DirtyMaterialMutex);
        m_DirtyMaterials.emplace_back(a_Material);
    }

    void MaterialManager::CleanUp(const RenderData& a_RenderData)
    {
        if(!m_Initialized)
        {
            printf("Cannot clean up Material Manager that was not initialized.\n");
            return;
        }

        m_Data.RemoveAll([](MaterialMemoryData& a_Data)
            {
                return true;
            });
        std::queue<uint32_t>().swap(m_FreedIndices);    //Cleans the queue.
        m_DirtyMaterials.clear();

        //TODO clean up other things.


        m_Initialized = false;
    }
}
