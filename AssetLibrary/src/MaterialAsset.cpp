#include "Asset.hpp"
#include "MaterialAsset.hpp"

#include <nlohmann/json.hpp>

namespace Assets
{

	static TransparencyMode ParseTransparencyMode(const char* mode)
	{
		if (strcmp("opaque", mode) == 0)
			return TransparencyMode::Opaque;

		if (strcmp("transparent", mode) == 0)
			return TransparencyMode::Transparent;

		if (strcmp("masked", mode) == 0)
			return TransparencyMode::Masked;

		return TransparencyMode::Opaque;
	}

	MaterialAssetInfo ParseMaterialAssetInfo(Asset* file)
	{
		MaterialAssetInfo info = {};
		nlohmann::json metadata = nlohmann::json::parse(file->Json);

		for (auto& [key, value] : metadata["textures"].items())
			info.Textures[key] = value;

		const std::string transparency = metadata["transparency"];
		info.Transparency = ParseTransparencyMode(transparency.c_str());

		return info;
	}

	Asset PackMaterial(MaterialAssetInfo* info)
	{
		nlohmann::json metadata;

		metadata["textures"] = info->Textures;

		switch (info->Transparency)
		{
			case TransparencyMode::Opaque:
				metadata["transparency"] = "opaque";
				break;

			case TransparencyMode::Transparent:
				metadata["transparency"] = "transparent";
				break;

			case TransparencyMode::Masked:
				metadata["transparency"] = "masked";
				break;
		}

		const std::string jsonString = metadata.dump();

		Asset file = {};

		file.Type[0] = 'M'; file.Type[1] = 'A';
		file.Type[2] = 'T'; file.Type[3] = 'X';

		file.Json = std::move(jsonString);

		return file;
	}
	
}