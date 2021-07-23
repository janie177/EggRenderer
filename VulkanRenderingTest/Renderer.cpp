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
	if(m_Initialized)
	{
        printf("Cannot initialize renderer: already initialized!\n");
        return false;
	}

	/*
	 * Ensure that the settings make sense.
	 */
    assert(a_Settings.resolutionX > 0);
    assert(a_Settings.resolutionY > 0);
    assert(a_Settings.resolutionX <= 1000000);
    assert(a_Settings.resolutionY <= 1000000);
    assert(a_Settings.m_SwapBufferCount > 0);
    assert(a_Settings.m_SwapBufferCount < 100);
	
    m_RenderData.m_Settings = a_Settings;
    m_FrameCounter = 0;
    m_MeshCounter = 0;

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = glfwCreateWindow(a_Settings.resolutionX, a_Settings.resolutionY, "Vulkan Render Test", nullptr, nullptr);


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
	
    //Create the render targets for the pipeline.
	if(!CreateSwapChainFrameData())
	{
        printf("Could not initialize render targets for swap chain!\n");
        return false;
	}

	//Finally acquire the first frame index to render into.
    if (!AcquireSwapChainIndex())
    {
        printf("Could not acquire first frame index from swap chain!");
        return false;
    }
	
    m_Initialized = true;
	return true;
}

bool Renderer::Resize(std::uint32_t a_Width, std::uint32_t a_Height)
{
	//If resizing to the same size, just don't do anything.
    if(a_Width == m_RenderData.m_Settings.resolutionX && a_Height == m_RenderData.m_Settings.resolutionY)
    {
        return true;
    }

	//Resize the GLFW window.
    glfwSetWindowSize(m_Window, a_Width, a_Height);
	
    //Wait for the pipeline to finish before molesting all the objects.
    for (auto& frame : m_RenderData.m_FrameData)
    {
        vkWaitForFences(m_RenderData.m_Device, 1, &frame.m_Fence, true, std::numeric_limits<uint32_t>::max());
    }
	
	/*
	 * Update the settings.
	 */
    m_RenderData.m_Settings.resolutionX = a_Width;
    m_RenderData.m_Settings.resolutionY = a_Height;

	//Destroy all the render stages as those were created last.
    for(int i = m_RenderStages.size() - 1; i >= 0; --i)
    {
		if(!m_RenderStages[i]->CleanUp(m_RenderData))
		{
            printf("Could not clean up renderstage during resize!\n");
            return false;
		}
    }

    //Destroy the swap chain and frame buffers.
    if (!CleanUpSwapChain())
    {
        printf("Could not clean up swap chain and frame buffers during resize!\n");
        return false;
    }

	//Make a new swap chain.
    if (!CreateSwapChain())
    {
        printf("Could not init swap chain during resize!\n");
        return false;
    }

	//Initialize all the render stages.
	for(auto& stage : m_RenderStages)
	{
        if(!stage->Init(m_RenderData))
        {
            printf("Could not init renderstage during resize!\n");
            return false;
        }
	}

	//Create the frame buffers and semaphores/fences.
	//This happens after the render stages because a render pass has to be defined by the last stage.
	//This pass is then passed into the FBO create struct.
    if(!CreateSwapChainFrameData())
    {
        printf("Could not init frame buffers during resize.\n");
        return false;
    }

	//Lastly retrieve the first frame's index.
	if(!AcquireSwapChainIndex())
	{
        printf("Could not acquire swap chain index for next frame during resize!\n");
        return false;
	}
	
    return true;
}

bool Renderer::CleanUp()
{
    if(!m_Initialized)
    {
        printf("Cannot cleanup renderer that was not initialized!\n");
        return false;
    }

	//Wait for the pipeline to finish.
	for(auto& frame : m_RenderData.m_FrameData)
	{
        vkWaitForFences(m_RenderData.m_Device, 1, &frame.m_Fence, true, std::numeric_limits<uint32_t>::max());
	}

	//Unload all meshes.
    m_Meshes.RemoveAll([&](Mesh& a_Mesh)
    {
		vmaDestroyBuffer(m_RenderData.m_Allocator, a_Mesh.GetBuffer(), a_Mesh.GetAllocation());
    });

    //TODO render stage and textures

	/*
	 * Clean up the render stages.
	 * This happens in reverse order.
	 */
    for(int i = m_RenderStages.size() - 1; i >= 0; --i)
    {
        m_RenderStages[i]->CleanUp(m_RenderData);
    }

    //Destroy the resources per frame.
    for(auto& frame : m_RenderData.m_FrameData)
    {
        vkFreeCommandBuffers(m_RenderData.m_Device, frame.m_CommandPool, 1, &frame.m_CommandBuffer);
        vkDestroyCommandPool(m_RenderData.m_Device, frame.m_CommandPool, nullptr);
    }

	//Clean the swapchain and associated frame buffers.
    CleanUpSwapChain();

    vkFreeCommandBuffers(m_RenderData.m_Device, m_CopyCommandPool, 1, &m_CopyBuffer);
    vkDestroyCommandPool(m_RenderData.m_Device, m_CopyCommandPool, nullptr);

    vkDestroySurfaceKHR(m_RenderData.m_VulkanInstance, m_RenderData.m_Surface, nullptr);

    vmaDestroyAllocator(m_RenderData.m_Allocator);
    vkDestroyDevice(m_RenderData.m_Device, nullptr);
    vkDestroyInstance(m_RenderData.m_VulkanInstance, nullptr);

    glfwDestroyWindow(m_Window);
	
    return true;
}

Renderer::Renderer() :
	m_Initialized(false),
	m_FrameCounter(0),
	m_MeshCounter(0),
	m_Window(nullptr),
	m_SwapChain(nullptr),
	m_CopyBuffer(nullptr),
	m_CopyCommandPool(nullptr),
	m_CurrentFrameIndex(0),
	m_FrameReadySemaphore(nullptr),
	m_HelloTriangleStage(nullptr),
	m_DeferredStage(nullptr)
{
}

bool Renderer::DrawFrame(const DrawData& a_DrawData)
{
    //Ensure that the renderer has been properly set-up.
    if(!m_Initialized)
    {
        printf("Renderer not initialized!\n");
        return false;
    }

    if(a_DrawData.m_Camera == nullptr || a_DrawData.m_pDrawCalls == nullptr)
    {
        printf("A valid camera and draw call pointer has to be specified for DrawData!\n");
        return false;
    }

    //The frame data for the current frame.
    auto& frameData = m_RenderData.m_FrameData[m_CurrentFrameIndex];
    auto& cmdBuffer = frameData.m_CommandBuffer;

    //Ensure that command buffer execution is done for this frame by waiting for fence completion.
    vkWaitForFences(m_RenderData.m_Device, 1, &frameData.m_Fence, true, std::numeric_limits<std::uint32_t>::max());

    //Reset the fence now that it has been signaled.
    vkResetFences(m_RenderData.m_Device, 1, &frameData.m_Fence);

    //Prepare the command buffer for rendering
    vkResetCommandPool(m_RenderData.m_Device, frameData.m_CommandPool, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if(vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
    {
        printf("Could not fill command buffer!\n");
        return false;
    }

	//All semapores the command buffer should wait for and signal.
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkSemaphore> signalSemaphores;
    std::vector<VkPipelineStageFlags> waitStageFlags;       //The stages the wait semaphores should wait in.

    /*
     * Setup data for each stage depending on what has been provided this frame.
     */
    m_DeferredStage->SetDrawData(a_DrawData);
	
    /*
     * Execute all the render stages.
     */
	for(auto& stage : m_RenderStages)
	{
		if(stage->IsEnabled())
		{
            //These functions may add waiting dependencies to the semaphore vectors.
            stage->RecordCommandBuffer(m_RenderData, cmdBuffer, m_CurrentFrameIndex, waitSemaphores, signalSemaphores, waitStageFlags);
		}
	}

	/*
	 * Finally end the command list and submit it.
	 */
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
    {
        printf("Could not end recording of command buffer!\n");
        return false;
    }

	//Ensure that the command buffer waits for the frame to be ready, and signals to the swapchain that it's ready to be presented.
    signalSemaphores.push_back(frameData.m_WaitForRenderSemaphore);
    waitSemaphores.push_back(m_FrameReadySemaphore);
    waitStageFlags.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);    //Last added semaphore should wait before outputting any data.

	//Ensure that the semaphore buffers are the right size.
	if(waitStageFlags.size() != waitSemaphores.size())
	{
        printf("Error: wait semaphores and wait stages do not match in size. Every wait needs a stage defined!\n");
        return false;
	}

    //Submit the command queue. Signal the fence once done.
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameData.m_CommandBuffer;
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();                             //Signal this semaphore when done so that the image is presented.
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();                                 //Wait for this semaphore so that the frame buffer is actually available when the rendering happens.
    submitInfo.pWaitDstStageMask = waitStageFlags.data();
	
    vkQueueSubmit(m_RenderData.m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_GRAPHICS)].m_Queue, 1, &submitInfo, frameData.m_Fence);

    //Start building the command buffer.
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameData.m_WaitForRenderSemaphore;  //Wait for the command buffer to stop executing before presenting.
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_SwapChain;
    presentInfo.pImageIndices = &m_CurrentFrameIndex;
    presentInfo.pResults = nullptr;
    vkQueuePresentKHR(m_RenderData.m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_GRAPHICS)].m_Queue, &presentInfo);

    //Every time a full swap chain has been consumed, remove unused resources.
	if(m_FrameCounter % m_RenderData.m_Settings.m_SwapBufferCount == 0)
	{
        m_Meshes.RemoveUnused(
            [&](Mesh& a_Mesh) -> bool
	        {
            	//If the resource has not been used in the last full swap-chain cylce, it's not in flight.
            	if(m_CurrentFrameIndex - a_Mesh.m_LastUsedFrameId > m_RenderData.m_Settings.m_SwapBufferCount)
            	{
                    vmaDestroyBuffer(m_RenderData.m_Allocator, a_Mesh.GetBuffer(), a_Mesh.GetAllocation());
                    return true;
            	}
                return false;
	        }
        );
	}


    /*
     * Retrieve the next available frame index.
     * The semaphore will be signaled as soon as the frame becomes available.
     * Remember it for the next frame, to be used with the queue submit command.
     */
    vkAcquireNextImageKHR(m_RenderData.m_Device, m_SwapChain, std::numeric_limits<unsigned>::max(), frameData.m_WaitForFrameSemaphore, nullptr, &m_CurrentFrameIndex);
    m_FrameReadySemaphore = frameData.m_WaitForFrameSemaphore;

	//Increment the frame index.
    ++m_FrameCounter;
	
	return true;
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
    if (vmaCreateBuffer(m_RenderData.m_Allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS)
    {
        printf("Error! Could not allocate memory for mesh.\n");
        return nullptr;
    }

    //Create a buffer on the GPU that can be copied into from the CPU.
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vmaCreateBuffer(m_RenderData.m_Allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingBufferAllocation, nullptr) != VK_SUCCESS)
    {
        printf("Error! Could not allocate copy memory for mesh.\n");
        return nullptr;
    }

    //Retrieve information about the staging and GPU buffers (handles)
    VmaAllocationInfo stagingBufferInfo;
    VmaAllocationInfo gpuBufferInfo;
    vmaGetAllocationInfo(m_RenderData.m_Allocator, stagingBufferAllocation, &stagingBufferInfo);
    vmaGetAllocationInfo(m_RenderData.m_Allocator, allocation, &gpuBufferInfo);


    /*
     * Copy from CPU memory to CPU only memory on the GPU.
     */
    void* data;
    //Vertex
    vkMapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory, 0, bufferSize, 0, &data);
    memcpy(data, a_VertexBuffer.data(), vertexSizeBytes);
    vkUnmapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory);
    //Index
    vkMapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory, indexOffset, bufferSize, 0, &data);
    memcpy(data, a_IndexBuffer.data(), indexSizeBytes);
    vkUnmapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory);

    /*
     * Copy from the CPU memory to the GPU using a command buffer
     */

    //First reset the command pool. This automatically resets all associated buffers as well (if flag was not individual)
    vkResetCommandPool(m_RenderData.m_Device, m_CopyCommandPool, 0);
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
    VkBufferCopy copyInfo{};
    copyInfo.size = bufferSize;
    copyInfo.dstOffset = 0;
    copyInfo.srcOffset = 0;
    vkCmdCopyBuffer(m_CopyBuffer, stagingBuffer, buffer, 1, &copyInfo);

    //Stop recording.
    vkEndCommandBuffer(m_CopyBuffer);

    VkSubmitInfo submitInfo{};
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
    VkFence uploadFence{};
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    fenceInfo.pNext = nullptr;
    vkCreateFence(m_RenderData.m_Device, &fenceInfo, nullptr, &uploadFence);

    //Submit the work and then wait for the fence to be signaled.
    vkQueueSubmit(m_RenderData.m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_TRANSFER)].m_Queue, 1, &submitInfo, uploadFence);
    vkWaitForFences(m_RenderData.m_Device, 1, &uploadFence, true, std::numeric_limits<uint32_t>::max());

    //Free the staging buffer
    vmaDestroyBuffer(m_RenderData.m_Allocator, stagingBuffer, stagingBufferAllocation);
    vkDestroyFence(m_RenderData.m_Device, uploadFence, nullptr);

    //Finally create a shared pointer and return a copy of it after putting it in the registry.
    auto ptr = std::make_shared<Mesh>(m_MeshCounter, allocation, buffer, a_IndexBuffer.size(), a_VertexBuffer.size(), indexOffset, vertexOffset);
    m_Meshes.Add(ptr);

    ++m_MeshCounter;
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
        appInfo.apiVersion = VK_API_VERSION_1_2;
        appInfo.pNext = nullptr;
    }

    //Information about the vulkan context.
    VkInstanceCreateInfo createInfo{};
    VkDebugUtilsMessengerCreateInfoEXT debug;
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    {
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;   //0 by default, but can be enabled below.

        if (m_RenderData.m_Settings.enableDebugMode)
        {
            //Add the debug callback extension and configure it.
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug.flags = 0;
            debug.pNext = nullptr;
            debug.messageSeverity = static_cast<uint32_t>(m_RenderData.m_Settings.debugFlags);    //These flags correspond to the Vk Enumeration.
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;                      //Print everything basically.
            debug.pfnUserCallback = debugCallback;
            debug.pUserData = nullptr;
            createInfo.pNext = &debug;

        	/*
        	 * Ensure that the validation layers are actually supported.
        	 * Mostly taken from Vulkan-Tutorial.com
        	 */
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            auto itr = validationLayers.begin();
            while (itr != validationLayers.end()) 
            {
                bool layerFound = false;

                for (const auto& layerProperties : availableLayers) 
                {
                    if (strcmp(*itr, layerProperties.layerName) == 0) 
                    {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound) 
                {
                    printf("Could not find layer: %s. Skipping layer addition.\n", *itr);
                    itr = validationLayers.erase(itr);
                }
                else 
                {
                    ++itr;
                }
            }

            //Set the layers and pass data pointers.
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = createInfo.enabledLayerCount == 0 ? nullptr : validationLayers.data();
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = createInfo.enabledExtensionCount == 0 ? nullptr : extensions.data();
    }

    //Create the actual instance.
    const auto initResult = vkCreateInstance(&createInfo, nullptr, &m_RenderData.m_VulkanInstance);
    if (initResult != VK_SUCCESS)
    {
        printf("Could not create Vulkan instance. Cause: %u\n", initResult);
        return false;
    }

    printf("Vulkan instance succesfully created.\n");

    /*
     * Bind GLFW and Vulkan.
     */
    const auto result = glfwCreateWindowSurface(m_RenderData.m_VulkanInstance, m_Window, NULL, &m_RenderData.m_Surface);
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
    vkEnumeratePhysicalDevices(m_RenderData.m_VulkanInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        printf("No physical GPU found.\n");
        return false;
    }

    // Retrieve all the devices available on this PC.
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_RenderData.m_VulkanInstance, &deviceCount, devices.data());

    /*
     * Note to self:
     * So it appears that in Vulkan you have full control over the queues.
     * First you query the hardware for the supported queues, and then you can create virtual queues on the logical device from their indices.
     * So ideally, I'd make a function that lets me specify which GPU I want, how many copy/compute/graphics queues I'd want.
     * Then I guess the function would create these queue objects if they are valid for the GPU selected.
     */

    if (deviceCount <= m_RenderData.m_Settings.gpuIndex)
    {
        printf("Invalid GPU index specified in renderer settings. Not that many devices.\n");
        return false;
    }

    auto& device = devices[m_RenderData.m_Settings.gpuIndex];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);   //So this has to be called to allocate the space...
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());  //And then called again to actually write to it. ..... .. ... 

    //Assign the device.
    m_RenderData.m_PhysicalDevice = device;

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
            vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIdx, m_RenderData.m_Surface, &presentSupport);
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

    if (m_RenderData.m_PhysicalDevice == VK_NULL_HANDLE)
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

        if (m_RenderData.m_Settings.enableDebugMode)
        {
            // To have device level validation information, the layers are added here.
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
    }

    //Create the logical device instance.
    if (vkCreateDevice(m_RenderData.m_PhysicalDevice, &createInfo, nullptr, &m_RenderData.m_Device) != VK_SUCCESS)
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
        vkGetDeviceQueue(m_RenderData.m_Device, queueFamIndex[i], queueIndex[i], &info.m_Queue);
        m_RenderData.m_Queues[i] = info;
    }

    printf("Vulkan device and queues successfully initialized.\n");
    return true;
}

bool Renderer::InitMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = m_RenderData.m_PhysicalDevice;
    allocatorInfo.device = m_RenderData.m_Device;
    allocatorInfo.instance = m_RenderData.m_VulkanInstance;
    if(vmaCreateAllocator(&allocatorInfo, &m_RenderData.m_Allocator) != VK_SUCCESS)
    {
        printf("Vma could not be initialized.\n");
        return false;
    }

    return true;
}

bool Renderer::AcquireSwapChainIndex()
{
    //Assign the frame index to be 0
    m_FrameReadySemaphore = m_RenderData.m_FrameData[m_RenderData.m_Settings.m_SwapBufferCount - 1].m_WaitForFrameSemaphore;   //Semaphore for the frame before this is used.
    vkAcquireNextImageKHR(m_RenderData.m_Device, m_SwapChain, std::numeric_limits<unsigned>::max(), m_FrameReadySemaphore, nullptr, &m_CurrentFrameIndex);
    if (m_CurrentFrameIndex != 0)
    {
        printf("First frame index is not 0! This doesn't work with my setup.\n");
        return false;
    }

    return true;
}

bool Renderer::CreateSwapChain()
{
    //The surface data required for the swap chain.
    VkExtent2D swapExtent = { m_RenderData.m_Settings.resolutionX, m_RenderData.m_Settings.resolutionY };

    //This can be queried, but I don't do that because it's extra effort.
    //Not all surfaces support all formats and spaces.
    //I just force these, and if not supported I can always adjusted.
    VkSurfaceFormatKHR surfaceFormat;
    surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    surfaceFormat.format = m_RenderData.m_Settings.outputFormat;

    //Query the capabilities for the physical device and surface that were created earlier.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_RenderData.m_PhysicalDevice, m_RenderData.m_Surface, &surfaceCapabilities);

    //If max int is defined in the surface current extent, it means the swapchain can decide. If it is something else, that's the one that is required.
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) 
    {
        swapExtent = surfaceCapabilities.currentExtent;

    	//Also overwrite the settings because it's not supported.
        m_RenderData.m_Settings.resolutionX = swapExtent.width;
        m_RenderData.m_Settings.resolutionY = swapExtent.height;
    }

    /*
     * Create the swap chain images.
     */
    uint32_t swapBufferCount = surfaceCapabilities.minImageCount + 1;
    swapBufferCount = std::max(swapBufferCount, m_RenderData.m_Settings.m_SwapBufferCount);
    swapBufferCount = std::min(surfaceCapabilities.maxImageCount, swapBufferCount);
    m_RenderData.m_Settings.m_SwapBufferCount = swapBufferCount;
	
    VkSwapchainCreateInfoKHR swapChainInfo;
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.pNext = NULL;
    swapChainInfo.flags = 0;
    swapChainInfo.surface = m_RenderData.m_Surface;
    swapChainInfo.minImageCount = swapBufferCount;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = swapExtent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;         //This has to be concurrent when the present queue is different from graphics queue.
    swapChainInfo.queueFamilyIndexCount = 0;                            //This is only set when sharing mode is set to concurrent.
    swapChainInfo.pQueueFamilyIndices = NULL;                           //Again only relevant when set to concurrent.
    swapChainInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode = m_RenderData.m_Settings.vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    swapChainInfo.clipped = VK_TRUE;
    swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

    //Create the swap chain.
    if (vkCreateSwapchainKHR(m_RenderData.m_Device, &swapChainInfo, NULL, &m_SwapChain) != VK_SUCCESS)
    {
        printf("Could not create SwapChain for Vulkan.\n");
        return false;
    }

    //Now query for the swap chains images and initialize them as render targets.
    std::vector<VkImage> swapBuffers;
    uint32_t bufferCount;
    vkGetSwapchainImagesKHR(m_RenderData.m_Device, m_SwapChain, &bufferCount, NULL);
    swapBuffers.resize(bufferCount);
    vkGetSwapchainImagesKHR(m_RenderData.m_Device, m_SwapChain, &bufferCount, swapBuffers.data());
    
    VkImageViewCreateInfo createInfo;
    {
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = surfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;           //Note: Swapchain format is BGRA so swap B and R.
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
    }

    //Resize the render data vector to contain an entry for each frame.
    m_RenderData.m_FrameData.resize(m_RenderData.m_Settings.m_SwapBufferCount);

    //Loop over each swapchain buffer, and create an image view for it.
    for (size_t i = 0; i < swapBuffers.size(); i++) 
    {
        createInfo.image = swapBuffers[i];

        if (vkCreateImageView(m_RenderData.m_Device, &createInfo, nullptr, &m_RenderData.m_FrameData[i].m_SwapchainView) != VK_SUCCESS)
        {
            printf("Could not create image view for swap chain!\n");
            return false;
        }
    }

    printf("SwapChain successfully created.\n");

    return true;
}

bool Renderer::CreateSwapChainFrameData()
{
	for(int frameIndex = 0; frameIndex < m_RenderData.m_Settings.m_SwapBufferCount; ++frameIndex)
	{
        auto& frameData = m_RenderData.m_FrameData[frameIndex];
        VkImageView attachments[] = { m_RenderData.m_FrameData[frameIndex].m_SwapchainView };    //image view corresponding with the swapchain index for this frame.

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;  //Signal by default so that the frame is marked as available.
        if (vkCreateFence(m_RenderData.m_Device, &fenceInfo, nullptr, &frameData.m_Fence) != VK_SUCCESS)
        {
            printf("Could not create fence for frame!\n");
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(m_RenderData.m_Device, &semaphoreInfo, nullptr, &frameData.m_WaitForFrameSemaphore) != VK_SUCCESS || vkCreateSemaphore(m_RenderData.m_Device, &semaphoreInfo, nullptr, &frameData.m_WaitForRenderSemaphore) != VK_SUCCESS)
        {
            printf("Could not create semaphore for frame!\n");
            return false;
        }
    }

    return true;
}

bool Renderer::CleanUpSwapChain()
{
    //Destroy frame buffers and such. Also synchronization objects.
    for (auto& frame : m_RenderData.m_FrameData)
    {
        vkDestroyFence(m_RenderData.m_Device, frame.m_Fence, nullptr);
        vkDestroySemaphore(m_RenderData.m_Device, frame.m_WaitForFrameSemaphore, nullptr);
        vkDestroySemaphore(m_RenderData.m_Device, frame.m_WaitForRenderSemaphore, nullptr);
        vkDestroyImageView(m_RenderData.m_Device, frame.m_SwapchainView, nullptr);
    }

    vkDestroySwapchainKHR(m_RenderData.m_Device, m_SwapChain, nullptr);
	
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
    copyPoolInfo.queueFamilyIndex = m_RenderData.m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_TRANSFER)].m_FamilyIndex;
    if (vkCreateCommandPool(m_RenderData.m_Device, &copyPoolInfo, nullptr, &m_CopyCommandPool) != VK_SUCCESS)
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
    vkAllocateCommandBuffers(m_RenderData.m_Device, &copyCommandBufferInfo, &m_CopyBuffer);

    /*
     * Add all the stages to the stage buffer.
     */
    //m_HelloTriangleStage = AddRenderStage(std::make_unique<RenderStage_HelloTriangle>());
    m_DeferredStage = AddRenderStage(std::make_unique<RenderStage_Deferred>());   //TODO
	
    /*
     * Init the render stages for each frame.
     */
	for(auto& stage : m_RenderStages)
	{
        stage->Init(m_RenderData);
	}

    /*
     * Set up the resources for each frame.
     */
    for (auto frameIndex = 0u; frameIndex < m_RenderData.m_Settings.m_SwapBufferCount; ++frameIndex)
    {
        auto& frameData = m_RenderData.m_FrameData[frameIndex];

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.queueFamilyIndex = m_RenderData.m_Queues[static_cast<unsigned>(QueueType::QUEUE_TYPE_GRAPHICS)].m_FamilyIndex;
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = 0;

        if (vkCreateCommandPool(m_RenderData.m_Device, &poolInfo, nullptr, &frameData.m_CommandPool) != VK_SUCCESS)
        {
            printf("Could not create graphics command pool for frame index %i!\n", frameIndex);
            return false;
        }

        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandBufferCount = 1;
        bufferInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferInfo.commandPool = frameData.m_CommandPool;

        if (vkAllocateCommandBuffers(m_RenderData.m_Device, &bufferInfo, &frameData.m_CommandBuffer) != VK_SUCCESS)
        {
            printf("Could not create graphics command buffer for frame index %i!\n", frameIndex);
            return false;
        }
    }

    printf("Successfully created graphics pipeline!\n");
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
