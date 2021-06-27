#ifndef PERFHAS_H
#define PERFHAS_H

#include <string>
#include <vector>
#include <set>
#include <algorithm>

namespace util
{
	/// FNVhash function, see http://isthe.com/chongo/tech/comp/fnv/
	inline std::int32_t hash(const wchar_t *label, size_t label_length, std::int32_t d)
	{
		const unsigned char *ptr = (const unsigned char *)label; // operate on octets
		const std::int32_t fnv_prime = 16777619;
		d = d == 0 ? 2166136261 : d;
		for (size_t i = 0; i < label_length * sizeof(wchar_t); i++)
		{
			d *= fnv_prime;
			d ^= (std::int32_t)ptr[i];
		}
		// xor folding described on page linked above
		return d >> 31 ^ (d & 0x7FFFFFFF);
	}

	// Preallocatge offsets to have as many elements as the key set!!!!!!
	void mkPerfectHash(const std::set<std::wstring> &keys, std::int32_t *offsets);

	inline std::uint32_t find(const wchar_t *label, size_t label_length, const std::int32_t *offsets, size_t hash_size)
	{
		// Look up offest
		std::int32_t d = offsets[hash(label, label_length, 0) % hash_size];
		// If negative, compute slot directly, otherwise rehash with correct d
		return (std::uint32_t)(d < 0 ? (-d) - 1 : hash(label, label_length, d) % hash_size);
	}

}

#endif