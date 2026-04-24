// CPython-compatible Mersenne Twister + random.random() (53-bit), matching Python 3's _random.Random.
// Derived from CPython's Modules/_randommodule.c (PSF license) — core twister + init_by_array + genrand_res53.

#ifndef PYTHON_RANDOM_HPP
#define PYTHON_RANDOM_HPP

#include <cstdint>
#include <cstdlib>

struct CPythonRandom {
	static constexpr int N = 624;
	static constexpr int M = 397;
	static constexpr uint32_t MATRIX_A = 0x9908b0dfU;
	static constexpr uint32_t UPPER_MASK = 0x80000000U;
	static constexpr uint32_t LOWER_MASK = 0x7fffffffU;

	int index = N + 1;
	uint32_t state[N];

	void init_genrand(uint32_t s) {
		state[0] = s;
		for (int i = 1; i < N; i++) {
			state[i] = (1812433253U * (state[i - 1] ^ (state[i - 1] >> 30)) + static_cast<uint32_t>(i));
		}
		index = N;
	}

	void init_by_array(const uint32_t *init_key, size_t key_length) {
		init_genrand(19650218U);
		size_t i = 1;
		size_t j = 0;
		size_t k = (static_cast<size_t>(N) > key_length) ? static_cast<size_t>(N) : key_length;
		for (; k != 0; k--) {
			state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1664525U)) + init_key[j] + static_cast<uint32_t>(j);
			i++;
			j++;
			if (i >= static_cast<size_t>(N)) {
				state[0] = state[N - 1];
				i = 1;
			}
			if (j >= key_length) {
				j = 0;
			}
		}
		for (k = N - 1; k != 0; k--) {
			state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1566083941U)) - static_cast<uint32_t>(i);
			i++;
			if (i >= static_cast<size_t>(N)) {
				state[0] = state[N - 1];
				i = 1;
			}
		}
		state[0] = 0x80000000U;
		index = N;
	}

	// Match random.seed(n) for int n >= 0: one 32-bit little-endian limb when n fits.
	void seed_int64(int64_t match_seed) {
		uint64_t u = static_cast<uint64_t>(std::llabs(match_seed));
		uint32_t key[2];
		size_t key_len;
		if (u <= 0xffffffffULL) {
			key[0] = static_cast<uint32_t>(u);
			key_len = 1;
		} else {
			key[0] = static_cast<uint32_t>(u & 0xffffffffULL);
			key[1] = static_cast<uint32_t>(u >> 32);
			key_len = 2;
		}
		init_by_array(key, key_len);
	}

	uint32_t genrand_uint32() {
		uint32_t y;
		static const uint32_t mag01[2] = { 0x0U, MATRIX_A };
		if (index >= N) {
			int kk;
			if (index > N) {
				init_genrand(5489U);
			}
			for (kk = 0; kk < N - M; kk++) {
				y = (state[kk] & UPPER_MASK) | (state[kk + 1] & LOWER_MASK);
				state[kk] = state[kk + M] ^ (y >> 1) ^ mag01[y & 0x1U];
			}
			for (; kk < N - 1; kk++) {
				y = (state[kk] & UPPER_MASK) | (state[kk + 1] & LOWER_MASK);
				state[kk] = state[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1U];
			}
			y = (state[N - 1] & UPPER_MASK) | (state[0] & LOWER_MASK);
			state[N - 1] = state[M - 1] ^ (y >> 1) ^ mag01[y & 0x1U];
			index = 0;
		}
		y = state[index++];
		y ^= (y >> 11);
		y ^= (y << 7) & 0x9d2c5680U;
		y ^= (y << 15) & 0xefc60000U;
		y ^= (y >> 18);
		return y;
	}

	double random_random() {
		uint32_t a = genrand_uint32() >> 5;
		uint32_t b = genrand_uint32() >> 6;
		return (static_cast<double>(a) * 67108864.0 + static_cast<double>(b)) * (1.0 / 9007199254740992.0);
	}
};

#endif
