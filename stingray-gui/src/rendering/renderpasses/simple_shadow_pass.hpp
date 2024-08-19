#pragma once

#include "../render_graph.hpp"
#include "../../data/scene.hpp"

// Simple Shadow Pass (SSM)
namespace sr::simpleshadowpass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, Scene& scene);
}