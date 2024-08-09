#include "device_dx12.hpp"
#include "graphics_dx12.hpp"
#include "../../core/utilities.hpp"

#include "dxsdk/d3d12.h"
#include "dxsdk/d3dx12.h"
#include <dxgi.h>
#include <dxgi1_6.h>

// DirectX Shader Compiler
#include "dxc/dxcapi.h"
#include "dxc/d3d12shader.h"

#include <wrl.h>
#include <Windows.h>

#include <cassert>
#include <format>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

// D3D12 Agility SDK
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace sr {
	inline void ThrowIfFailed(HRESULT hr) {
		if (FAILED(hr)) {
			// Set a breakpoint on this line to catch DirectX API errors
			throw std::exception();
		}
	}

	/* ---------------------------------------------------------------------- */
	/*                   GraphicsDevice_DX12 Implementation                   */
	/* ---------------------------------------------------------------------- */
	struct GraphicsDevice_DX12::Impl {
		struct CopyAllocator { // NOTE: Only used for immediate copy commands
			Impl* impl = nullptr;

			struct CopyCMD {
				ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
				ComPtr<ID3D12GraphicsCommandList> cmdList = nullptr;
				ComPtr<ID3D12Fence> fence = nullptr;
				uint64_t fenceWaitForValue = 0;
			};

			void init(Impl* impl);
			CopyCMD allocate(uint64_t size);
			void submit(CopyCMD& command);
		};

		struct Descriptor {
			D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
			uint32_t index = 0;

			void init(Impl* device, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, ID3D12Resource* res);
			void init(Impl* device, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, ID3D12Resource* res);
			void init(Impl* device, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, ID3D12Resource* res);
			void init(Impl* device, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, ID3D12Resource* res);
		};

		struct DescriptorHeap {
			D3D12_DESCRIPTOR_HEAP_TYPE type = {};
			uint32_t capacity = 0U;
			Impl* impl = nullptr;

			ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
			uint32_t descriptorSize = 0U;

			D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandleFromStart = {};
			D3D12_CPU_DESCRIPTOR_HANDLE currentDescriptorHandle = {};

			D3D12_CPU_DESCRIPTOR_HANDLE getCurrentDescriptorHandle() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getDescriptorHandle(uint32_t index) const;
			int getDescriptorIndex(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

			void init(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, Impl* impl);
			void offsetCurrentDescriptorHandle(uint32_t offset = 1U);
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
		void createCommandAllocators();
		void createCommandQueues();
		void createDescriptorHeaps();

		#ifdef _DEBUG
		ComPtr<ID3D12Debug1> debugController = nullptr;
		ComPtr<ID3D12DebugDevice> debugDevice = nullptr;
		#endif

		ComPtr<ID3D12Device5> device = nullptr;
		ComPtr<IDXGIAdapter4> deviceAdapter = nullptr;
		ComPtr<IDXGIFactory4> dxgiFactory = nullptr;

		ComPtr<ID3D12CommandAllocator> commandAllocators[QUEUE_COUNT][NUM_BUFFERS] = {}; // For each queue type: store NUM_BUFFERS number of command allocators
		CommandQueue_DX12 commandQueues[QUEUE_COUNT] = {}; // One command queue for each queue type
		std::vector<std::unique_ptr<CommandList_DX12>> commandLists = {};
		ComPtr<ID3D12Fence> frameFences[NUM_BUFFERS][QUEUE_COUNT] = {};

		// Descriptors
		DescriptorHeap rtvDescriptorHeap = {};
		DescriptorHeap dsvDescriptorHeap = {};
		DescriptorHeap resourceDescriptorHeap = {}; // NOTE: CBV_SRV_UAV bindless heap
		DescriptorHeap samplerDescriptorHeap = {};

		CopyAllocator copyAllocator = {};
		uint32_t commandCounter = 0;
		bool allowTearing = false;
	};

	void GraphicsDevice_DX12::Impl::checkRayTracingSupport() {
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
		ThrowIfFailed(device->CheckFeatureSupport(
			D3D12_FEATURE_D3D12_OPTIONS5,
			&options5,
			sizeof(options5)
		));

		if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0) {
			throw std::runtime_error("DX12 ERROR: Ray-tracing not supported on GPU!");
		}
	}

	void GraphicsDevice_DX12::Impl::createDXGIFactory() {
		UINT dxgiFactoryFlags = 0U;

		// Create debug controller and specify factory flags
		#ifdef _DEBUG
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(true);

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		#endif

		HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
		BOOL allowTearing = FALSE;

		if (SUCCEEDED(result)) {
			ComPtr<IDXGIFactory5> dxgiFactory5 = nullptr; // used for checking feature support

			result = dxgiFactory.As(&dxgiFactory5);
			if (SUCCEEDED(result)) {
				result = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
			}
		}

		allowTearing = SUCCEEDED(result) && allowTearing;
	}

	void GraphicsDevice_DX12::Impl::createDevice() {
		// Enumerate available adapters
		ComPtr<IDXGIAdapter1> dxgiAdapter1;
		SIZE_T maxDedicatedVideoMemory = 0;

		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory) {

				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&deviceAdapter));
			}
		}

		// Device name
		DXGI_ADAPTER_DESC1 adapterDesc = {};
		deviceAdapter->GetDesc1(&adapterDesc);
		//m_DeviceName = sr::utilities::wStringToString(adapterDesc.Description);

		ThrowIfFailed(D3D12CreateDevice(
			deviceAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&device)
		));

		#ifdef _DEBUG
			device->QueryInterface(IID_PPV_ARGS(&debugDevice));
		#endif
	}

	void GraphicsDevice_DX12::Impl::createCommandAllocators() {
		// TODO: For now we only use direct commands, no copy command allocators
		for (size_t i = 0; i < NUM_BUFFERS; ++i) {
			ThrowIfFailed(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&commandAllocators[QueueType::DIRECT][i])
			));

			ThrowIfFailed(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COPY,
				IID_PPV_ARGS(&commandAllocators[QueueType::COPY][i])
			));

			ThrowIfFailed(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COMPUTE,
				IID_PPV_ARGS(&commandAllocators[QueueType::COMPUTE][i])
			));
		}
	}

	void GraphicsDevice_DX12::Impl::createCommandQueues() {
		// Direct command queue
		const D3D12_COMMAND_QUEUE_DESC directQueueDesc = {
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.NodeMask = 0
		};

		ThrowIfFailed(device->CreateCommandQueue(
			&directQueueDesc,
			IID_PPV_ARGS(&commandQueues[QueueType::DIRECT].commandQueue)
		));

		// Copy command queue
		const D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {
			.Type = D3D12_COMMAND_LIST_TYPE_COPY,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.NodeMask = 0
		};

		ThrowIfFailed(device->CreateCommandQueue(
			&copyQueueDesc,
			IID_PPV_ARGS(&commandQueues[QueueType::COPY].commandQueue)
		));

		// Compute command queue
		const D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {
			.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.NodeMask = 0
		};

		ThrowIfFailed(device->CreateCommandQueue(
			&computeQueueDesc,
			IID_PPV_ARGS(&commandQueues[QueueType::COMPUTE].commandQueue)
		));

		// Frame fences
		for (size_t f = 0; f < NUM_BUFFERS; ++f) {
			for (size_t q = 0; q < QUEUE_COUNT; ++q) {
				ThrowIfFailed(device->CreateFence(
					0,
					D3D12_FENCE_FLAG_NONE,
					IID_PPV_ARGS(&frameFences[f][q])
				));
			}
		}
	}

	void GraphicsDevice_DX12::Impl::createDescriptorHeaps() {
		rtvDescriptorHeap.init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS, this);
		dsvDescriptorHeap.init(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 8, this);
		resourceDescriptorHeap.init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_TEXTURE_DESCRIPTORS, this); // TODO: We should probably chain together sums of resource types here in the `capacity` parameter
		samplerDescriptorHeap.init(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS, this);
	}

	void GraphicsDevice_DX12::Impl::CopyAllocator::init(Impl* impl) {
		this->impl = impl;
	}

	GraphicsDevice_DX12::Impl::CopyAllocator::CopyCMD GraphicsDevice_DX12::Impl::CopyAllocator::allocate(uint64_t size) {
		CopyCMD command = {};

		ThrowIfFailed(impl->device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_COPY,
			IID_PPV_ARGS(&command.commandAllocator)
		));

		ThrowIfFailed(impl->device->CreateCommandList(
			0U,
			D3D12_COMMAND_LIST_TYPE_COPY,
			command.commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&command.cmdList)
		));
		command.cmdList->Close();

		ThrowIfFailed(impl->device->CreateFence(
			0U,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&command.fence)
		));

		command.commandAllocator->Reset();
		command.cmdList->Reset(command.commandAllocator.Get(), nullptr);

		return command;
	}

	void GraphicsDevice_DX12::Impl::CopyAllocator::submit(CopyCMD& command) {
		command.cmdList->Close();

		ID3D12CommandList* cmdLists[] = { command.cmdList.Get() };
		CommandQueue_DX12& commandQueue = impl->commandQueues[QueueType::COPY];
		commandQueue.commandQueue->ExecuteCommandLists(1, cmdLists);

		++command.fenceWaitForValue;

		// CPU wait until GPU has completed the execution of copy command
		commandQueue.commandQueue->Signal(command.fence.Get(), command.fenceWaitForValue);
		if (command.fence->GetCompletedValue() < command.fenceWaitForValue) {
			command.fence->SetEventOnCompletion(command.fenceWaitForValue, nullptr);
		}

		command.fenceWaitForValue = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice_DX12::Impl::DescriptorHeap::getCurrentDescriptorHandle() const {
		return currentDescriptorHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice_DX12::Impl::DescriptorHeap::getDescriptorHandle(uint32_t index) const {
		assert(index < capacity);

		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHandleFromStart;
		descriptorHandle.ptr += static_cast<SIZE_T>(descriptorSize * index);
		return descriptorHandle;
	}

	int GraphicsDevice_DX12::Impl::DescriptorHeap::getDescriptorIndex(D3D12_CPU_DESCRIPTOR_HANDLE handle) const {
		return static_cast<int>(handle.ptr - descriptorHandleFromStart.ptr) / descriptorSize;
	}

	void GraphicsDevice_DX12::Impl::DescriptorHeap::init(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, Impl* impl) {
		this->type = type;
		this->capacity = capacity;
		this->impl = impl;

		D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
			heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		}

		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = type,
			.NumDescriptors = capacity,
			.Flags = heapFlags,
			.NodeMask = 0U
		};

		ThrowIfFailed(impl->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));
		descriptorSize = impl->device->GetDescriptorHandleIncrementSize(type);
		descriptorHandleFromStart = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		currentDescriptorHandle = descriptorHandleFromStart;
	}

	void GraphicsDevice_DX12::Impl::DescriptorHeap::offsetCurrentDescriptorHandle(uint32_t offset) {
		assert((currentDescriptorHandle.ptr + descriptorSize * offset - descriptorHandleFromStart.ptr) / descriptorSize <= capacity);

		currentDescriptorHandle.ptr += static_cast<SIZE_T>(descriptorSize * offset);
	}

	void GraphicsDevice_DX12::Impl::Descriptor::init(Impl* impl, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, ID3D12Resource* res) {
		handle = impl->resourceDescriptorHeap.getCurrentDescriptorHandle();
		index = impl->resourceDescriptorHeap.getDescriptorIndex(handle);

		impl->device->CreateShaderResourceView(
			res,
			&srvDesc,
			handle
		);

		impl->resourceDescriptorHeap.offsetCurrentDescriptorHandle();
	}

	void GraphicsDevice_DX12::Impl::Descriptor::init(Impl* impl, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, ID3D12Resource* res) {
		handle = impl->rtvDescriptorHeap.getCurrentDescriptorHandle();
		index = impl->rtvDescriptorHeap.getDescriptorIndex(handle);

		impl->device->CreateRenderTargetView(
			res,
			&rtvDesc,
			handle
		);

		impl->rtvDescriptorHeap.offsetCurrentDescriptorHandle();
	}

	void GraphicsDevice_DX12::Impl::Descriptor::init(Impl* impl, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, ID3D12Resource* res) {
		handle = impl->dsvDescriptorHeap.getCurrentDescriptorHandle();
		index = impl->dsvDescriptorHeap.getDescriptorIndex(handle);

		impl->device->CreateDepthStencilView(
			res,
			&dsvDesc,
			handle
		);

		impl->dsvDescriptorHeap.offsetCurrentDescriptorHandle();
	}

	void GraphicsDevice_DX12::Impl::Descriptor::init(Impl* impl, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, ID3D12Resource* res) {
		handle = impl->resourceDescriptorHeap.getCurrentDescriptorHandle();
		index = impl->resourceDescriptorHeap.getDescriptorIndex(handle);

		impl->device->CreateUnorderedAccessView(
			res,
			nullptr,
			&uavDesc,
			handle
		);

		impl->resourceDescriptorHeap.offsetCurrentDescriptorHandle();
	}

	ComPtr<IDxcCompiler3> compiler{};
	ComPtr<IDxcUtils> utils{};
	ComPtr<IDxcIncludeHandler> includeHandler{};

	/* ---------------------------------------------------------------------- */
	/*                     GraphicsDevice_DX12 Interface                      */
	/* ---------------------------------------------------------------------- */
	GraphicsDevice_DX12::GraphicsDevice_DX12(int width, int height, void* window) {
		m_Impl = new Impl();

		m_Impl->createDXGIFactory();
		m_Impl->createDevice();
		m_Impl->checkRayTracingSupport();
		m_Impl->createCommandAllocators();
		m_Impl->createCommandQueues();
		m_Impl->createDescriptorHeaps();

		m_Impl->copyAllocator.init(m_Impl);
	}

	GraphicsDevice_DX12::~GraphicsDevice_DX12() {
		delete m_Impl;
	}

	void GraphicsDevice_DX12::createBuffer(const BufferInfo& info, Buffer& buffer, const void* data) {
		auto internalState = std::make_shared<Impl::Buffer_DX12>();
		internalState->info = info;

		buffer.internalState = internalState;
		buffer.info = info;
		buffer.type = Resource::Type::BUFFER;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;

		D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

		switch (info.usage) {
		case Usage::UPLOAD:
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		}

		const D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0U,
			.Width = info.size,
			.Height = 1U,
			.DepthOrArraySize = 1U,
			.MipLevels = 1U,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = {.Count = 1U, .Quality = 0U },
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};

		if (info.usage == Usage::UPLOAD) {
			ThrowIfFailed(m_Impl->device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				resourceState,
				nullptr,
				IID_PPV_ARGS(&internalState->resource)
			));

			internalState->gpuAddress = internalState->resource->GetGPUVirtualAddress();

			// NOTE 1: Passing `nullptr` to Map/Unmap indicates that the entire subresource is being mapped
			// NOTE 2: We do not unmap since UPLOAD usage often implies persistent mapping, we only do so
			// if `persistentMap` is false.
			internalState->resource->Map(0, nullptr, &buffer.mappedData);

			if (data != nullptr) {
				std::memcpy(buffer.mappedData, data, info.size);
			}

			buffer.mappedSize = info.size;

			if (!info.persistentMap) {
				internalState->resource->Unmap(0, nullptr);
			}
		}
		else if (info.usage == Usage::DEFAULT) {
			// NOTE: The purpose of DEFAULT usage is that we provide the CPU no immediate access
			// to the GPU memory, as opposed to UPLOAD usage where we can have a persistently
			// mapped buffer and write to it (while handling synchronization of course).
			// But in the case of DEFAULT usage, we do not have this luxury, since DEFAULT
			// usage implies that the CPU will NEVER have immediate access. Thus, the solution
			// is to issue a 'copy' command via a Command List that then copies data into
			// a destination resource, which in this case is our buffer. DEFAULT usage
			// is to be preferred for resources whose data only need to be set once.
			// Such as vertex and index buffers for example.

			// Define the buffer resource to be the destination of a copy command
			ThrowIfFailed(m_Impl->device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&internalState->resource)
			));

			internalState->gpuAddress = internalState->resource->GetGPUVirtualAddress();

			// TODO: This might be entirely wrong, we might instead want to have a predefined upload heap for this
			// I guess we'll just create a temporary upload heap (aka staging buffer)
			BindFlag stagingBindFlags = info.bindFlags & ~BindFlag::SHADER_RESOURCE;

			const BufferInfo stagingBufferInfo = {
				.size = info.size,
				.stride = info.stride,
				.usage = Usage::UPLOAD,
				.bindFlags = stagingBindFlags,
				.miscFlags = info.miscFlags,
				.persistentMap = false
			};

			Buffer stagingBuffer = {};
			createBuffer(stagingBufferInfo, stagingBuffer, data);
			auto internalStagingBuffer = (Impl::Buffer_DX12*)stagingBuffer.internalState.get();

			Impl::CopyAllocator::CopyCMD copyCommand = m_Impl->copyAllocator.allocate(info.size);
			copyCommand.cmdList->CopyResource(internalState->resource.Get(), internalStagingBuffer->resource.Get());

			m_Impl->copyAllocator.submit(copyCommand);
		}

		// Create CBV if requested
		if (has_flag(info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
			const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
				.BufferLocation = internalState->gpuAddress,
				.SizeInBytes = static_cast<UINT>(info.size)
			};

			m_Impl->device->CreateConstantBufferView(
				&cbvDesc,
				m_Impl->resourceDescriptorHeap.getCurrentDescriptorHandle()
			);

			m_Impl->resourceDescriptorHeap.offsetCurrentDescriptorHandle(1);
		}

		// Create SRV for Structured Buffer
		if (has_flag(info.bindFlags, BindFlag::SHADER_RESOURCE) && has_flag(info.miscFlags, MiscFlag::BUFFER_STRUCTURED)) {
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = 0,
					.NumElements = static_cast<UINT>(info.size / info.stride),
					.StructureByteStride = info.stride,
					.Flags = D3D12_BUFFER_SRV_FLAG_NONE
				}
			};

			internalState->srvDescriptor.init(m_Impl, srvDesc, internalState->resource.Get());
		}

		// Create SRV for raw buffer (aka Byte Address Buffer)
		if (has_flag(info.bindFlags, BindFlag::SHADER_RESOURCE) && has_flag(info.miscFlags, MiscFlag::BUFFER_RAW)) {
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = DXGI_FORMAT_R32_TYPELESS,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = 0,
					.NumElements = static_cast<UINT>(info.size / info.stride),
					.Flags = D3D12_BUFFER_SRV_FLAG_RAW
				}
			};

			internalState->srvDescriptor.init(m_Impl, srvDesc, internalState->resource.Get());
		}
	}

	void GraphicsDevice_DX12::createPipeline(const PipelineInfo& info, Pipeline& pipeline) {
		auto internalState = std::make_shared<Pipeline_DX12>();
		internalState->info = info;

		pipeline.internalState = internalState;
		pipeline.info = info;

		// TODO: Allow for compute pipeline
		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineDesc = {};
		graphicsPipelineDesc.NodeMask = 0U;

		std::vector<D3D12_ROOT_PARAMETER1> rootParameters = {};
		// TODO: We should really redo this root parameter UID check, because right now registers and spaces
		// can clash even if they exist in different register types

		// Predefined root parameters
		const D3D12_ROOT_PARAMETER1 perFrameDescriptor = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor = {
				.ShaderRegister = 0U,
				.RegisterSpace = 0U,
				.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_ROOT_PARAMETER1 pushConstant = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0U,
				.RegisterSpace = 2U,
				.Num32BitValues = 32U, // 128 bytes
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_DESCRIPTOR_RANGE1 samplerRange = {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
			.NumDescriptors = MAX_SAMPLER_DESCRIPTORS,
			.BaseShaderRegister = 0U,
			.RegisterSpace = 0U,
			.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
			.OffsetInDescriptorsFromTableStart = 0U
		};

		const D3D12_ROOT_PARAMETER1 samplerTable = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1U,
				.pDescriptorRanges = &samplerRange
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
		};

		rootParameters.push_back(perFrameDescriptor);
		rootParameters.push_back(pushConstant);
		rootParameters.push_back(samplerTable);

		internalState->rootParameterIndexLUT.insert({ "g_PerFrameData", 0 });
		internalState->rootParameterIndexLUT.insert({ "pushConstant", 1 });
		internalState->rootParameterIndexLUT.insert({ "samplerRange", 2 });

		// Vertex shader
		if (info.vertexShader != nullptr) {
			auto internalShader = (Shader_DX12*)info.vertexShader->internalState.get();

			const D3D12_SHADER_BYTECODE shaderCode = {
				.pShaderBytecode = internalShader->shaderBlob->GetBufferPointer(),
				.BytecodeLength = internalShader->shaderBlob->GetBufferSize()
			};

			for (uint32_t i = 0; i < internalShader->rootParameters.size(); ++i) {
				const D3D12_ROOT_PARAMETER1 rootParam = internalShader->rootParameters[i];

				if (!internalState->rootParameterIndexLUT.contains(internalShader->rootParameterNameLUT[i])) {
					//rootParameterUIDs.insert(getRootParameterUID(rootParam));
					internalState->rootParameterIndexLUT.insert(std::pair(internalShader->rootParameterNameLUT[i], static_cast<uint32_t>(rootParameters.size())));
					rootParameters.push_back(rootParam);
				}
			}

			graphicsPipelineDesc.VS = shaderCode;
		}
		// Fragment shader (pixel shader)
		if (info.fragmentShader != nullptr) {
			auto internalShader = (Shader_DX12*)info.fragmentShader->internalState.get();

			const D3D12_SHADER_BYTECODE shaderCode = {
				.pShaderBytecode = internalShader->shaderBlob->GetBufferPointer(),
				.BytecodeLength = internalShader->shaderBlob->GetBufferSize()
			};

			for (uint32_t i = 0; i < internalShader->rootParameters.size(); ++i) {
				const D3D12_ROOT_PARAMETER1 rootParam = internalShader->rootParameters[i];

				if (!internalState->rootParameterIndexLUT.contains(internalShader->rootParameterNameLUT[i])) {
					//rootParameterUIDs.insert(getRootParameterUID(rootParam));
					internalState->rootParameterIndexLUT.insert(std::pair(internalShader->rootParameterNameLUT[i], static_cast<uint32_t>(rootParameters.size())));
					rootParameters.push_back(rootParam);
				}
			}

			graphicsPipelineDesc.PS = shaderCode;
		}

		// Root signature
		struct D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters = static_cast<UINT>(rootParameters.size()),
				.pParameters = rootParameters.data(),
				.NumStaticSamplers = 0U,
				.pStaticSamplers = nullptr,
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
					D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
			}
		};

		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> rootSignatureErrorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(
			&rootSignatureDesc,
			&rootSignatureBlob,
			&rootSignatureErrorBlob
		));

		ThrowIfFailed(m_Impl->device->CreateRootSignature(
			0U,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&internalState->rootSignature)
		));

		graphicsPipelineDesc.pRootSignature = internalState->rootSignature.Get();

		// Blend state
		if (info.blendState != nullptr) {
			D3D12_BLEND_DESC blendDesc = {
				.AlphaToCoverageEnable = info.blendState->alphaToCoverage,
				.IndependentBlendEnable = info.blendState->independentBlend,
			};

			for (size_t i = 0; i < 8; ++i) {
				D3D12_RENDER_TARGET_BLEND_DESC& d3d12BlendDescRT = blendDesc.RenderTarget[i];
				const BlendState::RenderTargetBlendState& blendDescRT = info.blendState->renderTargetBlendStates[i];

				d3d12BlendDescRT.BlendEnable = blendDescRT.blendEnable;
				d3d12BlendDescRT.SrcBlend = toDX12Blend(blendDescRT.srcBlend);
				d3d12BlendDescRT.DestBlend = toDX12Blend(blendDescRT.dstBlend);
				d3d12BlendDescRT.BlendOp = toDX12BlendOp(blendDescRT.blendOp);
				d3d12BlendDescRT.SrcBlendAlpha = toDX12AlphaBlend(blendDescRT.srcBlendAlpha);
				d3d12BlendDescRT.DestBlendAlpha = toDX12AlphaBlend(blendDescRT.dstBlendAlpha);
				d3d12BlendDescRT.BlendOpAlpha = toDX12BlendOp(blendDescRT.blendOpAlpha);
				d3d12BlendDescRT.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // TODO: Very unlikely to need anything else, but we should make it dynamic
			}

			graphicsPipelineDesc.BlendState = blendDesc;
		}
		else {
			graphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		}

		// Rasterizer state
		const D3D12_RASTERIZER_DESC rasterizerDesc = {
			.FillMode = toDX12FillMode(info.rasterizerState.fillMode),
			.CullMode = toDX12CullMode(info.rasterizerState.cullMode),
			.FrontCounterClockwise = info.rasterizerState.frontCW,
			.DepthBias = info.rasterizerState.depthBias,
			.DepthBiasClamp = info.rasterizerState.depthBiasClamp,
			.SlopeScaledDepthBias = info.rasterizerState.slopeScaledDepthBias,
			.DepthClipEnable = info.rasterizerState.depthClipEnable,
			.MultisampleEnable = info.rasterizerState.multisampleEnable,
			.AntialiasedLineEnable = info.rasterizerState.antialisedLineEnable,
			.ForcedSampleCount = 0U,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		};

		// Depth stencil state
		const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
			.DepthEnable = info.depthStencilState.depthEnable,
			.DepthWriteMask = info.depthStencilState.depthWriteMask == DepthWriteMask::ALL ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = toDX12ComparisonFunc(info.depthStencilState.depthFunction),
			.StencilEnable = info.depthStencilState.stencilEnable,
			.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
			.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
			.FrontFace = {
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
			.BackFace = {
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
		};

		// Input layout
		const size_t numElements = info.inputLayout.elements.size();

		auto& internalElements = internalState->inputElements;
		internalElements.resize(numElements);

		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
		inputLayoutDesc.NumElements = static_cast<UINT>(numElements);

		for (size_t i = 0; i < numElements; ++i) {
			const InputLayout::Element& inputElement = info.inputLayout.elements[i];

			internalElements[i].SemanticName = inputElement.name.c_str();
			internalElements[i].SemanticIndex = 0U; // TODO: FIX THIS!!!
			internalElements[i].Format = toDX12Format(inputElement.format);
			internalElements[i].InputSlot = 0U; // TODO: FIX THIS TOO!!!
			internalElements[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			internalElements[i].InputSlotClass = toDX12InputClass(inputElement.inputClassification);
			internalElements[i].InstanceDataStepRate = 0U; // TODO: Will break for per-instance data
		}

		inputLayoutDesc.pInputElementDescs = internalElements.data();
		graphicsPipelineDesc.InputLayout = inputLayoutDesc;

		// Render targets
		graphicsPipelineDesc.NumRenderTargets = info.numRenderTargets;

		for (size_t i = 0; i < info.numRenderTargets; ++i) {
			graphicsPipelineDesc.RTVFormats[i] = toDX12Format(info.renderTargetFormats[i]);
		}

		// Depth stencil format
		graphicsPipelineDesc.DSVFormat = toDX12Format(info.depthStencilFormat);
		graphicsPipelineDesc.RasterizerState = rasterizerDesc;
		graphicsPipelineDesc.DepthStencilState = depthStencilDesc;
		graphicsPipelineDesc.SampleMask = UINT_MAX;
		graphicsPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		graphicsPipelineDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_Impl->device->CreateGraphicsPipelineState(
			&graphicsPipelineDesc,
			IID_PPV_ARGS(&internalState->pipelineState)
		));
	}

	void GraphicsDevice_DX12::createSampler(const SamplerInfo& info, Sampler& sampler) {
		auto internalState = std::make_shared<Sampler_DX12>();
		internalState->info = info;

		sampler.internalState = internalState;
		sampler.info = info;

		D3D12_SAMPLER_DESC samplerDesc = {
			.Filter = toDX12Filter(info.filter),
			.AddressU = toDX12TextureAddressMode(info.addressU),
			.AddressV = toDX12TextureAddressMode(info.addressV),
			.AddressW = toDX12TextureAddressMode(info.addressW),
			.MipLODBias = info.mipLODBias,
			.MaxAnisotropy = info.maxAnisotropy,
			.ComparisonFunc = toDX12ComparisonFunc(info.comparisonFunc),
			.MinLOD = info.minLOD,
			.MaxLOD = info.maxLOD
		};

		switch (info.borderColor) {
		case BorderColor::OPAQUE_BLACK:
			{
				samplerDesc.BorderColor[0] = 0.0f;
				samplerDesc.BorderColor[1] = 0.0f;
				samplerDesc.BorderColor[2] = 0.0f;
				samplerDesc.BorderColor[3] = 1.0f;
			}
			break;
		case BorderColor::OPAQUE_WHITE:
			{
				samplerDesc.BorderColor[0] = 1.0f;
				samplerDesc.BorderColor[1] = 1.0f;
				samplerDesc.BorderColor[2] = 1.0f;
				samplerDesc.BorderColor[3] = 1.0f;
			}
			break;
		default:
			{
				samplerDesc.BorderColor[0] = 0.0f;
				samplerDesc.BorderColor[1] = 0.0f;
				samplerDesc.BorderColor[2] = 0.0f;
				samplerDesc.BorderColor[3] = 0.0f;
			}
			break;
		}

		m_Impl->device->CreateSampler(&samplerDesc, m_Impl->samplerDescriptorHeap.getCurrentDescriptorHandle());
		m_Impl->samplerDescriptorHeap.offsetCurrentDescriptorHandle();
	}

	void GraphicsDevice_DX12::createShader(ShaderStage stage, const std::string& path, Shader& shader) {
		auto internalState = std::make_shared<Shader_DX12>();
		internalState->stage = stage;

		shader.stage = stage;
		shader.internalState = internalState;

		const std::wstring wPath = sr::utilities::toWideString(path);
		WCHAR absolutePath[MAX_PATH] = { 0 };
		GetFullPathName(wPath.c_str(), MAX_PATH, absolutePath, nullptr);

		const std::wstring directory = std::wstring(path.begin(), path.begin() + path.find_last_of(L'/') + 1);
		WCHAR absoluteDirectory[MAX_PATH] = { 0 };
		GetFullPathName(directory.c_str(), MAX_PATH, absoluteDirectory, nullptr);

		if (!utils) {
			HMODULE dxc = LoadLibrary(L"dxcompiler.dll");

			DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc, "DxcCreateInstance");

			ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
			ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
			ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));
		}

		const std::wstring targetProfile = [=]() {
			switch (stage) {
			case ShaderStage::VERTEX:
				return L"vs_6_6";
			case ShaderStage::PIXEL:
				return L"ps_6_6";
			case ShaderStage::COMPUTE:
				return L"cs_6_6";
			case ShaderStage::LIBRARY:
				return L"lib_6_6";
			default:
				return L"";
			}
		}();

		// Compilation arguments
		std::vector<LPCWSTR> compilationArguments = {
			L"-HV",
			L"2021",
			L"-I",
			absoluteDirectory,
			L"-E",
			L"main",
			L"-T",
			targetProfile.c_str(),
			DXC_ARG_PACK_MATRIX_COLUMN_MAJOR,
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_ALL_RESOURCES_BOUND,
		};

		#ifdef _DEBUG
			compilationArguments.push_back(DXC_ARG_DEBUG);
		#else
			compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
		#endif

		// Load the shader source file to a blob
		ComPtr<IDxcBlobEncoding> sourceBlob = {};
		ThrowIfFailed(utils->LoadFile(absolutePath, nullptr, &sourceBlob));

		const DxcBuffer sourceBuffer = {
			.Ptr = sourceBlob->GetBufferPointer(),
			.Size = sourceBlob->GetBufferSize(),
			.Encoding = 0U,
		};

		// Compile the shader
		ComPtr<IDxcResult> compiledShaderBuffer = {};
		HRESULT hr = compiler->Compile(
			&sourceBuffer,
			compilationArguments.data(),
			static_cast<uint32_t>(compilationArguments.size()),
			includeHandler.Get(),
			IID_PPV_ARGS(&compiledShaderBuffer)
		);

		assert(SUCCEEDED(hr) && "Failed to compile shader!");

		if (FAILED(hr)) {
			OutputDebugString(std::format(L"Failed to compile shader at: {}", wPath).c_str());
		}

		// Get compilation errors (if any)
		ComPtr<IDxcBlobUtf8> errors = nullptr;
		ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));

		if (errors && errors->GetStringLength() > 0) {
			const LPCSTR errorMessage = errors->GetStringPointer();
			OutputDebugStringA(errorMessage);
		}

		// Get shader reflection data
		ComPtr<IDxcBlob> reflectionBlob = nullptr;
		ThrowIfFailed(compiledShaderBuffer->GetOutput(
			DXC_OUT_REFLECTION,
			IID_PPV_ARGS(&reflectionBlob),
			nullptr
		));

		const DxcBuffer reflectionBuffer = {
			.Ptr = reflectionBlob->GetBufferPointer(),
			.Size = reflectionBlob->GetBufferSize(),
			.Encoding = 0U
		};

		// DXR shader libraries require the ID3D12LibraryReflection
		if (has_flag(stage, ShaderStage::LIBRARY)) {
			ComPtr<ID3D12LibraryReflection> libraryReflection = nullptr;
			utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&libraryReflection));

			D3D12_LIBRARY_DESC libraryDesc = {};
			libraryReflection->GetDesc(&libraryDesc);

			for (int i = 0; i < libraryDesc.FunctionCount; ++i) {
				ID3D12FunctionReflection* function = libraryReflection->GetFunctionByIndex(i);
				D3D12_FUNCTION_DESC functionDesc = {};
				function->GetDesc(&functionDesc);

				for (uint32_t j = 0; j < functionDesc.BoundResources; ++j) {
					D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc = {};
					ThrowIfFailed(function->GetResourceBindingDesc(j, &shaderInputBindDesc));

					// Get CBV reflection data
					if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER) {
						ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = function->GetConstantBufferByIndex(j);
						D3D12_SHADER_BUFFER_DESC constantBufferDesc = {};
						shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

						const D3D12_ROOT_PARAMETER1 rootParameter = {
							.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
							.Descriptor = {
								.ShaderRegister = shaderInputBindDesc.BindPoint,
								.RegisterSpace = shaderInputBindDesc.Space,
								.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE
							},
							.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
						};

						internalState->rootParameterNameLUT[j] = shaderInputBindDesc.Name;
						internalState->rootParameters.push_back(rootParameter);
					}
				}
			}
		}
		else {
			ComPtr<ID3D12ShaderReflection> shaderReflection = nullptr;
			hr = utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));


			D3D12_SHADER_DESC shaderDesc = {};

			if (shaderReflection != nullptr) {
				shaderReflection->GetDesc(&shaderDesc);
			}

			// Shader reflection data
			for (uint32_t i = 0; i < shaderDesc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc = {};
				ThrowIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

				// Get CBV reflection data
				if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER) {
					ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
					D3D12_SHADER_BUFFER_DESC constantBufferDesc = {};
					shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

					const D3D12_ROOT_PARAMETER1 rootParameter = {
						.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
						.Descriptor = {
							.ShaderRegister = shaderInputBindDesc.BindPoint,
							.RegisterSpace = shaderInputBindDesc.Space,
							.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE
						},
						.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
					};

					internalState->rootParameterNameLUT[i] = shaderInputBindDesc.Name;
					internalState->rootParameters.push_back(rootParameter);
				}
				// Get structured buffer reflection data
				else if (shaderInputBindDesc.Type == D3D_SIT_STRUCTURED) {
					const D3D12_ROOT_PARAMETER1 rootParameter = {
						.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
						.Descriptor = {
							.ShaderRegister = shaderInputBindDesc.BindPoint,
							.RegisterSpace = shaderInputBindDesc.Space,
							.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE
						},
						.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
					};

					internalState->rootParameterNameLUT[i] = shaderInputBindDesc.Name;
					internalState->rootParameters.push_back(rootParameter);
				}
			}
		}

		ComPtr<IDxcBlob> compiledShaderBlob = {};
		compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

		internalState->shaderBlob = compiledShaderBlob;
	}

	void GraphicsDevice_DX12::createSwapChain(const SwapChainInfo& info, SwapChain& swapChain, void* window) {
		auto internalState = std::make_shared<SwapChain_DX12>();
		internalState->info = info;

		HWND internalWindow = (HWND)window;

		swapChain.info = info;
		swapChain.internalState = internalState;

		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
			.Width = info.width,
			.Height = info.height,
			.Format = toDX12Format(info.format),
			.Stereo = FALSE,
			.SampleDesc = { 1, 0 },
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = info.bufferCount,
			.Scaling = DXGI_SCALING_NONE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = m_Impl->allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
		};

		ComPtr<IDXGISwapChain1> dxgiSwapChain1 = nullptr;
		ThrowIfFailed(m_Impl->dxgiFactory->CreateSwapChainForHwnd(
			m_Impl->commandQueues[QueueType::DIRECT].commandQueue.Get(),
			internalWindow,
			&swapChainDesc,
			nullptr,
			nullptr,
			&dxgiSwapChain1
		));

		// Disable Alt + Enter fullscreen toggle feature
		ThrowIfFailed(m_Impl->dxgiFactory->MakeWindowAssociation(internalWindow, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(dxgiSwapChain1.As(&internalState->swapChain));

		m_BufferIndex = internalState->swapChain->GetCurrentBackBufferIndex();

		// Render target views
		internalState->backBuffers.resize(info.bufferCount);

		for (uint32_t i = 0; i < info.bufferCount; ++i) {
			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = m_Impl->rtvDescriptorHeap.getCurrentDescriptorHandle();

			ThrowIfFailed(internalState->swapChain->GetBuffer(
				i,
				IID_PPV_ARGS(&internalState->backBuffers[i])
			));

			m_Impl->device->CreateRenderTargetView(
				internalState->backBuffers[i].Get(),
				nullptr,
				rtvDescriptorHandle
			);

			m_Impl->rtvDescriptorHeap.offsetCurrentDescriptorHandle();
		}
	}

	void GraphicsDevice_DX12::createTexture(const TextureInfo& info, Texture& texture, const SubresourceData* data) {
		auto internalState = std::make_shared<Impl::Texture_DX12>();
		internalState->info = info;

		texture.internalState = internalState;
		texture.info = info;
		texture.type = Resource::Type::TEXTURE;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0U;
		heapProperties.VisibleNodeMask = 0U;

		D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

		switch (info.usage) {
		case Usage::UPLOAD:
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		case Usage::COPY: // aka Readback
			heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
			resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
		}

		if (has_flag(info.bindFlags, BindFlag::RENDER_TARGET)) {
			resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
			resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if (has_flag(info.bindFlags, BindFlag::DEPTH_STENCIL)) {
			resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}
		if (has_flag(info.bindFlags, BindFlag::UNORDERED_ACCESS)) {
			resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		const D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = 0U,
			.Width = static_cast<UINT64>(info.width),
			.Height = static_cast<UINT>(info.height),
			.DepthOrArraySize = static_cast<UINT16>(info.depth),
			.MipLevels = static_cast<UINT16>(info.mipLevels),
			.Format = toDX12Format(info.format),
			.SampleDesc = {.Count = info.sampleCount, .Quality = 0U },
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = resourceFlags
		};

		if (info.usage == Usage::DEFAULT) {
			// Fill out the staging buffer
			// TODO: This section needs a massive rewrite once we start looking at mipmapping,
			// most likely, all mipmaps will be pre-generated because fuck writing a compute
			// shader for generating them. So for now, we always assume that 1 subresource
			// (aka 1 mip) will ever be used.
			if (data != nullptr) {
				// Texture resource (DEFAULT usage)
				ThrowIfFailed(m_Impl->device->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&resourceDesc,
					D3D12_RESOURCE_STATE_COPY_DEST, // The texture resource will act as the destination of a copy command
					nullptr,
					IID_PPV_ARGS(&internalState->resource)
				));

				UINT64 textureMemorySize = 0;
				UINT numFootprints = info.arraySize * std::max(1U, info.mipLevels);
				std::vector<UINT> numRows(numFootprints);
				std::vector<UINT64> rowSizesInBytes(numFootprints);
				std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(numFootprints);

				m_Impl->device->GetCopyableFootprints(
					&resourceDesc,
					0,
					numFootprints,
					0,
					footprints.data(),
					numRows.data(),
					rowSizesInBytes.data(),
					&textureMemorySize
				);

				// Staging buffer
				const BufferInfo stagingBufferInfo = {
					.size = textureMemorySize,
					.stride = 0, // doesn't matter
					.usage = Usage::UPLOAD,
					.persistentMap = true
				};

				Buffer stagingBuffer = {};
				createBuffer(stagingBufferInfo, stagingBuffer, nullptr);
				auto internalStagingBuffer = (Impl::Buffer_DX12*)stagingBuffer.internalState.get();

				Impl::CopyAllocator::CopyCMD copyCommand = m_Impl->copyAllocator.allocate(0);

				for (size_t i = 0; i < footprints.size(); ++i) {
					const D3D12_SUBRESOURCE_DATA subResourceData = {
						.pData = data->data,
						.RowPitch = data->rowPitch,
						.SlicePitch = data->slicePitch
					};

					const D3D12_MEMCPY_DEST destData = {
						.pData = (void*)((UINT64)stagingBuffer.mappedData + footprints[i].Offset),
						.RowPitch = (SIZE_T)footprints[i].Footprint.RowPitch,
						.SlicePitch = (SIZE_T)footprints[i].Footprint.RowPitch * (SIZE_T)numRows[i]
					};

					MemcpySubresource(&destData, &subResourceData, rowSizesInBytes[i], numRows[i], footprints[i].Footprint.Depth);

					const D3D12_TEXTURE_COPY_LOCATION dst = {
						.pResource = internalState->resource.Get(),
						.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
						.SubresourceIndex = static_cast<UINT>(i)
					};

					const D3D12_TEXTURE_COPY_LOCATION src = {
						.pResource = internalStagingBuffer->resource.Get(),
						.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
						.PlacedFootprint = footprints[i], // TODO: If we ever want a ring buffer, we will have to also change the offset
					};

					copyCommand.cmdList->CopyTextureRegion(
						&dst,
						0U,
						0U,
						0U,
						&src,
						nullptr
					);
				}

				m_Impl->copyAllocator.submit(copyCommand);
			}
			else {
				if (has_flag(info.bindFlags, BindFlag::DEPTH_STENCIL)) {
					const D3D12_CLEAR_VALUE clearValue = {
						.Format = resourceDesc.Format,
						.DepthStencil = {
							.Depth = 1.0F,
							.Stencil = 0
						}
					};

					ThrowIfFailed(m_Impl->device->CreateCommittedResource(
						&heapProperties,
						D3D12_HEAP_FLAG_NONE,
						&resourceDesc,
						resourceState,
						&clearValue,
						IID_PPV_ARGS(&internalState->resource)
					));
				}
				else if (has_flag(info.bindFlags, BindFlag::RENDER_TARGET)) {
					const D3D12_CLEAR_VALUE clearValue = {
						.Format = resourceDesc.Format,
						.Color = { 0.0f, 0.0f, 0.0f, 1.0f }
					};

					ThrowIfFailed(m_Impl->device->CreateCommittedResource(
						&heapProperties,
						D3D12_HEAP_FLAG_NONE,
						&resourceDesc,
						resourceState,
						&clearValue,
						IID_PPV_ARGS(&internalState->resource)
					));
				}
				else {
					ThrowIfFailed(m_Impl->device->CreateCommittedResource(
						&heapProperties,
						D3D12_HEAP_FLAG_NONE,
						&resourceDesc,
						resourceState,
						nullptr,
						IID_PPV_ARGS(&internalState->resource)
					));
				}
			}
		}

		// Create shader resource view (SRV) if requested
		if (has_flag(info.bindFlags, BindFlag::SHADER_RESOURCE)) {
			internalState->subResourceType = SubresourceType::SRV;

			DXGI_FORMAT srvFormat = resourceDesc.Format;

			if (has_flag(info.bindFlags, BindFlag::DEPTH_STENCIL)) {
				if (info.format == Format::D32_FLOAT) {
					srvFormat = DXGI_FORMAT_R32_FLOAT;
				}
				else if (info.format == Format::D16_UNORM) {
					srvFormat = DXGI_FORMAT_R16_UNORM;
				}
			}

			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = srvFormat,
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D = {
					.MostDetailedMip = 0,
					.MipLevels = info.mipLevels,
				}
			};

			internalState->srvDescriptor.init(m_Impl, srvDesc, internalState->resource.Get());
		}

		// Create render target view (RTV) if requested
		if (has_flag(info.bindFlags, BindFlag::RENDER_TARGET)) {
			internalState->subResourceType = SubresourceType::RTV;

			const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
				.Format = resourceDesc.Format,
				.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
			};

			internalState->rtvDescriptor.init(m_Impl, rtvDesc, internalState->resource.Get());
		}

		// Create depth stencil view (DSV) if requested
		if (has_flag(info.bindFlags, BindFlag::DEPTH_STENCIL)) {
			internalState->subResourceType = SubresourceType::DSV;

			const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
				.Format = resourceDesc.Format,
				.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
				.Flags = D3D12_DSV_FLAG_NONE
			};

			internalState->dsvDescriptor.init(m_Impl, dsvDesc, internalState->resource.Get());
		}

		// Create unordered access view (UAV) if requested
		if (has_flag(info.bindFlags, BindFlag::UNORDERED_ACCESS)) {
			internalState->subResourceType = SubresourceType::UAV;

			const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
				.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
			};

			internalState->uavDescriptor.init(m_Impl, uavDesc, internalState->resource.Get());
		}
	}

	void GraphicsDevice_DX12::createShaderTable(const RTPipeline& rtPipeline, Buffer& table, const std::string& exportName) {
		auto internalRTPipeline = (RayTracingPipeline_DX12*)rtPipeline.internalState.get();
		auto internalTable = (Impl::Buffer_DX12*)table.internalState.get();

		const UINT shaderTableSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		ID3D12StateObjectProperties* stateObjectProperties = nullptr;
		ThrowIfFailed(internalRTPipeline->pso->QueryInterface(IID_PPV_ARGS(&stateObjectProperties)));

		const UINT alignment = 16;
		std::vector<uint8_t> alignedShaderTableData(shaderTableSize + alignment - 1);

		uint8_t* pAlignedShaderTableData = alignedShaderTableData.data() + ((UINT64)alignedShaderTableData.data() % alignment);
		std::wstring wExportName = sr::utilities::toWideString(exportName);
		void* pRayGenShaderData = stateObjectProperties->GetShaderIdentifier(wExportName.c_str()); // TODO: Make dynamic

		std::memcpy(pAlignedShaderTableData, pRayGenShaderData, shaderTableSize);

		const BufferInfo tableInfo = {
			.size = shaderTableSize,
			.stride = shaderTableSize,
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_RAW,
		};

		createBuffer(tableInfo, table, alignedShaderTableData.data());
	}

	void GraphicsDevice_DX12::createRTAS(const RayTracingASInfo& info, RayTracingAS& bvh) {
		auto internalState = std::make_shared<Impl::RayTracingAS_DX12>();

		bvh.info = info;
		bvh.internalState = internalState;
		bvh.type = Resource::Type::RAYTRACING_AS;

		if (info.type == RayTracingASType::TLAS) {
			auto internalInstanceBuffer = (Impl::Buffer_DX12*)info.tlas.instanceBuffer->internalState.get();

			internalState->desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
			internalState->desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			internalState->desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			internalState->desc.InstanceDescs = internalInstanceBuffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)info.tlas.offset;
			internalState->desc.NumDescs = info.tlas.numInstances;
			internalState->desc.pGeometryDescs = nullptr; // NOTE: unused in the case of TLAS

		}
		else if (info.type == RayTracingASType::BLAS) {
			internalState->desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			internalState->desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			internalState->desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

			// Set up DX12 BLAS geometries
			for (auto& geometry : info.blas.geometries) {
				// Mesh data properties
				auto internalVertexBuffer = (Impl::Buffer_DX12*)geometry.triangles.vertexBuffer->internalState.get();
				auto internalIndexBuffer = (Impl::Buffer_DX12*)geometry.triangles.indexBuffer->internalState.get();

				auto& dx12GeometryDesc = internalState->geometries.emplace_back(); // create new empty DX12 RT geometry desc
				dx12GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES; // TODO: Add checks for geometry type, for now we assume triangles
				dx12GeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				dx12GeometryDesc.Triangles.VertexFormat = toDX12Format(geometry.triangles.vertexFormat);
				dx12GeometryDesc.Triangles.VertexCount = geometry.triangles.vertexCount;
				dx12GeometryDesc.Triangles.VertexBuffer.StartAddress = internalVertexBuffer->gpuAddress + geometry.triangles.vertexByteOffset;
				dx12GeometryDesc.Triangles.VertexBuffer.StrideInBytes = geometry.triangles.vertexStride;
				dx12GeometryDesc.Triangles.IndexBuffer = internalIndexBuffer->gpuAddress + geometry.triangles.indexOffset * sizeof(uint32_t); // TODO: u32 assumed
				dx12GeometryDesc.Triangles.IndexCount = geometry.triangles.indexCount;
				dx12GeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
				dx12GeometryDesc.Triangles.Transform3x4 = 0;
			}

			internalState->desc.pGeometryDescs = internalState->geometries.data();
			internalState->desc.NumDescs = static_cast<UINT>(internalState->geometries.size());
		}

		m_Impl->device->GetRaytracingAccelerationStructurePrebuildInfo(&internalState->desc, &internalState->prebuildInfo);

		// Create acceleration structure resource
		const D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = internalState->prebuildInfo.ResultDataMaxSizeInBytes,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = {.Count = 1, .Quality = 0 },
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		};

		const D3D12_HEAP_PROPERTIES heapProperties = {
			.Type = D3D12_HEAP_TYPE_DEFAULT,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 0U,
			.VisibleNodeMask = 0U
		};

		ThrowIfFailed(m_Impl->device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&internalState->resource)
		));

		internalState->gpuAddress = internalState->resource->GetGPUVirtualAddress();

		// SRV
		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			//.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.RaytracingAccelerationStructure = { .Location = internalState->gpuAddress }
		};

		m_Impl->device->CreateShaderResourceView(
			nullptr, // This should be fine I think
			&srvDesc,
			m_Impl->resourceDescriptorHeap.getCurrentDescriptorHandle()
		);

		m_Impl->resourceDescriptorHeap.offsetCurrentDescriptorHandle(1);

		// Create scratch buffer
		const BufferInfo scratchBufferInfo = {
			.size = internalState->prebuildInfo.ScratchDataSizeInBytes
		};

		createBuffer(scratchBufferInfo, internalState->scratchBuffer, nullptr);
	}

	void GraphicsDevice_DX12::buildRTAS(const RayTracingAS& dst, const RayTracingAS* src, const CommandList& cmdList) {
		auto internalDstAS = (Impl::RayTracingAS_DX12*)dst.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs = internalDstAS->desc;

		if (dst.info.type == RayTracingASType::TLAS) {
			auto internalDstInstanceBuffer = (Impl::Buffer_DX12*)dst.info.tlas.instanceBuffer->internalState.get();
			buildDesc.Inputs.InstanceDescs = internalDstInstanceBuffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)dst.info.tlas.offset;
		}
		else if (dst.info.type == RayTracingASType::BLAS) {

		}

		if (src != nullptr) {
			auto internalSrcAS = (Impl::RayTracingAS_DX12*)src->internalState.get();

			buildDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			buildDesc.SourceAccelerationStructureData = internalSrcAS->gpuAddress;
		}

		auto internalDstScratchBuffer = (Impl::Buffer_DX12*)internalDstAS->scratchBuffer.internalState.get();
		buildDesc.DestAccelerationStructureData = internalDstAS->gpuAddress;
		buildDesc.ScratchAccelerationStructureData = internalDstScratchBuffer->gpuAddress;

		internalCommandList->getGraphicsCommandList()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	}

	void GraphicsDevice_DX12::writeTLASInstance(const RayTracingTLAS::Instance& instance, void* dest) {
		auto internalBLASResource = (Impl::RayTracingAS_DX12*)instance.blasResource->internalState.get();

		D3D12_RAYTRACING_INSTANCE_DESC dx12Instance = {};
		dx12Instance.AccelerationStructure = internalBLASResource->gpuAddress;
		dx12Instance.InstanceID = instance.instanceID;
		dx12Instance.InstanceMask = instance.instanceMask;
		dx12Instance.InstanceContributionToHitGroupIndex = instance.instanceContributionHitGroupIndex;
		dx12Instance.Flags = instance.flags;
		std::memcpy(dx12Instance.Transform, &instance.transform, sizeof(dx12Instance.Transform));

		// Memcpy the DX12 instance to the instance buffer mapped pointer (I guess?)
		std::memcpy(dest, &dx12Instance, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
	}

	void GraphicsDevice_DX12::createRTPipeline(const RTPipelineInfo& info, RTPipeline& rtPipeline) {
		auto internalState = std::make_shared<RayTracingPipeline_DX12>();

		rtPipeline.info = info;
		rtPipeline.internalState = internalState;

		std::vector<D3D12_ROOT_PARAMETER1> rootParameters = {};
		CD3DX12_STATE_OBJECT_DESC stateObjectDesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		// Shader libraries
		auto internalShader = (Shader_DX12*)info.shaderLibraries.front().shader->internalState.get();

		auto shaderLibrary = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE shaderDxil = CD3DX12_SHADER_BYTECODE((void*)internalShader->shaderBlob->GetBufferPointer(), internalShader->shaderBlob->GetBufferSize());
		shaderLibrary->SetDXILLibrary(&shaderDxil);

		// TODO: The naming here is very confusing, it's not shader libraries, there will only be one.
		// Instead, just rename this shit to "shaderFunctions" or something and export each one.
		for (size_t i = 0; i < info.shaderLibraries.size(); ++i) {
			shaderLibrary->DefineExport(sr::utilities::toWideString(info.shaderLibraries[i].functionName).c_str());
		}

		// Standard root parameters (will be refined later on)
		const D3D12_ROOT_PARAMETER1 accelerationStructureRootParam = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
			.Descriptor = {
				.ShaderRegister = 0,
				.RegisterSpace = 0,
				.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_DESCRIPTOR_RANGE1 uavRange = {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.NumDescriptors = 1, // TODO: We will revise this later
			.BaseShaderRegister = 0,
			.RegisterSpace = 0,
			.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
			.OffsetInDescriptorsFromTableStart = 0U
		};

		const D3D12_ROOT_PARAMETER1 uavTable = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1U,
				.pDescriptorRanges = &uavRange
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_ROOT_PARAMETER1 geometryInfoParam = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
			.Descriptor = {
				.ShaderRegister = 1,
				.RegisterSpace = 0,
				.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_ROOT_PARAMETER1 materialInfoParam = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
			.Descriptor = {
				.ShaderRegister = 2,
				.RegisterSpace = 0,
				.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		const D3D12_ROOT_PARAMETER1 pushConstant = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0U,
				.RegisterSpace = 2U,
				.Num32BitValues = 32U, // 128 bytes
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		rootParameters.push_back(accelerationStructureRootParam);
		rootParameters.push_back(uavTable);
		rootParameters.push_back(geometryInfoParam);
		rootParameters.push_back(materialInfoParam);
		rootParameters.push_back(pushConstant);

		internalState->rootParameterIndexLUT.insert({ "Scene", 0 });
		internalState->rootParameterIndexLUT.insert({ "RenderTarget", 1 });
		internalState->rootParameterIndexLUT.insert({ "g_GeometryInfo", 2 });
		internalState->rootParameterIndexLUT.insert({ "g_MaterialInfo", 3 });
		internalState->rootParameterIndexLUT.insert({ "pushConstant", 4 });

		// Retrieve root parameters
		for (uint32_t i = 0; i < internalShader->rootParameters.size(); ++i) {
			const D3D12_ROOT_PARAMETER1 rootParam = internalShader->rootParameters[i];

			if (!internalState->rootParameterIndexLUT.contains(internalShader->rootParameterNameLUT[i])) {
				internalState->rootParameterIndexLUT.insert(std::pair(internalShader->rootParameterNameLUT[i], static_cast<uint32_t>(rootParameters.size())));
				rootParameters.push_back(rootParam);
			}
		}

		// Hit groups
		for (size_t i = 0; i < info.hitGroups.size(); ++i) {
			auto hitGroup = stateObjectDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			const std::wstring wName = sr::utilities::toWideString(info.hitGroups[i].name);

			hitGroup->SetClosestHitShaderImport(L"MyClosestHitShader"); // TODO: Make dynamic
			hitGroup->SetHitGroupExport(wName.c_str());
			hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES); // TODO: Make dynamic
		}

		// TODO: We likely want to make this dynamic
		const UINT payloadSize = info.payloadSize;
		const UINT attributeSize = 2 * sizeof(float); // float2 barycentrics (NOTE: Built-in attribute from BuiltInTriangleIntersectionAttributes in the spec)

		auto shaderConfig = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		shaderConfig->Config(payloadSize, attributeSize);

		// TODO: Perhaps move this to local root signature?
		// Global root signature
		struct D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
				.Desc_1_1 = {
					.NumParameters = static_cast<UINT>(rootParameters.size()),
					.pParameters = rootParameters.data(),
					.NumStaticSamplers = 0U,
					.pStaticSamplers = nullptr,
					.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
						D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
			}
		};

		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> rootSignatureErrorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(
			&rootSignatureDesc,
			&rootSignatureBlob,
			&rootSignatureErrorBlob
		));

		ThrowIfFailed(m_Impl->device->CreateRootSignature(
			0U,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&internalState->rootSignature)
		));

		auto globalRootSignature = stateObjectDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		globalRootSignature->SetRootSignature(internalState->rootSignature.Get());

		auto pipelineConfig = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		const UINT maxRecursionDepth = 3; // TODO: Move to info-struct instead
		pipelineConfig->Config(maxRecursionDepth);

		// Build the pipeline state object
		ThrowIfFailed(m_Impl->device->CreateStateObject(
			stateObjectDesc,
			IID_PPV_ARGS(&internalState->pso)
		));
	}

	void GraphicsDevice_DX12::bindRTPipeline(const RTPipeline& rtPipeline, const Texture& rtOutputUAV, const CommandList& cmdList) {
		auto internalRTPipeline = (RayTracingPipeline_DX12*)rtPipeline.internalState.get();
		auto internalRTOutputUAV = (Impl::Texture_DX12*)rtOutputUAV.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		internalCommandList->getGraphicsCommandList()->SetPipelineState1(internalRTPipeline->pso.Get());

		internalCommandList->getGraphicsCommandList()->SetComputeRootSignature(internalRTPipeline->rootSignature.Get());

		D3D12_GPU_DESCRIPTOR_HANDLE rtOutputUAVHandle = m_Impl->resourceDescriptorHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		rtOutputUAVHandle.ptr += getDescriptorIndex(rtOutputUAV) * m_Impl->resourceDescriptorHeap.descriptorSize;

		internalCommandList->getGraphicsCommandList()->SetComputeRootDescriptorTable(1, rtOutputUAVHandle);
		//internalCommandList->getGraphicsCommandList()->SetComputeRootDescriptorTable(1, m_ResourceDescriptorHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	void GraphicsDevice_DX12::bindRTResource(const Resource& res, const std::string& name, const RTPipeline& rtPipeline, const CommandList& cmdList) {
		auto internalResource = (Resource_DX12*)res.internalState.get();
		auto internalPipeline = (RayTracingPipeline_DX12*)rtPipeline.internalState.get();
		auto internalCmdList = (CommandList_DX12*)cmdList.internalState;

		const auto& nameSearch = internalPipeline->rootParameterIndexLUT.find(name);
		if (nameSearch == internalPipeline->rootParameterIndexLUT.end()) {
			OutputDebugStringA(std::format("BIND ERROR: Failed to find root parameter with name \"{}\"", name).c_str());
			return;
		}

		// TODO: Add more cases
		switch (res.type) {
		case Resource::Type::BUFFER:
			{
				auto internalBuffer = (Impl::Buffer_DX12*)internalResource;
				auto graphicsCmdList = internalCmdList->getGraphicsCommandList();

				if (has_flag(internalBuffer->info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
					graphicsCmdList->SetComputeRootConstantBufferView(
						nameSearch->second,
						internalBuffer->gpuAddress
					);
				}
				else if (has_flag(internalBuffer->info.miscFlags, MiscFlag::BUFFER_STRUCTURED)) {
					graphicsCmdList->SetComputeRootShaderResourceView(
						nameSearch->second,
						internalBuffer->gpuAddress
					);
				}
			}
			break;
		case Resource::Type::RAYTRACING_AS:
			{
				auto internalAS = (Impl::RayTracingAS_DX12*)internalResource;
				auto graphicsCmdList = internalCmdList->getGraphicsCommandList();

				graphicsCmdList->SetComputeRootShaderResourceView(
					nameSearch->second,
					internalAS->gpuAddress
				);
			}
			break;
		}
	}

	void GraphicsDevice_DX12::createRTInstanceBuffer(Buffer& buffer, uint32_t numBottomLevels) {
		const BufferInfo instanceBufferInfo = {
			.size = numBottomLevels * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
			.stride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		createBuffer(instanceBufferInfo, buffer, nullptr);
	}

	void GraphicsDevice_DX12::dispatchRays(const DispatchRaysInfo& info, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
		dispatchRaysDesc.Width = info.width;
		dispatchRaysDesc.Height = info.height;
		dispatchRaysDesc.Depth = info.depth;

		// TODO: Perhaps add offsets? Because that seems to be required sometimes I guess
		if (info.rayGenTable != nullptr) {
			auto internalRayGenTable = (Impl::Buffer_DX12*)info.rayGenTable->internalState.get();

			dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = internalRayGenTable->gpuAddress;
			dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = internalRayGenTable->info.size;
		}

		if (info.missTable != nullptr) {
			auto internalMissTable = (Impl::Buffer_DX12*)info.missTable->internalState.get();

			dispatchRaysDesc.MissShaderTable.StartAddress = internalMissTable->gpuAddress;
			dispatchRaysDesc.MissShaderTable.SizeInBytes = internalMissTable->info.size;
			dispatchRaysDesc.MissShaderTable.StrideInBytes = internalMissTable->info.stride;
		}

		if (info.hitGroupTable != nullptr) {
			auto internalHitGroupTable = (Impl::Buffer_DX12*)info.hitGroupTable->internalState.get();

			dispatchRaysDesc.HitGroupTable.StartAddress = internalHitGroupTable->gpuAddress;
			dispatchRaysDesc.HitGroupTable.SizeInBytes = internalHitGroupTable->info.size;
			dispatchRaysDesc.HitGroupTable.StrideInBytes = internalHitGroupTable->info.stride;
		}

		internalCommandList->getGraphicsCommandList()->DispatchRays(&dispatchRaysDesc);
	}

	void GraphicsDevice_DX12::bindPipeline(const Pipeline& pipeline, const CommandList& cmdList) {
		auto internalPipeline = (Pipeline_DX12*)pipeline.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		if (cmdList.type == QueueType::DIRECT) {
			ID3D12GraphicsCommandList* graphicsCommandList = internalCommandList->getGraphicsCommandList();

			graphicsCommandList->SetPipelineState(internalPipeline->pipelineState.Get());
			graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			graphicsCommandList->SetGraphicsRootSignature(internalPipeline->rootSignature.Get());

			auto textureHandleStart = m_Impl->resourceDescriptorHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

			graphicsCommandList->SetGraphicsRootDescriptorTable(
				2U,
				m_Impl->samplerDescriptorHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart()
			);
		}
	}

	void GraphicsDevice_DX12::bindViewport(const Viewport& viewport, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		if (cmdList.type == QueueType::DIRECT) {
			internalCommandList->getGraphicsCommandList()->RSSetViewports(
				1U,
				reinterpret_cast<const D3D12_VIEWPORT*>(&viewport)
			);
		}
	}

	void GraphicsDevice_DX12::bindVertexBuffer(const Buffer& vertexBuffer, const CommandList& cmdList) {
		if (cmdList.type != QueueType::DIRECT) {
			return;
		}

		auto internalBuffer = (Impl::Buffer_DX12*)vertexBuffer.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {
			.BufferLocation = internalBuffer->gpuAddress,
			.SizeInBytes = static_cast<UINT>(vertexBuffer.info.size),
			.StrideInBytes = vertexBuffer.info.stride
		};

		internalCommandList->getGraphicsCommandList()->IASetVertexBuffers(
			0U,
			1U,
			&vertexBufferView
		);
	}

	void GraphicsDevice_DX12::bindIndexBuffer(const Buffer& indexBuffer, const CommandList& cmdList) {
		if (cmdList.type != QueueType::DIRECT) {
			return;
		}

		auto internalBuffer = (Impl::Buffer_DX12*)indexBuffer.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		const D3D12_INDEX_BUFFER_VIEW indexBufferView = {
			.BufferLocation = internalBuffer->gpuAddress,
			.SizeInBytes = static_cast<UINT>(indexBuffer.info.size),
			.Format = DXGI_FORMAT_R32_UINT
		};

		internalCommandList->getGraphicsCommandList()->IASetIndexBuffer(&indexBufferView);
	}

	void GraphicsDevice_DX12::bindSampler(const Sampler& sampler) {
	}

	void GraphicsDevice_DX12::bindResource(const Resource& res, const std::string& name, const Pipeline& pipeline, const CommandList& cmdList) {
		auto internalResource = (Resource_DX12*)res.internalState.get();
		auto internalPipeline = (Pipeline_DX12*)pipeline.internalState.get();
		auto internalCmdList = (CommandList_DX12*)cmdList.internalState;

		const auto& nameSearch = internalPipeline->rootParameterIndexLUT.find(name);
		if (nameSearch == internalPipeline->rootParameterIndexLUT.end()) {
			OutputDebugStringA(std::format("BIND ERROR: Failed to find root parameter with name \"{}\"", name).c_str());
			return;
		}

		// TODO: Add more cases
		switch (res.type) {
		case Resource::Type::BUFFER:
			{
				auto internalBuffer = (Impl::Buffer_DX12*)internalResource;
				auto graphicsCommandList = internalCmdList->getGraphicsCommandList();

				if (has_flag(internalBuffer->info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
					graphicsCommandList->SetGraphicsRootConstantBufferView(
						nameSearch->second,
						internalBuffer->gpuAddress
					);
				}
				else if (has_flag(internalBuffer->info.miscFlags, MiscFlag::BUFFER_STRUCTURED)) {
					graphicsCommandList->SetGraphicsRootShaderResourceView(
						nameSearch->second,
						internalBuffer->gpuAddress
					);
				}
			}
			break;
		}
	}

	void GraphicsDevice_DX12::copyResource(const Resource* dst, const Resource* src, const CommandList& cmdList) {
		auto internalDstResource = (Resource_DX12*)dst->internalState.get();
		auto internalSrcResource = (Resource_DX12*)src->internalState.get();
		auto internalCmdList = (CommandList_DX12*)cmdList.internalState;

		// TODO: Allow for other resource copy operations
		internalCmdList->getGraphicsCommandList()->CopyResource(
			internalDstResource->resource.Get(),
			internalSrcResource->resource.Get()
		);
	}

	void GraphicsDevice_DX12::pushConstants(const void* data, uint32_t size, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		D3D12_ROOT_PARAMETER rootParameter = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0U,
				.RegisterSpace = 0U,
				.Num32BitValues = size >> 2
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		internalCommandList->getGraphicsCommandList()->SetGraphicsRoot32BitConstants(
			1U,
			rootParameter.Constants.Num32BitValues,
			data,
			0U
		);
	}

	void GraphicsDevice_DX12::pushConstantsCompute(const void* data, uint32_t size, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		D3D12_ROOT_PARAMETER rootParameter = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0U,
				.RegisterSpace = 0U,
				.Num32BitValues = size >> 2
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};

		internalCommandList->getGraphicsCommandList()->SetComputeRoot32BitConstants(
			4U,
			rootParameter.Constants.Num32BitValues,
			data,
			0U
		);
	}

	void GraphicsDevice_DX12::barrier(const GPUBarrier& barrier, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		D3D12_RESOURCE_BARRIER dx12Barrier = {};

		switch (barrier.type) {
		case GPUBarrier::Type::UAV:
			{
				auto internalUAVResource = (Resource_DX12*)barrier.uav.resource->internalState.get();

				dx12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				dx12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				dx12Barrier.UAV = { .pResource = internalUAVResource->resource.Get() };
			}
			break;
		case GPUBarrier::Type::IMAGE:
			{
				auto internalImage = (Impl::Texture_DX12*)barrier.image.texture->internalState.get();

				dx12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				dx12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				dx12Barrier.Transition = {
					.pResource = internalImage->resource.Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = toDX12ResourceState(barrier.image.stateBefore),
					.StateAfter = toDX12ResourceState(barrier.image.stateAfter)
				};
			}
			break;
		}

		internalCommandList->getGraphicsCommandList()->ResourceBarrier(1, &dx12Barrier);
	}

	CommandList GraphicsDevice_DX12::beginCommandList(QueueType type) {
		const uint32_t currentCommand = m_Impl->commandCounter++;

		if (currentCommand >= m_Impl->commandLists.size()) {
			m_Impl->commandLists.push_back(std::make_unique<CommandList_DX12>());
		}

		CommandList_DX12* internalCommandList = m_Impl->commandLists[currentCommand].get();
		internalCommandList->type = type;

		CommandList cmdList = {};
		cmdList.internalState = internalCommandList;
		cmdList.type = type;

		if (internalCommandList->cmdList == nullptr) { // create d3d12 command list if not present
			ThrowIfFailed(m_Impl->device->CreateCommandList(
				0,
				toDX12CommandListType(type),
				m_Impl->commandAllocators[type][m_BufferIndex].Get(), // TODO: This is likely wrong
				nullptr,
				IID_PPV_ARGS(&internalCommandList->cmdList)
			));

			// Graphics command lists are created in the recording state, but there is nothing to record yet so we close it
			internalCommandList->getGraphicsCommandList()->Close();
		}

		// Reset command list and allocator back to intial state (AKA begin recording!)
		ID3D12CommandAllocator* commandAllocator = m_Impl->commandAllocators[type][m_BufferIndex].Get();
		commandAllocator->Reset();

		if (type == QueueType::DIRECT) {
			internalCommandList->getGraphicsCommandList()->Reset(commandAllocator, nullptr);

			// Set descriptor heaps
			ID3D12DescriptorHeap* descriptorHeaps[] = {
				m_Impl->resourceDescriptorHeap.descriptorHeap.Get(),
				m_Impl->samplerDescriptorHeap.descriptorHeap.Get()
			};

			internalCommandList->getGraphicsCommandList()->SetDescriptorHeaps(2, descriptorHeaps);

			// Set scissor rect
			D3D12_RECT pRects[D3D12_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1] = {};
			for (uint32_t i = 0; i < D3D12_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1; ++i) {
				pRects[i].left = 0;
				pRects[i].right = 16384;
				pRects[i].top = 0;
				pRects[i].bottom = 16384;
			}

			internalCommandList->getGraphicsCommandList()->RSSetScissorRects(D3D12_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1, pRects);
		}

		return cmdList;
	}

	void GraphicsDevice_DX12::beginRenderPass(const SwapChain& swapChain, const PassInfo& renderPass, const CommandList& cmdList, bool clearTargets) {
		if (cmdList.type != QueueType::DIRECT) {
			return;
		}

		auto internalSwapChain = (SwapChain_DX12*)swapChain.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		// Resource barrier PRESENT -> RT
		const D3D12_RESOURCE_BARRIER rtvBarrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition = {
				.pResource = internalSwapChain->backBuffers[m_BufferIndex].Get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_PRESENT, // aka D3D12_RESOURCE_STATE_COMMON
				.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
			}
		};

		internalCommandList->getGraphicsCommandList()->ResourceBarrier(1, &rtvBarrier);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_Impl->rtvDescriptorHeap.getDescriptorHandle(m_BufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

		if (renderPass.depth != nullptr) { // has depth attachment
			auto internalDepthTexture = (Impl::Texture_DX12*)renderPass.depth->internalState.get();
			dsvHandle = internalDepthTexture->dsvDescriptor.handle;
		}

		// TODO: It might be better to use BeginRenderPass and EndRenderPass functions
		// available through the graphics command lists instead of using
		// OMSetRenderTargets and ClearRenderTargetView. This at least seems to be the
		// case on Adreno GPUs. On the other hand, it seems like Xbox platforms
		// prefer the standard method.
		internalCommandList->getGraphicsCommandList()->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			renderPass.depth != nullptr ? &dsvHandle : nullptr
		);

		if (clearTargets) {
			const FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			internalCommandList->getGraphicsCommandList()->ClearRenderTargetView(
				rtvHandle,
				clearColor,
				0,
				nullptr
			);

			if (renderPass.depth != nullptr) {
				internalCommandList->getGraphicsCommandList()->ClearDepthStencilView(
					dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH,
					1.0F,
					0,
					0,
					nullptr
				);
			}
		}
	}

	void GraphicsDevice_DX12::beginRenderPass(const PassInfo& renderPass, const CommandList& cmdList, bool clearTargets) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles(renderPass.numColorAttachments);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

		for (size_t i = 0; i < renderPass.numColorAttachments; ++i) {
			auto internalColorAttachment = (Impl::Texture_DX12*)renderPass.colors[i]->internalState.get();
			rtvHandles[i] = internalColorAttachment->rtvDescriptor.handle;
		}

		if (renderPass.depth != nullptr) {
			auto internalDepthAttachment = (Impl::Texture_DX12*)renderPass.depth->internalState.get();
			dsvHandle = internalDepthAttachment->dsvDescriptor.handle;
		}

		if (renderPass.numColorAttachments == 0) {
			internalCommandList->getGraphicsCommandList()->OMSetRenderTargets(
				renderPass.numColorAttachments,
				nullptr,
				FALSE,
				renderPass.depth != nullptr ? &dsvHandle : nullptr
			);
		}
		else {

			internalCommandList->getGraphicsCommandList()->OMSetRenderTargets(
				renderPass.numColorAttachments,
				rtvHandles.data(),
				FALSE,
				renderPass.depth != nullptr ? &dsvHandle : nullptr
			);
		}

		if (clearTargets) {
			const FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

			for (size_t i = 0; i < renderPass.numColorAttachments; ++i) {
				internalCommandList->getGraphicsCommandList()->ClearRenderTargetView(
					rtvHandles[i],
					clearColor,
					0,
					nullptr
				);
			}

			if (renderPass.depth != nullptr) {
				internalCommandList->getGraphicsCommandList()->ClearDepthStencilView(
					dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH,
					1.0f,
					0,
					0,
					nullptr
				);
			}
		}
	}

	void GraphicsDevice_DX12::endRenderPass(const SwapChain& swapChain, const CommandList& cmdList) {
		if (cmdList.type != QueueType::DIRECT) {
			return;
		}

		auto internalSwapChain = (SwapChain_DX12*)swapChain.internalState.get();
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		// Resource barrier RT -> PRESENT
		const D3D12_RESOURCE_BARRIER rtvBarrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition = {
				.pResource = internalSwapChain->backBuffers[m_BufferIndex].Get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.StateAfter = D3D12_RESOURCE_STATE_PRESENT, // aka D3D12_RESOURCE_STATE_COMMON
			}
		};

		internalCommandList->getGraphicsCommandList()->ResourceBarrier(1, &rtvBarrier);
	}

	void GraphicsDevice_DX12::endRenderPass() {
	}

	void GraphicsDevice_DX12::submitCommandLists(SwapChain& swapChain) {
		auto internalSwapChain = (SwapChain_DX12*)swapChain.internalState.get();

		const size_t tempCommandCount = m_Impl->commandCounter;
		m_Impl->commandCounter = 0; // reset command counter

		for (size_t i = 0; i < tempCommandCount; ++i) {
			CommandList_DX12* cmdList = m_Impl->commandLists[i].get();

			if (cmdList->type == QueueType::DIRECT) {
				cmdList->getGraphicsCommandList()->Close();
			}

			CommandQueue_DX12& commandQueue = m_Impl->commandQueues[cmdList->type];
			commandQueue.submittedCommandLists.push_back(cmdList->cmdList.Get());
		}

		for (size_t q = 0; q < QUEUE_COUNT; ++q) {
			CommandQueue_DX12& commandQueue = m_Impl->commandQueues[q];

			if (!commandQueue.submittedCommandLists.empty()) {
				commandQueue.commandQueue->ExecuteCommandLists(
					static_cast<UINT>(commandQueue.submittedCommandLists.size()),
					commandQueue.submittedCommandLists.data()
				);

				commandQueue.submittedCommandLists.clear();
			}

			ThrowIfFailed(commandQueue.commandQueue->Signal(
				m_Impl->frameFences[m_BufferIndex][q].Get(),
				1
			));
		}

		UINT presentFlags = 0;
		const UINT syncInterval = swapChain.info.vsync ? 1 : 0;

		if (!swapChain.info.vsync && !swapChain.info.fullscreen && m_Impl->allowTearing) {
			presentFlags = DXGI_PRESENT_ALLOW_TEARING;
		}

		internalSwapChain->swapChain->Present(syncInterval, presentFlags);

		++m_FrameCount;

		for (size_t q = 0; q < QUEUE_COUNT; ++q) {
			if (m_FrameCount > NUM_BUFFERS && m_Impl->frameFences[m_BufferIndex][q]->GetCompletedValue() < 1) {
				m_Impl->frameFences[m_BufferIndex][q]->SetEventOnCompletion(1, nullptr);
			}

			m_Impl->frameFences[m_BufferIndex][q]->Signal(0);
		}

		m_BufferIndex = (m_BufferIndex + 1) % static_cast<uint32_t>(NUM_BUFFERS);
	}

	void GraphicsDevice_DX12::draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) {
		drawInstanced(vertexCount, 1, startVertex, 0, cmdList);
	}

	void GraphicsDevice_DX12::drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		if (cmdList.type == QueueType::DIRECT) {
			internalCommandList->getGraphicsCommandList()->DrawInstanced(
				vertexCount,
				instanceCount,
				startVertex,
				startInstance
			);
		}
	}

	void GraphicsDevice_DX12::drawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) {
		auto internalCommandList = (CommandList_DX12*)cmdList.internalState;

		if (cmdList.type == QueueType::DIRECT) {
			internalCommandList->getGraphicsCommandList()->DrawIndexedInstanced(
				indexCount,
				1U,
				startIndex,
				baseVertex,
				0U
			);
		}
	}

	uint32_t GraphicsDevice_DX12::getDescriptorIndex(const Resource& resource) const {
		if (resource.type == Resource::Type::TEXTURE) {
			auto internalTexture = (Impl::Texture_DX12*)resource.internalState.get();

			switch (internalTexture->subResourceType) {
			case SubresourceType::RTV:
			case SubresourceType::SRV:
			case SubresourceType::DSV:
				return internalTexture->srvDescriptor.index;
			case SubresourceType::UAV:
				return internalTexture->uavDescriptor.index;
			}
		}
		else if (resource.type == Resource::Type::BUFFER) {
			auto internalBuffer = (Impl::Buffer_DX12*)resource.internalState.get();

			return internalBuffer->srvDescriptor.index;
		}

		return 0;
	}

	void GraphicsDevice_DX12::waitForGPU() const {
		ComPtr<ID3D12Fence> fence = nullptr;
		ThrowIfFailed(m_Impl->device->CreateFence(
			0U,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&fence)
		));

		for (size_t i = 0; i < QUEUE_COUNT; ++i) {
			ThrowIfFailed(m_Impl->commandQueues[i].commandQueue->Signal(fence.Get(), 1U));

			if (fence->GetCompletedValue() < 1U) {
				ThrowIfFailed(fence->SetEventOnCompletion(1U, nullptr));
			}

			fence->Signal(0U);
		}
	}
}