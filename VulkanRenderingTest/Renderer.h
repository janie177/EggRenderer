#pragma once
#include <vulkan/vulkan.h>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <glm/glm/glm.hpp>

#include "ConcurrentRegistry.h"
#include "vk_mem_alloc.h"
#include "RenderStage.h"
#include "Resources.h"

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
	std::uint32_t resolutionX = 512;
	std::uint32_t resolutionY = 512;

	//Use vsync or not.
	bool vSync = true;

	//The amount of buffers in the swapchain, 2 or three is preferred. May be changed depending on device minimum and maximum.
	std::uint32_t m_SwapBufferCount = 2;

	//The clear color for the screen.
	glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

	//The format used to output to the screen.
	VkFormat outputFormat = VK_FORMAT_B8G8R8A8_SRGB;
};

/*
 * Struct containing information about the renderer.
 * This is passed to any rendering stage for access to the pipeline objects.
 */
struct RenderData
{
	RenderData() : m_VulkanInstance(nullptr),
	               m_PhysicalDevice(nullptr),
	               m_Device(nullptr),
	               m_Surface(nullptr),
	               m_Queues{},
	               m_Allocator(nullptr),
				   m_Settings()
	{
	}

	VkInstance m_VulkanInstance;			//The global vulkan context.
	VkPhysicalDevice m_PhysicalDevice;		//Physical GPU device.
	VkDevice m_Device;						//Logical device wrapping around physical GPU.
	VkSurfaceKHR m_Surface;					//The output surface. In this case provided by GLFW.
	QueueInfo m_Queues[3];					//One queue of each type.
	VmaAllocator m_Allocator;				//External library handling memory management to keep this project a bit cleaner.
	std::vector<Frame> m_FrameData;			//Resources for each frame.
	
	RendererSettings m_Settings;			//All settings for the renderer.

};

/*
 * The main renderer class.
 */
class Renderer
{
public:
	Renderer();

	/*
	 * Initialize systems.
	 */
	bool Init(const RendererSettings& a_Settings);

	/*
	 * Resize the the rendering output.
	 */
	bool Resize(std::uint32_t a_Width, std::uint32_t a_Height);

	/*
	 * Destroy the renderer.
	 */
	bool CleanUp();

	/*
	 * Draw the next frame.
	 */
	bool DrawFrame(const DrawData& a_DrawData);

	/*
	 * Create a mesh resource.
	 * This uploads the provided buffers to the GPU.
	 */
	std::shared_ptr<Mesh> CreateMesh(const std::vector<Vertex>& a_VertexBuffer, const std::vector<std::uint32_t>& a_IndexBuffer);

private:
	template<typename T>
	inline T* AddRenderStage(std::unique_ptr<T>&& a_Stage)
	{
		//Ensure the right type is provided.
		//This only runs at program startup so dynamic cast is fine here.
		if(dynamic_cast<T*>(a_Stage.get()) == nullptr)
		{
			printf("Wrong render stage type provided!");
			exit(0);
			return nullptr;
		}
		
		m_RenderStages.emplace_back(std::move(a_Stage));
		T* ptr = static_cast<T*>(m_RenderStages[m_RenderStages.size() - 1].get());
		return ptr;
	}
	
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
	 * Acquire the swap chain index for the next frame.
	 */
	bool AcquireSwapChainIndex();

	/*
	 * Initialize the Vulkan swapchain.
	 */
	bool CreateSwapChain();

	/*
	 * Create the frame buffers and synchronization objects for the swap chain.
	 * 
	 */
	bool CreateSwapChainFrameData();
	
	/*
	 * Destroy the swapchain and associated render targets.
	 * Also cleans up any synchronisation objects.
	 */
	bool CleanUpSwapChain();

	/*
     * Create the pipeline related objects.
     */
	bool InitPipeline();

	//Vulkan debug layer callback function.
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

private:
	bool m_Initialized;

	uint32_t m_FrameCounter;					//The index of the current frame. Used to track resource usage.
	uint32_t m_MeshCounter;						//The mesh ID incrementing counter.
	
	/*
     * GLFW Objects.
     */
	GLFWwindow* m_Window;

    /*
	 * Main Vulkan objects.
	 */
	RenderData m_RenderData;

	VkSwapchainKHR m_SwapChain;				//The swapchain for the GLFW window.
	std::vector<VkImageView> m_SwapViews;	//The views for the swapchain images.
	
	VkCommandBuffer m_CopyBuffer;			//The command buffer used to copy resources to the GPU.
	VkCommandPool m_CopyCommandPool;		//The command pool used for copying data.
	
	std::uint32_t m_CurrentFrameIndex;		//The current frame index.
	VkSemaphore m_FrameReadySemaphore;		//This semaphore is signaled by the swapchain when it's ready for the next frame. 

	/*
	 * The render stages in this renderer.
	 */
	std::vector<std::unique_ptr<RenderStage>> m_RenderStages;

	/*
	 * References to render stages for individual specific use.
	 */
	RenderStage_HelloTriangle* m_HelloTriangleStage;	//The hello world triangle for testing.
	RenderStage_Deferred* m_DeferredStage;				//The deferred render pass.
	
	/*
	 * Dynamic Vulkan objects directly related to rendering.
	 */
	ConcurrentRegistry<Mesh> m_Meshes;		//Vector of all the meshes loaded. If ref count reaches 1, free.
};
