#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "device.hpp"

namespace sr {
	class RenderGraph;

	enum class AttachmentType {
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

	class RenderPass {
	public:
		RenderPass(RenderGraph& renderGraph, unsigned int index) : m_Graph(renderGraph), m_Index(index) {}
		~RenderPass();

		RenderPassAttachment& addInputAttachment(const std::string& name);
		RenderPassAttachment& addOutputAttachment(const std::string& name, const AttachmentInfo& info);
		void execute(GraphicsDevice& device, const CommandList& cmdList);

		inline std::vector<RenderPassAttachment*>& getInputAttachments() { return m_InputAttachments; }
		inline std::vector<RenderPassAttachment*>& getOutputAttachments() { return m_OutputAttachments; }

		inline void setExecuteCallback(std::function<void(RenderGraph& graph, GraphicsDevice& device, const CommandList& cmdList)> callback) {
			m_ExecuteCallback = std::move(callback);
		}

	private:
		RenderGraph& m_Graph;
		unsigned int m_Index;
		std::function<void(RenderGraph& graph, GraphicsDevice& device, const CommandList& cmdList)> m_ExecuteCallback;

		std::vector<RenderPassAttachment*> m_OutputAttachments = {};
		std::vector<RenderPassAttachment*> m_InputAttachments = {};
	};

	class RenderGraph {
	public:
		RenderGraph(GraphicsDevice& device) : m_Device(device) {};
		~RenderGraph() {};

		RenderPass& addPass(const std::string& name);

		void build();
		void execute(SwapChain& swapChain, const CommandList& cmdList);

		RenderPassAttachment* getAttachment(const std::string& name);

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