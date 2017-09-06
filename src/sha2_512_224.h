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

#ifndef CHOCOBO1_SHA2_512_224_H
#define CHOCOBO1_SHA2_512_224_H

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
	// SHA2_512_224();
}


namespace Chocobo1
{
// users should ignore things in this namespace
namespace SHA2_512_224_Hash
{
	class SHA2_512_224
	{
	// https://dx.doi.org/10.6028/NIST.FIPS.180-4

	public:
		template <typename T>
		using Span = gsl::span<T>;

		typedef uint8_t Byte;


		inline explicit SHA2_512_224();

		inline void reset();
		inline SHA2_512_224& finalize();  // after this, only `toString()`, `toVector()`, `reset()` are available

		inline std::string toString() const;
		inline std::vector<SHA2_512_224::Byte> toVector() const;

		inline SHA2_512_224& addData(const Span<const Byte> inData);
		inline SHA2_512_224& addData(const void *ptr, const long int length);

	private:
		class Uint128
		{
			public:
				explicit Uint128()
					: m_lo(0), m_hi(0)
				{
				}

				Uint128& operator= (const uint64_t n)
				{
					this->m_lo = n;
					this->m_hi = 0;
					return (*this);
				}

				Uint128& operator* (const unsigned int n)
				{
					// only handle `*8` case
					assert(n == 8);

					const uint8_t msb = (m_lo >> 61) & 0xff;
					m_hi = (m_hi << 3) | msb;
					m_lo = m_lo << 3;

					return (*this);
				}

				Uint128& operator+= (const uint64_t n)
				{
					const uint64_t newLo = (m_lo + n);
					if (newLo < m_lo)
						++m_hi;
					m_lo = newLo;

					return (*this);
				}

				uint64_t low() const
				{
					return m_lo;
				}

				uint64_t high() const
				{
					return m_hi;
				}

			private:
				uint64_t m_lo;
				uint64_t m_hi;
		};

		inline void addDataImpl(const Span<const Byte> data);

		const unsigned int BLOCK_SIZE = 128;

		std::vector<Byte> m_buffer;
		Uint128 m_sizeCounter;

		uint64_t m_h[8];
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
				static_assert(std::is_same<T, uint64_t>::value, "");
				// handle specific endianness here
				const uint8_t *ptr = m_ptr + (sizeof(T) * idx);
				return  ( (static_cast<T>(*(ptr + 0)) << 56)
						| (static_cast<T>(*(ptr + 1)) << 48)
						| (static_cast<T>(*(ptr + 2)) << 40)
						| (static_cast<T>(*(ptr + 3)) << 32)
						| (static_cast<T>(*(ptr + 4)) << 24)
						| (static_cast<T>(*(ptr + 5)) << 16)
						| (static_cast<T>(*(ptr + 6)) <<  8)
						| (static_cast<T>(*(ptr + 7)) <<  0));
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
	SHA2_512_224::SHA2_512_224()
	{
		m_buffer.reserve(BLOCK_SIZE * 2);  // x2 for paddings
		reset();
	}

	void SHA2_512_224::reset()
	{
		m_buffer.clear();
		m_sizeCounter = 0;

		m_h[0] = 0x8C3D37C819544DA2;
		m_h[1] = 0x73E1996689DCD4D6;
		m_h[2] = 0x1DFAB7AE32FF9C82;
		m_h[3] = 0x679DD514582F9FCF;
		m_h[4] = 0x0F6D2B697BD44DA8;
		m_h[5] = 0x77E36F7304C48942;
		m_h[6] = 0x3F9D85A86A1D36C8;
		m_h[7] = 0x1112E6AD91D692A1;
	}

	SHA2_512_224& SHA2_512_224::finalize()
	{
		m_sizeCounter += m_buffer.size();

		// append 1 bit
		m_buffer.emplace_back(1 << 7);

		// append paddings
		const size_t len = BLOCK_SIZE - ((m_buffer.size() + 16) % BLOCK_SIZE);
		m_buffer.insert(m_buffer.end(), (len + 16), 0);

		// append size in bits
		const Uint128 sizeCounterBits = m_sizeCounter * 8;
		const uint64_t sizeCounterBitsL = sizeCounterBits.low();
		const uint64_t sizeCounterBitsH = sizeCounterBits.high();
		for (int i = 0; i < 8; ++i)
		{
			m_buffer[m_buffer.size() - 16 + i] = ror<Byte>(sizeCounterBitsH, (8 * (7 - i)));
			m_buffer[m_buffer.size() - 8 + i] = ror<Byte>(sizeCounterBitsL, (8 * (7 - i)));
		}

		addDataImpl(m_buffer);
		m_buffer.clear();

		return (*this);
	}

	std::string SHA2_512_224::toString() const
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

	std::vector<SHA2_512_224::Byte> SHA2_512_224::toVector() const
	{
		const Span<const uint64_t> state(m_h, 3);
		const int dataSize = sizeof(decltype(state)::value_type);

		std::vector<Byte> ret;
		ret.reserve(dataSize * state.size());
		for (const auto &i : state)
		{
			for (int j = (dataSize - 1); j >= 0; --j)
				ret.emplace_back(ror<Byte>(i, (j * 8)));
		}
		ret.emplace_back(ror<Byte>(m_h[3], 56));
		ret.emplace_back(ror<Byte>(m_h[3], 48));
		ret.emplace_back(ror<Byte>(m_h[3], 40));
		ret.emplace_back(ror<Byte>(m_h[3], 32));

		return ret;
	}

	SHA2_512_224& SHA2_512_224::addData(const Span<const Byte> inData)
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

	SHA2_512_224& SHA2_512_224::addData(const void *ptr, const long int length)
	{
		// gsl::span::index_type = long int
		return addData({reinterpret_cast<const Byte*>(ptr), length});
	}

	void SHA2_512_224::addDataImpl(const Span<const Byte> data)
	{
		assert((data.size() % BLOCK_SIZE) == 0);

		m_sizeCounter += data.size();

		for (size_t iter = 0, iend = static_cast<size_t>(data.size() / BLOCK_SIZE); iter < iend; ++iter)
		{
			const Loader<uint64_t> m(reinterpret_cast<const Byte *>(data.data() + (iter * BLOCK_SIZE)));

			static const uint64_t kTable[80] =
			{
				0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
				0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
				0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
				0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
				0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
				0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
				0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
				0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
				0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
				0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
				0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
				0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
				0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
				0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
				0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
				0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
				0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
				0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
				0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
				0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
			};

			const auto ssig0 = [](const uint64_t x) -> uint64_t
			{
				return (rotr(x, 1) ^ rotr(x, 8) ^ ror<uint64_t>(x, 7));
			};
			const auto ssig1 = [](const uint64_t x) -> uint64_t
			{
				return (rotr(x, 19) ^ rotr(x, 61) ^ ror<uint64_t>(x, 6));
			};
			uint64_t wTable[80];
			for (int t = 0; t < 16; ++t)
				wTable[t] = m[t];
			for (int t = 16; t < 80; ++t)
				wTable[t] = ssig1(wTable[t - 2]) + wTable[t - 7] + ssig0(wTable[t - 15]) + wTable[t - 16];

			uint64_t a = m_h[0];
			uint64_t b = m_h[1];
			uint64_t c = m_h[2];
			uint64_t d = m_h[3];
			uint64_t e = m_h[4];
			uint64_t f = m_h[5];
			uint64_t g = m_h[6];
			uint64_t h = m_h[7];

			const auto round = [&wTable](uint64_t &a, uint64_t &b, uint64_t &c, uint64_t &d, uint64_t &e, uint64_t &f, uint64_t &g, uint64_t &h, const unsigned int t) -> void
			{
				const auto ch = [](const uint64_t x, const uint64_t y, const uint64_t z) -> uint64_t
				{
					return ((x & (y ^ z)) ^ z);  // alternative
				};
				const auto maj = [](const uint64_t x, const uint64_t y, const uint64_t z) -> uint64_t
				{
					return ((x & (y | z)) | (y & z));  // alternative
				};
				const auto bsig0 = [](const uint64_t x) -> uint64_t
				{
					return (rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39));
				};
				const auto bsig1 = [](const uint64_t x) -> uint64_t
				{
					return (rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41));
				};

				const uint64_t t1 = h + bsig1(e) + ch(e, f, g) + kTable[t] + wTable[t];
				const uint64_t t2 = bsig0(a) + maj(a, b, c);

				h = t1;
				d += h;
				h += t2;
			};
			for (int t = 0; t < 10; ++t)
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
	using SHA2_512_224 = SHA2_512_224_Hash::SHA2_512_224;
}

#endif  // CHOCOBO1_SHA2_512_224_H
