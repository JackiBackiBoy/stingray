#pragma once

#include "../render_graph.hpp"
#include "../../core/settings.hpp"
#include "../../data/scene.hpp"

namespace sr::fstripass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const Settings& settings, Scene& scene);
}