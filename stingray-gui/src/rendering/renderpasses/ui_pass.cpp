#include "ui_pass.hpp"

#include "../../data/font.hpp"
#include "../../managers/asset_manager.hpp"
#include "../../math/sr_math.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
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
		uint32_t texIndex = 0;
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
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
	static Asset g_CheckIcon = {};
	static Font g_DefaultFont = {};
	static Font g_DefaultBoldFont = {};

	static glm::vec2 g_DefaultCursorOrigin = { 0, 0 };
	static glm::vec2 g_CursorOriginReturn = g_DefaultCursorOrigin; // NOTE: Used when 'sameLine' function is used
	static glm::vec2 g_CursorOrigin = g_DefaultCursorOrigin;
	static bool g_SameLineActive = false;
	static bool g_Initialized = false;

	void drawText(const std::string& text, const glm::vec2& pos, const glm::vec4 color, const Font* font, UIPosFlag posFlags = UIPosFlag::NONE) {
		float textPosX = pos.x;
		float textPosY = pos.y;

		const float textPosOriginX = textPosX;
		const float textPosOriginY = textPosY;

		if (font == nullptr) {
			font = &g_DefaultFont;
		}

		if (has_flag(posFlags, UIPosFlag::HCENTER)) {
			textPosX -= (float)(font->calcTextWidth(text) / 2);
		}
		if (has_flag(posFlags, UIPosFlag::VCENTER)) {
			textPosY -= (float)(font->maxBearingY / 2);
		}

		for (size_t i = 0; i < text.length(); ++i) {
			const char character = text[i];
			const GlyphData& glyphData = font->glyphs[character];

			if (character == ' ') {
				textPosX += glyphData.advanceX;
				continue;
			}

			if (character == '\n') {
				textPosX = textPosOriginX;
				textPosY += font->lineSpacing;
			}

			TextParams textParams = {};
			textParams.color = color;
			textParams.position.x = i == 0 ? textPosX : textPosX + glyphData.bearingX; // Only use bearing if it's not the first character
			textParams.position.y = textPosY + (font->maxBearingY - glyphData.bearingY);
			textParams.size = { glyphData.width, glyphData.height };
			textParams.texCoords[0] = glyphData.texCoords[0];
			textParams.texCoords[1] = glyphData.texCoords[1];
			textParams.texCoords[2] = glyphData.texCoords[2];
			textParams.texCoords[3] = glyphData.texCoords[3];
			textParams.texIndex = g_Device->getDescriptorIndex(font->fontAtlasTexture);
			g_TextParamsData.push_back(textParams);

			textPosX += glyphData.advanceX;
		}
	}

	void drawRect(const glm::vec2& pos, int width, int height, glm::vec4 color, const Texture* texture = nullptr) {
		if (g_Device == nullptr) {
			return;
		}

		UIParams uiParams = {};
		uiParams.color = color;
		uiParams.position.x = pos.x;
		uiParams.position.y = pos.y;
		uiParams.size = { width, height };

		if (texture != nullptr) {
			// TODO: Breaks for DSV
			uiParams.texIndex = g_Device->getDescriptorIndex(*texture);
		}

		uiParams.texCoords[0] = { 0.0f, 0.0f };
		uiParams.texCoords[1] = { 1.0f, 0.0f };
		uiParams.texCoords[2] = { 1.0f, 1.0f };
		uiParams.texCoords[3] = { 0.0f, 1.0f };

		g_UIParamsData.push_back(uiParams);
	}

	static void initialize(GraphicsDevice& device, RenderGraph& renderGraph, const FrameInfo& frameInfo, Settings& settings) {
		g_Device = &device;
		sr::fontloader::loadFromFile("assets/fonts/segoeui.ttf", 14, g_DefaultFont, device);
		sr::fontloader::loadFromFile("assets/fonts/segoeuib.ttf", 14, g_DefaultBoldFont, device);

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
		g_CheckIcon = sr::assetmanager::loadFromFile("assets/icons/check.png", device);

		// UI
		g_CursorOrigin = { 0, 31 };
		auto mainLayout = createLayout(8, 8, frameInfo.width, frameInfo.height - 31);
		auto leftLayout = createLayout(7, 1, frameInfo.width / 7, frameInfo.height - 31, 0, mainLayout);

		auto statsLayout = createLayout(5, 1, 0, 0, 8, leftLayout);
		statsLayout->backgroundColor = UI_COLOR_PRIMARY1;
		auto statsTitle = createLabel("Info & Statistics", statsLayout, &g_DefaultBoldFont);
		g_FPSCounterLabel = createLabel("FPS: ", statsLayout);
		auto gpuLabel = createLabel("GPU: " + device.getDeviceName(), statsLayout, &g_DefaultFont);

		auto settingsLayout = createLayout(6, 1, 0, 200, 8, leftLayout);
		settingsLayout->backgroundColor = UI_COLOR_PRIMARY1;
		auto settingsTitle = createLabel("Settings", settingsLayout, &g_DefaultBoldFont);
		auto checkbox1 = createCheckBox("Draw Wireframe", nullptr, settingsLayout);
		auto checkbox2 = createCheckBox("Ambient Occlusion", &settings.enableAO, settingsLayout);
		auto checkbox3 = createCheckBox("Shadows", &settings.enableShadows, settingsLayout);
		auto fovSlider = createSliderInt("Vertical FOV", 137, 20, 5, 130, &settings.verticalFOV, settingsLayout);

		auto renderPassesLayout = createLayout(3, 3, 0, 0, 8, leftLayout);
		renderPassesLayout->backgroundColor = UI_COLOR_PRIMARY1;
		auto renderPassesTitle = createLabel("Renderpasses", renderPassesLayout, &g_DefaultBoldFont);

		for (const auto& pass : renderGraph.getPasses()) {
			createLabel(" - " + pass->getName(), renderPassesLayout, &g_DefaultFont);
		}


		// Renderpass overview
		auto positionImage = createImage(&renderGraph.getAttachment("Position")->texture, 0, 0, "Position", mainLayout);
		auto albedoImage = createImage(&renderGraph.getAttachment("Albedo")->texture, 0, 0, "Albedo", mainLayout);
		auto normalTexture = createImage(&renderGraph.getAttachment("Normal")->texture, 0, 0, "Normal", mainLayout);
		auto shadowTexture = createImage(&renderGraph.getAttachment("ShadowMap")->texture, 500, 500, "Shadow Map", mainLayout);
		auto aoImage = createImage(&renderGraph.getAttachment("AmbientOcclusion")->texture, 0, 0, "RTAO", mainLayout);
		auto aoAccumulatedImage = createImage(&renderGraph.getAttachment("AOAccumulation")->texture, 0, 0, "RTAO (Accumulation)", mainLayout);
	}

	void onExecute(PassExecuteInfo& executeInfo, Settings& settings) {
		GraphicsDevice& device = *executeInfo.device;
		RenderGraph& renderGraph = *executeInfo.renderGraph;
		const CommandList& cmdList = *executeInfo.cmdList;
		const FrameInfo& frameInfo = *executeInfo.frameInfo;

		if (!g_Initialized) {
			initialize(device, renderGraph, frameInfo, settings);
			g_Initialized = true;
		}

		const float fWidth = static_cast<float>(frameInfo.width);
		const float fHeight = static_cast<float>(frameInfo.height);

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
		drawRect({ 0, 0 }, fWidth, 31, { 0.13f, 0.13f, 0.135f, 1.0f }, nullptr);
		drawRect({ fWidth - sysMenuWidth, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_MinimizeIcon.getTexture());
		drawRect({ fWidth - sysMenuWidth + 44, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_MaximizeIcon.getTexture());
		drawRect({ fWidth - sysMenuWidth + 88, 0 }, 44, 31, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_CloseIcon.getTexture());
		drawText("Stingray", { fWidth / 2, 8 }, { 1.0f, 1.0f, 1.0f, 1.0f }, &g_DefaultFont, UIPosFlag::HCENTER);

		// Push constants
		g_TextPushConstant.uiProjection = glm::ortho(0.0f, fWidth, fHeight, 0.0f);
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

		int i = layout->getCurrentOccupiedWidth();
		int j = layout->getCurrentOccupiedHeight();

		element->position.x = layout->padding + layout->position.x + layout->getCurrentOccupiedWidth();
		element->position.y = layout->padding + layout->position.y + layout->getCurrentOccupiedHeight();

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

	void UILayout::draw(GraphicsDevice& device) {
		if (backgroundColor.a != 0.0f) {
			drawRect(position, width, height, backgroundColor, nullptr);
		}
	}

	void UILayout::processEvent(const UIEvent& event) {

	}

	int UILayout::getLastColWidth() const {
		if (elementIndices.empty()) {
			return 0;
		}

		return g_UIElements[elementIndices.back()]->width;
	}

	int UILayout::getLastRowHeight() const {
		if (elementIndices.empty()) {
			return 0;
		}

		return g_UIElements[elementIndices.back()]->height;
	}

	int UILayout::getCurrentOccupiedWidth() const {
		if (elementIndices.size() % cols == 0) {
			return 0;
		}

		const size_t currentCol = elementIndices.size() % cols;
		const size_t currentRow = elementIndices.size() / cols;
		const size_t rowStartIndex = currentRow * cols;
		const size_t rowEndIndex = rowStartIndex + currentCol;
		int occupiedWidth = 0;

		for (size_t i = rowStartIndex; i < rowEndIndex; ++i) {
			const UIElement* element = g_UIElements[elementIndices[i]].get();
			occupiedWidth += element->width + element->margin;
		}

		return occupiedWidth;
	}

	int UILayout::getCurrentOccupiedHeight() const {
		if (elementIndices.size() / cols == 0) {
			return 0;
		}

		const size_t currentCol = elementIndices.size() % cols;
		const size_t currentRow = elementIndices.size() / cols;
		const size_t endIndex = currentRow * cols;
		int occupiedHeight = 0;

		for (size_t i = 0; i < endIndex; i += cols) {
			const UIElement* element = g_UIElements[elementIndices[i]].get();
			occupiedHeight += element->height + element->margin;
		}

		return occupiedHeight;
	}

	void UIButton::draw(GraphicsDevice& device) {
		const glm::vec2 buttonCenter = position + glm::vec2(width / 2, height / 2);
		drawRect(position, width, height, m_DisplayColor, nullptr);

		if (!text.empty()) {
			drawText(text, buttonCenter, UI_COLOR_TEXT_PRIMARY, font, UIPosFlag::HCENTER | UIPosFlag::VCENTER);
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
				m_DisplayColor = UI_COLOR_PRIMARY1;
			}
			break;
		case UIEventType::MouseDown:
			{
				m_DisplayColor = UI_COLOR_CLICK;
			}
			break;
		case UIEventType::MouseUp:
			{
				m_DisplayColor = UI_COLOR_HOVER;
			}
			break;
		}
	}

	void UILabel::draw(GraphicsDevice& device) {
		drawText(text, position, UI_COLOR_TEXT_PRIMARY, font);
	}

	void UILabel::processEvent(const UIEvent& event) {

	}

	void UICheckBox::draw(GraphicsDevice& device) {
		drawRect(position, UI_CHECKBOX_SIZE, UI_CHECKBOX_SIZE, UI_COLOR_SECONDARY, nullptr); // border
		drawRect(position + glm::vec2(1, 1), UI_CHECKBOX_SIZE - 2, UI_CHECKBOX_SIZE - 2, UI_COLOR_PRIMARY2, nullptr); // background

		if (outValue != nullptr && *outValue) {
			const Texture* checkTexture = &g_CheckIcon.getTexture();
			drawRect(
				position + glm::vec2(UI_CHECKBOX_SIZE / 2 - 16 / 2),
				16,
				16,
				UI_COLOR_ACCENT,
				checkTexture
			);
		}

		drawText(" " + text, position + glm::vec2(UI_CHECKBOX_SIZE, height / 2), UI_COLOR_TEXT_PRIMARY, font, UIPosFlag::VCENTER);
	}

	void UICheckBox::processEvent(const UIEvent& event) {
		switch (event.getType()) {
		case UIEventType::MouseDown:
		{
			isClicked = true;
		}
		break;
		case UIEventType::MouseUp:
		{
			if (isClicked && outValue != nullptr) {
				*outValue = !(*outValue);
			}

			isClicked = false;
		}
		break;
		}
	}

	void UISliderInt::draw(GraphicsDevice& device) {
		const int sliderAreaWidth = width - 2;
		const int sliderAreaHeight = height - 2;

		drawRect(position, width, height, UI_COLOR_SECONDARY, nullptr);
		drawRect(position + glm::vec2(1), sliderAreaWidth, sliderAreaHeight, UI_COLOR_PRIMARY2, nullptr);

		if (value == nullptr) {
			return;
		}

		const int clampedValue = std::clamp(*value, min, max);
		const int valueRange = std::abs(max - min);
		const float sliderPercentage = (*value - min) / (float)valueRange;

		const glm::vec2 centerPos = position + glm::vec2(width / 2, height / 2);
		const int sliderWidth = sliderPercentage * sliderAreaWidth;

		drawRect(position + glm::vec2(1), sliderWidth, height - 2, UI_COLOR_ACCENT, nullptr);
		drawText(std::to_string(*value), centerPos, UI_COLOR_TEXT_PRIMARY, font, UIPosFlag::HCENTER | UIPosFlag::VCENTER);
		drawText(" " + text, { position.x + width, centerPos.y }, UI_COLOR_TEXT_PRIMARY, font, UIPosFlag::VCENTER);
	}

	void UISliderInt::processEvent(const UIEvent& event) {
		switch (event.getType()) {
		case UIEventType::MouseDrag:
			{
				const int sliderWidth = width - 2; // adjusted to borders
				const int relSliderPos = (int)event.getMouseData().position.x - (int)position.x + 1;

				const float sliderPercentage = (relSliderPos) / (float)sliderWidth;
				const int sliderValue = sr::math::lerp(min, max, sliderPercentage);

				if (value == nullptr) {
					return;
				}
				
				*value = std::clamp(sliderValue, min, max);
			}
			break;
		}
	}

	void UIImage::draw(GraphicsDevice& device) {
		drawRect(position, width, height, glm::vec4(1.0f), texture);

		const glm::vec2 captionBackgroundPos = position + glm::vec2(0, height - g_DefaultFont.boundingBoxHeight - g_DefaultFont.maxBearingY);
		drawRect(captionBackgroundPos, width, g_DefaultFont.boundingBoxHeight + g_DefaultFont.maxBearingY, { 0.0f, 0.0f, 0.0f, 0.6f }, nullptr);

		const glm::vec2 captionPos = position + glm::vec2(width / 2, height - g_DefaultFont.boundingBoxHeight);
		drawText(caption, captionPos, glm::vec4(1.0f), &g_DefaultFont, UIPosFlag::HCENTER | UIPosFlag::VCENTER);
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
		case UIEventType::MouseDrag:
			{
				if (g_CurrentUIElement != nullptr) {
					g_CurrentUIElement->processEvent(event);
				}
			}
			break;
		case UIEventType::MouseDown:
		case UIEventType::MouseUp:
			{
				if (g_CurrentUIElement != nullptr) {
					g_CurrentUIElement->processEvent(event);
				}
			}
			break;
		}
	}

	UILayout* createLayout(int rows, int cols, int width, int height, int padding, UILayout* parentLayout) {
		UILayout* layout = (UILayout*)g_UIElements.emplace_back(std::make_unique<UILayout>()).get();
		layout->position = g_CursorOrigin;
		layout->width = width;
		layout->height = height;
		layout->rows = rows;
		layout->cols = cols;
		layout->padding = padding;

		if (parentLayout != nullptr) {
			if (width == 0) {
				layout->width = parentLayout->getColWidth();
			}

			if (height == 0) {
				layout->height = parentLayout->getRowHeight();
			}

			positionWithLayout(layout, parentLayout);
		}

		++g_UIElementIndex;

		return layout;
	}

	UIButton* createButton(const std::string& text, int width, int height, UILayout* layout, const Font* font) {
		UIButton* button = (UIButton*)g_UIElements.emplace_back(std::make_unique<UIButton>()).get();
		button->position = g_CursorOrigin;
		button->text = text;
		button->font = font != nullptr ? font : &g_DefaultFont;

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

		++g_UIElementIndex;

		return button;
	}

	UILabel* createLabel(const std::string& text, UILayout* layout, const Font* font) {
		UILabel* label = (UILabel*)g_UIElements.emplace_back(std::make_unique<UILabel>()).get();
		label->position = g_CursorOrigin;
		label->font = font != nullptr ? font : &g_DefaultFont;
		label->width = label->font->calcTextWidth(text);
		label->height = label->font->boundingBoxHeight;
		label->text = text;

		if (layout != nullptr) {
			positionWithLayout(label, layout);
		}

		++g_UIElementIndex;

		return label;
	}

	UICheckBox* createCheckBox(const std::string& text, bool* outValue, UILayout* layout, const Font* font) {
		UICheckBox* checkBox = (UICheckBox*)g_UIElements.emplace_back(std::make_unique<UICheckBox>()).get();
		checkBox->position = g_CursorOrigin;
		checkBox->font = font != nullptr ? font : &g_DefaultFont;
		checkBox->width = UI_CHECKBOX_SIZE + checkBox->font->calcTextWidth(" " + text);
		checkBox->height = UI_CHECKBOX_SIZE;
		checkBox->text = text;
		checkBox->outValue = outValue;

		if (layout != nullptr) {
			positionWithLayout(checkBox, layout);
		}

		++g_UIElementIndex;

		return checkBox;
	}

	UISliderInt* createSliderInt(const std::string& text, int width, int height, int min, int max, int* value, UILayout* layout, const Font* font) {
		UISliderInt* sliderInt = (UISliderInt*)g_UIElements.emplace_back(std::make_unique<UISliderInt>()).get();
		sliderInt->position = g_CursorOrigin;
		sliderInt->width = width;
		sliderInt->height = height;
		sliderInt->text = text;
		sliderInt->min = min;
		sliderInt->max = max;
		sliderInt->value = value;
		sliderInt->font = font != nullptr ? font : &g_DefaultFont;

		if (layout != nullptr) {
			positionWithLayout(sliderInt, layout);
		}

		++g_UIElementIndex;

		return sliderInt;
	}

	UIImage* createImage(const Texture* texture, int width, int height, const std::string& caption, UILayout* layout) {
		UIImage* image = (UIImage*)g_UIElements.emplace_back(std::make_unique<UIImage>()).get();
		image->position = g_CursorOrigin;
		image->texture = texture;
		image->caption = caption;

		if (width == 0 && height == 0) {
			if (layout != nullptr) {
				image->width = layout->getColWidth();
				image->height = layout->getRowHeight();
			}
			else {

			}
		}
		else {
			image->width = width;
			image->height = height;
		}

		if (layout != nullptr) {
			positionWithLayout(image, layout);
		}

		++g_UIElementIndex;

		return image;
	}

}