#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"
#include "../../data/entity.hpp"

namespace sr::gbufferpass {
	void onExecute(RenderGraph& graph, GraphicsDevice& device, const CommandList& cmdList, Buffer& perFrameUBO, const std::vector<Entity*>& entities);
}