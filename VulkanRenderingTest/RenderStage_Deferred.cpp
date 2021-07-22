#include <filesystem>
#include <string>
#include <vector>

#include "Renderer.h"
#include "RenderStage.h"
#include "RenderUtility.h"

VkRenderPass& RenderStage_Deferred::GetRenderPass()
{

    return m_DeferredRenderPass;
}

bool RenderStage_Deferred::Init(const RenderData& a_RenderData)
{
    m_Frames.resize(a_RenderData.m_Settings.m_SwapBufferCount);

    //How each attachment will be used.
    constexpr VkImageUsageFlags imageUsage[DEFERRED_NUM_ATTACHMENTS]
    {
        VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    constexpr VkImageType imageType[DEFERRED_NUM_ATTACHMENTS]
    {
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
        VkImageType::VK_IMAGE_TYPE_2D,
    };

    constexpr VkFormat imageFormat[DEFERRED_NUM_ATTACHMENTS]
    {
        VkFormat::VK_FORMAT_D32_SFLOAT,             //Depth
        VkFormat::VK_FORMAT_R32G32B32_SFLOAT,       //Position
        VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,    //Normal/Material ID
        VkFormat::VK_FORMAT_R32G32B32_SFLOAT,       //Tangent
        VkFormat::VK_FORMAT_R32G32_SFLOAT,          //UV
        VkFormat::VK_FORMAT_D32_SFLOAT,             //Depth
        VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,    //Color
    };

    /*
     * Set up the buffers and objects per frame.
     */
    for (auto& frame : m_Frames)
    {
        /*
         * Initialize the output images.
         * Loop over the attachments, and then create the VkImage and VkImageView objects accordingly.
         */
        for (int attachmentIndex = 0; attachmentIndex < DEFERRED_NUM_ATTACHMENTS; ++attachmentIndex)
        {
            VkImageCreateInfo imgInfo{};
            imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.arrayLayers = 1;
            imgInfo.format = imageFormat[attachmentIndex];
            imgInfo.extent = VkExtent3D{a_RenderData.m_Settings.resolutionX, a_RenderData.m_Settings.resolutionY, 1};
            imgInfo.imageType = imageType[attachmentIndex];
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.mipLevels = 1;
            imgInfo.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            imgInfo.usage = imageUsage[attachmentIndex];
            imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocationCreateInfo{};
            allocationCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
            allocationCreateInfo.requiredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.pNext = nullptr;
            viewInfo.flags = 0;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageFormat[attachmentIndex];
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = (imageFormat[attachmentIndex] == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vmaCreateImage(a_RenderData.m_Allocator, &imgInfo, &allocationCreateInfo, &frame.m_Images[attachmentIndex], &frame.m_ImageAllocations[attachmentIndex], nullptr) != VK_SUCCESS)
            {
                printf("Could not create image in deferred stage.\n");
                return false;
            }

            //Set the image.
            viewInfo.image = frame.m_Images[attachmentIndex];

            if (vkCreateImageView(a_RenderData.m_Device, &viewInfo, nullptr, &frame.m_ImageViews[attachmentIndex]) != VK_SUCCESS)
            {
                printf("Could not create image view in deferred stage.\n");
                return false;
            }
        }

        /*
         * Descriptors used to read the deferred output in the image.
         */
        constexpr auto numDeferredReadDescriptors = DEFERRED_NUM_ATTACHMENTS - 2;   //Only the inputs need to be read in this shader stage.

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.descriptorCount = numDeferredReadDescriptors;
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorSetLayoutBinding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;    //Only used in the fragment shader.
        descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
        setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.bindingCount = 1;
        setLayoutInfo.pBindings = &descriptorSetLayoutBinding;

        VkDescriptorPoolSize poolSizes{};
        poolSizes.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes.descriptorCount = DEFERRED_NUM_ATTACHMENTS;
        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.maxSets = 1;
        descPoolInfo.poolSizeCount = 1;
        descPoolInfo.pPoolSizes = &poolSizes;

        //Make the descriptor set layout.
        if (vkCreateDescriptorSetLayout(a_RenderData.m_Device, &setLayoutInfo, nullptr, &frame.m_DescriptorSetLayout) != VK_SUCCESS)
        {
            printf("Could not create descriptor set layout for deferred render stage.\n");
            return false;
        }

        //Make the descriptor pool. Pool uses the layout created above.
        if (vkCreateDescriptorPool(a_RenderData.m_Device, &descPoolInfo, nullptr, &frame.m_DescriptorPool) != VK_SUCCESS)
        {
            printf("Could not create descriptor pool for deferred render stage.\n");
            return false;
        }

        //Make the actual descriptor set. It's created inside the pool above using the layout above.
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pSetLayouts = &frame.m_DescriptorSetLayout;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.descriptorPool = frame.m_DescriptorPool;
        if (vkAllocateDescriptorSets(a_RenderData.m_Device, &descriptorSetAllocateInfo, &frame.m_DescriptorSet) != VK_SUCCESS)
        {
            printf("Could not create descriptor set for deferred render stage.\n");
            return false;
        }
    }

	/*
	 * Deferred pipeline definition.
	 */
    {
        /*
         * Create a render pass that accepts the output attachments.
         */
         VkAttachmentDescription attachments[5]
         {
             {},{},{},{},{}
         };

         constexpr VkImageLayout layouts[5]
         {
             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         };

         //The render pass.
         for (int i = 0; i < 5; ++i)
         {
             attachments[i].format = imageFormat[i];
             attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
             attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
             attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
             attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
             attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
             attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
             attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
         }


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
         renderPassInfo.pAttachments = &attachments[0];
         renderPassInfo.subpassCount = 1;
         renderPassInfo.pSubpasses = &subpass;

         if (vkCreateRenderPass(a_RenderData.m_Device, &renderPassInfo, nullptr, &m_DeferredRenderPass) != VK_SUCCESS)
         {
             printf("Could not create render pass for pipeline!\n");
             return false;
         }

        PipelineCreateInfo deferredInfo;
        deferredInfo.m_Shaders.push_back({ "deferred.vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT });
        deferredInfo.m_Shaders.push_back({"deferred.frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT });
        deferredInfo.resolution.m_ResolutionX = a_RenderData.m_Settings.resolutionX;
        deferredInfo.resolution.m_ResolutionY = a_RenderData.m_Settings.resolutionY;
        deferredInfo.vertexData.m_VertexBindings.push_back({ 0, sizeof(Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX });
        deferredInfo.vertexData.m_VertexAttributes.push_back({ 0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, 0});
        deferredInfo.vertexData.m_VertexAttributes.push_back({ 1, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, 12 });
        deferredInfo.vertexData.m_VertexAttributes.push_back({ 2, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, 24 });
        deferredInfo.vertexData.m_VertexAttributes.push_back({ 3, 0, VkFormat::VK_FORMAT_R32G32_SFLOAT, 36 });

        deferredInfo.pushConstants.m_PushConstantRanges.push_back({VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DeferredPushConstants)});
        deferredInfo.renderPass.m_RenderPass = m_DeferredRenderPass;

        if(!RenderUtility::CreatePipeline(deferredInfo, a_RenderData.m_Device, a_RenderData.m_Settings.shadersPath, m_DeferredPipelineData))
        {
            return false;
        }
    }
	
	return true;
}

bool RenderStage_Deferred::CleanUp(const RenderData& a_RenderData)
{
    //TODO

    return true;
}

bool RenderStage_Deferred::RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
	const uint32_t currentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
	std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags)
{
    return true;
}