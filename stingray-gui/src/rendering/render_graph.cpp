#include "render_graph.hpp"

namespace sr {
	/* ------ RenderPass ------ */
	RenderPass::~RenderPass() {
	}

	RenderPassAttachment& RenderPass::addInputAttachment(const std::string& name) {
		RenderPassAttachment* attachment = m_Graph.getAttachment(name);
		attachment->readInPasses.emplace_back(m_Index);

		m_InputAttachments.push_back(attachment);

		return *attachment;
	}

	RenderPassAttachment& RenderPass::addOutputAttachment(const std::string& name, const AttachmentInfo& info) {
		RenderPassAttachment* attachment = m_Graph.getAttachment(name);
		attachment->info = info;
		attachment->writtenInPasses.emplace_back(m_Index);

		// Initial state
		switch (info.type) {
		case AttachmentType::RENDER_TARGET:
			attachment->currentState = ResourceState::RENDER_TARGET;
			break;
		case AttachmentType::DEPTH_STENCIL:
			attachment->currentState = ResourceState::DEPTH_WRITE;
			break;
		case AttachmentType::RW_TEXTURE:
			attachment->currentState = ResourceState::UNORDERED_ACCESS;
			break;
		}

		m_OutputAttachments.push_back(attachment);

		return *attachment;
	}

	void RenderPass::execute(GraphicsDevice& device, const CommandList& cmdList) {
		if (m_ExecuteCallback) {
			m_ExecuteCallback(m_Graph, device, cmdList);
		}
	}

	/* ------ RenderGraph ------ */
	RenderPass& RenderGraph::addPass(const std::string& name) {
		auto search = m_PassIndexLUT.find(name);

		if (search != m_PassIndexLUT.end()) {
			return *m_Passes[search->second];
		}

		size_t passIndex = m_Passes.size();
		m_PassIndexLUT.insert({ name, passIndex });
		m_Passes.push_back(std::make_unique<RenderPass>(*this, passIndex));

		return *m_Passes.back();
	}

	void RenderGraph::build() {
		// We begin at the root pass and recursively go upwards
		// TODO: The purpose of this function is to take an acyclic graph
		// of renderpasses, and then arrange them in the correct order
		// in a flat list. And THEN arrange pipeline barriers
		recurseBuild(m_Passes.size() - 1);
	}

	void RenderGraph::execute(SwapChain& swapChain, const CommandList& cmdList) {
		bool clearTargets = true;
		bool encounteredFirstRootPass = false;

		for (size_t p = 0; p < m_Passes.size(); ++p) {
			bool isRootPass = false;

			RenderPass& pass = *m_Passes[p];
			auto& inputAttachments = pass.getInputAttachments();
			auto& outputAttachments = pass.getOutputAttachments();
			RenderPassInfo passInfo = {}; // general pass info to be sent to the device

			// Root pass must use the swapchain backbuffer
			if (encounteredFirstRootPass) {
				clearTargets = false;
			}

			if (p == m_Passes.size() - 1 || outputAttachments.empty()) {
				if (!encounteredFirstRootPass) {
					encounteredFirstRootPass = true;
				}

				isRootPass = true;

				//passInfo.colors[passInfo.numColorAttachments++] = &swapChain.backbuffer;
			}

			// Define outputs
			for (size_t a = 0; a < outputAttachments.size(); ++a) {
				RenderPassAttachment* attachment = outputAttachments[a];

				ResourceState targetState = {};

				switch (attachment->info.type) {
				case AttachmentType::RENDER_TARGET:
					{
						passInfo.colors[passInfo.numColorAttachments++] = &attachment->texture;
						targetState = ResourceState::RENDER_TARGET;
					}
					break;
				case AttachmentType::DEPTH_STENCIL:
					{
						passInfo.depth = &attachment->texture;
						targetState = ResourceState::DEPTH_WRITE;
					}
					break;
				case AttachmentType::RW_TEXTURE:
					{
						targetState = ResourceState::UNORDERED_ACCESS;
					}
					break;
				}

				if (attachment->currentState != targetState) {
					m_Device.barrier(
						GPUBarrier::imageBarrier(&attachment->texture, attachment->currentState, targetState),
						cmdList
					);
					attachment->currentState = targetState;
				}
			}

			// Pipeline barriers to await required input transition layouts
			for (size_t a = 0; a < inputAttachments.size(); ++a) {
				RenderPassAttachment* attachment = inputAttachments[a];
				ResourceState targetState = ResourceState::SHADER_RESOURCE;

				if (attachment->currentState != targetState) {
					m_Device.barrier(
						GPUBarrier::imageBarrier(&attachment->texture, attachment->currentState, targetState),
						cmdList
					);
					attachment->currentState = targetState;
				}
			}

			if (isRootPass) {
				m_Device.beginRenderPass(swapChain, passInfo, cmdList, clearTargets);
				pass.execute(m_Device, cmdList);
				m_Device.endRenderPass(swapChain, cmdList);
			}
			else {
				m_Device.beginRenderPass(passInfo, cmdList, clearTargets);
				pass.execute(m_Device, cmdList);
				m_Device.endRenderPass();
			}
		}

		//m_Device.present(swapChain);
	}

	RenderPassAttachment* RenderGraph::getAttachment(const std::string& name) {
		auto search = m_AttachmentIndexLUT.find(name);

		if (search != m_AttachmentIndexLUT.end()) {
			return m_Attachments[search->second].get();
		}

		m_AttachmentIndexLUT.insert({ name, m_AttachmentIndexLUT.size() });

		auto attachment = std::make_unique<RenderPassAttachment>();
		attachment->name = name;

		m_Attachments.push_back(std::move(attachment));

		return m_Attachments.back().get();
	}

	void RenderGraph::recurseBuild(unsigned int index) {
		RenderPass& pass = *m_Passes[index];
		auto& inputs = pass.getInputAttachments();
		auto& outputs = pass.getOutputAttachments();

		// Build all outputs
		for (size_t i = 0; i < outputs.size(); ++i) {
			RenderPassAttachment* attachment = outputs[i];

			TextureInfo textureInfo = {
				.width = attachment->info.width,
				.height = attachment->info.height,
				.depth = 1, // TODO: For now we do not support 3D textures,
				.arraySize = 1,
				.mipLevels = 1,
				.sampleCount = attachment->info.sampleCount,
				.format = attachment->info.format,
				.usage = Usage::DEFAULT,
				.bindFlags = attachment->readInPasses.empty() ? BindFlag::NONE : BindFlag::SHADER_RESOURCE
			};

			switch (attachment->info.type) {
			case AttachmentType::RENDER_TARGET:
				textureInfo.bindFlags |= BindFlag::RENDER_TARGET;
				break;
			case AttachmentType::DEPTH_STENCIL:
				textureInfo.bindFlags |= BindFlag::DEPTH_STENCIL;
				break;
			case AttachmentType::RW_TEXTURE:
				textureInfo.bindFlags = BindFlag::UNORDERED_ACCESS;
				break;
			}

			m_Device.createTexture(textureInfo, attachment->texture, nullptr);
		}

		for (size_t i = 0; i < inputs.size(); ++i) {
			RenderPassAttachment* attachment = inputs[i];

			if (attachment->texture.internalState != nullptr) {
				continue;
			}

			for (size_t w = 0; w < attachment->writtenInPasses.size(); ++w) {
				recurseBuild(attachment->writtenInPasses[w]);
			}
		}

		if (inputs.empty() && outputs.empty() && index > 0) {
			recurseBuild(index - 1);
		}
	}

	void RenderGraph::generateBarriers() {
		//// Iterate through the list backwards, i.e. from the last pass to the first pass
		//for (size_t p = m_Passes.size() - 1; p > 0; --p) {
		//	RenderPass& pass = *m_Passes[p];
		//	auto& passInputs = pass.getInputAttachments();

		//	for (size_t i = 0; i < passInputs.size(); ++i) {

		//	}
		//}
	}
}