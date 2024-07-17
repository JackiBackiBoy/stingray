#include "gbuffer_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace sr::gbufferpass {
	struct PushConstant {
		glm::mat4 modelMatrix = { 1.0f };
		uint32_t albedoMapIndex = 0;
		uint32_t normalMapIndex = 1;
		uint32_t pad1 = 0;
		uint32_t pad2 = 0;
	};

	static Shader g_VertexShader = {};
	static Shader g_PixelShader = {};
	static Pipeline g_Pipeline = {};
	static PushConstant g_PushConstant = {};
	static bool g_Initialized = false;

	void initialize(GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, L"assets/shaders/gbuffer.vs.hlsl", g_VertexShader);
		device.createShader(ShaderStage::PIXEL, L"assets/shaders/gbuffer.ps.hlsl", g_PixelShader);

		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = &g_PixelShader,
			.rasterizerState = {
				.cullMode = CullMode::BACK,
				.frontCW = true
			},
			.depthStencilState = {
				.depthEnable = true,
				.depthWriteMask = DepthWriteMask::ALL,
				.depthFunction = ComparisonFunc::LESS
			},
			.inputLayout = {
				.elements = {
					{ "POSITION", Format::R32G32B32_FLOAT },
					{ "NORMAL", Format::R32G32B32_FLOAT },
					{ "TANGENT", Format::R32G32B32_FLOAT },
					{ "TEXCOORD", Format::R32G32_FLOAT }
				}
			},
			.numRenderTargets = 3,
			.renderTargetFormats = {
				Format::R32G32B32A32_FLOAT, // position // TODO: Remove this and get position from depth instead, it will increase perf massively
				Format::R8G8B8A8_UNORM, // albedo
				Format::R16G16B16A16_FLOAT, // normal
			},
			.depthStencilFormat = Format::D32_FLOAT
		};

		device.createPipeline(pipelineInfo, g_Pipeline);
	}

	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const std::vector<Entity*>& entities) {
		RenderGraph& graph = *executeInfo.renderGraph;
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;

		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		device.bindPipeline(g_Pipeline, cmdList);
		device.bindResource(perFrameUBO, "g_PerFrameData", g_Pipeline, cmdList);

		for (const auto& entity : entities) {
			device.bindVertexBuffer(entity->model->vertexBuffer, cmdList);
			device.bindIndexBuffer(entity->model->indexBuffer, cmdList);

			g_PushConstant = {}; // reset push constant data
			g_PushConstant.modelMatrix =
				glm::translate(g_PushConstant.modelMatrix, entity->position) *
				sr::quatToMat4(entity->orientation) *
				glm::scale(g_PushConstant.modelMatrix, entity->scale);

			for (const auto& mesh : entity->model->meshes) {
				device.pushConstants(&g_PushConstant, sizeof(g_PushConstant), cmdList);
				device.drawIndexed(mesh.numIndices, mesh.baseIndex, mesh.baseVertex, cmdList);
			}
		}
	}
}