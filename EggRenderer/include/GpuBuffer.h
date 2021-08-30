#pragma once
#include <vk_mem_alloc.h>

namespace egg
{
	/*
	 * Used to allocate memory.
	 */
	struct GpuBufferSettings
	{
		size_t m_SizeInBytes = 0;				//The buffer size in bytes.
		size_t m_AlignmentBytes = 0;			//The buffers minimum alignment in bytes.
		VmaMemoryUsage m_MemoryUsage = VMA_MEMORY_USAGE_UNKNOWN;
		VkBufferUsageFlags m_BufferUsageFlags = 0;
	};

	struct CPUWrite
	{
		void* m_Data = nullptr;
		size_t m_Offset = 0;
		size_t m_Size = 0;
	};
	
	class GpuBuffer
	{
	public:
		GpuBuffer();
		GpuBuffer(GpuBuffer&&) = default;
		GpuBuffer& operator =(GpuBuffer&&) = default;
		
		GpuBuffer(const GpuBuffer&) = delete;
		GpuBuffer& operator =(const GpuBuffer&) = delete;

		/*
		 * Initialize this buffer.
		 * Must be called only once before this buffer can be used.
		 */
		bool Init(const GpuBufferSettings& a_InitialSettings, VkDevice& a_Device, VmaAllocator& a_Allocator);
		
		/*
		 * Write to this GPU buffer from the CPU.
		 * When a_Resize is true, the buffer will be resized if needed.
		 * 
		 * Returns true if writing was successful.
		 * Returns false if the data could not be written.
		 */
		bool Write(const CPUWrite* a_Writes, size_t a_NumWrites, bool a_Resize = false);

		/*
		 * Resize the buffer with the given settings.
		 * The old buffer data will be lost.
		 */
		bool Resize(const GpuBufferSettings& a_Settings);

		/*
		 * Free all allocated resources for this buffer.
		 */
		bool CleanUp();

		/*
		 * Get the size in bytes for this buffer.
		 */
		size_t GetSize() const;
		
		VkBuffer GetBuffer() const;
		VmaAllocation GetAllocation() const;
		VmaAllocationInfo GetAllocationInfo() const;

	private:
		//The buffer has to have access to the device and allocator.
		VkDevice m_Device;
		VmaAllocator m_Allocator;
		bool m_Initialized;
		
		//Allocation specific owned data.
		GpuBufferSettings m_Settings;
		VmaAllocation m_Allocation;
		VmaAllocationInfo m_AllocationInfo;
		VkBuffer m_Buffer;
	};
}