#include <filesystem>

#include "RenderStage.h"
#include "Renderer.h"
#include "RenderUtility.h"

bool RenderStage_HelloTriangle::Init(const RenderData& a_RenderData)
{

    /*
     * Load the Spir-V shaders from disk.
     */
    const std::string workingDir = std::filesystem::current_path().string();

    if (!RenderUtility::CreateShaderModuleFromSpirV(workingDir + "/shaders/output/default.vert.spv", m_VertexShader, a_RenderData.m_Device) || !RenderUtility::CreateShaderModuleFromSpirV(workingDir + "/shaders/output/default.frag.spv", m_FragmentShader, a_RenderData.m_Device))
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
    viewport.width = (float)a_RenderData.m_Settings.resolutionX;
    viewport.height = (float)a_RenderData.m_Settings.resolutionY;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = VkExtent2D{ a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY };
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

    if (vkCreatePipelineLayout(a_RenderData.m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
    {
        printf("Could not create pipeline layout for rendering pipeline!\n");
        return false;
    }

    //The render pass.
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = a_RenderData.m_Settings.outputFormat;
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

    if (vkCreateRenderPass(a_RenderData.m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
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

    if (vkCreateGraphicsPipelines(a_RenderData.m_Device, VK_NULL_HANDLE, 1, &psoInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
    {
        printf("Could not create graphics pipeline!\n");
        return false;
    }

    m_FrameBuffers.resize(a_RenderData.m_Settings.m_SwapBufferCount);
    for(int i = 0; i < a_RenderData.m_Settings.m_SwapBufferCount; ++i)
    {
        VkFramebufferCreateInfo fboInfo{};
        fboInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fboInfo.renderPass = m_RenderPass;
        fboInfo.attachmentCount = 1;
        fboInfo.pAttachments = &a_RenderData.m_FrameData[i].m_SwapchainView;
        fboInfo.width = a_RenderData.m_Settings.resolutionX;
        fboInfo.height = a_RenderData.m_Settings.resolutionY;
        fboInfo.layers = 1;

        if (vkCreateFramebuffer(a_RenderData.m_Device, &fboInfo, nullptr, &m_FrameBuffers[i]) != VK_SUCCESS)
        {
            printf("Could not create FBO for frame index %i!\n", i);
            return false;
        }
    }

    return true;
}

bool RenderStage_HelloTriangle::RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
    const uint32_t a_CurrentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
    std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags)
{
    //Fill the command buffer
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_FrameBuffers[a_CurrentFrameIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY };
    VkClearValue clearColor = { a_RenderData.m_Settings.clearColor.r, a_RenderData.m_Settings.clearColor.g, a_RenderData.m_Settings.clearColor.b, a_RenderData.m_Settings.clearColor.a };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(a_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    vkCmdDraw(a_CommandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(a_CommandBuffer);

    return true;
}

bool RenderStage_HelloTriangle::CleanUp(const RenderData& a_RenderData)
{
    //Pipeline related objects.
    vkDestroyPipeline(a_RenderData.m_Device, m_Pipeline, nullptr);
    vkDestroyRenderPass(a_RenderData.m_Device, m_RenderPass, nullptr);
    vkDestroyPipelineLayout(a_RenderData.m_Device, m_PipelineLayout, nullptr);

    //Destroy the frame buffers for each frame.
    for(int i = 0; i < a_RenderData.m_Settings.m_SwapBufferCount; ++i)
    {
        vkDestroyFramebuffer(a_RenderData.m_Device, m_FrameBuffers[i], nullptr);
    }

    //Delete the allocated fragment and vertex shaders.
    vkDestroyShaderModule(a_RenderData.m_Device, m_VertexShader, nullptr);
    vkDestroyShaderModule(a_RenderData.m_Device, m_FragmentShader, nullptr);
	
    return true;
}

VkRenderPass& RenderStage_HelloTriangle::GetRenderPass()
{
    return m_RenderPass;
}
