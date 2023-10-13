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
#extension GL_GOOGLE_include_directive : enable
#include "srgb_utility.glsl"

#define _255 0.003921568627451f

//! Color written to the swapchain image
layout (location = 0) out vec4 g_out_color;

//! The texture with shading information
layout (binding = 0, input_attachment_index = 0) uniform subpassInput g_shading_buffer;

// Whether to take a HDR screenshot
layout (push_constant, std430) uniform readonly pc { uint g_frame_bits; };

void main() {
	g_out_color = subpassLoad(g_shading_buffer);

	// Here is how we support HDR screenshots: Always rendering to an
	// intermediate HDR render target would be wasting resources, since we do
	// not take screenshots each frame. Instead, a HDR screenshot consists of
	// two LDR screenshots holding different bits of halfs.
	if (g_frame_bits > 0) {
		uint mask = (g_frame_bits == 1) ? 0xFF : 0xFF00;
		uint shift = (g_frame_bits == 1) ? 0 : 8;
		uvec2 half_bits = uvec2(packHalf2x16(g_out_color.rg), packHalf2x16(g_out_color.ba));
		g_out_color = vec4(
			((half_bits[0] & mask) >> shift) * (_255),
			((((half_bits[0] & 0xFFFF0000) >> 16) & mask) >> shift) * (_255),
			((half_bits[1] & mask) >> shift) * (_255),
			1.0f
		);
		// We just want to write bits to the render target, not colors. If the
		// graphics pipeline does linear to sRGB conversion for us, we do sRGB
		// to linear conversion here to counter that.
#if OUTPUT_LINEAR_RGB
		g_out_color.rgb = convert_srgb_to_linear_rgb(g_out_color.rgb);
#endif
	}
#if !OUTPUT_LINEAR_RGB
	// if g_frame_bits == 0, we output linear RGB or sRGB as requested
	else
		g_out_color.rgb = convert_linear_rgb_to_srgb(g_out_color.rgb);
#endif
}
