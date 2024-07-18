#pragma once

#include "../data/font.hpp"
#include "../data/model.hpp"
#include "../rendering/device.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sr {
	struct Asset {
		std::shared_ptr<void> internalState = nullptr;

		Model& getModel() const;
		Texture& getTexture() const;
	};

	namespace assetmanager {
		Asset loadFromFile(const std::string& path, GraphicsDevice& device);
	}
}