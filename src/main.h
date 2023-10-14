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
#include "noise_table.h"
#include "camera.h"
#include "polygonal_light.h"
#include "ltc_table.h"
#include "scene.h"
#include "imgui_vulkan.h"
#include "vk_mem_alloc.h"

/*! Holds all information that characterizes the scene (geometry, materials,
	lighting and camera). It does not hold the loaded objects.*/
typedef struct scene_specification_s {
	//! Path to the *.vks file holding scene geometry
	char* file_path;
	//! Path to the directory holding scene textures
	char* texture_path;
	//! Path to the file from which light sources and camera have been loaded
	char* quick_save_path;
	//! The current camera
	first_person_camera_t camera;
	//! Number of polygonal lights illuminating the scene
	uint32_t polygonal_light_count;
	//! The polygonal lights illuminating the scene
	polygonal_light_t* polygonal_lights;
} scene_specification_t;

//! Available methods to combine diffuse and specular samples
typedef enum sampling_strategies_e {
	//! Only the sampling strategy for diffuse samples is used but the full
	//! BRDF is evaluated
	sampling_strategies_diffuse_only,
	//! Sampling strategies for diffuse and specular samples are used and
	//! combined by multiple importance sampling
	sampling_strategies_diffuse_specular_mis,
	//! Number of available sampling strategies
	sampling_strategies_count
} sampling_strategies_t;

//! Available variants of multiple importance sampling. See Veach's thesis:
//! http://graphics.stanford.edu/papers/veach_thesis/
typedef enum mis_heuristic_e {
	//! The balance heuristic as described by Veach (p. 264)
	mis_heuristic_balance,
	//! The power heuristic with exponent 2 as described by Veach (p. 273)
	mis_heuristic_power,
	//! Our weighted variant of the balance heuristic that incorporates
	//! estimates of the unshadowed reflected radiance
	mis_heuristic_weighted,
	//! Our optimal MIS strategy, clamped to only use non-negative weights
	mis_heuristic_optimal_clamped,
	//! Our optimal MIS strategy. It is a blend between balance heuristic and
	//! weighted balance heuristic and under some fairly weak assumptions it is
	//! provably optimal for some degree of visibility. It uses negative
	//! weights and produces non-zero splats (with zero-mean) for occluded
	//! rays. This way, it reduces variance a bit but introduces leaking.
	mis_heuristic_optimal,
	//! Number of available heuristics
	mis_heuristic_count
} mis_heuristic_t;

typedef enum light_sampling_strategies_e {
	//! Use uniform random light sampling
	light_uniform,
	//! Use reservoir sampling
	light_reservoir
} light_sampling_strategies_t;

//! Either defines a boolean value or leaves it undefined
typedef enum bool_override_e {
	bool_override_false = 0,
	bool_override_true = 1,
	bool_override_none = 2,
} bool_override_t;

//! Options that control how the scene will be rendered
typedef struct render_settings_s {
	//! Constant factors for the overall brightness and surface roughness
	float exposure_factor, roughness_factor;
	//! The number of samples used per sampling technique
	uint32_t sample_count;
	//! The number of samples used for choosing lights
	uint32_t sample_count_light;
	//! The heuristic used for multiple importance sampling
	mis_heuristic_t mis_heuristic;
	//! Light sampling techniques
	light_sampling_strategies_t light_sampling;
	//! An estimate of how much each shading point is shadowed. Used as a
	//! parameter to control optimal multiple importance sampling.
	float mis_visibility_estimate;
	//! The technique used to sample polygonal lights
	sample_polygon_technique_t polygon_sampling_technique;
	//! Whether noise should be updated each frame
	VkBool32 animate_noise;
	//! Whether to accumulate frames
	VkBool32 accum;
	//! Whether light sources should be rendered
	VkBool32 show_polygonal_lights;
	//! Whether the user interface should be rendered
	VkBool32 show_gui;
	//! Whether vertical synchronization should be used (caps frame rate)
	VkBool32 v_sync;
	//! Whether to use fast atan
	VkBool32 fast_atan;
} render_settings_t;


//! An enumeration of indices for g_scene_paths, i.e. it lists available scenes
typedef enum scene_index_e
{
	scene_bistro_inside,
	scene_bistro_outside,
	scene_zeroday,
	scene_count
} scene_index_t;

/*! For each available scene, this array holds its display name, the path to
	the *.vks file, the directory holding the textures and the path to the
	quick save file.*/
extern const char* const g_scene_paths[scene_count][4];


//! Specifies a scene, a camera, lighting and render settings. Overall, a frame
//! to be rendered is characterized completely.
typedef struct experiment_s {
	//! The swapchain resolution that should be used for this experiment. May
	//! be zero to indicate no preference.
	uint32_t width, height;
	//! Index into g_scene_paths to specify the scene
	scene_index_t scene_index;
	//! The path to the quick save specifying camera and lighting. If it is,
	//! NULL the default quicksave for the scene is used.
	char* quick_save_path;
	//! VK_FALSE if screenshot_path is a *.png, VK_TRUE if it is an *.hdr
	VkBool32 use_hdr;
	//! The path at which a screenshot with the result of this experiment
	//! should be stored. It must be a format string consuming a float for the
	//! frame time in milliseconds. The file format extension should be *.png.
	char* screenshot_path;
	//! Total number of samples to shoot
	uint32_t num_samples;
	//! The render settings to be used
	render_settings_t render_settings;
	//! The base directory to store timings
	char* base_dir;
	//! Path to timings
	char* timings_path;
	//! Path to per frame screenshots
	char* screenshots_dir;
	//! The extension to use for screenshots
	char* ext;
	//! The name of the experiment, a directory with this name
	//! should be created beforehand
	char* exp_name;
	//! Whether to dump a screenshot every frame
	VkBool32 ss_per_frame;
} experiment_t;


//! The various stages of performing an experiment
typedef enum experiment_state_e {
	//! The renderer is rendering without disturbance
	experiment_state_rendering,
	//! First frame for taking a screenshot
	experiment_state_screenshot_frame_0,
	//! Second frame for taking a screenshot (needed for HDR output)
	experiment_state_screenshot_frame_1,
	//! The next experiment has been set up in the previous frame
	experiment_state_new_experiment,
} experiment_state_t;


//! Holds a list of experiments to perform and keeps track of the progress
typedef struct experiment_list_s {
	//! An array of experiments to be performed on request
	experiment_t* experiments;
	//! The currently running experiment
	const experiment_t* experiment;
	//! Number of experiments in experiments
	uint32_t count;
	//! The experiment to perform next (greater than experiment_count to
	//! indicate that no experiments are running)
	uint32_t next;
	//! The frame_index at which the current experiment should be recorded
	uint32_t next_setup_frame;
	//! The current state of execution of experiments
	experiment_state_t state;
	//! File pointer to the file being used to store timings
	FILE* timings_file;
} experiment_list_t;


/*! Provides convenient access to all render targets used by this application,
	except for swapchain images. These render targets are duplicated per
	swapchain image, to enable overlapping execution of work between frames.*/
typedef struct render_targets_s {
	//! The number of held render targets per swapchain image, i.e. the number
	//! of entries in the union below
	uint32_t target_count;
	//! The number of duplicates for each target, i.e. the number of swapchain
	//! images when the render targets were allocated
	uint32_t duplicate_count;
	//! Images for all allocated render targets (not including the swapchain)
	images_t targets_allocation;
	//! This union provides convenient access to all render targets, either as
	//! array or by name. The pointer is a pointer to an array of length
	//! duplicate_count.
	union {
		struct {
			//! The depth buffer used in the geometry pass
			image_t depth_buffer;
			//! The visibility buffer, which stores a primitive index per pixel
			image_t visibility_buffer;
			//! The shading buffer, which stores output for 1 spp
			image_t shading_buffer;
			//! The accumulation buffer, which stores output for N spp
			image_t accum_buffer;
		};
		//! Array of all render targets available from this object
		image_t targets[4];
	}* targets;
} render_targets_t;


//! Keeps track of all constant buffers used in this application
typedef struct constant_buffers_s {
	//! One copy of the constant buffer per swapchain image
	buffers_t buffers;
	//! Pointer where the data of constant_buffer is mapped
	void* data;
} constant_buffers_t;

//! Keeps track of all light buffers used in this application
typedef struct light_buffers_s {
	//! One copy of the light buffer per swapchain image
	VkBuffer buffer;
	VmaAllocation allocation;
	uint32_t size;
} light_buffers_t;

//! The sub pass that produces the visibility buffer by rasterizing all
//! geometry once
typedef struct geometry_pass_s {
	//! Pipeline state and bindings for the geometry pass
	pipeline_with_bindings_t pipeline;
	//! The used vertex and fragment shader
	shader_t vertex_shader, fragment_shader;
} geometry_pass_t;


//! The sub pass that renders a screen filling triangle to perform deferred
//! shading in a fragment shader, possibly with ray queries for shadows
typedef struct shading_pass_s {
	//! Pipeline state and bindings for the shading pass
	pipeline_with_bindings_t pipeline;
	//! The vertex and fragment shader that implements the shading pass
	shader_t vertex_shader, fragment_shader;
	//! The sampler for light textures
	VkSampler light_texture_sampler;
} shading_pass_t;

//! The sub pass that renders a screen filling triangle to perform deferred
//! shading in a fragment shader, possibly with ray queries for shadows
typedef struct accum_pass_s {
	//! Pipeline state and bindings for the shading pass
	pipeline_with_bindings_t pipeline;
	//! The vertex and fragment shader that implements the shading pass
	shader_t vertex_shader, fragment_shader;
} accum_pass_t;

//! The sub pass that renders a screen filling triangle to perform copy
//! the accumulated buffer into the swapchain
typedef struct copy_pass_s {
	//! Pipeline state and bindings for the copy pass
	pipeline_with_bindings_t pipeline;
	//! The vertex and fragment shader that implements the shading pass
	shader_t vertex_shader, fragment_shader;
} copy_pass_t;

//! The sub pass that renders the user interface on top of the shaded frame
typedef struct interface_pass_s {
	//! Buffers holding all geometry for the interface pass. They are
	//! duplicated once per swapchain image.
	buffers_t geometry_allocation;
	//! A pointer to the buffers held by geometry_allocation with union type
	//! for convenient access by name. The array index is the swapchain index.
	union {
		struct {
			//! Vertex buffer for imgui geometry
			buffer_t vertices;
			//! Index buffer for imgui geometry
			buffer_t indices;
		};
		//! All buffers for the interface pass (same array as in
		//! geometry_allocation)
		buffer_t buffers[2];
	}* geometries;
	//! A pointer to the mapped memory of geometry_allocation
	void* geometry_data;
	//! The number of array entries in geometries, i.e. the number of swapchain
	//! images when this object was created
	uint32_t frame_count;

	//! geometry_count objects needed to query drawing commands for imgui
	imgui_frame_t* frames;
	//! The image holding fonts and icons for imgui
	images_t texture;
	//! A graphics pipeline for rasterizing the user interface
	pipeline_with_bindings_t pipeline;
	//! The used vertex and fragment shader for rendering the user interface
	shader_t vertex_shader, fragment_shader;
	//! The sampler used to access the font texture of imgui
	VkSampler sampler;
} interface_pass_t;


//! The render pass that renders a complete frame
typedef struct render_pass_s {
	//! Number of held framebuffers (= swapchain images)
	uint32_t framebuffer_count;
	//! A framebuffer per swapchain image with the depth buffer (0), the
	//! visibility buffer (1) and the swapchain image (2) attached
	VkFramebuffer* framebuffers;
	//! The render pass that encompasses all subpasses for rendering a frame
	VkRenderPass render_pass;
} render_pass_t;


//! Gathers objects that are required to synchroinze rendering of a frame with
//! the swapchain
typedef struct frame_sync_s {
	//! Signaled once an image from the swapchain is available
	VkSemaphore image_acquired;
} frame_sync_t;


/*! Command buffers and synchronization objects for rendering and presenting a
	frame. Each instance is specific to one swapchain image.*/
typedef struct frame_workload_s {
	//! A command buffer using resources associated with the swapchain image to
	//! do all drawing, compute, ray tracing and transfer work for a frame
	VkCommandBuffer command_buffer;
	//! Whether this workload has ever been submitted for rendering
	VkBool32 used;
	//! Signaled once all drawing has finished. Used to synchronize CPU and GPU
	//! execution by waiting for this fence before reusing this workload
	VkFence drawing_finished_fence;
} frame_workload_t;

//! Handles a command buffer for each swapchain image and corresponding
//! synchronization objects
typedef struct frame_queue_s {
	//! Number of entries in the command_buffers array, i.e. the number of
	//! swapchain images when this object was created
	uint32_t frame_count;
	//! An array providing one workload to execute per swapchain image that may
	//! need to be rendered
	frame_workload_t* workloads;
	//! A ring buffer of synchronization objects. It is indexed by sync_index,
	//! not by swapchain image indices. It has size frame_count.
	frame_sync_t* syncs;
	//! Index of the most recent entry of syncs that was used for rendering
	uint32_t sync_index;
	//! Set if rendering of the previous frame encountered an exception that
	//! may indicate that the swapchain needs to be resized or if vsync is
	//! being switched
	VkBool32 recreate_swapchain;
} frame_queue_t;


//! The meaning of the bits written to the swapchain. Relevant for taking HDR
//! screenshots.
typedef enum frame_bits_e {
	//! The swapchain contains standard sRGB colors
	frame_bits_ldr = 0,
	//! The swapchain contains the 8 lower bits of half precision floats
	frame_bits_hdr_low = 1,
	//! The swapchain contains the 8 higher bits of half precision floats
	frame_bits_hdr_high = 2,
} frame_bits_t;


/*! Handles intermediate objects such as staging buffers and file handles, that
	are needed to take a screenshot. For HDR screenshots, this object lives two
	frames to grab the 48 bits per pixel by grabbing 24 bits per frame.*/
typedef struct screenshot_s {
	/*! The file path to which the screenshot should be written. If one of
		these is not NULL, it indicates that a screenshot should be taken. You
		cannot mix LDR formats (*.png, *.jpg) with HDR formats (*.hdr) but
		taking *.png and *.jpg screenshots at the same time is fine.*/
	char *path_png, *path_jpg, *path_hdr;
	//! 0 if the screenshot to be taken should be LDR, 1 if low bits of an HDR
	//! screenshot are being captured, 2 for high bits
	frame_bits_t frame_bits;
	//! The image in host memory to which the swapchain image is copied
	images_t staging;
	//! This image holds an LDR copy converted to the appropriate format. For
	//! HDR screenshots the buffer is large enough for two LDR images stored
	//! one after the other.
	uint8_t* ldr_copy;
	//! An HDR copy of the screenshot, i.e. three linear RGB floats per pixel
	float* hdr_copy;
} screenshot_t;

/*! Holds information about the accumulation buffer, namely the width, height,
	number of samples accumulated till now and the image itself
*/
typedef struct accum_buffer_s {
	// The last buffer which was written to
	uint32_t last_buffer;
	// Number of samples accumulated till now
	uint32_t num_samples;
	// The actual image
	images_t image;
} accum_buffer_t;


/*! Holds boolean flags to indicate what aspects of the application need to be
	updated after a frame due to user input.*/
typedef struct application_updates_s {
	//! The application has just been started
	VkBool32 startup;
	//! The requested new dimensions for the content area of the window. Either
	//! one can be zero to indicate no change. Implies recreate_swapchain.
	uint32_t window_width, window_height;
	//! The window size has changed
	VkBool32 recreate_swapchain;
	//! All shaders need to be recompiled
	VkBool32 reload_shaders;
	//! The number of light sources in the scene or the number of vertices in a
	//! polygonal light source has changed
	VkBool32 update_light_count;
	//! A texture of a polygonal light source has changed
	VkBool32 update_light_textures;
	//! The scene itself has changed
	VkBool32 reload_scene;
	//! Settings that define how shading is performed have changed
	VkBool32 change_shading;
	//! The current camera and lights should be stored to / loaded from a file
	VkBool32 quick_save, quick_load;
} application_updates_t;

typedef struct query_pool_s {
	VkQueryPool pool;
} query_pool_t;

/*! Bundles together all information needed to run this application.*/
typedef struct application_s {
	device_t device;
	swapchain_t swapchain;
	imgui_handle_t imgui;
	//! Complete specification of the scene to be rendered
	scene_specification_t scene_specification;
	//! Complete specification of how the scene should be rendered
	render_settings_t render_settings;
	//! The currently loaded scene
	scene_t scene;
	noise_table_t noise_table;
	ltc_table_t ltc_table;
	render_targets_t render_targets;
	constant_buffers_t constant_buffers;
	light_buffers_t light_buffers;
	images_t light_textures;
	geometry_pass_t geometry_pass;
	shading_pass_t shading_pass;
	accum_pass_t accum_pass;
	copy_pass_t copy_pass;
	interface_pass_t interface_pass;
	render_pass_t render_pass;
	frame_queue_t frame_queue;
	screenshot_t screenshot;
	experiment_list_t experiment_list;
	bool_override_t run_all_exp;
	uint32_t accum_num;
	query_pool_t query_pool;
	VmaAllocator allocator;
	FILE *timings;
} application_t;


/*! Holds uniforms for shaders that might be updated each frame. It is padded
	appropriately to match the binary layout of constants in the shader. See
	the definition in the shader for detailed comments. The scene specification
	may add additional constants of variable size.
	\see shared_constants.glsl */
typedef struct per_frame_constants_s {
	float mesh_dequantization_factor[3], padding_0, mesh_dequantization_summand[3];
	float padding_1;
	float world_to_projection_space[4][4];
	float pixel_to_ray_direction_world_space[3][4];
	float camera_position_world_space[3];
	float mis_visibility_estimate;
	VkExtent2D viewport_size;
	int32_t cursor_position[2];
	float exposure_factor;
	float roughness_factor;
	uint32_t noise_resolution_mask[2];
	uint32_t noise_texture_index_mask;
	uint32_t padding_3[3];
	uint32_t noise_random_numbers[4];
	ltc_constants_t ltc_constants;
} per_frame_constants_t;


//! Creates a list of experiments. Each results in a screenshot with a timing.
//! Returns a list of experiments to perform and outputs the count. To be freed
//! by the calling side. Defined in experiment_list.c.
void create_experiment_list(experiment_list_t* list);

//! Frees memory of the given experiment list
void destroy_experiment_list(experiment_list_t* list);
