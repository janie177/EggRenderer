#pragma once
#include <vulkan/vulkan.h>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>

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
	 * Run for your life!
	 */
	bool Run();

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
	 * Initialize the Vulkan swapchain.
	 */
	bool CreateSwapChain();

	//Vulkan debug layer callback function.
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

private:
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
	VkQueue m_Queues[3];					//One queue of each type.
	VkSwapchainKHR m_SwapChain;				//The swapchain for the GLFW window.

	/*
	 * Dynamic Vulkan objects directly related to rendering.
	 */


	/*
	 * Other objects.
	 */
	RendererSettings m_Settings;
};
