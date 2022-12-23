#pragma once

#include <string>

namespace Assets
{

	enum class TextureFormat
	{
		None = 0,
		RGBA8,
	};

	enum class CompressionMode;

	struct TextureAssetInfo
	{
		std::string Name = "";
		TextureFormat Format = TextureFormat::RGBA8;
		CompressionMode Compression;
		size_t FileSize = 0;
		uint32_t PixelSize[3] = { 0, 0, 0 };
	};

	struct Asset;

	TextureAssetInfo ParseTextureAssetInfo(Asset* file);

	void UnpackTexture(TextureAssetInfo* info, const uint8_t* src, uint8_t* dst, size_t size);
	Asset PackTexture(TextureAssetInfo* info, void* data);

}