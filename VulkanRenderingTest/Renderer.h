#pragma once
#include <vulkan/vulkan.h>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <glm/glm/glm.hpp>

#include "vk_mem_alloc.h"

enum class DebugPrintFlags : uint32_t
{
	VERBOSE = 1,
	INFO = 16,
	WARNING = 256,
	ERROR = 4096
};

enum class QueueType
{
    QUEUE_TYPE_GRAPHICS = 0,
	QUEUE_TYPE_COMPUTE = 1,
	QUEUE_TYPE_TRANSFER = 2
};

inline DebugPrintFlags operator | (const DebugPrintFlags& a_Lhs, const DebugPrintFlags& a_Rhs)
{
	return static_cast<DebugPrintFlags>(static_cast<uint32_t>(a_Lhs) | static_cast<uint32_t>(a_Rhs));
}

/*
 * Information about a queue (handle, indices).
 */
struct QueueInfo
{
	VkQueue m_Queue;				//The queue handle.
	std::uint32_t m_FamilyIndex;	//The queue family index.
	std::uint32_t m_QueueIndex;		//The queue index within the family.
};

/*
 * Struct containing all the resources needed for a single frame.
 */
struct Frame
{
	VkFence m_Fence;						//The fence used to signal when the frame has completed.
	VkSemaphore m_WaitForFrameSemaphore;	//The semaphore that is signaled by the swapchain when a frame is ready to be written to.
	VkSemaphore m_WaitForRenderSemaphore;	//The semaphore that is signaled when the command buffer finishes, and the frame can be presented.
	VkCommandBuffer m_CommandBuffer;		//The graphics command buffer used for drawing and presenting.
	VkCommandPool m_CommandPool;			//The command pool used to allocate commands for this frame.
	VkFramebuffer m_FrameBuffer;			//The framebuffer that is bound to the swap chain's image view.
};

struct RendererSettings
{
	//Set to true to enable debug callbacks and validation layers.
	bool enableDebugMode = true;

	//Bit combined flags determining which messages get printed when debugging is enabled.
	DebugPrintFlags debugFlags = DebugPrintFlags::ERROR | DebugPrintFlags::WARNING;

	//The index of the physical graphics device to use.
	std::uint32_t gpuIndex = 0;		

	//Window and swapchain resolution.
	std::uint32_t windowWidth = 512;
	std::uint32_t windowHeight = 512;

	//Use vsync or not.
	bool vSync = true;

	//The clear color for the screen.
	glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

	//The format used to output to the screen.
	VkFormat outputFormat = VK_FORMAT_B8G8R8A8_SRGB;
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
class Mesh
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

class Renderer
{
public:
	Renderer();

	/*
	 * Initialize systems.
	 */
	bool Init(const RendererSettings& a_Settings);

	/*
	 * Destroy the renderer.
	 */
	bool CleanUp();

	/*
	 * Run for your life!
	 */
	bool Run();

	/*
	 * Create a mesh resource.
	 * This uploads the provided buffers to the GPU.
	 */
	std::shared_ptr<Mesh> CreateMesh(const std::vector<Vertex>& a_VertexBuffer, const std::vector<std::uint32_t>& a_IndexBuffer);

private:
	/*
	 * Initialize Vulkan context and enable debug layers if specified.
	 */
	bool InitVulkan();

	/*
	 * Select a GPU and initialize queues. 
	 */
	bool InitDevice();

	/*
	 * Initialize the memory allocation system.
	 */
	bool InitMemoryAllocator();

	/*
	 * Initialize the Vulkan swapchain.
	 */
	bool CreateSwapChain();

	/*
     * Create the pipeline related objects.
     */
	bool InitPipeline();

	/*
	 * Read a file and store the contents in the given output buffer as chars.
	 */
	bool ReadFile(const std::string& a_File, std::vector<char>& a_Output);

	/*
	 * Load a Spir-V shader from file and compile it.
	 */
	bool CreateShaderModuleFromSpirV(const std::string& a_File, VkShaderModule& a_Output);

	//Vulkan debug layer callback function.
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

private:
	bool m_Initialized;

	/*
     * GLFW Objects.
     */
	GLFWwindow* m_Window;

    /*
	 * Main Vulkan objects.
	 */
	VkInstance m_VulkanInstance;			//The global vulkan context.
	VkPhysicalDevice m_PhysicalDevice;		//Physical GPU device.
	VkDevice m_Device;						//Logical device wrapping around physical GPU.
	VkSurfaceKHR m_Surface;					//The output surface. In this case provided by GLFW.
	QueueInfo m_Queues[3];					//One queue of each type.

	VkSwapchainKHR m_SwapChain;				//The swapchain for the GLFW window.
	std::vector<VkImageView> m_SwapViews;	//The views for the swapchain images.

	VkCommandBuffer m_CopyBuffer;			//The command buffer used to copy resources to the GPU.
	VkCommandPool m_CopyCommandPool;		//The command pool used for copying data.

	VmaAllocator m_Allocator;				//External library handling memory management to keep this project a bit cleaner.

	std::uint32_t m_NumFrames;				//The amount of frames in the swapchain.
	std::uint32_t m_CurrentFrameIndex;		//The current frame index.
	VkSemaphore m_FrameReadySemaphore;	//This semaphore is signaled by the swapchain when it's ready for the next frame. 
	std::vector<Frame> m_FrameData;			//Resources for each frame.
	VkPipeline m_Pipeline;					//The pipeline containing all state used for rendering.
	VkShaderModule m_VertexShader;			//The vertex shader for the graphics pipeline.
	VkShaderModule m_FragmentShader;		//The fragment shader for the graphics pipeline.
	VkPipelineLayout m_PipelineLayout;		//The layout of the graphics pipeline.
	VkRenderPass m_RenderPass;				//The render pass for the default rendering pipeline.

	/*
	 * Dynamic Vulkan objects directly related to rendering.
	 */
	std::vector<std::shared_ptr<Mesh>> m_Meshes;	//Vector of all the meshes loaded. If ref count reaches 1, free.

	/*
	 * Other objects.
	 */
	RendererSettings m_Settings;
};
