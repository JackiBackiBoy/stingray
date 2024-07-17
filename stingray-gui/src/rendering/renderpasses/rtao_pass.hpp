#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"
#include "../../data/entity.hpp"

namespace sr::rtaopass {
	void onExecute(PassExecuteInfo& executeInfo, const Buffer& perFrameUBO, const std::vector<Entity*>& entities);
}