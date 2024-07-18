#include "ui_pass.hpp"

#include "../../data/font.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace sr::uipass {
	static constexpr uint32_t MAX_TEXT_PARAMS = 8192;

	struct TextParams {
		glm::vec4 color = {};
		glm::vec2 position = {};
		glm::vec2 size = {};
		glm::vec2 texCoords[4] = {};
	};

	struct TextPushConstant {
		glm::mat4 uiProjection = { 1.0f };
		uint32_t texIndex = 0;
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	static Shader g_TextVertexShader = {};
	static Shader g_TextPixelShader = {};
	static BlendState g_TextBlendState = {};
	static Pipeline g_TextPipeline = {};
	static TextPushConstant g_TextPushConstant = {};
	static Buffer g_TextParamsBuffers[GraphicsDevice::NUM_BUFFERS] = {};
	static std::vector<TextParams> g_TextParamsData = {};

	static glm::vec2 g_DefaultCursorOrigin = { 8, 8 };
	static glm::vec2 g_CursorOriginReturn = g_DefaultCursorOrigin; // NOTE: Used when 'sameLine' function is used
	static glm::vec2 g_CursorOrigin = g_DefaultCursorOrigin;
	static glm::vec2 g_CursorOriginStep = { 0, 0 };
	static bool g_SameLineActive = false;
	static Font g_DefaultFont = {};
	static bool g_Initialized = false;

	void drawText(const std::string& text, const glm::vec4 color, const Font& font, UIPosFlag posFlags = UIPosFlag::NONE) {
		if (g_SameLineActive) {
			g_CursorOrigin.x += g_CursorOriginStep.x;
		}
		else {
			g_CursorOrigin.y += g_CursorOriginStep.y;
		}

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

		g_CursorOriginStep.x = textPosX - g_CursorOrigin.x + font.glyphs[text[text.length() - 1]].advanceX;
		g_CursorOriginStep.y = textPosY - g_CursorOrigin.y + font.lineSpacing;
	}

	static void initialize(GraphicsDevice& device) {
		sr::fontloader::loadFromFile("assets/fonts/segoeui.ttf", 16, g_DefaultFont, device);

		device.createShader(ShaderStage::VERTEX, L"assets/shaders/text.vs.hlsl", g_TextVertexShader);
		device.createShader(ShaderStage::PIXEL, L"assets/shaders/text.ps.hlsl", g_TextPixelShader);

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

		g_CursorOrigin.x = fWidth / 2;
		drawText("Stingray (DX12) - Demo Scene", { 0.0f, 0.0f, 0.0f, 1.0f }, g_DefaultFont, UIPosFlag::HCENTER);

		g_TextPushConstant.uiProjection = glm::ortho(0.0f, fWidth, fHeight, 0.0f);
		g_TextPushConstant.texIndex = device.getDescriptorIndex(g_DefaultFont.fontAtlasTexture);

		// Update structured buffers for text and UI
		std::memcpy(g_TextParamsBuffers[device.getBufferIndex()].mappedData, g_TextParamsData.data(), g_TextParamsData.size() * sizeof(TextParams));

		device.bindPipeline(g_TextPipeline, cmdList);
		device.bindResource(g_TextParamsBuffers[device.getBufferIndex()], "g_TextParamsBuffer", g_TextPipeline, cmdList);

		device.pushConstants(&g_TextPushConstant, sizeof(g_TextPushConstant), cmdList);
		device.drawInstanced(6, static_cast<uint32_t>(g_TextParamsData.size()), 0, 0, cmdList);

		g_TextParamsData.clear();
		g_CursorOrigin = g_DefaultCursorOrigin;
		g_CursorOriginStep = { 0, 0 };
		g_SameLineActive = false;
	}

}