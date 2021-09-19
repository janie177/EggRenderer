#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>
#include <map>

#include "vk_mem_alloc.h"

namespace egg
{

    struct ImageInfo
    {
        //The type of the image.
        VkImageType m_ImageType = VK_IMAGE_TYPE_2D;

        //The dimensions of the image.
        VkExtent3D m_Dimensions = { 0, 0, 1 };

        //Set to more than 1 in case this is an array texture.
        uint32_t m_ArrayLayers = 1;

        //Set to higher than one to use mipmaps.
        uint32_t m_MipLevels = 1;

        //The format of the image.
        VkFormat m_Format = VK_FORMAT_R32G32B32A32_SFLOAT;

        //How this image will be used.
        VkImageUsageFlags m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    };

    /*
     * Returned when an image is created.
     */
    struct ImageData
    {
        //The image created.
        VkImage m_Image = nullptr;

        //The allocation for the image.
        VmaAllocation m_Allocation = nullptr;
    };

    /*
     * Information to create an image view.
     */
    struct ImageViewInfo
    {
        //The vulkan image handle that the view is created for.
        VkImage m_Image = nullptr;

        //The type of view.
        VkImageViewType m_ViewType = VK_IMAGE_VIEW_TYPE_2D;

        //The lowest accessible mip level for this view.
        uint32_t m_BaseMipLevel = 0;

        //The lowest accessible array layer for this view.
        uint32_t m_BaseArrayLayer = 0;

        //The amount of layers accessible for this view.
        uint32_t m_ArrayLayers = 1;

        //The amount of mip levels accessible to this view.
        uint32_t m_MipLevels = 1;

        //The format of the image.
        VkFormat m_Format = VK_FORMAT_R32G32B32A32_SFLOAT;

        //The visible aspects of the image for this view.
        VkImageAspectFlags m_VisibleAspects = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
    };

    /*
     * Information about a shader.
     */
    struct ShaderInfo
    {
        //The name of the Spir-V shader file.
        std::string m_ShaderFileName = "shaderName.vert.spv";

        //The shaders entry function. Normally "main".
        std::string m_ShaderEntryPoint = "main";

        //The type of shader this is.
        VkShaderStageFlagBits m_ShaderStage = VK_SHADER_STAGE_VERTEX_BIT;
    };

    struct AttachmentInfo
    {
        /*
         * The format of the attachment.
         */
        VkFormat m_AttachmentFormat = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
    };

    /*
     * Struct containing all the relevant information to create an entire pipeline.
     * This should hopefully take away a lot of boilerplate code.
     */
    struct PipelineCreateInfo
    {

        struct
        {
            /*
             * When true, a depth attachment is used.
             */
            bool m_UseDepth = true;

            /*
             * When true, the depth buffer will be overwritten with new entries.
             */
            bool m_WriteDepth = true;

            /*
             * The format of the depth buffer.
             */
            VkFormat m_DepthFormat = VK_FORMAT_D32_SFLOAT;

        } depth;

        /*
         * Vertex layout.
         */
        struct
        {
            //The vertex attributes.
            std::vector< VkVertexInputAttributeDescription> m_VertexAttributes;


            //The vertex bindings referred to by the vertex attributes.
            std::vector< VkVertexInputBindingDescription> m_VertexBindings;

        } vertexData;

        /*
         * Push constant data.
         */
        struct
        {
            //The push constant ranges used for this pipeline.
            std::vector< VkPushConstantRange> m_PushConstantRanges;

        } pushConstants;

        struct
        {
            /*
             * All descriptor set layouts used with this pipeline.
             */
            std::vector<VkDescriptorSetLayout> m_Layouts;

        } descriptors;

        struct
        {
            /*
             * The number of attachments used with this pipeline.
             * This will also be the amount of blend states configured for the pipeline.
             */
            uint32_t m_NumAttachments = 1;

        } attachments;

        /*
         * Viewport resolution.
         */
        struct
        {
            uint32_t m_ResolutionX = 0;
            uint32_t m_ResolutionY = 0;

        } resolution;

        //The shader stages to load.
        std::vector<ShaderInfo> m_Shaders;

        /*
         * All information related to the renderpass used with this pipeline.
         */
        struct
        {
            //The render pass that will be used with this pipeline.
            VkRenderPass m_RenderPass = nullptr;

            //The index of the subpass within the render pass to use for this pipeline.
            uint32_t m_SubpassIndex = 0;

        } renderPass;

        struct
        {
            VkFrontFace m_FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            /*
             * The cull mode to use.
             */
            VkCullModeFlags m_CullMode = VK_CULL_MODE_NONE;
        } culling;
    };

    /*
     * Contains all objects that need to be destroyed after a pipeline has been used.
     */
    struct PipelineData
    {
        //All shader modules loaded for this pipeline, in the order they were provided in.
        std::vector<VkShaderModule> m_ShaderModules;

        //The layout for the pipeline.
        VkPipelineLayout m_PipelineLayout = nullptr;

        //The pipeline object itself.
        VkPipeline m_Pipeline = nullptr;
    };

    /*
     * Contains descriptor sets and a layout + pool.
     */
    struct DescriptorSetContainer
    {
        //All descriptor currently residing in the pool with given layout.
        std::vector<VkDescriptorSet> m_Sets;
        VkDescriptorSetLayout m_Layout;
        VkDescriptorPool m_Pool;

        //The bindings used for the set layout.
        std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
    };

    /*
     * Information to create some descriptor sets.
     */
    struct DescriptorSetContainerCreateInfo
    {
        friend class RenderUtility;
    public:
        /*
         * Helper function to create a new instance.
         */
        static DescriptorSetContainerCreateInfo Create(uint32_t a_NumSets)
        {
            DescriptorSetContainerCreateInfo info;
            info.m_NumSets = a_NumSets;
            return info;
        }

        /*
         * Helper function to build up bindings.
         * Optionally provide binding flags.
         */
        DescriptorSetContainerCreateInfo& AddBinding(uint32_t a_BindingIndex,
            uint32_t a_DescriptorCount,
            VkDescriptorType a_DescriptorType,
            VkShaderStageFlags a_ShaderStageFlags,
            VkDescriptorBindingFlags a_BindingFlags = 0
        )
        {
            assert(a_DescriptorCount > 0 && "Need at least one descriptor per binding");
            m_Bindings.push_back(VkDescriptorSetLayoutBinding{ a_BindingIndex, a_DescriptorType, a_DescriptorCount, a_ShaderStageFlags, nullptr });
            m_BindingFlags.push_back(a_BindingFlags);
            return *this;
        }

    private:
        //The amount of sets to create.
        uint32_t m_NumSets = 0;

        //A vector of all the bindings the sets will have.
        std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
        std::vector<VkDescriptorBindingFlags> m_BindingFlags;
    };

    /*
     * Builder that accumulates writes to a descriptor set and then builds.
     */
    class DescriptorSetWriteBuilder
    {
    public:
        DescriptorSetWriteBuilder(const VkDevice& a_Device, const DescriptorSetContainer& a_DescriptorContainer):
            m_Device(a_Device),
            m_Container(a_DescriptorContainer)
        {
            
        }

        /*
         * Write a buffer to a descriptor.
         * For now does not support arrays.
         */
        DescriptorSetWriteBuilder& WriteBuffer(uint32_t a_SetIndex, uint32_t a_Binding, VkBuffer a_Buffer, VkDeviceSize a_Offset, VkDeviceSize a_Size)
        {
            assert(a_Binding < m_Container.m_Bindings.size() && "Binding out of bounds.");
            assert(a_SetIndex < m_Container.m_Sets.size() && "Set index out of bounds.");
            assert(a_Size > 0 && "Cannot write 0 size to descriptor set.");

            //Add a buffer into object to the end of the list.
            auto& buffer = m_BufferInfo.emplace_back(VkDescriptorBufferInfo{a_Buffer, a_Offset, a_Size});

            VkWriteDescriptorSet setWrite{};
            setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrite.descriptorCount = 1;   //No array support
            setWrite.descriptorType = m_Container.m_Bindings[a_Binding].descriptorType;
            setWrite.dstBinding = a_Binding;
            setWrite.dstArrayElement = 0;   //No array support
            setWrite.dstSet = m_Container.m_Sets[a_SetIndex];
            setWrite.pBufferInfo = &buffer; //Normally this is an array, but for now no array support.
            m_Writes.push_back(setWrite);

            return *this;
        }

        /*
         * Update the descriptors in this builder with all the accumulated data.
         */
        void Upload()
        {
            //Only upload when writes were actually done.
            if(!m_Writes.empty())
            {
                vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(m_Writes.size()), m_Writes.data(), 0, nullptr);
                m_Writes.clear();
                m_BufferInfo.clear();
            }
        }
    private:
        const VkDevice& m_Device;
        const DescriptorSetContainer& m_Container;

        std::vector<VkWriteDescriptorSet> m_Writes;
        std::list<VkDescriptorBufferInfo> m_BufferInfo; //Has to be list to prevent reallocation while building, which would invalidate existing writes.
    };

    /*
     * Read a file and store the contents in the given output buffer as chars.
     */
    class RenderUtility
    {
    public:
        /*
         * Get a tool that allows you to write to a specific descriptor set.
         */
        static DescriptorSetWriteBuilder WriteDescriptors(const VkDevice& a_Device, const DescriptorSetContainer& a_DescriptorContainer)
        {
            return DescriptorSetWriteBuilder(a_Device, a_DescriptorContainer);
        }

        /*
         * Destroy allocated objects for a descriptor set.
         */
        static void DestroyDescriptorSetContainer(
            const VkDevice& a_Device,
            const DescriptorSetContainer& a_Container)
        {
            vkDestroyDescriptorPool(a_Device, a_Container.m_Pool, nullptr);
            vkDestroyDescriptorSetLayout(a_Device, a_Container.m_Layout, nullptr);
        }

        /*
         * Create a descriptor set layout, pool and the given amount of sets.
         * This is not very dynamic but it's easy to quickly set up a few sets.
         *
         * Outputs all data to the a_Output object.
         */
        static bool CreateDescriptorSetContainer(
            const VkDevice& a_Device,
            const DescriptorSetContainerCreateInfo& a_Info,
            DescriptorSetContainer& a_Output)
        {
            assert(!a_Info.m_Bindings.empty() && "At least one binding is required to create descriptor sets!");
            assert(a_Info.m_NumSets > 0 && "At least one set needs to be created!");

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(a_Info.m_Bindings.size());
            descriptorSetLayoutCreateInfo.pBindings = a_Info.m_Bindings.data();

            //Explicitly specify binding flags for each binding.
            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
            bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            bindingFlags.bindingCount = static_cast<uint32_t>(a_Info.m_BindingFlags.size());
            bindingFlags.pBindingFlags = a_Info.m_BindingFlags.data();

            descriptorSetLayoutCreateInfo.pNext = &bindingFlags;

            if (vkCreateDescriptorSetLayout(a_Device, &descriptorSetLayoutCreateInfo, nullptr, &a_Output.m_Layout) != VK_SUCCESS)
            {
                printf("Could not create descriptor set layout!\n");
                return false;
            }

            std::map<VkDescriptorType, uint32_t> descriptorCounts;

            for(auto& binding : a_Info.m_Bindings)
            {
                const auto found = descriptorCounts.find(binding.descriptorType);
                if(found != descriptorCounts.end())
                {
                    found->second += binding.descriptorCount;
                }
                else
                {
                    descriptorCounts.insert({binding.descriptorType, binding.descriptorCount});
                }
            }

            std::vector<VkDescriptorPoolSize> poolSizes;
            poolSizes.reserve(descriptorCounts.size());
            for(auto& entry : descriptorCounts)
            {
                assert(entry.second > 0 && "Need at least one descriptor in a binding.");
                //For each descriptor type, multiply with the amount of sets to be allocated.
                poolSizes.push_back(VkDescriptorPoolSize{ entry.first, entry.second * a_Info.m_NumSets });
            }

            VkDescriptorPoolCreateInfo descriptorPoolInfo{};
            descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolInfo.maxSets = a_Info.m_NumSets;
            descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            descriptorPoolInfo.pPoolSizes = poolSizes.data();

            if (vkCreateDescriptorPool(a_Device, &descriptorPoolInfo, nullptr, &a_Output.m_Pool) != VK_SUCCESS)
            {
                printf("Could not create descriptor pool!\n");
                return false;
            }

            //Each set in the VkDescriptorSetAllocateInfo points to the same set, but has to be in array format.
            std::vector layoutVec(a_Info.m_NumSets, a_Output.m_Layout);

            //Allocate sets.
            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorSetCount = a_Info.m_NumSets;
            descriptorSetAllocateInfo.descriptorPool = a_Output.m_Pool;
            descriptorSetAllocateInfo.pSetLayouts = layoutVec.data();

            //Where to store the sets.
            a_Output.m_Sets.resize(a_Info.m_NumSets);

            if (vkAllocateDescriptorSets(a_Device, &descriptorSetAllocateInfo, a_Output.m_Sets.data()) != VK_SUCCESS)
            {
                printf("Could not create descriptor set!\n");
                return false;
            }

            //Copy the bindings over for later runtime reflection of sets.
            a_Output.m_Bindings = a_Info.m_Bindings;

            return true;
        }

        static bool CreateImage(const VkDevice& a_Device, const VmaAllocator& a_Allocator, const ImageInfo& a_CreateInfo, ImageData& a_Result)
        {
            /*
             * Ensure that the provided information is correct.
             */
            if (a_CreateInfo.m_MipLevels < 1)
            {
                printf("Images need a mip level of at least 1.\n");
                return false;
            }

            if (a_CreateInfo.m_ArrayLayers < 1)
            {
                printf("Images need at least one array layer.\n");
                return false;
            }

            if (a_CreateInfo.m_Dimensions.width < 1 || a_CreateInfo.m_Dimensions.height < 1 || a_CreateInfo.m_Dimensions.depth < 1)
            {
                printf("Image does not have valid dimensions: %u %u %u.\n", a_CreateInfo.m_Dimensions.width, a_CreateInfo.m_Dimensions.height, a_CreateInfo.m_Dimensions.depth);
                return false;
            }

            ImageData result;

            VkImageCreateInfo imgInfo{};
            imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.arrayLayers = a_CreateInfo.m_ArrayLayers;
            imgInfo.format = a_CreateInfo.m_Format;
            imgInfo.extent = a_CreateInfo.m_Dimensions;

            imgInfo.imageType = a_CreateInfo.m_ImageType;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.mipLevels = a_CreateInfo.m_MipLevels;
            imgInfo.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            imgInfo.usage = a_CreateInfo.m_Usage;
            imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
            //This is not needed, only when it's a 3D image and you want to access it as a 2D array.
            //imgInfo.flags = (imgInfo.arrayLayers > 1 ? VkImageCreateFlagBits::VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0);
            imgInfo.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;    //Optimal is the only one that can be used as color attachment. Keep that in mind!!

            VmaAllocationCreateInfo allocationCreateInfo{};
            allocationCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
            allocationCreateInfo.requiredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            if (vmaCreateImage(a_Allocator, &imgInfo, &allocationCreateInfo, &result.m_Image, &result.m_Allocation, nullptr) != VK_SUCCESS)
            {
                printf("Could not create image.\n");
                return false;
            }

            a_Result = result;
            return true;
        }

        static bool CreateImageView(const VkDevice& a_Device, const ImageViewInfo& a_CreateInfo, VkImageView& a_Result)
        {
            if (a_CreateInfo.m_Image == nullptr)
            {
                printf("Image view creation needs a valid image handle that is not nullptr.\n");
                return false;
            }

            if (a_CreateInfo.m_MipLevels < 1)
            {
                printf("Image views need a mip level of at least 1.\n");
                return false;
            }

            if (a_CreateInfo.m_ArrayLayers < 1)
            {
                printf("Image views need at least one array layer.\n");
                return false;
            }


            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.pNext = nullptr;
            viewInfo.flags = 0;
            viewInfo.viewType = a_CreateInfo.m_ViewType;
            viewInfo.format = a_CreateInfo.m_Format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = a_CreateInfo.m_VisibleAspects;
            viewInfo.subresourceRange.baseMipLevel = a_CreateInfo.m_BaseMipLevel;
            viewInfo.subresourceRange.levelCount = a_CreateInfo.m_MipLevels;
            viewInfo.subresourceRange.baseArrayLayer = a_CreateInfo.m_BaseArrayLayer;
            viewInfo.subresourceRange.layerCount = a_CreateInfo.m_ArrayLayers;
            viewInfo.image = a_CreateInfo.m_Image;

            if (vkCreateImageView(a_Device, &viewInfo, nullptr, &a_Result) != VK_SUCCESS)
            {
                printf("Could not create image view.\n");
                return false;
            }

            return true;
        }

        static bool ReadFile(const std::string& a_File, std::vector<char>& a_Output)
        {
            std::ifstream fileStream(a_File, std::ios::binary | std::ios::ate); //Ate will start the file pointer at the back, so tellg will return the size of the file.
            if (fileStream.is_open())
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

        /*
         * Load a Spir-V shader from file and compile it.
         */
        static bool CreateShaderModuleFromSpirV(const std::string& a_File, VkShaderModule& a_Output, const VkDevice& a_Device)
        {
            std::vector<char> byteCode;
            if (!ReadFile(a_File, byteCode))
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
            if (vkCreateShaderModule(a_Device, &shaderCreateInfo, nullptr, &a_Output) != VK_SUCCESS)
            {
                printf("Could not convert Spir-V to shader module for file %s!\n", a_File.c_str());
                return false;
            }

            return true;
        }



        /*
         * Create a vulkan pipeline state object.
         */
        static bool CreatePipeline(const PipelineCreateInfo& a_CreateInfo, const VkDevice& a_Device, const std::string& a_ShadersPath, PipelineData& a_Result)
        {
            /*
             * Verify the passed parameters.
             */

             //Ensure that a vertex shader is provided.
            bool vertexShaderFound = false;
            for (auto& shaderInfo : a_CreateInfo.m_Shaders)
            {
                if (shaderInfo.m_ShaderStage == VK_SHADER_STAGE_VERTEX_BIT)
                {
                    vertexShaderFound = true;
                    break;
                }
            }

            if (!vertexShaderFound)
            {
                printf("Trying to create pipeline that does not have a vertex shader specified!\n");
                return false;
            }

            if (a_CreateInfo.resolution.m_ResolutionX <= 0 || a_CreateInfo.resolution.m_ResolutionY <= 0)
            {
                printf("Invalid resolution specified for pipeline creation! X: %u Y: %u\n", a_CreateInfo.resolution.m_ResolutionX, a_CreateInfo.resolution.m_ResolutionY);
                return false;
            }

            //Ensure that the bindings referred to by the vertex attributes are actually valid.
            for (auto& vertexAttrib : a_CreateInfo.vertexData.m_VertexAttributes)
            {
                bool found = false;
                for (auto& binding : a_CreateInfo.vertexData.m_VertexBindings)
                {
                    if (binding.binding == vertexAttrib.binding)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    printf("Vertex attribute refers to binding %i, but that binding point was not specified.\n", vertexAttrib.binding);
                    return false;
                }
            }

            if (a_CreateInfo.renderPass.m_RenderPass == nullptr)
            {
                printf("No renderpass provided to create pipeline.\n");
                return false;
            }

            /*
             * Set up the pipeline.
             */
            PipelineData result;

            //Load the shaders.
            std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
            shaderStages.reserve(a_CreateInfo.m_Shaders.size());
            result.m_ShaderModules.reserve(a_CreateInfo.m_Shaders.size());
            int index = 0;
            for (auto& shader : a_CreateInfo.m_Shaders)
            {
                const std::string path = a_ShadersPath + shader.m_ShaderFileName;
                VkShaderModule module;
                //Failed to load
                if (!RenderUtility::CreateShaderModuleFromSpirV(path, module, a_Device))
                {
                    printf("Could not create shader from file: %s of type: %u.\n", path.c_str(), shader.m_ShaderStage);
                    return false;
                }
                //Load succeeded.
                else
                {
                    result.m_ShaderModules.push_back(module);
                    shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, shader.m_ShaderStage, result.m_ShaderModules[index], shader.m_ShaderEntryPoint.c_str(), nullptr });
                }
                ++index;
            }


            //Vertex input
            VkPipelineVertexInputStateCreateInfo vertexInfo{};
            vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(a_CreateInfo.vertexData.m_VertexBindings.size());
            vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(a_CreateInfo.vertexData.m_VertexAttributes.size());
            vertexInfo.pVertexBindingDescriptions = vertexInfo.vertexBindingDescriptionCount == 0 ? nullptr : a_CreateInfo.vertexData.m_VertexBindings.data();
            vertexInfo.pVertexAttributeDescriptions = vertexInfo.vertexAttributeDescriptionCount == 0 ? nullptr : a_CreateInfo.vertexData.m_VertexAttributes.data();

            //Input assembly state
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = false;

            //Viewport
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = static_cast<float>(a_CreateInfo.resolution.m_ResolutionY); //Note: Nomally 0, but since Y is flipped..
            viewport.width = static_cast<float>(a_CreateInfo.resolution.m_ResolutionX);
            viewport.height = -static_cast<float>(a_CreateInfo.resolution.m_ResolutionY); //NOTE: Vulkan has Y inverted, so negative here flips it back!
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = VkExtent2D{ a_CreateInfo.resolution.m_ResolutionX, a_CreateInfo.resolution.m_ResolutionY };
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
            rasterizationState.cullMode = a_CreateInfo.culling.m_CullMode;    //Same as GL
            rasterizationState.frontFace = a_CreateInfo.culling.m_FrontFace; //Same as GL
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
            depthStencilState.depthTestEnable = a_CreateInfo.depth.m_UseDepth;
            depthStencilState.depthWriteEnable = a_CreateInfo.depth.m_WriteDepth;
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

            std::vector< VkPipelineColorBlendAttachmentState> blending;
            blending.resize(a_CreateInfo.attachments.m_NumAttachments, colorBlendAttachment);

            VkPipelineColorBlendStateCreateInfo colorBlendState{};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.logicOpEnable = VK_FALSE;
            colorBlendState.logicOp = VK_LOGIC_OP_COPY;
            colorBlendState.attachmentCount = a_CreateInfo.attachments.m_NumAttachments;
            colorBlendState.pAttachments = blending.data();
            colorBlendState.blendConstants[0] = 0.0f;

            //The pipeline layout.
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(a_CreateInfo.descriptors.m_Layouts.size());
            pipelineLayoutInfo.pSetLayouts = (pipelineLayoutInfo.setLayoutCount > 0 ? a_CreateInfo.descriptors.m_Layouts.data() : nullptr);
            pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(a_CreateInfo.pushConstants.m_PushConstantRanges.size());
            pipelineLayoutInfo.pPushConstantRanges = pipelineLayoutInfo.pushConstantRangeCount > 0 ? a_CreateInfo.pushConstants.m_PushConstantRanges.data() : nullptr;

            if (vkCreatePipelineLayout(a_Device, &pipelineLayoutInfo, nullptr, &result.m_PipelineLayout) != VK_SUCCESS)
            {
                printf("Could not create pipeline layout for rendering pipeline!\n");
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
            psoInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
            psoInfo.pStages = shaderStages.data();
            psoInfo.pVertexInputState = &vertexInfo;
            psoInfo.pInputAssemblyState = &inputAssembly;
            psoInfo.pViewportState = &viewportState;
            psoInfo.pRasterizationState = &rasterizationState;
            psoInfo.pMultisampleState = &multiSampleState;
            psoInfo.pDepthStencilState = &depthStencilState;
            psoInfo.pColorBlendState = &colorBlendState;
            psoInfo.layout = result.m_PipelineLayout;
            psoInfo.renderPass = a_CreateInfo.renderPass.m_RenderPass;
            psoInfo.subpass = a_CreateInfo.renderPass.m_SubpassIndex;

            psoInfo.pDynamicState = nullptr;
            psoInfo.basePipelineHandle = nullptr;
            psoInfo.basePipelineIndex = -1;

            if (vkCreateGraphicsPipelines(a_Device, VK_NULL_HANDLE, 1, &psoInfo, nullptr, &result.m_Pipeline) != VK_SUCCESS)
            {
                printf("Could not create graphics pipeline!\n");
                return false;
            }

            a_Result = result;
            return true;
        }
    };
}