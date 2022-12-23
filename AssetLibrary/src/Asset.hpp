#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace Assets
{

	struct Asset
	{
		char Type[4] = {};
		uint32_t Version = 1;
		std::string Json = "";
		std::vector<uint8_t> Binary = {};
	};

	bool LoadBinary(const char* path, Asset& file);
	bool SaveBinary(const char* path, const Asset& file);

}