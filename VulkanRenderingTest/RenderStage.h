#pragma once
#include <glm/glm/glm.hpp>
#include <vulkan/vulkan.h>

//Forward declare the settings used for rendering.
struct RenderData;

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
	VkPipeline m_Pipeline;					//The pipeline containing all state used for rendering.
	VkShaderModule m_VertexShader;			//The vertex shader for the graphics pipeline.
	VkShaderModule m_FragmentShader;		//The fragment shader for the graphics pipeline.
	VkPipelineLayout m_PipelineLayout;		//The layout of the deferred graphics pipeline.
	VkRenderPass m_RenderPass;				//The render pass used for deferred rendering.
};