#include "ui_pass.hpp"

#include "../../data/font.hpp"
#include "../../managers/asset_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <memory>
#include <unordered_map>

namespace sr::uipass {
	static constexpr uint32_t MAX_TEXT_PARAMS = 8192;
	static constexpr uint32_t MAX_UI_PARAMS = 8192;
	
	static std::vector<std::unique_ptr<UIElement>> g_UIElements = {};
	static size_t g_UIElementIndex = 0;
	static UIElement* g_CurrentUIElement = nullptr;
	static UIElement* g_LastUIElement = nullptr;

	static UILabel* g_FPSCounterLabel = nullptr;
	static uint64_t g_LastFrameCount = 0;
	static std::chrono::high_resolution_clock::time_point g_FPSStartTime = {};

	struct TextParams {
		glm::vec4 color = {};
		glm::vec2 position = {};
		glm::vec2 size = {};
		glm::vec2 texCoords[4] = {};
	};

	struct UIParams {
		glm::vec4 color = {};
		glm::vec2 position = {};
		glm::vec2 size = {};
		glm::vec2 texCoords[4] = {};
		uint32_t texIndex = 0;
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	struct TextPushConstant {
		glm::mat4 uiProjection = { 1.0f };
		uint32_t texIndex = 0;
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	struct UIPushConstant {
		glm::mat4 uiProjection = { 1.0f };
	};

	static GraphicsDevice* g_Device = nullptr;
	static Shader g_TextVertexShader = {};
	static Shader g_TextPixelShader = {};
	static BlendState g_TextBlendState = {};
	static Pipeline g_TextPipeline = {};
	static TextPushConstant g_TextPushConstant = {};
	static Buffer g_TextParamsBuffers[GraphicsDevice::NUM_BUFFERS] = {};
	static std::vector<TextParams> g_TextParamsData = {};

	static Shader g_UIVertexShader = {};
	static Shader g_UIPixelShader = {};
	static BlendState g_UIBlendState = {};
	static Pipeline g_UIPipeline = {};
	static UIPushConstant g_UIPushConstant = {};
	static Buffer g_UIParamsBuffers[GraphicsDevice::NUM_BUFFERS] = {};
	static std::vector<UIParams> g_UIParamsData = {};

	static Asset g_MinimizeIcon = {};
	static Asset g_MaximizeIcon = {};
	static Asset g_CloseIcon = {};
	static Font g_DefaultFont = {};

	static glm::vec2 g_DefaultCursorOrigin = { 0, 0 };
	static glm::vec2 g_CursorOriginReturn = g_DefaultCursorOrigin; // NOTE: Used when 'sameLine' function is used
	static glm::vec2 g_CursorOrigin = g_DefaultCursorOrigin;
	static bool g_SameLineActive = false;
	static bool g_Initialized = false;

	void drawText(const std::string& text, const glm::vec2& pos, const glm::vec4 color, const Font& font, UIPosFlag posFlags = UIPosFlag::NONE) {
		float textPosX = pos.x;
		float textPosY = pos.y;

		const float textPosOriginX = textPosX;
		const float textPosOriginY = textPosY;

		if (has_flag(posFlags, UIPosFlag::HCENTER)) {
			textPosX -= (float)(g_DefaultFont.calcTextWidth(text) / 2);
		}
		if (has_flag(posFlags, UIPosFlag::VCENTER)) {
			textPosY -= (float)(g_DefaultFont.boundingBoxHeight / 2);
		}

		for (size_t i = 0; i < text.length(); ++i) {
			const char character = text[i];
			const GlyphData& glyphData = font.glyphs[character];

			if (character == ' ') {
				textPosX += glyphData.advanceX;
				continue;
			}

			if (character == '\n') {
				textPosX = textPosOriginX;
				textPosY += font.lineSpacing;
			}

			TextParams textParams = {};
			textParams.color = color;
			textParams.position.x = i == 0 ? textPosX : textPosX + glyphData.bearingX; // Only use bearing if it's not the first character
			textParams.position.y = textPosY + (font.maxBearingY - glyphData.bearingY);
			textParams.size = { glyphData.width, glyphData.height };
			textParams.texCoords[0] = glyphData.texCoords[0];
			textParams.texCoords[1] = glyphData.texCoords[1];
			textParams.texCoords[2] = glyphData.texCoords[2];
			textParams.texCoords[3] = glyphData.texCoords[3];
			g_TextParamsData.push_back(textParams);

			textPosX += glyphData.advanceX;
		}
	}

	void drawRect(const glm::vec2& pos, int width, int height, glm::vec4 color, const Texture* texture) {
		if (g_Device == nullptr) {
			return;
		}

		UIParams uiParams = {};
		uiParams.color = color;
		uiParams.position.x = pos.x;
		uiParams.position.y = pos.y;
		uiParams.size = { width, height };

		if (texture != nullptr) {
			uiParams.texIndex = g_Device->getDescriptorIndex(*texture);
		}

		uiParams.texCoords[0] = { 0.0f, 0.0f };
		uiParams.texCoords[1] = { 1.0f, 0.0f };
		uiParams.texCoords[2] = { 1.0f, 1.0f };
		uiParams.texCoords[3] = { 0.0f, 1.0f };

		g_UIParamsData.push_back(uiParams);
	}

	static void initialize(GraphicsDevice& device, const FrameInfo& frameInfo) {
		g_Device = &device;
		sr::fontloader::loadFromFile("assets/fonts/segoeui.ttf", 14, g_DefaultFont, device);

		// Text pipeline
		device.createShader(ShaderStage::VERTEX, "assets/shaders/text.vs.hlsl", g_TextVertexShader);
		device.createShader(ShaderStage::PIXEL, "assets/shaders/text.ps.hlsl", g_TextPixelShader);

		g_TextBlendState.alphaToCoverage = false;
		g_TextBlendState.independentBlend = false;
		g_TextBlendState.renderTargetBlendStates[0].blendEnable = true;

		const PipelineInfo textPipelineInfo = {
			.vertexShader = &g_TextVertexShader,
			.fragmentShader = &g_TextPixelShader,
			.blendState = &g_TextBlendState,
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM },
		};

		device.createPipeline(textPipelineInfo, g_TextPipeline);

		const BufferInfo textParamsInfo = {
			.size = MAX_TEXT_PARAMS * sizeof(TextParams),
			.stride = sizeof(TextParams),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		for (size_t i = 0; i < GraphicsDevice::NUM_BUFFERS; ++i) {
			device.createBuffer(textParamsInfo, g_TextParamsBuffers[i], nullptr);
		}

		// General UI pipeline
		device.createShader(ShaderStage::VERTEX, "assets/shaders/ui.vs.hlsl", g_UIVertexShader);
		device.createShader(ShaderStage::PIXEL, "assets/shaders/ui.ps.hlsl", g_UIPixelShader);

		g_UIBlendState.alphaToCoverage = false;
		g_UIBlendState.independentBlend = false;
		g_UIBlendState.renderTargetBlendStates[0].blendEnable = true;

		const PipelineInfo uiPipelineInfo = {
			.vertexShader = &g_UIVertexShader,
			.fragmentShader = &g_UIPixelShader,
			.blendState = &g_UIBlendState,
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM },
		};

		device.createPipeline(uiPipelineInfo, g_UIPipeline);

		const BufferInfo uiParamsInfo = {
			.size = MAX_UI_PARAMS * sizeof(UIParams),
			.stride = sizeof(UIParams),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		for (size_t i = 0; i < GraphicsDevice::NUM_BUFFERS; ++i) {
			device.createBuffer(uiParamsInfo, g_UIParamsBuffers[i], nullptr);
		}

		// Textures
		g_MinimizeIcon = sr::assetmanager::loadFromFile("assets/icons/minimize.png", device);
		g_MaximizeIcon = sr::assetmanager::loadFromFile("assets/icons/maximize.png", device);
		g_CloseIcon = sr::assetmanager::loadFromFile("assets/icons/close.png", device);

		// UI
		g_CursorOrigin = { 0, 31 };
		auto mainLayout = createLayout(2, 2, frameInfo.width, frameInfo.height - 31);
		auto settingsLayout = createLayout(8, 1, 200, frameInfo.height - 31, mainLayout);

		auto statsLayout = createLayout(5, 1, 0, 0, settingsLayout);
		statsLayout->backgroundColor = UI_COLOR_PRIMARY;
		auto statsTitle = createLabel("Statistics:", statsLayout);
		g_FPSCounterLabel = createLabel("FPS: ", statsLayout);

		auto button1 = createButton("Button 1", 0, 0, settingsLayout);
		auto button2 = createButton("Button 2", 0, 0, settingsLayout);
		auto button3 = createButton("Button 3", 0, 0, settingsLayout);
		auto button4 = createButton("Button 4", 0, 0, settingsLayout);
		auto button5 = createButton("Button 5", 0, 0, settingsLayout);
		auto button6 = createButton("Button 6", 0, 0, settingsLayout);
		auto button7 = createButton("Button 7", 0, 0, settingsLayout);
	}

	void onExecute(PassExecuteInfo& executeInfo) {
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;
		const FrameInfo& frameInfo = *executeInfo.frameInfo;

		if (!g_Initialized) {
			initialize(device, frameInfo);
			g_Initialized = true;
		}

		float fWidth = static_cast<float>(frameInfo.width);
		float fHeight = static_cast<float>(frameInfo.height);

		// Update FPS counter
		const auto currentTime = std::chrono::high_resolution_clock::now();
		if (std::chrono::duration<float, std::chrono::seconds::period>(
			currentTime - g_FPSStartTime).count() >= 1.0f) {
			g_FPSStartTime = currentTime;
			g_FPSCounterLabel->text = "FPS: " + std::to_string(device.getFrameCount() - g_LastFrameCount);
			g_LastFrameCount = device.getFrameCount();
		}

		// Draw UI elements
		for (auto& element : g_UIElements) {
			element->draw(device);
		}

		// Draw titlebar
		const int sysMenuWidth = 44 * 3;
		drawRect({ 0, 0 }, fWidth, 31, { 0.13f, 0.13f, 0.135f, 0.95f }, nullptr);
		drawRect({ fWidth - sysMenuWidth, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_MinimizeIcon.getTexture());
		drawRect({ fWidth - sysMenuWidth + 44, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_MaximizeIcon.getTexture());
		drawRect({ fWidth - sysMenuWidth + 88, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_CloseIcon.getTexture());
		drawText("Stingray", { fWidth / 2, 8 }, { 1.0f, 1.0f, 1.0f, 1.0f }, g_DefaultFont, UIPosFlag::HCENTER);

		// Push constants
		g_TextPushConstant.uiProjection = glm::ortho(0.0f, fWidth, fHeight, 0.0f);
		g_TextPushConstant.texIndex = device.getDescriptorIndex(g_DefaultFont.fontAtlasTexture);
		g_UIPushConstant.uiProjection = g_TextPushConstant.uiProjection;

		// Update structured buffers for text and UI
		std::memcpy(g_TextParamsBuffers[device.getBufferIndex()].mappedData, g_TextParamsData.data(), g_TextParamsData.size() * sizeof(TextParams));
		std::memcpy(g_UIParamsBuffers[device.getBufferIndex()].mappedData, g_UIParamsData.data(), g_UIParamsData.size() * sizeof(UIParams));

		// UI pipeline
		device.bindPipeline(g_UIPipeline, cmdList);
		device.bindResource(g_UIParamsBuffers[device.getBufferIndex()], "g_UIParamsBuffer", g_UIPipeline, cmdList);

		device.pushConstants(&g_UIPushConstant, sizeof(g_UIPushConstant), cmdList);
		device.drawInstanced(6, static_cast<uint32_t>(g_UIParamsData.size()), 0, 0, cmdList);

		// Text pipeline
		device.bindPipeline(g_TextPipeline, cmdList);
		device.bindResource(g_TextParamsBuffers[device.getBufferIndex()], "g_TextParamsBuffer", g_TextPipeline, cmdList);

		device.pushConstants(&g_TextPushConstant, sizeof(g_TextPushConstant), cmdList);
		device.drawInstanced(6, static_cast<uint32_t>(g_TextParamsData.size()), 0, 0, cmdList);

		g_TextParamsData.clear();
		g_UIParamsData.clear();
		g_CursorOrigin = g_DefaultCursorOrigin;
		g_SameLineActive = false;
	}

	void positionWithLayout(UIElement* element, UILayout* layout) {
		// We will distribute the width/height errors to correct some elements to fit
		const int widthError = layout->getColWidthError();
		const int heightError = layout->getRowHeightError();
		const int currentRow = (layout->elementIndices.size() / layout->cols);
		const int currentCol = (layout->elementIndices.size() % layout->cols);

		element->position.x = layout->position.x + currentCol * layout->getColWidth();
		element->position.y = layout->position.y + currentRow * layout->getRowHeight();

		// Distribute column width error
		if (currentCol < widthError) {
			element->width++;

			if (currentCol > 0) {
				element->position.x++;
			}
		}
		else {
			element->position.x += widthError;
		}

		// Distribute row height error
		if (currentRow < heightError) {
			element->height++;

			if (currentRow > 0) {
				element->position.y++;
			}
		}
		else {
			element->position.y += heightError;
		}

		layout->elementIndices.push_back(g_UIElementIndex);
	}

	UILayout* createLayout(int rows, int cols, int width, int height, UILayout* parentLayout /*= nullptr*/) {
		UILayout* layout = (UILayout*)g_UIElements.emplace_back(std::make_unique<UILayout>()).get();
		layout->position = g_CursorOrigin;
		layout->rows = rows;
		layout->cols = cols;
		
		if (width == 0 && height == 0) {
			if (parentLayout != nullptr) {
				layout->width = parentLayout->getColWidth();
				layout->height = parentLayout->getRowHeight();
			}
		}
		else {
			layout->width = width;
			layout->height = height;
		}
		
		if (parentLayout != nullptr) {
			positionWithLayout(layout, parentLayout);
		}

		return layout;
	}

	UIButton* createButton(const std::string& text, int width, int height, UILayout* layout /*= nullptr*/) {
		UIButton* button = (UIButton*)g_UIElements.emplace_back(std::make_unique<UIButton>()).get();
		button->position = g_CursorOrigin;
		button->text = text;

		if (width == 0 && height == 0) {
			if (layout != nullptr) {
				button->width = layout->getColWidth();
				button->height = layout->getRowHeight();
			}
			else {

			}
		}
		else {
			button->width = width;
			button->height = height;
		}
		if (layout != nullptr) {
			positionWithLayout(button, layout);
		}

		return button;
	}

	UILabel* createLabel(const std::string& text, UILayout* layout /*= nullptr*/) {
		UILabel* label = (UILabel*)g_UIElements.emplace_back(std::make_unique<UILabel>()).get();
		label->position = g_CursorOrigin;
		label->width = g_DefaultFont.calcTextWidth(text);
		label->height = g_DefaultFont.boundingBoxHeight;
		label->text = text;

		if (layout != nullptr) {
			positionWithLayout(label, layout);
		}

		return label;
	}

	void UILayout::draw(GraphicsDevice& device) {
		if (backgroundColor.a != 0.0f) {
			drawRect(position, width, height, backgroundColor, nullptr);
		}
	}

	void UILayout::processEvent(const UIEvent& event) {

	}

	void UIButton::draw(GraphicsDevice& device) {
		const glm::vec2 buttonCenter = position + glm::vec2(width / 2, height / 2);
		drawRect(position, width, height, m_DisplayColor, nullptr);

		if (!text.empty()) {
			drawText(text, buttonCenter, UI_COLOR_TEXT_PRIMARY, g_DefaultFont, UIPosFlag::HCENTER | UIPosFlag::VCENTER);
		}
	}

	void UIButton::processEvent(const UIEvent& event) {
		switch (event.getType()) {
		case UIEventType::MouseEnter:
			{
				m_DisplayColor = UI_COLOR_HOVER;
			}
			break;
		case UIEventType::MouseExit:
			{
				m_DisplayColor = UI_COLOR_PRIMARY;
			}
			break;
		}
	}

	void UILabel::draw(GraphicsDevice& device) {
		drawText(text, position, UI_COLOR_TEXT_PRIMARY, g_DefaultFont);
	}

	void UILabel::processEvent(const UIEvent& event) {

	}

	void processEvent(const UIEvent& event) {
		switch (event.getType()) {
		case UIEventType::MouseMove:
			{
				const glm::vec2 mousePos = event.getMouseData().position;
				UIElement* hitElement = nullptr;

				// Iterate over the UI elements backwards
				for (int i = (int)g_UIElements.size() - 1; i >= 0; --i) {
					UIElement* element = g_UIElements[i].get();

					// TODO: Move hit test logic elsewhere (we can also make it faster with an O(log n) runtime
					if (mousePos.x >= element->position.x && mousePos.x < element->position.x + element->width &&
						mousePos.y >= element->position.y && mousePos.y < element->position.y + element->height) {

						hitElement = element;

						if (element != g_CurrentUIElement) {
							element->processEvent({ UIEventType::MouseEnter });
							g_CurrentUIElement = element;
						}

						break;
					}
				}

				if (g_CurrentUIElement != g_LastUIElement && g_LastUIElement != nullptr) {
					g_LastUIElement->processEvent({ UIEventType::MouseExit });
				}

				g_LastUIElement = hitElement;
			}
			break;
		}
	}

}