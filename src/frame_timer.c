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


#include "frame_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLFW/glfw3.h>

//! How many past frame times are used to compute the median
#define FRAME_TIME_COUNT 100

//! A ring buffer of glfwGetTime() values in past invocations of 
//! record_frame_time(). Invalid entries are zero.
static double g_recorded_times[FRAME_TIME_COUNT] = {0.0};
//! The most recently written entry in the ring buffer record_times
static uint32_t g_recorded_time_index = FRAME_TIME_COUNT - 1;

void reset_timer_buffer() {
	g_recorded_time_index = FRAME_TIME_COUNT - 1;
}

void record_frame_time(uint32_t swapchain_index, VkQueryPool pool, VkDevice device, float ts_period, FILE* timings, uint32_t accum_num) {
	uint64_t timestamps[2];
	VkResult result = vkGetQueryPoolResults(device, pool, swapchain_index*2, 2, 2*sizeof(uint64_t), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	if (result == VK_NOT_READY) {
		return;
	} else if (result == VK_SUCCESS) {
		++g_recorded_time_index;
		if (g_recorded_time_index >= FRAME_TIME_COUNT)
			g_recorded_time_index -= FRAME_TIME_COUNT;
		uint64_t timestamp_units = timestamps[1] - timestamps[0];
		float timestamp_ns = (float)timestamp_units / ts_period;
		g_recorded_times[g_recorded_time_index] = timestamp_ns * 1e-9;
		if (timings != NULL) {
			fprintf(timings, "%i,%f\n", accum_num, timestamp_ns * 1e-6);
		}
	} else {
		printf("Failed to record runtime\n");
	}
}

int compare_floats(const void* lhs_pointer, const void* rhs_pointer) {
	float lhs = *((float*) lhs_pointer);
	float rhs = *((float*) rhs_pointer);
	return  (lhs < rhs) ? -1 : ((lhs == rhs) ? 0 : 1);
}


float get_frame_time(uint32_t get_last) {
	// Only return last frame time if that was requested
	if (get_last) return g_recorded_times[g_recorded_time_index % FRAME_TIME_COUNT];

	// List valid frame times from previous frames
	float frame_times[FRAME_TIME_COUNT];
	float recorded_sum = 0.0f;
	uint32_t recorded_count = 0;
	for (int32_t i = 0; i != FRAME_TIME_COUNT - 1; ++i) {
		int32_t lhs = (g_recorded_time_index + FRAME_TIME_COUNT - i) % FRAME_TIME_COUNT;
		if (g_recorded_times[lhs] != 0.0) {
			frame_times[recorded_count] = g_recorded_times[lhs];
			recorded_sum += frame_times[recorded_count];
			++recorded_count;
		}
	}
	if (recorded_count == 0)
		return 0.0f;
	// Sort
	qsort(frame_times, recorded_count, sizeof(frame_times[0]), compare_floats);
	// Compare the median to the mean and warn if there is a big discrepancy
	float median = frame_times[recorded_count / 2];
	float mean = recorded_sum / recorded_count;
	//if (median < mean * 0.96f || median > mean * 1.04f)
	//	printf("Warning: Frame time median is %.2f ms but frame time mean is %.2f ms.\n", median * 1000.0f, mean * 1000.0f);
	// Return the median
	return median;
}


void print_frame_time(float interval_in_seconds) {
	double current_time = g_recorded_times[g_recorded_time_index];
	static double last_print_time = 0.0;
	if (last_print_time == 0.0 || last_print_time + (double)interval_in_seconds < current_time) {
		float frame_time = get_frame_time(0);
		if (frame_time > 0.0f)
			printf("Frame time: %.3f ms\n", frame_time * 1.0e3f);
		last_print_time = current_time;
	}
}
