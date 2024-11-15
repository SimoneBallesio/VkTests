#pragma once

namespace VKP
{

	class UID
	{
	public:
		static uint64_t Invalid;

		UID();
		UID(uint64_t uid) : m_UID(uid) {}
		UID(const UID& other) : m_UID(other.m_UID) {}

		~UID() = default;

		inline operator const uint64_t& () const { return m_UID; }

	private:
		uint64_t m_UID;
	};

}