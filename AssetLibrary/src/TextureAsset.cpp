#include "Asset.hpp"
#include "TextureAsset.hpp"
#include "Compression.hpp"

#include <nlohmann/json.hpp>

#include <lz4.h>

namespace Assets
{

	static TextureFormat ParseTextureAssetFormat(const char* format)
	{
		if (strcmp(format, "RGBA8") == 0)
			return TextureFormat::RGBA8;

		return TextureFormat::None;
	}

	TextureAssetInfo ParseTextureAssetInfo(Asset* file)
	{
		TextureAssetInfo info = {};
		nlohmann::json metadata = nlohmann::json::parse(file->Json);

		info.Name = metadata["name"];
		info.FileSize = metadata["filesize"];
		info.PixelSize[0] = metadata["width"];
		info.PixelSize[1] = metadata["height"];
		info.PixelSize[2] = metadata["depth"];

		const std::string format = metadata["format"];
		info.Format = ParseTextureAssetFormat(format.c_str());

		const std::string compression = metadata["compression"];
		info.Compression = ParseCompressionMode(compression.c_str());

		return info;
	}

	void UnpackTexture(TextureAssetInfo* info, const uint8_t* src, uint8_t* dst, size_t size)
	{
		if (info->Compression == CompressionMode::LZ4)
		{
			LZ4_decompress_safe((const char*)src, (char*)dst, size, info->FileSize);
			return;
		}

		memcpy(dst, src, size);
	}

	Asset PackTexture(TextureAssetInfo* info, void* data)
	{
		nlohmann::json metadata;

		metadata["format"] = "RGBA8";
		metadata["compression"] = "LZ4";
		metadata["name"] = info->Name;
		metadata["filesize"] = info->FileSize;
		metadata["width"] = info->PixelSize[0];
		metadata["height"] = info->PixelSize[1];
		metadata["depth"] = info->PixelSize[2];

		const std::string jsonString = metadata.dump();

		Asset file = {};

		file.Type[0] = 'T'; file.Type[1] = 'E';
		file.Type[2] = 'X'; file.Type[3] = 'I';

		file.Json = std::move(jsonString);

		int stagingSize = LZ4_compressBound(info->FileSize);
		file.Binary.resize(stagingSize);

		int compressedSize = LZ4_compress_default((const char*)data, (char*)file.Binary.data(), info->FileSize, stagingSize);
		file.Binary.resize(compressedSize);

		return file;
	}

}