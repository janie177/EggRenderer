#pragma once
#include <memory>
#include <vector>
#include <glm/glm/glm.hpp>

#include "vk_mem_alloc.h"

/*
 * Base resource class.
 */
class Resource
{
	friend class Renderer;
public:
	virtual ~Resource() = default;
	Resource() : m_LastUsedFrameId(0){}

private:
	uint32_t m_LastUsedFrameId;		//The frame index when this resource was used.
};

/*
 * Basic vertex format.
 */
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

/*
 * Mesh class containing a vertex and index buffer.
 */
class Mesh : public Resource
{
public:
	Mesh(VmaAllocation a_Allocation, VkBuffer a_Buffer, std::uint64_t a_NumIndices, std::uint64_t a_NumVertices, size_t a_IndexBufferOffset, size_t a_VertexBufferOffset) :
		m_Allocation(a_Allocation),
		m_Buffer(a_Buffer),
		m_IndexOffset(a_IndexBufferOffset),
		m_VertexOffset(a_VertexBufferOffset),
		m_NumIndices(a_NumIndices),
		m_NumVertices(a_NumVertices)
	{
	}

	VkBuffer& GetBuffer() { return m_Buffer; }
	VmaAllocation& GetAllocation() { return m_Allocation; }

	size_t GetNumIndices() const { return m_NumIndices; }
	size_t GetNumVertices() const { return m_NumVertices; }

	size_t GetIndexBufferOffset() const { return m_IndexOffset; }
	size_t GetVertexBufferOffset() const { return m_VertexOffset; }


private:
	VmaAllocation m_Allocation;		//The memory allocation containing the buffer.
	VkBuffer m_Buffer;				//The buffer containing the vertex and index buffers.

	size_t m_IndexOffset;			//The offset into m_Buffer for the index buffer start.
	size_t m_VertexOffset;			//The offset into m_Buffer for the vertex buffer 
	size_t m_NumIndices;			//The amount of indices in the index buffer.
	size_t m_NumVertices;			//The amount of vertices in the vertex buffer.
};