#pragma once
#include <cinttypes>
#include <vulkan/vulkan.h>
#include <RenderUtility.h>

#include "HandleRecycler.h"

namespace egg
{
    /*
     * Settings to initialize the bindless descriptor heap with.
     */
    struct BindlessSettings
    {
        uint32_t m_NumSrvSlots = 0;
        uint32_t m_NumUavSlots = 0;
        uint32_t m_NumCbvSlots = 0;
    };

    /*
     * The types of descriptor that exist in the heap.
     */
    enum class DescriptorType
    {
        SRV,    //Used to access read only textures.
        UAV,    //Used to access read-write textures.
        CBV     //Used to access constant buffers.
    };

    /*
     * A handle for a descriptor.
     */
    class BindlessHandle
    {
        friend class Bindless;
    private:
        DescriptorType m_Type;
        uint32_t m_Index;
    };

    /*
     * Bindless is the system that allocates all descriptors for SRV/UAV/CBS resources.
     * These can then be written to and accessed in the shader.
     */
    class Bindless
    {
    public:
        Bindless();

        /*
         * Initialize the bindless system.
         */
        bool Init(VkDevice& a_Device, BindlessSettings& a_Settings);

        /*
         * Clean up the bindless system.
         */
        bool CleanUp(VkDevice& a_Device);

        /*
         * Retrieve a handle to a descriptor of the given type if available.
         * Stores the handle in a_Handle.
         *
         * Returns true if a new handle could be allocated, false otherwise.
         */
        bool CreateDescriptor(const DescriptorType a_Type, BindlessHandle& a_Handle);

        /*
         * Free a descriptor handle so that it can be recycled immediately.
         */
        void FreeDescriptor(const BindlessHandle& a_Handle);

        /*
         * Get the handle to the internal descriptor set.
         */
        VkDescriptorSet GetDescriptorSet() const;

        /*
         * Get the handle to the internal descriptor set layout.
         */
        VkDescriptorSetLayout GetDescriptorSetLayout() const;

    private:
        //Descriptor set, pool and descriptors.
        bool m_Initialized;
        DescriptorSetContainer m_DescriptorContainer;
        HandleRecycler<uint32_t> m_SrvHandles;
        HandleRecycler<uint32_t> m_UavHandles;
        HandleRecycler<uint32_t> m_CbvHandles;
        BindlessSettings m_Settings;
    };
}
