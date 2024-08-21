#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "device.hpp"
#include "../core/frame_info.hpp"

namespace sr {
	class RenderGraph;

	enum class AttachmentType : uint8_t {
		RENDER_TARGET,
		DEPTH_STENCIL,
		RW_TEXTURE
	};

	struct AttachmentInfo {
		AttachmentType type = AttachmentType::RENDER_TARGET;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t sampleCount = 1;
		Format format = Format::UNKNOWN;
	};

	struct RenderPassAttachment {
		AttachmentInfo info = {};
		Texture texture = {};
		std::string name = {};
		ResourceState currentState = ResourceState::UNDEFINED;

		std::vector<unsigned int> writtenInPasses = {};
		std::vector<unsigned int> readInPasses = {};
	};

	struct PassExecuteInfo {
		RenderGraph* renderGraph;
		GraphicsDevice* device;
		const CommandList* cmdList;
		const FrameInfo* frameInfo;
	};

	class RenderPass {
	public:
		RenderPass(RenderGraph& renderGraph, unsigned int index, const std::string& name) :
			m_Graph(renderGraph), m_Index(index), m_Name(name) {}
		~RenderPass();

		RenderPassAttachment& addInputAttachment(const std::string& name);
		RenderPassAttachment& addOutputAttachment(const std::string& name, const AttachmentInfo& info);
		void execute(GraphicsDevice& device, const CommandList& cmdList, const FrameInfo& frameInfo);

		inline std::string getName() const { return m_Name; }
		inline std::vector<RenderPassAttachment*>& getInputAttachments() { return m_InputAttachments; }
		inline std::vector<RenderPassAttachment*>& getOutputAttachments() { return m_OutputAttachments; }

		inline void setExecuteCallback(std::function<void(PassExecuteInfo& executeInfo)> callback) {
			m_ExecuteCallback = std::move(callback);
		}

	private:
		RenderGraph& m_Graph;
		unsigned int m_Index;
		std::string m_Name;
		std::function<void(PassExecuteInfo& executeInfo)> m_ExecuteCallback;

		std::vector<RenderPassAttachment*> m_OutputAttachments = {};
		std::vector<RenderPassAttachment*> m_InputAttachments = {};
	};

	class RenderGraph {
	public:
		RenderGraph(GraphicsDevice& device) : m_Device(device) {};
		~RenderGraph() {};

		RenderPass& addPass(const std::string& name);

		void build();
		void execute(SwapChain& swapChain, const CommandList& cmdList, const FrameInfo& frameInfo);

		RenderPassAttachment* getAttachment(const std::string& name);
		inline const std::vector<std::unique_ptr<RenderPass>>& getPasses() const { return m_Passes; }

	private:
		void recurseBuild(unsigned int index);
		void generateBarriers();

		GraphicsDevice& m_Device;

		std::vector<std::unique_ptr<RenderPass>> m_Passes = {};
		std::vector<std::unique_ptr<RenderPassAttachment>> m_Attachments = {};
		std::unordered_map<std::string, size_t> m_PassIndexLUT = {};
		std::unordered_map<std::string, size_t> m_AttachmentIndexLUT = {};
	};
}