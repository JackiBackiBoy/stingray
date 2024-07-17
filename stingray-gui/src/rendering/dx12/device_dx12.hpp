#pragma once

#include "../device.hpp"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <string>
#include <vector>

using namespace Microsoft::WRL;

namespace sr {
	class GraphicsDevice_DX12 final : public GraphicsDevice {
	public:
		GraphicsDevice_DX12(int width, int height, HWND window);
		~GraphicsDevice_DX12();

		/* Resource Creation */
		void createBuffer(const BufferInfo& info, Buffer& buffer, const void* data) override;
		void createPipeline(const PipelineInfo& info, Pipeline& pipeline) override;
		void createSampler(const SamplerInfo& info, Sampler& sampler) override;
		void createShader(ShaderStage stage, const std::wstring& path, Shader& shader) override;
		void createSwapChain(const SwapChainInfo& info, SwapChain& swapChain, HWND window) override;
		void createTexture(const TextureInfo& info, Texture& texture, const SubresourceData* data) override;
		void createShaderTable(const RayTracingPipeline& rtPipeline, Buffer& table, const std::string& exportName) override;

		/* Ray Tracing */
		void createRayTracingAS(const RayTracingASInfo& info, RayTracingAS& bvh) override;
		void buildRayTracingAS(const RayTracingAS& dst, const RayTracingAS* src, const CommandList& cmdList) override;
		void writeTLASInstance(const RayTracingTLAS::Instance& instance, void* dest) override;
		void createRayTracingPipeline(const RayTracingPipelineInfo& info, RayTracingPipeline& rtPipeline) override;
		void bindRayTracingPipeline(const RayTracingPipeline& rtPipeline, const Texture& rtOutputUAV, const CommandList& cmdList) override;
		void bindRayTracingResource(const Resource& res, const std::string& name, const RayTracingPipeline& rtPipeline, const CommandList& cmdList) override;
		void createRayTracingInstanceBuffer(Buffer& buffer, uint32_t numBottomLevels) override;
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
		void beginRenderPass(const SwapChain& swapChain, const RenderPassInfo& renderPass, const CommandList& cmdList, bool clearTargets) override;
		void beginRenderPass(const RenderPassInfo& renderPass, const CommandList& cmdList, bool clearTargets) override;
		void endRenderPass(const SwapChain& swapChain, const CommandList& cmdList) override;
		void endRenderPass() override;
		void submitCommandLists(SwapChain& swapChain) override;

		void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) override;
		void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) override;
		void drawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) override;

		uint32_t getDescriptorIndex(const Resource& resource) const override;

		void waitForGPU() const override;

	private:
		struct CommandList_DX12 {
			QueueType type = {};
			ComPtr<ID3D12CommandList> cmdList = nullptr;

			inline ID3D12GraphicsCommandList4* getGraphicsCommandList() { return (ID3D12GraphicsCommandList4*)cmdList.Get(); }
		};

		struct CommandQueue {
			ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
			std::vector<ID3D12CommandList*> submittedCommandLists = {};
		};
		
		struct CopyAllocator { // NOTE: This is only used for immediate copy commands
			GraphicsDevice_DX12* device = nullptr;

			struct CopyCMD {
				ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
				ComPtr<ID3D12GraphicsCommandList> cmdList = nullptr;
				ComPtr<ID3D12Fence> fence = nullptr;
				uint64_t fenceWaitForValue = 0;
			};

			void init(GraphicsDevice_DX12* device);
			CopyCMD allocate(uint64_t size);
			void submit(CopyCMD& command);
		};

		struct Descriptor {
			D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
			uint32_t index = 0;

			void init(GraphicsDevice_DX12* device, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, ID3D12Resource* res);
			void init(GraphicsDevice_DX12* device, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, ID3D12Resource* res);
			void init(GraphicsDevice_DX12* device, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, ID3D12Resource* res);
			void init(GraphicsDevice_DX12* device, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, ID3D12Resource* res);
		};

		struct DescriptorHeap {
			D3D12_DESCRIPTOR_HEAP_TYPE type = {};
			uint32_t capacity = 0U;
			GraphicsDevice_DX12* device = nullptr;

			ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
			uint32_t descriptorSize = 0U;

			D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandleFromStart = {};
			D3D12_CPU_DESCRIPTOR_HANDLE currentDescriptorHandle = {};

			D3D12_CPU_DESCRIPTOR_HANDLE getCurrentDescriptorHandle() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getDescriptorHandle(uint32_t index) const;
			int getDescriptorIndex(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

			void init(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, GraphicsDevice_DX12* device);
			void offsetCurrentDescriptorHandle(uint32_t offset = 1U);
		};

		struct Resource_DX12 {
			D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = NULL;
			ComPtr<ID3D12Resource> resource = nullptr;
		};

		struct Buffer_DX12 : public Resource_DX12 {
			BufferInfo info = {};

			Descriptor srvDescriptor = {};
		};

		struct RayTracingAS_DX12 : public Resource_DX12 {
			RayTracingASInfo info = {};

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS desc = {};
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
			std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries = {};
			Buffer scratchBuffer = {};
		};

		struct Texture_DX12 : public Resource_DX12 {
			TextureInfo info = {};
			SubresourceType subResourceType = {};

			Descriptor srvDescriptor = {};
			Descriptor rtvDescriptor = {};
			Descriptor dsvDescriptor = {};
			Descriptor uavDescriptor = {};
		};

		void checkRayTracingSupport();
		void createDXGIFactory();
		void createDevice();
		void createCommandAllocators(); // TODO: This shit sucks, but it works. Need to make it more dynamic
		void createCommandQueues();
		void createDescriptorHeaps();

		#ifdef _DEBUG
			ComPtr<ID3D12Debug1> m_DebugController = nullptr;
			ComPtr<ID3D12DebugDevice> m_DebugDevice = nullptr;
		#endif

		ComPtr<ID3D12Device5> m_Device = nullptr;
		ComPtr<IDXGIAdapter4> m_DeviceAdapter = nullptr;
		ComPtr<IDXGIFactory4> m_DXGIFactory = nullptr;

		static constexpr size_t QUEUE_COUNT = (size_t)QueueType::QUEUE_COUNT;

		ComPtr<ID3D12CommandAllocator> m_CommandAllocators[QUEUE_COUNT][NUM_BUFFERS] = {}; // For each queue type: store NUM_BUFFERS number of command allocators
		CommandQueue m_CommandQueues[QUEUE_COUNT] = {}; // One command queue for each queue type
		std::vector<std::unique_ptr<CommandList_DX12>> m_CommandLists = {};
		ComPtr<ID3D12Fence> m_FrameFences[NUM_BUFFERS][QUEUE_COUNT] = {};

		// Descriptors
		DescriptorHeap m_RTVDescriptorHeap = {};
		DescriptorHeap m_DSVDescriptorHeap = {};
		DescriptorHeap m_ResourceDescriptorHeap = {}; // NOTE: CBV_SRV_UAV bindless heap
		DescriptorHeap m_SamplerDescriptorHeap = {};

		CopyAllocator m_CopyAllocator = {};
		uint32_t m_CommandCounter = 0;
		bool m_AllowTearing = false;
	};
}