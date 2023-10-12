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
#include <stdint.h>

#ifdef __cplusplus
//! To avoid name mangling when this header is included in C++ code for the UI,
//! we need this modifier for functions
#define EXTERN_C extern "C"
#else
//! For C code, the keyword is not needed
#define EXTERN_C
#endif

//! Available methods for sampling polygonal lights
typedef enum sample_polygon_technique_e {
	//! A bogus technique, designed to have practically zero cost. Useful for
	//! comparison of run time measurements
	sample_polygon_baseline,
	//! Uniform sampling over the area of the polygon using Turk's method
	sample_polygon_area_turk,
	//! Sampling proportional to projected solid angle with our method
	sample_polygon_projected_solid_angle,
	//! Sampling proportional to projected solid angle with a biased but less
	//! costly version of our method
	sample_polygon_projected_solid_angle_biased,
	//! Evaluate weight using LTC and final answer using CP
	sample_polygon_ltc_cp,
	//! Number of available polygon sampling techniques
	sample_polygon_count
} sample_polygon_technique_t;


//! Available methods to make use of a texture for computation of the radiance
//! of a polygonal light. In any case, the value from the texture is multiplied
//! onto the uniform surface radiance.
typedef enum polygon_texturing_technique_e {
	//! No texturing is used
	polygon_texturing_none = 0,
	//! Plane space coordinates are used as texture coordinates
	polygon_texturing_area = 1,
	//! The outgoing light direction gets flipped and used for a lookup in a
	//! light probe parameterized through spherical coordinates
	polygon_texturing_portal = 2,
	//! A 1D texture is interpreted as IES profile, i.e. it changes the
	//! emission dependent on the angle
	polygon_texturing_ies_profile = 3,
	//! Number of available polygon texturing methods
	polygon_texturing_count,
	//! Force the use of a 32-bit integer
	polygon_texturing_force_int = 0x7fffffff
} polygon_texturing_technique_t;


/*! This struct represents a convex polygonal light source. The polygon is
	planar but oriented arbitrarily in 3D space. It **doesn't** match the
	layout of the corresponding structure in the shader . */
typedef struct polygonal_light_s {
	float rotation_angles[3];
	float scaling_x;
	float translation[3];
	float scaling_y;
	float radiant_flux[3]; // Not used, kept for legacy purposes
	float inv_scaling_x;
	//! Written by update_polygonal_light()
	float surface_radiance[3];
	float inv_scaling_y;
	float plane[4];
	uint32_t vertex_count;
	polygon_texturing_technique_t texturing_technique;
	uint32_t texture_index;
	uint32_t padding_0;
	float rotation[3][4];
	float area, rcp_area;
	float padding_1[2];
	//! The file path for the used texture or NULL if no texture is used
	char* texture_file_path;
	//! Due to GLSL padding rules, vertex i is at entries 4 * i + 0 and
	//! 4 * i + 1
	float* vertices_plane_space;
	//! Written by update_polygonal_light() but allocated before. Due to GLSL
	//! padding rules, vertex i is at entries 4 * i + 0 to 4 * i + 2.
	float* vertices_world_space;
} polygonal_light_t;

/*! This struct represents a convex polygonal light source. The polygon is
	planar but oriented arbitrarily in 3D space. By design, it matches the
	layout of the corresponding structure in the shader.*/
typedef struct polygonal_light_upload_s {
	//! Written by update_polygonal_light()
	float surface_radiance[3];
	float padding_1;
	float plane[4];
	uint32_t vertex_count;
	float padding_2[3];
} polygonal_light_upload_t;

//! This many bytes at the beginning of the structure polygonal_light_t are
//! stored into a quicksave. After that, there is some data of variable size.
#define POLYGONAL_LIGHT_QUICKSAVE_SIZE (sizeof(float) * 20 + sizeof(uint32_t) * 2)

//! This many bytes at the beginning of the structure polygonal_light_t are
//! written into a light buffer. After that, there is variable size data.
#define POLYGONAL_LIGHT_FIXED_CONSTANT_BUFFER_SIZE (sizeof(float) * 12)


//! Sets the vertex_count member and allocates the appropriate amount of memory
//! for vertices. If vertex_count and vertices_plane_space was not null before,
//! old vertices are copied. New vertices are placed at zero.
//! \return Non-zero value if and only if the vertex count has changed
EXTERN_C int set_polygonal_light_vertex_count(polygonal_light_t* light, uint32_t vertex_count);

//! Updates values of redundant members. Note that vertices_world_space must be
//! already allocated with appropriate size but 
EXTERN_C void update_polygonal_light(polygonal_light_t* light);

//! Returns a deep copy of the given polygonal light
EXTERN_C polygonal_light_t duplicate_polygonal_light(const polygonal_light_t* light);

//! Frees memory and zeros the object
EXTERN_C void destroy_polygonal_light(polygonal_light_t* light);
