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

#include <glm/glm.hpp>
#include <DirectXMath.h> // temp

namespace sr {
	class Application {
	public:
		Application(const std::string& title);
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
			glm::mat4 invViewProjection = {};
			glm::vec3 cameraPosition = { 0.0f, 0.0f, 0.0f };
			float pad1 = 0.0f;
		};

		// TODO: Likely needs a name change
		struct GeometryInfo { // NOTE: Per BLAS
			uint32_t vertexBufferIndex = 0;
			uint32_t indexBufferIndex = 0;
		};

		struct MaterialInfo {
			glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			float roughness = 1.0f;
		};

		void initialize();
		void createEntities();
		void createRTObjects();
		void createBLASes();
		void createTLAS();
		void writeTLASInstances();
		

		void updateRT(float dt);
		void renderRT(float dt);

		int m_Width = 0;
		int m_Height = 0;
		std::string m_Title;
		std::unique_ptr<Window> m_Window = {};
		std::unique_ptr<GraphicsDevice> m_Device = {};
		SwapChain m_SwapChain = {};

		Camera m_Camera = {};

		// Ray-Tracing Objects
		Shader m_RayTracingShaderLibrary = {};
		Buffer m_RayGenShaderTable = {};
		Buffer m_MissShaderTable = {};
		Buffer m_HitShaderTable = {};
		Buffer m_InstanceBuffer = {};

		Buffer m_PerFrameUBOs[GraphicsDevice::NUM_BUFFERS] = {};
		PerFrameUBO m_PerFrameUBOData = {};
		Buffer m_GeometryInfoBuffer = {};
		std::vector<GeometryInfo> m_GeometryInfoData = {};
		Buffer m_MaterialInfoBuffer = {};
		std::vector<MaterialInfo> m_MaterialInfoData = {};

		std::vector<std::unique_ptr<RayTracingAS>> m_RayTracingBLASes = {};
		RayTracingAS m_RayTracingTLAS = {};
		RayTracingPipeline m_RayTracingPipeline = {};
		Texture m_RayTracingOutput = {};

		// Entities and assets (temporary)
		Asset m_CubeModel = {};
		Asset m_PlaneModel = {};

		// TODO: Move this to a scene class or something
		std::vector<std::unique_ptr<Entity>> m_Entities = {};
		Entity* m_SphereEntity = nullptr;
		Entity* m_MirrorEntity = nullptr;
	};
}