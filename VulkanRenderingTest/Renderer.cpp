#include "Renderer.h"

#include <cstdio>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

bool Renderer::Init(const std::uint32_t a_Width, const std::uint32_t a_Height, const bool a_EnableDebugValidation, DebugPrintFlags a_DebugFlags)
{
	/*
	 * Init GLFW and ensure that it supports Vulkan.
	 */
	if(!glfwInit())
	{
		printf("Could not initialize GLFW!\n");
		return false;
	}

	if(!glfwVulkanSupported())
	{
		printf("Vulkan is not supported for GLFW!");
		return false;
	}

    //Window creation
    
    // With GLFW_CLIENT_API set to GLFW_NO_API there will be no OpenGL (ES) context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(a_Width, a_Height, "Vulkan Render Test", NULL, NULL);


    //Try to initialize the vulkan context.
    if (!InitVulkan(a_Width, a_Height, a_EnableDebugValidation, a_DebugFlags))
	{
        return false;
	}

    //Try to find a GPU, then check it for compatibility and create the devices and queues.
    if(!InitDevice(a_EnableDebugValidation))
    {
        return false;
    }



	return true;
}

Renderer::Renderer() : m_Window(nullptr)
{

}

bool Renderer::Run()
{
	return false;
}

bool Renderer::InitVulkan(const std::uint32_t a_Width, const std::uint32_t a_Height, const bool a_EnableDebugValidation,
    const DebugPrintFlags a_DebugFlags)
{
    /*
     * Create the Vulkan instance
     */

     //Get all the vulkan extensions required for GLTF to work.
    std::vector<const char*> extensions{};
    uint32_t count;
    const char** surfaceExtensions = glfwGetRequiredInstanceExtensions(&count);
    for (uint32_t i = 0; i < count; i++)
    {
        extensions.push_back(surfaceExtensions[i]);
    }

    //Generic information about the application such as names and versions.
    VkApplicationInfo appInfo;
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VulkanTestProject";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "TestRenderer";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.pNext = NULL;
    }

    //Information about the vulkan context.
    VkInstanceCreateInfo createInfo;
    VkDebugUtilsMessengerCreateInfoEXT debug;
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    {
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;   //0 by default, but can be enabled below.

        if (a_EnableDebugValidation)
        {
            //Add the debug callback extension and configure it.
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug.flags = 0;
            debug.pNext = NULL;
            debug.messageSeverity = static_cast<uint32_t>(a_DebugFlags);    //These flags correspond to the Vk Enumeration.
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;                      //Print everything basically.
            debug.pfnUserCallback = debugCallback;
            debug.pUserData = nullptr;
            createInfo.pNext = &debug;

            //Set the layers and pass data pointers.
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
    }

    //Create the actual instance.
    const auto initResult = vkCreateInstance(&createInfo, NULL, &m_VulkanInstance);
    if (initResult != VK_SUCCESS)
    {
        printf("Could not create Vulkan instance. Cause: %i\n", initResult);
        return false;
    }

    printf("Vulkan instance succesfully created.\n");

    /*
     * Bind GLFW and Vulkan.
     */
    const auto result = glfwCreateWindowSurface(m_VulkanInstance, m_Window, NULL, &m_Surface);
    if (result != VK_SUCCESS)
    {
        printf("Could not create window surface for Vulkan and GLFW.\n");
        return false;
    }

    return true;
}

bool Renderer::InitDevice(const bool a_EnableDebugValidation)
{
    /*
     * Find the most suitable GPU.
     */
    uint32_t gpuQueueIndex = 0;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_VulkanInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        printf("No physical GPU found.\n");
        return false;
    }

    // Retrieve all the devices available on this PC.
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_VulkanInstance, &deviceCount, devices.data());

    /*
     * Note to self:
     * So it appears that in Vulkan you have full control over the queues.
     * First you query the hardware for the supported queues, and then you can create virtual queues on the logical device from their indices.
     * So ideally, I'd make a function that lets me specify which GPU I want, how many copy/compute/graphics queues I'd want.
     * Then I guess the function would create these queue objects if they are valid for the GPU selected.
     */

     //Loop over the devices, and choose a suitable one. This should be end-user selectable normally I guess. 
    for (const auto& device : devices)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);   //So this has to be called to allocate the space...
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());  //And then called again to actually write to it. ..... .. ... 

        bool suitableGPU = false;

        uint32_t queueFamilyIdx = 0;
        for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 presentSupport;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIdx, m_Surface, &presentSupport);
                if (presentSupport)
                {
                    suitableGPU = true;
                    gpuQueueIndex = queueFamilyIdx;
                    break;
                }
            }
            queueFamilyIdx++;
        }

        if (suitableGPU)
        {
            m_PhysicalDevice = device;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE)
    {
        printf("Could not find suitable GPU for Vulkan.\n");
        return false;
    }

    /*
     * Create the logic device and queue.
     */
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo;
    {
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = NULL;
        queueCreateInfo.flags = 0;
        queueCreateInfo.queueFamilyIndex = gpuQueueIndex;   //The GPU Queue that was checked for present and graphics support earlier.
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
    }

    // 3.2. The queue family/families must be provided to allow the device to use them.
    std::vector<uint32_t> uniqueQueueFamilies = { gpuQueueIndex };

    // G.X. TODO: add device swapchane extension check.

    // 3.3. Specify the device creation information.
    VkDeviceCreateInfo createInfo;
    const std::vector<const char*> swapchainExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    {
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.pEnabledFeatures = NULL;
        createInfo.enabledExtensionCount = (uint32_t)swapchainExtensions.size();
        createInfo.ppEnabledExtensionNames = swapchainExtensions.data();
        createInfo.enabledLayerCount = 0;

        if (a_EnableDebugValidation) {
            // To have device level validation information, the layers are added here.
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
    }

    //Creat the logical device instance.
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, NULL, &m_Device) != VK_SUCCESS) 
    {
        printf("Could not create Vulkan logical device.\n");
        return false;
    }

    /*
     * Initialize the queue that was selected on the selected GPU.
     * My GTX 1080 apparently contains three queue families. Each QueueFamily contains physical queues.
     */
    vkGetDeviceQueue(m_Device, gpuQueueIndex, 0, &m_Queue);
    return true;
}

bool Renderer::CreateSwapChain()
{
    //TODO


    return true;
}

VkBool32 Renderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                 void* pUserData)
{
    std::vector<std::string> severities{ "Verbose", "Info", "Warning", "Error", "???"};
    std::string* severity = nullptr;

    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        severity = &severities[0];
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        severity = &severities[1];
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        severity = &severities[2];
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        severity = &severities[3];
        break;
    default:
        severity = &severities[4];
    }

    std::cout << "[Vulkan] [" << *severity << "] " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}
