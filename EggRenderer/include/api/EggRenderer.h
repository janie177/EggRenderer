#pragma once
#include <cstdint>
#include <glm/glm/glm.hpp>
#include <glm/glm/ext/matrix_transform.hpp>
#include <string>

#include "Camera.h"
#include "EggMaterial.h"
#include "EggMesh.h"
#include "InputQueue.h"
#include "DrawData.h"

namespace egg
{
    class EggRenderer;
    struct MeshInstance;

	/*
     * The vertex format for meshes.
     */
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
		glm::vec2 uv;
	};

	struct MeshCreateInfo
	{
		const Vertex* m_VertexBuffer = nullptr;
		const uint32_t* m_IndexBuffer = nullptr;
		uint32_t m_NumIndices = 0;
		uint32_t m_NumVertices = 0;
	};

	/*
     * Shape type for basic mesh creation.
     */
	enum class Shape
	{
		CUBE,
		SPHERE,
		PLANE
	};

	enum class DebugPrintFlags : uint32_t
	{
		VERBOSE = 1,
		INFO = 16,
		WARNING = 256,
		ERROR = 4096
	};

	inline DebugPrintFlags operator | (const DebugPrintFlags& a_Lhs, const DebugPrintFlags& a_Rhs)
	{
		return static_cast<DebugPrintFlags>(static_cast<uint32_t>(a_Lhs) | static_cast<uint32_t>(a_Rhs));
	}

	/*
	 * Format types for the renderer.
	 */
	enum class Format
	{
		//Format corresponding with the vulkan enum entry.
		FORMAT_R8_G8_B8_SRGB = 50
	};

	struct RendererSettings
	{
		//The name of the window.
		std::string windowName = "My Window!";

		//Set to true to enable debug callbacks and validation layers.
		bool enableDebugMode = true;

		//Bit combined flags determining which messages get printed when debugging is enabled.
		DebugPrintFlags debugFlags = DebugPrintFlags::ERROR | DebugPrintFlags::WARNING;

		//The index of the physical graphics device to use.
		std::uint32_t gpuIndex = 0;

		//Window and swapchain resolution.
		std::uint32_t resolutionX = 512;
		std::uint32_t resolutionY = 512;

		//Make the window full-screen or not.
		bool fullScreen = false;

		//Lock the cursor to the window or not.
		bool lockCursor = false;

		//Use vsync or not.
		bool vSync = true;

		//The amount of buffers in the swapchain, 2 or three is preferred. May be changed depending on device minimum and maximum.
		std::uint32_t m_SwapBufferCount = 2;

		//The clear color for the screen.
		glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

		//The format used to output to the screen.
		Format outputFormat = Format::FORMAT_R8_G8_B8_SRGB;

		//The path where all spir-v shaders are stored.
		std::string shadersPath = "/shaders/";

		//How many materials to allow to exist. Allocates all memory up-front.
		uint32_t maxNumMaterials = 1000000;

		//How often to clean up unused resources in frames.
		uint32_t cleanUpInterval = 120;
	};

	/*
     * Information used when creating shapes.
     */
	struct ShapeCreateInfo
	{
		//The type of shape to create.
		Shape m_ShapeType = Shape::CUBE;

		//The radius of the shape.
		float m_Radius = 1.f;

		//Sphere specific settings.
		struct
		{
			//How many splits vertically on the sphere surface.
			uint32_t m_StackCount = 10.f;

			//How many splits horizontally on the sphere surface.
			uint32_t m_SectorCount = 10.f;
		} m_Sphere;

		//The transform applied directly to the vertices of the shape.
		glm::mat4 m_InitialTransform = glm::identity<glm::mat4>();

	};

	/*
	 * The public interface for the main renderer instance.
	 */
	class EggRenderer
	{
	public:
		virtual ~EggRenderer() = default;

		/*
		 * Create an instance of the underlying renderer implementation.
		 */
		static std::unique_ptr<EggRenderer> CreateInstance(const RendererSettings& a_Settings);

		/*
		 * Initialize the renderer.
		 */
		virtual bool Init(const RendererSettings& a_Settings) = 0;

		/*
		 * Draw the next frame.
		 */
		virtual bool DrawFrame(const DrawData& a_DrawData) = 0;

		/*
		 * Create a dynamic draw-call from the meshes.
		 * The mesh and all instances to be included should be provided.
		 * The mesh instances contain indices into the a_Materials vector.
		 *
		 * Compiling a draw call stores all data in a compact format.
		 * Changes to materials or transforms will require a recompilation.
		 */
		virtual DynamicDrawCall CreateDynamicDrawCall(
			const std::shared_ptr<EggMesh>& a_Mesh,
			const std::vector<MeshInstance>& a_Instances,
			const std::vector<std::shared_ptr<EggMaterial>>& a_Materials,
			bool a_Transparent = false) = 0;

		/*
		 * Resize the the rendering output.
		 */
		virtual bool Resize(bool a_FullScreen, std::uint32_t a_Width, std::uint32_t a_Height) = 0;

		/*
		 * Returns true if the window is in full-screen mode.
		 */
		virtual bool IsFullScreen() const = 0;

		/*
		 * Get all input events since this function was last called.
		 */
		virtual InputData QueryInput() = 0;

		/*
		 * Destroy the renderer.
		 */
		virtual bool CleanUp() = 0;

		/*
		 * Get the current render resolution.
		 */
		virtual glm::vec2 GetResolution() const = 0;

		/*
		 * Create a mesh resource.
		 * This uploads the provided buffers to the GPU.
		 */
		virtual std::shared_ptr<EggMesh> CreateMesh(const std::vector<Vertex>& a_VertexBuffer, const std::vector<std::uint32_t>& a_IndexBuffer) = 0;

		/*
		 * Create a mesh using raw pointers instead of vectors.
		 */
		virtual std::shared_ptr<EggMesh> CreateMesh(const MeshCreateInfo& a_MeshCreateInfo) = 0;

		/*
		 * Create multiple mesh resources.
		 */
		virtual std::vector<std::shared_ptr<EggMesh>> CreateMeshes(const std::vector<MeshCreateInfo>& a_MeshCreateInfos) = 0;

		/*
		 * Create a mesh of a certain type.
		 * The transform provided is applied to the vertices themselves.
		 *
		 * Note: Unevenly scaling a mesh (x, y, z scale are not equal) will warp normals.
		 * To e.g. turn a cube into a rectangle, the initial transform can be used to not affect the normals this way.
		 */
		virtual std::shared_ptr<EggMesh> CreateMesh(const ShapeCreateInfo& a_ShapeCreateInfo) = 0;

	};

}
