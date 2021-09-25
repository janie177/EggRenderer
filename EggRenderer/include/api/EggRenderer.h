#pragma once
#include <cstdint>
#include <glm/glm/glm.hpp>
#include <glm/glm/ext/matrix_transform.hpp>
#include <string>

#include "EggDrawData.h"
#include "Camera.h"
#include "EggMaterial.h"
#include "EggStaticMesh.h"
#include "EggTexture.h"
#include "InputQueue.h"

namespace egg
{
    class EggRenderer;
	class EggDrawData;

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
		TextureFormat outputFormat = TextureFormat::FORMAT_R8_G8_B8_SRGB;

		//The path where all spir-v shaders are stored.
		std::string shadersPath = "/shaders/";

		//The amount of allocated bindless texture descriptors.
		uint32_t maximumBindlessTextures = 300000;

		//The amount of allocated bindless writable texture descriptors.
		uint32_t maximumBindlessWriteTextures = 300000;

		//The amount of allocated buffer descriptors.
		uint32_t maximumBindlessBuffers = 300000;
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
		 * The DrawData object provided will be consumed upon calling.
		 *
		 * Returns true if no horrible things happened (explosions, plagues, forgetting to activate AH bonus box etc..).
		 */
		virtual bool DrawFrame(std::unique_ptr<EggDrawData>& a_DrawData) = 0;

		/*
		 * Create a new material with the given properties.
		 */
		virtual std::shared_ptr<EggMaterial> CreateMaterial(const MaterialCreateInfo& a_Info) = 0;

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
		 * Create a texture from the provided data.
		 * When no data is provided, the texture will not be written to.
		 */
		virtual std::shared_ptr<EggTexture> CreateTexture(const TextureCreateInfo& a_TextureCreateInfo) = 0;

		/*
		 * Create a mesh from the provided data.
		 */
		virtual std::shared_ptr<EggStaticMesh> CreateMesh(const StaticMeshCreateInfo& a_MeshCreateInfo) = 0;

		/*
		 * Create multiple mesh resources.
		 */
		virtual std::vector<std::shared_ptr<EggStaticMesh>> CreateMeshes(const std::vector<StaticMeshCreateInfo>& a_MeshCreateInfos) = 0;

		/*
		 * Create a mesh of a certain type.
		 * The transform provided is applied to the vertices themselves.
		 *
		 * Note: Unevenly scaling a mesh (x, y, z scale are not equal) will warp normals.
		 * To e.g. turn a cube into a rectangle, the initial transform can be used to not affect the normals this way.
		 */
		virtual std::shared_ptr<EggStaticMesh> CreateMesh(const ShapeCreateInfo& a_ShapeCreateInfo) = 0;

		/*
		 * Create a new DrawData object.
		 * This object can be used to define resources and drawing operations.
		 * It is passed to the DrawFrame() function, which takes ownership.
		 *
		 * Returns a unique pointer containing the new DrawData object.
		 */
		virtual std::unique_ptr<EggDrawData> CreateDrawData() = 0;

	};

}
