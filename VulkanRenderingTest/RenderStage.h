#pragma once
#include <glm/glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

#include "RenderUtility.h"
#include "vk_mem_alloc.h"

//Forward declare the settings used for rendering.
struct RenderData;

/*
 * 128 byte struct to send data to the shader quickly.
 */
struct DeferredPushConstants
{
	glm::mat4 m_VPMatrix;	//Camera view projection matrix.
	glm::vec4 m_Data1;		//Anything can be stored in these.
	glm::vec4 m_Data2;
	glm::vec4 m_Data3;
	glm::vec4 m_Data4;
};

/*
 * The basic render stage class that is derived from.
 */
class RenderStage
{
public:
	RenderStage() : m_Enabled(true) {}
	virtual ~RenderStage() = default;
	
	/*
	 * Initialize this render stage.
	 * Provices the Vulkan logical device and the amount of buffers in the swap chain.
	 */
	virtual bool Init(const RenderData& a_RenderData) = 0;
	
	/*
	 * Deallocate any resources that were created by this render stage.
	 */
	virtual bool CleanUp(const RenderData& a_RenderData) = 0;
	
	/*
	 * Record commands in the given command buffer for this stage.
	 * RenderData contains all information about the renderer (device, memory allocator etc).
	 * The command buffer provided is the main rendering command buffer.
	 * The current frame index is the index of the frame that is currently being written to in the swap chain.
	 * WaitSemaphores and SignalSemaphores are two vectors that can have semaphores added to them.
	 * Upon execution, the command buffer provided will wait for all wait semaphores.
	 * All signal semaphores will be signaled when the command buffer is done executing.
	 * The wait stage flags vector requires a stage defined to wait in for every wait semaphore that is added.
	 * Failing to do this will terminate the program.
	 *
	 */
	virtual bool RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer, const uint32_t currentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores, std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags) = 0;

	/*
	 * Enable or disable this render stage.
	 */
	void SetEnabled(bool a_Enabled) { m_Enabled = a_Enabled; }

	/*
	 * See whether or not this render stage is enabled.
	 */
	bool IsEnabled() const { return m_Enabled; }

private:
	bool m_Enabled;
};

class RenderStage_HelloTriangle : public RenderStage
{
public:
	/*
	 * Get a reference to the render pass (layout is required for constructing frame buffers).
	 */
	VkRenderPass& GetRenderPass();

	bool Init(const RenderData& a_RenderData) override;
	
	bool CleanUp(const RenderData& a_RenderData) override;
	bool RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
		const uint32_t a_CurrentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
		std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags) override;
private:
	VkPipeline m_Pipeline;
	VkShaderModule m_VertexShader;
	VkShaderModule m_FragmentShader;
	VkPipelineLayout m_PipelineLayout;
	VkRenderPass m_RenderPass;
	std::vector<VkFramebuffer> m_FrameBuffers;	//Framebuffers for each frame.
};

class RenderStage_Deferred : public RenderStage
{
public:
	/*
	 * Get a reference to the render pass (layout is required for constructing frame buffers).
	 */
	VkRenderPass& GetRenderPass();

	bool Init(const RenderData& a_RenderData) override;

	bool CleanUp(const RenderData& a_RenderData) override;

	bool RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
		const uint32_t currentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
		std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags) override;
private:
	/*
	 * Pipeline objects for the deferred rendering stage.
	 */
	PipelineData m_DeferredPipelineData;
	VkRenderPass m_DeferredRenderPass;

	/*
	 * Pipeline objects for the shading stage.
	 */
	VkPipelineLayout m_ShadingPipelineLayout;
	VkPipeline m_ShadingPipeline;
	VkShaderModule m_ShadingVertexShader;
	VkShaderModule m_ShadingFragmentShader;
	VkRenderPass m_ShadingRenderPass;

	enum DeferredAttachments
	{
		DEFFERED_ATTACHMENT_DEPTH,						//Only stores depth.
		DEFFERED_ATTACHMENT_POSITIONS,					//Stores the world space position and W component for projection.
		DEFFERED_ATTACHMENTS_NORMALS_MATERIALIDS,		//Stores the world space normal and material ID in the W component.
		DEFFERED_ATTACHMENTS_TANGENTS,					//Stores the tangents in the XYX components.
		DEFFERED_ATTACHMENTS_UV,						//Stores the UV coordinates.

		DEFERRED_OUTPUT_DEPTH,							//The depth output of the deferred pass after shading.
		DEFERRED_OUTPUT_COLOR,							//The color output for the deferred pass after shading.

		DEFERRED_NUM_ATTACHMENTS						//Last element indicating number of attachments.
	};

	/*
     * Storage for the attachments for the deferred stage.
     */
	struct DeferredFrame
	{
		std::array<VkImage, DEFERRED_NUM_ATTACHMENTS> m_Images;
		std::array<VmaAllocation, DEFERRED_NUM_ATTACHMENTS> m_ImageAllocations;
		std::array<VkImageView, DEFERRED_NUM_ATTACHMENTS> m_ImageViews;
		VkFramebuffer m_DeferredBuffer;
		VkFramebuffer m_OutputBuffer;

		//Ways to access the deferred images from the processing shader.
		VkDescriptorPool m_DescriptorPool;
		VkDescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSet;
	};

	//Separate buffers for each frame.
	std::vector<DeferredFrame> m_Frames;
};