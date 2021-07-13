#include "Renderer.h"

#include <cstdio>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <filesystem>

#include "vk_mem_alloc.h"

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
		printf("Vulkan is not supported for GLFW!\n");
		return false;
	}

    //Window creation
    
    // With GLFW_CLIENT_API set to GLFW_NO_API there will be no OpenGL (ES) context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(a_Settings.windowWidth, a_Settings.windowHeight, "Vulkan Render Test", nullptr, nullptr);


    //Try to initialize the vulkan context.
    if (!InitVulkan())
	{
        printf("Could not initialize Vulkan context!\n");
        return false;
	}

    //Try to find a GPU, then check it for compatibility and create the devices and queues.
    if(!InitDevice())
    {
        printf("Could not initialize Vulkan devices and queues!\n");
        return false;
    }

    if(!InitMemoryAllocator())
    {
        printf("Could not initialize Memory Allocator.\n");
        return false;
    }

    //Swapchain used for presenting.
    if(!CreateSwapChain())
    {
        printf("Could not initialize Vulkan swap chain!\n");
        return false;
    }

    //The actual rendering pipeline related stuff.
    if(!InitPipeline())
    {
        printf("Could not initialize Vulkan pipeline!\n");
        return false;
    }

    m_Initialized = true;
	return true;
}

bool Renderer::CleanUp()
{
    if(!m_Initialized)
    {
        printf("Cannot cleanup renderer that was not initialized!\n");
        return false;
    }

    //Free all meshes.
    for(auto& mesh : m_Meshes)
    {
        //Destroy the buffers.
        vmaDestroyBuffer(m_Allocator, mesh->GetBuffer(), mesh->GetAllocation());
    }
    m_Meshes.clear();

    //TODO clean up textures, etc.

    //Pipeline related objects.
    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

    //Destroy the resources per frame.
    for(auto& frame : m_FrameData)
    {
        vkFreeCommandBuffers(m_Device, frame.m_CommandPool, 1, &frame.m_CommandBuffer);
        vkDestroyCommandPool(m_Device, frame.m_CommandPool, nullptr);
        vkDestroyFence(m_Device, frame.m_Fence, nullptr);
        vkDestroySemaphore(m_Device, frame.m_Semaphore, nullptr);
    }

    //Delete the allocated fragment and vertex shaders.
    vkDestroyShaderModule(m_Device, m_VertexShader, nullptr);
    vkDestroyShaderModule(m_Device, m_FragmentShader, nullptr);

    //Destroy all allocated memory.
    for(auto& view : m_SwapViews)
    {
        vkDestroyImageView(m_Device, view, nullptr);
    }

    vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);

    vkFreeCommandBuffers(m_Device, m_CopyCommandPool, 1, &m_CopyBuffer);
    vkDestroyCommandPool(m_Device, m_CopyCommandPool, nullptr);

    vkDestroySurfaceKHR(m_VulkanInstance, m_Surface, nullptr);

    vmaDestroyAllocator(m_Allocator);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroyInstance(m_VulkanInstance, nullptr);

    glfwDestroyWindow(m_Window);

    return true;
}

Renderer::Renderer() :
            m_Initialized(false),
            m_Window(nullptr),
            m_VulkanInstance(nullptr),
            m_PhysicalDevice(nullptr),
            m_Device(nullptr),
            m_Surface(nullptr),
            m_Queues{},
            m_SwapChain(nullptr),
            m_CopyBuffer(nullptr),
            m_CopyCommandPool(nullptr),
            m_Allocator(nullptr)
{
}

bool Renderer::Run()
{
    if(!m_Initialized)
    {
        printf("Renderer not initialized!\n");
        return false;
    }

    //Destroy unused meshes. Ensure that they are not in flight by keeping a reference of them in the renderer when on the GPU.
    auto itr = m_Meshes.begin();
    for(auto& mesh : m_Meshes)
    {
        if(mesh.use_count() == 1)
        {
            //Destroy the buffers.
            vmaDestroyBuffer(m_Allocator, mesh->GetBuffer(), mesh->GetAllocation());
            itr = m_Meshes.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

	return false;
}

std::shared_ptr<Mesh> Renderer::CreateMesh(const std::vector<Vertex>& a_VertexBuffer, const std::vector<std::uint32_t>& a_IndexBuffer)
{
    /*
     * Note: This function creates a staging buffer and fence, then deletes them again. Re-use would be much better in a real application.
     */

    //Calculate buffer size. Offset to be 16-byte aligned.
    const auto vertexSizeBytes = sizeof(Vertex) * a_VertexBuffer.size();
    const auto indexSizeBytes = sizeof(std::uint32_t) * a_IndexBuffer.size();

    //Ensure that the vertex buffer has size that aligns to 16 bytes. This is faster: (vertexSizeBytes + 15) & ~15, but adds together right away.
    const auto vertexPadding = (16 - (vertexSizeBytes % 16)) % 16;
    const auto bufferSize = vertexSizeBytes + vertexPadding + indexSizeBytes;
    const size_t vertexOffset = 0;
    const size_t indexOffset = vertexSizeBytes + vertexPadding;

    //Create the vertex buffer + index buffer. 
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    //Allocate some GPU-only memory.
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //The GPU buffer and memory now exist.
    VkBuffer buffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(m_Allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS)
    {
        printf("Error! Could not allocate memory for mesh.\n");
        return nullptr;
    }

    //Create a buffer on the GPU that can be copied into from the CPU.
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vmaCreateBuffer(m_Allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingBufferAllocation, nullptr) != VK_SUCCESS)
    {
        printf("Error! Could not allocate copy memory for mesh.\n");
        return nullptr;
    }

    //Retrieve information about the staging and GPU buffers (handles)
    VmaAllocationInfo stagingBufferInfo;
    VmaAllocationInfo gpuBufferInfo;
    vmaGetAllocationInfo(m_Allocator, stagingBufferAllocation, &stagingBufferInfo);
    vmaGetAllocationInfo(m_Allocator, allocation, &gpuBufferInfo);


    /*
     * Copy from CPU memory to CPU only memory on the GPU.
     */
    void* data;
    //Vertex
    vkMapMemory(m_Device, stagingBufferInfo.deviceMemory, 0, bufferSize, 0, &data);
    memcpy(data, a_VertexBuffer.data(), vertexSizeBytes);
    vkUnmapMemory(m_Device, stagingBufferInfo.deviceMemory);
    //Index
    vkMapMemory(m_Device, stagingBufferInfo.deviceMemory, indexOffset, bufferSize, 0, &data);
    memcpy(data, a_IndexBuffer.data(), indexSizeBytes);
    vkUnmapMemory(m_Device, stagingBufferInfo.deviceMemory);

    /*
     * Copy from the CPU memory to the GPU using a command buffer
     */

    //First reset the command pool. This automatically resets all associated buffers as well (if flag was not individual)
    vkResetCommandPool(m_Device, m_CopyCommandPool, 0);
    //
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.pNext = nullptr;

    if (vkBeginCommandBuffer(m_CopyBuffer, &beginInfo) != VK_SUCCESS) 
    {
        printf("Could not begin recording copy command buffer!\n");
        return nullptr;
    }

    //Specify which data to copy where.
    VkBufferCopy copyInfo;
    copyInfo.size = bufferSize;
    copyInfo.dstOffset = 0;
    copyInfo.srcOffset = 0;
    vkCmdCopyBuffer(m_CopyBuffer, stagingBuffer, buffer, 1, &copyInfo);

    //Stop recording.
    vkEndCommandBuffer(m_CopyBuffer);

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CopyBuffer;
    submitInfo.pNext = nullptr;
    submitInfo.pSignalSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.signalSemaphoreCount = 0;

    //Create a fence for synchronization.
    VkFence uploadFence;
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    fenceInfo.pNext = nullptr;
    vkCreateFence(m_Device, &fenceInfo, nullptr, &uploadFence);

    //Submit the work and then wait for the fence to be signaled.
    vkQueueSubmit(m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_TRANSFER)].m_Queue, 1, &submitInfo, uploadFence);
    vkWaitForFences(m_Device, 1, &uploadFence, true, std::numeric_limits<uint32_t>::max());

    //Free the staging buffer
    vmaDestroyBuffer(m_Allocator, stagingBuffer, stagingBufferAllocation);
    vkDestroyFence(m_Device, uploadFence, nullptr);

    //Finally create a shared pointer and return a copy of it after putting it in the registry.
    auto ptr = std::make_shared<Mesh>(allocation, buffer, a_IndexBuffer.size(), a_VertexBuffer.size(), indexOffset, vertexOffset);
    m_Meshes.push_back(ptr);
    return ptr;
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
        appInfo.pNext = nullptr;
    }

    //Information about the vulkan context.
    VkInstanceCreateInfo createInfo;
    VkDebugUtilsMessengerCreateInfoEXT debug;
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    {
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;   //0 by default, but can be enabled below.

        if (m_Settings.enableDebugMode)
        {
            //Add the debug callback extension and configure it.
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug.flags = 0;
            debug.pNext = nullptr;
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
    const auto initResult = vkCreateInstance(&createInfo, nullptr, &m_VulkanInstance);
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
        queueCreateInfo[i].pNext = nullptr;
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
        createInfo.pNext = nullptr;
        createInfo.flags = 0;

        //Create three queues as specified in the struct above (containing the right family index, and queue index is stored in an array).
        createInfo.queueCreateInfoCount = uniqueQueueIds.size();
        createInfo.pQueueCreateInfos = &queueCreateInfo[0];

        createInfo.pEnabledFeatures = nullptr;
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
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) 
    {
        printf("Could not create Vulkan logical device.\n");
        return false;
    }

    /*
     * Get the queues that were initialized for this device.
     * Store the family and queue indices as those are needed later.
     */
    for(int i = 0; i < 3; ++i)
    {
        QueueInfo info;
        info.m_FamilyIndex = queueFamIndex[i];
        info.m_QueueIndex = queueIndex[i];
        vkGetDeviceQueue(m_Device, queueFamIndex[i], queueIndex[i], &info.m_Queue);
        m_Queues[i] = info;
    }

    printf("Vulkan device and queues successfully initialized.\n");
    return true;
}

bool Renderer::InitMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device;
    allocatorInfo.instance = m_VulkanInstance;
    if(vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS)
    {
        printf("Vma could not be initialized.\n");
        return false;
    }

    return true;
}

bool Renderer::CreateSwapChain()
{
    //The surface data required for the swap chain.
    VkExtent2D swapExtent = { m_Settings.windowWidth, m_Settings.windowHeight };

    //This can be queried, but I don't do that because it's extra effort.
    //Not all surfaces support all formats and spaces.
    //I just force these, and if not supported I can always adjusted.
    VkSurfaceFormatKHR surfaceFormat;
    surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    surfaceFormat.format = m_Settings.m_OutputFormat;

    //Query the capabilities for the physical device and surface that were created earlier.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &surfaceCapabilities);

    //If max int is defined in the surface current extent, it means the swapchain can decide. If it is something else, that's the one that is required.
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) 
    {
        swapExtent = surfaceCapabilities.currentExtent;
    }

    /*
     * Create the swap chain images.
     */
    uint32_t swapBufferCount = surfaceCapabilities.minImageCount + 1;
    VkSwapchainCreateInfoKHR swapChainInfo;
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.pNext = NULL;
    swapChainInfo.flags = 0;
    swapChainInfo.surface = m_Surface;
    swapChainInfo.minImageCount = swapBufferCount;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = swapExtent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;         //Only one swapchain will have access.
    swapChainInfo.queueFamilyIndexCount = 0;                            //This is only set when sharing mode is set to concurrent.
    swapChainInfo.pQueueFamilyIndices = NULL;                           //Again only relevant when set to concurrent.
    swapChainInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapChainInfo.clipped = VK_TRUE;
    swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

    //Creat the swap chain.
    if (vkCreateSwapchainKHR(m_Device, &swapChainInfo, NULL, &m_SwapChain) != VK_SUCCESS) 
    {
        printf("Could not create SwapChain for Vulkan.\n");
        return false;
    }

    //Now query for the swap chains images and initialize them as render targets.
    std::vector<VkImage> swapBuffers;
    uint32_t bufferCount;
    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &bufferCount, NULL);
    swapBuffers.resize(bufferCount);
    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &bufferCount, swapBuffers.data());

    //Create the views for the swap chain.
    m_SwapViews.resize(swapBuffers.size());
    
    VkImageViewCreateInfo createInfo;
    {
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = surfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
    }

    //Loop over each swapchain buffer, and create an image view for it.
    for (size_t i = 0; i < swapBuffers.size(); i++) 
    {
        createInfo.image = swapBuffers[i];

        if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapViews[i]) != VK_SUCCESS) 
        {
            printf("Could not create image view for swap chain!\n");
            return false;
        }
    }

    printf("SwapChain successfully created.\n");

    return true;
}

bool Renderer::InitPipeline()
{
    /*
     * Setup the copy command buffer and pool.
     * These are used to copy data to the GPU.
     */
    VkCommandPoolCreateInfo copyPoolInfo;
    copyPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    copyPoolInfo.pNext = nullptr;
    copyPoolInfo.flags = 0;
    copyPoolInfo.queueFamilyIndex = m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_TRANSFER)].m_FamilyIndex;
    if (vkCreateCommandPool(m_Device, &copyPoolInfo, nullptr, &m_CopyCommandPool) != VK_SUCCESS)
    {
        printf("Could not create copy command pool!\n");
        return false;
    }

    //Command buffers are tightly bound to their queues and pools.
    VkCommandBufferAllocateInfo copyCommandBufferInfo;
    copyCommandBufferInfo.commandBufferCount = 1;
    copyCommandBufferInfo.commandPool = m_CopyCommandPool;
    copyCommandBufferInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    copyCommandBufferInfo.pNext = nullptr;
    copyCommandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkAllocateCommandBuffers(m_Device, &copyCommandBufferInfo, &m_CopyBuffer);

    /*
     * Set up the resources for each frame.
     */
    for(auto frameIndex = 0; frameIndex < NUM_FRAMES; ++frameIndex)
    {
        auto& frameData = m_FrameData[frameIndex];

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.queueFamilyIndex = m_Queues[(unsigned)QueueType::QUEUE_TYPE_GRAPHICS].m_FamilyIndex;
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = 0;

        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &frameData.m_CommandPool) != VK_SUCCESS)
        {
            printf("Could not create graphics command pool for frame index %i!\n", frameIndex);
            return false;
        }

        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandBufferCount = 1;
        bufferInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferInfo.commandPool = frameData.m_CommandPool;

        if (vkAllocateCommandBuffers(m_Device, &bufferInfo, &frameData.m_CommandBuffer) != VK_SUCCESS)
        {
            printf("Could not create graphics command buffer for frame index %i!\n", frameIndex);
            return false;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;  //Signal by default so that the frame is marked as available.
        if(vkCreateFence(m_Device, &fenceInfo, nullptr, &frameData.m_Fence) != VK_SUCCESS)
        {
            printf("Could not create fence for frame index %i!\n", frameIndex);
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &frameData.m_Semaphore) != VK_SUCCESS)
        {
            printf("Could not create fence for frame index %i!\n", frameIndex);
            return false;
        }
    }

    /*
     * Load the Spir-V shaders from disk.
     */
    const std::string workingDir = std::filesystem::current_path().string();

    if(!CreateShaderModuleFromSpirV(workingDir + "/shaders/output/default.vert.spv", m_VertexShader) || !CreateShaderModuleFromSpirV(workingDir + "/shaders/output/default.frag.spv", m_FragmentShader))
    {
        printf("Could not load fragment or vertex shader from Spir-V.\n");
        return false;
    }

    /*
     * Create a graphics pipeline state object.
     * This object requires all state like blending and shaders used to be bound.
     */

    //Add the shaders to the pipeline
    VkPipelineShaderStageCreateInfo vertexStage{};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = m_VertexShader;
    vertexStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = m_FragmentShader;
    fragmentStage.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexStage, fragmentStage };

    //Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInfo.vertexBindingDescriptionCount = 0;
    vertexInfo.vertexAttributeDescriptionCount = 0; //TODO
    vertexInfo.pVertexBindingDescriptions = nullptr;
    vertexInfo.pVertexAttributeDescriptions = nullptr;

    //Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;   //TODO: maybe different kinds?
    inputAssembly.primitiveRestartEnable = false;

    //Viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_Settings.windowWidth;
    viewport.height = (float)m_Settings.windowHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = VkExtent2D{m_Settings.windowWidth, m_Settings.windowHeight};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    //Rasterizer state
    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.depthClampEnable = VK_FALSE;         //Clamp instead of discard would be nice for shadow mapping
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;  //Disables all input, not sure when this would be useful, maybe when switching menus and not rendering the main scene?
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;  //Can draw as lines which is really awesome for debugging and making people think I'm hackerman
    rasterizationState.lineWidth = 1.0f;                    //Line width in case of hackerman mode
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;    //Same as GL
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE; //Same as GL
    rasterizationState.depthBiasEnable = VK_FALSE;          //Useful for shadow mapping to prevent acne.
    rasterizationState.depthBiasConstantFactor = 0.0f;      //^    
    rasterizationState.depthBiasClamp = 0.0f;               //^
    rasterizationState.depthBiasSlopeFactor = 0.0f;         //^

    //Multi-sampling which nobody really uses anymore anyways. TAA all the way!
    VkPipelineMultisampleStateCreateInfo multiSampleState{};
    multiSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multiSampleState.sampleShadingEnable = VK_FALSE;
    multiSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multiSampleState.minSampleShading = 1.0f;
    multiSampleState.pSampleMask = nullptr;
    multiSampleState.alphaToCoverageEnable = VK_FALSE;
    multiSampleState.alphaToOneEnable = VK_FALSE;

    //The depth state. Stencil is not used for now.
    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = true;
    depthStencilState.stencilTestEnable = false;
    depthStencilState.depthBoundsTestEnable = false;

    //Color blending.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;
    colorBlendState.blendConstants[0] = 0.0f;

    //The pipeline layout.  //TODO actual descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
    {
        printf("Could not create pipeline layout for rendering pipeline!\n");
        return false;
    }

    //The render pass.
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_Settings.m_OutputFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;  //Attachment 0 means that writing to layout location 0 in the fragment shader will affect this attachment.
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) 
    {
        printf("Could not create render pass for pipeline!\n");
        return false;
    }


    /*
     * Add all these together in the final pipeline state info struct.
     */
    VkGraphicsPipelineCreateInfo psoInfo{}; //Initializer list default inits pointers and numeric variables to 0! Vk structs don't have a default constructor.
    psoInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    psoInfo.pNext = nullptr;
    psoInfo.flags = 0;

    //Add the data to the pipeline.
    psoInfo.stageCount = 2;
    psoInfo.pStages = &shaderStages[0];
    psoInfo.pVertexInputState = &vertexInfo;
    psoInfo.pInputAssemblyState = &inputAssembly;
    psoInfo.pViewportState = &viewportState;
    psoInfo.pRasterizationState = &rasterizationState;
    psoInfo.pMultisampleState = &multiSampleState;
    psoInfo.pDepthStencilState = &depthStencilState;
    psoInfo.pColorBlendState = &colorBlendState;
    psoInfo.layout = m_PipelineLayout;
    psoInfo.renderPass = m_RenderPass;
    psoInfo.subpass = 0;

    psoInfo.pDynamicState = nullptr;
    psoInfo.basePipelineHandle = nullptr;
    psoInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &psoInfo, nullptr, &m_Pipeline) != VK_SUCCESS) 
    {
        printf("Could not create graphics pipeline!\n");
        return false;
    }

    printf("Succesfully created graphics pipeline!\n");
    return true;
}

bool Renderer::ReadFile(const std::string& a_File, std::vector<char>& a_Output)
{
    std::ifstream fileStream(a_File, std::ios::binary | std::ios::ate); //Ate will start the file pointer at the back, so tellg will return the size of the file.
    if(fileStream.is_open())
    {
        const auto size = fileStream.tellg();
        fileStream.seekg(0); //Reset to start of the file.
        a_Output.resize(size);
        fileStream.read(&a_Output[0], size);
        fileStream.close();
        return true;
    }
    return false;
}

bool Renderer::CreateShaderModuleFromSpirV(const std::string& a_File, VkShaderModule& a_Output)
{
    std::vector<char> byteCode;
    if(!ReadFile(a_File, byteCode))
    {
        printf("Could not load shader %s from Spir-V file.\n", a_File.c_str());
        return false;
    }

    VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = byteCode.size();
    shaderCreateInfo.pNext = nullptr;
    shaderCreateInfo.flags = 0;
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());
    if (vkCreateShaderModule(m_Device, &shaderCreateInfo, nullptr, &a_Output) != VK_SUCCESS)
    {
        printf("Could not convert Spir-V to shader module for file %s!\n", a_File.c_str());
        return false;
    }

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
