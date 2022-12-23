#include "Compression.hpp"

#include <cstring>

namespace Assets
{

	CompressionMode ParseCompressionMode(const char* mode)
	{
		if (strcmp(mode, "LZ4") == 0)
			return CompressionMode::LZ4;

		return CompressionMode::None;
	}
	
}