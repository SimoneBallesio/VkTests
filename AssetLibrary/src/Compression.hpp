#pragma once

namespace Assets
{

	enum class CompressionMode
	{
		None = 0,
		LZ4,
	};

	CompressionMode ParseCompressionMode(const char* mode);

}