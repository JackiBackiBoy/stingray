#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"

namespace sr::fstripass {
	void onExecute(RenderGraph& graph, GraphicsDevice& device, const CommandList& cmdList);
}