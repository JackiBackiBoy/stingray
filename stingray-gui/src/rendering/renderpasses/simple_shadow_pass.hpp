#pragma once

#include "../render_graph.hpp"
#include "../../data/scene.hpp"

namespace sr::simpleshadowpass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const Scene& scene);
}