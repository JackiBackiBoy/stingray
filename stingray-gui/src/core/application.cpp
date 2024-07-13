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

		m_Window = std::make_unique<Window>(title, m_Width, m_Height);
		
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
	}

	void Application::createBLASes() {
		// Cube BLAS
		m_RayTracingBLASes.push_back(std::make_unique<RayTracingAS>());

		RayTracingAS& cubeBLAS = *m_RayTracingBLASes.back();
		Model& cubeModel = m_CubeModel.getModel();

		cubeBLAS.info.type = RayTracingASType::BLAS;
		cubeBLAS.info.blas.geometries.resize(cubeModel.meshes.size());

		for (auto& geometry : cubeBLAS.info.blas.geometries) {
			geometry.type = RayTracingBLASGeometry::Type::TRIANGLES;

			auto& triangles = geometry.triangles;
			triangles.vertexBuffer = &cubeModel.vertexBuffer;
			triangles.indexBuffer = &cubeModel.indexBuffer;
			triangles.vertexFormat = Format::R32G32B32_FLOAT;
			triangles.vertexCount = static_cast<uint32_t>(cubeModel.vertices.size());
			triangles.vertexStride = sizeof(ModelVertex);
			triangles.indexCount = static_cast<uint32_t>(cubeModel.indices.size());
		}

		// Plane BLAS
		m_RayTracingBLASes.push_back(std::make_unique<RayTracingAS>());

		RayTracingAS& planeBLAS = *m_RayTracingBLASes.back();
		Model& planeModel = m_PlaneModel.getModel();

		planeBLAS.info.type = RayTracingASType::BLAS;
		planeBLAS.info.blas.geometries.resize(planeModel.meshes.size());

		for (auto& geometry : planeBLAS.info.blas.geometries) {
			geometry.type = RayTracingBLASGeometry::Type::TRIANGLES;

			auto& triangles = geometry.triangles;
			triangles.vertexBuffer = &planeModel.vertexBuffer;
			triangles.indexBuffer = &planeModel.indexBuffer;
			triangles.vertexFormat = Format::R32G32B32_FLOAT;
			triangles.vertexCount = static_cast<uint32_t>(planeModel.vertices.size());
			triangles.vertexStride = sizeof(ModelVertex);
			triangles.indexCount = static_cast<uint32_t>(planeModel.indices.size());
		}

		m_Device->createRayTracingAS(cubeBLAS.info, cubeBLAS);
		m_Device->createRayTracingAS(planeBLAS.info, planeBLAS);
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

			void* dataSection = (uint8_t*)m_InstanceBuffer.mappedData + i * m_InstanceBuffer.info.stride;
			m_Device->writeTLASInstance(instance, dataSection);
		}
	}

	void Application::createEntities() {
		m_CubeModel = sr::assetmanager::loadFromFile("assets/models/cube.gltf", *m_Device);
		m_PlaneModel = sr::assetmanager::loadFromFile("assets/models/sphere.gltf", *m_Device);

		m_CubeEntity.position.z = 5.0f;
		m_CubeEntity.model = &m_CubeModel.getModel();
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