#pragma once

#include "../device.hpp"
#include "../render_graph.hpp"
#include "../../core/enum_flags.hpp"
#include "../../ui/ui_event.hpp"

#include <functional>
#include <string>
#include <vector>

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
	static constexpr glm::vec4 UI_COLOR_PRIMARY = { 0.16f, 0.16f, 0.16f, 1.0f };
	static constexpr glm::vec4 UI_COLOR_SECONDARY = { 0.3f, 0.3f, 0.3f, 1.0f };
	static constexpr glm::vec4 UI_COLOR_TEXT_PRIMARY = { 1.0f, 1.0f, 1.0f, 1.0f };
	static constexpr glm::vec4 UI_COLOR_TEXT_SECONDARY = { 0.6f, 0.6f, 0.6f, 1.0f };
	static constexpr glm::vec4 UI_COLOR_HOVER = { 0.4f, 0.4f, 0.4f, 1.0f };

	struct UIElement {
		glm::vec2 position = {};
		int width = 0;
		int height = 0;

		std::function<void()> onClick;

		virtual void draw(GraphicsDevice& device) = 0;
		virtual void processEvent(const UIEvent& event) = 0;
	};

	struct UILayout : public UIElement {
		int rows = 0;
		int cols = 0;
		std::vector<size_t> elementIndices = {};
		glm::vec4 backgroundColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		inline int getColWidth() const { return width / cols; }
		inline int getRowHeight() const { return height / rows; }
		inline int getColWidthError() const { return width - getColWidth() * cols; } // NOTE: Total column width error across all columns
		inline int getRowHeightError() const { return height - getRowHeight() * rows; } // NOTE: Total row height error across all rows

		void draw(GraphicsDevice& device) override;
		void processEvent(const UIEvent& event) override;
	};

	struct UIButton : public UIElement {
		std::string text = "";
		glm::vec4 m_DisplayColor = UI_COLOR_PRIMARY;

		void draw(GraphicsDevice& device) override;
		void processEvent(const UIEvent& event) override;
	};

	struct UILabel : public UIElement {
		std::string text = "";

		void draw(GraphicsDevice& device) override;
		void processEvent(const UIEvent& event) override;
	};

	void processEvent(const UIEvent& event);

	// NOTE: width = 0 and height = 0 indicates that the layout dimensions should be automatically fitted
	UILayout* createLayout(int rows, int cols, int width, int height, UILayout* parentLayout = nullptr);
	UIButton* createButton(const std::string& text, int width, int height, UILayout* layout = nullptr);
	UILabel* createLabel(const std::string& text, UILayout* layout = nullptr);

	void onExecute(PassExecuteInfo& executeInfo);
}