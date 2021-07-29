#pragma once
#include <memory>
#include <vector>
#include <glm/glm/glm.hpp>

#include "api/Transform.h"
#include "vk_mem_alloc.h"
#include "api/Camera.h"
#include "api/EggMesh.h"
#include "api/EggMaterial.h"

namespace egg
{
    class MaterialManager;
	struct MaterialMemoryData;

    /*
	 * Base resource class.
	 */
	class Resource
	{
		friend class Renderer;
	public:
		virtual ~Resource() = default;
		Resource() : m_LastUsedFrameId(0) {}

	private:
		uint32_t m_LastUsedFrameId;		//The frame index when this resource was used.
	};

	/*
	 * Mesh class containing a vertex and index buffer.
	 */
	class Mesh : public EggMesh, public Resource
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

	union UI32UI8Alias
	{
		uint32_t m_Data;
		struct
		{
			uint8_t m_X;
			uint8_t m_Y;
			uint8_t m_Z;
			uint8_t m_W;
		};
	};

	/*
     *Packed format with easy to access members for the CPU side of things.
     */
	union PackedMaterialData
	{
		glm::uvec4 m_Data;

		//Note: these can all be packed in a single byte (unless HDR) in case more data needs to be added. uint8_t is enough for metal/roughness.
		struct
		{
			uint16_t m_MetallicFactor;
			uint16_t m_RoughnessFactor;

			uint32_t m_TexturesIndex;
			UI32UI8Alias m_AlbedoFactor;
			UI32UI8Alias m_EmissiveFactor;
		};
	};

	/*
	 * A material instance with GPU backing memory.
	 */
	class Material : public EggMaterial, public Resource, public std::enable_shared_from_this<Material>
	{
		friend class MaterialManager;
		friend class Renderer;
    public:
		Material(const MaterialCreateInfo& a_Info, MaterialManager& a_Manager);

        glm::vec3 GetAlbedoFactor() const override;
        void SetAlbedoFactor(const glm::vec3& a_Factor) override;
        glm::vec3 GetEmissiveFactor() const override;
        void SetEmissiveFactor(const glm::vec3& a_Factor) override;
        float GetMetallicFactor() const override;
        void SetMetallicFactor(const float a_Factor) override;
        float GetRoughnessFactor() const override;
        void SetRoughnessFactor(const float a_Factor) override;
        std::shared_ptr<EggMaterialTextures> GetMaterialTextures() const override;
        void SetMaterialTextures(const std::shared_ptr<EggMaterialTextures>& a_Texture) override;

		/*
		 * Pack all material data in a tight format.
		 */
		PackedMaterialData PackMaterialData() const;

		/*
		 * Get the index in the GPU material buffer that currently contains valid material data for this material.
		 */
		uint32_t GetCurrentlyUsedGpuIndex() const;

		/*
		 * Returns true if a value has been changed, and a re-upload is necessary.
		 */
		bool IsDirty() const;

		/*
		 * Mark this material as dirty.
		 * This will force a re-upload of the data.
		 */
		void MarkAsDirty();

	private:
		//Material data.
		float m_MetallicFactor;
		float m_RoughnessFactor;
		glm::vec3 m_AlbedoFactor;
		glm::vec3 m_EmissiveFactor;
		std::shared_ptr<EggMaterialTextures> m_Textures;

		//Link to the manager that this material belongs to.
		MaterialManager* m_Manager;

		//Keep track of when to re-upload.
		bool m_DirtyFlag;

		//Current and previous allocation to be safe when not yet done uploading.
		std::shared_ptr<MaterialMemoryData> m_CurrentAllocation;
		std::shared_ptr<MaterialMemoryData> m_PreviousAllocation;
    };
}
