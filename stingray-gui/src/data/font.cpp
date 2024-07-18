#include "font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/freetype.h>

#include <stdexcept>

namespace sr {

	int Font::calcTextWidth(const std::string& text) {
		// Edge case
		if (text.length() == 1) {
			return glyphs[text[0]].width;
		}

		int result = 0;
		for (size_t i = 0; i < text.length(); ++i) {
			GlyphData& glyphData = glyphs[text[i]];

			// For the last character we want its full width including bearing
			if (i == text.length() - 1) {
				result += glyphData.bearingX + glyphData.width;
				continue;
			}

			result += glyphData.advanceX;

			// We disregard the bearing of the first character
			if (i == 0) {
				result -= glyphData.bearingX;
			}
		}

		return result;
	}

	namespace fontloader {

		void loadFromFile(const std::string& path, unsigned int ptSize, Font& font, GraphicsDevice& device) {
			font = {};

			FT_Library ft = {};
			if (FT_Init_FreeType(&ft)) {
				throw std::runtime_error("FREETYPE ERROR: Failed to initialize the FreeType library!");
			}

			FT_Face face = {};
			if (FT_New_Face(ft, path.c_str(), 0, &face)) {
				throw std::runtime_error("FREETYPE ERROR: Failed to load font!");
			}

			FT_Set_Char_Size(face, 0, ptSize * 64, 0, 0);
			font.lineSpacing = face->size->metrics.height >> 6;

			const bool haskerning = FT_HAS_KERNING(face);
			const unsigned int padding = 4;
			const unsigned int numGlyphs = 126 - 32; // NOTE: Number of visible glyphs in atlas (does not include space character)
			unsigned int atlasOffsetX = padding;
			unsigned int atlasOffsetY = padding;

			unsigned int atlasWidth = padding;
			unsigned int atlasHeight = padding;

			// Calculate atlas dimensions
			for (unsigned char c = 33; c < 127; ++c) {
				if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
					throw std::runtime_error("FREETYPE ERROR: Failed to load glyph!");
				}

				FT_Bitmap* bmp = &face->glyph->bitmap;

				atlasWidth += bmp->width + padding;
				atlasHeight += bmp->rows + padding;
			}

			float f = ceilf(sqrtf((float)numGlyphs));
			float t = ceilf(log2f((float)std::max(atlasWidth, atlasHeight) / ceilf(sqrtf((float)numGlyphs))));
			unsigned int maxDim = 1 << (unsigned int)(1 + ceilf(log2f((float)std::max(atlasWidth, atlasHeight) / ceilf(sqrtf((float)numGlyphs)))));
			atlasWidth = maxDim;
			atlasHeight = maxDim;

			uint8_t* atlasPixels = (uint8_t*)calloc(static_cast<size_t>(atlasWidth * atlasHeight), 1);
			unsigned int tallestCharInRow = 0;

			// Fill out the atlas
			for (unsigned char c = 32; c < 127; ++c) {
				if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
					throw std::runtime_error("FREETYPE ERROR: Failed to load glyph!");
				}

				FT_Bitmap* bmp = &face->glyph->bitmap;
				GlyphData& glyph = font.glyphs[c];
				glyph.width = bmp->width;
				glyph.height = bmp->rows;
				glyph.bearingX = face->glyph->bitmap_left;
				glyph.bearingY = face->glyph->bitmap_top;
				glyph.advanceX = face->glyph->advance.x >> 6;
				glyph.advanceY = face->glyph->advance.y >> 6;

				if (bmp->rows > tallestCharInRow) {
					tallestCharInRow = bmp->rows;
				}

				// Max bearing
				if (glyph.bearingY > font.maxBearingY) {
					font.maxBearingY = glyph.bearingY;
				}

				if (c == ' ') { // we do not have to store 'white-space' in the atlas
					continue;
				}

				if (atlasOffsetX + bmp->width >= atlasWidth - padding) {
					atlasOffsetX = padding;
					atlasOffsetY += tallestCharInRow + padding;
					tallestCharInRow = 0u;
				}

				const float glyphCoordTop = (float)atlasOffsetY / atlasHeight;
				const float glyphCoordLeft = (float)atlasOffsetX / atlasWidth;
				const float glyphCoordBottom = (float)(atlasOffsetY + bmp->rows) / atlasHeight;
				const float glyphCoordRight = (float)(atlasOffsetX + bmp->width) / atlasWidth;

				glyph.texCoords[0] = { glyphCoordLeft, glyphCoordTop }; // top left
				glyph.texCoords[1] = { glyphCoordRight, glyphCoordTop }; // top right
				glyph.texCoords[2] = { glyphCoordRight, glyphCoordBottom }; // bottom right
				glyph.texCoords[3] = { glyphCoordLeft, glyphCoordBottom }; // bottom left

				for (unsigned int y = 0; y < bmp->rows; ++y) {
					for (unsigned int x = 0; x < bmp->width; ++x) {
						assert(atlasOffsetX + x < atlasWidth);
						assert(atlasOffsetY + y < atlasHeight);

						atlasPixels[(atlasOffsetX + x) + (atlasOffsetY + y) * atlasWidth] = bmp->buffer[x + y * bmp->pitch];
					}
				}

				atlasOffsetX += bmp->width + padding;
			}

			FT_Done_FreeType(ft);

			// Create font atlas in GPU memory
			const TextureInfo fontAtlasInfo = {
				.width = static_cast<uint32_t>(atlasWidth),
				.height = static_cast<uint32_t>(atlasHeight),
				.format = Format::R8_UNORM,
				.bindFlags = BindFlag::SHADER_RESOURCE
			};

			const SubresourceData fontAtlasData = {
				.data = atlasPixels,
				.rowPitch = static_cast<uint32_t>(atlasWidth)
			};

			device.createTexture(fontAtlasInfo, font.fontAtlasTexture, &fontAtlasData);

			free(atlasPixels);
		}

	}
}