#include "accumulation_pass.hpp"

namespace sr::accumulationpass {
	struct PushConstant {
		uint32_t lastFrameIndex;
		uint32_t currFrameIndex;
		uint32_t accumulationCount;
	};

	static Shader g_VertexShader = {};
	static Shader g_PixelShader = {};
	static Pipeline g_Pipeline = {};
	static Texture g_AccumulationTexture = {};
	static uint32_t g_AccumulationCount = 0;
	static bool g_Initialized = false;

	static void initialize(RenderGraph& graph, GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, "assets/shaders/accumulation.vs.hlsl", g_VertexShader);
		device.createShader(ShaderStage::PIXEL, "assets/shaders/accumulation.ps.hlsl", g_PixelShader);

		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = &g_PixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM }
		};

		device.createPipeline(pipelineInfo, g_Pipeline);

		auto aoAttachment = graph.getAttachment("AmbientOcclusion");
		const TextureInfo accumulationTextureInfo = {
			.width = aoAttachment->info.width,
			.height = aoAttachment->info.height,
			.format = aoAttachment->info.format,
			.bindFlags = BindFlag::SHADER_RESOURCE
		};

		device.createTexture(accumulationTextureInfo, g_AccumulationTexture, nullptr);
	}

	void onExecute(PassExecuteInfo& executeInfo) {
		RenderGraph& graph = *executeInfo.renderGraph;
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;

		if (!g_Initialized) {
			initialize(graph, device);
			g_Initialized = true;
		}

		if (executeInfo.frameInfo->cameraMoved) {
			g_AccumulationCount = 0;
		}

		auto thisAttachment = graph.getAttachment("AOAccumulation");
		auto aoAttachment = graph.getAttachment("AmbientOcclusion");

		const PushConstant pushConstant = {
			.lastFrameIndex = device.getDescriptorIndex(g_AccumulationTexture),
			.currFrameIndex = device.getDescriptorIndex(aoAttachment->texture),
			.accumulationCount = g_AccumulationCount++
		};

		device.bindPipeline(g_Pipeline, cmdList);
		device.pushConstants(&pushConstant, sizeof(pushConstant), cmdList);
		device.draw(3, 0, cmdList);

		// Copy current frame to last frame
		GPUBarrier preBarrier1 = GPUBarrier::imageBarrier(&thisAttachment->texture, ResourceState::RENDER_TARGET, ResourceState::COPY_SRC);
		GPUBarrier preBarrier2 = GPUBarrier::imageBarrier(&g_AccumulationTexture, ResourceState::SHADER_RESOURCE, ResourceState::COPY_DST);
		device.barrier(preBarrier1, cmdList);
		device.barrier(preBarrier2, cmdList);

		device.copyResource(&g_AccumulationTexture, &thisAttachment->texture, cmdList);

		GPUBarrier postBarrier1 = GPUBarrier::imageBarrier(&thisAttachment->texture, ResourceState::COPY_SRC, ResourceState::RENDER_TARGET);
		GPUBarrier postBarrier2 = GPUBarrier::imageBarrier(&g_AccumulationTexture, ResourceState::COPY_DST, ResourceState::SHADER_RESOURCE);
		device.barrier(postBarrier1, cmdList);
		device.barrier(postBarrier2, cmdList);
	}

}