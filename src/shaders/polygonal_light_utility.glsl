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


//! Constants from polygon_texturing_technique_t
#define polygon_texturing_none 0
#define polygon_texturing_area 1
#define polygon_texturing_portal 2
#define polygon_texturing_ies_profile 3

/*! This struct represents a convex polygonal light source. The polygon is
	planar but oriented arbitrarily in 3D space. Shaders do not care about the
 	plane space or transformation attributes but the C code does.*/
struct polygonal_light_t {
	//! By default the polygon acts as Lambertian emitter, which emits this
	//! radiance at each surface point. The radiance may get scaled by a
	//! texture.
	vec3 surface_radiance;
	//! The dot product between this vector and a point in homogeneous
	//! coordinates is zero, if the point is on the plane of this light
	//! source. plane.xyz has unit length.
	vec4 plane;
	uint vertex_count;
#ifdef MAX_POLYGONAL_LIGHT_VERTEX_COUNT
	//! The 3D vertex locations of the polygon in world space. If vertex_count 
	//! < MAX_POLYGONAL_LIGHT_VERTEX_COUNT, the first vertex is repeated at
	//! that index.
	vec3 vertices_world_space[MAX_POLYGONAL_LIGHT_VERTEX_COUNT];
#endif
};


#ifdef MAX_POLYGONAL_LIGHT_VERTEX_COUNT

/*! This function performs an intersection test for the polygonal light and the
	line segment connecting ray_origin to ray_end. ray_end is given in
	homogeneous coordinates, so if you want a specific point, set w to 1.0f, if
	you want a semi-infinite ray, pass the direction and set w to 0.0f.
	\return true iff an intersection exists.*/
bool polygonal_light_ray_intersection(polygonal_light_t light, vec3 ray_origin, vec4 ray_end) {
	// Check whether the ray begins and ends on opposite sides of the plane
	if (dot(light.plane, vec4(ray_origin, 1.0f)) * dot(light.plane, ray_end) > 0.0f)
		return false;
	// Check whether the ray is on the same side of each edge
	vec3 ray_dir = ray_end.xyz - ray_end.w * ray_origin;
	float previous_sign = 0.0f;
	bool result = true;
	[[unroll]]
	for (uint i = 0; i != MAX_POLYGONAL_LIGHT_VERTEX_COUNT; ++i) {
		float sign = determinant(mat3(
			ray_dir,
			light.vertices_world_space[i] - ray_origin,
			light.vertices_world_space[(i + 1) % MAX_POLYGONAL_LIGHT_VERTEX_COUNT] - ray_origin
		));
		result = result && ((i >= 3 && i >= light.vertex_count) || previous_sign * sign >= 0.0f);
		previous_sign = sign;
	}
	return result;
}

#endif
