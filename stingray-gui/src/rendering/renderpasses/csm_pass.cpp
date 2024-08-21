#include "csm_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace sr::csmpass {
	struct PushConstant {
		glm::mat4 modelMatrix = { 1.0f };
		glm::mat4 lightSpaceMatrix = { 1.0f };
	};

	static Shader g_VertexShader = {};
	static Pipeline g_Pipeline = {};
	static PushConstant g_PushConstant = {};
	static bool g_Initialized = false;

	static void initialize(GraphicsDevice& device) {
		device.createShader(ShaderStage::VERTEX, "assets/shaders/simple_shadows.vs.hlsl", g_VertexShader);

		// Pipeline
		const PipelineInfo pipelineInfo = {
			.vertexShader = &g_VertexShader,
			.fragmentShader = nullptr, // no pixel shader for shadow pass
			.rasterizerState = {
				.cullMode = CullMode::BACK,
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
	}

	enum class CascadePartitioning : uint8_t {
		LINEAR,
		QUADRATIC
	};

	void generateCascadeLevels(float* cascadeLevels, int numLevels, float zNear, float zFar, CascadePartitioning partion) {
		assert(cascadeLevels != nullptr && numLevels > 0);

		const float zRange = zFar - zNear;
		// TODO: Handle case where numLevels = 1

		if (partion == CascadePartitioning::QUADRATIC) {
			const int powTwoSum = (1 << (numLevels + 1)) - 1;  // Sum of 1 + 2 + 4 + 8 + ...
			const float totalLevelErrorPercentage = 2.0f - static_cast<float>(powTwoSum) / (1 << numLevels);
			const float perLevelError = (totalLevelErrorPercentage * zRange) / (numLevels - 1);

			// Distribute the error evenly per cascade level
			for (int i = 0; i < numLevels; ++i) {
				cascadeLevels[i] = zNear + (1.0f / (1 << (numLevels - 1 - i))) * zRange;

				if (i < numLevels - 1) {
					cascadeLevels[i] -= perLevelError;
				}
			}
		}
	}

	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, Scene& scene) {
		GraphicsDevice& device = *executeInfo.device;
		RenderGraph& renderGraph = *executeInfo.renderGraph;
		const CommandList& cmdList = *executeInfo.cmdList;

		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		// Update shadow UBO data
		//g_ShadowUBOData.lightSpaceMatrix = scene.getSunLSMatrix();
		//std::memcpy(g_ShadowUBOs[device.getBufferIndex()].mappedData, &g_ShadowUBOData, sizeof(g_ShadowUBOData));

		// Rendering
		device.bindPipeline(g_Pipeline, cmdList);

		const auto& entities = scene.getEntities();

		// Re-render scene for each shadow cascade level
		const int numCascades = 4; // TODO: Retrieve from Settings struct instead
		const int shadowMapDim = renderGraph.getAttachment("ShadowMap")->info.width;
		const int cascadesPerRow = (numCascades >> 2) + 1;
		const int cascadeDim = shadowMapDim / cascadesPerRow;
		const float fCascadeDim = static_cast<float>(cascadeDim);

		const Camera& camera = *executeInfo.frameInfo->camera;
		Camera::Frustum frustum = camera.getFrustum();
		DirectionLight& sunLight = scene.getSunLight();

		const float zNear = camera.getZNear();
		const float zFar = camera.getZFar();
		
		generateCascadeLevels(
			sunLight.cascadeDistances,
			numCascades,
			zNear,
			zFar,
			CascadePartitioning::QUADRATIC
		);

		float lastCascadeDistance = camera.getZNear();

		for (int c = 0; c < numCascades; ++c) {
			const Viewport viewport = {
					.topLeftX = fCascadeDim * (c % cascadesPerRow),
					.topLeftY = fCascadeDim * (c / cascadesPerRow),
					.width = fCascadeDim,
					.height = fCascadeDim,
					.minDepth = 0.0f,
					.maxDepth = 1.0f
			};

			device.bindViewport(viewport, cmdList);

			// Compute tight projection for the current cascade
			const glm::mat4 cascadeProjection = glm::perspective(
				camera.getVerticalFOV(),
				camera.getAspectRatio(),
				lastCascadeDistance,
				sunLight.cascadeDistances[c]
			);

			lastCascadeDistance = sunLight.cascadeDistances[c];

			const Camera::Frustum cascadeFrustum = Camera::Frustum::getFrustum(
				cascadeProjection,
				camera.getViewMatrix()
			);

			glm::vec3 cascadeFrustumCenter = glm::vec3(0.0f);
			for (size_t i = 0; i < 8; ++i) {
				cascadeFrustumCenter += cascadeFrustum.corners[i];
			}
			cascadeFrustumCenter /= 8;

			const glm::mat4 lightView = glm::lookAt(
				cascadeFrustumCenter + scene.getSunDirection(),
				cascadeFrustumCenter,
				{ 0.0f, 1.0f, 0.0f }
			);

			const glm::vec4 lightViewCascadeCorners[8] = {
				lightView * glm::vec4(cascadeFrustum.corners[0], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[1], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[2], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[3], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[4], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[5], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[6], 1.0f),
				lightView * glm::vec4(cascadeFrustum.corners[7], 1.0f)
			};
			
			glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
			glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

			for (size_t i = 0; i < 8; ++i) {
				const glm::vec4& corner = lightViewCascadeCorners[i];

				if (corner.x < min.x) { min.x = corner.x; }
				if (corner.y < min.y) { min.y = corner.y; }
				if (corner.z < min.z) { min.z = corner.z; }

				if (corner.x > max.x) { max.x = corner.x; }
				if (corner.y > max.y) { max.y = corner.y; }
				if (corner.z > max.z) { max.z = corner.z; }
			}

			const glm::mat4 lightProjection = glm::ortho(min.x, max.x, min.y, max.y, min.z, max.z);
			const glm::mat4 lightSpaceMatrix = lightProjection * lightView;
			
			scene.getSunLight().cascadeProjections[c] = lightSpaceMatrix;
			scene.getSunLight().numCascades = numCascades;

			for (const auto& entity : entities) {
				device.bindVertexBuffer(entity->model->vertexBuffer, cmdList);
				device.bindIndexBuffer(entity->model->indexBuffer, cmdList);

				g_PushConstant = {}; // reset push constant data
				g_PushConstant.modelMatrix =
					glm::translate(g_PushConstant.modelMatrix, entity->position) *
					sr::quatToMat4(entity->orientation) *
					glm::scale(g_PushConstant.modelMatrix, entity->scale);
				g_PushConstant.lightSpaceMatrix = lightSpaceMatrix;

				device.pushConstants(&g_PushConstant, sizeof(g_PushConstant), cmdList);

				for (const auto& mesh : entity->model->meshes) {
					device.drawIndexed(mesh.numIndices, mesh.baseIndex, mesh.baseVertex, cmdList);
				}
			}
		}
	}

}