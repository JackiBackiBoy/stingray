#include "ui_pass.hpp"

namespace sr::uipass {
	static bool g_Initialized = false;

	void initialize(GraphicsDevice& device) {

	}

	void onExecute(RenderGraph& renderGraph, GraphicsDevice& device, const CommandList& cmdList) {
		if (!g_Initialized) {

			g_Initialized = true;
		}
	}

}