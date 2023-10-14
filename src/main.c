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


#include "main.h"
#include "math_utilities.h"
#include "string_utilities.h"
#include "frame_timer.h"
#include "user_interface.h"
#include "textures.h"
#include "fs.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*! GLFW callbacks do not support passing a user-defined pointer. Thus, we have
	a single static, global pointer to the running application to give them access
	to whatever they need.*/
static application_t* g_glfw_application = NULL;

const char* const g_scene_paths[][4] = {
	{"Bistro Interior", "data/Bistro_interior.vks", "data/Bistro_textures", "data/quicksaves/Bistro_interior.save"},
	{"Bistro Exterior", "data/Bistro_exterior.vks", "data/Bistro_textures", "data/quicksaves/Bistro_exterior.save"},
	{"Zero Day", "data/zeroday.vks", "data/ZeroDay_textures", "data/quicksaves/ZeroDay.save"},
};


/*! Writes the camera and lights of the given scene into its associated
	quicksave file.*/
void quick_save(scene_specification_t* scene) {
	FILE* file = fopen_setvbuf(scene->quick_save_path, "wb");
	if (!file) {
		printf("Quick save failed. Please check path and permissions: %s\n", scene->quick_save_path);
		return;
	}
	fwrite(&scene->camera, sizeof(scene->camera), 1, file);
	uint32_t legacy_count = 0;
	fwrite(&legacy_count, sizeof(uint32_t), 1, file);
	fwrite(&scene->polygonal_light_count, sizeof(uint32_t), 1, file);
	for (uint32_t i = 0; i != scene->polygonal_light_count; ++i) {
		polygonal_light_t* light = &scene->polygonal_lights[i];
		fwrite(light, POLYGONAL_LIGHT_QUICKSAVE_SIZE, 1, file);
		size_t path_size = 0;
		if (light->texture_file_path) {
			path_size = strlen(light->texture_file_path) + 1;
			fwrite(&path_size, sizeof(path_size), 1, file);
			fwrite(light->texture_file_path, sizeof(char), path_size, file);
		}
		else
			fwrite(&path_size, sizeof(path_size), 1, file);
		// Write NULL pointers for backward compatibility
		float* null_pointers[2] = {NULL, NULL};
		fwrite(null_pointers, sizeof(float*), 2, file);
		fwrite(light->vertices_plane_space, sizeof(float), 4 * light->vertex_count, file);
	}
	fclose(file);
}

/*! Loads camera and light sources from the quicksave file specified for the
	given scene and writes them into the scene specification. If the number or
	texturing of lights changes, booleans in the given updates structure (if
	any) are set accordingly.*/
void quick_load(scene_specification_t* scene, application_updates_t* updates) {
	FILE* file = fopen_setvbuf(scene->quick_save_path, "rb");
	if (!file) {
		printf("Failed to load a quick save. Please check path and permissions: %s\n", scene->quick_save_path);
		return;
	}
	// Load the camera
	fread(&scene->camera, sizeof(scene->camera), 1, file);
	// Legacy
	uint32_t legacy_count;
	fread(&legacy_count, sizeof(uint32_t), 1, file);
	// Load polygonal lights
	uint32_t old_polygonal_light_count = scene->polygonal_light_count;
	polygonal_light_t* old_polygonal_lights = scene->polygonal_lights;
	scene->polygonal_light_count = 0;
	fread(&scene->polygonal_light_count, sizeof(uint32_t), 1, file);
	VkBool32 vertex_count_changed = VK_FALSE;
	scene->polygonal_lights = malloc(sizeof(polygonal_light_t) * scene->polygonal_light_count);
	for (uint32_t i = 0; i != scene->polygonal_light_count; ++i) {
		polygonal_light_t* light = &scene->polygonal_lights[i];
		fread(light, POLYGONAL_LIGHT_QUICKSAVE_SIZE, 1, file);
		if (i < old_polygonal_light_count && light->vertex_count != old_polygonal_lights[i].vertex_count)
			vertex_count_changed = VK_TRUE;
		// Quick fix for legacy files
		if (light->scaling_y <= 0.0f) light->scaling_y = light->scaling_x;
		// Read the texture file path (if any)
		size_t path_size = 0;
		fread(&path_size, sizeof(path_size), 1, file);
		light->texture_file_path = NULL;
		if (path_size) {
			light->texture_file_path = malloc(sizeof(char) * path_size);
			fread(light->texture_file_path, sizeof(char), path_size, file);
			if (i < old_polygonal_light_count && old_polygonal_lights[i].texture_file_path != NULL && strcmp(light->texture_file_path, old_polygonal_lights[i].texture_file_path) != 0)
				updates->update_light_textures = VK_TRUE;
		}
		// Read NULL pointers for backward compatibility
		fread(&light->vertices_plane_space, sizeof(float*), 2, file);
		// Allocate and read vertex locations
		set_polygonal_light_vertex_count(light, light->vertex_count);
		fread(light->vertices_plane_space, sizeof(float), 4 * light->vertex_count, file);
	}
	for (uint32_t i = 0; i != old_polygonal_light_count; ++i)
		destroy_polygonal_light(&old_polygonal_lights[i]);
	free(old_polygonal_lights);
	fclose(file);
	if (updates)
		updates->update_light_count |= old_polygonal_light_count != scene->polygonal_light_count || vertex_count_changed;
}


//! Fills the given object with a complete specification of the default scene
void specify_default_scene(scene_specification_t* scene) {
	uint32_t scene_index = scene_zeroday;
	// uint32_t scene_index = scene_bistro_outside;
	// uint32_t scene_index = scene_bistro_inside;
	scene->file_path = copy_string(g_scene_paths[scene_index][1]);
	scene->texture_path = copy_string(g_scene_paths[scene_index][2]);
	scene->quick_save_path = copy_string(g_scene_paths[scene_index][3]);
	first_person_camera_t camera = {
		.near = 0.05f, .far = 1.0e3f,
		.vertical_fov = 0.33f * M_PI_F,
		.rotation_x = 0.43f * M_PI_F,
		.rotation_z = 1.3f * M_PI_F,
		.position_world_space = {-3.0f, -2.0f, 1.65f},
		.speed = 2.0f
	};
	scene->camera = camera;
	// Create a polygonal light
	scene->polygonal_light_count = 1;
	polygonal_light_t default_light;
	memset(&default_light, 0, sizeof(default_light));
	default_light.rotation_angles[0] = M_HALF_PI;
	default_light.scaling_x = default_light.scaling_y = 1.0f;
	default_light.radiant_flux[0] = default_light.radiant_flux[1] = default_light.radiant_flux[2] = 1.0f;
	set_polygonal_light_vertex_count(&default_light, 4);
	default_light.vertices_plane_space[0 * 4 + 0] = 0.0f;
	default_light.vertices_plane_space[0 * 4 + 1] = 0.0f;
	default_light.vertices_plane_space[1 * 4 + 0] = 1.0f;
	default_light.vertices_plane_space[1 * 4 + 1] = 0.0f;
	default_light.vertices_plane_space[2 * 4 + 0] = 1.0f;
	default_light.vertices_plane_space[2 * 4 + 1] = 1.0f;
	default_light.vertices_plane_space[3 * 4 + 0] = 0.0f;
	default_light.vertices_plane_space[3 * 4 + 1] = 1.0f;
	scene->polygonal_lights = malloc(sizeof(default_light));
	scene->polygonal_lights[0] = default_light;
	// Try to quick load. Upon success, it will override the defaults above.
	quick_load(scene, NULL);
}


//! \return The minimal number of vertices in a single polygonal light (3 if
//!		there are no polygonal lights)
uint32_t get_min_polygonal_light_vertex_count(const scene_specification_t* scene_specification) {
	if (!scene_specification->polygonal_light_count) return 3;
	uint32_t minimum = 0x7FFFFFFF;
	for (uint32_t i = 0; i != scene_specification->polygonal_light_count; ++i)
		if (minimum > scene_specification->polygonal_lights[i].vertex_count)
			minimum = scene_specification->polygonal_lights[i].vertex_count;
	return minimum;
}

//! \return The maximal number of vertices in a single polygonal light (3 if
//!		there are no polygonal lights)
uint32_t get_max_polygonal_light_vertex_count(const scene_specification_t* scene_specification) {
	uint32_t maximum = 3;
	for (uint32_t i = 0; i != scene_specification->polygonal_light_count; ++i)
		if (maximum < scene_specification->polygonal_lights[i].vertex_count)
			maximum = scene_specification->polygonal_lights[i].vertex_count;
	return maximum;
}

//! \return The maximal number of vertices in a polygon for a polygonal light
//!		that may occur after clipping
uint32_t get_max_polygon_vertex_count(const scene_specification_t* scene_specification, const render_settings_t* render_settings) {
	uint32_t max_polygonal_light_vertex_count = get_max_polygonal_light_vertex_count(scene_specification);
	switch (render_settings->polygon_sampling_technique) {
	/*
	case sample_polygon_projected_solid_angle:
	case sample_polygon_projected_solid_angle_biased:
	*/
	case sample_polygon_ltc_cp:
		// Each time the polygon is clipped, that may create another vertex
		return max_polygonal_light_vertex_count + 1;
	/*
	case sample_polygon_baseline:
	case sample_polygon_area_turk:
	default:
		return max_polygonal_light_vertex_count;
	*/
	}
}


//! Frees memory and zeros
void destroy_scene_specification(scene_specification_t* scene) {
	free(scene->file_path);
	free(scene->texture_path);
	free(scene->quick_save_path);
	for (uint32_t i = 0; i != scene->polygonal_light_count; ++i)
		destroy_polygonal_light(&scene->polygonal_lights[i]);
	free(scene->polygonal_lights);
	memset(scene, 0, sizeof(*scene));
}


//! Sets render settings to default values
void specify_default_render_settings(render_settings_t* settings) {
	settings->exposure_factor = 1.5f;
	settings->roughness_factor = 1.0f;
	settings->sample_count = 1;
	settings->sample_count_light = 1;
	settings->mis_heuristic = mis_heuristic_optimal_clamped;
	settings->mis_visibility_estimate = 0.5f;
	settings->polygon_sampling_technique = sample_polygon_ltc_cp;
	settings->light_sampling = light_reservoir;
	settings->accum = VK_TRUE;
	settings->show_polygonal_lights = VK_FALSE;
	settings->animate_noise = VK_TRUE;
	settings->v_sync = VK_FALSE;
	settings->show_gui = VK_TRUE;
}


//! Frees objects and zeros
void destroy_render_targets(render_targets_t* render_targets, const device_t* device) {
	destroy_images(&render_targets->targets_allocation, device);
	memset(render_targets, 0, sizeof(*render_targets));
}

//! Creates render targets and associated objects
int create_render_targets(render_targets_t* targets, const device_t* device, const swapchain_t* swapchain) {
	memset(targets, 0, sizeof(*targets));
	VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
	image_request_t image_requests[] = {
		{// depth buffer
			.image_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.extent = {swapchain->extent.width, swapchain->extent.height, 1},
				.mipLevels = 1, .arrayLayers = 1, .samples = 1,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			},
			.view_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
				}
			}
		},
		{// visibility buffer
			.image_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32_UINT,
				.extent = {swapchain->extent.width, swapchain->extent.height, 1},
				.mipLevels = 1, .arrayLayers = 1, .samples = 1,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
			},
			.view_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
				}
			}
		},
		{// shading buffer
			.image_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.extent = {swapchain->extent.width, swapchain->extent.height, 1},
				.mipLevels = 1, .arrayLayers = 1, .samples = 1,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
			},
			.view_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
				}
			}
		},
		{// accum buffer
			.image_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.extent = {swapchain->extent.width, swapchain->extent.height, 1},
				.mipLevels = 1, .arrayLayers = 1, .samples = 1,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT
			},
			.view_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
				}
			}
		}
	};
	targets->target_count = COUNT_OF(image_requests);
	targets->duplicate_count = swapchain->image_count;
	// Duplicate the requests per swapchain image
	image_request_t* all_requests = malloc(sizeof(image_requests) * targets->duplicate_count);
	for (uint32_t i = 0; i != targets->duplicate_count; ++i) {
		memcpy(all_requests + i * targets->target_count, image_requests, sizeof(image_requests));
	}
	if (create_images(&targets->targets_allocation, device, all_requests,
		targets->target_count * targets->duplicate_count, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT))
	{
		printf("Failed to create render targets.\n");
		free(all_requests);
		destroy_render_targets(targets, device);
		return 1;
	}
	free(all_requests);
	targets->targets = (void*) targets->targets_allocation.images;
	
	// Change layout of accumulation buffer to general
	VkImage* to_convert = malloc(sizeof(VkImage) * targets->duplicate_count);
	VkImageLayout* src_layout = malloc(sizeof(VkImageLayout) * targets->duplicate_count);
	VkImageLayout* dst_layout = malloc(sizeof(VkImageLayout) * targets->duplicate_count);
	for (uint32_t i = 0; i != targets->duplicate_count; ++i) { 
		to_convert[i] = targets->targets[i].accum_buffer.image;
		src_layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
		dst_layout[i] = VK_IMAGE_LAYOUT_GENERAL;
	}
	uint32_t result = convert_image_layout(targets->duplicate_count, to_convert, src_layout, dst_layout, device);
	free(to_convert);
	free(src_layout);
	free(dst_layout);
	if (result) {
		printf("Failed to convert accum_buffer to VK_IMAGE_LAYOUT_GENERAL\n");
		return 1;
	}

	return 0;
}


//! Frees objects and zeros
void destroy_constant_buffers(constant_buffers_t* constant_buffers, const device_t* device) {
	if (constant_buffers->data)
		vkUnmapMemory(device->device, constant_buffers->buffers.memory);
	destroy_buffers(&constant_buffers->buffers, device);
	memset(constant_buffers, 0, sizeof(*constant_buffers));
}

//! Allocates constant buffers and maps their memory
int create_constant_buffers(constant_buffers_t* constant_buffers, const device_t* device, const swapchain_t* swapchain, const scene_specification_t* scene_specification, const render_settings_t* render_settings) {
	memset(constant_buffers, 0, sizeof(*constant_buffers));
	// Compute the total size for the constant buffer
	size_t size = sizeof(per_frame_constants_t);
	// Create one constant buffer per swapchain image
	VkBufferCreateInfo constant_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	};
	VkBufferCreateInfo* constant_buffer_infos = malloc(sizeof(VkBufferCreateInfo) * swapchain->image_count);
	for (uint32_t i = 0; i != swapchain->image_count; ++i)
		constant_buffer_infos[i] = constant_buffer_info;
	if (create_aligned_buffers(&constant_buffers->buffers, device, constant_buffer_infos, swapchain->image_count, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, device->physical_device_properties.limits.nonCoherentAtomSize)) {
		printf("Failed to create constant buffers.\n");
		free(constant_buffer_infos);
		destroy_constant_buffers(constant_buffers, device);
		return 1;
	}
	free(constant_buffer_infos);
	// Map the complete memory
    if (vkMapMemory(device->device, constant_buffers->buffers.memory, 0, constant_buffers->buffers.size, 0, &constant_buffers->data)) {
		printf("Failed to map constant buffers.\n");
		destroy_constant_buffers(constant_buffers, device);
		return 1;
	}
	return 0;
}


//! Frees objects and zeros
void destroy_light_textures(images_t* light_textures, const device_t* device) {
	destroy_images(light_textures, device);
}

//! Loads the textures for all polygonal light sources (avoiding duplication)
//! and sets texture indices in the polygonal lights accordingly. Pass NULL for
//! light_textures, if you only want to update indices.
int create_and_assign_light_textures(images_t* light_textures, const device_t* device, scene_specification_t* scene_specification) {
	// Create a list of all file paths, using a default for empty or invalid
	// entries and removing duplicates
	char default_path[] = "data/white.vkt";
	const char** unique_paths = malloc(sizeof(char*) * (scene_specification->polygonal_light_count + 1));
	uint32_t unique_count = 0;
	for (uint32_t i = 0; i != scene_specification->polygonal_light_count; ++i) {
		const char* new_path = scene_specification->polygonal_lights[i].texture_file_path;
		if (!new_path || strlen(new_path) == 0)
			new_path = default_path;
		else {
			// Check if the file exists and resort to the default if it does
			// not
			FILE* file = fopen_setvbuf(new_path, "rb");
			if (file)
				fclose(file);
			else {
				printf("The light texture at path %s does not exist. Using a white texture instead.\n", new_path);
				new_path = default_path;
			}
		}
		// Check if the new path is identical to an existing one
		scene_specification->polygonal_lights[i].texture_index = unique_count;
		for (uint32_t j = 0; j != unique_count; ++j)
			if (strcmp(new_path, unique_paths[j]) == 0)
				// Reuse the existing one
				scene_specification->polygonal_lights[i].texture_index = j;
		// Otherwise allocate a new one
		if (scene_specification->polygonal_lights[i].texture_index == unique_count) {
			unique_paths[unique_count] = new_path;
			++unique_count;
		}
	}
	if (!light_textures) {
		free(unique_paths);
		return 0;
	}
	// Empty descriptor arrays cause some trouble
	if (unique_count == 0) {
		unique_count = 1;
		unique_paths[0] = default_path;
	}
	// Try to load the textures
	int result = load_2d_textures(light_textures, device, unique_count, unique_paths, VK_IMAGE_USAGE_SAMPLED_BIT);
	free(unique_paths);
	return result;
}

//! Writes lights matching the current state of the application to the given
//! memory location
void write_lights(void* data, application_t* app) {
	// Write polygonal lights
	uint32_t max_vertex_count = get_max_polygonal_light_vertex_count(&app->scene_specification);
	size_t offset = 0;
	const uint32_t zero = 0;
	printf("Found %d triangle lights\n", app->scene_specification.polygonal_light_count);
	for (uint32_t i = 0; i != app->scene_specification.polygonal_light_count; ++i) {
		polygonal_light_t* light = &app->scene_specification.polygonal_lights[i];
		// Ensure that redundant attributes (including the texture index) are
		// up to date
		update_polygonal_light(light);
		// Make the structure to upload ready
		polygonal_light_upload_t upload_light = {
			.surface_radiance[0] = light->surface_radiance[0],
			.surface_radiance[1] = light->surface_radiance[1],
			.surface_radiance[2] = light->surface_radiance[2],
			.plane[0] = light->plane[0],
			.plane[1] = light->plane[1],
			.plane[2] = light->plane[2],
			.plane[3] = light->plane[3],
			.vertex_count = light->vertex_count,
		};
		create_and_assign_light_textures(NULL, &app->device, &app->scene_specification);
		// Write fixed-size data
		memcpy(((char*) data) + offset, &upload_light, POLYGONAL_LIGHT_FIXED_CONSTANT_BUFFER_SIZE);
		offset += POLYGONAL_LIGHT_FIXED_CONSTANT_BUFFER_SIZE;

		// Write vertex world space
		memcpy(((char*) data) + offset, light->vertices_world_space, sizeof(float) * 4 * light->vertex_count);
		if (light->vertex_count < max_vertex_count)
			// Repeat the first vertex
			memcpy(((char*) data) + offset + sizeof(float) * 4 * light->vertex_count, light->vertices_world_space, sizeof(float) * 4);
		offset += sizeof(float) * 4 * max_vertex_count;
	}
}

//! Frees objects and zeros
void destroy_light_buffers(light_buffers_t* light_buffers, const device_t* device, VmaAllocator allocator) {
	if(!light_buffers->buffer || !light_buffers->allocation) return;

	vmaDestroyBuffer(allocator, light_buffers->buffer, light_buffers->allocation);
	memset(light_buffers, 0, sizeof(*light_buffers));
}

//! Allocates constant buffers and maps their memory
int create_light_buffers(light_buffers_t* light_buffers, const device_t* device, const swapchain_t* swapchain, const scene_specification_t* scene_specification, application_t *app) {
	memset(light_buffers, 0, sizeof(*light_buffers));
	// Compute the total size for the light buffer
	size_t polygonal_light_size = POLYGONAL_LIGHT_FIXED_CONSTANT_BUFFER_SIZE + sizeof(float) * (12 * get_max_polygonal_light_vertex_count(scene_specification) - 8);
	size_t size = scene_specification->polygonal_light_count * polygonal_light_size;
	if (scene_specification->polygonal_light_count == 0) size += polygonal_light_size;

	// Create a staging buffer on CPU side
	VkBufferCreateInfo staging_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	VmaAllocationCreateInfo staging_alloc_info = { .usage = VMA_MEMORY_USAGE_CPU_ONLY };
	VkBuffer staging_buffer;
	VmaAllocation staging_allocation;
	if (vmaCreateBuffer(app->allocator, &staging_buffer_info, &staging_alloc_info, &staging_buffer, &staging_allocation, NULL)) {
		printf("Failed to create staging buffer\n");
		return 1;
	}

	// Write the data to the staging buffer
	void *data;
	vmaMapMemory(app->allocator, staging_allocation, &data);
	write_lights(data, app);
	vmaUnmapMemory(app->allocator, staging_allocation);

	// Allocate a buffer on the GPU
	light_buffers->size = size;
	VkBufferCreateInfo light_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
	};
	VmaAllocationCreateInfo light_alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
	if (vmaCreateBuffer(app->allocator, &light_buffer_info, &light_alloc_info, &light_buffers->buffer, &light_buffers->allocation, NULL)) {
		printf("Failed to create light buffers.\n");
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		return 1;
	}

	// Copy data to GPU
	// Initialize command pool
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cmd_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = device->command_pool,
		.commandBufferCount = 1,
	};
	if (vkAllocateCommandBuffers(device->device, &cmd_buffer_info, &cmd)) {
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		return 1;
	};

	// Initialize fence to wait for above command buffer to complete
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  		.pNext = NULL,
	};
	VkFence fence;
	if (vkCreateFence(device->device, &fence_info, NULL, &fence)) {
		printf("Failed to create copy fence\n");
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		vkFreeCommandBuffers(device->device, device->command_pool, 1, &cmd);
		return 1;
	}
	
	// Start recording commands
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	if (vkBeginCommandBuffer(cmd, &begin_info)) {
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		vkFreeCommandBuffers(device->device, device->command_pool, 1, &cmd);
		vkDestroyFence(device->device, fence, NULL);
		return 1;
	}

	VkBufferCopy buffer_copy = {
		.dstOffset = 0,
		.srcOffset = 0,
		.size = size,
	};
	vkCmdCopyBuffer(cmd, staging_buffer, light_buffers->buffer, 1, &buffer_copy);

	// End recording commands
	if (vkEndCommandBuffer(cmd)) {
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		vkFreeCommandBuffers(device->device, device->command_pool, 1, &cmd);
		vkDestroyFence(device->device, fence, NULL);
		return 1;
	}

	// Submit commands to queue
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = NULL,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = NULL,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = NULL
	};
	if (vkQueueSubmit(device->queue, 1, &submit_info, fence)) {
		destroy_light_buffers(light_buffers, device, app->allocator);
		vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
		vkFreeCommandBuffers(device->device, device->command_pool, 1, &cmd);
		vkDestroyFence(device->device, fence, NULL);
		return 1;
	}

	// Wait on the fence
	VkResult fence_result;
	do {
		fence_result = vkWaitForFences(app->device.device, 1, &fence, VK_TRUE, 100000000);
	} while (fence_result == VK_TIMEOUT);
	vkResetFences(device->device, 1, &fence);

	// Destroy stuff
	vkDestroyFence(device->device, fence, NULL);
	vmaDestroyBuffer(app->allocator, staging_buffer, staging_allocation);
	
	return 0;
}


//! Frees objects and zeros
void destroy_geometry_pass(geometry_pass_t* pass, const device_t* device) {
	destroy_pipeline_with_bindings(&pass->pipeline, device);
	destroy_shader(&pass->vertex_shader, device);
	destroy_shader(&pass->fragment_shader, device);
	memset(pass, 0, sizeof(*pass));
}

//! Creates Vulkan objects for the geometry pass
int create_geometry_pass(geometry_pass_t* pass, const device_t* device, const swapchain_t* swapchain,
	const scene_t* scene, const constant_buffers_t* constant_buffers, const render_targets_t* render_targets, const render_pass_t* render_pass)
{
	memset(pass, 0, sizeof(*pass));
	pipeline_with_bindings_t* pipeline = &pass->pipeline;
	// Create a pipeline layout for the geometry pass
	VkDescriptorSetLayoutBinding layout_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	};
	descriptor_set_request_t set_request = {
		.stage_flags = VK_SHADER_STAGE_VERTEX_BIT,
		.min_descriptor_count = 1,
		.binding_count = 1,
		.bindings = &layout_binding,
	};
	if (create_descriptor_sets(pipeline, device, &set_request, swapchain->image_count, NULL, 0)) {
		printf("Failed to create a descriptor set for the geometry pass.\n");
		destroy_geometry_pass(pass, device);
		return 1;
	}
	// Write to the descriptor set
	VkDescriptorBufferInfo descriptor_buffer_info = {.offset = 0};
	VkWriteDescriptorSet descriptor_set_write = {
		.dstBinding = 0, .pBufferInfo = &descriptor_buffer_info
	};
	complete_descriptor_set_write(1, &descriptor_set_write, &set_request);
	for (uint32_t i = 0; i != swapchain->image_count; ++i) {
		descriptor_buffer_info.buffer = constant_buffers->buffers.buffers[i].buffer;
		descriptor_buffer_info.range = constant_buffers->buffers.buffers[i].size;
		descriptor_set_write.dstSet = pipeline->descriptor_sets[i];
		vkUpdateDescriptorSets(device->device, 1, &descriptor_set_write, 0, NULL);
	}

	// Compile a vertex and fragment shader
	shader_request_t vertex_shader_request = {
		.shader_file_path = "src/shaders/visibility_pass.vert.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_VERTEX_BIT
	};
	shader_request_t fragment_shader_request = {
		.shader_file_path = "src/shaders/visibility_pass.frag.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT
	};
	if (compile_glsl_shader_with_second_chance(&pass->vertex_shader, device, &vertex_shader_request)) {
		printf("Failed to compile the vertex shader for the geometry pass.\n");
		destroy_geometry_pass(pass, device);
		return 1;
	}
	if (compile_glsl_shader_with_second_chance(&pass->fragment_shader, device, &fragment_shader_request)) {
		printf("Failed to compile the fragment shader for the geometry pass.\n");
		destroy_geometry_pass(pass, device);
		return 1;
	}
	
	// Define the graphics pipeline state
	VkVertexInputBindingDescription vertex_binding = {.binding = 0, .stride = sizeof(uint32_t) * 2};
	VkVertexInputAttributeDescription vertex_attribute = {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_UINT, .offset = 0};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &vertex_attribute
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.primitiveRestartEnable = VK_FALSE,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment_state = {
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment_state,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.blendConstants = {1.0f, 1.0f, 1.0f, 1.0f}
	};
	VkViewport viewport = {
		.x = 0.0f, .y = 0.0f,
		.width = (float)swapchain->extent.width, .height = (float)swapchain->extent.height,
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	VkRect2D scissor = {.extent = swapchain->extent};
	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
		.pScissors = &scissor,
		.pViewports = &viewport
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS
	};
	VkPipelineMultisampleStateCreateInfo multi_sample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = pass->vertex_shader.module,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pass->fragment_shader.module,
			.pName = "main"
		}
	};
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipeline->pipeline_layout,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pRasterizationState = &raster_info,
		.pColorBlendState = &blend_info,
		.pTessellationState = NULL,
		.pMultisampleState = &multi_sample_info,
		.pDynamicState = NULL,
		.pViewportState = &viewport_info,
		.pDepthStencilState = &depth_stencil_info,
		.stageCount = 2,
		.pStages = shader_stages,
		.renderPass = render_pass->render_pass,
		.subpass = 0
	};
	if (vkCreateGraphicsPipelines(device->device, NULL, 1, &pipeline_info, NULL, &pass->pipeline.pipeline)) {
		printf("Failed to create a graphics pipeline for the geometry pass.\n");
		destroy_geometry_pass(pass, device);
		return 1;
	}
	return 0;
}


//! Frees objects and zeros
void destroy_shading_pass(shading_pass_t* pass, const device_t* device) {
	destroy_pipeline_with_bindings(&pass->pipeline, device);
	destroy_shader(&pass->vertex_shader, device);
	destroy_shader(&pass->fragment_shader, device);
	if (pass->light_texture_sampler)
		vkDestroySampler(device->device, pass->light_texture_sampler, NULL);
	memset(pass, 0, sizeof(*pass));
}

//! Creates Vulkan objects for the shading pass
int create_shading_pass(shading_pass_t* pass, application_t* app)
{
	memset(pass, 0, sizeof(*pass));
	// Get lots of short-hands
	const device_t* device = &app->device;
	const swapchain_t* swapchain = &app->swapchain;
	const scene_t* scene = &app->scene;
	const constant_buffers_t* constant_buffers = &app->constant_buffers;
	const render_targets_t* render_targets = &app->render_targets;
	const noise_table_t* noise_table = &app->noise_table;
	const ltc_table_t* ltc_table = &app->ltc_table;
	const light_buffers_t* lights = &app->light_buffers;
	pipeline_with_bindings_t* pipeline = &pass->pipeline;
	// Create a sampler for light textures
	VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.anisotropyEnable = VK_FALSE, .maxAnisotropy = 1,
		.minLod = 0.0f, .maxLod = 3.4e38f,
		// This is the way to go for theta-phi parametrizations
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	};
	if (vkCreateSampler(device->device, &sampler_info, NULL, &pass->light_texture_sampler)) {
		printf("Failed to create a sampler for light textures in the shading pass.\n");
		destroy_shading_pass(pass, device);
		return 1;
	}
	// Create descriptor sets for the shading pass
	uint32_t light_texture_count = app->light_textures.image_count;
	uint32_t light_count = app->scene_specification.polygonal_light_count;
	VkDescriptorSetLayoutBinding layout_bindings[] = {
		{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
		{ .binding = 5},	// Filled below
		{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 2 },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = light_texture_count },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // To store lights
		{ .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
	};
	get_materials_descriptor_layout(&layout_bindings[5], 5, &scene->materials);
	uint32_t binding_count = COUNT_OF(layout_bindings);
	descriptor_set_request_t set_request = {
		.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.min_descriptor_count = 1,
		.binding_count = binding_count,
		.bindings = layout_bindings,
	};
	if (create_descriptor_sets(pipeline, device, &set_request, app->swapchain.image_count, NULL, 0)) {
		printf("Failed to allocate descriptor sets for the shading pass.\n");
		destroy_shading_pass(pass, device);
		return 1;
	}
	// Write to the descriptor sets
	VkDescriptorBufferInfo constant_buffer_info = {.offset = 0};
	VkDescriptorImageInfo visibility_buffer_info = {
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};
	VkDescriptorImageInfo ltc_table_infos[2] = {
		{
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.imageView = ltc_table->texture_arrays.images[0].view,
			.sampler = ltc_table->sampler
		},
		{
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.imageView = ltc_table->texture_arrays.images[1].view,
			.sampler = ltc_table->sampler
		}
	};
	VkDescriptorBufferInfo light_buffer_info = { .offset = 0 };
	VkWriteDescriptorSet descriptor_set_writes[COUNT_OF(layout_bindings)] = {
		{ .dstBinding = 0, .pBufferInfo = &constant_buffer_info },
		{ .dstBinding = 4, .pImageInfo = &visibility_buffer_info },
		{ .dstBinding = 6, .pImageInfo = ltc_table_infos },
		{ .dstBinding = 8, .pBufferInfo = &light_buffer_info },
		{ .dstBinding = 7 },	// Light Textures
		{ .dstBinding = 5 },	// Materials
	};
	// Light textures
	VkDescriptorImageInfo* light_texture_writes = malloc(sizeof(VkDescriptorImageInfo) * light_texture_count);
	for (uint32_t i = 0; i != light_texture_count; ++i) {
		light_texture_writes[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		light_texture_writes[i].imageView = app->light_textures.images[i].view;
		light_texture_writes[i].sampler = pass->light_texture_sampler;
	}
	descriptor_set_writes[4].pImageInfo = light_texture_writes;
	// Materials
	uint32_t material_write_index = 5;
	descriptor_set_writes[material_write_index].pImageInfo = get_materials_descriptor_infos(&descriptor_set_writes[material_write_index].descriptorCount, &scene->materials);
	for (uint32_t i = 0; i != mesh_buffer_count; ++i) {
		VkWriteDescriptorSet write = {
			.dstBinding = i + 1, .pTexelBufferView = &scene->mesh.buffer_views[i]
		};
		descriptor_set_writes[material_write_index + 1 + i] = write;
	}
	VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &app->scene.acceleration_structure.top_level
	};
	VkWriteDescriptorSet acceleration_structure_write = {
		.dstBinding = 9, .pNext = &acceleration_structure_info
	};
	descriptor_set_writes[material_write_index + 1 + mesh_buffer_count] = acceleration_structure_write;
	complete_descriptor_set_write(binding_count, descriptor_set_writes, &set_request);
	light_buffer_info.buffer = lights->buffer;
	light_buffer_info.range = lights->size;

	const uint32_t sz_descriptor_set_writes = COUNT_OF(descriptor_set_writes);
	for (uint32_t i = 0; i != swapchain->image_count; ++i) {
		constant_buffer_info.buffer = constant_buffers->buffers.buffers[i].buffer;
		constant_buffer_info.range = constant_buffers->buffers.buffers[i].size;
		visibility_buffer_info.imageView = render_targets->targets[i].visibility_buffer.view;
		for (uint32_t j = 0; j != sz_descriptor_set_writes; ++j)
			descriptor_set_writes[j].dstSet = pipeline->descriptor_sets[i];
		vkUpdateDescriptorSets(device->device, binding_count, descriptor_set_writes, 0, NULL);
	}
	free(light_texture_writes);
	free((void*) descriptor_set_writes[material_write_index].pImageInfo);

	// Prepare defines for the shader
	mis_heuristic_t mis_heuristic = app->render_settings.mis_heuristic;
	sample_polygon_technique_t polygon_technique = app->render_settings.polygon_sampling_technique;
	uint32_t min_polygonal_light_vertex_count = get_min_polygonal_light_vertex_count(&app->scene_specification);
	uint32_t max_polygonal_light_vertex_count = get_max_polygonal_light_vertex_count(&app->scene_specification);
	uint32_t max_polygon_vertex_count = get_max_polygon_vertex_count(&app->scene_specification, &app->render_settings);

	char* defines[] = {
		format_uint("MATERIAL_COUNT=%u", (uint32_t) scene->materials.material_count),
		format_uint("POLYGONAL_LIGHT_COUNT=%u", app->scene_specification.polygonal_light_count),
		format_uint("POLYGONAL_LIGHT_ARRAY_SIZE=%u", (app->scene_specification.polygonal_light_count > 0) ? app->scene_specification.polygonal_light_count : 1),
		//format_uint("POLYGONAL_LIGHT_COUNT_CLAMPED=%u", (app->scene_specification.polygonal_light_count < 33) ? app->scene_specification.polygonal_light_count : 33),
		format_uint("LIGHT_SAMPLES=%u", app->render_settings.sample_count_light),
		//format_uint("LIGHT_SAMPLES_CLAMPED=%u", (app->render_settings.sample_count_light < 33) ? app->render_settings.sample_count_light: 33),
		format_uint("LIGHT_TEXTURE_COUNT=%u", app->light_textures.image_count),
		format_uint("MIN_POLYGON_VERTEX_COUNT_BEFORE_CLIPPING=%u", min_polygonal_light_vertex_count),
		format_uint("MAX_POLYGONAL_LIGHT_VERTEX_COUNT=%u", max_polygonal_light_vertex_count),
		format_uint("MAX_POLYGON_VERTEX_COUNT=%u", max_polygon_vertex_count),
		format_uint("SAMPLE_COUNT=%u", app->render_settings.sample_count),
		format_uint("SAMPLE_COUNT_CLAMPED=%u", (app->render_settings.sample_count < 33) ? app->render_settings.sample_count : 33),
		format_uint("MIS_HEURISTIC_BALANCE=%u", mis_heuristic == mis_heuristic_balance),
		format_uint("MIS_HEURISTIC_POWER=%u", mis_heuristic == mis_heuristic_power),
		format_uint("MIS_HEURISTIC_WEIGHTED=%u", mis_heuristic == mis_heuristic_weighted),
		format_uint("MIS_HEURISTIC_OPTIMAL_CLAMPED=%u", mis_heuristic == mis_heuristic_optimal_clamped),
		format_uint("MIS_HEURISTIC_OPTIMAL=%u", mis_heuristic == mis_heuristic_optimal),
		format_uint("SAMPLE_LIGHT_UNIFORM=%u", app->render_settings.light_sampling == light_uniform),
		format_uint("SAMPLE_LIGHT_RIS=%u", app->render_settings.light_sampling == light_reservoir),
		//format_uint("SAMPLE_POLYGON_BASELINE=%u", polygon_technique == sample_polygon_baseline),
		//format_uint("SAMPLE_POLYGON_AREA_TURK=%u", polygon_technique == sample_polygon_area_turk),
		//format_uint("SAMPLE_POLYGON_PROJECTED_SOLID_ANGLE=%u", polygon_technique == sample_polygon_projected_solid_angle || polygon_technique == sample_polygon_projected_solid_angle_biased),
		//format_uint("SAMPLE_POLYGON_LTC_CP=%u", polygon_technique == sample_polygon_ltc_cp),
		copy_string((polygon_technique == sample_polygon_projected_solid_angle_biased) ? "USE_BIASED_PROJECTED_SOLID_ANGLE_SAMPLING" : "DONT_USE_BIASED_PROJECTED_SOLID_ANGLE_SAMPLING"),
		format_uint("USE_FAST_ATAN=%u", app->render_settings.fast_atan),
	};

	const uint32_t sz_defines = COUNT_OF(defines);

	// Compile a fragment shader
	shader_request_t fragment_shader_request = {
		.shader_file_path = "src/shaders/shading_pass.frag.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.define_count = sz_defines,
		.defines = defines
	};
	int compile_result = compile_glsl_shader_with_second_chance(&pass->fragment_shader, device, &fragment_shader_request);
	for (uint32_t i = 0; i != sz_defines; ++i)
		free(defines[i]);
	if (compile_result) {
		printf("Failed to compile the fragment shader for the shading pass.\n");
		destroy_shading_pass(pass, device);
		return 1;
	}
	// Compile a vertex shader
	shader_request_t vertex_shader_request = {
		.shader_file_path = "src/shaders/shading_pass.vert.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_VERTEX_BIT
	};
	if (compile_glsl_shader_with_second_chance(&pass->vertex_shader, device, &vertex_shader_request)) {
		printf("Failed to compile the vertex shader for the shading pass.\n");
		destroy_shading_pass(pass, device);
		return 1;
	}

	// Define the graphics pipeline state
	VkVertexInputBindingDescription vertex_binding = { .binding = 0, .stride = sizeof(int8_t) * 2 };
	VkVertexInputAttributeDescription vertex_attribute = { .location = 0, .binding = 0, .format = VK_FORMAT_R8G8_SINT };
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &vertex_attribute,
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.primitiveRestartEnable = VK_FALSE,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.lineWidth = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment_state = {
		.blendEnable = VK_FALSE,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1, .pAttachments = &blend_attachment_state,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.blendConstants = {1.0f, 1.0f, 1.0f, 1.0f}
	};
	VkViewport viewport = {
		.x = 0.0f, .y = 0.0f,
		.width = (float) swapchain->extent.width, .height = (float) swapchain->extent.height,
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	VkRect2D scissor = {.extent = swapchain->extent};
	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &viewport,
		.scissorCount = 1, .pScissors = &scissor,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE
	};
	VkPipelineMultisampleStateCreateInfo multi_sample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = pass->vertex_shader.module,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pass->fragment_shader.module,
			.pName = "main"
		}
	};
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipeline->pipeline_layout,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pRasterizationState = &raster_info,
		.pColorBlendState = &blend_info,
		.pTessellationState = NULL,
		.pMultisampleState = &multi_sample_info,
		.pDynamicState = NULL,
		.pViewportState = &viewport_info,
		.pDepthStencilState = &depth_stencil_info,
		.stageCount = 2, .pStages = shader_stages,
		.renderPass = app->render_pass.render_pass,
		.subpass = 1
	};
	if (vkCreateGraphicsPipelines(device->device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline)) {
		printf("Failed to create a graphics pipeline for the shading pass.\n");
		destroy_shading_pass(pass, device);
		return 1;
	}
	return 0;
}

//! Frees objects and zeros
void destroy_accum_pass(accum_pass_t* pass, const device_t* device) {
	destroy_pipeline_with_bindings(&pass->pipeline, device);
	destroy_shader(&pass->vertex_shader, device);
	destroy_shader(&pass->fragment_shader, device);
	memset(pass, 0, sizeof(*pass));
}

//! Creates Vulkan objects for the shading pass
int create_accum_pass(accum_pass_t * pass, application_t* app)
{
	memset(pass, 0, sizeof(*pass));
	// Get lots of short-hands
	const device_t* device = &app->device;
	const swapchain_t* swapchain = &app->swapchain;
	const render_targets_t* render_targets = &app->render_targets;
	uint32_t image_count = swapchain->image_count;
	pipeline_with_bindings_t* pipeline = &pass->pipeline;
	// Create descriptor sets for the accum pass
	VkDescriptorSetLayoutBinding layout_bindings[] = {
		{ .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
		{ .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE  }
	};
	uint32_t binding_count = COUNT_OF(layout_bindings);
	descriptor_set_request_t set_request = {
		.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.min_descriptor_count = 1,
		.binding_count = binding_count,
		.bindings = layout_bindings,
	};
	VkPushConstantRange range = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(app->accum_num) + 4	// Last 4 bytes for g_frame_bits
	};
	if (create_descriptor_sets(pipeline, device, &set_request, image_count, &range, 1)) {
		printf("Failed to allocate descriptor sets for the shading pass.\n");
		destroy_accum_pass(pass, device);
		return 1;
	}
	// Write to the descriptor sets
	VkDescriptorImageInfo shading_buffer_info = {
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};
	VkDescriptorImageInfo accum_buffer_info = {
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};
	VkWriteDescriptorSet descriptor_set_writes[COUNT_OF(layout_bindings)] = {
		{ .dstBinding = 0, .pImageInfo = &shading_buffer_info },
		{ .dstBinding = 1, .pImageInfo = &accum_buffer_info },
	};
	complete_descriptor_set_write(binding_count, descriptor_set_writes, &set_request);

	const uint32_t sz_descriptor_set_writes = COUNT_OF(descriptor_set_writes);
	for (uint32_t i = 0; i != image_count; ++i) {
		uint32_t prev_swapchain_id = (((int32_t)i - 1) + image_count) % image_count;
		// Attach the previous framebuffer
		accum_buffer_info.imageView = render_targets->targets[prev_swapchain_id].accum_buffer.view;
		shading_buffer_info.imageView = render_targets->targets[i].shading_buffer.view;
		for (uint32_t j = 0; j != sz_descriptor_set_writes; ++j)
			descriptor_set_writes[j].dstSet = pipeline->descriptor_sets[i];
		vkUpdateDescriptorSets(device->device, binding_count, descriptor_set_writes, 0, NULL);
	}

	// Prepare defines for the shader
	VkBool32 output_linear_rgb = swapchain->format == VK_FORMAT_R8G8B8A8_SRGB || swapchain->format == VK_FORMAT_B8G8R8A8_SRGB;

	// Compile a fragment shader
	shader_request_t fragment_shader_request = {
		.shader_file_path = "src/shaders/accum_pass.frag.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	int compile_result = compile_glsl_shader_with_second_chance(&pass->fragment_shader, device, &fragment_shader_request);
	if (compile_result) {
		printf("Failed to compile the fragment shader for the accumulation pass.\n");
		destroy_accum_pass(pass, device);
		return 1;
	}
	// Compile a vertex shader
	shader_request_t vertex_shader_request = {
		.shader_file_path = "src/shaders/accum_pass.vert.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_VERTEX_BIT
	};
	if (compile_glsl_shader_with_second_chance(&pass->vertex_shader, device, &vertex_shader_request)) {
		printf("Failed to compile the vertex shader for the accumulation pass.\n");
		destroy_accum_pass(pass, device);
		return 1;
	}

	// Define the graphics pipeline state
	VkVertexInputBindingDescription vertex_binding = { .binding = 0, .stride = sizeof(int8_t) * 2 };
	VkVertexInputAttributeDescription vertex_attribute = { .location = 0, .binding = 0, .format = VK_FORMAT_R8G8_SINT };
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &vertex_attribute,
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.primitiveRestartEnable = VK_FALSE,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.lineWidth = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment_state = {
		.blendEnable = VK_FALSE,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1, .pAttachments = &blend_attachment_state,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.blendConstants = {1.0f, 1.0f, 1.0f, 1.0f}
	};
	VkViewport viewport = {
		.x = 0.0f, .y = 0.0f,
		.width = (float) swapchain->extent.width, .height = (float) swapchain->extent.height,
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	VkRect2D scissor = {.extent = swapchain->extent};
	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &viewport,
		.scissorCount = 1, .pScissors = &scissor,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE
	};
	VkPipelineMultisampleStateCreateInfo multi_sample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = pass->vertex_shader.module,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pass->fragment_shader.module,
			.pName = "main"
		}
	};
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipeline->pipeline_layout,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pRasterizationState = &raster_info,
		.pColorBlendState = &blend_info,
		.pTessellationState = NULL,
		.pMultisampleState = &multi_sample_info,
		.pDynamicState = NULL,
		.pViewportState = &viewport_info,
		.pDepthStencilState = &depth_stencil_info,
		.stageCount = 2, .pStages = shader_stages,
		.renderPass = app->render_pass.render_pass,
		.subpass = 2
	};
	if (vkCreateGraphicsPipelines(device->device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline)) {
		printf("Failed to create a graphics pipeline for the accumulation pass.\n");
		destroy_accum_pass(pass, device);
		return 1;
	}
	return 0;
}

//! Frees objects and zeros
void destroy_copy_pass(copy_pass_t* pass, const device_t* device) {
	destroy_pipeline_with_bindings(&pass->pipeline, device);
	destroy_shader(&pass->vertex_shader, device);
	destroy_shader(&pass->fragment_shader, device);
	memset(pass, 0, sizeof(*pass));
}

//! Creates Vulkan objects for the copy pass
int create_copy_pass(copy_pass_t * pass, application_t* app) {
	memset(pass, 0, sizeof(*pass));
	// Get lots of short-hands
	const device_t* device = &app->device;
	const swapchain_t* swapchain = &app->swapchain;
	const render_targets_t* render_targets = &app->render_targets;
	uint32_t image_count = swapchain->image_count;
	pipeline_with_bindings_t* pipeline = &pass->pipeline;
	// Create descriptor sets for the accum pass
	VkDescriptorSetLayoutBinding layout_bindings[] = {
		{ .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
	};
	uint32_t binding_count = COUNT_OF(layout_bindings);
	descriptor_set_request_t set_request = {
		.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.min_descriptor_count = 1,
		.binding_count = binding_count,
		.bindings = layout_bindings,
	};
	VkPushConstantRange range = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = 4	// 4 bytes
	};
	if (create_descriptor_sets(pipeline, device, &set_request, image_count, &range, 1)) {
		printf("Failed to allocate descriptor sets for the shading pass.\n");
		destroy_copy_pass(pass, device);
		return 1;
	}
	// Write to the descriptor sets
	VkDescriptorImageInfo accum_buffer_info = {
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};
	VkWriteDescriptorSet descriptor_set_writes[COUNT_OF(layout_bindings)] = {
		{ .dstBinding = 0, .pImageInfo = &accum_buffer_info },
	};
	complete_descriptor_set_write(binding_count, descriptor_set_writes, &set_request);

	const uint32_t sz_descriptor_set_writes = COUNT_OF(descriptor_set_writes);
	for (uint32_t i = 0; i != image_count; ++i) {
		accum_buffer_info.imageView = render_targets->targets[i].accum_buffer.view;
		for (uint32_t j = 0; j != sz_descriptor_set_writes; ++j)
			descriptor_set_writes[j].dstSet = pipeline->descriptor_sets[i];
		vkUpdateDescriptorSets(device->device, binding_count, descriptor_set_writes, 0, NULL);
	}

	// Prepare defines for the shader
	VkBool32 output_linear_rgb = swapchain->format == VK_FORMAT_R8G8B8A8_SRGB || swapchain->format == VK_FORMAT_B8G8R8A8_SRGB;
	char* defines[] = {
		format_uint("OUTPUT_LINEAR_RGB=%u", output_linear_rgb),
	};

	const uint32_t sz_defines1 = COUNT_OF(defines);

	// Compile a fragment shader
	shader_request_t fragment_shader_request = {
		.shader_file_path = "src/shaders/copy_pass.frag.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.define_count = sz_defines1,
		.defines = defines
	};
	int compile_result = compile_glsl_shader_with_second_chance(&pass->fragment_shader, device, &fragment_shader_request);
	for (uint32_t i = 0; i != sz_defines1; ++i)
		free(defines[i]);
	if (compile_result) {
		printf("Failed to compile the fragment shader for the copy pass.\n");
		destroy_copy_pass(pass, device);
		return 1;
	}
	// Compile a vertex shader
	shader_request_t vertex_shader_request = {
		.shader_file_path = "src/shaders/copy_pass.vert.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_VERTEX_BIT
	};
	if (compile_glsl_shader_with_second_chance(&pass->vertex_shader, device, &vertex_shader_request)) {
		printf("Failed to compile the vertex shader for the copy pass.\n");
		destroy_copy_pass(pass, device);
		return 1;
	}

	// Define the graphics pipeline state
	VkVertexInputBindingDescription vertex_binding = { .binding = 0, .stride = sizeof(int8_t) * 2 };
	VkVertexInputAttributeDescription vertex_attribute = { .location = 0, .binding = 0, .format = VK_FORMAT_R8G8_SINT };
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &vertex_attribute,
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.primitiveRestartEnable = VK_FALSE,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.lineWidth = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment_state = {
		.blendEnable = VK_FALSE,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1, .pAttachments = &blend_attachment_state,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.blendConstants = {1.0f, 1.0f, 1.0f, 1.0f}
	};
	VkViewport viewport = {
		.x = 0.0f, .y = 0.0f,
		.width = (float) swapchain->extent.width, .height = (float) swapchain->extent.height,
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	VkRect2D scissor = {.extent = swapchain->extent};
	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &viewport,
		.scissorCount = 1, .pScissors = &scissor,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE
	};
	VkPipelineMultisampleStateCreateInfo multi_sample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = pass->vertex_shader.module,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pass->fragment_shader.module,
			.pName = "main"
		}
	};
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipeline->pipeline_layout,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pRasterizationState = &raster_info,
		.pColorBlendState = &blend_info,
		.pTessellationState = NULL,
		.pMultisampleState = &multi_sample_info,
		.pDynamicState = NULL,
		.pViewportState = &viewport_info,
		.pDepthStencilState = &depth_stencil_info,
		.stageCount = 2, .pStages = shader_stages,
		.renderPass = app->render_pass.render_pass,
		.subpass = 3
	};
	if (vkCreateGraphicsPipelines(device->device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline)) {
		printf("Failed to create a graphics pipeline for the accumulation pass.\n");
		destroy_copy_pass(pass, device);
		return 1;
	}
	return 0;
}


//! Frees objects and zeros
void destroy_interface_pass(interface_pass_t* pass, const device_t* device) {
	for (uint32_t i = 0; i != pass->frame_count; ++i)
		if (pass->frames) free(pass->frames[i].draws);
	free(pass->frames);
	destroy_buffers(&pass->geometry_allocation, device);
	destroy_images(&pass->texture, device);
	destroy_pipeline_with_bindings(&pass->pipeline, device);
	destroy_shader(&pass->vertex_shader, device);
	destroy_shader(&pass->fragment_shader, device);
	if (pass->sampler) vkDestroySampler(device->device, pass->sampler, NULL);
	memset(pass, 0, sizeof(*pass));
}

//! Creates the pass for rendering a user interface
int create_interface_pass(interface_pass_t* pass, const device_t* device, imgui_handle_t imgui, const swapchain_t* swapchain, const render_targets_t* render_targets, const render_pass_t* render_pass) {
	memset(pass, 0, sizeof(*pass));
	// Create geometry buffers and map memory
	uint32_t imgui_quad_count = 0xFFFF;
	VkBufferCreateInfo geometry_infos[] = {
		{ // 0 - Imgui vertices
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(imgui_vertex_t) * 4 * imgui_quad_count,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		},
		{ // 1 - Imgui indices
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(uint16_t) * 6 * imgui_quad_count,
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		}
	};
	pass->frame_count = swapchain->image_count;
	uint32_t geometry_count = COUNT_OF(geometry_infos) * pass->frame_count;
	VkBufferCreateInfo* duplicate_geometry_infos = malloc(sizeof(VkBufferCreateInfo) * geometry_count);
	for (uint32_t i = 0; i != geometry_count; ++i)
		duplicate_geometry_infos[i] = geometry_infos[i % COUNT_OF(geometry_infos)];
	if (create_aligned_buffers(&pass->geometry_allocation, device, duplicate_geometry_infos, geometry_count, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device->physical_device_properties.limits.nonCoherentAtomSize)) {
		printf("Failed to create geometry buffers for the interface pass.\n");
		destroy_interface_pass(pass, device);
		free(duplicate_geometry_infos);
		return 1;
	}
	free(duplicate_geometry_infos);
	pass->geometries = (void*) pass->geometry_allocation.buffers;
	if (vkMapMemory(device->device, pass->geometry_allocation.memory, 0, pass->geometry_allocation.size, 0, &pass->geometry_data)) {
		printf("Failed to map geometry buffers for the interface pass.\n");
		destroy_interface_pass(pass, device);
		return 1;
	}
	// Prepare the object used to query drawing commands
	pass->frames = malloc(sizeof(imgui_frame_t) * pass->frame_count);
	memset(pass->frames, 0, sizeof(imgui_frame_t) * pass->frame_count);
	for (uint32_t i = 0; i != pass->frame_count; ++i) {
		pass->frames[i].draws_size = 1000;
		pass->frames[i].draws = malloc(sizeof(imgui_draw_t) * pass->frames[i].draws_size);
		pass->frames[i].vertices = (imgui_vertex_t*) (((uint8_t*) pass->geometry_data) + pass->geometries[i].vertices.offset);
		pass->frames[i].indices = (uint16_t*) (((uint8_t*) pass->geometry_data) + pass->geometries[i].indices.offset);
		pass->frames[i].vertices_size = 4 * imgui_quad_count;
		pass->frames[i].indices_size = 6 * imgui_quad_count;
	}

	// Compile vertex and fragment shaders for imgui
	VkBool32 output_linear_rgb = swapchain->format == VK_FORMAT_R8G8B8A8_SRGB || swapchain->format == VK_FORMAT_B8G8R8A8_SRGB;
	char* fragment_defines[2] = {
		"OUTPUT_LINEAR_RGB=0",
		"OUTPUT_LINEAR_RGB=1"
	};
	char* gui_defines[] = {
		copy_string(fragment_defines[output_linear_rgb ? 1 : 0]),
		format_uint("VIEWPORT_WIDTH=%u", swapchain->extent.width),
		format_uint("VIEWPORT_HEIGHT=%u", swapchain->extent.height),
	};

	const uint32_t sz_gui_defines = COUNT_OF(gui_defines);

	shader_request_t gui_vertex_request = {
		.shader_file_path = "src/shaders/imgui.vert.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.define_count = sz_gui_defines, .defines = gui_defines
	};
	shader_request_t gui_fragment_request = {
		.shader_file_path = "src/shaders/imgui.frag.glsl",
		.include_path = "src/shaders",
		.entry_point = "main",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.define_count = sz_gui_defines, .defines = gui_defines
	};
	if (compile_glsl_shader_with_second_chance(&pass->vertex_shader, device, &gui_vertex_request)
		|| compile_glsl_shader_with_second_chance(&pass->fragment_shader, device, &gui_fragment_request))
	{
		printf("Failed to compile shaders for the GUI rendering.\n");
		destroy_interface_pass(pass, device);
		for (uint32_t i = 0; i != sz_gui_defines; ++i)
			free(gui_defines[i]);
		return 1;
	}
	for (uint32_t i = 0; i != sz_gui_defines; ++i)
		free(gui_defines[i]);
	// Create a sampler for the font texture of imgui
	VkSamplerCreateInfo gui_sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	};
	if (vkCreateSampler(device->device, &gui_sampler_info, NULL, &pass->sampler)) {
		printf("Failed to create a sampler for rendering the GUI.\n");
		destroy_interface_pass(pass, device);
		return 1;
	}

	// Create the image with fonts for imgui
	VkExtent2D gui_texture_extent;
	get_imgui_image(NULL, &gui_texture_extent.width, &gui_texture_extent.height, imgui);
	VkBufferCreateInfo gui_staging_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(uint8_t) * gui_texture_extent.width * gui_texture_extent.height,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	buffers_t gui_staging_buffer;
	uint8_t* gui_staging_data;
	if (create_buffers(&gui_staging_buffer, device, &gui_staging_info, 1, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		|| vkMapMemory(device->device, gui_staging_buffer.memory, 0, gui_staging_buffer.size, 0, (void**) &gui_staging_data))
	{
		printf("Failed to create and map a staging buffer for the the GUI.\n");
		destroy_buffers(&gui_staging_buffer, device);
		destroy_interface_pass(pass, device);
		return 1;
	}
	get_imgui_image(gui_staging_data, NULL, NULL, imgui);
	vkUnmapMemory(device->device, gui_staging_buffer.memory);
	image_request_t gui_texture_request = {
		.image_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R8_UNORM,
			.extent = {gui_texture_extent.width, gui_texture_extent.height, 1},
			.mipLevels = 1, .arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		},
		.view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT}
		}
	};
	VkBufferImageCopy gui_texture_region = {
		.imageExtent = gui_texture_request.image_info.extent,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1
		}
	};
	if (create_images(&pass->texture, device, &gui_texture_request, 1, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
		|| copy_buffers_to_images(device, 1, &gui_staging_buffer.buffers[0].buffer, &pass->texture.images[0].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &gui_texture_region))
	{
		printf("Failed to create and fill the GUI texture.\n");
		destroy_buffers(&gui_staging_buffer, device);
		destroy_interface_pass(pass, device);
		return 1;
	}
	destroy_buffers(&gui_staging_buffer, device);

	// Create the pipeline with descriptor set for GUI rendering
	pipeline_with_bindings_t* pipeline = &pass->pipeline;
	// Begin by allocating descriptor sets
	VkDescriptorSetLayoutBinding sampler_binding = {
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	};
	descriptor_set_request_t set_request = {
		.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.min_descriptor_count = 1,
		.binding_count = 1,
		.bindings = &sampler_binding,
	};
	if (create_descriptor_sets(pipeline, device, &set_request, swapchain->image_count, NULL, 0)) {
		printf("Failed to allocate descriptor sets for the interface pass.\n");
		destroy_interface_pass(pass, device);
		return 1;
	}
	// Write to the descriptor set
	VkDescriptorImageInfo gui_descriptor_image_info = {
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.imageView = pass->texture.images[0].view,
		.sampler = pass->sampler
	};
	VkWriteDescriptorSet descriptor_set_write = { .pImageInfo = &gui_descriptor_image_info  };
	complete_descriptor_set_write(1, &descriptor_set_write, &set_request);
	for (uint32_t j = 0; j != swapchain->image_count; ++j) {
		descriptor_set_write.dstSet = pipeline->descriptor_sets[j];
		vkUpdateDescriptorSets(device->device, 1, &descriptor_set_write, 0, NULL);
	}
	// Define the graphics pipeline state
	VkVertexInputBindingDescription gui_vertex_bindings[] = {
		{.binding = 0, .stride = sizeof(imgui_vertex_t)}
	};
	VkVertexInputAttributeDescription gui_vertex_attributes[] = {
		{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
		{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 2},
		{.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = sizeof(float) * 4},
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = COUNT_OF(gui_vertex_bindings), .pVertexBindingDescriptions = gui_vertex_bindings,
		.vertexAttributeDescriptionCount = COUNT_OF(gui_vertex_attributes),	.pVertexAttributeDescriptions = gui_vertex_attributes,
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.primitiveRestartEnable = VK_FALSE,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.lineWidth = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment_state = {
		.blendEnable = VK_TRUE,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1, .pAttachments = &blend_attachment_state,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.blendConstants = {1.0f, 1.0f, 1.0f, 1.0f}
	};
	VkViewport viewport = {
		.x = 0.0f, .y = 0.0f,
		.width = (float) swapchain->extent.width, .height = (float) swapchain->extent.height,
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	VkRect2D scissor = {.extent = swapchain->extent};
	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &viewport,
		.scissorCount = 1, .pScissors = &scissor,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE
	};
	VkPipelineMultisampleStateCreateInfo multi_sample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = pass->vertex_shader.module,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pass->fragment_shader.module,
			.pName = "main"
		}
	};
	VkDynamicState dynamic_scissor_state = VK_DYNAMIC_STATE_SCISSOR;
	VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 1, .pDynamicStates = &dynamic_scissor_state
	};
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipeline->pipeline_layout,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pRasterizationState = &raster_info,
		.pColorBlendState = &blend_info,
		.pTessellationState = NULL,
		.pMultisampleState = &multi_sample_info,
		.pDynamicState = NULL,
		.pViewportState = &viewport_info,
		.pDepthStencilState = &depth_stencil_info,
		.pDynamicState = &dynamic_state,
		.stageCount = 2, .pStages = shader_stages,
		.renderPass = render_pass->render_pass,
		.subpass = 4,
	};
	if (vkCreateGraphicsPipelines(device->device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline)) {
		printf("Failed to create a graphics pipeline for the transfer pass.\n");
		destroy_interface_pass(pass, device);
		return 1;
	}
	return 0;
}


//! Frees objects and zeros
void destroy_render_pass(render_pass_t* pass, const device_t* device) {
	for (uint32_t i = 0; i != pass->framebuffer_count; ++i)
		if (pass->framebuffers[i])
			vkDestroyFramebuffer(device->device, pass->framebuffers[i], NULL);
	free(pass->framebuffers);
	if (pass->render_pass) vkDestroyRenderPass(device->device, pass->render_pass, NULL);
	memset(pass, 0, sizeof(*pass));
}


//! Creates the render pass that renders a complete frame
int create_render_pass(render_pass_t* pass, const device_t* device, const swapchain_t* swapchain, const render_targets_t* render_targets) {
	memset(pass, 0, sizeof(*pass));
	// Create the render pass
	VkAttachmentDescription attachments[] = {
		{ // 0 - Depth buffer
			.format = render_targets->targets[0].depth_buffer.image_info.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		},
		{ // 1 - Visibility buffer
			.format = render_targets->targets[0].visibility_buffer.image_info.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_GENERAL
		},
		{ // 2 - Shading buffer
			.format = render_targets->targets[0].shading_buffer.image_info.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_GENERAL
		},
		{ // 3 - Accum buffer
			.format = render_targets->targets[0].accum_buffer.image_info.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_GENERAL
		},
		{ // 4 - Swapchain image
			.format = swapchain->format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		},
	};
	VkAttachmentReference depth_reference = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkAttachmentReference visibility_output_reference = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference visibility_input_reference = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_GENERAL};
	VkAttachmentReference shading_output_reference = {.attachment = 2, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference shading_input_reference = {.attachment = 2, .layout = VK_IMAGE_LAYOUT_GENERAL};
	VkAttachmentReference accum_output_reference = {.attachment = 3, .layout = VK_IMAGE_LAYOUT_GENERAL};
	VkAttachmentReference accum_input_reference = {.attachment = 3, .layout = VK_IMAGE_LAYOUT_GENERAL};
	VkAttachmentReference swapchain_output_reference = {.attachment = 4, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpasses[] = {
		{ // 0 - visibility pass
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.pDepthStencilAttachment = &depth_reference,
			.colorAttachmentCount = 1, .pColorAttachments = &visibility_output_reference,
		},
		{ // 1 - shading pass
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 1, .pInputAttachments = &visibility_input_reference,
			.colorAttachmentCount = 1, .pColorAttachments = &shading_output_reference,
		},
		{ // 2 - accum pass
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 1, .pInputAttachments = &shading_input_reference,
			.colorAttachmentCount = 1, .pColorAttachments = &accum_output_reference,
		},
		{ // 3 - copy pass
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 1, .pInputAttachments = &accum_input_reference,
			.colorAttachmentCount = 1, .pColorAttachments = &swapchain_output_reference,
		},
		{ // 4 - interface pass
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.colorAttachmentCount = 1, .pColorAttachments = &swapchain_output_reference,
		},
	};
	VkSubpassDependency dependencies[] = {
		{ // Swapchain image has been acquired
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 1,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		},
		{ // Visibility buffer has been drawn
			.srcSubpass = 0,
			.dstSubpass = 1,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		},
		{ // The shading pass has finished drawing
			.srcSubpass = 1,
			.dstSubpass = 2,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		},
		{ // The accum pass has finished drawing
			.srcSubpass = 2,
			.dstSubpass = 3,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		},
		{ // The copy pass has finished drawing
			.srcSubpass = 3,
			.dstSubpass = 4,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		},
	};
	VkRenderPassCreateInfo renderpass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = COUNT_OF(attachments), .pAttachments = attachments,
		.subpassCount = COUNT_OF(subpasses), .pSubpasses = subpasses,
		.dependencyCount = COUNT_OF(dependencies), .pDependencies = dependencies
	};
	if (vkCreateRenderPass(device->device, &renderpass_info, NULL, &pass->render_pass)) {
		printf("Failed to create a render pass for the geometry pass.\n");
		destroy_render_pass(pass, device);
		return 1;
	}

	// Create one framebuffer per swapchain image
	VkImageView framebuffer_attachments[5];
	VkFramebufferCreateInfo framebuffer_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = pass->render_pass,
		.attachmentCount = COUNT_OF(framebuffer_attachments),
		.pAttachments = framebuffer_attachments,
		.width = swapchain->extent.width,
		.height = swapchain->extent.height,
		.layers = 1
	};
	pass->framebuffer_count = swapchain->image_count;
	pass->framebuffers = malloc(sizeof(VkFramebuffer) * pass->framebuffer_count);
	memset(pass->framebuffers, 0, sizeof(VkFramebuffer) * pass->framebuffer_count);
	for (uint32_t i = 0; i != pass->framebuffer_count; ++i) {
		framebuffer_attachments[0] = render_targets->targets[i].depth_buffer.view;
		framebuffer_attachments[1] = render_targets->targets[i].visibility_buffer.view;
		framebuffer_attachments[2] = render_targets->targets[i].shading_buffer.view;
		framebuffer_attachments[3] = render_targets->targets[i].accum_buffer.view;
		framebuffer_attachments[4] = swapchain->image_views[i];
		if (vkCreateFramebuffer(device->device, &framebuffer_info, NULL, &pass->framebuffers[i])) {
			printf("Failed to create a framebuffer for the main render pass.\n");
			destroy_render_pass(pass, device);
			return 1;
		}
	}
	return 0;
}


/*! Adds commands for rendering the imgui user interface to the given command
	buffer, which is currently being recorded.
	\return 0 on success.*/
int render_gui(VkCommandBuffer cmd, application_t* app, uint32_t swapchain_index) {
	interface_pass_t* pass = &app->interface_pass;
	if (get_imgui_frame(&pass->frames[swapchain_index], app->imgui))
		return 1;
	VkMappedMemoryRange gui_ranges[] = {
		{
			.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.memory = pass->geometry_allocation.memory,
			.offset = pass->geometries[swapchain_index].buffers[0].offset,
			.size = get_mapped_memory_range_size(&app->device, &pass->geometry_allocation, 2 * swapchain_index + 0),
		},
		{
			.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.memory = pass->geometry_allocation.memory,
			.offset = pass->geometries[swapchain_index].buffers[1].offset,
			.size = get_mapped_memory_range_size(&app->device, &pass->geometry_allocation, 2 * swapchain_index + 1),
		},
	};
	vkFlushMappedMemoryRanges(app->device.device, COUNT_OF(gui_ranges), gui_ranges);
	// Record all draw calls
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		app->interface_pass.pipeline.pipeline_layout, 0, 1, &pass->pipeline.descriptor_sets[swapchain_index], 0, NULL);
	vkCmdBindIndexBuffer(cmd, pass->geometries[swapchain_index].indices.buffer, 0, VK_INDEX_TYPE_UINT16);
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, &pass->geometries[swapchain_index].vertices.buffer, offsets);
	for (uint32_t i = 0; i != pass->frames[swapchain_index].draw_count; ++i) {
		const imgui_draw_t* draw = &pass->frames[swapchain_index].draws[i];
		VkRect2D scissor = {
			.offset = {draw->scissor_x, draw->scissor_y},
			.extent = {draw->scissor_width, draw->scissor_height}
		};
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdDrawIndexed(cmd, (uint32_t) draw->triangle_count, 1, (uint32_t) draw->index_offset, 0, 0);
	}
	return 0;
}


/*! This function records commands for rendering a frame to the given swapchain
	image into the given command buffer
	\return 0 on success.*/
int record_render_frame_commands(VkCommandBuffer cmd, application_t* app, uint32_t swapchain_index) {
	const device_t* device = &app->device;
	// Start recording commands
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	if (vkBeginCommandBuffer(cmd, &begin_info)) {
		printf("Failed to begin using a command buffer for rendering the scene.\n");
		return 1;
	}
	// Begin the render pass that renders the whole frame
	VkClearValue clear_values[] = {
		{.depthStencil = {.depth = 1.0f}},
		{.color = {.uint32 = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}}},
		{.color = {.uint32 = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}}},
		{.color = {.uint32 = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}}},
		{.color = {.uint32 = {0, 0, 0, 0}}},
	};
	VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = app->render_pass.render_pass,
		.framebuffer = app->render_pass.framebuffers[swapchain_index],
		.renderArea.offset = {0, 0},
		.renderArea.extent = app->swapchain.extent,
		.clearValueCount = COUNT_OF(clear_values), .pClearValues = clear_values
	};
	// Clear query pool
	vkCmdResetQueryPool(cmd, app->query_pool.pool, swapchain_index*2, 2);
	// Record beginning timestamp
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, app->query_pool.pool, swapchain_index*2);
	vkCmdBeginRenderPass(cmd, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
	// Render the scene to the visibility buffer
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->geometry_pass.pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		app->geometry_pass.pipeline.pipeline_layout, 0, 1, &app->geometry_pass.pipeline.descriptor_sets[swapchain_index], 0, NULL);
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, &app->scene.mesh.positions.buffer, offsets);
	vkCmdDraw(cmd, (uint32_t)app->scene.mesh.triangle_count * 3, 1, 0, 0);
	// Run the shading pass
	vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->shading_pass.pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		app->shading_pass.pipeline.pipeline_layout, 0, 1, &app->shading_pass.pipeline.descriptor_sets[swapchain_index], 0, NULL);
	vkCmdBindVertexBuffers(cmd, 0, 1, &app->scene.mesh.triangle.buffer, offsets);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	// Run the accum pass
	vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->accum_pass.pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		app->accum_pass.pipeline.pipeline_layout, 0, 1, &app->accum_pass.pipeline.descriptor_sets[swapchain_index], 0, NULL);
	vkCmdBindVertexBuffers(cmd, 0, 1, &app->scene.mesh.triangle.buffer, offsets);
	vkCmdPushConstants(cmd, app->accum_pass.pipeline.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(app->accum_num), &app->accum_num);
	vkCmdPushConstants(cmd, app->accum_pass.pipeline.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 4, 4, &app->screenshot.frame_bits);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	// Run the copy pass
	vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->copy_pass.pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		app->copy_pass.pipeline.pipeline_layout, 0, 1, &app->copy_pass.pipeline.descriptor_sets[swapchain_index], 0, NULL);
	vkCmdBindVertexBuffers(cmd, 0, 1, &app->scene.mesh.triangle.buffer, offsets);
	vkCmdPushConstants(cmd, app->copy_pass.pipeline.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &app->screenshot.frame_bits);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	// Run the interface pass
	vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
	if (app->render_settings.show_gui && !app->screenshot.path_hdr) {
		if (render_gui(cmd, app, swapchain_index)) {
			printf("Failed to render the user interface.\n");
			return 1;
		}
	}
	// The frame is rendered completely
	vkCmdEndRenderPass(cmd);
	// Record end timestamp
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, app->query_pool.pool, swapchain_index*2+1);
	// Finish recording
	if (vkEndCommandBuffer(cmd)) {
		printf("Failed to end using a command buffer for rendering the scene.\n");
		return 1;
	}

	return 0;
}


//! Frees objects and zeros
void destroy_frame_sync(frame_sync_t* sync, const device_t* device) {
	if (sync->image_acquired) vkDestroySemaphore(device->device, sync->image_acquired, NULL);
	memset(sync, 0, sizeof(*sync));
}

//! Creates synchronization objects for rendering a single frame
//! \return 0 on success.
int create_frame_sync(frame_sync_t* sync, const device_t* device) {
	memset(sync, 0, sizeof(*sync));
	VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	if (vkCreateSemaphore(device->device, &semaphore_info, NULL, &sync->image_acquired)) {
		printf("Failed to create a semaphore.\n");
		destroy_frame_sync(sync, device);
		return 1;
	}
	return 0;
}


//! Frees objects and zeros
void destroy_frame_queue(frame_queue_t* queue, const device_t* device) {
	for (uint32_t i = 0; i != queue->frame_count; ++i) {
		if (queue->workloads) {
			frame_workload_t* workload = &queue->workloads[i];
			if (workload->command_buffer)
				vkFreeCommandBuffers(device->device, device->command_pool, 1, &workload->command_buffer);
			if (workload->drawing_finished_fence)
				vkDestroyFence(device->device, workload->drawing_finished_fence, NULL);
		}
		if (queue->syncs)
			destroy_frame_sync(&queue->syncs[i], device);
	}
	free(queue->workloads);
	free(queue->syncs);
	memset(queue, 0, sizeof(*queue));
}

//! Creates the frame queue, including both synchronization objects and
//! command buffers
int create_frame_queue(frame_queue_t* queue, const device_t* device, const swapchain_t* swapchain) {
	memset(queue, 0, sizeof(*queue));
	// Create synchronization objects
	queue->frame_count = swapchain->image_count;
	queue->syncs = malloc(sizeof(frame_sync_t) * queue->frame_count);
	memset(queue->syncs, 0, sizeof(frame_sync_t) * queue->frame_count);
	for (uint32_t i = 0; i != queue->frame_count; ++i) {
		if (create_frame_sync(&queue->syncs[i], device)) {
			destroy_frame_queue(queue, device);
			return 1;
		}
	}
	// Allocate command buffers for rendering the scene to each of the swapchain
	// images
	VkCommandBufferAllocateInfo command_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = device->command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	queue->workloads = malloc(sizeof(frame_workload_t) * queue->frame_count);
	memset(queue->workloads, 0, sizeof(frame_workload_t) * queue->frame_count);
	for (uint32_t i = 0; i != queue->frame_count; ++i) {
		if (vkAllocateCommandBuffers(device->device, &command_info, &queue->workloads[i].command_buffer)) {
			printf("Failed to allocate command buffers for rendering.\n");
			destroy_frame_queue(queue, device);
			return 1;
		}
		VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		if (vkCreateFence(device->device, &fence_info, NULL, &queue->workloads[i].drawing_finished_fence)) {
			printf("Failed to create a fence.\n");
			destroy_frame_queue(queue, device);
			return 1;
		}
	}
	return 0;
}

//! Cleans up intermediate objects for taking a screenshot. The object is ready
//! to be reused for the next screenshot afterwards.
void destroy_screenshot(screenshot_t* screenshot, const device_t* device) {
	free(screenshot->path_png);
	free(screenshot->path_jpg);
	free(screenshot->path_hdr);
	destroy_images(&screenshot->staging, device);
	free(screenshot->ldr_copy);
	free(screenshot->hdr_copy);
	memset(screenshot, 0, sizeof(*screenshot));
}


/*! Sets output paths for a screenshot and thus kicks off taking a screenshot.
	Can be called any time, except when taking a screenshot is already in
	progress. If *.hdr is non-NULL, the others must be NULL.*/
void take_screenshot(screenshot_t* screenshot, const char* path_png, const char* path_jpg, const char* path_hdr) {
	if (path_hdr && (path_png || path_jpg)) {
		printf("Cannot mix LDR and HDR screenshots.\n");
		return;
	}
	if (screenshot->path_png || screenshot->path_jpg || screenshot->path_hdr) {
		printf("Cannot take another screenshot while a screenshot is already being taken.\n");
		return;
	}
	if(path_png) screenshot->path_png = copy_string(path_png);
	if(path_jpg) screenshot->path_jpg = copy_string(path_jpg);
	if(path_hdr) {
		screenshot->path_hdr = copy_string(path_hdr);
		screenshot->frame_bits = frame_bits_hdr_low;
	}
}


//! Helper for implement_screenshot(). Allocates staging memory and
//! intermediate buffers.
int create_screenshot_staging_buffers(screenshot_t* screenshot, const swapchain_t* swapchain, const device_t* device) {
	VkBool32 hdr_mode = (screenshot->path_hdr != NULL);
	// Create a staging image
	VkFormat source_format = swapchain->format;
	image_request_t staging_request = {
		.image_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = source_format,
			.extent = { swapchain->extent.width, swapchain->extent.height, 1 },
			.mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_LINEAR,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		}
	};
	if (create_images(&screenshot->staging, device, &staging_request, 1, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
		printf("Failed to create a staging image for taking a screenshot.\n");
		return 1;
	}
	// Allocate buffers for stb to read from
	uint32_t pixel_count = swapchain->extent.width * swapchain->extent.height;
	screenshot->ldr_copy = malloc(sizeof(uint8_t) * 3 * pixel_count * (hdr_mode ? 2 : 1));
	if(hdr_mode)
		screenshot->hdr_copy = malloc(sizeof(float) * 3 * pixel_count);
	return 0;
}


//! Helper for implement_screenshot(). More precisely, it copies contents of
//! the swapchain image first to the staging image and then to the appropriate
//! part of the LDR buffer.
int grab_screenshot_ldr(screenshot_t* screenshot, const swapchain_t* swapchain, const device_t* device, uint32_t swapchain_index) {
	// Wait for all rendering to finish
	if (vkDeviceWaitIdle(device->device)) {
		printf("Failed to wait for rendering to finish to take a screenshot.\n");
		return 1;
	}
	// Copy the swapchain image
	VkImage source_image = swapchain->images[swapchain_index];
	VkImageCopy region = {
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1
		},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1
		},
		.extent = { swapchain->extent.width, swapchain->extent.height, 1 },
	};
	if (copy_images(device, 1, &source_image, &screenshot->staging.images[0].image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &region)) {
		printf("Failed to copy the swapchain image to a staging image for taking a screenshot.\n");
		return 1;
	}
	// Map the memory of the staging image
	uint8_t* staging_data;
	if (vkMapMemory(device->device, screenshot->staging.memories[0], screenshot->staging.images[0].memory_offset,
		screenshot->staging.images[0].memory_size, 0, (void**) &staging_data))
	{
		printf("Failed to map the host memory holding the screenshot.\n");
		return 1;
	}
	// Figure out what to do with the source format
	VkBool32 source_10_bit_hdr = VK_FALSE;
	uint32_t channel_permutation[3] = { 0, 1, 2 };
	switch (swapchain->format) {
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
		channel_permutation[0] = 2;
		channel_permutation[2] = 0;
		break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		source_10_bit_hdr = VK_TRUE;
		channel_permutation[0] = 2;
		channel_permutation[2] = 0;
		break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		source_10_bit_hdr = VK_TRUE;
		break;
	default:
		break;
	};
	// Query the row pitch
	VkImageSubresource subresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };
	VkSubresourceLayout subresource_layout;
	vkGetImageSubresourceLayout(device->device, screenshot->staging.images[0].image, &subresource, &subresource_layout);
	if (subresource_layout.rowPitch % 4) {
		printf("Unexpected row pitch. Failed to take a screenshot.\n");
		return 1;
	}
	VkDeviceSize pixel_row_pitch = subresource_layout.rowPitch / 4;
	// Convert to an appropriate format for stb
	VkExtent3D extent = region.extent;
	uint8_t* ldr_copy = screenshot->ldr_copy;
	if (screenshot->frame_bits == frame_bits_hdr_high)
		ldr_copy += 3 * extent.width * extent.height;
	int stride = extent.width * 3 * sizeof(uint8_t);
	if (!source_10_bit_hdr) {
		VkDeviceSize source_index0 = 0;
		VkDeviceSize source_index4 = 0;
		VkDeviceSize index3 = 0;
		for (uint32_t y = 0; y != extent.height; ++y) {
			source_index4 = source_index0 << 2;
			for (uint32_t x = 0; x != extent.width; ++x) {
				ldr_copy[index3 + channel_permutation[0]] = staging_data[source_index4];
				ldr_copy[index3 + channel_permutation[1]] = staging_data[source_index4 + 1];
				ldr_copy[index3 + channel_permutation[2]] = staging_data[source_index4 + 2];
				index3 += 3;
				source_index4 += 4;
			}
			source_index0 += pixel_row_pitch;
		}
	}
	else {
		VkDeviceSize source_index0 = 0;
		VkDeviceSize source_index = 0;
		VkDeviceSize index3 = 0;
		for (uint32_t y = 0; y != extent.height; ++y) {
			source_index = source_index0;
			for (uint32_t x = 0; x != extent.width; ++x) {
				uint32_t pixel = ((uint32_t*) staging_data)[source_index];
				uint32_t red = (pixel & 0x3FF) >> 2;
				uint32_t green = (pixel & 0xFFC00) >> 12;
				uint32_t blue = (pixel & 0x3FF00000) >> 22;
				ldr_copy[index3 + channel_permutation[0]] = (uint8_t) red;
				ldr_copy[index3 + channel_permutation[1]] = (uint8_t) green;
				ldr_copy[index3 + channel_permutation[2]] = (uint8_t) blue;
				index3 += 3;
				source_index++;
			}
		}
	}
	vkUnmapMemory(device->device, screenshot->staging.memories[0]);
	return 0;
}


//! Helper for implement_screenshot(). Takes two LDR screenshots and turns then
//! into a single HDR screenshot.
void combine_ldr_screenshots_into_hdr(screenshot_t* screenshot) {
	VkExtent3D extent = screenshot->staging.images->image_info.extent;
	uint32_t pixel_count = extent.width * extent.height;
	uint32_t entry_count = 3 * pixel_count;
	for (uint32_t i = 0; i != entry_count; ++i) {
		uint16_t low_bits = screenshot->ldr_copy[i];
		uint16_t high_bits = screenshot->ldr_copy[i + entry_count];
		uint16_t half_bits = low_bits | (high_bits << 8);
		screenshot->hdr_copy[i] = half_to_float(half_bits);
	}
}


/*! Invoked once per frame just after submitting all drawing commands. If
	requested, the swapchain image is copied to an LDR buffer and stored as a
	screenshot. HDR screenshots arise from the combination of two LDR
	screenshots. 10-bit HDR is converted to 8-bit LDR, the alpha chennel is
	removed.
	\return 0 on success. On failure, rendering can proceed normally.*/
int implement_screenshot(screenshot_t* screenshot, const swapchain_t* swapchain, const device_t* device, uint32_t swapchain_index) {
	VkBool32 hdr_mode = (screenshot->path_hdr != NULL);
	if (!screenshot->path_png && !screenshot->path_jpg && !hdr_mode)
		return 0;
	// If we are just getting started, allocate staging memory
	if (screenshot->frame_bits != frame_bits_hdr_high) {
		if (create_screenshot_staging_buffers(screenshot, swapchain, device)) {
			destroy_screenshot(screenshot, device);
			return 1;
		}
	}
	// Grab swapchain contents and convert to LDR
	if (grab_screenshot_ldr(screenshot, swapchain, device, swapchain_index)) {
		destroy_screenshot(screenshot, device);
		return 1;
	}
	// Store the image
	if (screenshot->path_png) {
		int stride = swapchain->extent.width * 3 * sizeof(uint8_t);
		if (!stbi_write_png(screenshot->path_png, (int) swapchain->extent.width, (int) swapchain->extent.height, 3, screenshot->ldr_copy, stride)) {
			printf("Failed to store a screenshot to the *.png file at %s. Please check path and permissions.\n", screenshot->path_png);
			destroy_screenshot(screenshot, device);
			return 1;
		}
		printf("Wrote screenshot to %s.\n", screenshot->path_png);
	}
	if (screenshot->path_jpg) {
		if (!stbi_write_jpg(screenshot->path_jpg, (int) swapchain->extent.width, (int) swapchain->extent.height, 3, screenshot->ldr_copy, 70)) {
			printf("Failed to store a screenshot to the *.jpg file at %s. Please check path and permissions.\n", screenshot->path_jpg);
			destroy_screenshot(screenshot, device);
			return 1;
		}
		printf("Wrote screenshot to %s.\n", screenshot->path_jpg);
	}
	if (screenshot->path_hdr && screenshot->frame_bits == frame_bits_hdr_high) {
		// Combine both LDR frames into an HDR frame
		combine_ldr_screenshots_into_hdr(screenshot);
		// Store the screenshot
		if (!stbi_write_hdr(screenshot->path_hdr, (int) swapchain->extent.width, (int) swapchain->extent.height, 3, screenshot->hdr_copy)) {
			printf("Failed to store a screenshot to the *.hdr file at %s. Please check path and permissions.\n", screenshot->path_hdr);
			destroy_screenshot(screenshot, device);
			return 1;
		}
		printf("Wrote screenshot to %s.\n", screenshot->path_hdr);
	}
	if (screenshot->frame_bits == frame_bits_hdr_low)
		// The screenshot will be completed in the next frame
		screenshot->frame_bits = frame_bits_hdr_high;
	else
		destroy_screenshot(screenshot, device);
	return 0;
}

void destroy_query_pool(query_pool_t* query_pool, const device_t* device) {
	vkDestroyQueryPool(device->device, query_pool->pool, NULL);
}

int create_query_pool(query_pool_t* query_pool, swapchain_t* swapchain, const device_t* device) {
	// Create query pool for recording timestamps
	VkQueryPoolCreateInfo pool_create_info = {
		.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queryType = VK_QUERY_TYPE_TIMESTAMP,
		.queryCount = swapchain->image_count * 2,
	};
	if (vkCreateQueryPool(device->device, &pool_create_info, NULL, &query_pool->pool)) {
		printf("Failed to create query pool for querying timestamps\n");
		return 1;
	}

	return 0;
}

//! Destroys all objects associated with this application. Probably the last
//! thing you invoke before shutdown.
void destroy_application(application_t* app) {
	if(app->device.device)
		vkDeviceWaitIdle(app->device.device);
	destroy_frame_queue(&app->frame_queue, &app->device);
	destroy_interface_pass(&app->interface_pass, &app->device);
	destroy_copy_pass(&app->copy_pass, &app->device);
	destroy_accum_pass(&app->accum_pass, &app->device);
	destroy_shading_pass(&app->shading_pass, &app->device);
	destroy_geometry_pass(&app->geometry_pass, &app->device);
	destroy_render_pass(&app->render_pass, &app->device);
	destroy_render_targets(&app->render_targets, &app->device);
	destroy_light_textures(&app->light_textures, &app->device);
	destroy_light_buffers(&app->light_buffers, &app->device, app->allocator);
	destroy_constant_buffers(&app->constant_buffers, &app->device);
	destroy_ltc_table(&app->ltc_table, &app->device);
	destroy_scene(&app->scene, &app->device);
	destroy_experiment_list(&app->experiment_list);
	vmaDestroyAllocator(app->allocator);
	destroy_scene_specification(&app->scene_specification);
	destroy_query_pool(&app->query_pool, &app->device);
	destroy_swapchain(&app->swapchain, &app->device);
	destroy_vulkan_device(&app->device);
	destroy_imgui(app->imgui);
}


//! Callback to respond to window size changes
void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);


/*! Repeats all initialization procedures that need to be performed to
	implement the given update.
	\return 0 on success.*/
int update_application(application_t* app, const application_updates_t* update_in, uint32_t* reset_accum) {
	application_updates_t update = *update_in;
	// Perform a quick save if requested
	if (update.quick_save) quick_save(&app->scene_specification);
	// Check if a window resize is requested
	uint32_t width = (update.window_width != 0) ? update.window_width : app->swapchain.extent.width;
	uint32_t height = (update.window_height != 0) ? update.window_height : app->swapchain.extent.height;
	if (app->swapchain.extent.width != width || app->swapchain.extent.height != height) {
		glfwSetWindowSize(app->swapchain.window, (int) width, (int) height);
		update.recreate_swapchain = VK_TRUE;
	}
	// Return early, if there is nothing to update
	if (!update.startup && !update.recreate_swapchain && !update.reload_shaders
		&& !update.quick_load && !update.update_light_count && !update.update_light_textures
		&& !update.reload_scene && !update.change_shading)
		return 0;
	// Perform a quick load
	if (update.quick_load)
		quick_load(&app->scene_specification, &update);
	// Flag objects that need to be rebuilt because something changed directly
	VkBool32 swapchain = update.recreate_swapchain;
	VkBool32 ltc_table = update.startup;
	VkBool32 scene = update.startup | update.reload_scene;
	VkBool32 render_targets = update.startup;
	VkBool32 render_pass = update.startup;
	VkBool32 constant_buffers = update.startup | update.update_light_count | update.change_shading;
	VkBool32 light_buffers = update.startup | update.update_light_count;	// TODO: Verify if change_shading is required
	VkBool32 light_textures = update.startup | update.reload_scene | update.update_light_count | update.update_light_textures;
	VkBool32 geometry_pass = update.startup | update.reload_shaders;
	VkBool32 accum_pass = update.startup | update.reload_shaders;
	VkBool32 copy_pass = update.startup | update.reload_shaders;
	VkBool32 shading_pass = update.startup | update.change_shading | update.reload_shaders;
	VkBool32 interface_pass = update.startup | update.reload_shaders;
	VkBool32 frame_queue = update.startup;
	// Now propagate dependencies (as indicated by the parameter lists of
	// create functions)
	uint32_t max_dependency_path_length = 16;
	for (uint32_t i = 0; i != max_dependency_path_length; ++i) {
		render_targets |= swapchain;
		render_pass |= swapchain | render_targets;
		constant_buffers |= swapchain;
		geometry_pass |= swapchain | scene | constant_buffers | render_targets;
		shading_pass |= swapchain | ltc_table | scene | render_targets | constant_buffers | light_buffers | light_textures | geometry_pass | shading_pass | interface_pass | frame_queue;
		interface_pass |= swapchain | render_targets;
		frame_queue |= swapchain;
		accum_pass |= swapchain | render_targets;
		copy_pass |= swapchain | render_targets;
	}
	// Tear down everything that needs to be reinitialized in reverse order
	vkDeviceWaitIdle(app->device.device);
	if (frame_queue) destroy_frame_queue(&app->frame_queue, &app->device);
	if (interface_pass) destroy_interface_pass(&app->interface_pass, &app->device);
	if (copy_pass) destroy_copy_pass(&app->copy_pass, &app->device);
	if (accum_pass) destroy_accum_pass(&app->accum_pass, &app->device);
	if (shading_pass) destroy_shading_pass(&app->shading_pass, &app->device);
	if (geometry_pass) destroy_geometry_pass(&app->geometry_pass, &app->device);
	if (light_textures) destroy_light_textures(&app->light_textures, &app->device);
	if (light_buffers) destroy_light_buffers(&app->light_buffers, &app->device, app->allocator);
	if (constant_buffers) destroy_constant_buffers(&app->constant_buffers, &app->device);
	if (render_pass) destroy_render_pass(&app->render_pass, &app->device);
	if (render_targets) destroy_render_targets(&app->render_targets, &app->device);
	if (scene) destroy_scene(&app->scene, &app->device);
	if (ltc_table) destroy_ltc_table(&app->ltc_table, &app->device);
	// Attempt to recreate the swapchain and finish early if the window is
	// minimized
	if (swapchain) {
		int swapchain_result = create_or_resize_swapchain(&app->swapchain, &app->device, VK_TRUE, "", 0, 0, app->render_settings.v_sync);
		if (swapchain_result == 2)
			return 0;
		else if (swapchain_result) {
			printf("Swapchain resize failed.\n");
			return 1;
		}
	}
	// Rebuild everything else
	if (   (ltc_table && load_ltc_table(&app->ltc_table, &app->device, "data/ggx_ltc_fit", 51))
		|| (scene && load_scene(&app->scene, &app->device, app->scene_specification.file_path, app->scene_specification.texture_path, VK_TRUE))
		|| (render_targets && create_render_targets(&app->render_targets, &app->device, &app->swapchain))
		|| (render_pass && create_render_pass(&app->render_pass, &app->device, &app->swapchain, &app->render_targets))
		|| (constant_buffers && create_constant_buffers(&app->constant_buffers, &app->device, &app->swapchain, &app->scene_specification, &app->render_settings))
		|| (light_buffers && create_light_buffers(&app->light_buffers, &app->device, &app->swapchain, &app->scene_specification, app))
		|| (light_textures && create_and_assign_light_textures(&app->light_textures, &app->device, &app->scene_specification))
		|| (geometry_pass && create_geometry_pass(&app->geometry_pass, &app->device, &app->swapchain, &app->scene, &app->constant_buffers, &app->render_targets, &app->render_pass))
		|| (shading_pass && create_shading_pass(&app->shading_pass, app))
		|| (accum_pass && create_accum_pass(&app->accum_pass, app))
		|| (copy_pass && create_copy_pass(&app->copy_pass, app))
		|| (interface_pass && create_interface_pass(&app->interface_pass, &app->device, app->imgui, &app->swapchain, &app->render_targets, &app->render_pass))
		|| (frame_queue && create_frame_queue(&app->frame_queue, &app->device, &app->swapchain)))
		return 1;
	// If we are here, something has changed and we need to reset the accumalation count
	*reset_accum = 1;
	return 0;
}


/*! Creates all objects needed by this application at start up. Once the
	function returns, everything is ready for rendering the first frame.
	\param experiment_index An index of an experiment in the experiment list
		that should be used for the initial configuration instead of the
		default configuration. An invalid index implies the default.
	\param v_sync_override Lets you force v-sync on or off.
	\return 0 on success.*/
int startup_application(application_t* app, int experiment_index, bool_override_t v_sync_override, bool_override_t run_all_exp) {
	memset(app, 0, sizeof(*app));
	g_glfw_application = app;
	const char application_display_name[] = "Vulkan renderer";
	const char application_internal_name[] = "vulkan_renderer";
	// Create the device
	if (create_vulkan_device(&app->device, application_internal_name, 0, VK_TRUE)) {
		destroy_application(app);
		return 1;
	}
	// Define available experiments
	create_experiment_list(&app->experiment_list);
	// Specify the scene, settings and experiments
	if (experiment_index >= 0 && experiment_index < app->experiment_list.count) {
		const experiment_t* experiment = &app->experiment_list.experiments[experiment_index];
		// Set up the scene
		app->scene_specification.file_path = copy_string(g_scene_paths[experiment->scene_index][1]);
		app->scene_specification.texture_path = copy_string(g_scene_paths[experiment->scene_index][2]);
		const char* quicksave_path = experiment->quick_save_path;
		if (!quicksave_path) quicksave_path = g_scene_paths[experiment->scene_index][3];
		app->scene_specification.quick_save_path = copy_string(quicksave_path);
		quick_load(&app->scene_specification, NULL);
		// Set render settings
		app->render_settings = experiment->render_settings;
		if (v_sync_override != bool_override_none) app->render_settings.v_sync = v_sync_override;
	}
	else {
		specify_default_scene(&app->scene_specification);
		specify_default_render_settings(&app->render_settings);

		if (run_all_exp) {
			// Start running experiments after start
			app->experiment_list.next = 0;
			app->experiment_list.state = experiment_state_new_experiment;
		}
	}
	app->run_all_exp = run_all_exp;
	app->accum_num = 0;
	app->timings = NULL;
	// Create the swapchain
	if (create_or_resize_swapchain(&app->swapchain, &app->device, VK_FALSE, application_display_name, 1920, 1080, app->render_settings.v_sync)) {
		destroy_application(app);
		return 1;
	}
	// Create query pool for recording timestamps
	if (create_query_pool(&app->query_pool, &app->swapchain, &app->device)) {
		destroy_application(app);
		return 1;
	}
	glfwSetFramebufferSizeCallback(app->swapchain.window, &glfw_framebuffer_size_callback);
	// Prepare imgui for being used
	app->imgui = init_imgui(app->swapchain.window);

	// Initialize Vulkan Memory Allocator
	VmaVulkanFunctions vulkanFunctions = {0};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {0};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
	allocatorCreateInfo.physicalDevice = app->device.physical_device;
	allocatorCreateInfo.device = app->device.device;
	allocatorCreateInfo.instance = app->device.instance;
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

	vmaCreateAllocator(&allocatorCreateInfo, &app->allocator);
	
	// Load and create everything else
	uint32_t tmp;
	application_updates_t update = { .startup = VK_TRUE };
	if (update_application(app, &update, &tmp)) {
		destroy_application(app);
		return 1;
	}
	return 0;
}

//! Helper function for advance_experiments which sets up a new experiment
void setup_experiment(application_updates_t* updates, experiment_list_t* list, scene_specification_t* scene, render_settings_t* render_settings, uint32_t *accum_num, FILE** timings) {
	// Prepare the next experiment
	list->experiment = &list->experiments[list->next];
	
	mkdir(list->experiment->screenshots_dir);

	// Define when this experiment will end (number of samples to take)
	list->next_setup_frame = list->experiment->num_samples;
	list->state = experiment_state_rendering;

	if (!list->experiment->ss_per_frame) {
		// Open timings file
		*timings = fopen_setvbuf(list->experiment->timings_path, "w");
	}

	// Adjust the resolution as needed
	updates->window_width = list->experiment->width;
	updates->window_height = list->experiment->height;
	// Prepare the new scene
	if (strcmp(scene->file_path, g_scene_paths[list->experiment->scene_index][1]) != 0) {
		scene->file_path = copy_string(g_scene_paths[list->experiment->scene_index][1]);
		scene->texture_path = copy_string(g_scene_paths[list->experiment->scene_index][2]);
		updates->reload_scene = VK_TRUE;
	}
	// Prepare camera and lights
	if (list->experiment->quick_save_path)
		scene->quick_save_path = copy_string(list->experiment->quick_save_path);
	else
		scene->quick_save_path = copy_string(g_scene_paths[list->experiment->scene_index][3]);
	updates->quick_load = VK_TRUE;
	// Ensure that render settings are applied properly
	if (render_settings->v_sync != list->experiment->render_settings.v_sync)
		updates->recreate_swapchain = VK_TRUE;
	updates->change_shading = VK_TRUE;
	(*render_settings) = list->experiment->render_settings;

	// Reset the accum_num here to get correct filename
	*accum_num = 0;

	list->state = experiment_state_rendering;
}

//! Helper function for advance_experiments which cleans up an setup experiment
int cleanup_experiment(experiment_list_t* list, FILE** timings) {
	// We are done with the experiment
	// Close the timings file
	if (timings) {
		fclose(*timings);
		*timings = NULL;
	}

	// We are done with all experiments
	if (list->next + 1 == list->count) {
		// Set this so that we know when to exit
		list->state = experiment_state_new_experiment;
		list->experiment = NULL;
		list->next = list->count + 1;
		return 1;
	}

	// Proceed with the next experiment
	list->state = experiment_state_new_experiment;
	++list->next;

	return 0;
}

//!	Checks if it is time to complete an experiment and to prepare the next one
//! and updates settings accordingly. Returns 1 if experiments go from runnning
//! to stopped which can be used to exit the renderer. Otherwise return 0.
//! This function will exit the renderer whenever all experiments are done and
//! only supports HDR images for now
int advance_experiments(screenshot_t* screenshot, application_updates_t* updates, experiment_list_t* list, scene_specification_t* scene, render_settings_t* render_settings, uint32_t *accum_num, FILE** timings) {
	if (list->next > list->count) {
		if (list->state == experiment_state_new_experiment) return 1;
		// Experiments are not running
		list->state = experiment_state_rendering;
		return 0;
	}

	if (list->state == experiment_state_new_experiment) {
		setup_experiment(updates, list, scene, render_settings, accum_num, timings);
	}

	VkBool32 ss_per_frame = VK_FALSE;
	if (list->experiment) ss_per_frame = list->experiment->ss_per_frame;

	if (ss_per_frame == VK_TRUE) {
		if (list->state == experiment_state_screenshot_frame_0) {
			if (list->next_setup_frame > *accum_num + 1) {
				// Continue rendering
				list->state = experiment_state_rendering;
			} else {
				cleanup_experiment(list, NULL);
			}
		} else if (list->state == experiment_state_rendering) {
			// Take a screenshot for the current experiment (if any)
			if (*accum_num % 10 == 0) {
				if (list->experiment) {
					const experiment_t* exp = list->experiment;

					// Replace screenshot path with current sample number
					char *filename = format_uint("/%05d", *accum_num);
					const char* screenshot_path_peices[] = {exp->base_dir, exp->exp_name, filename, ".", exp->ext};
					char *tmp = concatenate_strings(COUNT_OF(screenshot_path_peices), screenshot_path_peices);
					strcpy(exp->screenshot_path, tmp);
					free(tmp);
					free(filename);

					if (list->experiment->use_hdr)
						take_screenshot(screenshot, NULL, NULL, list->experiment->screenshot_path);
					else
						take_screenshot(screenshot, list->experiment->screenshot_path, NULL, NULL);
				}
				// End the current experiment
				list->state = experiment_state_screenshot_frame_0;
			}
		}
	} else {
		if (list->state == experiment_state_screenshot_frame_0) {
			cleanup_experiment(list, timings);
		} else if (list->state == experiment_state_rendering && list->next_setup_frame <= *accum_num) {
			// Take a screenshot for the current experiment (if any)
			if (list->experiment) {
				const experiment_t* exp = list->experiment;

				// Replace screenshot path with current sample number
				char *filename = format_uint("/%05d", *accum_num);
				const char* screenshot_path_peices[] = {exp->base_dir, exp->exp_name, filename, ".", exp->ext};
				char *tmp = concatenate_strings(COUNT_OF(screenshot_path_peices), screenshot_path_peices);
				strcpy(exp->screenshot_path, tmp);
				free(tmp);
				free(filename);

				if (list->experiment->use_hdr)
					take_screenshot(screenshot, NULL, NULL, list->experiment->screenshot_path);
				else
					take_screenshot(screenshot, list->experiment->screenshot_path, NULL, NULL);
			}
			// End the current experiment
			list->state = experiment_state_screenshot_frame_0;
		}
	}

	return 0;
}


void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	application_t* app = g_glfw_application;
	swapchain_t* swapchain = app ? &app->swapchain : NULL;
	if (swapchain) {
	    // Query the new framebuffer size and do an early out if that is the
	    // swapchain size already
        int framebuffer_width = 0, framebuffer_height = 0;
        glfwGetFramebufferSize(swapchain->window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width == (int) swapchain->extent.width && framebuffer_height == (int) swapchain->extent.height)
            return;
        // Implement the update
		application_updates_t updates = {VK_FALSE};
		updates.recreate_swapchain = VK_TRUE;
		uint32_t reset_accum = 0;
		if (update_application(app, &updates, &reset_accum)) {
			printf("Swapchain resize failed.\n");
			glfwSetWindowShouldClose(window, 1);
			return;
		}
		app->accum_num = 0;
	}
}


/*! Given a key code such as GLFW_KEY_F5, this function returns whether this
	key is currently pressed but was not pressed when this function was last
	invoked for this key (or this function was not invoked for this key 
	yet).*/
VkBool32 key_pressed(GLFWwindow* window, int key) {
	if (key < 0 || key >= GLFW_KEY_LAST)
		return VK_FALSE;
	static int key_state[GLFW_KEY_LAST + 1] = {GLFW_RELEASE};
	int current_state = glfwGetKey(window, key);
	VkBool32 result = (current_state == GLFW_PRESS && key_state[key] == GLFW_RELEASE);
	key_state[key] = current_state;
	return result;
}


/*! Implements user input and scene updates. Invoke this once per frame.
	\return 0 if the application should keep running, 1 if it needs to end.*/
int handle_frame_input(application_t* app) {
	// Define the user interface for the current frame
	application_updates_t updates = { VK_FALSE };
	uint32_t reset_accum = 0;
	specify_user_interface(&updates, app, get_frame_time(0), &reset_accum);
	// Pressing escape ends the application
	GLFWwindow* window = app->swapchain.window;
	if (key_pressed(window, GLFW_KEY_ESCAPE)) {
		printf("Escape pressed. Shutting down.\n");
		return 1;
	}
	// Reload shaders on request
	if (key_pressed(window, GLFW_KEY_F5)) {
		printf("Reloading all shaders.\n");
		updates.reload_shaders = VK_TRUE;
	}
	// Quick save and quick load
	if (key_pressed(window, GLFW_KEY_F3)) {
		printf("Quick save.\n");
		updates.quick_save = VK_TRUE;
	}
	if (key_pressed(window, GLFW_KEY_F4)) {
		printf("Quick load.\n");
		updates.quick_load = VK_TRUE;
	}
	// Take a screenshot
	if (key_pressed(app->swapchain.window, GLFW_KEY_F10))
		take_screenshot(&app->screenshot, "data/screenshot.png", NULL, NULL);
	if (key_pressed(app->swapchain.window, GLFW_KEY_F12))
		take_screenshot(&app->screenshot, NULL, NULL, "data/screenshot.hdr");
	// Toggle the user interface
	app->render_settings.show_gui ^= key_pressed(window, GLFW_KEY_F1);
	// Toggle v-sync
	if (key_pressed(window, GLFW_KEY_F2)) {
		app->render_settings.v_sync ^= 1;
		updates.recreate_swapchain = VK_TRUE;
	}
	// If things went badly in the previous frame, we may want to resize the
	// swapchain
	if (app->frame_queue.recreate_swapchain) {
		app->frame_queue.recreate_swapchain = VK_FALSE;
		updates.recreate_swapchain = VK_TRUE;
	}
	// Cycle through experiments (if they are ongoing)
	int exp_done = advance_experiments(&app->screenshot, &updates, &app->experiment_list, &app->scene_specification, &app->render_settings, &app->accum_num, &app->timings);
	if (exp_done && app->run_all_exp) {
		printf("All experiments finished. Shutting down.\n");
		return 1;
	}
	// Handle updates
	if (update_application(app, &updates, &reset_accum)) {
		printf("Failed to apply changed settings. Shutting down.\n");
		return 1;
	}
	// Update the camera
	control_camera(&app->scene_specification.camera, app->swapchain.window, &reset_accum);

	// Reset accumulation if needed
	if (reset_accum) app->accum_num = 0;

	return 0;
}


//! Writes constants matching the current state of the application to the given
//! memory location
void write_constants(void* data, application_t* app) {
	const scene_t* scene = &app->scene;
	const first_person_camera_t* camera = &app->scene_specification.camera;
	double cursor_position[2];
	glfwGetCursorPos(app->swapchain.window, &cursor_position[0], &cursor_position[1]);
	per_frame_constants_t constants = {
		.mesh_dequantization_factor = {scene->mesh.dequantization_factor[0], scene->mesh.dequantization_factor[1], scene->mesh.dequantization_factor[2]},
		.mesh_dequantization_summand = {scene->mesh.dequantization_summand[0], scene->mesh.dequantization_summand[1], scene->mesh.dequantization_summand[2]},
		.camera_position_world_space = {camera->position_world_space[0], camera->position_world_space[1], camera->position_world_space[2]},
		.mis_visibility_estimate = app->render_settings.mis_visibility_estimate,
		.viewport_size = app->swapchain.extent,
		.cursor_position = { (int32_t) cursor_position[0], (int32_t) cursor_position[1] },
		.ltc_constants = app->ltc_table.constants,
		.exposure_factor = app->render_settings.exposure_factor,
		.roughness_factor = app->render_settings.roughness_factor,
	};
	set_noise_constants(constants.noise_resolution_mask, &constants.noise_texture_index_mask, constants.noise_random_numbers, &app->noise_table, app->render_settings.animate_noise);
	get_world_to_projection_space(constants.world_to_projection_space, camera, get_aspect_ratio(&app->swapchain));
	// Construct the transform that produces ray directions from pixel
	// coordinates
	float viewport_transform[4];
	viewport_transform[0] = 2.0f / app->swapchain.extent.width;
	viewport_transform[1] = 2.0f / app->swapchain.extent.height;
	viewport_transform[2] = 0.5f * viewport_transform[0] - 1.0f;
	viewport_transform[3] = 0.5f * viewport_transform[1] - 1.0f;
	float projection_to_world_space_no_translation[4][4];
	float world_to_projection_space_no_translation[4][4];
	memcpy(world_to_projection_space_no_translation, constants.world_to_projection_space, sizeof(world_to_projection_space_no_translation));
	world_to_projection_space_no_translation[0][3] = 0.0f;
	world_to_projection_space_no_translation[1][3] = 0.0f;
	world_to_projection_space_no_translation[2][3] = 0.0f;
	matrix_inverse(projection_to_world_space_no_translation, world_to_projection_space_no_translation);
	float pixel_to_ray_direction_projection_space[4][3] = {
		{viewport_transform[0], 0.0f,	viewport_transform[2]},
		{0.0f, viewport_transform[1],	viewport_transform[3]},
		{0.0f,					0.0f,	1.0f},
		{0.0f,					0.0f,	1.0f},
	};
	for (uint32_t i = 0; i != 3; ++i)
		for (uint32_t j = 0; j != 3; ++j)
			for (uint32_t k = 0; k != 4; ++k)
				constants.pixel_to_ray_direction_world_space[i][j] += projection_to_world_space_no_translation[i][k] * pixel_to_ray_direction_projection_space[k][j];
	memcpy(data, &constants, sizeof(constants));
}


/*! Renders a frame for the current state of the scene.
	\note This function mostly takes care of synchronization and submits
		previously created command buffers for rendering.
	\see record_render_frame_commands()
	\return 0 if the application should keep running, 1 if something went
		terribly wrong and it has to end.*/
int render_frame(application_t* app) {
	// Get synchronization objects
	frame_queue_t* queue = &app->frame_queue;
	queue->sync_index = (queue->sync_index + 1) % queue->frame_count;
	frame_sync_t* sync = &queue->syncs[queue->sync_index];
	// Acquire the next swapchain image
	uint32_t swapchain_index;
	if (vkAcquireNextImageKHR(app->device.device, app->swapchain.swapchain, UINT64_MAX, sync->image_acquired, NULL, &swapchain_index)) {
		printf("Failed to acquire the next image from the swapchain.\n");
		return 1;
	}
	frame_workload_t* workload = &queue->workloads[swapchain_index];
	// Perform GPU-CPU synchronization to be sure that resources that we are
	// going to overwrite now are no longer used for rendering
	if (workload->used) {
		VkResult fence_result;
		do {
			fence_result = vkWaitForFences(app->device.device, 1, &workload->drawing_finished_fence, VK_TRUE, 100000000);
		} while (fence_result == VK_TIMEOUT);
		if (fence_result != VK_SUCCESS) {
			printf("Failed to wait for rendering of a frame to finish.\n");
			return 1;
		}
		if (vkResetFences(app->device.device, 1, &workload->drawing_finished_fence)) {
			printf("Failed to reset a fence for reuse in upcoming frames.\n");
			return 1;
		}
	}
	workload->used = VK_TRUE;
	// Update the constant buffer
	write_constants((char*) app->constant_buffers.data + app->constant_buffers.buffers.buffers[swapchain_index].offset, app);
	VkMappedMemoryRange constant_range = {
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = app->constant_buffers.buffers.memory,
		.size = get_mapped_memory_range_size(&app->device, &app->constant_buffers.buffers, swapchain_index),
		.offset = app->constant_buffers.buffers.buffers[swapchain_index].offset
	};
	vkFlushMappedMemoryRanges(app->device.device, 1, &constant_range);
	// Record the command buffer for rendering
	if (record_render_frame_commands(workload->command_buffer, app, swapchain_index)) {
		printf("Failed to record a command buffer for rendering the scene.\n");
		return 1;
	}
	// Queue the command buffer for rendering
	VkPipelineStageFlags destination_stage_masks[] = {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo render_submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &workload->command_buffer,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &sync->image_acquired,
		.pWaitDstStageMask = destination_stage_masks,
	};
	if (vkQueueSubmit(app->device.queue, 1, &render_submit_info, workload->drawing_finished_fence)) {
		printf("Failed to submit the command buffer for rendering a frame to the queue.\n");
		return 1;
	}
	
	// Record frametimes
	record_frame_time(swapchain_index, app->query_pool.pool, app->device.device, app->device.physical_device_properties.limits.timestampPeriod, app->timings, app->accum_num);

	// Take a screenshot if requested
	implement_screenshot(&app->screenshot, &app->swapchain, &app->device, swapchain_index);
	// Present the image in the window
	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.swapchainCount = 1,
		.pSwapchains = &app->swapchain.swapchain,
		.pImageIndices = &swapchain_index
	};
	VkResult present_result;
	if (present_result = vkQueuePresentKHR(app->device.queue, &present_info)) {
		printf("Failed to present the rendered frame to the window. Error code %d. Attempting a swapchain resize.\n", present_result);
		app->frame_queue.recreate_swapchain = VK_TRUE;
	}
	
	// Only increment if we are not currently taking a HDR screenshot
	// If HDR is requested, LDR screenshot was taken in this frame.
	// If that is the case, we don't want to increment the accum_num
	if (app->render_settings.accum && app->screenshot.frame_bits < 2) {
		if (app->run_all_exp && app->accum_num % 1000 == 0)
			printf("%d Samples Completed\n", app->accum_num);
		app->accum_num += 1;
	}

	return 0;
}


int main(int argc, char** argv) {
	// Parse settings, e.g. which experiment should be shown
	int experiment = -1;
	bool_override_t v_sync_override = bool_override_none;
	bool_override_t gui_override = bool_override_none;
	bool_override_t run_all_exp = bool_override_false;
	for (int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if (arg[0] == '-' && arg[1] == 'e') sscanf(arg + 2, "%d", &experiment);
		if (strcmp(arg, "-no_v_sync") == 0) v_sync_override = bool_override_false;
		if (strcmp(arg, "-v_sync") == 0) v_sync_override = bool_override_true;
		if (strcmp(arg, "-no_gui") == 0) gui_override = bool_override_false;
		if (strcmp(arg, "-gui") == 0) gui_override = bool_override_true;
		if (strcmp(arg, "-run_exp") == 0) run_all_exp = bool_override_true;
	}
	// Start the application
	application_t app;
	if (startup_application(&app, experiment, v_sync_override, run_all_exp)) {
		printf("Application startup has failed.\n");
		return 1;
	}
	if (gui_override != bool_override_none) app.render_settings.show_gui = gui_override;
	// Main loop
	while (!glfwWindowShouldClose(app.swapchain.window)) {
		glfwPollEvents();
		// Check whether the window is minimized
		if (app.swapchain.swapchain) {
			if (handle_frame_input(&app)) break;
			if (render_frame(&app)) break;
		}
	}
	// Clean up
	destroy_application(&app);
	return 0;
}

FILE* fopen_setvbuf(const char *file_path, const char *mode) {
	FILE* file = fopen(file_path, mode);
	if(file) setvbuf(file, NULL, _IOFBF, 64 * 1024);

	return file;
}