#include "application.hpp"

#if defined(SR_WINDOWS)
#include "../rendering/dx12/device_dx12.hpp"
#include "../input/rawinput.hpp"
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

namespace sr {

	Application::Application(const std::string& title) : m_Title(title) {
		m_Width = 1280;
		m_Height = 720;

		const WindowInfo windowInfo = {
			.title = title,
			.width = m_Width,
			.height = m_Height,
			.flags = WindowFlag::HCENTER | WindowFlag::VCENTER | WindowFlag::SIZE_IS_CLIENT_AREA
		};

		m_Window = std::make_unique<Window>(windowInfo);
		
	#if defined(SR_WINDOWS)
		m_Device = std::make_unique<GraphicsDevice_DX12>(m_Width, m_Height, (HWND)m_Window->getHandle());
	#endif
	}

	Application::~Application() {

	}

	void Application::run() {
		initialize();
		createEntities();
		createRTObjects();

		onInitialize();

		MSG msg = {};
		while (msg.message != WM_QUIT) {
			static auto lastTime = std::chrono::high_resolution_clock::now();
			const auto currentTime = std::chrono::high_resolution_clock::now();

			if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
				lastTime = currentTime;

				updateRT(dt);
				renderRT(dt);
			}
		}

		m_Device->waitForGPU();
	}

	Entity* Application::addEntity(const std::string& name) {
		m_Entities.push_back(std::make_unique<Entity>());

		return m_Entities.back().get();
	}

	void Application::initialize() {
		sr::rawinput::initialize();

		// Render objects
		const SwapChainInfo swapChainInfo = {
			.width = static_cast<uint32_t>(m_Width),
			.height = static_cast<uint32_t>(m_Height),
			.bufferCount = 3,
			.format = Format::R8G8B8A8_UNORM,
			.fullscreen = false,
			.vsync = true
		};

		// TODO: Only works on windows, we'll fix this later
		m_Device->createSwapChain(swapChainInfo, m_SwapChain, (HWND)m_Window->getHandle());
	}

	void Application::createEntities() {
		m_CubeModel = sr::assetmanager::loadFromFile("assets/models/SheenCloth.gltf", *m_Device);
		m_PlaneModel = sr::assetmanager::loadFromFile("assets/models/plane.gltf", *m_Device);

		// Cornell box
		const float cornellScale = 4.0f;

		auto cornellFloor = addEntity("Floor");
		cornellFloor->scale = glm::vec3(cornellScale);
		cornellFloor->model = &m_PlaneModel.getModel();

		auto cornellTop = addEntity("Top");
		cornellTop->position.y = 2 * cornellScale;
		cornellTop->scale = glm::vec3(cornellScale);
		cornellTop->model = &m_PlaneModel.getModel();

		auto cornellWallLeft = addEntity("WallLeft");
		cornellWallLeft->position.x = -cornellScale;
		cornellWallLeft->position.y = cornellScale;
		cornellWallLeft->scale = glm::vec3(cornellScale);
		cornellWallLeft->model = &m_PlaneModel.getModel();
		cornellWallLeft->orientation = quatFromAxisAngle({ 0.0f, 0.0f, 1.0f }, glm::radians(-90.0f));
		cornellWallLeft->color = { 1.0f, 0.0f, 0.0f, 1.0f };

		auto cornellWallRight = addEntity("WallRight");
		cornellWallRight->position.x = cornellScale;
		cornellWallRight->position.y = cornellScale;
		cornellWallRight->scale = glm::vec3(cornellScale);
		cornellWallRight->model = &m_PlaneModel.getModel();
		cornellWallRight->orientation = quatFromAxisAngle({ 0.0f, 0.0f, 1.0f }, glm::radians(-90.0f));
		cornellWallRight->color = { 0.0f, 1.0f, 0.0f, 1.0f };

		auto cornellWallBack = addEntity("WallBack");
		cornellWallBack->position.y = cornellScale;
		cornellWallBack->position.z = cornellScale;
		cornellWallBack->scale = glm::vec3(cornellScale);
		cornellWallBack->model = &m_PlaneModel.getModel();
		cornellWallBack->orientation = quatFromAxisAngle({ 1.0f, 0.0f, 0.0f }, glm::radians(-90.0f));

		m_SphereEntity = addEntity("Sphere");
		m_SphereEntity->position.y = 0;
		m_SphereEntity->scale = glm::vec3(50.0f);
		m_SphereEntity->model = &m_CubeModel.getModel();
	}

	void Application::createRTObjects() {
		createBLASes();

		// Create ray tracing instance buffer
		const uint32_t numBLASes = static_cast<uint32_t>(m_RayTracingBLASes.size());
		m_Device->createRayTracingInstanceBuffer(m_InstanceBuffer, numBLASes);

		createTLAS();

		// Write TLAS instances
		writeTLASInstances();

		// Ray-tracing shader library
		m_Device->createShader(ShaderStage::LIBRARY, L"assets/shaders/raytracing.hlsl", m_RayTracingShaderLibrary);

		// Pipeline
		const RayTracingPipelineInfo rtPipelineInfo = {
			.shaderLibraries = {
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::RAYGENERATION, &m_RayTracingShaderLibrary, "MyRaygenShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::CLOSESTHIT, &m_RayTracingShaderLibrary, "MyClosestHitShader" },
				RayTracingShaderLibrary { RayTracingShaderLibrary::Type::MISS, &m_RayTracingShaderLibrary, "MyMissShader" },
			},
			.hitGroups = {
				RayTracingShaderHitGroup { .type = RayTracingShaderHitGroup::Type::TRIANGLES, .name = "MyHitGroup" }
			}
		};

		m_Device->createRayTracingPipeline(rtPipelineInfo, m_RayTracingPipeline);

		// Shader tables
		m_Device->createShaderTable(m_RayTracingPipeline, m_RayGenShaderTable, "MyRaygenShader");
		m_Device->createShaderTable(m_RayTracingPipeline, m_MissShaderTable, "MyMissShader");
		m_Device->createShaderTable(m_RayTracingPipeline, m_HitShaderTable, "MyHitGroup");

		// Output resource (UAV texture)
		const TextureInfo rayTracingOutputInfo{
			.width = static_cast<uint32_t>(m_Width),
			.height = static_cast<uint32_t>(m_Height),
			.format = Format::R8G8B8A8_UNORM,
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::UNORDERED_ACCESS
		};
		m_Device->createTexture(rayTracingOutputInfo, m_RayTracingOutput, nullptr);

		// Per-frame constant buffers (one for each frame in flight)
		const BufferInfo perFrameUBOInfo = {
			.size = sizeof(PerFrameUBO),
			.stride = sizeof(PerFrameUBO),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::UNIFORM_BUFFER,
			.persistentMap = true
		};

		for (size_t f = 0; f < GraphicsDevice::NUM_BUFFERS; ++f) {
			m_Device->createBuffer(perFrameUBOInfo, m_PerFrameUBOs[f], &m_PerFrameUBOData);
		}

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
				.vertexBufferIndex = m_Device->getDescriptorIndex(*blas->info.blas.geometries[0].triangles.vertexBuffer),
				.indexBufferIndex = m_Device->getDescriptorIndex(*blas->info.blas.geometries[0].triangles.indexBuffer)
			};

			m_GeometryInfoData.push_back(geometryInfo);
		}

		m_Device->createBuffer(geometryInfoBufferInfo, m_GeometryInfoBuffer, m_GeometryInfoData.data());

		// Create material info buffer
		const BufferInfo materialInfoBufferInfo{
			.size = sizeof(MaterialInfo) * m_Entities.size(),
			.stride = sizeof(MaterialInfo),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		m_MaterialInfoData.reserve(m_Entities.size());
		for (auto& entity : m_Entities) {
			const MaterialInfo materialInfo = {
				.color = entity->color,
				.roughness = entity->roughness
			};

			m_MaterialInfoData.push_back(materialInfo);
		}

		m_Device->createBuffer(materialInfoBufferInfo, m_MaterialInfoBuffer, m_MaterialInfoData.data());
	}

	void Application::createBLASes() {
		for (auto& entity : m_Entities) {
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

			m_Device->createRayTracingAS(blas.info, blas);
		}
	}

	void Application::createTLAS() {
		const RayTracingASInfo rtTLASInfo = {
			.type = RayTracingASType::TLAS,
			.tlas = {
				.instanceBuffer = &m_InstanceBuffer,
				.numInstances = static_cast<uint32_t>(m_RayTracingBLASes.size()) // TODO: We assume that the number of instances is the same as as number of BLASes
			}
		};

		m_Device->createRayTracingAS(rtTLASInfo, m_RayTracingTLAS);
	}

	void Application::writeTLASInstances() {
		for (uint32_t i = 0; i < m_RayTracingBLASes.size(); ++i) {
			RayTracingTLAS::Instance instance = {
				.instanceID = i,
				.instanceMask = 1,
				.instanceContributionHitGroupIndex = 0, // TODO: Check out what this really does
				.blasResource = m_RayTracingBLASes[i].get()
			};

			// Update the transform data
			Entity* entity = m_Entities[i].get();
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity->scale);
			glm::mat4 rotation = quatToMat4(entity->orientation);
			glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
			glm::mat4 transformation = glm::transpose(translation * rotation * scale); // convert to row-major representation

			std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

			void* dataSection = (uint8_t*)m_InstanceBuffer.mappedData + i * m_InstanceBuffer.info.stride;
			m_Device->writeTLASInstance(instance, dataSection);
		}
	}

	void Application::updateRT(float dt) {
		sr::rawinput::update();
		sr::rawinput::RawKeyboardState keyboard = {};
		sr::rawinput::RawMouseState mouse = {};

		sr::rawinput::getKeyboardState(keyboard);
		sr::rawinput::getMouseState(mouse);

		// Camera movement
		const float cameraMoveSpeed = 5.0f;

		if (mouse.mouse3) {
			m_Camera.orientation = m_Camera.orientation * quatFromAxisAngle({ 1.0f, 0.0f, 0.0f }, mouse.deltaY * dt); // pitch
			m_Camera.orientation = quatFromAxisAngle({ 0.0f, 1.0f, 0.0f }, mouse.deltaX * dt) * m_Camera.orientation; // yaw
		}

		const glm::vec3 qRight = quatRotateVector(m_Camera.orientation, { 1.0f, 0.0f, 0.0f });
		const glm::vec3 qUp = quatRotateVector(m_Camera.orientation, { 0.0f, 1.0f, 0.0f });
		const glm::vec3 qForward = quatRotateVector(m_Camera.orientation, { 0.0f, 0.0f, 1.0f });

		if (sr::rawinput::isDown(KeyCode::W)) {
			m_Camera.position += qForward * cameraMoveSpeed * dt;
		}
		if (sr::rawinput::isDown(KeyCode::A)) {
			m_Camera.position -= qRight * cameraMoveSpeed * dt;
		}
		if (sr::rawinput::isDown(KeyCode::S)) {
			m_Camera.position -= qForward * cameraMoveSpeed * dt;
		}
		if (sr::rawinput::isDown(KeyCode::D)) {
			m_Camera.position += qRight * cameraMoveSpeed * dt;
		}

		if (sr::rawinput::isDown(KeyCode::Space)) {
			m_Camera.position.y += cameraMoveSpeed * dt;
		}
		if (sr::rawinput::isDown(KeyCode::LeftControl)) {
			m_Camera.position.y -= cameraMoveSpeed * dt;
		}

		m_Camera.viewMatrix = glm::lookAt(m_Camera.position, m_Camera.position + qForward, qUp);

		// Update per-frame data
		m_PerFrameUBOData.cameraPosition = m_Camera.position;

		float aspectRatio = (float)m_Width / m_Height;
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 125.0f);

		m_PerFrameUBOData.invViewProjection = glm::inverse(proj * m_Camera.viewMatrix);
		std::memcpy(m_PerFrameUBOs[m_Device->getBufferIndex()].mappedData, &m_PerFrameUBOData, sizeof(m_PerFrameUBOData));
	}

	void Application::renderRT(float dt) {
		CommandList commandList = m_Device->beginCommandList(QueueType::DIRECT);

		static bool builtAS = false;
		if (!builtAS) {
			for (auto& blas : m_RayTracingBLASes) {
				m_Device->buildRayTracingAS(*blas, nullptr, commandList);

				const GPUBarrier barrierBLAS = { .type = GPUBarrier::Type::UAV, .uav = { blas.get() } };
				m_Device->barrier(barrierBLAS, commandList);
			}

			m_Device->buildRayTracingAS(m_RayTracingTLAS, nullptr, commandList);
			builtAS = true;
		}

		const Viewport viewport = {
			.width = static_cast<float>(m_Width),
			.height = static_cast<float>(m_Height),
		};
		m_Device->bindViewport(viewport, commandList);

		m_Device->beginRTRenderPass(m_SwapChain, commandList);
		m_Device->bindRayTracingPipeline(m_RayTracingPipeline, m_RayTracingOutput, commandList);
		m_Device->bindRayTracingAS(m_RayTracingTLAS, "Scene", m_RayTracingPipeline, commandList);
		m_Device->bindRayTracingConstantBuffer(m_PerFrameUBOs[m_Device->getBufferIndex()], "g_PerFrameData", m_RayTracingPipeline, commandList);
		m_Device->bindRayTracingStructuredBuffer(m_GeometryInfoBuffer, "g_GeometryInfo", m_RayTracingPipeline, commandList);
		m_Device->bindRayTracingStructuredBuffer(m_MaterialInfoBuffer, "g_MaterialInfo", m_RayTracingPipeline, commandList);

		const DispatchRaysInfo dispatchRaysInfo = {
			.rayGenTable = &m_RayGenShaderTable,
			.missTable = &m_MissShaderTable,
			.hitGroupTable = &m_HitShaderTable,
			.width = m_RayTracingOutput.info.width,
			.height = m_RayTracingOutput.info.height,
			.depth = 1
		};

		m_Device->dispatchRays(dispatchRaysInfo, commandList);
		m_Device->copyRTOutputToBackbuffer(m_SwapChain, m_RayTracingOutput, commandList);

		m_Device->submitCommandLists(m_SwapChain);
	}

}