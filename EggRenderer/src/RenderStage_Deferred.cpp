#include <filesystem>
#include <string>
#include <vector>

#include "Resources.h"
#include "Renderer.h"
#include "RenderStage.h"
#include "RenderUtility.h"
#include "api/Timer.h"

namespace egg
{

    VkRenderPass& RenderStage_Deferred::GetRenderPass()
    {

        return m_DeferredRenderPass;
    }

    bool RenderStage_Deferred::Init(const RenderData& a_RenderData)
    {
        m_Frames.resize(a_RenderData.m_Settings.m_SwapBufferCount);

        constexpr auto DEFERRED_COLOR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr auto DEFERRED_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

        /*
         * Create descriptor sets for shading data access.
         */
        if(!RenderUtility::CreateDescriptorSetContainer(a_RenderData.m_Device,
            DescriptorSetContainerCreateInfo::Create(a_RenderData.m_Settings.m_SwapBufferCount)
            .AddBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            ,m_ShadingDescriptors))
        {
            printf("Could not create descriptor sets!\n");
            return false;
        }

        /*
         * Create the descriptor pool and set layout for the instance data buffers.
         * Two bindings are used, one for instance data and one for the indirection buffer.
         */
        if (!RenderUtility::CreateDescriptorSetContainer(a_RenderData.m_Device,
            DescriptorSetContainerCreateInfo::Create(a_RenderData.m_Settings.m_SwapBufferCount)
            .AddBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .AddBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            , m_InstanceDescriptors))
        {
            printf("Could not create descriptor sets!\n");
            return false;
        }

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
        attachments[DEFERRED_ATTACHMENT_MAX_ENUM].format = static_cast<VkFormat>(a_RenderData.m_Settings.outputFormat);
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
        VkAttachmentReference outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM + 1]{};
        for (int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
        {
            outputReferences[i].attachment = VK_ATTACHMENT_UNUSED;
            //outputReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM].attachment = DEFERRED_ATTACHMENT_MAX_ENUM;
        outputReferences[DEFERRED_ATTACHMENT_MAX_ENUM].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //Deferred images read as shader read only resources.
        VkAttachmentReference secondPassInputs[DEFERRED_ATTACHMENT_MAX_ENUM]{ {}, {}, {}, {}, {} };
        for (int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
        {
            secondPassInputs[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  //Shader has to write to location 5 for the output.
            secondPassInputs[i].attachment = i;
        }

        VkSubpassDescription subpass[]{ {}, {} };
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
         * Set up a descriptor pool and set layout used to access the deferred subpass output.
         */
        constexpr auto numDeferredReadDescriptors = EDeferredFrameAttachments::DEFERRED_ATTACHMENT_MAX_ENUM;
        auto attachmentDescriptorCreateInfo = DescriptorSetContainerCreateInfo::Create(a_RenderData.m_Settings.m_SwapBufferCount);
        for (int i = 0; i < numDeferredReadDescriptors; ++i)
        {
            attachmentDescriptorCreateInfo.AddBinding(i, 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        if (!RenderUtility::CreateDescriptorSetContainer(a_RenderData.m_Device, attachmentDescriptorCreateInfo, m_ProcessingDescriptors))
        {
            printf("Could not create descriptor sets!\n");
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

            if (!RenderUtility::CreateImageView(a_RenderData.m_Device, depthImageViewInfo, frame.m_DeferredImageViews[0]))
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

            for (int attachment = 1; attachment < DEFERRED_ATTACHMENT_MAX_ENUM; ++attachment)
            {
                //Only grant each view access to a specific layer.
                arrayImageViewInfo.m_BaseArrayLayer = attachment - 1;

                if (!RenderUtility::CreateImageView(a_RenderData.m_Device, arrayImageViewInfo, frame.m_DeferredImageViews[attachment]))
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
            VkDescriptorImageInfo descriptors[numDeferredReadDescriptors]{ {}, {}, {}, {}, {} };
            for (int i = 0; i < DEFERRED_ATTACHMENT_MAX_ENUM; ++i)
            {
                descriptors[i].imageView = frame.m_DeferredImageViews[i];
                descriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptors[i].sampler = VK_NULL_HANDLE;    //Input attachments do not use samples since they are just single values in a location.
            }

            VkWriteDescriptorSet writeDescriptorSet[numDeferredReadDescriptors]{ {}, {}, {}, {}, {} };
            for (int i = 0; i < numDeferredReadDescriptors; ++i)
            {
                writeDescriptorSet[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet[i].dstSet = m_ProcessingDescriptors.m_Sets[frameIndex];
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
            pipelineInfo.m_Shaders.push_back({ "deferred_processing.frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT });
            pipelineInfo.resolution.m_ResolutionX = a_RenderData.m_Settings.resolutionX;
            pipelineInfo.resolution.m_ResolutionY = a_RenderData.m_Settings.resolutionY;
            pipelineInfo.renderPass.m_RenderPass = m_DeferredRenderPass;
            pipelineInfo.renderPass.m_SubpassIndex = 1;     //Use the 2nd sub-pass.
            pipelineInfo.depth.m_UseDepth = false;          //This is just shading so no need to use depth.
            pipelineInfo.depth.m_WriteDepth = false;
            pipelineInfo.descriptors.m_Layouts.push_back(m_ProcessingDescriptors.m_Layout);
            pipelineInfo.descriptors.m_Layouts.push_back(m_ShadingDescriptors.m_Layout);
            pipelineInfo.attachments.m_NumAttachments = DEFERRED_ATTACHMENT_MAX_ENUM + 1;
            pipelineInfo.pushConstants.m_PushConstantRanges.push_back({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DeferredProcessingPushConstants) });

            if (!RenderUtility::CreatePipeline(pipelineInfo, a_RenderData.m_Device, a_RenderData.m_Settings.shadersPath, m_DeferredProcessingPipelineData))
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
            pipelineInfo.vertexData.m_VertexAttributes.push_back({ 2, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, 24 });
            pipelineInfo.vertexData.m_VertexAttributes.push_back({ 3, 0, VkFormat::VK_FORMAT_R32G32_SFLOAT, 40 });
            pipelineInfo.pushConstants.m_PushConstantRanges.push_back({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DeferredPushConstants) });
            pipelineInfo.renderPass.m_RenderPass = m_DeferredRenderPass;
            pipelineInfo.attachments.m_NumAttachments = DEFERRED_ATTACHMENT_MAX_ENUM - 1;
            pipelineInfo.culling.m_CullMode = VK_CULL_MODE_BACK_BIT;    //Cull back facing geometry.
            pipelineInfo.descriptors.m_Layouts.push_back(m_InstanceDescriptors.m_Layout);

            if (!RenderUtility::CreatePipeline(pipelineInfo, a_RenderData.m_Device, a_RenderData.m_Settings.shadersPath, m_DeferredPipelineData))
            {
                return false;
            }
        }

        return true;
    }

    bool RenderStage_Deferred::CleanUp(const RenderData& a_RenderData)
    {
    	//Pipelines!
        vkDestroyPipeline(a_RenderData.m_Device, m_DeferredPipelineData.m_Pipeline, nullptr);
        vkDestroyPipelineLayout(a_RenderData.m_Device, m_DeferredPipelineData.m_PipelineLayout, nullptr);
        vkDestroyPipeline(a_RenderData.m_Device, m_DeferredProcessingPipelineData.m_Pipeline, nullptr);
        vkDestroyPipelineLayout(a_RenderData.m_Device, m_DeferredProcessingPipelineData.m_PipelineLayout, nullptr);

        //Destroy all shaders.
        for (auto& shader : m_DeferredPipelineData.m_ShaderModules)
        {
            vkDestroyShaderModule(a_RenderData.m_Device, shader, nullptr);
        }
        for (auto& shader : m_DeferredProcessingPipelineData.m_ShaderModules)
        {
            vkDestroyShaderModule(a_RenderData.m_Device, shader, nullptr);
        }

        for (auto& frame : m_Frames)
        {
            //Only destroy the views created by this stage! The last view belongs to the swapchain, and was created by the renderer itself. Will be killed there.
            for (int index = 0; index < static_cast<int>(sizeof(frame.m_DeferredImageViews) / sizeof(frame.m_DeferredImageViews[0])) - 1; ++index)
            {
                vkDestroyImageView(a_RenderData.m_Device, frame.m_DeferredImageViews[index], nullptr);
            }

            vmaDestroyImage(a_RenderData.m_Allocator, frame.m_DeferredArrayImage.m_Image, frame.m_DeferredArrayImage.m_Allocation);
            vmaDestroyImage(a_RenderData.m_Allocator, frame.m_DepthImage.m_Image, frame.m_DepthImage.m_Allocation);

            vkDestroyFramebuffer(a_RenderData.m_Device, frame.m_DeferredBuffer, nullptr);
        }

        //Destroy allocated descriptor set layouts and pools.
        RenderUtility::DestroyDescriptorSetContainer(a_RenderData.m_Device, m_InstanceDescriptors);
        RenderUtility::DestroyDescriptorSetContainer(a_RenderData.m_Device, m_ShadingDescriptors);
        RenderUtility::DestroyDescriptorSetContainer(a_RenderData.m_Device, m_ProcessingDescriptors);

        vkDestroyRenderPass(a_RenderData.m_Device, m_DeferredRenderPass, nullptr);

        return true;
    }

    bool RenderStage_Deferred::RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
        const uint32_t a_CurrentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
        std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags)
    {
        /*
         * Uploading data for the next frame.
         */
        auto& frame = a_RenderData.m_FrameData[a_CurrentFrameIndex];
        auto& frameData = m_Frames[a_CurrentFrameIndex];

		//Update the descriptor set to point to the instance data and indirection buffer.
        VkDescriptorBufferInfo descriptorBufferInfo[2]{};
        const auto& indirectionBuffer = a_RenderData.m_FrameData[a_CurrentFrameIndex].m_UploadData.m_IndirectionBuffer;
        descriptorBufferInfo[0].offset = 0; //Note that this is relative to the buffer, so 0 for all of it.
    										//This is NOT the same as the VMA allocation info offset, which refers to an entire block.
        descriptorBufferInfo[0].buffer = indirectionBuffer.GetBuffer();
        descriptorBufferInfo[0].range = VK_WHOLE_SIZE;
    	
        const auto& instanceBuffer = a_RenderData.m_FrameData[a_CurrentFrameIndex].m_UploadData.m_InstanceBuffer;
        descriptorBufferInfo[1].offset = 0;
        descriptorBufferInfo[1].buffer = instanceBuffer.GetBuffer();
        descriptorBufferInfo[1].range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet setWrite[2]{};
        setWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrite[0].descriptorCount = 1;
        setWrite[0].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setWrite[0].dstBinding = 0;
        setWrite[0].dstArrayElement = 0;
        setWrite[0].dstSet = m_InstanceDescriptors.m_Sets[a_CurrentFrameIndex];
        setWrite[0].pBufferInfo = &descriptorBufferInfo[0];
        setWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrite[1].descriptorCount = 1;
        setWrite[1].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setWrite[1].dstBinding = 1;
        setWrite[1].dstArrayElement = 0;
        setWrite[1].dstSet = m_InstanceDescriptors.m_Sets[a_CurrentFrameIndex];
        setWrite[1].pBufferInfo = &descriptorBufferInfo[1];

    	//Do two writes within the set: instance and indirection data.
        vkUpdateDescriptorSets(a_RenderData.m_Device, 2, &setWrite[0], 0, nullptr);


        //Make the descriptor set for shading point to the right per frame buffers.
        //This is used for materials, lights etc.
        VkDescriptorBufferInfo materialDescriptorBufferInfo{};
        materialDescriptorBufferInfo.offset = 0;
        materialDescriptorBufferInfo.range = VK_WHOLE_SIZE;
        materialDescriptorBufferInfo.buffer = frame.m_UploadData.m_MaterialBuffer.GetBuffer();
        VkWriteDescriptorSet materialWriteDescriptorSet{};
        materialWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        materialWriteDescriptorSet.descriptorCount = 1;
        materialWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        materialWriteDescriptorSet.dstArrayElement = 0;
        materialWriteDescriptorSet.dstBinding = 0;
        materialWriteDescriptorSet.dstSet = m_ShadingDescriptors.m_Sets[a_CurrentFrameIndex];
        materialWriteDescriptorSet.pBufferInfo = &materialDescriptorBufferInfo;
        vkUpdateDescriptorSets(a_RenderData.m_Device, 1, &materialWriteDescriptorSet, 0, nullptr);

    	
        /*
         * Rendering the current frame.
         */

        //First set the pipeline and pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_DeferredRenderPass;
        renderPassInfo.framebuffer = frameData.m_DeferredBuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY };
        VkClearValue clearColor = {
            a_RenderData.m_Settings.clearColor.r,
            a_RenderData.m_Settings.clearColor.g,
            a_RenderData.m_Settings.clearColor.b,
            a_RenderData.m_Settings.clearColor.a
        };
    	//Clear depth attachment with 1.0, and the rest with the provided clear color
        VkClearValue clearColors[DEFERRED_ATTACHMENT_MAX_ENUM + 1]
        {
            {1.f}, clearColor, clearColor, clearColor, clearColor, clearColor
        };
        renderPassInfo.clearValueCount = DEFERRED_ATTACHMENT_MAX_ENUM + 1;
        renderPassInfo.pClearValues = &clearColors[0];
        vkCmdBeginRenderPass(a_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineData.m_Pipeline);

        auto& drawData = *frame.m_DrawData;
    	
        //Put the previous frame's camera in the push constants.
        DeferredPushConstants pushData;
        pushData.m_VPMatrix = drawData.m_Camera.CalculateVPMatrix();

        //Bind the push constants.
        vkCmdPushConstants(a_CommandBuffer, m_DeferredPipelineData.m_PipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(DeferredPushConstants), &pushData);

        vkCmdBindDescriptorSets(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineData.m_PipelineLayout,
            0, 1, &m_InstanceDescriptors.m_Sets[a_CurrentFrameIndex], 0, nullptr);

        for (auto& drawPass : drawData.m_DrawPasses)
        {
        	//First do static deferred shading.
            if(drawPass.m_Type == DrawPassType::STATIC_DEFERRED_SHADING)
            {
	            for(int drawCallIndex : drawPass.m_DrawCalls)
	            {
                    auto& drawCall = drawData.m_drawCalls[drawCallIndex];
	            	
                    const auto& mesh = std::static_pointer_cast<Mesh>(drawData.m_Meshes[drawCall.m_MeshIndex]);
                    const auto buffer = mesh->GetBuffer();
                    const auto vertexOffset = mesh->GetVertexBufferOffset();
                    const auto indexBufferOffset = mesh->GetIndexBufferOffset();

                    //Vertex and index data is stored in the same buffer.
                    vkCmdBindVertexBuffers(a_CommandBuffer, 0, 1, &buffer, &vertexOffset);
                    vkCmdBindIndexBuffer(a_CommandBuffer, buffer, indexBufferOffset, VkIndexType::VK_INDEX_TYPE_UINT32);

                    //Push constants contain the offset and and total instance count in the first vec4.
                    //vkCmdPushConstants(a_CommandBuffer, m_DeferredPipelineData.m_PipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::uvec4), &drawLocalData);

                    //Instanced draw call.
	            	//Offset into the indirection buffer is passed as the first instance.
                    vkCmdDrawIndexed(a_CommandBuffer, static_cast<uint32_t>(mesh->GetNumIndices()), static_cast<uint32_t>(drawCall.m_NumInstances), 0, 0, drawCall.m_IndirectionBufferOffset);
	            }
            }
        }

        //Next pass!
        vkCmdNextSubpass(a_CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        //Process in the second stage.
        vkCmdBindPipeline(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredProcessingPipelineData.m_Pipeline);

        //Bind the descriptor set that handles G-Buffer input.
        VkDescriptorSet sets[2]{ m_ProcessingDescriptors.m_Sets[a_CurrentFrameIndex], m_ShadingDescriptors.m_Sets[a_CurrentFrameIndex]};
        vkCmdBindDescriptorSets(a_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredProcessingPipelineData.m_PipelineLayout, 0, 2, sets, 0, nullptr);

        DeferredProcessingPushConstants processingPushData;
        processingPushData.m_CameraPosition = glm::vec4(drawData.m_Camera.GetTransform().GetTranslation(), 0.f);
        vkCmdPushConstants(a_CommandBuffer, m_DeferredProcessingPipelineData.m_PipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(DeferredProcessingPushConstants), &processingPushData);

        vkCmdDraw(a_CommandBuffer, 3, 1, 0, 0); //Draw a full-screen triangle.
        vkCmdEndRenderPass(a_CommandBuffer);
    	
        return true;
    }

    void RenderStage_Deferred::WaitForIdle(const RenderData& a_RenderData)
    {
        //Nothing to wait for here.
    }
}
