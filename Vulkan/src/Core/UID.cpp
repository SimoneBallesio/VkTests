#include "Pch.hpp"

#include "Core/UID.hpp"

#include <random>

namespace VKP
{

	static std::random_device s_RandomDevice;
	static std::mt19937_64 eng(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

	uint64_t UID::Invalid = 0;

	UID::UID() : m_UID(s_UniformDistribution(eng)) {}

}