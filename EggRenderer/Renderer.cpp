#include "Renderer.h"

#include <cstdio>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <glm/glm/glm.hpp>

#include "vk_mem_alloc.h"

namespace egg
{

    bool Renderer::Init(const RendererSettings& a_Settings)
    {
	    if(m_Initialized)
	    {
            printf("Cannot initialize renderer: already initialized!\n");
            return false;
	    }

        m_LastMousePos = glm::vec2(a_Settings.resolutionX / 2.f, a_Settings.resolutionY / 2.f);

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

        //Make the window in either full screen or windowed mode.
        auto* mainMonitor = glfwGetPrimaryMonitor();
        auto* videoMode = glfwGetVideoMode(mainMonitor);
        if (a_Settings.fullScreen)
        {
            m_FullScreenResolution = { videoMode->width, videoMode->height };
            m_Window = glfwCreateWindow(videoMode->width, videoMode->height, a_Settings.windowName.c_str(), mainMonitor, nullptr);
        }
        else
        {
            m_FullScreenResolution = { 0, 0 };
            m_Window = glfwCreateWindow(a_Settings.resolutionX, a_Settings.resolutionY, a_Settings.windowName.c_str(), nullptr, nullptr);
        }
        if(a_Settings.lockCursor)
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }


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

    bool Renderer::Resize(bool a_FullScreen, std::uint32_t a_Width, std::uint32_t a_Height)
    {
	    //If resizing to the same size, just don't do anything.
        if(a_Width == m_RenderData.m_Settings.resolutionX && a_Height == m_RenderData.m_Settings.resolutionY && a_FullScreen == m_RenderData.m_Settings.fullScreen)
        {
            return true;
        }
	    
        //Wait for the pipeline to finish before molesting all the objects.
        for (auto& frame : m_RenderData.m_FrameData)
        {
            vkWaitForFences(m_RenderData.m_Device, 1, &frame.m_Fence, true, std::numeric_limits<uint32_t>::max());
        }
        //Stages may have frame-independent stuff going on too.
        for(auto& stage : m_RenderStages)
        {
            stage->WaitForIdle(m_RenderData);
        }

        //Resize the GLFW window.
        glfwSetWindowSize(m_Window, a_Width, a_Height);
        auto* mainMonitor = glfwGetPrimaryMonitor();
        auto* videoMode = glfwGetVideoMode(mainMonitor);
        if (a_FullScreen)
        {
            glfwSetWindowMonitor(m_Window, mainMonitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
            m_FullScreenResolution = { videoMode->width, videoMode->height };
        }
        else
        {
            glfwSetWindowMonitor(m_Window, nullptr, 50, 50, a_Width, a_Height, videoMode->refreshRate);
        }
	    
	    /*
	     * Update the settings.
	     */
        m_RenderData.m_Settings.resolutionX = a_Width;
        m_RenderData.m_Settings.resolutionY = a_Height;
        m_RenderData.m_Settings.fullScreen = a_FullScreen;

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

    bool Renderer::IsFullScreen() const
    {
        return m_RenderData.m_Settings.fullScreen;
    }

    InputData Renderer::QuerryInput()
    {
        //Retrieve input.
        glfwPollEvents();

        return m_InputQueue.GetQueuedEvents();
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
        //Stages may have frame-independent stuff going on too.
        for (auto& stage : m_RenderStages)
        {
            stage->WaitForIdle(m_RenderData);
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

        vkDestroyFence(m_RenderData.m_Device, m_CopyFence, nullptr);

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

        //Only draw when the window is not minimized.
        const bool minimized = glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED);
        if(minimized)
        {
            return true;
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
        std::vector<VkPipelineStageFlags> waitStageFlags;       //The stages the wait semaphores should wait before.

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

        //Retrieve the first queue in the graphics vector. This is guaranteed to support presenting.
        const auto& queue = m_RenderData.m_GraphicsQueues[0];

        if(vkQueueSubmit(queue.m_Queue, 1, &submitInfo, frameData.m_Fence) != VK_SUCCESS)
        {
            printf("Could not submit queue in swapchain!\n");
            return false;
        }

        //Start building the command buffer.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &frameData.m_WaitForRenderSemaphore;  //Wait for the command buffer to stop executing before presenting.
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_SwapChain;
        presentInfo.pImageIndices = &m_CurrentFrameIndex;
        presentInfo.pResults = nullptr;

        if(vkQueuePresentKHR(queue.m_Queue, &presentInfo) != VK_SUCCESS)
        {
            printf("Could not present swapchain!\n");
            return false;
        }

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
        if(vkAcquireNextImageKHR(m_RenderData.m_Device, m_SwapChain, std::numeric_limits<unsigned>::max(), frameData.m_WaitForFrameSemaphore, nullptr, &m_CurrentFrameIndex) != VK_SUCCESS)
        {
            printf("Could not get next image in swap chain!\n");
            return false;
        }
        m_FrameReadySemaphore = frameData.m_WaitForFrameSemaphore;

	    //Increment the frame index.
        ++m_FrameCounter;
	    
	    return true;
    }

    glm::vec2 Renderer::GetResolution() const
    {
        if(m_RenderData.m_Settings.fullScreen)
        {
            return m_FullScreenResolution;
        }
        else
        {
            return glm::vec2(m_RenderData.m_Settings.resolutionX, m_RenderData.m_Settings.resolutionY);
        }
    }

    std::vector<std::shared_ptr<Mesh>> Renderer::CreateMeshes(const std::vector<MeshCreateInfo>& a_MeshCreateInfos)
    {
        //First lock this mutex so that no other thread can start accessing the upload.
        std::lock_guard<std::mutex> lock(m_CopyMutex);

        //Wait for previous uploads to end.
        vkWaitForFences(m_RenderData.m_Device, 1, &m_CopyFence, VK_TRUE, std::numeric_limits<uint64_t>::max());

        std::vector<std::shared_ptr<Mesh>> meshes;
        meshes.reserve(a_MeshCreateInfos.size());

        for (auto& info : a_MeshCreateInfos)
        {
            //If invalid, return nullptr.
            if(info.m_NumIndices == 0 || info.m_NumVertices == 0 || info.m_IndexBuffer == nullptr || info.m_VertexBuffer == nullptr)
            {
                printf("Invalid mesh info provided to mesh creation function! Nullptr or 0 sized arrays.\n");
                meshes.push_back(nullptr);
                continue;
            }

            //Calculate buffer size. Offset to be 16-byte aligned.
            const auto vertexSizeBytes = sizeof(Vertex) * info.m_NumVertices;
            const auto indexSizeBytes = sizeof(std::uint32_t) * info.m_NumIndices;

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
                return {};
            }

            //Create a buffer on the GPU that can be copied into from the CPU.
            VkBuffer stagingBuffer;
            VmaAllocation stagingBufferAllocation;
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if (vmaCreateBuffer(m_RenderData.m_Allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingBufferAllocation, nullptr) != VK_SUCCESS)
            {
                printf("Error! Could not allocate copy memory for mesh.\n");
                return {};
            }

            //Retrieve information about the staging and GPU buffers (handles)
            VmaAllocationInfo stagingBufferInfo;
            VmaAllocationInfo gpuBufferInfo;
            vmaGetAllocationInfo(m_RenderData.m_Allocator, stagingBufferAllocation, &stagingBufferInfo);
            vmaGetAllocationInfo(m_RenderData.m_Allocator, allocation, &gpuBufferInfo);


            /*
             * Copy from CPU memory to CPU only memory on the GPU.
             * NOTE: Vma buffer into deviceMemory is shared, so offset should also be used!
             */
            void* data;
            //Vertex
            vkMapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory, stagingBufferInfo.offset,
                VK_WHOLE_SIZE, 0, &data);

            memcpy(data, info.m_VertexBuffer, vertexSizeBytes);
            vkUnmapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory);
            //Index
            vkMapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory, stagingBufferInfo.offset + indexOffset,
                VK_WHOLE_SIZE, 0, &data);

            memcpy(data, info.m_IndexBuffer, indexSizeBytes);
            vkUnmapMemory(m_RenderData.m_Device, stagingBufferInfo.deviceMemory);

            /*
             * Copy from the CPU memory to the GPU using a command buffer
             */

             //First reset the command pool. This automatically resets all associated buffers as well (if flag was not individual)
            vkResetCommandPool(m_RenderData.m_Device, m_CopyCommandPool, 0);

            VkCommandBufferBeginInfo beginInfo;
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;
            beginInfo.pNext = nullptr;

            if (vkBeginCommandBuffer(m_CopyBuffer, &beginInfo) != VK_SUCCESS)
            {
                printf("Could not begin recording copy command buffer!\n");
                return {};
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

            //Take the first transfer queue, and if not present take the last generic graphics queue.
            const auto& transferQueue = m_RenderData.m_MeshUploadQueue->m_Queue;

            //Submit the work and then wait for the fence to be signaled.
            vkResetFences(m_RenderData.m_Device, 1, &m_CopyFence);
            vkQueueSubmit(transferQueue, 1, &submitInfo, m_CopyFence);
            vkWaitForFences(m_RenderData.m_Device, 1, &m_CopyFence, true, std::numeric_limits<uint32_t>::max());

            //Free the staging buffer
            vmaDestroyBuffer(m_RenderData.m_Allocator, stagingBuffer, stagingBufferAllocation);

            //Finally create a shared pointer and return a copy of it after putting it in the registry.
            auto ptr = std::make_shared<Mesh>(m_MeshCounter, allocation, buffer, info.m_NumIndices, info.m_NumVertices, indexOffset, vertexOffset);
            m_Meshes.Add(ptr);

            ++m_MeshCounter;
            meshes.push_back(ptr);
        }

        return meshes;
    }

    std::shared_ptr<Mesh> Renderer::CreateMesh(const Shape a_Shape, const glm::mat4& a_InitialTransform)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        switch (a_Shape)
        {
        case Shape::PLANE:
        {
            vertices =
            {
                Vertex{{1.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {1.f, 1.f}},
                Vertex{{-1.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f}},
                Vertex{{-1.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f}},
                Vertex{{1.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {1.f, 0.f}},
            };
            indices = { 0, 1, 2, 0, 3, 1 };
        }
        break;
        case Shape::CUBE:
        {
            vertices = {
                Vertex{{-1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{-1.000000, 1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, 1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{1.000000, 1.000000, -1.000000}, {0.000000, 0.000000, 1.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{1.000000, -1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, -1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, 1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{1.000000, 1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{1.000000, -1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, 1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, -1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{-1.000000, -1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{1.000000, 1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, 1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, -1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{-1.000000, 1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{-1.000000, -1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, -1.000000, -1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, 1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{-1.000000, 1.000000, 1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{-1.000000, -1.000000, -1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, 1.000000, -1.000000}, {-1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, 1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{1.000000, 1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{-1.000000, 1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, 1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, 1.000000, -1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{1.000000, 1.000000, 1.000000}, {1.000000, 0.000000, 0.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{-1.000000, -1.000000, 1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, -1.000000, 1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{-1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 0.000000}},
                Vertex{{-1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {1.000000, 0.000000}},
                Vertex{{1.000000, -1.000000, 1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}},
                Vertex{{1.000000, -1.000000, -1.000000}, {0.000000, 0.000000, -1.000000}, {0.000000, 1.000000, 0.000000}, {0.000000, 1.000000}}
            };

            for(int i = 0; i < 36; ++i)
            {
                indices.push_back(i);
            }
        }
        break;
        case Shape::SPHERE: //Unimplemented for now.
        default:
        {
            printf("Trying to create non-existent shape mesh: %u!\n", static_cast<uint32_t>(a_Shape));
            return nullptr;
        }
        break;
        }

        //If the provided transform is not the identity, transform all positions, normals and tangents.
        if (a_InitialTransform != glm::identity<glm::mat4>())
        {
            glm::mat4 normalMatrix = glm::transpose(glm::inverse(a_InitialTransform));

            for(auto& vertex : vertices)
            {
                vertex.normal = glm::normalize(glm::vec3(normalMatrix * glm::vec4(vertex.normal, 0.f)));
                vertex.tangent = glm::normalize(glm::vec3(normalMatrix * glm::vec4(vertex.tangent, 0.f)));
                vertex.position = a_InitialTransform * glm::vec4(vertex.position, 1.f);
            }
        }

        //make the mesh!
        return CreateMesh(vertices, indices);
    }

    std::shared_ptr<Mesh> Renderer::CreateMesh(const std::vector<Vertex>& a_VertexBuffer, const std::vector<std::uint32_t>& a_IndexBuffer)
    {
        MeshCreateInfo info = { a_VertexBuffer.data(), a_IndexBuffer.data(), static_cast<uint32_t>(a_IndexBuffer.size()), static_cast<uint32_t>(a_VertexBuffer.size()) };
        auto vector = CreateMeshes(std::vector<MeshCreateInfo>{info});
        if(!vector.empty())
        {
            return vector[0];
        }
        return nullptr;
    }

    std::shared_ptr<Mesh> Renderer::CreateMesh(const MeshCreateInfo& a_MeshCreateInfo)
    {
        auto vector = CreateMeshes(std::vector<MeshCreateInfo>{a_MeshCreateInfo});
        if (!vector.empty())
        {
            return vector[0];
        }
        return nullptr;
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

        printf("Vulkan instance successfully created.\n");

        /*
         * Bind GLFW and Vulkan.
         */
        const auto result = glfwCreateWindowSurface(m_RenderData.m_VulkanInstance, m_Window, NULL, &m_RenderData.m_Surface);
        if (result != VK_SUCCESS)
        {
            printf("Could not create window surface for Vulkan and GLFW.\n");
            return false;
        }
        //Store this instance with the window. This allows key callbacks to access the input queue instance.
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, MousePositionCallback);
        glfwSetScrollCallback(m_Window, MouseScrollCallback);


        return true;
    }

    void Renderer::KeyCallback(GLFWwindow* a_Window, int a_Key, int a_Scancode, int a_Action, int a_Mods)
    {
        Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(a_Window));

        switch (a_Action)
        {
        case GLFW_PRESS:
            renderer->m_InputQueue.AddKeyboardEvent({KeyboardAction::KEY_PRESSED, static_cast<uint16_t>(a_Key)});
            break;
        case GLFW_RELEASE:
            renderer->m_InputQueue.AddKeyboardEvent({ KeyboardAction::KEY_RELEASED, static_cast<uint16_t>(a_Key) });
            break;
        default:
            break;
        }
    }

    void Renderer::MousePositionCallback(GLFWwindow* a_Window, double a_Xpos, double a_Ypos)
    {
        Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(a_Window));

        float deltaX = static_cast<float>(a_Xpos) - renderer->m_LastMousePos.x;
        float deltaY = static_cast<float>(a_Ypos) - renderer->m_LastMousePos.y;
        renderer->m_LastMousePos = glm::vec2(a_Xpos, a_Ypos);

        if(deltaX != 0.f)
        {
            renderer->m_InputQueue.AddMouseEvent({ MouseAction::MOVE_X, deltaX, MouseButton::NONE});
        }
        if (deltaY != 0.f)
        {
            renderer->m_InputQueue.AddMouseEvent({ MouseAction::MOVE_Y, deltaY, MouseButton::NONE });
        }
    }

    void Renderer::MouseButtonCallback(GLFWwindow* a_Window, int a_Button, int a_Action, int a_Mods)
    {
        Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(a_Window));

        MouseAction action;
        MouseButton button;

        switch (a_Action)
        {
        case GLFW_PRESS:
            action = MouseAction::CLICK;
            break;
        case GLFW_RELEASE:
            action = MouseAction::RELEASE;
            break;
        default:
            action = MouseAction::NONE;
            break;
        }

        switch (a_Button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            button = MouseButton::LMB;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            button = MouseButton::RMB;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            button = MouseButton::MMB;
            break;
        default:
            button = MouseButton::NONE;
            break;
        }

        renderer->m_InputQueue.AddMouseEvent({ action, 0, button });
    }

    void Renderer::MouseScrollCallback(GLFWwindow* a_Window, double a_Xoffset, double a_Yoffset)
    {
        Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(a_Window));

        if(a_Xoffset != 0.0)
        {
            renderer->m_InputQueue.AddMouseEvent({ MouseAction::SCROLL, static_cast<float>(a_Xoffset), MouseButton::NONE});
        }

        if(a_Yoffset != 0.0)
        {
            renderer->m_InputQueue.AddMouseEvent({ MouseAction::SCROLL, static_cast<float>(a_Yoffset), MouseButton::NONE });
        }

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

        printf("Number of GPU queue families found: %i.\n", queueFamilyCount);

        //Assign the device.
        m_RenderData.m_PhysicalDevice = device;

        if (m_RenderData.m_PhysicalDevice == VK_NULL_HANDLE)
        {
            printf("Could not find suitable GPU for Vulkan.\n");
            return false;
        }

        /*
         * Find queues of each type to use.
         * Vec2 containing Family ID and number of queues available.
         */
        std::vector<glm::uvec2> transferOnlyQueueFamilies;
        std::vector<glm::uvec2> presentGraphicsQueueFamilies;
        std::vector<glm::uvec2> genericQueueFamilies;
        std::vector<glm::uvec2> computeOnlyQueueFamilies;
        std::vector<glm::uvec2> transferComputeQueueFamilies;

        std::set<uint32_t> presentSupportedFamilyIndices;

        int familyIndex = 0;
        for(auto& properties : queueFamilies)
        {
            bool graphics = (properties.queueFlags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT) != 0;
            bool compute = (properties.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT) != 0;
            bool transfer = (properties.queueFlags & VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT) != 0;
            VkBool32 presentSupport;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, m_RenderData.m_Surface, &presentSupport);
            if(presentSupport)
            {
                presentSupportedFamilyIndices.insert(familyIndex);
            }

            //Transfer only
            if(transfer && !graphics && !compute)
            {
                transferOnlyQueueFamilies.emplace_back(glm::uvec2{ familyIndex, properties.queueCount});
            }
            //Transfer compute mixed bag.
            else if(transfer && compute && !graphics)
            {
                transferComputeQueueFamilies.emplace_back(glm::uvec2{ familyIndex, properties.queueCount });
            }
            //Compute only
            else if(compute && !transfer && !graphics)
            {
                computeOnlyQueueFamilies.emplace_back(glm::uvec2{ familyIndex, properties.queueCount });
            }
            //Present mode graphics queue
            else if(graphics  && presentSupport)
            {
                presentGraphicsQueueFamilies.emplace_back(glm::uvec2{ familyIndex, properties.queueCount });
            }
            else if(graphics && transfer && compute)
            {
                genericQueueFamilies.emplace_back(glm::uvec2{ familyIndex, properties.queueCount });
            }

            ++familyIndex;
        }

        /*
         * Each queue gets it's own entry in these vectors. X = family Y = queue index.
         */
        std::vector<glm::uvec2> graphicsQueues;
        std::vector<glm::uvec2> transferQueues;
        std::vector<glm::uvec2> computeQueues;

        //If there's transfer only families available, use those for transfers.
        if(!transferOnlyQueueFamilies.empty())
        {
            for(auto& family : transferOnlyQueueFamilies)
            {
                const auto id = family.x;
                for (uint32_t i = 0; i < family.y; ++i)
                {
                    transferQueues.emplace_back(glm::uvec2{ id, i });
                }
            }
        }
        //Check if there's compute only families available.
        if(!computeOnlyQueueFamilies.empty())
        {
            for (auto& family : computeOnlyQueueFamilies)
            {
                const auto id = family.x;
                for (uint32_t i = 0; i < family.y; ++i)
                {
                    computeQueues.emplace_back(glm::uvec2{ id, i });
                }
            }
        }
        //Now check for combined compute and transfer queues.
        if(!transferComputeQueueFamilies.empty())
        {
            std::vector<glm::uvec2> totalQueues;
            for (auto& family : transferComputeQueueFamilies)
            {
                const auto id = family.x;
                for (uint32_t i = 0; i < family.y; ++i)
                {
                    totalQueues.emplace_back(glm::uvec2{ id, i });
                }
            }

            //If both compute and transfer are empty, divide the queues.
            if(computeQueues.empty() && transferQueues.empty())
            {
                const uint32_t halfwayPoint = totalQueues.size() / 2;

                for(uint32_t i = 0; i < static_cast<uint32_t>(totalQueues.size()); ++i)
                {
                    if(i < halfwayPoint)
                    {
                        computeQueues.emplace_back(totalQueues[i]);
                    }
                    else
                    {
                        transferQueues.emplace_back(totalQueues[i]);
                    }
                }
            }
            //Add to transfer queues only
            else if(transferQueues.empty())
            {
                transferQueues.insert(transferQueues.end(), totalQueues.begin(), totalQueues.end());
            }
            //Add to compute queues only.
            else
            {
                computeQueues.insert(computeQueues.end(), totalQueues.begin(), totalQueues.end());
            }
        }

        //Add the graphics with present mode queues.
        if(!presentGraphicsQueueFamilies.empty())
        {
            for (auto& family : presentGraphicsQueueFamilies)
            {
                const auto id = family.x;
                for (uint32_t i = 0; i < family.y; ++i)
                {
                    graphicsQueues.emplace_back(glm::uvec2{ id, i });
                }
            }
        }
        //Add to graphics queues.
        if(!genericQueueFamilies.empty())
        {
            for (auto& family : genericQueueFamilies)
            {
                const auto id = family.x;
                for (uint32_t i = 0; i < family.y; ++i)
                {
                    graphicsQueues.emplace_back(glm::uvec2{ id, i });
                }
            }
        }

        /*
         * Create all the queues.
         */
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<std::vector<float>> priorities;
        priorities.resize(queueFamilyCount);

        int index = 0;
        for(auto& queueFamily : queueFamilies)
        {
            priorities[index].resize(queueFamily.queueCount, 1.f);

            VkDeviceQueueCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueCount = queueFamily.queueCount;
            info.queueFamilyIndex = index;
            info.pQueuePriorities = priorities[index].data();
            queueCreateInfos.push_back(info);
            ++index;
        }


        /*
         * Create the actual device and specify the queues to use etc.
         */
        VkDeviceCreateInfo createInfo;
        const std::vector<const char*> swapchainExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
        {
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = 0;

            //Create the queues defined above.
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();

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
        for(auto& queue : graphicsQueues)
        {
            QueueInfo info;
            info.m_Type = QueueType::QUEUE_TYPE_GRAPHICS;
            info.m_FamilyIndex = queue.x;
            info.m_QueueIndex = queue.y;
            vkGetDeviceQueue(m_RenderData.m_Device, queue.x, queue.y, &info.m_Queue);
            bool present = presentSupportedFamilyIndices.find(queue.x) != presentSupportedFamilyIndices.end();

            info.m_SupportsPresent = present;
            m_RenderData.m_GraphicsQueues.push_back(info);
        }
        for (auto& queue : computeQueues)
        {
            QueueInfo info;
            info.m_Type = QueueType::QUEUE_TYPE_COMPUTE;
            info.m_FamilyIndex = queue.x;
            info.m_QueueIndex = queue.y;
            vkGetDeviceQueue(m_RenderData.m_Device, queue.x, queue.y, &info.m_Queue);
            info.m_SupportsPresent = false;
            m_RenderData.m_ComputeQueues.push_back(info);
        }
        for (auto& queue : transferQueues)
        {
            QueueInfo info;
            info.m_Type = QueueType::QUEUE_TYPE_TRANSFER;
            info.m_FamilyIndex = queue.x;
            info.m_QueueIndex = queue.y;
            vkGetDeviceQueue(m_RenderData.m_Device, queue.x, queue.y, &info.m_Queue);
            info.m_SupportsPresent = false;
            m_RenderData.m_TransferQueues.push_back(info);
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
        uint32_t swapBufferCount = surfaceCapabilities.minImageCount;
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
        //Assign the queues used for the main pipeline.
        m_RenderData.m_MeshUploadQueue = &(!m_RenderData.m_TransferQueues.empty() ? m_RenderData.m_TransferQueues[0] : m_RenderData.m_GraphicsQueues[m_RenderData.m_GraphicsQueues.size() - 1]);
        m_RenderData.m_PresentQueue = &m_RenderData.m_GraphicsQueues[0];

        /*
         * Setup the copy command buffer and pool.
         * These are used to copy data to the GPU.
         */
        VkCommandPoolCreateInfo copyPoolInfo;
        copyPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        copyPoolInfo.pNext = nullptr;
        copyPoolInfo.flags = 0;
        copyPoolInfo.queueFamilyIndex = m_RenderData.m_MeshUploadQueue->m_FamilyIndex;
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
            poolInfo.queueFamilyIndex = m_RenderData.m_PresentQueue->m_FamilyIndex;
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

        //Create a fence for synchronization in the uploading of meshes.
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
        fenceInfo.pNext = nullptr;
        vkCreateFence(m_RenderData.m_Device, &fenceInfo, nullptr, &m_CopyFence);

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
}