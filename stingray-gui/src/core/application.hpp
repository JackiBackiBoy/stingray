#pragma once

#include <string>
#include <memory>
#include <vector>

#include "window.hpp"
#include "../data/entity.hpp"
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
		virtual ~Application();

		void run();
		Entity* addEntity(const std::string& name);

		virtual void onInitialize() = 0;
		virtual void onUpdate() = 0;

	protected:
		struct Camera {
			glm::mat4 viewMatrix = { 1.0f };
			glm::vec3 position = { 0.0f, 0.0f, -10.0f };
			quat orientation = {};
		};

		struct alignas(256) PerFrameUBO {
			glm::mat4 projectionMatrix = { 1.0f };
			glm::mat4 viewMatrix = { 1.0f };
			glm::mat4 invViewProjection = { 1.0f };
			glm::vec3 cameraPosition = { 0.0f, 0.0f, 0.0f };
			uint32_t pad1 = 0;
		};

		void preInitialize();
		void createEntities();
		void update(float dt);
		void render(float dt);

		int m_Width = 0;
		int m_Height = 0;
		std::string m_Title;

		std::unique_ptr<Window> m_Window = {};
		std::unique_ptr<GraphicsDevice> m_Device = {};
		std::unique_ptr<RenderGraph> m_RenderGraph = {};
		Camera m_Camera = {};

		SwapChain m_SwapChain = {};
		Sampler m_LinearSampler = {};
		Buffer m_PerFrameUBOs[GraphicsDevice::NUM_BUFFERS] = {};
		PerFrameUBO m_PerFrameUBOData = {};

		// Entities and assets (temporary)
		Asset m_CubeModel = {};
		Asset m_PlaneModel = {};
		Asset m_StatueModel = {};

		// Default resources
		Texture m_DefaultAlbedoMap = {};
		Texture m_DefaultNormalMap = {};

		// TODO: Move this to a scene class or something
		std::vector<Entity*> m_Entities = {};
		Entity* m_SphereEntity = nullptr;
		Entity* m_MirrorEntity = nullptr;
	};
}