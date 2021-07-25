#include <filesystem>
#include <string>
#include <vector>

#include "Resources.h"
#include "Renderer.h"
#include "RenderStage.h"
#include "RenderUtility.h"

VkRenderPass& RenderStage_Deferred::GetRenderPass()
{

    return m_DeferredRenderPass;
}

void RenderStage_Deferred::SetDrawData(const DrawData& a_Data)
{
    m_DrawData = &a_Data;
}

bool RenderStage_Deferred::Init(const RenderData& a_RenderData)
{
    m_Frames.resize(a_RenderData.m_Settings.m_SwapBufferCount);

    constexpr auto DEFERRED_COLOR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr auto DEFERRED_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

    //Ensure that the format is supported as color attachment.
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(a_RenderData.m_PhysicalDevice, DEFERRED_COLOR_FORMAT, &properties);

    bool attachment = (VkFormatFeatureFlagBits::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT & properties.optimalTilingFeatures) != 0;
    if (!attachment)
    {
        printf("Image format for deferred buffer does not support being a color attachment!\n");
        return false;
    }

    /*
     * Create the render pass describing the attachments used.
     */
    VkAttachmentDescription attachments[DEFERRED_ATTACHMENT_MAX_ENUM + 1];
    for (int i = 0; i <= DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
    {
        attachments[i].format = DEFERRED_COLOR_FORMAT;
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[i].flags = 0;
    }

    //Override for the depth as that one is slightly different.
    attachments[0].format = DEFERRED_DEPTH_FORMAT;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //Override for the swap chains output.
    attachments[DEFERRED_ATTACHMENT_MAX_ENUM].format = a_RenderData.m_Settings.outputFormat;
    attachments[DEFERRED_ATTACHMENT_MAX_ENUM].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[DEFERRED_ATTACHMENT_MAX_ENUM].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //One depth attachment, followed by four color attachments.
    VkAttachmentReference attachmentReferences[DEFERRED_ATTACHMENT_MAX_ENUM];
    for (int i = 1; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
    {
        attachmentReferences[i].attachment = i;
        attachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    attachmentReferences[0].attachment = 0;
    attachmentReferences[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //An output reference for all the attachments in the framebuffer, just set most of them to unused.
    VkAttachmentReference outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM + 1] {};
    for(int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
    {
        outputReferences[i].attachment = VK_ATTACHMENT_UNUSED;
        //outputReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM].attachment = DEFERRED_ATTACHMENT_MAX_ENUM;
    outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //Deferred images read as shader read only resources.
    VkAttachmentReference secondPassInputs[DEFERRED_ATTACHMENT_MAX_ENUM]{ {}, {}, {}, {}, {}};
    for(int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
    {
        secondPassInputs[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  //Shader has to write to location 5 for the output.
        secondPassInputs[i].attachment = i;
    }

    VkSubpassDescription subpass[]{{}, {}};
    //First subpass outputs to the deferred images.
    subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass[0].colorAttachmentCount = DEFERRED_ATTACHMENT_MAX_ENUM - 1;
    subpass[0].pColorAttachments = &attachmentReferences[1];
    subpass[0].pDepthStencilAttachment = &attachmentReferences[0];

    //Second subpass only outputs to the swap chain view.
    subpass[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass[1].colorAttachmentCount = DEFERRED_ATTACHMENT_MAX_ENUM + 1; //6 attachments, but first 5 are unused.
    subpass[1].pColorAttachments = &outputReferences[0];
    subpass[1].pDepthStencilAttachment = nullptr;

    //Second subpass uses the first passes' outputs as inputs.
    subpass[1].inputAttachmentCount = DEFERRED_ATTACHMENT_MAX_ENUM;
    subpass[1].pInputAttachments = &secondPassInputs[0];

    /*
     * Set up dependencies between the two passes.
     */
    VkSubpassDependency subPassDependencies[3]{ {}, {}, {} };

    //Dependency between previous commands and starting the deferred rendering.
    subPassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subPassDependencies[0].dstSubpass = 0;
    subPassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subPassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subPassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subPassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subPassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;   //Only applies locally within this framebuffer.

    //Transitions from subpass 0 to 1.
    //In this subpass, the outputs of the previous stage become inputs.
    //The stage starts with the color attachment outputs, and ends in the fragment shader.
    subPassDependencies[1].srcSubpass = 0;
    subPassDependencies[1].dstSubpass = 1;
    subPassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subPassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subPassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subPassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subPassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    //Final dependency to transition out of the last sub pass.
    subPassDependencies[2].srcSubpass = 1;
    subPassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    subPassDependencies[2].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subPassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subPassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subPassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subPassDependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    //Combine all these.
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = DEFERRED_ATTACHMENT_MAX_ENUM + 1;  //5 deferred attachments + 1 output to the swapchain.
    renderPassInfo.pAttachments = &attachments[0];
    renderPassInfo.subpassCount = 2;
    renderPassInfo.pSubpasses = &subpass[0];
    renderPassInfo.pDependencies = &subPassDependencies[0];
    renderPassInfo.dependencyCount = 3;

    /*
     * And finally make the render pass.
     */
    if (vkCreateRenderPass(a_RenderData.m_Device, &renderPassInfo, nullptr, &m_DeferredRenderPass) != VK_SUCCESS)
    {
        printf("Could not create render pass for pipeline!\n");
        return false;
    }

    /*
     * Set up a descriptor pool and set layout.
     */
    constexpr auto numDeferredReadDescriptors = EDeferredFrameAttachments::DEFERRED_ATTACHMENT_MAX_ENUM;

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[numDeferredReadDescriptors]{{}, {}, {}, {}, {}};

    for(int i = 0; i < numDeferredReadDescriptors; ++i)
    {
        descriptorSetLayoutBinding[i].descriptorCount = 1;
        descriptorSetLayoutBinding[i].binding = i;
        descriptorSetLayoutBinding[i].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        descriptorSetLayoutBinding[i].stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;    //Only used in the fragment shader.   
    }

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = numDeferredReadDescriptors;
    setLayoutInfo.pBindings = &descriptorSetLayoutBinding[0];


    //Make the descriptor set layout.
    if (vkCreateDescriptorSetLayout(a_RenderData.m_Device, &setLayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
    {
        printf("Could not create descriptor set layout for deferred render stage.\n");
        return false;
    }

    //Pool that will hold just one descriptor set.
    VkDescriptorPoolSize poolSizes{};
    poolSizes.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    poolSizes.descriptorCount = numDeferredReadDescriptors;
    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets = a_RenderData.m_Settings.m_SwapBufferCount;   //Each frame gets a set!
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = &poolSizes;

    //Make the descriptor pool. Pool uses the layout created above.
    if (vkCreateDescriptorPool(a_RenderData.m_Device, &descPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
    {
        printf("Could not create descriptor pool for deferred render stage.\n");
        return false;
    }

    /*
     * Set up the buffers and objects per frame.
     */
    int frameIndex = 0;
    for (auto& frame : m_Frames)
    {
        /*
         * Create an array texture and depth texture for the deferred pass.
         * Create views and a Frame Buffer that uses the views to write to the right outputs.
         */
        ImageInfo arrayImage;
        arrayImage.m_Format = DEFERRED_COLOR_FORMAT;
        arrayImage.m_ArrayLayers = DEFERRED_ATTACHMENT_MAX_ENUM - 1;
        arrayImage.m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        arrayImage.m_Dimensions = { a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY, 1 };
        arrayImage.m_ImageType = VK_IMAGE_TYPE_2D;
        arrayImage.m_MipLevels = 1;

        ImageInfo depthImage;
        depthImage.m_Format = DEFERRED_DEPTH_FORMAT;
        depthImage.m_Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        depthImage.m_Dimensions = { a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY, 1 };

        if (!RenderUtility::CreateImage(a_RenderData.m_Device, a_RenderData.m_Allocator, arrayImage, frame.m_DeferredArrayImage) 
            || !RenderUtility::CreateImage(a_RenderData.m_Device, a_RenderData.m_Allocator, depthImage, frame.m_DepthImage))
        {
            printf("Could not create images in deferred stage.\n");
            return false;
        }

        //Create the depth view at index 0.
        ImageViewInfo depthImageViewInfo;
        depthImageViewInfo.m_Format = depthImage.m_Format;
        depthImageViewInfo.m_Image = frame.m_DepthImage.m_Image;
        depthImageViewInfo.m_VisibleAspects = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(!RenderUtility::CreateImageView(a_RenderData.m_Device, depthImageViewInfo, frame.m_DeferredImageViews[0]))
        {
            printf("Could not create depth image view in deferred stage.\n");
            return false;
        }

        //Create the other views at index 1..4
        ImageViewInfo arrayImageViewInfo;
        arrayImageViewInfo.m_Format = arrayImage.m_Format;
        arrayImageViewInfo.m_MipLevels = arrayImage.m_MipLevels;
        arrayImageViewInfo.m_ArrayLayers = 1;
        arrayImageViewInfo.m_Image = frame.m_DeferredArrayImage.m_Image;
        arrayImageViewInfo.m_VisibleAspects = VK_IMAGE_ASPECT_COLOR_BIT;
        arrayImageViewInfo.m_ViewType = VK_IMAGE_VIEW_TYPE_2D;

        for(int attachment = 1; attachment < DEFERRED_ATTACHMENT_MAX_ENUM; ++attachment)
        {
            //Only grant each view access to a specific layer.
            arrayImageViewInfo.m_BaseArrayLayer = attachment - 1;

            if(!RenderUtility::CreateImageView(a_RenderData.m_Device, arrayImageViewInfo, frame.m_DeferredImageViews[attachment]))
            {
                printf("Could not create deferred image view for depth %i.\n", attachment);
                return false;
            }
        }

        /*
         * The last attachment is the swap chain's view.
         */
        frame.m_DeferredImageViews[DEFERRED_ATTACHMENT_MAX_ENUM] = a_RenderData.m_FrameData[frameIndex].m_SwapchainView;

        /*
         * The frame buffer to use the views.
         */

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass = m_DeferredRenderPass;
        frameBufferInfo.attachmentCount = DEFERRED_ATTACHMENT_MAX_ENUM + 1; //Last attachment is the swapchain output.
        frameBufferInfo.pAttachments = frame.m_DeferredImageViews;
        frameBufferInfo.width = a_RenderData.m_Settings.resolutionX;
        frameBufferInfo.height = a_RenderData.m_Settings.resolutionY;
        frameBufferInfo.layers = 1;
        if (vkCreateFramebuffer(a_RenderData.m_Device, &frameBufferInfo, nullptr, &frame.m_DeferredBuffer) != VK_SUCCESS)
        {
            printf("Could not create frame buffer for deferred stage!\n");
            return false;
        }

        /*
         * Descriptors used to read the deferred output in the image.
         */
        VkDescriptorImageInfo descriptors[numDeferredReadDescriptors]{{}, {}, {}, {}, {}};
        for(int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
        {
            descriptors[i].imageView = frame.m_DeferredImageViews[i];
            descriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptors[i].sampler = VK_NULL_HANDLE;    //Input attachments do not use samples since they are just single values in a location.
        }

        //Make the actual descriptor set. It's created inside the pool above using the layout above.
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pSetLayouts = &m_DescriptorSetLayout;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.descriptorPool = m_DescriptorPool;
        if (vkAllocateDescriptorSets(a_RenderData.m_Device, &descriptorSetAllocateInfo, &frame.m_DescriptorSet) != VK_SUCCESS)
        {
            printf("Could not create descriptor set for deferred render stage.\n");
            return false;
        }

        VkWriteDescriptorSet writeDescriptorSet[numDeferredReadDescriptors]{{}, {}, {}, {}, {}};
        for (int i = 0; i < numDeferredReadDescriptors; ++i)
        {
            writeDescriptorSet[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet[i].dstSet = frame.m_DescriptorSet;
            writeDescriptorSet[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            writeDescriptorSet[i].descriptorCount = 1;
            writeDescriptorSet[i].dstBinding = i;
            writeDescriptorSet[i].pImageInfo = &descriptors[i];
        }
        vkUpdateDescriptorSets(a_RenderData.m_Device, numDeferredReadDescriptors, &writeDescriptorSet[0], 0, nullptr);

        ++frameIndex;
    }

	/*
	 * Deferred processing pipeline definition.
	 */
    {
        PipelineCreateInfo pipelineInfo;
        pipelineInfo.m_Shaders.push_back({ "deferred_processing.vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT });
        pipelineInfo.m_Shaders.push_back({"deferred_processing.frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT });
        pipelineInfo.resolution.m_ResolutionX = a_RenderData.m_Settings.resolutionX;
        pipelineInfo.resolution.m_ResolutionY = a_RenderData.m_Settings.resolutionY;
        pipelineInfo.renderPass.m_RenderPass = m_DeferredRenderPass;
        pipelineInfo.renderPass.m_SubpassIndex = 1;     //Use the 2nd sub-pass.
        pipelineInfo.depth.m_UseDepth = false;          //This is just shading so no need to use depth.
        pipelineInfo.depth.m_WriteDepth = false;
        pipelineInfo.descriptors.m_Layouts.push_back(m_DescriptorSetLayout);
        pipelineInfo.attachments.m_NumAttachments = DEFERRED_ATTACHMENT_MAX_ENUM + 1;

        if(!RenderUtility::CreatePipeline(pipelineInfo, a_RenderData.m_Device, a_RenderData.m_Settings.shadersPath, m_DeferredProcessedPipelineData))
        {
            return false;
        }
    }

    /*
     * Deferred rendering pipeline.
     */
    {
        PipelineCreateInfo pipelineInfo;
        pipelineInfo.m_Shaders.push_back({ "deferred.vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT });
        pipelineInfo.m_Shaders.push_back({ "deferred.frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT });
        pipelineInfo.resolution.m_ResolutionX = a_RenderData.m_Settings.resolutionX;
        pipelineInfo.resolution.m_ResolutionY = a_RenderData.m_Settings.resolutionY;
        pipelineInfo.vertexData.m_VertexBindings.push_back({ 0, sizeof(Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX });
        pipelineInfo.vertexData.m_VertexAttributes.push_back({ 0, 0, VkFormat::VK_FORMAT_R32G32B32_SFLOAT, 0 });
        pipelineInfo.vertexData.m_VertexAttributes.push_back({ 1, 0, VkFormat::VK_FORMAT_R32G32B32_SFLOAT, 12 });
        pipelineInfo.vertexData.m_VertexAttributes.push_back({ 2, 0, VkFormat::VK_FORMAT_R32G32B32_SFLOAT, 24 });
        pipelineInfo.vertexData.m_VertexAttributes.push_back({ 3, 0, VkFormat::VK_FORMAT_R32G32_SFLOAT, 36 });
        pipelineInfo.pushConstants.m_PushConstantRanges.push_back({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DeferredPushConstants) });
        pipelineInfo.renderPass.m_RenderPass = m_DeferredRenderPass;
        pipelineInfo.attachments.m_NumAttachments = DEFERRED_ATTACHMENT_MAX_ENUM - 1;
        pipelineInfo.culling.m_CullMode = VK_CULL_MODE_BACK_BIT;    //Cull back facing geometry.

        if (!RenderUtility::CreatePipeline(pipelineInfo, a_RenderData.m_Device, a_RenderData.m_Settings.shadersPath, m_DeferredPipelineData))
        {
            return false;
        }
    }

    /*
     * Finally, set up the objects required per frame to upload instance data to the GPU.
     */

    //Choose the last upload queue, and otherwise the second last or last graphics queue.
    //I am disgusted by this abhorrent line of code. Apologies to anyone reading this.
    m_UploadQueue = &(!a_RenderData.m_TransferQueues.empty() ? a_RenderData.m_TransferQueues[a_RenderData.m_TransferQueues.size() - 1]
    : a_RenderData.m_GraphicsQueues.size() > 2 ? a_RenderData.m_GraphicsQueues[a_RenderData.m_GraphicsQueues.size() - 2]
    : a_RenderData.m_GraphicsQueues[a_RenderData.m_GraphicsQueues.size() - 1]);

    //Go over each frame and set up the objects.
    for(uint32_t i = 0; i < a_RenderData.m_Settings.m_SwapBufferCount; ++i)
    {
        auto& instanceData = m_Frames[i].m_InstanceData;

        //For the buffers, I just set the size to 0. Then when the frames happen, they will automatically resize as needed.
        instanceData.m_InstanceBufferSize = 0;
        instanceData.m_InstanceDataBuffer = nullptr;
        instanceData.m_InstanceBufferAllocation = nullptr;

        //Create a command pool
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = m_UploadQueue->m_FamilyIndex;
        if(vkCreateCommandPool(a_RenderData.m_Device, &commandPoolInfo, nullptr, &instanceData.m_UploadCommandPool) != VK_SUCCESS)
        {
            printf("Could not create command pool for instance data uploading in deferred stage!\n");
            return false;
        }

        VkCommandBufferAllocateInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandBufferCount = 1;
        commandBufferInfo.commandPool = instanceData.m_UploadCommandPool;
        commandBufferInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        if(vkAllocateCommandBuffers(a_RenderData.m_Device, &commandBufferInfo, &instanceData.m_UploadCommandBuffer) != VK_SUCCESS)
        {
            printf("Could not allocate command buffer to upload frame data with in deferred stage.\n");
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if(vkCreateSemaphore(a_RenderData.m_Device, &semaphoreInfo, nullptr, &instanceData.m_UploadSemaphore) != VK_SUCCESS)
        {
            printf("Could not create semaphore in deferred stage.\n");
            return false;
        }
    }
	
	return true;
}

bool RenderStage_Deferred::CleanUp(const RenderData& a_RenderData)
{
    //Destroy per frame instance data
    for (uint32_t i = 0; i < a_RenderData.m_Settings.m_SwapBufferCount; ++i)
    {
        auto& instanceData = m_Frames[i].m_InstanceData;
        vkDestroyCommandPool(a_RenderData.m_Device, instanceData.m_UploadCommandPool, nullptr);
        vkDestroySemaphore(a_RenderData.m_Device, instanceData.m_UploadSemaphore, nullptr);
        if(instanceData.m_InstanceBufferSize != 0)
        {
            vmaDestroyBuffer(a_RenderData.m_Allocator, instanceData.m_InstanceDataBuffer, instanceData.m_InstanceBufferAllocation);
        }
    }

    vkDestroyPipeline(a_RenderData.m_Device, m_DeferredPipelineData.m_Pipeline, nullptr);
    vkDestroyPipelineLayout(a_RenderData.m_Device, m_DeferredPipelineData.m_PipelineLayout, nullptr);
    vkDestroyPipeline(a_RenderData.m_Device, m_DeferredProcessedPipelineData.m_Pipeline, nullptr);
    vkDestroyPipelineLayout(a_RenderData.m_Device, m_DeferredProcessedPipelineData.m_PipelineLayout, nullptr);

    //Destroy all shaders.
    for(auto& shader : m_DeferredPipelineData.m_ShaderModules)
    {
        vkDestroyShaderModule(a_RenderData.m_Device, shader, nullptr);
    }
    for (auto& shader : m_DeferredProcessedPipelineData.m_ShaderModules)
    {
        vkDestroyShaderModule(a_RenderData.m_Device, shader, nullptr);
    }

    for(auto& frame : m_Frames)
    {
        //Only destroy the views created by this stage! The last view belongs to the swapchain, and was created by the renderer itself. Will be killed there.
        for(int index = 0; index < static_cast<int>(sizeof(frame.m_DeferredImageViews) / sizeof(frame.m_DeferredImageViews[0])) - 1; ++index)
        {
            vkDestroyImageView(a_RenderData.m_Device, frame.m_DeferredImageViews[index], nullptr);
        }

        vmaDestroyImage(a_RenderData.m_Allocator, frame.m_DeferredArrayImage.m_Image, frame.m_DeferredArrayImage.m_Allocation);
        vmaDestroyImage(a_RenderData.m_Allocator, frame.m_DepthImage.m_Image, frame.m_DepthImage.m_Allocation);

        vkDestroyFramebuffer(a_RenderData.m_Device, frame.m_DeferredBuffer, nullptr);
    }

    vkDestroyDescriptorPool(a_RenderData.m_Device, m_DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(a_RenderData.m_Device, m_DescriptorSetLayout, nullptr);

    vkDestroyRenderPass(a_RenderData.m_Device, m_DeferredRenderPass, nullptr);

    return true;
}

bool RenderStage_Deferred::RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
	const uint32_t a_CurrentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
	std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags)
{
    //First set the pipeline and pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_DeferredRenderPass;
    renderPassInfo.framebuffer = m_Frames[a_CurrentFrameIndex].m_DeferredBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY };
    VkClearValue clearColor = { a_RenderData.m_Settings.clearColor.r, a_RenderData.m_Settings.clearColor.g, a_RenderData.m_Settings.clearColor.b, a_RenderData.m_Settings.clearColor.a };
    VkClearValue clearColors[DEFERRED_ATTACHMENT_MAX_ENUM + 1]
    {
        {1.f}, clearColor, clearColor, clearColor, clearColor, clearColor
    };
    renderPassInfo.clearValueCount = DEFERRED_ATTACHMENT_MAX_ENUM + 1;
    renderPassInfo.pClearValues = &clearColors[0];
    vkCmdBeginRenderPass(a_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineData.m_Pipeline);

    DeferredPushConstants pushData;
    pushData.m_VPMatrix = m_DrawData->m_Camera->CalculateVPMatrix();

    //Bind the push constants.
    vkCmdPushConstants(a_CommandBuffer, m_DeferredPipelineData.m_PipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DeferredPushConstants), &pushData);

    //TODO bind the instance data like material ID (push constant) and matrices (upload previous frame, then use next frame and bind as SSBO).

    for(int drawCallIndex = 0; drawCallIndex < m_DrawData->m_NumDrawCalls; ++drawCallIndex)
    {
        auto& drawCall = m_DrawData->m_pDrawCalls[drawCallIndex];
        const auto buffer = drawCall.m_Mesh->GetBuffer();
        const auto vertexOffset = drawCall.m_Mesh->GetVertexBufferOffset();
        const auto indexBufferOffset = drawCall.m_Mesh->GetIndexBufferOffset();

        //Vertex and index data is stored in the same buffer.
        vkCmdBindVertexBuffers(a_CommandBuffer, 0, 1, &buffer, &vertexOffset);
        vkCmdBindIndexBuffer(a_CommandBuffer, buffer, indexBufferOffset, VkIndexType::VK_INDEX_TYPE_UINT32);

        //Instanced draw call
        vkCmdDrawIndexed(a_CommandBuffer, static_cast<uint32_t>(drawCall.m_Mesh->GetNumIndices()), static_cast<uint32_t>(drawCall.m_NumInstances), 0, 0, 0);
    }

    //Next pass!
    vkCmdNextSubpass(a_CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    //Process in the second stage.
    vkCmdBindPipeline(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredProcessedPipelineData.m_Pipeline);
    vkCmdBindDescriptorSets(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredProcessedPipelineData.m_PipelineLayout, 0, 1, &m_Frames[a_CurrentFrameIndex].m_DescriptorSet, 0, nullptr);
    vkCmdDraw(a_CommandBuffer, 3, 1, 0, 0); //Draw a full-screen triangle.

    vkCmdEndRenderPass(a_CommandBuffer);

    return true;
}