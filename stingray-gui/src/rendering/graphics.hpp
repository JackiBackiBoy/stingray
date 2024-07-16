#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../core/enum_flags.hpp"

namespace sr {
	enum class GraphicsAPI {
		DX12, // TODO: Work in progress
		Metal, // TODO: Not supported
		Vulkan // TODO: Not supported
	};

	enum class BindFlag {
		NONE = 0,
		VERTEX_BUFFER = 1 << 0,
		INDEX_BUFFER = 1 << 1,
		UNIFORM_BUFFER = 1 << 2,
		SHADER_RESOURCE = 1 << 3,
		RENDER_TARGET = 1 << 4,
		DEPTH_STENCIL = 1 << 5,
		UNORDERED_ACCESS = 1 << 6,
		SHADING_RATE = 1 << 7,
	};

	enum class QueueType {
		DIRECT,
		COPY,

		QUEUE_COUNT
	};

	enum class MiscFlag {
		NONE = 0,
		TEXTURECUBE = 1 << 0,
		INDIRECT_ARGS = 1 << 1,
		BUFFER_RAW = 1 << 2,
		BUFFER_STRUCTURED = 1 << 3,
	};

	enum class ShaderStage {
		NONE = 0,
		VERTEX = 1 << 0,
		PIXEL = 1 << 1,
		COMPUTE = 1 << 2,
		LIBRARY = 1 << 3,
	};

	enum class Blend {
		ZERO,
		ONE,
		SRC_COLOR,
		INV_SRC_COLOR,
		SRC_ALPHA,
		INV_SRC_ALPHA,
		DEST_ALPHA,
		INV_DEST_ALPHA,
		DEST_COLOR,
		INV_DEST_COLOR,
		SRC_ALPHA_SAT,
		BLEND_FACTOR,
		INV_BLEND_FACTOR,
		SRC1_COLOR,
		INV_SRC1_COLOR,
		SRC1_ALPHA,
		INV_SRC1_ALPHA,
	};

	enum class BlendOp {
		ADD,
		SUBTRACT,
		REV_SUBTRACT,
		MIN,
		MAX,
	};

	enum class ComparisonFunc {
		NEVER,
		LESS,
		EQUAL,
		LESS_EQUAL,
		GREATER,
		NOT_EQUAL,
		GREATER_EQUAL,
		ALWAYS,
	};

	enum class DepthWriteMask {
		ZERO,	// Disables depth write
		ALL,	// Enables depth write
	};

	enum class FillMode {
		WIREFRAME,
		SOLID
	};

	enum class CullMode {
		NONE,
		FRONT,
		BACK,
	};

	enum class Filter {
		MIN_MAG_MIP_POINT,
		MIN_MAG_POINT_MIP_LINEAR,
		MIN_POINT_MAG_LINEAR_MIP_POINT,
		MIN_POINT_MAG_MIP_LINEAR,
		MIN_LINEAR_MAG_MIP_POINT,
		MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MIN_MAG_LINEAR_MIP_POINT,
		MIN_MAG_MIP_LINEAR,
		ANISOTROPIC,
		COMPARISON_MIN_MAG_MIP_POINT,
		COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
		COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
		COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_MAG_MIP_LINEAR,
		COMPARISON_ANISOTROPIC,
		MINIMUM_MIN_MAG_MIP_POINT,
		MINIMUM_MIN_MAG_POINT_MIP_LINEAR,
		MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
		MINIMUM_MIN_POINT_MAG_MIP_LINEAR,
		MINIMUM_MIN_LINEAR_MAG_MIP_POINT,
		MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MINIMUM_MIN_MAG_LINEAR_MIP_POINT,
		MINIMUM_MIN_MAG_MIP_LINEAR,
		MINIMUM_ANISOTROPIC,
		MAXIMUM_MIN_MAG_MIP_POINT,
		MAXIMUM_MIN_MAG_POINT_MIP_LINEAR,
		MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
		MAXIMUM_MIN_POINT_MAG_MIP_LINEAR,
		MAXIMUM_MIN_LINEAR_MAG_MIP_POINT,
		MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MAXIMUM_MIN_MAG_LINEAR_MIP_POINT,
		MAXIMUM_MIN_MAG_MIP_LINEAR,
		MAXIMUM_ANISOTROPIC,
	};

	enum class Format {
		UNKNOWN,

		R32G32B32A32_FLOAT,
		R32G32B32A32_UINT,
		R32G32B32A32_SINT,

		R32G32B32_FLOAT,
		R32G32B32_UINT,
		R32G32B32_SINT,

		R16G16B16A16_FLOAT,
		R16G16B16A16_UNORM,
		R16G16B16A16_UINT,
		R16G16B16A16_SNORM,
		R16G16B16A16_SINT,

		R32G32_FLOAT,
		R32G32_UINT,
		R32G32_SINT,
		D32_FLOAT_S8X24_UINT,	// depth (32-bit) + stencil (8-bit) | SRV: R32_FLOAT (default or depth aspect), R8_UINT (stencil aspect)

		R10G10B10A2_UNORM,
		R10G10B10A2_UINT,
		R11G11B10_FLOAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB,
		R8G8B8A8_UINT,
		R8G8B8A8_SNORM,
		R8G8B8A8_SINT,
		B8G8R8A8_UNORM,
		B8G8R8A8_UNORM_SRGB,
		R16G16_FLOAT,
		R16G16_UNORM,
		R16G16_UINT,
		R16G16_SNORM,
		R16G16_SINT,
		D32_FLOAT,				// depth (32-bit) | SRV: R32_FLOAT
		R32_FLOAT,
		R32_UINT,
		R32_SINT,
		D24_UNORM_S8_UINT,		// depth (24-bit) + stencil (8-bit) | SRV: R24_INTERNAL (default or depth aspect), R8_UINT (stencil aspect)
		R9G9B9E5_SHAREDEXP,

		R8G8_UNORM,
		R8G8_UINT,
		R8G8_SNORM,
		R8G8_SINT,
		R16_FLOAT,
		D16_UNORM,				// depth (16-bit) | SRV: R16_UNORM
		R16_UNORM,
		R16_UINT,
		R16_SNORM,
		R16_SINT,

		R8_UNORM,
		R8_UINT,
		R8_SNORM,
		R8_SINT,

		// Formats that are not usable in render pass must be below because formats in render pass must be encodable as 6 bits:

		BC1_UNORM,			// Three color channels (5 bits:6 bits:5 bits), with 0 or 1 bit(s) of alpha
		BC1_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits), with 0 or 1 bit(s) of alpha
		BC2_UNORM,			// Three color channels (5 bits:6 bits:5 bits), with 4 bits of alpha
		BC2_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits), with 4 bits of alpha
		BC3_UNORM,			// Three color channels (5 bits:6 bits:5 bits) with 8 bits of alpha
		BC3_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits) with 8 bits of alpha
		BC4_UNORM,			// One color channel (8 bits)
		BC4_SNORM,			// One color channel (8 bits)
		BC5_UNORM,			// Two color channels (8 bits:8 bits)
		BC5_SNORM,			// Two color channels (8 bits:8 bits)
		BC6H_UF16,			// Three color channels (16 bits:16 bits:16 bits) in "half" floating point
		BC6H_SF16,			// Three color channels (16 bits:16 bits:16 bits) in "half" floating point
		BC7_UNORM,			// Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha
		BC7_UNORM_SRGB,		// Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha

		NV12,				// video YUV420; SRV Luminance aspect: R8_UNORM, SRV Chrominance aspect: R8G8_UNORM
	};

	enum class InputClassification {
		PER_VERTEX_DATA,
		PER_INSTANCE_DATA,
	};

	enum class RayTracingASType {
		TLAS, // Top-Level Acceleration Structure
		BLAS, // Bottom-Level Acceleration Structure
	};

	enum class ResourceState {
		UNDEFINED = 0,
		SHADER_RESOURCE = 1 << 0,
		UNORDERED_ACCESS = 1 << 1,
		RENDER_TARGET = 1 << 2,
		DEPTH_WRITE = 1 << 3,
		DEPTH_READ = 1 << 4,

		COPY_SRC = 1 << 5, // copy from
		COPY_DST = 1 << 6, // copy to
	};

	enum class SamplerBorderColor {
		TRANSPARENT_BLACK,
		OPAQUE_BLACK,
		OPAQUE_WHITE,
	};

	enum class SubresourceType : uint8_t {
		SRV, // shader resource view
		UAV, // unordered access view
		RTV, // render target view
		DSV, // depth stencil view
	};

	enum class TextureAddressMode {
		WRAP,
		MIRROR,
		CLAMP,
		BORDER,
		MIRROR_ONCE,
	};

	enum class Usage {
		DEFAULT, // CPU no access, GPU read/write. TIP: Useful for resources that do not change that often or at all
		UPLOAD, // CPU write, GPU read. TIP: Useful for resources that need to be updated frequently (e.g. uniform buffer). Also allows for persistent mapping
		COPY // Copy from GPU to CPU
	};

	struct BlendState {
		bool alphaToCoverage = false;
		bool independentBlend = false;

		struct RenderTargetBlendState {
			bool blendEnable = false;
			Blend srcBlend = Blend::SRC_ALPHA;
			Blend dstBlend = Blend::INV_SRC_ALPHA;
			BlendOp blendOp = BlendOp::ADD;

			Blend srcBlendAlpha = Blend::ONE;
			Blend dstBlendAlpha = Blend::ONE;
			BlendOp blendOpAlpha = BlendOp::ADD;
		};
		RenderTargetBlendState renderTargetBlendStates[8];
	};

	struct CommandList {
		QueueType type = QueueType::DIRECT;
		void* internalState = nullptr;
	};

	struct Resource {
		enum class Type {
			UNKNOWN,
			BUFFER,
			TEXTURE,
		} type = Type::UNKNOWN;

		void* mappedData = nullptr; // NOTE: Only valid for Usage::UPLOAD
		size_t mappedSize = 0; // NOTE: For buffers: full buffer size; for textures: full texture size including subresources

		std::shared_ptr<void> internalState = nullptr;
	};

	struct BufferInfo {
		uint64_t size = 0;
		uint32_t stride = 0;
		Usage usage = Usage::DEFAULT;
		BindFlag bindFlags = BindFlag::NONE;
		MiscFlag miscFlags = MiscFlag::NONE;
		bool persistentMap = false; // NOTE: Only considered for Usage::UPLOAD
	};

	struct Buffer : public Resource {
		BufferInfo info = {};
	};

	struct DepthStencilState {
		bool depthEnable = false;
		bool stencilEnable = false;
		DepthWriteMask depthWriteMask = DepthWriteMask::ZERO;
		ComparisonFunc depthFunction = ComparisonFunc::NEVER;
	};

	struct InputLayout {
		struct Element {
			std::string name = "";
			Format format = Format::UNKNOWN;
			InputClassification inputClassification = InputClassification::PER_VERTEX_DATA;
		};

		std::vector<Element> elements = {};
	};

	struct RasterizerState {
		FillMode fillMode = FillMode::SOLID;
		CullMode cullMode = CullMode::NONE;
		bool frontCW = false;
		int32_t depthBias = 0;
		float depthBiasClamp = 0.0f;
		float slopeScaledDepthBias = 0.0f;
		bool depthClipEnable = false;
		bool multisampleEnable = false;
		bool antialisedLineEnable = false;
	};

	struct Shader {
		ShaderStage stage = ShaderStage::NONE;
		std::shared_ptr<void> internalState = nullptr;
	};

	struct PipelineInfo {
		const Shader* vertexShader = nullptr;
		const Shader* fragmentShader = nullptr;
		const BlendState* blendState = nullptr;
		RasterizerState rasterizerState = {};
		DepthStencilState depthStencilState = {};
		InputLayout inputLayout = {};
		uint32_t numRenderTargets = 0;
		Format renderTargetFormats[8] = { Format::UNKNOWN };
		Format depthStencilFormat = Format::D32_FLOAT;
	};

	struct Pipeline {
		PipelineInfo info = {};
		std::shared_ptr<void> internalState = nullptr;
	};

	/* ------------------------- */
	/* ------ Ray Tracing ------ */
	/* ------------------------- */

	struct DispatchRaysInfo {
		const Buffer* rayGenTable = nullptr;
		const Buffer* missTable = nullptr;
		const Buffer* hitGroupTable = nullptr;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1;
	};

	struct RayTracingTLAS {
		struct Instance {
			float transform[3][4] = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			};
			uint32_t instanceID = 0;
			uint32_t instanceMask = 0;
			uint32_t instanceContributionHitGroupIndex = 0;
			uint32_t flags = 0;
			const Resource* blasResource = nullptr;
		};

		const Buffer* instanceBuffer = nullptr;
		uint32_t offset = 0;
		uint32_t numInstances = 0;
	};

	struct RayTracingBLASGeometry {
		enum class Type {
			TRIANGLES,
		} type;

		struct Triangles {
			const Buffer* vertexBuffer = nullptr;
			const Buffer* indexBuffer = nullptr;
			Format vertexFormat = Format::UNKNOWN;
			uint32_t vertexCount = 0;
			uint32_t vertexStride = 0;
			uint64_t vertexByteOffset = 0;
			uint32_t indexCount = 0;
			uint32_t indexOffset = 0;
		} triangles;
	};

	struct RayTracingBLAS {
		std::vector<RayTracingBLASGeometry> geometries = {};
	};

	struct RayTracingASInfo { // NOTE: Only one of the acceleration structures will be used depending on the type (TLAS or BLAS, not both)
		RayTracingASType type = {};
		RayTracingTLAS tlas;
		RayTracingBLAS blas;
	};

	struct RayTracingAS : public Resource {
		RayTracingASInfo info = {};
	};

	struct RayTracingShaderLibrary {
		enum class Type {
			RAYGENERATION,
			MISS,
			CLOSESTHIT,
			ANYHIT,
			INTERSECTION
		} type = Type::RAYGENERATION;

		const Shader* shader = nullptr;
		std::string functionName;
	};

	struct RayTracingShaderHitGroup {
		enum class Type {
			GENERAL,
			TRIANGLES,
			PROCEDURAL
		} type = Type::TRIANGLES;

		std::string name;
		uint32_t generalShader = ~0u;
		uint32_t closestHitShader = ~0u;
		uint32_t anyHitShader = ~0u;
		uint32_t intersectionShader = ~0u;
	};

	struct RayTracingPipelineInfo {
		std::vector<RayTracingShaderLibrary> shaderLibraries = {};
		std::vector<RayTracingShaderHitGroup> hitGroups = {};
		// TODO: Add more
	};

	struct RayTracingPipeline {
		RayTracingPipelineInfo info = {};
		std::shared_ptr<void> internalState = nullptr;
	};

	struct SubresourceData {
		const void* data = nullptr;
		uint32_t rowPitch = 0;
		uint32_t slicePitch = 0; // NOTE: Only used for 3D textures
	};

	struct TextureInfo {
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1; // NOTE: Only used for 3D textures
		uint32_t arraySize = 1;
		uint32_t mipLevels = 1;
		uint32_t sampleCount = 1;
		Format format = Format::UNKNOWN;
		Usage usage = Usage::DEFAULT;
		BindFlag bindFlags = BindFlag::NONE;
	};

	struct Texture : public Resource {
		TextureInfo info = {};
	};

	struct GPUBarrier {
		enum class Type {
			UAV, // UAV accesses
			IMAGE,
			BUFFER,
		} type = Type::IMAGE;

		struct UAV {
			const Resource* resource = nullptr;
		};

		struct Image {
			const Texture* texture = nullptr;
			ResourceState stateBefore = {};
			ResourceState stateAfter = {};
		};

		struct Buffer {
			const Buffer* buffer = nullptr;
			ResourceState stateBefore = {};
			ResourceState stateAfter = {};
		};

		union {
			UAV uav;
			Image image;
			Buffer buffer;
		};

		static GPUBarrier UAV(const Resource* resource) {
			const GPUBarrier barrier = {
				.type = Type::UAV,
				.uav = {
					.resource = resource
				}
			};

			return barrier;
		}

		static GPUBarrier imageBarrier(const Texture* texture, ResourceState before, ResourceState after) {
			const GPUBarrier barrier = {
				.type = Type::IMAGE,
				.image = {
					.texture = texture,
					.stateBefore = before,
					.stateAfter = after
				}
			};

			return barrier;
		}

		static GPUBarrier bufferBarrier(const Buffer* buffer, ResourceState before, ResourceState after) {
			const GPUBarrier barrier = {
				.type = Type::BUFFER,
				.buffer = {
					.buffer = buffer,
					.stateBefore = before,
					.stateAfter = after
				}
			};

			return barrier;
		}
	};

	struct RenderPassInfo {
		const Texture* colors[8] = { nullptr };
		uint32_t numColorAttachments = 0;
		const Texture* depth = nullptr;
	};

	struct SamplerInfo {
		Filter filter = Filter::MIN_MAG_MIP_LINEAR;
		TextureAddressMode addressU = TextureAddressMode::WRAP;
		TextureAddressMode addressV = TextureAddressMode::WRAP;
		TextureAddressMode addressW = TextureAddressMode::WRAP;
		float mipLODBias = 0;
		uint32_t maxAnisotropy = 0;
		ComparisonFunc comparisonFunc = ComparisonFunc::NEVER;
		SamplerBorderColor borderColor = SamplerBorderColor::TRANSPARENT_BLACK;
		float minLOD = 0;
		float maxLOD = std::numeric_limits<float>::max();
	};

	struct Sampler {
		SamplerInfo info = {};
		std::shared_ptr<void> internalState = nullptr;
	};

	struct SwapChainInfo {
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t bufferCount = 2;
		Format format = Format::R8G8B8A8_UNORM;
		bool fullscreen = false;
		bool vsync = true;
	};

	struct SwapChain {
		SwapChainInfo info = {};

		Texture backbuffer = {}; // TODO: Temporary fix
		std::shared_ptr<void> internalState = nullptr;
	};

	struct Viewport {
		float topLeftX = 0.0f;
		float topLeftY = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};

	template<typename T>
	constexpr T alignTo(T value, T alignment) {
		return ((value + alignment - T(1)) / alignment) * alignment;
	}

	template<>
	struct enable_bitmask_operators<BindFlag> { static const bool enable = true; };
	template<>
	struct enable_bitmask_operators<MiscFlag> { static const bool enable = true; };
	template<>
	struct enable_bitmask_operators<ShaderStage> { static const bool enable = true; };
	template<>
	struct enable_bitmask_operators<ResourceState> { static const bool enable = true; };
}