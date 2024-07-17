#pragma once

#include <windows.h>
#include <cstdint>
#include <string>

#include "graphics.hpp"

namespace sr {
	class GraphicsDevice {
	public:
		GraphicsDevice() = default;
		virtual ~GraphicsDevice() = default;

		inline virtual uint32_t getBufferIndex() const final { return m_BufferIndex; }
		inline virtual uint64_t getFrameCount() const final { return m_FrameCount; }

		/* Resource Creation */
		virtual void createBuffer(const BufferInfo& info, Buffer& buffer, const void* data) = 0;
		virtual void createPipeline(const PipelineInfo& info, Pipeline& pipeline) = 0;
		virtual void createSampler(const SamplerInfo& info, Sampler& sampler) = 0;
		virtual void createShader(ShaderStage stage, const std::wstring& path, Shader& shader) = 0;
		virtual void createSwapChain(const SwapChainInfo& info, SwapChain& swapChain, HWND window) = 0;
		virtual void createTexture(const TextureInfo& info, Texture& texture, const SubresourceData* data) = 0;
		virtual void createShaderTable(const RayTracingPipeline& rtPipeline, Buffer& table, const std::string& exportName) = 0;

		/* Ray Tracing */
		virtual void createRayTracingAS(const RayTracingASInfo& info, RayTracingAS& bvh) = 0;
		virtual void buildRayTracingAS(const RayTracingAS& dst, const RayTracingAS* src, const CommandList& cmdList) = 0;
		virtual void writeTLASInstance(const RayTracingTLAS::Instance& instance, void* dest) = 0;
		virtual void createRayTracingPipeline(const RayTracingPipelineInfo& info, RayTracingPipeline& rtPipeline) = 0;
		virtual void bindRayTracingPipeline(const RayTracingPipeline& rtPipeline, const Texture& rtOutputUAV, const CommandList& cmdList) = 0;
		virtual void bindRayTracingResource(const Resource& res, const std::string& name, const RayTracingPipeline& rtPipeline, const CommandList& cmdList) = 0;
		virtual void createRayTracingInstanceBuffer(Buffer& buffer, uint32_t numBottomLevels) = 0;
		virtual void dispatchRays(const DispatchRaysInfo& info, const CommandList& cmdList) = 0;

		/* Resource binding */
		virtual void bindPipeline(const Pipeline& pipeline, const CommandList& cmdList) = 0;
		virtual void bindViewport(const Viewport& viewport, const CommandList& cmdList) = 0;
		virtual void bindVertexBuffer(const Buffer& vertexBuffer, const CommandList& cmdList) = 0;
		virtual void bindIndexBuffer(const Buffer& indexBuffer, const CommandList& cmdList) = 0;
		virtual void bindSampler(const Sampler& sampler) = 0;
		virtual void bindResource(const Resource& res, const std::string& name, const Pipeline& pipeline, const CommandList& cmdList) = 0;
		virtual void pushConstants(const void* data, uint32_t size, const CommandList& cmdList) = 0;
		virtual void pushConstantsCompute(const void* data, uint32_t size, const CommandList& cmdList) = 0;
		virtual void barrier(const GPUBarrier& barrier, const CommandList& cmdList) = 0;

		/* Commands and renderpasses */
		virtual CommandList beginCommandList(QueueType type) = 0;
		virtual void beginRenderPass(const SwapChain& swapChain, const RenderPassInfo& renderPass, const CommandList& cmdList, bool clearTargets = true) = 0;
		virtual void beginRenderPass(const RenderPassInfo& renderPass, const CommandList& cmdList, bool clearTargets = true) = 0;
		virtual void endRenderPass(const SwapChain& swapChain, const CommandList& cmdList) = 0;
		virtual void endRenderPass() = 0;
		virtual void submitCommandLists(SwapChain& swapChain) = 0;

		virtual void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) = 0;
		virtual void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) = 0;

		virtual uint32_t getDescriptorIndex(const Resource& resource) const = 0; // TODO: Perhaps support subresources (we're not there yet though ;))
		virtual void waitForGPU() const = 0;

		static constexpr size_t NUM_BUFFERS = 3; // Triple buffering by default
		static constexpr uint32_t MAX_TEXTURE_DESCRIPTORS = 1024;
		static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 8;
		static constexpr uint32_t MAX_RTV_DESCRIPTORS = 32;

	protected:
		uint32_t m_BufferIndex = 0;
		uint64_t m_FrameCount = 0;
	};
}