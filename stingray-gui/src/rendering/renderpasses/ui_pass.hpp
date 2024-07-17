#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"
#include "../../core/enum_flags.hpp"

#include <string>

namespace sr {
	enum class UIPosFlag {
		NONE = 0,
		HCENTER = 1 << 0,
		VCENTER = 1 << 1,
	};

	template<>
	struct enable_bitmask_operators<UIPosFlag> { static const bool enable = true; };
}

namespace sr::uipass {
	//void drawText(const std::string& text, const Font& font, UIPosFlag posFlags = UIPosFlag::NONE);
	//void drawRect(GraphicsDevice& device, glm::vec4 color, int width, int height, const Texture* texture = nullptr);
	//void beginSameLine();
	//void endSameLine();

	//void setCursorOriginX(float x);
	//void setCursorOriginY(float y);

	void onExecute(PassExecuteInfo& executeInfo);
}