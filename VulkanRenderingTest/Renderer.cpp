#include "Renderer.h"

#include <cstdio>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

bool Renderer::Init(const RendererSettings& a_Settings)
{
    m_Settings = a_Settings;

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
    m_Window = glfwCreateWindow(a_Settings.windowWidth, a_Settings.windowHeight, "Vulkan Render Test", NULL, NULL);


    //Try to initialize the vulkan context.
    if (!InitVulkan())
	{
        return false;
	}

    //Try to find a GPU, then check it for compatibility and create the devices and queues.
    if(!InitDevice())
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

bool Renderer::InitVulkan()
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

        if (m_Settings.enableDebugMode)
        {
            //Add the debug callback extension and configure it.
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug.flags = 0;
            debug.pNext = NULL;
            debug.messageSeverity = static_cast<uint32_t>(m_Settings.debugFlags);    //These flags correspond to the Vk Enumeration.
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

bool Renderer::InitDevice()
{
    /*
     * Find the most suitable GPU.
     */
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

    if (deviceCount <= m_Settings.gpuIndex)
    {
        printf("Invalid GPU index specified in renderer settings. Not that many devices.\n");
        return false;
    }

    auto& device = devices[m_Settings.gpuIndex];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);   //So this has to be called to allocate the space...
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());  //And then called again to actually write to it. ..... .. ... 

    //Assign the device.
    m_PhysicalDevice = device;

    //Keep track of the queue indices and whether or not a queue was found yet.
    bool queueFound[3] = {false, false, false};
    std::uint32_t queueFamIndex[3] = {0, 0, 0};
    std::uint32_t queueIndex[3] = { 0, 0, 0 };
    VkQueueFlagBits queueFlags[3]{ VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };

    //Vector containing the IDs of every queue that is used. Has to be unique.
    //The reason is that vulkan devices need to know exactly which queue families are used, and you can only specify each once.
    //A family can support multiple flags, which means it can have all queues in it. That's why it needs to be separately tracked.
    //I could have one queue family with all my different queues inside it(with different queue indexes within the family).
    //Amount of queues per family also needs to be tracked.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> uniqueQueueIds;

    uint32_t queueFamilyIdx = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
    {
        bool used = false;
        std::uint32_t usedIndex = 0;
        //Check for graphics queue if not yet found. Separate because it needs to be able to present.
        if (queueFamily.queueFlags & queueFlags[0] && !queueFound[static_cast<uint32_t>(QueueType::QUEUE_TYPE_GRAPHICS)])
        {
            VkBool32 presentSupport;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIdx, m_Surface, &presentSupport);
            if (presentSupport)
            {
                queueFound[0] = true;
                queueFamIndex[0] = queueFamilyIdx;
                queueIndex[0] = usedIndex;
                used = true;
                ++usedIndex;
            }
        }

        //Find other queue types.
        for(int i = 1; i < 3; ++i)
        {
            if (queueFamily.queueFlags & queueFlags[i] && !queueFound[i] && usedIndex < queueFamily.queueCount)
            {

                queueFound[i] = true;
                queueFamIndex[i] = queueFamilyIdx;
                queueIndex[i] = usedIndex;
                used = true;
                ++usedIndex;
            }
        }

        if(used)
        {
            uniqueQueueIds.emplace_back(queueFamilyIdx, usedIndex);
        }

        queueFamilyIdx++;
    }

    //Ensure a queue was found for each.
    for(int i = 0; i < 3; ++i)
    {
        if(!queueFound[i])
        {
            printf("Could not find queue of type %i on the GPU.\n", i);
            return false;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE)
    {
        printf("Could not find suitable GPU for Vulkan.\n");
        return false;
    }

    /*
     * Every queue needs a priority within a family.
     */
    std::vector<std::vector<float>> priorities;
    priorities.reserve(uniqueQueueIds.size());  //Reserve to prevent reallocation and then invalid memory.

    /*
     * Create the logic device and queue.
     */
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo;
    queueCreateInfo.resize(uniqueQueueIds.size());
    for(int i = 0; i < uniqueQueueIds.size(); ++i)
    {
        queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo[i].pNext = NULL;
        queueCreateInfo[i].flags = 0;
        queueCreateInfo[i].queueFamilyIndex = uniqueQueueIds[i].first;  //Id of the queue family.
        queueCreateInfo[i].queueCount = uniqueQueueIds[i].second;       //Amount of queues to use within this family.

        //Create a vector containing the priorities. All 1.0 for now.
        priorities.emplace_back(std::vector<float>());
        auto& vec = priorities[priorities.size() - 1];

        //One priority per queue in the queue family.
        for (int j = 0; j < queueCreateInfo[i].queueCount; ++j)
        {
            vec.push_back(1.f);
        }
        queueCreateInfo[i].pQueuePriorities = &vec[0];
    }

    /*
     * Creat the actual device and specify the queues to use etc.
     */
    VkDeviceCreateInfo createInfo;
    const std::vector<const char*> swapchainExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    {
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;

        //Create three queues as specified in the struct above (containing the right family index, and queue index is stored in an array).
        createInfo.queueCreateInfoCount = uniqueQueueIds.size();
        createInfo.pQueueCreateInfos = &queueCreateInfo[0];

        createInfo.pEnabledFeatures = NULL;
        createInfo.enabledExtensionCount = (uint32_t)swapchainExtensions.size();
        createInfo.ppEnabledExtensionNames = swapchainExtensions.data();
        createInfo.enabledLayerCount = 0;

        if (m_Settings.enableDebugMode) 
        {
            // To have device level validation information, the layers are added here.
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
    }

    //Create the logical device instance.
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, NULL, &m_Device) != VK_SUCCESS) 
    {
        printf("Could not create Vulkan logical device.\n");
        return false;
    }

    /*
     * Get the queues that were initialized for this device.
     */
    for(int i = 0; i < 3; ++i)
    {
        vkGetDeviceQueue(m_Device, queueFamIndex[i], queueIndex[i], &m_Queues[i]);
    }
    return true;
}

bool Renderer::CreateSwapChain()
{
    //


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
