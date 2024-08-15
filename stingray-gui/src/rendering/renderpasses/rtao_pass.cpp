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
	static RTPipeline g_RayTracingPipeline = {};
	static RayTracingAS g_RayTracingTLAS = {};
	static Buffer g_RayGenShaderTable = {};
	static Buffer g_MissShaderTable = {};
	static Buffer g_HitShaderTable = {};

	static Buffer g_InstanceBuffer = {};
	static std::vector<RayTracingTLAS::Instance> g_TLASInstances = {};

	static Buffer g_GeometryInfoBuffer = {};
	static std::vector<GeometryInfo> g_GeometryInfoData = {};
	static Buffer g_MaterialInfoBuffer = {};
	static std::vector<MaterialInfo> g_MaterialInfoData = {};
	static std::vector<std::unique_ptr<RayTracingAS>> g_RayTracingBLASes = {};
	static bool g_Initialized = false;

	static void createBLASes(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		uint32_t numBLASes = 0;

		// TODO: For now we create one BLAS per mesh, which is not always ideal
		for (const auto& entity : entities) {
			for (const auto& mesh : entity->model->meshes) {
				++numBLASes;
			}
		}

		g_RayTracingBLASes.reserve(static_cast<size_t>(numBLASes));
		g_TLASInstances.reserve(static_cast<size_t>(numBLASes));
		device.createRTInstanceBuffer(g_InstanceBuffer, numBLASes);

		for (auto& entity : entities) {
			// TODO: Add loop for sub-meshes per model
			const Model* model = entity->model;

			for (size_t i = 0; i < model->meshes.size(); ++i) {
				const Mesh& mesh = model->meshes[i];

				g_RayTracingBLASes.push_back(std::make_unique<RayTracingAS>());
				RayTracingAS& blas = *g_RayTracingBLASes.back();

				blas.info.type = RayTracingASType::BLAS;
				blas.info.blas.geometries.resize(1);

				// TODO: For now we create one BLAS per mesh, which might not be ideal
				RayTracingBLASGeometry& geometry = blas.info.blas.geometries[0];
				geometry.type = RayTracingBLASGeometry::Type::TRIANGLES;

				auto& triangles = geometry.triangles;
				triangles.vertexBuffer = &model->vertexBuffer;
				triangles.indexBuffer = &model->indexBuffer;
				triangles.vertexFormat = Format::R32G32B32_FLOAT;
				triangles.vertexCount = static_cast<uint32_t>(mesh.numVertices);
				triangles.vertexStride = sizeof(ModelVertex);
				triangles.vertexByteOffset = sizeof(ModelVertex) * mesh.baseVertex;
				triangles.indexCount = static_cast<uint32_t>(mesh.numIndices);
				triangles.indexOffset = mesh.baseIndex;

				device.createRTAS(blas.info, blas);

				// Store the TLAS instance data (will be written to the actual TLAS later)
				RayTracingTLAS::Instance instance = {
					.instanceID = static_cast<uint32_t>(g_TLASInstances.size()),
					.instanceMask = 1,
					.instanceContributionHitGroupIndex = 0, // TODO: Check out what this really does
					.blasResource = &blas
				};

				// Update the transform data
				glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity->scale);
				glm::mat4 rotation = quatToMat4(entity->orientation);
				glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
				glm::mat4 transformation = glm::transpose(translation * rotation * scale); // convert to row-major representation

				std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

				g_TLASInstances.push_back(instance);
			}
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

		device.createRTAS(rtTLASInfo, g_RayTracingTLAS);
	}

	static void writeTLASInstances(GraphicsDevice& device) {
		for (uint32_t i = 0; i < g_TLASInstances.size(); ++i) {
			void* dataSection = (uint8_t*)g_InstanceBuffer.mappedData + i * g_InstanceBuffer.info.stride;
			device.writeTLASInstance(g_TLASInstances[i], dataSection);
		}
	}

	static void initialize(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		createBLASes(device, entities); // NOTE: We also create instance buffer here
		createTLAS(device);
		writeTLASInstances(device);

		// Ray-tracing shader library
		device.createShader(ShaderStage::LIBRARY, "assets/shaders/rtao.hlsl", g_RayTracingShaderLibrary);

		// Pipeline
		const RTPipelineInfo rtPipelineInfo = {
			.shaderLibraries = {
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::RAYGENERATION, &g_RayTracingShaderLibrary, "MyRaygenShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::CLOSESTHIT, &g_RayTracingShaderLibrary, "MyClosestHitShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::MISS, &g_RayTracingShaderLibrary, "MyMissShader" },
			},
			.hitGroups = {
				RayTracingShaderHitGroup {.type = RayTracingShaderHitGroup::Type::TRIANGLES, .name = "MyHitGroup" }
			},
			.payloadSize = 4 * sizeof(float) // payload: color
		};

		device.createRTPipeline(rtPipelineInfo, g_RayTracingPipeline);

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
				.color = glm::vec4(entity->color, 1.0f),
				.roughness = entity->roughness
			};

			g_MaterialInfoData.push_back(materialInfo);
		}

		device.createBuffer(materialInfoBufferInfo, g_MaterialInfoBuffer, g_MaterialInfoData.data());
	}

	void onExecute(PassExecuteInfo& executeInfo, const Buffer& perFrameUBO, const Scene& scene) {
		RenderGraph& graph = *executeInfo.renderGraph;
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;
		
		if (!g_Initialized) {
			initialize(device, scene.getEntities());
			g_Initialized = true;
		}

		static bool builtAS = false;
		if (!builtAS) {
			for (auto& blas : g_RayTracingBLASes) {
				device.buildRTAS(*blas, nullptr, cmdList);

				const GPUBarrier barrierBLAS = { .type = GPUBarrier::Type::UAV, .uav = { blas.get() } };
				device.barrier(barrierBLAS, cmdList);
			}

			device.buildRTAS(g_RayTracingTLAS, nullptr, cmdList);
			builtAS = true;
		}

		// Update transform data
		const auto& entities = scene.getEntities();

		// TODO: Only update if changed
		size_t instanceIndex = 0;

		for (const auto& entity : entities) {
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity->scale);
			glm::mat4 rotation = quatToMat4(entity->orientation);
			glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
			glm::mat4 transformation = glm::transpose(translation * rotation * scale); // convert to row-major representation

			for (const auto& mesh : entity->model->meshes) {
				RayTracingTLAS::Instance& instance = g_TLASInstances[instanceIndex];
				std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

				++instanceIndex;
			}
		}

		writeTLASInstances(device);
		// TLAS refitting
		device.buildRTAS(g_RayTracingTLAS, &g_RayTracingTLAS, cmdList);

		auto rtOutputAttachment = graph.getAttachment("AmbientOcclusion");
		auto positionAttachment = graph.getAttachment("Position");
		auto normalAttachment = graph.getAttachment("Normal");

		device.bindRTPipeline(g_RayTracingPipeline, rtOutputAttachment->texture, cmdList);
		device.bindRTResource(g_RayTracingTLAS, "Scene", g_RayTracingPipeline, cmdList);
		//device.bindRayTracingResource(perFrameUBO, "g_PerFrameData", g_RayTracingPipeline, cmdList);
		device.bindRTResource(g_GeometryInfoBuffer, "g_GeometryInfo", g_RayTracingPipeline, cmdList);
		device.bindRTResource(g_MaterialInfoBuffer, "g_MaterialInfo", g_RayTracingPipeline, cmdList);

		const PushConstant pushConstant = {
			.gBufferPositionIndex = device.getDescriptorIndex(positionAttachment->texture),
			.gBufferNormalIndex = device.getDescriptorIndex(normalAttachment->texture),
			.frameCount = (uint32_t)device.getFrameCount()
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

	void destroy() {

	}

}