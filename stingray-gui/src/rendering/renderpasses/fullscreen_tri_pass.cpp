#include "fullscreen_tri_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace sr::fstripass {
	struct alignas(256) LightingUBO {
		DirectionLight directionLight = {};
		uint32_t numPointLights = 0;
		uint32_t pad1 = 0;
		uint32_t pad2 = 0;
		uint32_t pad3 = 0;
		PointLight pointLights[Scene::MAX_POINT_LIGHTS] = {};
	};

	struct PushConstant {
		uint32_t gBufferPositionIndex;
		uint32_t gBufferAlbedoIndex;
		uint32_t gBufferNormalIndex;
		uint32_t depthIndex;
		uint32_t shadowMapIndex;
		uint32_t aoIndex;
		float shadowMinBias;
		float shadowMaxBias;
	};

	static Shader g_VertexShader = {};
	static Shader g_PixelShader = {};
	static Pipeline g_Pipeline = {};
	static Buffer g_LightingUBOs[GraphicsDevice::NUM_BUFFERS] = {};
	static LightingUBO g_LightingUBOData = {};
	static bool g_Initialized = false;

	static void initialize(GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, "assets/shaders/fullscreen_tri.vs.hlsl", g_VertexShader);
		device.createShader(ShaderStage::PIXEL, "assets/shaders/fullscreen_tri.ps.hlsl", g_PixelShader);

		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = &g_PixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM }
		};

		device.createPipeline(pipelineInfo, g_Pipeline);

		const BufferInfo lightingUBOInfo = {
			.size = sizeof(LightingUBO),
			.stride = sizeof(LightingUBO),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::UNIFORM_BUFFER,
			.persistentMap = true
		};

		for (size_t i = 0; i < GraphicsDevice::NUM_BUFFERS; ++i) {
			device.createBuffer(lightingUBOInfo, g_LightingUBOs[i], &g_LightingUBOData);
		}
	}

	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const Settings& settings, Scene& scene) {
		RenderGraph& graph = *executeInfo.renderGraph;
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;

		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		// Push constant data
		auto positionAttachment = graph.getAttachment("Position");
		auto albedoAttachment = graph.getAttachment("Albedo");
		auto normalAttachment = graph.getAttachment("Normal");
		auto depthAttachment = graph.getAttachment("Depth");
		auto shadowAttachment = graph.getAttachment("ShadowMap");
		auto accumulationAttachment = graph.getAttachment("AOAccumulation");

		const PushConstant pushConstant = {
			.gBufferPositionIndex = device.getDescriptorIndex(positionAttachment->texture),
			.gBufferAlbedoIndex = device.getDescriptorIndex(albedoAttachment->texture),
			.gBufferNormalIndex = device.getDescriptorIndex(normalAttachment->texture),
			.depthIndex = device.getDescriptorIndex(depthAttachment->texture),
			.shadowMapIndex = settings.enableShadows ? device.getDescriptorIndex(shadowAttachment->texture) : 0,
			.aoIndex = settings.enableAO ? device.getDescriptorIndex(accumulationAttachment->texture) : 0,
			.shadowMinBias = settings.ssmMinBias,
			.shadowMaxBias = settings.ssmMaxBias
		};

		// Lighting UBO data
		const auto& pointLights = scene.getPointLights();

		g_LightingUBOData.directionLight = scene.getSunLight();
		g_LightingUBOData.numPointLights = static_cast<uint32_t>(pointLights.size());
		
		for (size_t i = 0; i < pointLights.size(); ++i) {
			g_LightingUBOData.pointLights[i] = *pointLights[i];
		}

		int i = sizeof(DirectionLight);
		std::memcpy(g_LightingUBOs[device.getBufferIndex()].mappedData, &g_LightingUBOData, sizeof(g_LightingUBOData));

		// Drawing
		device.bindPipeline(g_Pipeline, cmdList);
		device.bindResource(perFrameUBO, "g_PerFrameData", g_Pipeline, cmdList);
		device.bindResource(g_LightingUBOs[device.getBufferIndex()], "g_LightingUBO", g_Pipeline, cmdList);

		device.pushConstants(&pushConstant, sizeof(pushConstant), cmdList);
		device.draw(3, 0, cmdList); // fullscreen triangle
	}

}