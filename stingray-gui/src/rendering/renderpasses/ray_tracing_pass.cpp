#include "ray_tracing_pass.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace sr::rtpass {
	static bool g_Initialized = false;

	struct GeometryInfo { // NOTE: Per BLAS
		uint32_t vertexBufferIndex = 0;
		uint32_t indexBufferIndex = 0;
	};

	struct MaterialInfo {
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float roughness = 1.0f;
	};

	Shader m_RayTracingShaderLibrary = {};
	Buffer m_RayGenShaderTable = {};
	Buffer m_MissShaderTable = {};
	Buffer m_HitShaderTable = {};
	Buffer m_InstanceBuffer = {};

	Buffer m_GeometryInfoBuffer = {};
	std::vector<GeometryInfo> m_GeometryInfoData = {};
	Buffer m_MaterialInfoBuffer = {};
	std::vector<MaterialInfo> m_MaterialInfoData = {};

	std::vector<std::unique_ptr<RayTracingAS>> m_RayTracingBLASes = {};
	RayTracingAS m_RayTracingTLAS = {};
	RayTracingPipeline m_RayTracingPipeline = {};

	static void createBLASes(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		for (auto& entity : entities) {
			// TODO: Add loop for sub-meshes per model
			m_RayTracingBLASes.push_back(std::make_unique<RayTracingAS>());
			RayTracingAS& blas = *m_RayTracingBLASes.back();
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
				.instanceBuffer = &m_InstanceBuffer,
				.numInstances = static_cast<uint32_t>(m_RayTracingBLASes.size()) // TODO: We assume that the number of instances is the same as as number of BLASes
			}
		};

		device.createRayTracingAS(rtTLASInfo, m_RayTracingTLAS);
	}

	static void writeTLASInstances(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		for (uint32_t i = 0; i < m_RayTracingBLASes.size(); ++i) {
			RayTracingTLAS::Instance instance = {
				.instanceID = i,
				.instanceMask = 1,
				.instanceContributionHitGroupIndex = 0, // TODO: Check out what this really does
				.blasResource = m_RayTracingBLASes[i].get()
			};

			// Update the transform data
			Entity* entity = entities[i];
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity->scale);
			glm::mat4 rotation = quatToMat4(entity->orientation);
			glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
			glm::mat4 transformation = glm::transpose(translation * rotation * scale); // convert to row-major representation

			std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

			void* dataSection = (uint8_t*)m_InstanceBuffer.mappedData + i * m_InstanceBuffer.info.stride;
			device.writeTLASInstance(instance, dataSection);
		}
	}

	static void initialize(GraphicsDevice& device, const std::vector<Entity*>& entities) {
		createBLASes(device, entities);

		// Create ray tracing instance buffer
		const uint32_t numBLASes = static_cast<uint32_t>(m_RayTracingBLASes.size());
		device.createRayTracingInstanceBuffer(m_InstanceBuffer, numBLASes);

		createTLAS(device);
		writeTLASInstances(device, entities);

		// Ray-tracing shader library
		device.createShader(ShaderStage::LIBRARY, L"assets/shaders/raytracing.hlsl", m_RayTracingShaderLibrary);

		// Pipeline
		const RayTracingPipelineInfo rtPipelineInfo = {
			.shaderLibraries = {
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::RAYGENERATION, &m_RayTracingShaderLibrary, "MyRaygenShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::CLOSESTHIT, &m_RayTracingShaderLibrary, "MyClosestHitShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::MISS, &m_RayTracingShaderLibrary, "MyMissShader" },
			},
			.hitGroups = {
				RayTracingShaderHitGroup {.type = RayTracingShaderHitGroup::Type::TRIANGLES, .name = "MyHitGroup" }
			}
		};

		device.createRayTracingPipeline(rtPipelineInfo, m_RayTracingPipeline);

		// Shader tables
		device.createShaderTable(m_RayTracingPipeline, m_RayGenShaderTable, "MyRaygenShader");
		device.createShaderTable(m_RayTracingPipeline, m_MissShaderTable, "MyMissShader");
		device.createShaderTable(m_RayTracingPipeline, m_HitShaderTable, "MyHitGroup");

		// Create geometry info buffer
		const BufferInfo geometryInfoBufferInfo = {
			.size = sizeof(GeometryInfo) * m_RayTracingBLASes.size(),
			.stride = sizeof(GeometryInfo),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		// This is really ugly
		m_GeometryInfoData.reserve(m_RayTracingBLASes.size());

		for (auto& blas : m_RayTracingBLASes) {
			const GeometryInfo geometryInfo = {
				.vertexBufferIndex = device.getDescriptorIndex(*blas->info.blas.geometries[0].triangles.vertexBuffer),
				.indexBufferIndex = device.getDescriptorIndex(*blas->info.blas.geometries[0].triangles.indexBuffer)
			};

			m_GeometryInfoData.push_back(geometryInfo);
		}

		device.createBuffer(geometryInfoBufferInfo, m_GeometryInfoBuffer, m_GeometryInfoData.data());

		// Create material info buffer
		const BufferInfo materialInfoBufferInfo{
			.size = sizeof(MaterialInfo) * entities.size(),
			.stride = sizeof(MaterialInfo),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		m_MaterialInfoData.reserve(entities.size());
		for (auto& entity : entities) {
			const MaterialInfo materialInfo = {
				.color = entity->color,
				.roughness = entity->roughness
			};

			m_MaterialInfoData.push_back(materialInfo);
		}

		device.createBuffer(materialInfoBufferInfo, m_MaterialInfoBuffer, m_MaterialInfoData.data());
	}

	void onExecute(RenderGraph& graph, GraphicsDevice& device, const CommandList& commandList, const Buffer& perFrameUBO, const std::vector<Entity*>& entities) {
		if (!g_Initialized) {
			initialize(device, entities);
			g_Initialized = true;
		}

		static bool builtAS = false;
		if (!builtAS) {
			for (auto& blas : m_RayTracingBLASes) {
				device.buildRayTracingAS(*blas, nullptr, commandList);

				const GPUBarrier barrierBLAS = { .type = GPUBarrier::Type::UAV, .uav = { blas.get() } };
				device.barrier(barrierBLAS, commandList);
			}

			device.buildRayTracingAS(m_RayTracingTLAS, nullptr, commandList);
			builtAS = true;
		}

		auto rtOutputAttachment = graph.getAttachment("RTOutput");
		device.bindRayTracingPipeline(m_RayTracingPipeline, rtOutputAttachment->texture, commandList);
		device.bindRayTracingAS(m_RayTracingTLAS, "Scene", m_RayTracingPipeline, commandList);
		device.bindRayTracingConstantBuffer(perFrameUBO, "g_PerFrameData", m_RayTracingPipeline, commandList);
		device.bindRayTracingStructuredBuffer(m_GeometryInfoBuffer, "g_GeometryInfo", m_RayTracingPipeline, commandList);
		device.bindRayTracingStructuredBuffer(m_MaterialInfoBuffer, "g_MaterialInfo", m_RayTracingPipeline, commandList);

		const DispatchRaysInfo dispatchRaysInfo = {
			.rayGenTable = &m_RayGenShaderTable,
			.missTable = &m_MissShaderTable,
			.hitGroupTable = &m_HitShaderTable,
			.width = rtOutputAttachment->info.width,
			.height = rtOutputAttachment->info.height,
			.depth = 1
		};

		device.dispatchRays(dispatchRaysInfo, commandList);
	}

}