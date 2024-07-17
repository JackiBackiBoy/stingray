#include "ui_pass.hpp"

namespace sr::uipass {
	static bool g_Initialized = false;

	void initialize(GraphicsDevice& device) {

	}

	void onExecute(PassExecuteInfo& executeInfo) {
		if (!g_Initialized) {

			g_Initialized = true;
		}
	}

}