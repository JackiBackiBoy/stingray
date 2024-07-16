#include "fullscreen_tri_pass.hpp"

namespace sr::fstripass {
	static bool g_Initialized = false;

	struct PushConstant {
		uint32_t renderTargetIndex;
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

	void onExecute(RenderGraph& graph, GraphicsDevice& device, const CommandList& commandList) {
		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		auto rtOutputAttachment = graph.getAttachment("RTOutput");
		PushConstant pushConstant = { device.getDescriptorIndex(rtOutputAttachment->texture) };

		device.bindPipeline(g_Pipeline, commandList);

		device.pushConstants(&pushConstant, sizeof(pushConstant), commandList);
		device.draw(3, 0, commandList);
	}

}