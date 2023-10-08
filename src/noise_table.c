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


#include "noise_table.h"
#include "string_utilities.h"
#include "math_utilities.h"
#include <stdio.h>


void set_noise_constants(uint32_t resolution_mask[2], uint32_t* texture_index_mask, uint32_t random_numbers[4], noise_table_t* noise, VkBool32 animate_noise) {
	for (uint32_t i = 0; i != 4; ++i)
		random_numbers[i] = animate_noise ? wang_random_number(noise->random_seed * 4 + i) : (i * 0x123456);
	++noise->random_seed;
}
