#pragma once
#include <queue>
#include <glm/glm/glm.hpp>

#include "Resources.h"
#include "ConcurrentRegistry.h"

namespace egg
{
    class Renderer;
    struct RenderData;
    struct QueueInfo;

    /*
     * Book-keeping information.
     */
    struct MaterialMemoryData
    {
        friend class MaterialManager;
        friend class Renderer;
        friend class Material;
    private:
        uint32_t m_Index;           //The index into the buffer where this material data is stored.
        uint32_t m_LastUsedFrame;   //The frame when this material was last used.
        uint32_t m_UpdatedFrame;    //The frame when this material allocation was uploaded.
        bool m_Uploaded;            //Bool set to true once data has finished uploading.
    };

    class MaterialManager
    {
    public:
        MaterialManager() : m_IndexCounter(0), m_MaxMaterials(0), m_Initialized(false), m_LastUpdateFrameIndex(0) {}

        /*
         * Set up internal systems and allocate memory.
         * Returns true if all went well.
         */
        bool Init(const RenderData& a_RenderData);

        /*
         * Process all queued for upload data.
         * This will stall the thread it is called from untill all data is on the GPU.
         */
        void UploadData(const uint32_t a_FrameIndex);

        /*
         * Create a new material.
         */
        std::shared_ptr<Material> CreateMaterial(const MaterialCreateInfo& a_CreateInfo);

        /*
         * Allocate some memory for a new material.
         * Returns a handle to the new memory.
         */
        std::shared_ptr<MaterialMemoryData> AllocateMaterialMemory();

        /*
         * Get the default fallback allocation that is always valid.
         */
        std::shared_ptr<MaterialMemoryData> GetDefaultAllocation() const;

        /*
         * Get the frame index when materials were last updated.
         */
        uint32_t GetLastUpdatedFrame();

        /*
         * Remove materials that are no longer referenced anywhere.
         */
        void RemoveUnused(const uint32_t a_CurrentFrameIndex, const uint32_t a_SwapChainCount);

        /*
         * Mark a material as dirty so that it will be updated before the next frame.
         */
        void RegisterDirtyMaterial(std::shared_ptr<Material>& a_Material);

        /*
         * Destroy all allocated memory.
         */
        void CleanUp(const RenderData& a_RenderData);

    private:
        ConcurrentRegistry<MaterialMemoryData> m_Data;  //Contains all the taken indices so far.
        std::queue<uint32_t> m_FreedIndices;            //Any index that has been freed is added here for re-use.

        std::mutex m_AllocationMutex;                   //Just in case another thread allocates new materials (async mesh loading).
        uint32_t m_IndexCounter;                        //Increments when allocating a new material.
        uint32_t m_MaxMaterials;                        //Maximum amount of materials.
        bool m_Initialized;                             //Set to true if Init() has been called.

        std::shared_ptr<MaterialMemoryData> m_DefaultAllocation;  //Default allocation to be used when still uploading.

        std::mutex m_UploadOperationMutex;  //Separate mutex to actually write to the GPU Buffer to ensure only one upload happens at a time.

        //Materials that are marked as dirty.
        std::mutex m_DirtyMaterialMutex;
        std::vector<std::shared_ptr<Material>> m_DirtyMaterials;
        std::vector<std::pair<std::shared_ptr<MaterialMemoryData>, PackedMaterialData>> m_ToUploadData;

        //The last time materials were uploaded.
        std::mutex m_LastUpdateFrameMutex;
        uint32_t m_LastUpdateFrameIndex;

        /*
         * Vulkan objects.
         */
        VkBuffer m_MaterialBuffer;
        VmaAllocation m_MaterialBufferAllocation;
        VkBuffer m_MaterialStagingBuffer;
        VmaAllocation m_MaterialStagingBufferAllocation;
        VkFence m_MaterialUploadFence;
        VkCommandPool m_UploadCommandPool;
        VkCommandBuffer m_UploadCommandBuffer;
        QueueInfo* m_UploadQueue;
        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkDescriptorPool m_DescriptorPool;
        VkDescriptorSet m_DescriptorSet;
    };
}
