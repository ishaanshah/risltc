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
#include "main.h"
#include "vulkan_basics.h"

void reset_timer_buffer();

//! Invoke this function exactly once per frame to record the current time.
//! Only then the other functions defined in this header will be available.
void record_frame_time(uint32_t swapchain_index, VkQueryPool pool, VkDevice device, float ts_period, FILE* timings, uint32_t accum_num);


//! Retrieves the current estimate of the frame time in seconds. It is the
//! median of a certain number of previously recorded frame times.
float get_frame_time(uint32_t get_last);


//! Prints the current estimate of the total frame time periodically, namely
//! once per given time interval (assuming that this function is invoked each
//! frame)
void print_frame_time(float interval_in_seconds);
