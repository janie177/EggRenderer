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
	glm::vec3 tangent;
	glm::vec2 uv;
};

/*
 * Mesh class containing a vertex and index buffer.
 */
class Mesh : public Resource
{
public:
	Mesh(uint32_t a_UniqueId, VmaAllocation a_Allocation, VkBuffer a_Buffer, std::uint64_t a_NumIndices, std::uint64_t a_NumVertices, size_t a_IndexBufferOffset, size_t a_VertexBufferOffset) :
		m_Allocation(a_Allocation),
		m_Buffer(a_Buffer),
		m_IndexOffset(a_IndexBufferOffset),
		m_VertexOffset(a_VertexBufferOffset),
		m_NumIndices(a_NumIndices),
		m_NumVertices(a_NumVertices),
		m_UniqueId(a_UniqueId)
	{
	}

	VkBuffer& GetBuffer() { return m_Buffer; }
	VmaAllocation& GetAllocation() { return m_Allocation; }

	size_t GetNumIndices() const { return m_NumIndices; }
	size_t GetNumVertices() const { return m_NumVertices; }

	size_t GetIndexBufferOffset() const { return m_IndexOffset; }
	size_t GetVertexBufferOffset() const { return m_VertexOffset; }

	uint32_t GetUniqueId() const { return m_UniqueId; }


private:
	uint32_t m_UniqueId;			//The unique ID for this mesh that can be used for sorting and comparing.
	
	VmaAllocation m_Allocation;		//The memory allocation containing the buffer.
	VkBuffer m_Buffer;				//The buffer containing the vertex and index buffers.

	size_t m_IndexOffset;			//The offset into m_Buffer for the index buffer start.
	size_t m_VertexOffset;			//The offset into m_Buffer for the vertex buffer 
	size_t m_NumIndices;			//The amount of indices in the index buffer.
	size_t m_NumVertices;			//The amount of vertices in the vertex buffer.
};

/*
 * Data to render a mesh.
 */
struct MeshInstance
{
	glm::mat4 m_Transform;	//The transform of the mesh.
							//TODO: material
};

/*
 * A single draw call.
 */
struct DrawCall
{
	/*
	 * The mesh to draw.
	 */
	std::shared_ptr<Mesh> m_Mesh;

	/*
	 * A pointer to an array of mesh instances to draw.
	 */
	MeshInstance* m_pMeshInstances = nullptr;

	/*
	 * The amount of mesh instances in the m_pMeshInstances array.
	 */
	size_t m_NumInstances = 0;

	/*
	 * If true, this geometry is drawn in a separate forward pass after the deferred stage ends.
	 */
	bool m_Transparent = false;
};

/*
 * The camera containing the FOV and such.
 */
class Camera
{
	//TODO
private:
	float m_Fov;
	float m_NearPlane;
	float m_FarPlane;
};

/*
 * All information needed to draw a single frame with the renderer.
 * All data is copied when this is passed to the DrawFrame function of Renderer.
 */
struct DrawData
{
	/*
	 * A pointer to the camera to use.
	 */
	Camera* m_Camera = nullptr;

	/*
	 * Pointer to an array of draw calls.
	 */
	DrawCall* m_pDrawCalls = nullptr;

	/*
	 * The amount of draw calls in the m_pDrawCalls array.
	 */
	size_t m_NumDrawCalls = 0;
};