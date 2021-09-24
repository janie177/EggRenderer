#include "Bindless.h"

#include <cassert>

namespace egg
{
    Bindless::Bindless() : m_Initialized(false)
    {
    }

    bool Bindless::Init(VkDevice& a_Device, BindlessSettings& a_Settings)
    {
        m_Settings = a_Settings;
        const auto flags = VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        auto info = DescriptorSetContainerCreateInfo::Create(1)
            .AddBinding(0, a_Settings.m_NumSrvSlots, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, flags)
            .AddBinding(1, a_Settings.m_NumUavSlots, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL, flags)
            .AddBinding(2, a_Settings.m_NumCbvSlots, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL, flags);

        return (m_Initialized = RenderUtility::CreateDescriptorSetContainer(a_Device, info, m_DescriptorContainer));
    }

    bool Bindless::CleanUp(VkDevice& a_Device)
    {
        RenderUtility::DestroyDescriptorSetContainer(a_Device, m_DescriptorContainer);
        return true;
    }

    bool Bindless::CreateDescriptor(const DescriptorType a_Type, BindlessHandle& a_Handle)
    {
        assert(m_Initialized);
        uint32_t handle = 0;
        uint32_t maximum = 0;
        switch(a_Type)
        {
        case DescriptorType::SRV: 
        {
            handle = m_SrvHandles.GetHandle();
            maximum = m_Settings.m_NumSrvSlots;
        }
            break;
        case DescriptorType::UAV: 
        {
            handle = m_UavHandles.GetHandle();
            maximum = m_Settings.m_NumUavSlots;
        }
            break;
        case DescriptorType::CBV: 
        {
            handle = m_CbvHandles.GetHandle();
            maximum = m_Settings.m_NumCbvSlots;
        }
            break;
        }

        //Write the values.
        a_Handle.m_Index = handle;
        a_Handle.m_Type = a_Type;

        //Only return true when in range.
        return handle < maximum;
    }

    void Bindless::FreeDescriptor(const BindlessHandle& a_Handle)
    {
        assert(m_Initialized);
        switch (a_Handle.m_Type)
        {
        case DescriptorType::SRV: m_SrvHandles.Recycle(a_Handle.m_Index);
        break;
        case DescriptorType::UAV: m_UavHandles.Recycle(a_Handle.m_Index);
            break;
        case DescriptorType::CBV: m_CbvHandles.Recycle(a_Handle.m_Index);
            break;
        }
    }

    VkDescriptorSet Bindless::GetDescriptorSet() const
    {
        assert(m_Initialized);
        return m_DescriptorContainer.m_Sets[0];
    }

    VkDescriptorSetLayout Bindless::GetDescriptorSetLayout() const
    {
        assert(m_Initialized);
        return m_DescriptorContainer.m_Layout;
    }
}
