#pragma once

#include "../render_graph.hpp"
#include "../../data/entity.hpp"

namespace sr::gbufferpass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO, const std::vector<Entity*>& entities);
}