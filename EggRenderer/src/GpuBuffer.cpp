#include "GpuBuffer.h"

#include <cassert>
#include <cstdio>
#include <memory>

namespace egg
{
	GpuBuffer::GpuBuffer(): m_Device(nullptr), m_Allocator(nullptr), m_Initialized(false), m_Allocation(nullptr),
	                        m_AllocationInfo(),
	                        m_Buffer(nullptr)
	{
	}

	bool GpuBuffer::Init(const GpuBufferSettings& a_InitialSettings, VkDevice& a_Device, VmaAllocator& a_Allocator)
	{
		if(m_Initialized)
		{
			printf("Trying to init GPU buffer that was already initialized.\n");
			return false;
		}

		//Store references to systems and set as initialized.
		m_Device = a_Device;
		m_Allocator = a_Allocator;
		m_Initialized = true;

		//Initialize the buffer, and return true when that has succeeded.
		return Resize(a_InitialSettings);
	}

	bool GpuBuffer::Write(const CPUWrite* a_Writes, size_t a_NumWrites, bool a_Resize)
	{
		assert(m_Initialized);
		
		//Ensure that this buffer allows CPU writing.
		if ((m_Settings.m_MemoryUsage & (VMA_MEMORY_USAGE_CPU_ONLY | VMA_MEMORY_USAGE_CPU_TO_GPU | VMA_MEMORY_USAGE_CPU_COPY)) == 0)
		{
			printf("Trying to write to a buffer not accessible to the CPU!\n");
			return false;
		}

		//Calculate the required size for the writes.
		size_t requiredSize = 0;
		for(int i = 0; i < static_cast<int>(a_NumWrites); ++i)
		{
			requiredSize = std::max(requiredSize, a_Writes[i].m_Offset + a_Writes[i].m_Size);
		}

		//Resize if allowed and required.
		if(m_Settings.m_SizeInBytes < requiredSize)
		{
			GpuBufferSettings settings = m_Settings;
			settings.m_SizeInBytes = requiredSize;
			if (!a_Resize || !Resize(settings))
			{
				printf("Not enough space to perform buffer writes! Buffer could not be resized.\n");
				return false;
			}
		}
		
		//Map the entire buffer.
		void* data;
		vkMapMemory(m_Device, m_AllocationInfo.deviceMemory, m_AllocationInfo.offset, VK_WHOLE_SIZE, 0, &data);

		//Perform each of the writes.
		for(int i = 0; i < static_cast<int>(a_NumWrites); ++i)
		{
			const auto& write = a_Writes[i];
			memcpy(data, write.m_Data, write.m_Size);
		}
		
		vkUnmapMemory(m_Device, m_AllocationInfo.deviceMemory);
		
		return true;
	}
	
	bool GpuBuffer::Resize(const GpuBufferSettings& a_Settings)
	{
		assert(m_Initialized);
		
		//First clean up existing stuff.
		CleanUp();

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
			if (vmaCreateBufferWithAlignment(m_Allocator,
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

			vmaGetAllocationInfo(m_Allocator, m_Allocation, &m_AllocationInfo);
		}
		return true;
	}

	bool GpuBuffer::CleanUp()
	{
		if(m_Settings.m_SizeInBytes != 0)
		{
			vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
		}
		//Overwrite with default initial settings.
		m_Settings = GpuBufferSettings();
		m_AllocationInfo = VmaAllocationInfo{};
		m_Buffer = {};
		m_Allocation = {};
		
		return true;
	}

	size_t GpuBuffer::GetSize() const
	{
		assert(m_Initialized);
		return m_Settings.m_SizeInBytes;
	}

	VkBuffer GpuBuffer::GetBuffer() const
	{
		assert(m_Initialized);
		return m_Buffer;
	}

	VmaAllocation GpuBuffer::GetAllocation() const
	{
		assert(m_Initialized);
		return m_Allocation;
	}

	VmaAllocationInfo GpuBuffer::GetAllocationInfo() const
	{
		assert(m_Initialized);
		return m_AllocationInfo;
	}
}
