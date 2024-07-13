#pragma once

#include "graphics.hpp"

#include <windows.h>
#include <cstdint>
#include <string>

namespace sr {
	class GraphicsDevice {
	public:
		GraphicsDevice() = default;
		virtual ~GraphicsDevice() = default;

		inline uint32_t getBufferIndex() const { return m_BufferIndex; }

		virtual void createBuffer(const BufferInfo& info, Buffer& buffer, const void* data) = 0;
		virtual void createPipeline(const PipelineInfo& info, Pipeline& pipeline) = 0;
		virtual void createSampler(const SamplerInfo& info, Sampler& sampler) = 0;
		virtual void createShader(ShaderStage stage, const std::wstring& path, Shader& shader) = 0;
		virtual void createSwapChain(const SwapChainInfo& info, SwapChain& swapChain, HWND window) = 0;
		virtual void createTexture(const TextureInfo& info, Texture& texture, const SubresourceData* data) = 0;
		virtual void createShaderTable(const RayTracingPipeline& rtPipeline, Buffer& table, const std::string& exportName) = 0;

		/* Ray Tracing */
		virtual void createRayTracingAS(const RayTracingASInfo& info, RayTracingAS& bvh) = 0;
		virtual void buildRayTracingAS(const RayTracingAS& dst, const RayTracingAS* src, const CommandList& commandList) = 0;
		virtual void writeTLASInstance(const RayTracingTLAS::Instance& instance, void* dest) = 0;
		virtual void createRayTracingPipeline(const RayTracingPipelineInfo& info, RayTracingPipeline& rtPipeline) = 0;
		virtual void bindRayTracingPipeline(const RayTracingPipeline& rtPipeline, const Texture& rtOutputUAV, const CommandList& commandList) = 0;
		virtual void bindRayTracingConstantBuffer(const Buffer& uniformBuffer, const std::string& name, const RayTracingPipeline& rtPipeline, const CommandList& commandList) = 0;
		virtual void bindRayTracingAS(const RayTracingAS& accelerationStructure, const std::string& name, const RayTracingPipeline& rtPipeline, const CommandList& commandList) = 0;
		virtual void createRayTracingInstanceBuffer(Buffer& buffer, uint32_t numBottomLevels) = 0;
		virtual void dispatchRays(const DispatchRaysInfo& info, const CommandList& commandList) = 0;

		virtual void bindPipeline(const Pipeline& pipeline, const CommandList& commandList) = 0;
		virtual void bindViewport(const Viewport& viewport, const CommandList& commandList) = 0;
		virtual void bindVertexBuffer(const Buffer& vertexBuffer, const CommandList& commandList) = 0;
		virtual void bindIndexBuffer(const Buffer& indexBuffer, const CommandList& commandList) = 0;
		virtual void bindSampler(const Sampler& sampler) = 0;
		virtual void bindStructuredBuffer(const Buffer& buffer, const std::string& name, const Pipeline& pipeline, const CommandList& commandList) = 0; // TODO: These could likely also benefit from being inside ONE function
		virtual void bindUniformBuffer(const Buffer& uniformBuffer, const std::string& name, const Pipeline& pipeline, const CommandList& commandList) = 0; // ^
		virtual void bindResource(const Resource& resource, uint32_t slot, ShaderStage stages) = 0;
		virtual void pushConstants(const void* data, uint32_t size, const CommandList& commandList) = 0;
		virtual void barrier(const GPUBarrier& barrier, const CommandList& commandList) = 0;

		virtual CommandList beginCommandList(QueueType type) = 0;
		virtual void beginRenderPass(const SwapChain& swapChain, const RenderPassInfo& renderPass, const CommandList& commandList, bool clearTargets = true) = 0;
		virtual void beginRenderPass(const RenderPassInfo& renderPass, const CommandList& commandList, bool clearTargets = true) = 0;
		virtual void endRenderPass(const SwapChain& swapChain, const CommandList& commandList) = 0;
		virtual void endRenderPass() = 0;
		virtual void submitCommandLists(SwapChain& swapChain) = 0;

		// TODO: TEMPORARY FUNCTIONS!!
		virtual void beginRTRenderPass(const SwapChain& swapChain, const CommandList& commandList) = 0;
		virtual void copyRTOutputToBackbuffer(const SwapChain& swapChain, const Texture& rtOutput, const CommandList& commandList) = 0;

		virtual void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& commandList) = 0;
		virtual void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& commandList) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& commandList) = 0;

		virtual int getDescriptorIndex(const Resource& resource) const = 0; // TODO: Perhaps support subresources (we're not there yet though ;))
		virtual void waitForGPU() const = 0;

		static constexpr size_t NUM_BUFFERS = 3; // Triple buffering by default
		static constexpr uint32_t MAX_TEXTURE_DESCRIPTORS = 1024U;
		static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 8U;
		static constexpr uint32_t MAX_RTV_DESCRIPTORS = 32U;

	protected:
		uint32_t m_BufferIndex = 0U;

	#ifdef _DEBUG
		static constexpr bool m_DebugEnabled = true;
	#else
		static constexpr bool m_DebugEnabled = false;
	#endif
	};
}