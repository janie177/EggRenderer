#pragma once
#include <vk_mem_alloc.h>

namespace egg
{
	/*
	 * Used to allocate memory.
	 */
	struct GpuBufferSettings
	{
		size_t m_SizeInBytes = 0;
		size_t m_AlignmentBytes = 0;
		VmaMemoryUsage m_MemoryUsage = VMA_MEMORY_USAGE_UNKNOWN;
		VkBufferUsageFlags m_BufferUsageFlags = 0;
	};
	
	class GpuBuffer
	{
	public:
		GpuBuffer() = default;
		GpuBuffer(const GpuBuffer&) = delete;
		GpuBuffer& operator =(const GpuBuffer&) = delete;
		
		bool Resize(const GpuBufferSettings& a_Settings, VmaAllocator& a_Allocator);
		bool CleanUp(VmaAllocator& a_Allocator);

		VkBuffer& GetBuffer();
		VmaAllocation& GetAllocation();
		VmaAllocationInfo& GetAllocationInfo();

	private:
		GpuBufferSettings m_Settings;
		VmaAllocation m_Allocation;
		VmaAllocationInfo m_AllocationInfo;
		VkBuffer m_Buffer;
	};
}