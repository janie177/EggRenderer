#pragma once
#include <vulkan/vulkan.h>
#include <cinttypes>
#include <iostream>
#include <GLFW/glfw3.h>

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

class Renderer
{
public:
	Renderer();

	/*
	 * Initialize systems.
	 */
	bool Init(const std::uint32_t a_Width, const std::uint32_t a_Height, const bool a_EnableDebugValidation, const DebugPrintFlags a_DebugFlags);

	/*
	 * Run for your life!
	 */
	bool Run();

private:
	/*
	 * Initialize Vulkan context and enable debug layers if specified.
	 */
	bool InitVulkan(const std::uint32_t a_Width, const std::uint32_t a_Height, const bool a_EnableDebugValidation, const DebugPrintFlags a_DebugFlags);

	/*
	 * Select a GPU and initialize queues. 
	 */
	bool InitDevice(const bool a_EnableDebugValidation);

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
	VkQueue m_Queue;						//The logical queue that is backed by the hardware selected.
	VkSwapchainKHR m_SwapChain;				//The swapchain for the GLFW window.

	/*
	 * Dynamic Vulkan objects directly related to rendering.
	 */


	/*
	 * Other objects.
	 */
};
