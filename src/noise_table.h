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


#pragma once
#include "vulkan_basics.h"
#include <stdint.h>


/*! This struct holds a texture array providing access to precomputed grids of
	sample points for integration (e.g. blue noise dither arrays).*/
typedef struct noise_table_s {
	//! The next random seed that will be used for randomization of accesses to
	//! the texture array
	uint32_t random_seed;
} noise_table_t;


//! Writes constants that are needed to sample noise from the given table to
//! the given output arrays. If animate_noise is VK_TRUE, the random numbers
//! are different each frame.
void set_noise_constants(uint32_t resolution_mask[2], uint32_t* texture_index_mask, uint32_t random_numbers[4], noise_table_t* noise, VkBool32 animate_noise);
