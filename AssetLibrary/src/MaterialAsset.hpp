#pragma once

#include <unordered_map>

namespace Assets
{

	enum TransparencyMode
	{
		Opaque = 0,
		Transparent,
		Masked,
	};

	struct MaterialAssetInfo
	{
		std::unordered_map<std::string, std::string> Textures;
		TransparencyMode Transparency = TransparencyMode::Transparent;
	};

	struct Asset;

	MaterialAssetInfo ParseMaterialAssetInfo(Asset* file);

	Asset PackMaterial(MaterialAssetInfo* info);

}