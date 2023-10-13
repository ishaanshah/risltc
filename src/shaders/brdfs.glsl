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


#include "math_constants.glsl"

//! All information about a shadoing point that is needed to perform shading
//! using world space coordinates
struct shading_data_t {
	//! The position of the shading point in world space
	vec3 position;
	//! The normalized world space shading normal
	vec3 normal;
	//! The normalized world-space direction towards the eye (or the outgoing
	//! light direction if global illumination is considered)
	vec3 outgoing;
	//! dot(normal, outgoing)
	float lambert_outgoing;
	//! The RGB diffuse albedo. Dependent on the BRDF an additional direction-
	//! dependent factor may come on top of that
	vec3 diffuse_albedo;
	//! The color of specular reflection at zero degrees inclination
	vec3 fresnel_0;
	//! Roughness coefficient for the GGX distribution of normals
	float roughness;
};


/*! An implementation of the Schlick approximation for the Fresnel term.*/
vec3 fresnel_schlick(vec3 fresnel_0, vec3 fresnel_90, float cos_theta) {
	float flipped = 1.0f - cos_theta;
	float flipped_squared = flipped * flipped;
	return fresnel_0 + (fresnel_90 - fresnel_0) * (flipped_squared * flipped * flipped_squared);
}


/*! Evaluates the full BRDF with both diffuse and specular terms (unless they
	are disabled by the given booleans). The diffuse BRDF is Disney diffuse.
	The specular BRDF is Frostbite specular, i.e. a microfacet BRDF with GGX
	normal distribution function, Smith masking-shadowing function and Fresnel-
	Schlick approximation. The material model as a whole, agrees with the model
	proposed for Frostbite:
	https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
	https://dl.acm.org/doi/abs/10.1145/2614028.2615431 */
vec3 evaluate_brdf(shading_data_t data, vec3 incoming_light_direction, bool diffuse, bool specular) {
	// A few computations are shared between diffuse and specular evaluation
	vec3 half_vector = normalize(incoming_light_direction + data.outgoing);
	float lambert_incoming = dot(data.normal, incoming_light_direction);
	float outgoing_dot_half = dot(data.outgoing, half_vector);
	vec3 brdf = vec3(0.0f);

	// Disney diffuse BRDF
	if (diffuse) {
		// CHANGED: to basic lambertian diffuse material
		// float fresnel_90 = fma(outgoing_dot_half * outgoing_dot_half, 2.0f * data.roughness, 0.5f);
		// float fresnel_product =
		// 	fresnel_schlick(vec3(1.0f), vec3(fresnel_90), data.lambert_outgoing).x
		// 	* fresnel_schlick(vec3(1.0f), vec3(fresnel_90), lambert_incoming).x;
		// brdf += fresnel_product * data.diffuse_albedo;
		brdf += data.diffuse_albedo;
	}
	// Frostbite specular BRDF
	if (specular) {
		float normal_dot_half = dot(data.normal, half_vector);
		// Evaluate the GGX normal distribution function
		float roughness_squared = data.roughness * data.roughness;
		float ggx = fma(fma(normal_dot_half, roughness_squared, -normal_dot_half), normal_dot_half, 1.0f);
		ggx = roughness_squared / (ggx * ggx);
		// Evaluate the Smith masking-shadowing function
		float masking = lambert_incoming * sqrt(fma(fma(-data.lambert_outgoing, roughness_squared, data.lambert_outgoing), data.lambert_outgoing, roughness_squared));
		float shadowing = data.lambert_outgoing * sqrt(fma(fma(-lambert_incoming, roughness_squared, lambert_incoming), lambert_incoming, roughness_squared));
		float smith = 0.5f / (masking + shadowing);
		// Evaluate the Fresnel term and put it all together
		vec3 fresnel = fresnel_schlick(data.fresnel_0, vec3(1.0f), clamp(outgoing_dot_half, 0.0f, 1.0f));
		// CHANGED: Convert to grayscale fresnel
		// brdf += ggx * smith * fresnel;
		brdf += ggx * smith * dot(fresnel, vec3(0.21263901f, 0.71516868f, 0.07219232f));
	}
	return brdf * M_INV_PI;
}


//! Overload that evaluates both diffuse and specular
vec3 evaluate_brdf(shading_data_t data, vec3 incoming_light_direction) {
	return evaluate_brdf(data, incoming_light_direction, true, true);
}


