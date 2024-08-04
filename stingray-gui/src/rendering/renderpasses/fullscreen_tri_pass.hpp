#pragma once

#include "../render_graph.hpp"

namespace sr::fstripass {
	void onExecute(PassExecuteInfo& executeInfo, Buffer& perFrameUBO);
}