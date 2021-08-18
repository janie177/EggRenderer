#include "GpuBuffer.h"

#include <cstdio>

namespace egg
{
	bool GpuBuffer::Resize(const GpuBufferSettings& a_Settings, VmaAllocator& a_Allocator)
	{
		//First clean up existing stuff.
		CleanUp(a_Allocator);

		//Overwrite settings object.
		m_Settings = a_Settings;

		if(m_Settings.m_SizeInBytes > 0)
		{
			VkBufferCreateInfo bufferCreateInfo{};
			VmaAllocationCreateInfo allocationCreateInfo{};

			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = m_Settings.m_SizeInBytes;
			bufferCreateInfo.usage = m_Settings.m_BufferUsageFlags;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			allocationCreateInfo.usage = m_Settings.m_MemoryUsage;

			//Allocate new memory.
			if (vmaCreateBufferWithAlignment(a_Allocator,
				&bufferCreateInfo,
				&allocationCreateInfo,
				m_Settings.m_AlignmentBytes,
				&m_Buffer,
				&m_Allocation,
				nullptr)
				!= VK_SUCCESS)
			{
				printf("Could not allocate Vulkan buffer!\n");
				return false;
			}

			vmaGetAllocationInfo(a_Allocator, m_Allocation, &m_AllocationInfo);
		}
		return true;
	}

	bool GpuBuffer::CleanUp(VmaAllocator& a_Allocator)
	{
		if(m_Settings.m_SizeInBytes != 0)
		{
			vmaDestroyBuffer(a_Allocator, m_Buffer, m_Allocation);
		}
		//Overwrite with default initial settings.
		m_Settings = GpuBufferSettings();
		m_AllocationInfo = VmaAllocationInfo{};
		m_Buffer = {};
		m_Allocation = {};
		
		return true;
	}

	VkBuffer& GpuBuffer::GetBuffer()
	{
		return m_Buffer;
	}

	VmaAllocation& GpuBuffer::GetAllocation()
	{
		return m_Allocation;
	}

	VmaAllocationInfo& GpuBuffer::GetAllocationInfo()
	{
		return m_AllocationInfo;
	}
}
