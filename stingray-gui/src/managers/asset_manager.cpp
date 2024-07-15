#include "asset_manager.hpp"

#include <stdexcept>
#include <unordered_map>

#include "../utility/stb_image.h"
#include "../utility/tiny_gltf.h"

//#include <Windows.h> // temp

namespace sr {
	enum class DataType {
		UNKNOWN,
		IMAGE,
		MODEL,
		SOUND,
	};

	struct AssetInternal {
		Texture texture{};
		Model model{};

		std::vector<uint8_t> data{};
	};

	namespace assetmanager {
		static std::unordered_map<std::string, std::weak_ptr<AssetInternal>> g_Assets;
		static tinygltf::TinyGLTF g_GltfLoader{};

		static const std::unordered_map<std::string, DataType> g_Types = {
			{ "BASIS", DataType::IMAGE },
			{ "KTX2", DataType::IMAGE },
			{ "JPG", DataType::IMAGE },
			{ "JPEG", DataType::IMAGE },
			{ "PNG", DataType::IMAGE },
			{ "BMP", DataType::IMAGE },
			{ "DDS", DataType::IMAGE },
			{ "TGA", DataType::IMAGE },
			{ "QOI", DataType::IMAGE },
			{ "HDR", DataType::IMAGE },
			{ "GLTF", DataType::MODEL },
			{ "WAV", DataType::SOUND },
			{ "OGG", DataType::SOUND },
		};

		Asset loadImage(const std::string& path, std::shared_ptr<AssetInternal> asset, GraphicsDevice& device) {
			int width = 0;
			int height = 0;
			int channels = 0;

			// TODO: For now, all images will be converted to RGBA format, which might not always be desired
			uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

			if (!data) {
				throw std::runtime_error("ASSET ERROR: Failed to load image file!");
			}

			const uint32_t bytesPerPixel = static_cast<uint32_t>(STBI_rgb_alpha);

			TextureInfo textureInfo{};
			textureInfo.width = width;
			textureInfo.height = height;
			textureInfo.format = Format::R8G8B8A8_UNORM;
			textureInfo.bindFlags = BindFlag::SHADER_RESOURCE;

			SubresourceData subresourceData{};
			subresourceData.data = data;
			subresourceData.rowPitch = static_cast<uint32_t>(width) * bytesPerPixel;

			device.createTexture(textureInfo, asset->texture, &subresourceData);

			stbi_image_free(data);

			Asset outAsset{};
			outAsset.internalState = asset;
			return outAsset;
		}

		/* Model Loading */
		Asset loadModel(const std::string& path, std::shared_ptr<AssetInternal> asset, GraphicsDevice& device) {
			// TODO: Loading models is a big question mark in this engine, because at some point
			// we will use our own model format. But that is at the time of writing not something
			// that is of high importance. Just try to keep in mind that this will very likely change.
			// For now however, we will only be using GLTF.

			tinygltf::Model gltfModel = {};
			std::string error = {};
			std::string warning = {};

			//char fullPath[128] = {};

			//GetFullPathNameA(path.c_str(), 128, fullPath, nullptr);

			if (!g_GltfLoader.LoadASCIIFromFile(&gltfModel, &error, &warning, path)) {
				throw std::runtime_error("GLTF ERROR: Failed to load GLFT model!");
			}

			asset->model.meshes.resize(gltfModel.meshes.size());
			//asset->model.materialTextures.resize(gltfModel.images.size());

			// Load material textures
			//for (size_t i = 0; i < gltfModel.images.size(); ++i) {
			//	const tinygltf::Image& gltfImage = gltfModel.images[i];

			//	const TextureInfo textureInfo = {
			//		.width = static_cast<uint32_t>(gltfImage.width),
			//		.height = static_cast<uint32_t>(gltfImage.height),
			//		.format = Format::R8G8B8A8_UNORM,
			//		.usage = Usage::DEFAULT,
			//		.bindFlags = BindFlag::SHADER_RESOURCE
			//	};

			//	const SubresourceData textureSubresource = {
			//		.data = gltfImage.image.data(),
			//		.rowPitch = 4U * static_cast<uint32_t>(gltfImage.width),
			//	};

			//	device.createTexture(textureInfo, asset->model.materialTextures[i], &textureSubresource);
			//}

			uint32_t baseVertex = 0;
			uint32_t baseIndex = 0;

			// Load model data
			for (size_t i = 0; i < gltfModel.meshes.size(); ++i) {
				tinygltf::Mesh gltfMesh = gltfModel.meshes[i];
				Mesh& mesh = asset->model.meshes[i];
				mesh.baseVertex = baseVertex;
				mesh.baseIndex = baseIndex;

				// TODO: We should probably support meshes having more than 1 primitive,
				// as of right now, we do not...
				for (size_t j = 0; j < gltfMesh.primitives.size(); ++j) {
					tinygltf::Primitive& gltfPrimitive = gltfMesh.primitives[j];

					// Position
					const tinygltf::Accessor& posAccessor = gltfModel.accessors[gltfPrimitive.attributes["POSITION"]];
					const tinygltf::BufferView& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
					const tinygltf::Buffer& posBuffer = gltfModel.buffers[posBufferView.buffer];

					const float* positions = reinterpret_cast<const float*>(
						&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]
						);

					// Normals
					const tinygltf::Accessor& normalAccessor = gltfModel.accessors[gltfPrimitive.attributes["NORMAL"]];
					const tinygltf::BufferView& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
					const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalBufferView.buffer];

					const float* normals = reinterpret_cast<const float*>(
						&posBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]
						);

					// Tangents
					const tinygltf::Accessor& tangentAccessor = gltfModel.accessors[gltfPrimitive.attributes["TANGENT"]];
					const tinygltf::BufferView& tangentBufferView = gltfModel.bufferViews[tangentAccessor.bufferView];
					const tinygltf::Buffer& tangentBuffer = gltfModel.buffers[tangentBufferView.buffer];

					const float* tangents = reinterpret_cast<const float*>(
						&tangentBuffer.data[tangentBufferView.byteOffset + tangentAccessor.byteOffset]
						);

					// Texture coordinates
					const tinygltf::Accessor& texCoordAccessor = gltfModel.accessors[gltfPrimitive.attributes["TEXCOORD_0"]];
					const tinygltf::BufferView& texCoordBufferView = gltfModel.bufferViews[texCoordAccessor.bufferView];
					const tinygltf::Buffer& texCoordBuffer = gltfModel.buffers[texCoordBufferView.buffer];

					const float* texCoords = reinterpret_cast<const float*>(
						&texCoordBuffer.data[texCoordBufferView.byteOffset + texCoordAccessor.byteOffset]
						);

					// Indices
					const tinygltf::Accessor& indicesAccessor = gltfModel.accessors[gltfPrimitive.indices];
					const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indicesAccessor.bufferView];
					const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];

					const uint16_t* indices = reinterpret_cast<const uint16_t*>(
						&indexBuffer.data[indexBufferView.byteOffset + indicesAccessor.byteOffset]
						);

					for (size_t k = 0; k < posAccessor.count; ++k) {
						ModelVertex vertex{};

						vertex.position = {
							positions[k * 3 + 2],
							positions[k * 3 + 1],
							positions[k * 3 + 0]
						};

						vertex.normal = {
							normals[k * 3 + 2],
							normals[k * 3 + 1],
							normals[k * 3 + 0]
						};

						//vertex.tangent = {
						//	tangents[k * 4 + 0],
						//	tangents[k * 4 + 1],
						//	tangents[k * 4 + 2]
						//};

						//vertex.texCoord = {
						//	texCoords[k * 2],
						//	texCoords[k * 2 + 1]
						//};

						asset->model.vertices.push_back(vertex);
					}

					for (size_t k = 0; k < indicesAccessor.count; ++k) {
						asset->model.indices.push_back(static_cast<uint32_t>(indices[k]));
					}

					mesh.numIndices = static_cast<uint32_t>(indicesAccessor.count);

					if (gltfPrimitive.material == -1) {
						continue;
					}

					const tinygltf::Material& material = gltfModel.materials[gltfPrimitive.material];
					//mesh.albedoMapIndex = material.pbrMetallicRoughness.baseColorTexture.index;
					//mesh.normalMapIndex = material.normalTexture.index;
				}

				baseVertex = static_cast<uint32_t>(asset->model.vertices.size());
				baseIndex = static_cast<uint32_t>(asset->model.indices.size());

			}

			// Create related buffers for the GPU
			const BufferInfo vertexBufferInfo = {
				.size = asset->model.vertices.size() * sizeof(ModelVertex),
				.stride = sizeof(ModelVertex),
				.usage = Usage::DEFAULT,
				.bindFlags = BindFlag::VERTEX_BUFFER | BindFlag::SHADER_RESOURCE, // TODO: This is hardcoded here for Ray-Tracing shader, should probably be altered
				.miscFlags = MiscFlag::BUFFER_STRUCTURED
			};

			const BufferInfo indexBufferInfo = {
				.size = asset->model.indices.size() * sizeof(uint32_t),
				.stride = sizeof(uint32_t),
				.usage = Usage::DEFAULT,
				.bindFlags = BindFlag::INDEX_BUFFER | BindFlag::SHADER_RESOURCE, // TODO: This is hardcoded here for Ray-Tracing shader, should probably be altered
				.miscFlags = MiscFlag::BUFFER_STRUCTURED
			};

			device.createBuffer(vertexBufferInfo, asset->model.vertexBuffer, asset->model.vertices.data());
			device.createBuffer(indexBufferInfo, asset->model.indexBuffer, asset->model.indices.data());

			Asset outAsset = {};
			outAsset.internalState = asset;
			return outAsset;
		}

		Asset loadFromFile(const std::string& path, GraphicsDevice& device) {
			std::weak_ptr<AssetInternal>& weakAsset = g_Assets[path];
			std::shared_ptr<AssetInternal> asset = weakAsset.lock();

			if (asset == nullptr) { // new asset added
				asset = std::make_shared<AssetInternal>();
				g_Assets[path] = asset;
			}

			// Determine asset type
			const size_t extensionPos = path.find_last_of('.');

			if (extensionPos == std::string::npos || extensionPos == 0) {
				throw std::runtime_error("ASSET ERROR: Could not find file extension!");
			}

			std::string extension = path.substr(extensionPos + 1);
			for (size_t i = 0; i < extension.length(); ++i) {
				extension[i] = std::toupper(extension[i]);
			}

			const auto typeSearch = g_Types.find(extension);

			if (typeSearch == g_Types.end()) {
				throw std::runtime_error("ASSET ERROR: File type not supported!");
			}

			const DataType dataType = typeSearch->second;

			switch (dataType) {
			case DataType::IMAGE:
				return loadImage(path, asset, device);
			case DataType::MODEL:
				return loadModel(path, asset, device);
			}

			return Asset();
		}
	}
	Model& Asset::getModel() const {
		AssetInternal* assetInternal = (AssetInternal*)internalState.get();
		return assetInternal->model;
	}

	Texture& Asset::getTexture() const {
		AssetInternal* assetInternal = (AssetInternal*)internalState.get();
		return assetInternal->texture;
	}
}