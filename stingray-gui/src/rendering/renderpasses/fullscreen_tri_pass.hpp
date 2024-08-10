#pragma once

#include "../render_graph.hpp"
#include "../../data/scene.hpp"

namespace sr::fstripass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, Scene& scene);
}