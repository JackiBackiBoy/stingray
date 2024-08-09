#include "simple_shadow_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace sr::simpleshadowpass {
	struct alignas(256) ShadowUBO {
		glm::mat4 lightSpaceMatrix = { 1.0f };
	};

	struct PushConstant {
		glm::mat4 modelMatrix = { 1.0f };
	};

	static Shader g_VertexShader = {};
	static Pipeline g_Pipeline = {};
	static Buffer g_ShadowUBOs[GraphicsDevice::NUM_BUFFERS] = {};
	static ShadowUBO g_ShadowUBOData = {};
	static PushConstant g_PushConstant = {};
	static bool g_Initialized = false;

	static void initialize(GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, "assets/shaders/simple_shadows.vs.hlsl", g_VertexShader);

		// Pipeline
		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = nullptr, // no pixel shader for shadow pass
			.rasterizerState = {
				.cullMode = CullMode::FRONT,
				.frontCW = true,
			},
			.depthStencilState = {
				.depthEnable = true,
				.stencilEnable = false,
				.depthWriteMask = DepthWriteMask::ALL,
				.depthFunction = ComparisonFunc::LESS
			},
			.inputLayout = {
				.elements = {
					{ "POSITION", Format::R32G32B32_FLOAT },
					{ "NORMAL", Format::R32G32B32_FLOAT },
					{ "TANGENT", Format::R32G32B32_FLOAT },
					{ "TEXCOORD", Format::R32G32_FLOAT },
				}
			},
			.depthStencilFormat = Format::D16_UNORM
		};

		device.createPipeline(pipelineInfo, g_Pipeline);

		// Shadow UBOs
		const BufferInfo shadowUBOInfo = {
			.size = sizeof(ShadowUBO),
			.stride = sizeof(ShadowUBO),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::UNIFORM_BUFFER,
			.persistentMap = true
		};

		for (size_t i = 0; i < GraphicsDevice::NUM_BUFFERS; ++i) {
			device.createBuffer(shadowUBOInfo, g_ShadowUBOs[i], &g_ShadowUBOData);
		}
	}

	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const std::vector<Entity*>& entities) {
		GraphicsDevice& device = *executeInfo.device;
		RenderGraph& renderGraph = *executeInfo.renderGraph;
		const CommandList& cmdList = *executeInfo.cmdList;

		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		device.bindPipeline(g_Pipeline, cmdList);
		device.bindResource(g_ShadowUBOs[device.getBufferIndex()], "g_ShadowUBO", g_Pipeline, cmdList);

		for (const auto& entity : entities) {
			device.bindVertexBuffer(entity->model->vertexBuffer, cmdList);
			device.bindIndexBuffer(entity->model->indexBuffer, cmdList);

			g_PushConstant = {}; // reset push constant data
			g_PushConstant.modelMatrix =
				glm::translate(g_PushConstant.modelMatrix, entity->position) *
				sr::quatToMat4(entity->orientation) *
				glm::scale(g_PushConstant.modelMatrix, entity->scale);
			device.pushConstants(&g_PushConstant, sizeof(g_PushConstant), cmdList);

			for (const auto& mesh : entity->model->meshes) {
				device.drawIndexed(mesh.numIndices, mesh.baseIndex, mesh.baseVertex, cmdList);
			}
		}
	}

}