#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"
#include "../../data/scene.hpp"

namespace sr::rtaopass {
	void onExecute(PassExecuteInfo& executeInfo, const Buffer& perFrameUBO, const Scene& scene);
	void destroy();
}