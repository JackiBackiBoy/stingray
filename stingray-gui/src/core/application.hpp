#pragma once

#include <string>
#include <memory>
#include <vector>

#include "window.hpp"
#include "frame_info.hpp"
#include "settings.hpp"
#include "../data/scene.hpp"
#include "../managers/asset_manager.hpp"
#include "../math/quat.hpp"
#include "../rendering/device.hpp"
#include "../rendering/graphics.hpp"
#include "../rendering/render_graph.hpp"

#include <glm/glm.hpp>

namespace sr {
	class Application {
	public:
		Application(const std::string& title, int width, int height);
		virtual ~Application() = default;

		virtual void run() final;

		virtual void onInitialize() = 0;
		virtual void onUpdate() = 0;

	protected:
		struct alignas(256) PerFrameUBO {
			glm::mat4 projectionMatrix = { 1.0f };
			glm::mat4 viewMatrix = { 1.0f };
			glm::mat4 invViewProjection = { 1.0f };
			glm::vec3 cameraPosition = { 0.0f, 0.0f, 0.0f };
			uint32_t pad1 = 0;
		};

		void preInitialize();
		void createDefaultTextures();
		void createDefaultBuffers();
		void createDefaultSamplers();
		void createRenderGraph();
		void createEntities();
		void update(FrameInfo& frameInfo);
		void render(FrameInfo& frameInfo);

		Entity* m_StatueEntity = nullptr;

		int m_Width = 0;
		int m_Height = 0;
		std::string m_Title = {};
		Settings m_Settings = {};

		std::unique_ptr<Window> m_Window = {};
		std::unique_ptr<GraphicsDevice> m_Device = {};
		std::unique_ptr<RenderGraph> m_RenderGraph = {};
		FrameInfo m_FrameInfo = {};

		SwapChain m_SwapChain = {};
		Sampler m_LinearSampler = {};
		Sampler m_DepthSampler = {};
		Buffer m_PerFrameUBOs[GraphicsDevice::NUM_BUFFERS] = {};
		PerFrameUBO m_PerFrameUBOData = {};

		// Entities and assets (temporary)
		Asset m_SofaModel = {};
		Asset m_CubeModel = {};
		Asset m_PlaneModel = {};
		Asset m_StatueModel = {};

		// Default resources
		Texture m_DefaultAlbedoMap = {};
		Texture m_DefaultNormalMap = {};

		// TODO: Move this to a scene class or something
		std::unique_ptr<Scene> m_Scene = {};
	};
}