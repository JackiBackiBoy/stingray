#pragma once

#include "../rendering/device.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace sr {
	struct GlyphData {
		unsigned int width = 0U;
		unsigned int height = 0U;
		int bearingX = 0;
		int bearingY = 0;
		int advanceX = 0;
		int advanceY = 0;
		glm::vec2 texCoords[4] = {};
	};

	// NOTE: Right now we only support ASCII characters,
	// but this will likely have to be expanded in the
	// future to UTF-8 or something else.
	struct Font {
		std::string name = "";
		float size = 0.0f;
		int maxBearingY = 0; // AKA: The height of the glyph above the text-baseline
		int boundingBoxHeight = 0;
		int lineSpacing = 0;
		GlyphData glyphs[128] = {};
		Texture fontAtlasTexture = {};

		int calcTextWidth(const std::string& text) const;
	};

	namespace fontloader {
		void loadFromFile(const std::string& path, unsigned int ptSize, Font& font, GraphicsDevice& device);
	}
}