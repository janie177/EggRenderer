#pragma once
#include <glm/glm/glm.hpp>
#include <vulkan/vulkan.h>

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
		const uint32_t currentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
		std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags) override;
private:
	VkPipeline m_Pipeline;
	VkShaderModule m_VertexShader;
	VkShaderModule m_FragmentShader;
	VkPipelineLayout m_PipelineLayout;
	VkRenderPass m_RenderPass;
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
	VkPipelineLayout m_DeferredPipelineLayout;
	VkPipeline m_DeferredPipeline;
	VkShaderModule m_DeferredVertexShader;
	VkShaderModule m_DeferredFragmentShader;
	VkRenderPass m_DeferredRenderPass;

	/*
	 * Pipeline objects for the shading stage.
	 */
	VkPipelineLayout m_ShadingPipelineLayout;
	VkPipeline m_ShadingPipeline;
	VkShaderModule m_ShadingVertexShader;
	VkShaderModule m_ShadingFragmentShader;
	VkRenderPass m_ShadingRenderPass;
};