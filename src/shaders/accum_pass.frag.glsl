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


#version 460

//! Color written to the swapchain image
layout (location = 0) out vec4 g_out_color;

//! The texture with shading information
layout (binding = 0, input_attachment_index = 0) uniform subpassInput g_shading_buffer;

//! The texture with accumulation information
layout (binding = 1, rgba32f) uniform readonly image2D g_accum_buffer;

//! The current sample number
layout (push_constant, std430) uniform readonly pc {
	uint g_accum_num;
	uint g_frame_bits;
};

void main() {
	// Obtain an integer pixel index
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	vec4 prev_color = imageLoad(g_accum_buffer, pixel);

	if (g_frame_bits == 2) {
		// We are capturing the hdr_hi bit in this frame, we should return the hdr_lo
		// image from the previous frame again
		g_out_color = prev_color;
	} else {
		vec4 curr_color = subpassLoad(g_shading_buffer);
		g_out_color.x = fma(prev_color.x, float(g_accum_num), curr_color.x);
		g_out_color.y = fma(prev_color.y, float(g_accum_num), curr_color.y);
		g_out_color.z = fma(prev_color.z, float(g_accum_num), curr_color.z);
		g_out_color.w = fma(prev_color.w, float(g_accum_num), curr_color.w);

		g_out_color /= float(g_accum_num+1);
	}

}
