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
        m_LastUpdateFrameIndex = 0;

        //Allocate the staging buffer and GPU memory.
        VmaAllocationCreateInfo allocateInfo{};
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(PackedMaterialData) * a_RenderData.m_Settings.maxNumMaterials;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        allocateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if(vmaCreateBufferWithAlignment(a_RenderData.m_Allocator, &bufferInfo, &allocateInfo,
            16, &m_MaterialBuffer, &m_MaterialBufferAllocation, nullptr) != VK_SUCCESS)
        {
            printf("Could not allocate memory in material manager.\n");
            return false;
        }

        bufferInfo.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        allocateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        if (vmaCreateBufferWithAlignment(a_RenderData.m_Allocator, &bufferInfo, &allocateInfo,
            16, &m_MaterialStagingBuffer, &m_MaterialStagingBufferAllocation, nullptr) != VK_SUCCESS)
        {
            printf("Could not allocate memory in material manager.\n");
            return false;
        }

        vmaGetAllocationInfo(a_RenderData.m_Allocator, m_MaterialStagingBufferAllocation, &m_MaterialStagingBufferAllocationInfo);
        vmaGetAllocationInfo(a_RenderData.m_Allocator, m_MaterialBufferAllocation, &m_MaterialBufferAllocationInfo);

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
        if(vkCreateFence(a_RenderData.m_Device, &fenceCreateInfo, nullptr, &m_MaterialUploadFence) != VK_SUCCESS)
        {
            printf("Could not create fence in material manager!\n");
            return false;
        }

        m_UploadQueue = &((!a_RenderData.m_TransferQueues.empty()) ? a_RenderData.m_TransferQueues[0] : a_RenderData.m_GraphicsQueues[a_RenderData.m_GraphicsQueues.size() - 1]);


        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = m_UploadQueue->m_FamilyIndex;

        if(vkCreateCommandPool(a_RenderData.m_Device, &commandPoolCreateInfo, nullptr, &m_UploadCommandPool) != VK_SUCCESS)
        {
            printf("Could not create command pool in material manager!\n");
            return false;
        }

        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandBufferCount = 1;
        commandBufferAllocateInfo.commandPool = m_UploadCommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        if (vkAllocateCommandBuffers(a_RenderData.m_Device, &commandBufferAllocateInfo, &m_UploadCommandBuffer) != VK_SUCCESS)
        {
            printf("Could not create command buffer in material manager!\n");
            return false;
        }

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorSetLayoutBinding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        if (vkCreateDescriptorSetLayout(a_RenderData.m_Device, &descriptorSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            printf("Could not create descriptor set layout in material manager!\n");
            return false;
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.descriptorCount = 1;
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.maxSets = 1;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &poolSize;

        if (vkCreateDescriptorPool(a_RenderData.m_Device, &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            printf("Could not create descriptor pool in material manager!\n");
            return false;
        }

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.descriptorPool = m_DescriptorPool;
        descriptorSetAllocateInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(a_RenderData.m_Device, &descriptorSetAllocateInfo, &m_DescriptorSet) != VK_SUCCESS)
        {
            printf("Could not create descriptor set in material manager!\n");
            return false;
        }


        //Set state to initialized. This happens before creating the default allocation because it requires this flag.
        m_Initialized = true;

        //Allocate the default memory (will be index 0).
        m_DefaultAllocation = AllocateMaterialMemory();

        //Upload the default allocation.
        PackedMaterialData defaultData{};
        defaultData.m_AlbedoFactor.m_Data = std::numeric_limits<uint32_t>::max();
        defaultData.m_EmissiveFactor.m_Data = 0.f;
        defaultData.m_MetallicFactor = 0;
        defaultData.m_RoughnessFactor = std::numeric_limits<uint16_t>::max();
        defaultData.m_TexturesIndex = 0;
        m_ToUploadData.push_back(std::make_pair(m_DefaultAllocation, defaultData));

        //This stalls the thread till done.
        UploadData(a_RenderData, 0);

        //Create a descriptor for the buffer.
        VkDescriptorBufferInfo bufferWriteInfo{};
        bufferWriteInfo.offset = 0;
        bufferWriteInfo.range = VK_WHOLE_SIZE;
        bufferWriteInfo.buffer = m_MaterialBuffer;

        VkWriteDescriptorSet writeDescriptoSet{};
        writeDescriptoSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptoSet.descriptorCount = 1;
        writeDescriptoSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptoSet.dstArrayElement = 0;
        writeDescriptoSet.dstBinding = 0;
        writeDescriptoSet.dstSet = m_DescriptorSet;
        writeDescriptoSet.pBufferInfo = &bufferWriteInfo;

        vkUpdateDescriptorSets(a_RenderData.m_Device, 1, &writeDescriptoSet, 0, nullptr);

        return true;
    }

    void MaterialManager::PrepareForUpload()
    {
        //Since Upload() also uses the same data, ensure that it's not running from before or something like that.
        std::lock_guard<std::mutex> lock(m_UploadOperationMutex);

        {
            //Swap the vectors while the mutex is locked.
            //Keep locked because m_DirtyMaterials is accessed below to add back some materials potentially.
            std::lock_guard<std::mutex> lock2(m_DirtyMaterialMutex);
            std::vector<std::shared_ptr<Material>> dirtyMaterialVector;
            dirtyMaterialVector.swap(m_DirtyMaterials);

            //Mark all the dirty materials for upload.
            for (auto& dirtyMaterial : dirtyMaterialVector)
            {
                //If the current material does not have pending uploads, queue it for uploading.
                if (dirtyMaterial->m_CurrentAllocation->m_Uploaded)
                {
                    //Ensure that the material is not edited while uploading.
                    std::lock_guard lock3(dirtyMaterial->m_DirtyFlagMutex);

                	//Move the current used material index to the back, so that it is used till the upload finishes.
                	//Allocate a new place for the updated data that is used as soon as the upload finishes.
                    dirtyMaterial->m_PreviousAllocation = dirtyMaterial->m_CurrentAllocation;
                    dirtyMaterial->m_CurrentAllocation = AllocateMaterialMemory();

                	//Tightly pack the material data and upload it to the newly allocated memory in the material buffer.
                    m_ToUploadData.emplace_back(std::make_pair(dirtyMaterial->m_CurrentAllocation, dirtyMaterial->PackMaterialData()));
                    dirtyMaterial->m_DirtyFlag = false; //Reset the flag after copying the latest data.
                }
                //The material is already waiting for an upload. If it's dirty, add it back for the next iteration.
                else
                {
                    m_DirtyMaterials.push_back(dirtyMaterial);
                }
            }
        }
    }

    void MaterialManager::UploadData(const RenderData& a_RenderData, const uint32_t a_FrameIndex)
    {
        //Only one upload can happen at a time.
        std::lock_guard<std::mutex> lock(m_UploadOperationMutex);

        //Only do something if there's actually stuff to upload.
        if(!m_ToUploadData.empty())
        {

            //Map the memory and copy into the staging buffer.
            std::vector<VkBufferCopy> copies;
            void* data;
            vkMapMemory(a_RenderData.m_Device, m_MaterialStagingBufferAllocationInfo.deviceMemory, m_MaterialStagingBufferAllocationInfo.offset, VK_WHOLE_SIZE, 0, &data);
            int index = 0;
            for(auto& entry : m_ToUploadData)
            {
                auto bufferOffset = static_cast<uintptr_t>(sizeof(PackedMaterialData) * index);
                PackedMaterialData * start = reinterpret_cast<PackedMaterialData*>(reinterpret_cast<uintptr_t>(data) + bufferOffset);
                memcpy(start, &entry.second, sizeof(PackedMaterialData));
                copies.emplace_back(VkBufferCopy{bufferOffset, entry.first->m_Index * sizeof(PackedMaterialData),sizeof(PackedMaterialData)});
                ++index;
            }
            vkUnmapMemory(a_RenderData.m_Device, m_MaterialStagingBufferAllocationInfo.deviceMemory);

            //First reset the command pool. This automatically resets all associated buffers as well (if flag was not individual)
            vkResetCommandPool(a_RenderData.m_Device, m_UploadCommandPool, 0);

            VkCommandBufferBeginInfo beginInfo;
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;
            beginInfo.pNext = nullptr;

            if (vkBeginCommandBuffer(m_UploadCommandBuffer, &beginInfo) != VK_SUCCESS)
            {
                printf("ERROR: Could not begin recording copy command buffer for material upload!\n");
                return;
            }

            //Specify which data to copy where.
            vkCmdCopyBuffer(m_UploadCommandBuffer, m_MaterialStagingBuffer, m_MaterialBuffer, copies.size(), copies.data());

            //Stop recording.
            vkEndCommandBuffer(m_UploadCommandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_UploadCommandBuffer;
            submitInfo.pNext = nullptr;
            submitInfo.pSignalSemaphores = nullptr;
            submitInfo.pWaitDstStageMask = nullptr;
            submitInfo.pWaitSemaphores = nullptr;
            submitInfo.waitSemaphoreCount = 0;
            submitInfo.signalSemaphoreCount = 0;

            //Submit the work and then wait for the fence to be signaled.
            vkResetFences(a_RenderData.m_Device, 1, &m_MaterialUploadFence);
            vkQueueSubmit(m_UploadQueue->m_Queue, 1, &submitInfo, m_MaterialUploadFence);
            vkWaitForFences(a_RenderData.m_Device, 1, &m_MaterialUploadFence, true, std::numeric_limits<uint64_t>::max());

            //Update the to-upload data to now be set to uploaded.
            //This will allow new data to be uploaded.
            for(auto& entry : m_ToUploadData)
            {
                entry.first->m_LastUsedFrame = a_FrameIndex;
                entry.first->m_UpdatedFrame = a_FrameIndex;
                entry.first->m_Uploaded = true;
            }

            //When everything is done set the frame index.
            //This can be used to see if a material is outdated.
            std::lock_guard<std::mutex> frameCountLock(m_LastUpdateFrameMutex);
            m_LastUpdateFrameIndex = a_FrameIndex;

            //Clear the vector when done.
            m_ToUploadData.clear();
        }
    }

    void MaterialManager::WaitForIdle(const RenderData& a_RenderData) const
    {
        vkWaitForFences(a_RenderData.m_Device, 1, &m_MaterialUploadFence, true, std::numeric_limits<uint64_t>::max());
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
        ptr->m_UpdatedFrame = 0;
        m_Data.Add(ptr);
        return ptr;
    }

    std::shared_ptr<MaterialMemoryData> MaterialManager::GetDefaultAllocation() const
    {
        return m_DefaultAllocation;
    }

    uint32_t MaterialManager::GetLastUpdatedFrame()
    {
        std::lock_guard<std::mutex> lock(m_LastUpdateFrameMutex);
        return m_LastUpdateFrameIndex;
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

        //Free all the vulkan objects.
        vmaDestroyBuffer(a_RenderData.m_Allocator, m_MaterialBuffer, m_MaterialBufferAllocation);
        vmaDestroyBuffer(a_RenderData.m_Allocator, m_MaterialStagingBuffer, m_MaterialStagingBufferAllocation);
        vkDestroyFence(a_RenderData.m_Device, m_MaterialUploadFence, nullptr);
        vkDestroyCommandPool(a_RenderData.m_Device, m_UploadCommandPool, nullptr);
        vkDestroyDescriptorPool(a_RenderData.m_Device, m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(a_RenderData.m_Device, m_DescriptorSetLayout, nullptr);

        m_Initialized = false;
    }

    VkDescriptorSetLayout MaterialManager::GetSetLayout() const
    {
        assert(m_Initialized);
        return m_DescriptorSetLayout;
    }

    VkDescriptorSet MaterialManager::GetDescriptorSet() const
    {
        assert(m_Initialized);
        return m_DescriptorSet;
    }
}
