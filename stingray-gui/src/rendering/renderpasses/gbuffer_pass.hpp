#pragma once

#include "../render_graph.hpp"
#include "../../data/scene.hpp"

namespace sr::gbufferpass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const Scene& scene);
}