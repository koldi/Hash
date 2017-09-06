/*
 *  Chocobo1/Hash
 *
 *   Copyright 2017 by Mike Tzou (Chocobo1)
 *     https://github.com/Chocobo1/Hash
 *
 *   Licensed under GNU General Public License 3 or later.
 *
 *  @license GPL3 <https://www.gnu.org/licenses/gpl-3.0-standalone.html>
 */

#ifndef CHOCOBO1_SHA2_256_H
#define CHOCOBO1_SHA2_256_H

#include "gsl/span"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>


namespace Chocobo1
{
	// Use these!!
	// SHA2_256();
}


namespace Chocobo1
{
// users should ignore things in this namespace
namespace SHA2_256_Hash
{
	class SHA2_256
	{
		// https://tools.ietf.org/html/rfc6234

		public:
			template <typename T>
			using Span = gsl::span<T>;

			typedef uint8_t Byte;


			inline explicit SHA2_256();

			inline void reset();
			inline SHA2_256& finalize();  // after this, only `toString()`, `toVector()`, `reset()` are available

			inline std::string toString() const;
			inline std::vector<SHA2_256::Byte> toVector() const;

			inline SHA2_256& addData(const Span<const Byte> inData);
			inline SHA2_256& addData(const void *ptr, const long int length);

		private:
			inline void addDataImpl(const Span<const Byte> data);

			const unsigned int BLOCK_SIZE = 64;

			std::vector<Byte> m_buffer;
			uint64_t m_sizeCounter;

			uint32_t m_h[8];
	};


	// helpers
	template <typename T>
	class Loader
	{
		// this class workaround loading data from unaligned memory boundaries
		// also eliminate endianness issues
		public:
			explicit Loader(const void *ptr)
				: m_ptr(static_cast<const uint8_t *>(ptr))
			{
			}

			constexpr T operator[](const size_t idx) const
			{
				static_assert(std::is_same<T, uint32_t>::value, "");
				// handle specific endianness here
				const uint8_t *ptr = m_ptr + (sizeof(T) * idx);
				return  ( (static_cast<T>(*(ptr + 0)) << 24)
						| (static_cast<T>(*(ptr + 1)) << 16)
						| (static_cast<T>(*(ptr + 2)) <<  8)
						| (static_cast<T>(*(ptr + 3)) <<  0));
			}

		private:
			const uint8_t *m_ptr;
	};

	template <typename R, typename T>
	constexpr R ror(const T x, const unsigned int s)
	{
		static_assert(std::is_unsigned<R>::value, "");
		const R mask = -1;
		return ((x >> s) & mask);
	}

	template <typename T>
	constexpr T rotr(const T x, const unsigned int s)
	{
		static_assert(std::is_unsigned<T>::value, "");
		if (s == 0)
			return x;
		return ((x >> s) | (x << ((sizeof(T) * 8) - s)));
	}


	//
	SHA2_256::SHA2_256()
	{
		m_buffer.reserve(BLOCK_SIZE * 2);  // x2 for paddings
		reset();
	}

	void SHA2_256::reset()
	{
		m_buffer.clear();
		m_sizeCounter = 0;

		m_h[0] = 0x6a09e667;
		m_h[1] = 0xbb67ae85;
		m_h[2] = 0x3c6ef372;
		m_h[3] = 0xa54ff53a;
		m_h[4] = 0x510e527f;
		m_h[5] = 0x9b05688c;
		m_h[6] = 0x1f83d9ab;
		m_h[7] = 0x5be0cd19;
	}

	SHA2_256& SHA2_256::finalize()
	{
		m_sizeCounter += m_buffer.size();

		// append 1 bit
		m_buffer.emplace_back(1 << 7);

		// append paddings
		const size_t len = BLOCK_SIZE - ((m_buffer.size() + 8) % BLOCK_SIZE);
		m_buffer.insert(m_buffer.end(), (len + 8), 0);

		// append size in bits
		const uint64_t sizeCounterBits = m_sizeCounter * 8;
		const uint32_t sizeCounterBitsL = ror<uint32_t>(sizeCounterBits, 0);
		const uint32_t sizeCounterBitsH = ror<uint32_t>(sizeCounterBits, 32);
		for (int i = 0; i < 4; ++i)
		{
			m_buffer[m_buffer.size() - 8 + i] = ror<Byte>(sizeCounterBitsH, (8 * (3 - i)));
			m_buffer[m_buffer.size() - 4 + i] = ror<Byte>(sizeCounterBitsL, (8 * (3 - i)));
		}

		addDataImpl(m_buffer);
		m_buffer.clear();

		return (*this);
	}

	std::string SHA2_256::toString() const
	{
		std::string ret;
		const auto v = toVector();
		ret.reserve(2 * v.size());
		for (const auto &i : v)
		{
			char buf[3];
			snprintf(buf, sizeof(buf), "%02x", i);
			ret.append(buf);
		}

		return ret;
	}

	std::vector<SHA2_256::Byte> SHA2_256::toVector() const
	{
		const Span<const uint32_t> state(m_h);
		const int dataSize = sizeof(decltype(state)::value_type);

		std::vector<Byte> ret;
		ret.reserve(dataSize * state.size());
		for (const auto &i : state)
		{
			for (int j = (dataSize - 1); j >= 0; --j)
				ret.emplace_back(ror<Byte>(i, (j * 8)));
		}

		return ret;
	}

	SHA2_256& SHA2_256::addData(const Span<const Byte> inData)
	{
		Span<const Byte> data = inData;

		if (!m_buffer.empty())
		{
			const size_t len = std::min<size_t>((BLOCK_SIZE - m_buffer.size()), data.size());  // try fill to BLOCK_SIZE bytes
			m_buffer.insert(m_buffer.end(), data.begin(), data.begin() + len);

			if (m_buffer.size() < BLOCK_SIZE)  // still doesn't fill the buffer
				return (*this);

			addDataImpl(m_buffer);
			m_buffer.clear();

			data = data.subspan(len);
		}

		const size_t dataSize = data.size();
		if (dataSize < BLOCK_SIZE)
		{
			m_buffer = {data.begin(), data.end()};
			return (*this);
		}

		const size_t len = dataSize - (dataSize % BLOCK_SIZE);  // align on BLOCK_SIZE bytes
		addDataImpl(data.first(len));

		if (len < dataSize)  // didn't consume all data
			m_buffer = {data.begin() + len, data.end()};

		return (*this);
	}

	SHA2_256& SHA2_256::addData(const void *ptr, const long int length)
	{
		// gsl::span::index_type = long int
		return addData({reinterpret_cast<const Byte*>(ptr), length});
	}

	void SHA2_256::addDataImpl(const Span<const Byte> data)
	{
		assert((data.size() % BLOCK_SIZE) == 0);

		m_sizeCounter += data.size();

		for (size_t i = 0, iend = static_cast<size_t>(data.size() / BLOCK_SIZE); i < iend; ++i)
		{
			const Loader<uint32_t> m(reinterpret_cast<const Byte *>(data.data() + (i * BLOCK_SIZE)));

			static const uint32_t kTable[64] =
			{
				0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
				0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
				0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
				0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
				0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
				0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
				0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
				0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
				0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
				0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
				0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
				0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
				0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
				0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
				0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
				0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
			};

			const auto ssig0 = [](const uint32_t x) -> uint32_t
			{
				return (rotr(x, 7) ^ rotr(x, 18) ^ ror<uint32_t>(x, 3));
			};
			const auto ssig1 = [](const uint32_t x) -> uint32_t
			{
				return (rotr(x, 17) ^ rotr(x, 19) ^ ror<uint32_t>(x, 10));
			};
			uint32_t wTable[64];
			for (int t = 0; t < 16; ++t)
				wTable[t] = m[t];
			for (int t = 16; t < 64; ++t)
				wTable[t] = ssig1(wTable[t - 2]) + wTable[t - 7] + ssig0(wTable[t - 15]) + wTable[t - 16];

			uint32_t a = m_h[0];
			uint32_t b = m_h[1];
			uint32_t c = m_h[2];
			uint32_t d = m_h[3];
			uint32_t e = m_h[4];
			uint32_t f = m_h[5];
			uint32_t g = m_h[6];
			uint32_t h = m_h[7];

			const auto round = [&wTable](uint32_t &a, uint32_t &b, uint32_t &c, uint32_t &d, uint32_t &e, uint32_t &f, uint32_t &g, uint32_t &h, const unsigned int t) -> void
			{
				const auto ch = [](const uint32_t x, const uint32_t y, const uint32_t z) -> uint32_t
				{
					return ((x & (y ^ z)) ^ z);  // alternative
				};
				const auto maj = [](const uint32_t x, const uint32_t y, const uint32_t z) -> uint32_t
				{
					return ((x & (y | z)) | (y & z));  // alternative
				};
				const auto bsig0 = [](const uint32_t x) -> uint32_t
				{
					return (rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22));
				};
				const auto bsig1 = [](const uint32_t x) -> uint32_t
				{
					return (rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25));
				};

				const uint32_t t1 = h + bsig1(e) + ch(e, f, g) + kTable[t] + wTable[t];
				const uint32_t t2 = bsig0(a) + maj(a, b, c);

				h = t1;
				d += h;
				h += t2;
			};
			for (int t = 0; t < 8; ++t)
			{
				round(a, b, c, d, e, f, g, h, (8 * t) + 0);
				round(h, a, b, c, d, e, f, g, (8 * t) + 1);
				round(g, h, a, b, c, d, e, f, (8 * t) + 2);
				round(f, g, h, a, b, c, d, e, (8 * t) + 3);
				round(e, f, g, h, a, b, c, d, (8 * t) + 4);
				round(d, e, f, g, h, a, b, c, (8 * t) + 5);
				round(c, d, e, f, g, h, a, b, (8 * t) + 6);
				round(b, c, d, e, f, g, h, a, (8 * t) + 7);
			}

			m_h[0] += a;
			m_h[1] += b;
			m_h[2] += c;
			m_h[3] += d;
			m_h[4] += e;
			m_h[5] += f;
			m_h[6] += g;
			m_h[7] += h;
		}
	}
}
	using SHA2_256 = SHA2_256_Hash::SHA2_256;
}

#endif  // CHOCOBO1_SHA2_256_H
