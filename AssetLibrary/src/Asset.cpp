#include "Asset.hpp"

#include <fstream>

namespace Assets
{

	bool LoadBinary(const char* path, Asset& file)
	{
		std::ifstream f(path, std::ios::in | std::ios::binary | std::ios::beg);

		if (!f.is_open()) return false;

		f.read(file.Type, 4);
		f.read((char*)&file.Version, sizeof(uint32_t));

		uint32_t jsonLength;
		f.read((char*)&jsonLength, sizeof(uint32_t));

		uint32_t binaryLength;
		f.read((char*)&binaryLength, sizeof(uint32_t));

		if (jsonLength > 0)
		{
			file.Json.resize(jsonLength);
			f.read(file.Json.data(), jsonLength);
		}

		if (binaryLength > 0)
		{
			file.Binary.resize(binaryLength);
			f.read((char*)file.Binary.data(), binaryLength);
		}

		return true;
	}

	bool SaveBinary(const char* path, const Asset& file)
	{
		std::ofstream f(path, std::ios::out | std::ios::binary);

		if (!f.is_open()) return false;

		f.write(file.Type, 4);
		f.write((const char*)&file.Version, sizeof(uint32_t));

		const uint32_t jsonLength = file.Json.size();
		f.write((const char*)&jsonLength, sizeof(uint32_t));

		const uint32_t binaryLength = file.Binary.size();
		f.write((const char*)&binaryLength, sizeof(uint32_t));

		f.write(file.Json.data(), jsonLength);
		f.write((const char*)file.Binary.data(), binaryLength);

		f.close();

		return true;
	}

}