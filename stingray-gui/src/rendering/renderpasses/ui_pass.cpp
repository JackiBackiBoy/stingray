#include "ui_pass.hpp"

#include "../../data/font.hpp"
#include "../../managers/asset_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace sr::uipass {
	static constexpr uint32_t MAX_TEXT_PARAMS = 8192;
	static constexpr uint32_t MAX_UI_PARAMS = 8192;

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

	void drawText(const std::string& text, const glm::vec4 color, const Font& font, UIPosFlag posFlags = UIPosFlag::NONE) {
		float textPosX = g_CursorOrigin.x;
		float textPosY = g_CursorOrigin.y;

		if (has_flag(posFlags, UIPosFlag::HCENTER)) {
			textPosX -= (float)(g_DefaultFont.calcTextWidth(text) / 2);
		}

		for (size_t i = 0; i < text.length(); ++i) {
			const char character = text[i];
			const GlyphData& glyphData = font.glyphs[character];

			if (character == ' ') {
				textPosX += glyphData.advanceX;
				continue;
			}

			if (character == '\n') {
				textPosX = g_CursorOrigin.x;
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

	void drawRect(GraphicsDevice& device, glm::vec4 color, int width, int height, const Texture* texture) {
		UIParams uiParams = {};
		uiParams.color = color;
		uiParams.position.x = g_CursorOrigin.x;
		uiParams.position.y = g_CursorOrigin.y;
		uiParams.size = { width, height };

		if (texture != nullptr) {
			uiParams.texIndex = device.getDescriptorIndex(*texture);
		}

		// I have no fucking idea what this order is, but if it works, it works
		uiParams.texCoords[0] = { 0.0f, 0.0f };
		uiParams.texCoords[1] = { 1.0f, 0.0f };
		uiParams.texCoords[2] = { 1.0f, 1.0f };
		uiParams.texCoords[3] = { 0.0f, 1.0f };

		g_UIParamsData.push_back(uiParams);
	}

	static void initialize(GraphicsDevice& device) {
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
	}

	void onExecute(PassExecuteInfo& executeInfo) {
		GraphicsDevice& device = *executeInfo.device;
		const CommandList& cmdList = *executeInfo.cmdList;
		const FrameInfo& frameInfo = *executeInfo.frameInfo;

		if (!g_Initialized) {
			initialize(device);
			g_Initialized = true;
		}

		float fWidth = static_cast<float>(frameInfo.width);
		float fHeight = static_cast<float>(frameInfo.height);

		// Draw titlebar
		const int sysMenuWidth = 44 * 3;
		g_CursorOrigin = { 0, 0 };
		drawRect(device, { 0.2f, 0.2f, 0.2f, 0.9f }, fWidth, 31, nullptr);

		g_CursorOrigin = { fWidth - sysMenuWidth, 0 };
		drawRect(device, { 1.0f, 1.0f, 1.0f, 1.0f }, 44, 31, &g_MinimizeIcon.getTexture());
		g_CursorOrigin = { fWidth - sysMenuWidth + 44, 0 };
		drawRect(device, { 1.0f, 1.0f, 1.0f, 1.0f }, 44, 31, &g_MaximizeIcon.getTexture());
		g_CursorOrigin = { fWidth - sysMenuWidth + 88, 0 };
		drawRect(device, { 1.0f, 1.0f, 1.0f, 1.0f }, 44, 31, &g_CloseIcon.getTexture());

		g_CursorOrigin = { fWidth / 2, 8 };
		drawText("Stingray", { 1.0f, 1.0f, 1.0f, 1.0f }, g_DefaultFont, UIPosFlag::HCENTER);

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

}