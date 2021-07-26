#pragma once
#include <glm/glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

#include "Resources.h"
#include "RenderUtility.h"
#include "vk_mem_alloc.h"

namespace egg
{

	//Forward declare the settings used for rendering.
	struct RenderData;
	struct QueueInfo;

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
		 * Stall the CPU till all in-flight resources are idle.
		 */
		virtual void WaitForIdle(const RenderData& a_RenderData) = 0;

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
		void WaitForIdle(const RenderData& a_RenderData) override;
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

		void SetDrawData(const DrawData& a_Data);

		bool Init(const RenderData& a_RenderData) override;

		bool CleanUp(const RenderData& a_RenderData) override;

		bool RecordCommandBuffer(const RenderData& a_RenderData, VkCommandBuffer& a_CommandBuffer,
			const uint32_t a_CurrentFrameIndex, std::vector<VkSemaphore>& a_WaitSemaphores,
			std::vector<VkSemaphore>& a_SignalSemaphores, std::vector<VkPipelineStageFlags>& a_WaitStageFlags) override;

		void WaitForIdle(const RenderData& a_RenderData) override;
	private:
		/*
		 * Draw call data pointer (valid during the frame).
		 */
		const DrawData* m_DrawData;

		/*
		 * Pipeline objects for the deferred rendering stage.
		 */
		PipelineData m_DeferredPipelineData;			//Used to write to the array images (pos, normal, tangent, uv) and to the depth buffer.
		PipelineData m_DeferredProcessedPipelineData;	//Reads the array images and depth buffer, then outputs to the swapchain.
		VkRenderPass m_DeferredRenderPass;				//Multiple sub-passes that use the above pipelines.

		/*
		 * The indices at which each attachment is bound.
		 */
		enum EDeferredFrameAttachments
		{
			DEFERRED_ATTACHMENT_DEPTH = 0,
			DEFERRED_ATTACHMENT_POSITION,
			DEFERRED_ATTACHMENT_NORMAL,
			DEFERRED_ATTACHMENT_TANGENT,
			DEFERRED_ATTACHMENT_UV_MATERIAL_ID,

			//Maximum enum value used to iterate.
			DEFERRED_ATTACHMENT_MAX_ENUM
		};

		/*
		 * Instance data that is packed and aligned correctly.
		 * For now just a using directive aliasing the instance struct.
		 */
		using PackedInstanceData = MeshInstance;

		struct GpuDrawCallData
		{
			std::shared_ptr<Mesh> m_Mesh;
			uint32_t m_InstanceOffset;
			uint32_t m_InstanceCount;
		};

		struct InstanceData
		{
			//Buffer on the GPU containing all instance data for a frame.
			VmaAllocation m_InstanceBufferAllocation;
			VkBuffer m_InstanceDataBuffer;
			VmaAllocation m_InstanceStagingBufferAllocation;
			VkBuffer m_InstanceStagingDataBuffer;
			VmaAllocationInfo m_StagingBufferInfo;
			VmaAllocationInfo m_GpuBufferInfo;
			size_t m_InstanceBufferEntrySize;	//The amount of instance data objects that fit in this buffer.

			//The upload command buffer and pool. The semaphore signals when uploading is finished.
			VkCommandPool m_UploadCommandPool;
			VkCommandBuffer m_UploadCommandBuffer;
			VkSemaphore m_UploadSemaphore;
			VkFence m_UploadFence;

			//Instance data descriptor set containing the buffer.
			VkDescriptorSet m_InstanceDataDescriptorSet;

			//A copy of the camera and all offsets and mesh data.
			Camera m_Camera;
			std::vector<GpuDrawCallData> m_GpuDrawCallDatas;

		};

		/*
		 * Storage for the attachments for the deferred stage.
		 */
		struct DeferredFrame
		{
			//A 2D texture array for all of the attachments.
			//Each attachment gets its own view.
			//The depth is a separate texture due to the different format and type.
			ImageData m_DeferredArrayImage;
			ImageData m_DepthImage;
			VkImageView m_DeferredImageViews[DEFERRED_ATTACHMENT_MAX_ENUM + 1];	//The +1 is for the swap chain's ouput view.

			//The framebuffer used to render to the deferred 2d image array.
			VkFramebuffer m_DeferredBuffer;
			VkDescriptorSet m_DescriptorSet;
		};

		//The queue used for uploading instance data.
		const QueueInfo* m_UploadQueue;

		//Descriptor pool and set for the deferred processing.
		VkDescriptorPool m_ProcessingDescriptorPool;
		VkDescriptorSetLayout m_ProcessingDescriptorSetLayout;

		//Descriptor pool and set layout for the instance data.
		VkDescriptorPool m_InstanceDescriptorPool;
		VkDescriptorSetLayout m_InstanceDescriptorSetLayout;

		//Separate buffers for each frame.
		std::vector<DeferredFrame> m_Frames;

		//Since the swapchain is deferred, swapCount + 1 instance buffers are needed to ensure
		uint32_t m_CurrentInstanceIndex;
		std::vector<InstanceData> m_InstanceDatas;
	};
}