#pragma once

#include "../graphics.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "dxc/dxcapi.h"

#include <string>
#include <unordered_map>
#include <vector>

using namespace Microsoft::WRL;

namespace sr {
	struct Pipeline_DX12 {
		PipelineInfo info = {};
		ComPtr<ID3D12PipelineState> pipelineState = nullptr;
		ComPtr<ID3D12RootSignature> rootSignature = nullptr;

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements = {};
		std::unordered_map<std::string, uint32_t> rootParameterIndexLUT = {};
	};

	struct RayTracingPipeline_DX12 {
		ComPtr<ID3D12StateObject> pso = nullptr;
		ComPtr<ID3D12RootSignature> rootSignature = nullptr;

		std::unordered_map<std::string, uint32_t> rootParameterIndexLUT = {};
	};

	struct Sampler_DX12 {
		SamplerInfo info = {};
	};

	struct Shader_DX12 {
		ShaderStage stage = ShaderStage::NONE;
		ComPtr<IDxcBlob> shaderBlob = nullptr;

		std::vector<D3D12_ROOT_PARAMETER1> rootParameters = {};
		std::unordered_map<uint32_t, std::string> rootParameterNameLUT = {};
	};

	struct SwapChain_DX12 {
		SwapChainInfo info = {};
		ComPtr<IDXGISwapChain4> swapChain = nullptr;
		std::vector<ComPtr<ID3D12Resource>> backBuffers = {};
	};

	/* ------ Converter Functions ------ */
	constexpr D3D12_BLEND convertBlend(Blend value) {
		switch (value) {
		case Blend::ZERO:
			return D3D12_BLEND_ZERO;
		case Blend::ONE:
			return D3D12_BLEND_ONE;
		case Blend::SRC_COLOR:
			return D3D12_BLEND_SRC_COLOR;
		case Blend::INV_SRC_COLOR:
			return D3D12_BLEND_INV_SRC_COLOR;
		case Blend::SRC_ALPHA:
			return D3D12_BLEND_SRC_ALPHA;
		case Blend::INV_SRC_ALPHA:
			return D3D12_BLEND_INV_SRC_ALPHA;
		case Blend::DEST_ALPHA:
			return D3D12_BLEND_DEST_ALPHA;
		case Blend::INV_DEST_ALPHA:
			return D3D12_BLEND_INV_DEST_ALPHA;
		case Blend::DEST_COLOR:
			return D3D12_BLEND_DEST_COLOR;
		case Blend::INV_DEST_COLOR:
			return D3D12_BLEND_INV_DEST_COLOR;
		case Blend::SRC_ALPHA_SAT:
			return D3D12_BLEND_SRC_ALPHA_SAT;
		case Blend::BLEND_FACTOR:
			return D3D12_BLEND_BLEND_FACTOR;
		case Blend::INV_BLEND_FACTOR:
			return D3D12_BLEND_INV_BLEND_FACTOR;
		case Blend::SRC1_COLOR:
			return D3D12_BLEND_SRC1_COLOR;
		case Blend::INV_SRC1_COLOR:
			return D3D12_BLEND_INV_SRC1_COLOR;
		case Blend::SRC1_ALPHA:
			return D3D12_BLEND_SRC1_ALPHA;
		case Blend::INV_SRC1_ALPHA:
			return D3D12_BLEND_INV_SRC1_ALPHA;
		default:
			return D3D12_BLEND_ZERO;
		}
	}

	constexpr D3D12_BLEND convertAlphaBlend(Blend value) {
		switch (value) {
		case Blend::SRC_COLOR:
			return D3D12_BLEND_SRC_ALPHA;
		case Blend::INV_SRC_COLOR:
			return D3D12_BLEND_INV_SRC_ALPHA;
		case Blend::DEST_COLOR:
			return D3D12_BLEND_DEST_ALPHA;
		case Blend::INV_DEST_COLOR:
			return D3D12_BLEND_INV_DEST_ALPHA;
		case Blend::SRC1_COLOR:
			return D3D12_BLEND_SRC1_ALPHA;
		case Blend::INV_SRC1_COLOR:
			return D3D12_BLEND_INV_SRC1_ALPHA;
		default:
			return convertBlend(value);
		}
	}

	constexpr D3D12_BLEND_OP convertBlendOp(BlendOp value) {
		switch (value) {
		case BlendOp::ADD:
			return D3D12_BLEND_OP_ADD;
		case BlendOp::SUBTRACT:
			return D3D12_BLEND_OP_SUBTRACT;
		case BlendOp::REV_SUBTRACT:
			return D3D12_BLEND_OP_REV_SUBTRACT;
		case BlendOp::MIN:
			return D3D12_BLEND_OP_MIN;
		case BlendOp::MAX:
			return D3D12_BLEND_OP_MAX;
		default:
			return D3D12_BLEND_OP_ADD;
		}
	}

	constexpr D3D12_COMMAND_LIST_TYPE convertCommandListType(QueueType type) {
		switch (type) {
		case QueueType::DIRECT:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case QueueType::COPY:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		}

		return D3D12_COMMAND_LIST_TYPE_DIRECT; // TODO: Remove
	}

	constexpr D3D12_COMPARISON_FUNC convertComparisonFunc(ComparisonFunc value) {
		switch (value) {
		case ComparisonFunc::NEVER:
			return D3D12_COMPARISON_FUNC_NEVER;
		case ComparisonFunc::LESS:
			return D3D12_COMPARISON_FUNC_LESS;
		case ComparisonFunc::EQUAL:
			return D3D12_COMPARISON_FUNC_EQUAL;
		case ComparisonFunc::LESS_EQUAL:
			return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ComparisonFunc::GREATER:
			return D3D12_COMPARISON_FUNC_GREATER;
		case ComparisonFunc::NOT_EQUAL:
			return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case ComparisonFunc::GREATER_EQUAL:
			return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case ComparisonFunc::ALWAYS:
			return D3D12_COMPARISON_FUNC_ALWAYS;
		default:
			return D3D12_COMPARISON_FUNC_NEVER;
		}
	}

	constexpr D3D12_FILL_MODE convertFillMode(FillMode value) {
		switch (value) {
		case FillMode::SOLID:
			return D3D12_FILL_MODE_SOLID;
		default:
			return D3D12_FILL_MODE_WIREFRAME;
		}
	}

	constexpr D3D12_CULL_MODE convertCullMode(CullMode value) {
		switch (value) {
		case CullMode::FRONT:
			return D3D12_CULL_MODE_FRONT;
		case CullMode::BACK:
			return D3D12_CULL_MODE_BACK;
		default:
			return D3D12_CULL_MODE_NONE;
		}
	}

	constexpr D3D12_FILTER convertFilter(Filter value) {
		switch (value) {
		case Filter::MIN_MAG_MIP_POINT:
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case Filter::MIN_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		case Filter::MIN_POINT_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case Filter::MIN_POINT_MAG_MIP_LINEAR:
			return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		case Filter::MIN_LINEAR_MAG_MIP_POINT:
			return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		case Filter::MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		case Filter::MIN_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		case Filter::MIN_MAG_MIP_LINEAR:
			return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		case Filter::ANISOTROPIC:
			return D3D12_FILTER_ANISOTROPIC;
		case Filter::COMPARISON_MIN_MAG_MIP_POINT:
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		case Filter::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
		case Filter::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case Filter::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
		case Filter::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
		case Filter::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		case Filter::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		case Filter::COMPARISON_MIN_MAG_MIP_LINEAR:
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		case Filter::COMPARISON_ANISOTROPIC:
			return D3D12_FILTER_COMPARISON_ANISOTROPIC;
		case Filter::MINIMUM_MIN_MAG_MIP_POINT:
			return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
		case Filter::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;
		case Filter::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case Filter::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:
			return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;
		case Filter::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:
			return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;
		case Filter::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		case Filter::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;
		case Filter::MINIMUM_MIN_MAG_MIP_LINEAR:
			return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
		case Filter::MINIMUM_ANISOTROPIC:
			return D3D12_FILTER_MINIMUM_ANISOTROPIC;
		case Filter::MAXIMUM_MIN_MAG_MIP_POINT:
			return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
		case Filter::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;
		case Filter::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case Filter::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:
			return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;
		case Filter::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:
			return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
		case Filter::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		case Filter::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:
			return D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;
		case Filter::MAXIMUM_MIN_MAG_MIP_LINEAR:
			return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
		case Filter::MAXIMUM_ANISOTROPIC:
			return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
		default:
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		}
	}

	constexpr DXGI_FORMAT convertFormat(Format value) {
		switch (value) {
		case Format::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
		case Format::R32G32B32A32_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Format::R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case Format::R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;
		case Format::R32G32B32_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case Format::R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;
		case Format::R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;
		case Format::R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case Format::R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case Format::R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case Format::R16G16B16A16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;
		case Format::R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;
		case Format::R32G32_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		case Format::R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;
		case Format::R32G32_SINT:
			return DXGI_FORMAT_R32G32_SINT;
		case Format::D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		case Format::R10G10B10A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case Format::R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;
		case Format::R11G11B10_FLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;
		case Format::R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Format::R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case Format::R8G8B8A8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		case Format::R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;
		case Format::R16G16_FLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;
		case Format::R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;
		case Format::R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;
		case Format::R16G16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;
		case Format::R16G16_SINT:
			return DXGI_FORMAT_R16G16_SINT;
		case Format::D32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;
		case Format::R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case Format::R32_UINT:
			return DXGI_FORMAT_R32_UINT;
		case Format::R32_SINT:
			return DXGI_FORMAT_R32_SINT;
		case Format::D24_UNORM_S8_UINT:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case Format::R9G9B9E5_SHAREDEXP:
			return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		case Format::R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case Format::R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case Format::R8G8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;
		case Format::R8G8_SINT:
			return DXGI_FORMAT_R8G8_SINT;
		case Format::R16_FLOAT:
			return DXGI_FORMAT_R16_FLOAT;
		case Format::D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
		case Format::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case Format::R16_UINT:
			return DXGI_FORMAT_R16_UINT;
		case Format::R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;
		case Format::R16_SINT:
			return DXGI_FORMAT_R16_SINT;
		case Format::R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;
		case Format::R8_UINT:
			return DXGI_FORMAT_R8_UINT;
		case Format::R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;
		case Format::R8_SINT:
			return DXGI_FORMAT_R8_SINT;
		case Format::BC1_UNORM:
			return DXGI_FORMAT_BC1_UNORM;
		case Format::BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM_SRGB;
		case Format::BC2_UNORM:
			return DXGI_FORMAT_BC2_UNORM;
		case Format::BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM_SRGB;
		case Format::BC3_UNORM:
			return DXGI_FORMAT_BC3_UNORM;
		case Format::BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM_SRGB;
		case Format::BC4_UNORM:
			return DXGI_FORMAT_BC4_UNORM;
		case Format::BC4_SNORM:
			return DXGI_FORMAT_BC4_SNORM;
		case Format::BC5_UNORM:
			return DXGI_FORMAT_BC5_UNORM;
		case Format::BC5_SNORM:
			return DXGI_FORMAT_BC5_SNORM;
		case Format::B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case Format::B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case Format::BC6H_UF16:
			return DXGI_FORMAT_BC6H_UF16;
		case Format::BC6H_SF16:
			return DXGI_FORMAT_BC6H_SF16;
		case Format::BC7_UNORM:
			return DXGI_FORMAT_BC7_UNORM;
		case Format::BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM_SRGB;
		case Format::NV12:
			return DXGI_FORMAT_NV12;
		}
		return DXGI_FORMAT_UNKNOWN;
	}

	constexpr D3D12_INPUT_CLASSIFICATION convertInputClassification(InputClassification value) {
		switch (value) {
		case InputClassification::PER_INSTANCE_DATA:
			return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
		default:
			return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		}
	}

	constexpr D3D12_RESOURCE_STATES convertResourceState(ResourceState value) {
		D3D12_RESOURCE_STATES state = {};

		if (has_flag(value, ResourceState::SHADER_RESOURCE)) {
			state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		if (has_flag(value, ResourceState::UNORDERED_ACCESS)) {
			state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		if (has_flag(value, ResourceState::COPY_SRC)) {
			state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		if (has_flag(value, ResourceState::COPY_DST)) {
			state |= D3D12_RESOURCE_STATE_COPY_DEST;
		}

		if (has_flag(value, ResourceState::RENDER_TARGET)) {
			state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		if (has_flag(value, ResourceState::DEPTH_WRITE)) {
			state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		if (has_flag(value, ResourceState::DEPTH_READ)) {
			state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		}

		return state;
	}

	constexpr D3D12_STATIC_BORDER_COLOR convertSamplerBorderColor(SamplerBorderColor value) {
		switch (value) {
		case SamplerBorderColor::TRANSPARENT_BLACK:
			return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		case SamplerBorderColor::OPAQUE_BLACK:
			return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		case SamplerBorderColor::OPAQUE_WHITE:
			return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		default:
			return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		}
	}

	constexpr D3D12_TEXTURE_ADDRESS_MODE convertTextureAddressMode(TextureAddressMode value) {
		switch (value) {
		case TextureAddressMode::WRAP:
			return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case TextureAddressMode::MIRROR:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case TextureAddressMode::CLAMP:
			return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case TextureAddressMode::BORDER:
			return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		case TextureAddressMode::MIRROR_ONCE:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		default:
			return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	}
}