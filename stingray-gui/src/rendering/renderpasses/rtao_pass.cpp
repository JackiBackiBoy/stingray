#include "rtao_pass.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace sr::rtaopass {
	struct GeometryInfo { // NOTE: Per BLAS
		uint32_t vertexBufferIndex = 0;
		uint32_t indexBufferIndex = 0;
	};

	struct MaterialInfo {
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float roughness = 1.0f;
	};

	struct PushConstant {
		uint32_t gBufferPositionIndex;
		uint32_t gBufferNormalIndex;
		uint32_t frameCount;
	};

	static Shader g_RayTracingShaderLibrary = {};
	static Buffer g_RayGenShaderTable = {};
	static Buffer g_MissShaderTable = {};
	static Buffer g_HitShaderTable = {};
	static Buffer g_InstanceBuffer = {};

	static Buffer g_GeometryInfoBuffer = {};
	static std::vector<GeometryInfo> g_GeometryInfoData = {};
	static Buffer g_MaterialInfoBuffer = {};
	static std::vector<MaterialInfo> g_MaterialInfoData = {};

	static std::vector<std::unique_ptr<RayTracingAS>> g_RayTracingBLASes = {};
	static RayTracingAS g_RayTracingTLAS = {};
	static RayTracingPipeline g_RayTracingPipeline = {};
	static bool g_Initialized = false;

	static void createBLASes(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		for (auto& entity : entities) {
			// TODO: Add loop for sub-meshes per model
			g_RayTracingBLASes.push_back(std::make_unique<RayTracingAS>());
			RayTracingAS& blas = *g_RayTracingBLASes.back();
			const Model* model = entity->model;

			blas.info.type = RayTracingASType::BLAS;
			blas.info.blas.geometries.resize(model->meshes.size());

			for (auto& geometry : blas.info.blas.geometries) {
				geometry.type = RayTracingBLASGeometry::Type::TRIANGLES;

				auto& triangles = geometry.triangles;
				triangles.vertexBuffer = &model->vertexBuffer;
				triangles.indexBuffer = &model->indexBuffer;
				triangles.vertexFormat = Format::R32G32B32_FLOAT;
				triangles.vertexCount = static_cast<uint32_t>(model->vertices.size());
				triangles.vertexStride = sizeof(ModelVertex);
				triangles.indexCount = static_cast<uint32_t>(model->indices.size());
			}

			device.createRayTracingAS(blas.info, blas);
		}
	}

	static void createTLAS(GraphicsDevice& device) {
		const RayTracingASInfo rtTLASInfo = {
			.type = RayTracingASType::TLAS,
			.tlas = {
				.instanceBuffer = &g_InstanceBuffer,
				.numInstances = static_cast<uint32_t>(g_RayTracingBLASes.size()) // TODO: We assume that the number of instances is the same as as number of BLASes
			}
		};

		device.createRayTracingAS(rtTLASInfo, g_RayTracingTLAS);
	}

	static void writeTLASInstances(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		for (uint32_t i = 0; i < g_RayTracingBLASes.size(); ++i) {
			RayTracingTLAS::Instance instance = {
				.instanceID = i,
				.instanceMask = 1,
				.instanceContributionHitGroupIndex = 0, // TODO: Check out what this really does
				.blasResource = g_RayTracingBLASes[i].get()
			};

			// Update the transform data
			Entity* entity = entities[i];
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity->scale);
			glm::mat4 rotation = quatToMat4(entity->orientation);
			glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
			glm::mat4 transformation = glm::transpose(translation * rotation * scale); // convert to row-major representation

			std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

			void* dataSection = (uint8_t*)g_InstanceBuffer.mappedData + i * g_InstanceBuffer.info.stride;
			device.writeTLASInstance(instance, dataSection);
		}
	}

	static void initialize(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		createBLASes(device, entities);

		// Create ray tracing instance buffer
		const uint32_t numBLASes = static_cast<uint32_t>(g_RayTracingBLASes.size());
		device.createRayTracingInstanceBuffer(g_InstanceBuffer, numBLASes);

		createTLAS(device);
		writeTLASInstances(device, entities);

		// Ray-tracing shader library
		device.createShader(ShaderStage::LIBRARY, L"assets/shaders/rtao.hlsl", g_RayTracingShaderLibrary);

		// Pipeline
		const RayTracingPipelineInfo rtPipelineInfo = {
			.shaderLibraries = {
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::RAYGENERATION, &g_RayTracingShaderLibrary, "MyRaygenShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::CLOSESTHIT, &g_RayTracingShaderLibrary, "MyClosestHitShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::MISS, &g_RayTracingShaderLibrary, "MyMissShader" },
			},
			.hitGroups = {
				RayTracingShaderHitGroup {.type = RayTracingShaderHitGroup::Type::TRIANGLES, .name = "MyHitGroup" }
			}
		};

		device.createRayTracingPipeline(rtPipelineInfo, g_RayTracingPipeline);

		// Shader tables
		device.createShaderTable(g_RayTracingPipeline, g_RayGenShaderTable, "MyRaygenShader");
		device.createShaderTable(g_RayTracingPipeline, g_MissShaderTable, "MyMissShader");
		device.createShaderTable(g_RayTracingPipeline, g_HitShaderTable, "MyHitGroup");

		// Create geometry info buffer
		const BufferInfo geometryInfoBufferInfo = {
			.size = sizeof(GeometryInfo) * g_RayTracingBLASes.size(),
			.stride = sizeof(GeometryInfo),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		// This is really ugly
		g_GeometryInfoData.reserve(g_RayTracingBLASes.size());

		for (auto& blas : g_RayTracingBLASes) {
			const GeometryInfo geometryInfo = {
				.vertexBufferIndex = device.getDescriptorIndex(*blas->info.blas.geometries[0].triangles.vertexBuffer),
				.indexBufferIndex = device.getDescriptorIndex(*blas->info.blas.geometries[0].triangles.indexBuffer)
			};

			g_GeometryInfoData.push_back(geometryInfo);
		}

		device.createBuffer(geometryInfoBufferInfo, g_GeometryInfoBuffer, g_GeometryInfoData.data());

		// Create material info buffer
		const BufferInfo materialInfoBufferInfo{
			.size = sizeof(MaterialInfo) * entities.size(),
			.stride = sizeof(MaterialInfo),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		g_MaterialInfoData.reserve(entities.size());
		for (auto& entity : entities) {
			const MaterialInfo materialInfo = {
				.color = entity->color,
				.roughness = entity->roughness
			};

			g_MaterialInfoData.push_back(materialInfo);
		}

		device.createBuffer(materialInfoBufferInfo, g_MaterialInfoBuffer, g_MaterialInfoData.data());
	}

	void onExecute(PassExecuteInfo& executeInfo, const Buffer& perFrameUBO, const std::vector<Entity*>& entities) {
		RenderGraph& graph = *executeInfo.renderGraph;
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;
		
		if (!g_Initialized) {
			initialize(device, entities);
			g_Initialized = true;
		}

		static bool builtAS = false;
		if (!builtAS) {
			for (auto& blas : g_RayTracingBLASes) {
				device.buildRayTracingAS(*blas, nullptr, cmdList);

				const GPUBarrier barrierBLAS = { .type = GPUBarrier::Type::UAV, .uav = { blas.get() } };
				device.barrier(barrierBLAS, cmdList);
			}

			device.buildRayTracingAS(g_RayTracingTLAS, nullptr, cmdList);
			builtAS = true;
		}

		auto rtOutputAttachment = graph.getAttachment("AmbientOcclusion");
		auto positionAttachment = graph.getAttachment("Position");
		auto normalAttachment = graph.getAttachment("Normal");

		device.bindRayTracingPipeline(g_RayTracingPipeline, rtOutputAttachment->texture, cmdList);
		device.bindRayTracingResource(g_RayTracingTLAS, "Scene", g_RayTracingPipeline, cmdList);
		//device.bindRayTracingResource(perFrameUBO, "g_PerFrameData", g_RayTracingPipeline, cmdList);
		device.bindRayTracingResource(g_GeometryInfoBuffer, "g_GeometryInfo", g_RayTracingPipeline, cmdList);
		device.bindRayTracingResource(g_MaterialInfoBuffer, "g_MaterialInfo", g_RayTracingPipeline, cmdList);

		const PushConstant pushConstant = {
			.gBufferPositionIndex = device.getDescriptorIndex(positionAttachment->texture),
			.gBufferNormalIndex = device.getDescriptorIndex(normalAttachment->texture),
			.frameCount = (uint32_t)device.getFrameCount() % 32000
		};

		device.pushConstantsCompute(&pushConstant, sizeof(pushConstant), cmdList);

		const DispatchRaysInfo dispatchRaysInfo = {
			.rayGenTable = &g_RayGenShaderTable,
			.missTable = &g_MissShaderTable,
			.hitGroupTable = &g_HitShaderTable,
			.width = rtOutputAttachment->info.width,
			.height = rtOutputAttachment->info.height,
			.depth = 1
		};

		device.dispatchRays(dispatchRaysInfo, cmdList);
	}
}