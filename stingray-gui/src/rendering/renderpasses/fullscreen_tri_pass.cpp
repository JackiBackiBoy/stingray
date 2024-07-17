#include "fullscreen_tri_pass.hpp"

namespace sr::fstripass {
	static bool g_Initialized = false;

	struct PushConstant {
		uint32_t gBufferPositionIndex;
		uint32_t gBufferAlbedoIndex;
		uint32_t gBufferNormalIndex;
		uint32_t aoIndex;
	};

	static Shader g_VertexShader = {};
	static Shader g_PixelShader = {};
	static Pipeline g_Pipeline = {};

	static void initialize(GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, L"assets/shaders/fullscreen_tri.vs.hlsl", g_VertexShader);
		device.createShader(ShaderStage::PIXEL, L"assets/shaders/fullscreen_tri.ps.hlsl", g_PixelShader);

		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = &g_PixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM }
		};

		device.createPipeline(pipelineInfo, g_Pipeline);
	}

	void onExecute(RenderGraph& graph, GraphicsDevice& device, const CommandList& cmdList) {
		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		//auto rtOutputAttachment = graph.getAttachment("RTOutput");
		//PushConstant pushConstant = { device.getDescriptorIndex(rtOutputAttachment->texture) };
		auto positionAttachment = graph.getAttachment("Position");
		auto albedoAttachment = graph.getAttachment("Albedo");
		auto normalAttachment = graph.getAttachment("Normal");
		auto aoAttachment = graph.getAttachment("AmbientOcclusion");

		const PushConstant pushConstant = {
			.gBufferPositionIndex = device.getDescriptorIndex(positionAttachment->texture),
			.gBufferAlbedoIndex = device.getDescriptorIndex(albedoAttachment->texture),
			.gBufferNormalIndex = device.getDescriptorIndex(normalAttachment->texture),
			.aoIndex = device.getDescriptorIndex(aoAttachment->texture)
		};

		device.bindPipeline(g_Pipeline, cmdList);

		device.pushConstants(&pushConstant, sizeof(pushConstant), cmdList);
		device.draw(3, 0, cmdList);
	}

}