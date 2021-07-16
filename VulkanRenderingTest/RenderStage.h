#pragma once
#include <glm/glm/glm.hpp>
#include <vulkan/vulkan.h>

//Forward declare the settings used for rendering.
struct RendererSettings;
struct Frame;

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
	virtual bool Init(const RendererSettings& a_Settings, const uint32_t a_SwapBufferCount, VkDevice& a_Device) = 0;

	/*
	 * Deallocate any resources that were created by this render stage.
	 */
	virtual bool CleanUp(VkDevice& a_Device) = 0;
	
	/*
	 * Record commands in the given command buffer for this stage.
	 */
	virtual bool RecordCommandBuffer(const RendererSettings& a_Settings, VkCommandBuffer& a_CommandBuffer, const uint32_t currentFrameIndex, Frame& a_FrameData) = 0;

	void SetEnabled(bool a_Enabled) { m_Enabled = a_Enabled; }
	bool IsEnabled() { return m_Enabled; }

private:
	bool m_Enabled;
};

class RenderStage_Deferred : public RenderStage
{
public:
	bool Init(const RendererSettings& a_Settings, const uint32_t a_SwapBufferCount, VkDevice& a_Device) override;
	bool RecordCommandBuffer(const RendererSettings& a_Settings, VkCommandBuffer& a_CommandBuffer, const uint32_t currentFrameIndex, Frame& a_FrameData) override;
	bool CleanUp(VkDevice& a_Device) override;

	/*
	 * Get a reference to the render pass (layout is required for constructing frame buffers).
	 */
	VkRenderPass& GetRenderPass();

private:
	VkPipeline m_Pipeline;					//The pipeline containing all state used for rendering.
	VkShaderModule m_VertexShader;			//The vertex shader for the graphics pipeline.
	VkShaderModule m_FragmentShader;		//The fragment shader for the graphics pipeline.
	VkPipelineLayout m_PipelineLayout;		//The layout of the deferred graphics pipeline.
	VkRenderPass m_RenderPass;				//The render pass used for deferred rendering.
};