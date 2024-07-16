#include "application.hpp"

#include <chrono>

#include "../rendering/renderpasses/ray_tracing_pass.hpp"
#include "../rendering/renderpasses/fullscreen_tri_pass.hpp"

#if defined(SR_WINDOWS)
#include "../rendering/dx12/device_dx12.hpp"
#include "../input/rawinput.hpp"
#endif

#include <glm/gtc/matrix_transform.hpp>

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

		m_RenderGraph = std::make_unique<RenderGraph>(*m_Device);
	}

	Application::~Application() {
		for (size_t i = 0; i < m_Entities.size(); ++i) {
			delete m_Entities[i];
			m_Entities[i] = nullptr;
		}

		m_Entities.clear();
	}

	void Application::run() {
		preInitialize();
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

				update(dt);
				render(dt);
			}
		}

		m_Device->waitForGPU();
	}

	Entity* Application::addEntity(const std::string& name) {
		m_Entities.push_back(new Entity());

		return m_Entities.back();
	}

	void Application::preInitialize() {
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

		// Default samplers
		const SamplerInfo linearSamplerInfo = {};
		m_Device->createSampler(linearSamplerInfo, m_LinearSampler);

		// Rendergraph
		auto& rtPass = m_RenderGraph->addPass("RTPass");
		rtPass.addOutputAttachment("RTOutput", { AttachmentType::RW_TEXTURE, (uint32_t)m_Width, (uint32_t)m_Height, 1, Format::R8G8B8A8_UNORM });
		rtPass.setExecuteCallback([&](RenderGraph& graph, GraphicsDevice& device, const CommandList& commandList) {
			sr::rtpass::onExecute(graph, device, commandList, m_PerFrameUBOs[m_Device->getBufferIndex()], m_Entities);
		});

		auto& fullscreenTriPass = m_RenderGraph->addPass("FullscreenTriPass");
		fullscreenTriPass.addInputAttachment("RTOutput");
		fullscreenTriPass.setExecuteCallback([](RenderGraph& graph, GraphicsDevice& device, const CommandList& commandList) {
			sr::fstripass::onExecute(graph, device, commandList);
		});

		m_RenderGraph->build();

		// Entities
		createEntities();
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

	void Application::update(float dt) {
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

	void Application::render(float dt) {
		CommandList commandList = m_Device->beginCommandList(QueueType::DIRECT);
		{
			const Viewport viewport = {
				.width = static_cast<float>(m_Width),
				.height = static_cast<float>(m_Height),
			};
			m_Device->bindViewport(viewport, commandList);

			m_RenderGraph->execute(m_SwapChain, commandList);
		}
		m_Device->submitCommandLists(m_SwapChain);
	}

}