//  Copyright (C) 2021, Christoph Peters, Karlsruhe Institute of Technology
//  Copyright (C) 2023, Ishaan Shah, International Institute of Information Technology, Hyderabad
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.


//! This structure holds all information needed to retrieve a large number of
//! noise values
struct noise_accessor_t {
	uint seed;
	//! A bunch of random bits used to randomize results across frames
	uvec4 random_numbers;
};

uint murmur_hash3_mix(uint hash, uint k) {
    uint c1 = 0xcc9e2d51;
    uint c2 = 0x1b873593;
    uint r1 = 15;
    uint r2 = 13;
    uint m = 5;
    uint n = 0xe6546b64;

    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;

    hash ^= k;
    hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;

    return hash;
}

uint murmur_hash3_finalize(uint hash) {
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

uint hash_wang(uint key) {
	key = (key ^ 61u) ^ (key >> 16u);
	key = key + (key << 3u);
	key = key ^ (key >> 4u);
	key = key * 0x27D4EB2Du;
	key = key ^ (key >> 15u);
	return key;
}

uint rand_lcg(uint rng_state) {
    // LCG values from Numerical Recipes
    rng_state = 1664525 * rng_state + 1013904223;
    return rng_state;
}

float get_noise_gen(inout uint seed) {
	seed = rand_lcg(seed);
	return seed * (1.0 / 4294967296.0);
}

//! Returns a noise accessor providing access to the first available noise
//! values in the sequence for the current frame and the given pixel.
//! Parameters forward to get_noise_sample().
noise_accessor_t get_noise_accessor(
	uvec2 pixel, uvec2 screen_resolution, uvec4 noise_random_numbers
) {
	noise_accessor_t result;
    uint index = murmur_hash3_mix(0, pixel.x + pixel.y * screen_resolution.x);
	result.seed = murmur_hash3_finalize(murmur_hash3_mix(index, noise_random_numbers.x));
	return result;
}

//! Retrieves the next two noise values and advances the accessor
vec2 get_noise_2(inout noise_accessor_t accessor) {
	return vec2(get_noise_gen(accessor.seed), get_noise_gen(accessor.seed));
}


//! Retrieves the next noise value and advances the accessor
float get_noise_1(inout noise_accessor_t accessor) {
	return get_noise_gen(accessor.seed);
}
