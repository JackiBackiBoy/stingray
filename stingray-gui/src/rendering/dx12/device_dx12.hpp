#pragma once

#include "../device.hpp"

namespace sr {
	class GraphicsDevice_DX12 final : public GraphicsDevice {
	public:
		GraphicsDevice_DX12(int width, int height, void* window);
		~GraphicsDevice_DX12();

		/* Resource Creation */
		void createBuffer(const BufferInfo& info, Buffer& buffer, const void* data) override;
		void createPipeline(const PipelineInfo& info, Pipeline& pipeline) override;
		void createSampler(const SamplerInfo& info, Sampler& sampler) override;
		void createShader(ShaderStage stage, const std::string& path, Shader& shader) override;
		void createSwapChain(const SwapChainInfo& info, SwapChain& swapChain, void* window) override;
		void createTexture(const TextureInfo& info, Texture& texture, const SubresourceData* data) override;
		void createShaderTable(const RTPipeline& rtPipeline, Buffer& table, const std::string& exportName) override;

		/* Ray Tracing */
		void createRTAS(const RayTracingASInfo& info, RayTracingAS& bvh) override;
		void buildRTAS(const RayTracingAS& dst, const RayTracingAS* src, const CommandList& cmdList) override;
		void writeTLASInstance(const RayTracingTLAS::Instance& instance, void* dest) override;
		void createRTPipeline(const RTPipelineInfo& info, RTPipeline& rtPipeline) override;
		void bindRTPipeline(const RTPipeline& rtPipeline, const Texture& rtOutputUAV, const CommandList& cmdList) override;
		void bindRTResource(const Resource& res, const std::string& name, const RTPipeline& rtPipeline, const CommandList& cmdList) override;
		void createRTInstanceBuffer(Buffer& buffer, uint32_t numBottomLevels) override;
		void dispatchRays(const DispatchRaysInfo& info, const CommandList& cmdList) override;

		/* Resource binding */
		void bindPipeline(const Pipeline& pipeline, const CommandList& cmdList) override;
		void bindViewport(const Viewport& viewport, const CommandList& cmdList) override;
		void bindVertexBuffer(const Buffer& vertexBuffer, const CommandList& cmdList) override;
		void bindIndexBuffer(const Buffer& indexBuffer, const CommandList& cmdList) override;
		void bindSampler(const Sampler& sampler) override;
		void bindResource(const Resource& res, const std::string& name, const Pipeline& pipeline, const CommandList& cmdList) override;
		void copyResource(const Resource* dst, const Resource* src, const CommandList& cmdList) override;
		void pushConstants(const void* data, uint32_t size, const CommandList& cmdList) override;
		void pushConstantsCompute(const void* data, uint32_t size, const CommandList& cmdList) override;
		void barrier(const GPUBarrier& barrier, const CommandList& cmdList) override;

		/* Commands and renderpasses */
		CommandList beginCommandList(QueueType type) override;
		void beginRenderPass(const SwapChain& swapChain, const PassInfo& renderPass, const CommandList& cmdList, bool clearTargets) override;
		void beginRenderPass(const PassInfo& renderPass, const CommandList& cmdList, bool clearTargets) override;
		void endRenderPass(const SwapChain& swapChain, const CommandList& cmdList) override;
		void endRenderPass() override;
		void submitCommandLists(SwapChain& swapChain) override;

		void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) override;
		void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) override;
		void drawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) override;

		uint32_t getDescriptorIndex(const Resource& resource) const override;

		void waitForGPU() const override;

	private:
		struct Impl;
		Impl* m_Impl = nullptr;
	};
}