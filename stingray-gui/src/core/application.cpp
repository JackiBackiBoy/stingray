#include "application.hpp"

#include <algorithm>
#include <chrono>

#include "../input/input.hpp"
#include "../rendering/renderpasses/accumulation_pass.hpp"
#include "../rendering/renderpasses/fullscreen_tri_pass.hpp"
#include "../rendering/renderpasses/gbuffer_pass.hpp"
#include "../rendering/renderpasses/rtao_pass.hpp"
#include "../rendering/renderpasses/simple_shadow_pass.hpp"
#include "../rendering/renderpasses/ui_pass.hpp"

#if defined(SR_WINDOWS)
	#include "../rendering/dx12/device_dx12.hpp"
#endif

#include <glm/gtc/matrix_transform.hpp>

namespace sr {

	Application::Application(const std::string& title, int width, int height) :
		m_Title(title), m_Width(width), m_Height(height) {

		// Camera
		m_Camera = std::make_unique<Camera>(
			glm::vec3(2.0f, 1.0f, -3.0f),
			quatFromAxisAngle({ 0.0f, 1.0f, 0.0f }, glm::radians(-32.0f)),
			glm::radians(60.0f),
			static_cast<float>(width) / height,
			0.01f,
			50.0f
		);

		// Window
		const WindowInfo windowInfo = {
			.title = title,
			.width = m_Width,
			.height = m_Height,
			.flags = WindowFlag::HCENTER | WindowFlag::VCENTER | WindowFlag::SIZE_IS_CLIENT_AREA | WindowFlag::NO_TITLEBAR
		};

		m_Window = std::make_unique<Window>(windowInfo);
		
		// Graphics-device
		#if defined(SR_WINDOWS)
			m_Device = std::make_unique<GraphicsDevice_DX12>(m_Width, m_Height, m_Window->getHandle());
		#elif defined(SR_MACOS)
			m_Device = std::make_unique<GraphicsDevice_Metal>(m_Width, m_Height, m_Window->getHandle());
		#endif
	}

	void Application::run() {
		preInitialize();
		onInitialize();

		m_FrameInfo.camera = m_Camera.get();

		bool firstFrame = true;
		while (!m_Window->shouldClose() && !sr::input::isDown(KeyCode::Escape)) {
			m_Window->pollEvents();

			static auto lastTime = std::chrono::high_resolution_clock::now();
			const auto currentTime = std::chrono::high_resolution_clock::now();
			const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
			lastTime = currentTime;

			m_FrameInfo.dt = dt;
			m_FrameInfo.width = static_cast<uint32_t>(m_Width);
			m_FrameInfo.height = static_cast<uint32_t>(m_Height);

			update(m_FrameInfo);
			m_Scene->update();
			render(m_FrameInfo);

			// Show the window when ONE frame has been generated
			if (firstFrame) {
				m_Window->show();
				firstFrame = false;
			}
		}

		m_Device->waitForGPU();
	}

	void Application::preInitialize() {
		sr::input::initialize();

		const uint32_t uWidth = static_cast<uint32_t>(m_Width);
		const uint32_t uHeight = static_cast<uint32_t>(m_Height);
		const float aspectRatio = (float)m_Width / m_Height;

		// Initial PerFrameUBO data
		m_PerFrameUBOData.projectionMatrix = m_Camera->getProjMatrix();
		m_PerFrameUBOData.viewMatrix = m_Camera->getViewMatrix();
		m_PerFrameUBOData.invViewProjection = m_Camera->getInvViewProjMatrix();
		m_PerFrameUBOData.cameraPosition = m_Camera->getPosition();

		// Swapchain
		const SwapChainInfo swapChainInfo = {
			.width = uWidth,
			.height = uHeight,
			.bufferCount = 3,
			.format = Format::R8G8B8A8_UNORM,
			.fullscreen = false,
			.vsync = true
		};
		m_Device->createSwapChain(swapChainInfo, m_SwapChain, m_Window->getHandle());

		createDefaultTextures();
		createDefaultBuffers();
		createDefaultSamplers();
		createRenderGraph();
		createEntities();
	}

	void Application::createDefaultTextures() {
		const uint32_t defaultAlbedoMapData = 0xffffffff;
		const uint32_t defaultNormalMapData = 0xffff8080; // tangent space default normal

		const TextureInfo texture1x1Info = {
			.width = 1,
			.height = 1,
			.format = Format::R8G8B8A8_UNORM,
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::SHADER_RESOURCE
		};

		const SubresourceData defaultAlbedoMapSubresource = {
			.data = &defaultAlbedoMapData,
			.rowPitch = sizeof(uint32_t)
		};

		const SubresourceData defaultNormalMapSubresource = {
			.data = &defaultNormalMapData,
			.rowPitch = sizeof(uint32_t)
		};

		m_Device->createTexture(texture1x1Info, m_DefaultAlbedoMap, &defaultAlbedoMapSubresource);
		m_Device->createTexture(texture1x1Info, m_DefaultNormalMap, &defaultNormalMapSubresource);
	}

	void Application::createDefaultBuffers() {
		// Per-frame constant buffers (one for each frame in flight)
		const BufferInfo perFrameUBOInfo = {
			.size = sizeof(PerFrameUBO),
			.stride = sizeof(PerFrameUBO),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::UNIFORM_BUFFER,
			.persistentMap = true
		};

		for (size_t f = 0; f < GraphicsDevice::NUM_BUFFERS; ++f) {
			m_Device->createBuffer(perFrameUBOInfo, m_PerFrameUBOs[f], nullptr);
		}
	}

	void Application::createDefaultSamplers() {
		const SamplerInfo linearSamplerInfo = {};
		const SamplerInfo depthSamplerInfo = {
			.filter = Filter::COMPARISON_MIN_MAG_MIP_LINEAR,
			.addressU = TextureAddressMode::BORDER,
			.addressV = TextureAddressMode::BORDER,
			.addressW = TextureAddressMode::BORDER,
			.maxAnisotropy = 1,
			.comparisonFunc = ComparisonFunc::LESS_EQUAL,
			.borderColor = BorderColor::OPAQUE_WHITE,
			.maxLOD = 0
		};

		m_Device->createSampler(linearSamplerInfo, m_LinearSampler);
		m_Device->createSampler(depthSamplerInfo, m_DepthSampler);
	}

	void Application::createRenderGraph() {
		const uint32_t uWidth = static_cast<uint32_t>(m_Width);
		const uint32_t uHeight = static_cast<uint32_t>(m_Height);

		m_RenderGraph = std::make_unique<RenderGraph>(*m_Device);

		auto& gBufferPass = m_RenderGraph->addPass("GBufferPass");
		gBufferPass.addOutputAttachment("Position", { AttachmentType::RENDER_TARGET, uWidth, uHeight, 1, Format::R32G32B32A32_FLOAT });
		gBufferPass.addOutputAttachment("Albedo", { AttachmentType::RENDER_TARGET, uWidth, uHeight, 1, Format::R8G8B8A8_UNORM });
		gBufferPass.addOutputAttachment("Normal", { AttachmentType::RENDER_TARGET, uWidth, uHeight, 1, Format::R16G16B16A16_FLOAT });
		gBufferPass.addOutputAttachment("Depth", { AttachmentType::DEPTH_STENCIL, uWidth, uHeight, 1, Format::D32_FLOAT });
		gBufferPass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::gbufferpass::onExecute(executeInfo, m_PerFrameUBOs[m_Device->getBufferIndex()], *m_Scene);
		});

		const uint32_t shadowMapDim = 4096;
		auto& simpleShadowPass = m_RenderGraph->addPass("SimpleShadowPass");
		simpleShadowPass.addOutputAttachment("ShadowMap", { AttachmentType::DEPTH_STENCIL, shadowMapDim, shadowMapDim, 1, Format::D16_UNORM });
		simpleShadowPass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::simpleshadowpass::onExecute(executeInfo, m_PerFrameUBOs[m_Device->getBufferIndex()], *m_Scene);
		});

		auto& rtaoPass = m_RenderGraph->addPass("RTAOPass");
		rtaoPass.addInputAttachment("Position");
		rtaoPass.addInputAttachment("Normal");
		rtaoPass.addOutputAttachment("AmbientOcclusion", { AttachmentType::RW_TEXTURE, uWidth, uHeight, 1, Format::R8G8B8A8_UNORM });
		rtaoPass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::rtaopass::onExecute(executeInfo, m_PerFrameUBOs[m_Device->getBufferIndex()], *m_Scene);
		});

		auto& accumulationpass = m_RenderGraph->addPass("AccumulationPass");
		accumulationpass.addInputAttachment("AmbientOcclusion");
		accumulationpass.addOutputAttachment("AOAccumulation", { AttachmentType::RENDER_TARGET, uWidth, uHeight, 1, Format::R8G8B8A8_UNORM });
		accumulationpass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::accumulationpass::onExecute(executeInfo);
		});

		auto& fullscreenTriPass = m_RenderGraph->addPass("FullscreenTriPass");
		fullscreenTriPass.addInputAttachment("Position");
		fullscreenTriPass.addInputAttachment("Albedo");
		fullscreenTriPass.addInputAttachment("Normal");
		fullscreenTriPass.addInputAttachment("Depth");
		fullscreenTriPass.addInputAttachment("ShadowMap");
		fullscreenTriPass.addInputAttachment("AmbientOcclusion");
		fullscreenTriPass.addInputAttachment("AOAccumulation");
		fullscreenTriPass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::fstripass::onExecute(executeInfo, m_PerFrameUBOs[m_Device->getBufferIndex()], m_Settings, *m_Scene);
		});

		auto& uiPass = m_RenderGraph->addPass("UIPass");
		uiPass.addInputAttachment("Position");
		uiPass.addInputAttachment("Albedo");
		uiPass.addInputAttachment("Normal");
		uiPass.addInputAttachment("ShadowMap");
		uiPass.addInputAttachment("AmbientOcclusion");
		uiPass.addInputAttachment("AOAccumulation");
		uiPass.setExecuteCallback([&](PassExecuteInfo& executeInfo) {
			sr::uipass::onExecute(executeInfo, m_Settings, *m_Scene);
		});

		m_RenderGraph->build();
	}

	void Application::createEntities() {
		m_Scene = std::make_unique<Scene>(*m_Camera);

		m_CubeModel = sr::assetmanager::loadFromFile("assets/models/multimeshtest.gltf", *m_Device);
		m_PlaneModel = sr::assetmanager::loadFromFile("assets/models/plane.gltf", *m_Device);
		m_StatueModel = sr::assetmanager::loadFromFile("assets/models/statue.gltf", *m_Device);
		m_MainSceneModel = sr::assetmanager::loadFromFile("assets/models/test_scene.gltf", *m_Device);

		// Main scene
		auto mainScene = m_Scene->createEntity("Main Scene");
		mainScene->model = &m_MainSceneModel.getModel();
		mainScene->color = { 0.5f, 0.5f, 0.5f };

		// Cornell box
		const float cornellScale = 4.0f;

		//auto cornellWallLeft = m_Scene->createEntity("WallLeft");
		//cornellWallLeft->position.x = -cornellScale * 0.5f + 0.01f;
		//cornellWallLeft->position.y = cornellScale * 0.5f;
		//cornellWallLeft->scale = glm::vec3(cornellScale);
		//cornellWallLeft->model = &m_PlaneModel.getModel();
		//cornellWallLeft->orientation = quatFromAxisAngle({ 0.0f, 0.0f, 1.0f }, glm::radians(-90.0f));
		//cornellWallLeft->color = { 1.0f, 0.0f, 0.0f };

		//auto cornellWallBack = m_Scene->createEntity("WallBack");
		//cornellWallBack->position.y = cornellScale * 0.5f;
		//cornellWallBack->position.z = cornellScale * 0.5f;
		//cornellWallBack->scale = glm::vec3(cornellScale);
		//cornellWallBack->model = &m_PlaneModel.getModel();
		//cornellWallBack->orientation = quatFromAxisAngle({ 1.0f, 0.0f, 0.0f }, glm::radians(-90.0f));
		//cornellWallBack->color = { 0.229f, 0.531f, 0.0f };

		auto sphere = m_Scene->createEntity("Sphere");
		sphere->scale = glm::vec3(0.5f);
		sphere->position = { -0.3f, 1.5f, 1.0f };
		sphere->model = &m_CubeModel.getModel();

		m_StatueEntity = m_Scene->createEntity("Statue");
		m_StatueEntity->position = { 0.5f, 0.0f, 0.2f };
		m_StatueEntity->scale = glm::vec3(1.0f);
		m_StatueEntity->model = &m_StatueModel.getModel();
		m_StatueEntity->orientation = quatFromAxisAngle({ 0.0f, 0.0f, 1.0f }, glm::radians(-90.0f));
		m_StatueEntity->orientation = quatFromAxisAngle({ 0.0f, 1.0f, 0.0f }, glm::radians(90.0f)) * m_StatueEntity->orientation;
		m_StatueEntity->color = { 0.5f, 0.6f, 0.7f };

		// Lights
		auto light1 = m_Scene->createPointLight("Light 1", { 1.0f, 0.0f, 0.0f, 1.5f }, { 0.0f, 2.0f, 0.0f });
	}

	void Application::update(FrameInfo& frameInfo) {
		// Entities update
		m_StatueEntity->orientation = m_StatueEntity->orientation * quatFromAxisAngle({ 1.0f, 0.0f, 0.0f }, frameInfo.dt);
		//m_StatueEntity->scale = glm::vec3(2.0f + cosf(t));

		sr::input::update();
		sr::input::KeyboardState keyboard = {};
		sr::input::MouseState mouse = {};

		sr::input::getKeyboardState(keyboard);
		sr::input::getMouseState(mouse);

		// Camera movement
		Camera* camera = frameInfo.camera;
		const float cameraMoveSpeed = 5.0f;
		const float mouseSensitivity = 0.001f;

		if (mouse.mouse3) {
			quat newOrientation = camera->getOrientation();

			if (mouse.deltaY != 0.0f) {
				newOrientation = newOrientation * quatFromAxisAngle({ 1.0f, 0.0f, 0.0f }, mouse.deltaY * mouseSensitivity); // pitch
			}

			if (mouse.deltaX != 0.0f) {
				newOrientation = quatFromAxisAngle({ 0.0f, 1.0f, 0.0f }, mouse.deltaX * mouseSensitivity) * newOrientation; // yaw
			}

			camera->setOrientation(newOrientation);
		}

		const glm::vec3 qRight = camera->getRight();
		const glm::vec3 qUp = camera->getUp();
		const glm::vec3 qForward = camera->getForward();

		glm::vec3 newPosition = camera->getPosition();

		if (sr::input::isDown(KeyCode::W)) {
			newPosition += qForward * cameraMoveSpeed * frameInfo.dt;
		}
		if (sr::input::isDown(KeyCode::A)) {
			newPosition -= qRight * cameraMoveSpeed * frameInfo.dt;
		}
		if (sr::input::isDown(KeyCode::S)) {
			newPosition -= qForward * cameraMoveSpeed * frameInfo.dt;
		}
		if (sr::input::isDown(KeyCode::D)) {
			newPosition += qRight * cameraMoveSpeed * frameInfo.dt;
		}

		if (sr::input::isDown(KeyCode::Space)) {
			newPosition.y += cameraMoveSpeed * frameInfo.dt;
		}
		if (sr::input::isDown(KeyCode::LeftControl)) {
			newPosition.y -= cameraMoveSpeed * frameInfo.dt;
		}

		camera->setPosition(newPosition);

		if (camera->getVerticalFOV() != glm::radians((float)m_Settings.verticalFOV)) {
			camera->setVerticalFOV(glm::radians((float)m_Settings.verticalFOV));
		}

		
		const float aspectRatio = static_cast<float>(m_Width) / m_Height;
		camera->setAspectRatio(aspectRatio);

		camera->update();

			// Update per-frame data
		m_PerFrameUBOData.projectionMatrix = camera->getProjMatrix();
		m_PerFrameUBOData.viewMatrix = camera->getViewMatrix();
		m_PerFrameUBOData.invViewProjection = camera->getInvViewProjMatrix();
		m_PerFrameUBOData.cameraPosition = camera->getPosition();

		std::memcpy(m_PerFrameUBOs[m_Device->getBufferIndex()].mappedData, &m_PerFrameUBOData, sizeof(m_PerFrameUBOData));
	}

	void Application::render(FrameInfo& frameInfo) {
		CommandList cmdList = m_Device->beginCommandList(QueueType::DIRECT);
		{
			m_RenderGraph->execute(m_SwapChain, cmdList, m_FrameInfo);
		}
		m_Device->submitCommandLists(m_SwapChain);
	}

}