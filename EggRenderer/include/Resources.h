#pragma once
#include <memory>
#include <glm/glm/glm.hpp>

#include "Bindless.h"
#include "vk_mem_alloc.h"
#include "api/EggStaticMesh.h"
#include "api/EggMaterial.h"
#include "api/EggTexture.h"

namespace egg
{
	struct MaterialMemoryData;

    /*
	 * Base resource class.
	 */
	class Resource
	{
		friend class Renderer;
		friend class DynamicDrawCall;
	public:
		virtual ~Resource() = default;
	};

	class Texture : public EggTexture, public Resource
	{
	public:
		Texture(VmaAllocator a_Allocator, VkImageType a_Type, const glm::uvec2& a_Dimensions, VkImage a_Image, VmaAllocation a_Allocation, VkAccessFlags a_AccessFlags, VkImageLayout a_Layout) :
			m_Allocator(a_Allocator), m_Type(a_Type), m_Dimensions(a_Dimensions), m_Image(a_Image), m_Allocation(a_Allocation), m_Layout(a_Layout), m_AccessFlags(a_AccessFlags)
		{}

		~Texture() override
		{
			vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
		}

		VkImageType GetType() const { return m_Type; }
		VkImage GetImage() const { return m_Image; }
		glm::uvec2 GetDimensions() const { return m_Dimensions; }

		BindlessHandle GetSrvHandle() const { return m_Srv; }
		BindlessHandle GetUavHandle() const { return m_Uav; }

		VkAccessFlags GetAccessFlags() const { return m_AccessFlags; }
		VkImageLayout GetLayout() const { return m_Layout; }

		/*
		 * Set the state that is stored in this Texture object.
		 * This does NOT actually do any state transitions.
		 * Manual barrier creation is required.
		 */
		void SetState(VkAccessFlags a_AccessFlags, VkImageLayout a_Layout)
		{
			m_AccessFlags = a_AccessFlags;
			m_Layout = a_Layout;
		}

	private:
		VmaAllocator m_Allocator;
		VkImageType m_Type;
		glm::uvec2 m_Dimensions;
		VkImage m_Image;
		VmaAllocation m_Allocation;
		BindlessHandle m_Uav;	//Every texture has a handle for writing and reading.
		BindlessHandle m_Srv;

		//State related data.
		VkImageLayout m_Layout;
		VkAccessFlags m_AccessFlags;
	};

	/*
	 * Mesh class containing a vertex and index buffer.
	 */
	class StaticMesh : public EggStaticMesh, public Resource
	{
	public:
		StaticMesh(uint32_t a_UniqueId, VmaAllocator a_Allocator, VmaAllocation a_Allocation, VkBuffer a_Buffer, std::uint64_t a_NumIndices, std::uint64_t a_NumVertices, size_t a_IndexBufferOffset, size_t a_VertexBufferOffset) :
			m_UniqueId(a_UniqueId),
			m_Allocator(a_Allocator),
			m_Allocation(a_Allocation),
			m_Buffer(a_Buffer),
			m_IndexOffset(a_IndexBufferOffset),
			m_VertexOffset(a_VertexBufferOffset),
			m_NumIndices(a_NumIndices),
			m_NumVertices(a_NumVertices)
		{
		}

        //Free memory when destructed automatically.
		//This means all buffers are OWNED by mesh. This only works because meshes are kept in a shared_ptr always.
		~StaticMesh() override
		{
			vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
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

		VmaAllocator m_Allocator;		//The allocator used to create this object.
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
     * Instance data that is packed and aligned correctly.
     * Custom data can be stored in the last row of the matrix.
     */
	union PackedInstanceData
	{
		//TODO try to pack everything in a mat4 in the future (last row is unused anyways).
		struct
		{
			//Mat4x4 (4x3 would add padding)
			glm::mat4 m_Transform;

			//Last column as individual components.
			union
			{
				glm::uvec4 m_CustomData;
				struct
				{
					uint32_t m_MaterialId;
					uint32_t m_CustomId;
					uint32_t m_CustomData3;
					uint32_t m_CustomData4;
				};
			};
		};
	};

	/*
	 * Light data ready to be uploaded to the GPU.
	 * This struct can contain position, direction, radiance, angle, radius etc.
	 */
	struct PackedLightData
	{
		//Light specific data.
		glm::vec4 m_Data1;
		glm::vec4 m_Data2;

		//Shared between all lights.
		union
		{
			glm::ivec4 m_SharedData;
			struct
			{
				int m_ShadowIndex;
				int m_Unused1;
				int m_Unused2;
				int m_Unused3;
			};
		};
	};

	/*
	 * A material instance with GPU backing memory.
	 */
	class Material : public EggMaterial, public Resource, public std::enable_shared_from_this<Material>
	{
    public:
		Material(const MaterialCreateInfo& a_Info);
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

	private:
		//Material data.
		float m_MetallicFactor;
		float m_RoughnessFactor;
		glm::vec3 m_AlbedoFactor;
		glm::vec3 m_EmissiveFactor;
		std::shared_ptr<EggMaterialTextures> m_Textures;
    };
}
