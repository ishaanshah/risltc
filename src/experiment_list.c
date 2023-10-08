//  Copyright (C) 2021, Christoph Peters, Karlsruhe Institute of Technology
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
#include "string_utilities.h"
#include <stdlib.h>
#include <string.h>

void fill_path_info(experiment_t *exp) {
	// Dummy name to get correct length for malloc
	const char* screenshot_path_peices[] = {exp->base_dir, exp->exp_name, "/00000", ".", exp->ext};
	exp->screenshot_path = concatenate_strings(COUNT_OF(screenshot_path_peices), screenshot_path_peices);

	const char* screenshot_dir_path_peices[] = {exp->base_dir, exp->exp_name};
	exp->screenshots_dir = concatenate_strings(COUNT_OF(screenshot_dir_path_peices), screenshot_dir_path_peices);

	const char* timings_path_peices[] = {exp->base_dir, exp->exp_name, "/timings.txt"};
	exp->timings_path = concatenate_strings(COUNT_OF(timings_path_peices), timings_path_peices);
}

void create_experiment_list(experiment_list_t* list) {
	memset(list, 0, sizeof(*list));
	// Allocate a fixed amount of space
	uint32_t count = 0;
	uint32_t size = 1000;
	experiment_t* experiments = malloc(size * sizeof(experiment_t));
	memset(experiments, 0, size * sizeof(experiment_t));
	// A mapping from polygon sampling techniques to names used in file names
	const char* sample_polygon_name[sample_polygon_count];
	sample_polygon_name[sample_polygon_baseline] = "baseline";
	sample_polygon_name[sample_polygon_area_turk] = "area_turk";
	sample_polygon_name[sample_polygon_projected_solid_angle] = "projected_solid_angle_ours";
	sample_polygon_name[sample_polygon_projected_solid_angle_biased] = "projected_solid_angle_biased_ours";

	// Set to VK_TRUE to run all experiments for generation of figures in the
	// paper (including some variants that are not in the paper)
	VkBool32 all_figs = VK_FALSE;
	// Set to VK_TRUE to create additional figures for the HTML viewer
	VkBool32 html_figs = VK_FALSE;
	// Set to VK_TRUE to run all experiments for generation of run time
	// measurements in the paper
	VkBool32 all_timings = VK_FALSE;
	// Set to VK_TRUE to take *.hdr screenshots (16-bit float stored as 32-bit
	// float) instead of *.png
	VkBool32 take_hdr_screenshots = VK_TRUE;

	VkBool32 lo_rough_figs = getenv("EXP_LO_ROUGH") ? VK_TRUE : VK_FALSE;
	VkBool32 med_rough_figs = getenv("EXP_MED_ROUGH") ? VK_TRUE : VK_FALSE;
	VkBool32 hi_rough_figs = getenv("EXP_HI_ROUGH") ? VK_TRUE : VK_FALSE;
	VkBool32 diffuse_figs = getenv("EXP_DIFFUSE") ? VK_TRUE : VK_FALSE;
	VkBool32 timings = getenv("EXP_TIMINGS") ? VK_TRUE : VK_FALSE;
	VkBool32 compare = getenv("EXP_COMPARE") ? VK_TRUE : VK_FALSE;
	VkBool32 teaser = getenv("EXP_TEASER") ? VK_TRUE : VK_FALSE;
	VkBool32 fig1 = getenv("EXP_FIG1") ? VK_TRUE : VK_FALSE;
	VkBool32 compute_gt = getenv("COMPUTE_GT") ? VK_TRUE : VK_FALSE;
	VkBool32 ensure_correct = getenv("EXP_ENSURE_CORRECT") ? VK_TRUE : VK_FALSE;
	
	char* sample_str = getenv("NUM_SAMPLES");
	uint32_t sample_count = 0;
	if (sample_str)
		sample_count = atoi(sample_str);

	if (sample_count == 0) {
		sample_count = 10000;
	}

	char* tmp = getenv("SCENE");
	char* scene_name;
	if (!tmp) {
		scene_name = copy_string("bistro_exterior");
	} else {
		scene_name = copy_string(tmp);
	}

	scene_index_t scene;
	char* quick_save_path;
	char* scene_dir;
	float diffuse_factor = 1.0;
	float rough_factor = 0.1;
	if (strcmp(scene_name, "bistro_inside") == 0) {
		scene = scene_bistro_inside;
		quick_save_path = copy_string("data/quicksaves/Bistro_interior.save");
		scene_dir = copy_string("bistro_inside/");
	} else {
		scene = scene_bistro_outside;
		quick_save_path = copy_string("data/quicksaves/Bistro_exterior.save");
		scene_dir = copy_string("bistro_exterior/");
	}

	printf("Requested %d samples per experiment\n", sample_count);

	if (teaser || VK_FALSE) {
		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = 0.1f, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene_bistro_outside,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string("data/quicksaves/teaser.save"),
			.use_hdr = VK_TRUE, .base_dir = copy_string("data/experiments/teaser/"),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};

		experiments[count] = bistro_base;
		experiments[count].num_samples = 10000;
		experiments[count].render_settings.light_sampling  = light_uniform;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
		experiments[count].exp_name = "uniform";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 10000;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
		experiments[count].exp_name = "ris";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 10000;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_ltc_cp;
		experiments[count].exp_name = "ours";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 10000;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
		experiments[count].exp_name = "ris_projltc";
		fill_path_info(&experiments[count]);
		++count;

		// Check if GT computation is asked for
		if (compute_gt) {
			experiments[count] = bistro_base;
			experiments[count].num_samples = 1000000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].exp_name = "gt";
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			fill_path_info(&experiments[count]);
			++count;
		}
	}

	if (fig1 || VK_FALSE) {
		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = 0.1f, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene_bistro_inside,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string("data/quicksaves/fig1.save"),
			.use_hdr = VK_TRUE, .base_dir = copy_string("data/experiments/fig1/"),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};

		experiments[count] = bistro_base;
		experiments[count].num_samples = 100;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_ltc_cp;
		experiments[count].exp_name = "ours";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 100;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
		experiments[count].exp_name = "ris_projltc";
		fill_path_info(&experiments[count]);
		++count;

		// Check if GT computation is asked for
		if (compute_gt) {
			experiments[count] = bistro_base;
			experiments[count].num_samples = 1000000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].exp_name = "gt";
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			fill_path_info(&experiments[count]);
			++count;
		}

	}

	uint32_t exp_count = 0;
	experiment_t* base_exps = malloc(sizeof(experiment_t) * 4);
	if (lo_rough_figs || VK_FALSE) {
		char* copy_str = copy_string("data/experiments/lo_rough/");
		const char* path_pieces[] = { copy_str , scene_dir };
		char* base_dir = concatenate_strings(COUNT_OF(path_pieces), path_pieces);

		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = 0.05f, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string(quick_save_path),
			.use_hdr = VK_TRUE, .base_dir = copy_string(base_dir),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};
		base_exps[exp_count] = bistro_base;
		exp_count++;

		free(base_dir);
		free(copy_str);
	}

	if (med_rough_figs || VK_FALSE) {
		char* copy_str = copy_string("E:/renders/med_rough/");
		const char* path_pieces[] = { copy_str, scene_dir };
		char* base_dir = concatenate_strings(COUNT_OF(path_pieces), path_pieces);

		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = rough_factor, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string(quick_save_path),
			.use_hdr = VK_TRUE, .base_dir = copy_string(base_dir),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};
		base_exps[exp_count] = bistro_base;
		exp_count++;

		free(base_dir);
		free(copy_str);
	}

	if (hi_rough_figs || VK_FALSE) {
		char* copy_str = copy_string("data/experiments/hi_rough/");
		const char* path_pieces[] = { copy_str, scene_dir };
		char* base_dir = concatenate_strings(COUNT_OF(path_pieces), path_pieces);

		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = 0.3f, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string(quick_save_path),
			.use_hdr = VK_TRUE, .base_dir = copy_string(base_dir),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};
		base_exps[exp_count] = bistro_base;
		exp_count++;

		free(base_dir);
		free(copy_str);
	}

	if (diffuse_figs || VK_FALSE) {
		char* copy_str = copy_string("E:/renders/diffuse/");
		const char* path_pieces[] = { copy_str, scene_dir };
		char* base_dir = concatenate_strings(COUNT_OF(path_pieces), path_pieces);

		render_settings_t settings_base = {
			.exposure_factor = 1.5f, .roughness_factor = diffuse_factor, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene,
			.width = 1920, .height = 1080,
			.render_settings = settings_base,
			.quick_save_path = copy_string(quick_save_path),
			.use_hdr = VK_TRUE, .base_dir = copy_string(base_dir),
			.ss_per_frame = VK_TRUE, .ext = copy_string("hdr")
		};
		base_exps[exp_count] = bistro_base;
		exp_count++;

		free(base_dir);
		free(copy_str);
	}

	for (int i = 0; i < exp_count; i++) {
		experiment_t base = base_exps[i];

		if (compare) {
			experiments[count] = base;
			experiments[count].num_samples = sample_count;
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
			experiments[count].exp_name = "uniform_uniform";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = sample_count;
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			experiments[count].exp_name = "uniform_cp";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = sample_count;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
			experiments[count].exp_name = "uniform_area";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = sample_count;
			experiments[count].exp_name = "ltc_cp";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = sample_count;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			experiments[count].exp_name = "cp_cp";
			fill_path_info(&experiments[count]);
			++count;
		}

		if (timings) {
			experiments[count] = base;
			experiments[count].num_samples = 1000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
			experiments[count].exp_name = "uniform_uniform_time";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = 1000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			experiments[count].exp_name = "uniform_cp_time";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = 1000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
			experiments[count].exp_name = "uniform_area_time";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = 1000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			experiments[count].exp_name = "cp_cp_time";
			fill_path_info(&experiments[count]);
			++count;

			experiments[count] = base;
			experiments[count].num_samples = 1000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_ltc_cp;
			experiments[count].exp_name = "ltc_cp_time";
			fill_path_info(&experiments[count]);
			++count;
		}

		// Check if GT computation is asked for
		if (compute_gt) {
			experiments[count] = base;
			experiments[count].num_samples = 100000;
			experiments[count].ss_per_frame = VK_FALSE;
			experiments[count].exp_name = "gt";
			experiments[count].render_settings.light_sampling = light_uniform;
			experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
			fill_path_info(&experiments[count]);
			++count;
		}
	}

	// Ensure that all of the figures are correct
	if (ensure_correct || VK_FALSE) {
		render_settings_t settings_base = {
			.exposure_factor = 2.0f, .roughness_factor = 0.1f, .sample_count = 1, .sample_count_light = 1,
			.mis_heuristic = mis_heuristic_optimal_clamped, .mis_visibility_estimate = 0.5f, .animate_noise = VK_TRUE,
			.show_polygonal_lights = VK_FALSE, .accum = VK_TRUE,
			.light_sampling = light_reservoir, .fast_atan = VK_FALSE
		};
		experiment_t bistro_base = {
			.scene_index = scene_bistro_outside,
			.width = 1280, .height = 720,
			.render_settings = settings_base,
			.quick_save_path = copy_string("data/quicksaves/Bistro_exterior.save"),
			.use_hdr = VK_TRUE, .base_dir = copy_string("data/experiments/ensure_correct/"),
			.ss_per_frame = VK_FALSE, .ext = copy_string("hdr")
		};

		experiments[count] = bistro_base;
		experiments[count].num_samples = 100000;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_area_turk;
		experiments[count].exp_name = "uniform_area";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 30000;
		experiments[count].render_settings.mis_heuristic = mis_heuristic_optimal_clamped;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_projected_solid_angle;
		experiments[count].exp_name = "cp_cp";
		fill_path_info(&experiments[count]);
		++count;

		experiments[count] = bistro_base;
		experiments[count].num_samples = 30000;
		experiments[count].render_settings.mis_heuristic = mis_heuristic_optimal_clamped;
		experiments[count].render_settings.polygon_sampling_technique = sample_polygon_ltc_cp;
		experiments[count].exp_name = "ltc_cp";
		fill_path_info(&experiments[count]);
		++count;
	}

	free(quick_save_path);
	free(scene_name);
	free(scene_dir);

	// Output everything
	if (count > size)
		printf("WARNING: Insufficient space allocated for %d experiments.\n", count);
	else
		printf("Defined %d experiments to reproduce.\n", count);
	// Print screenshot paths and indices (useful to figure out command line
	// arguments)
	if (VK_FALSE) {
		for (uint32_t i = 0; i != count; ++i)
			printf("%03d: %s\n", i, experiments[i].screenshot_path);
	}
	list->count = count;
	list->next = count + 1;
	list->experiments = experiments;
}


void destroy_experiment_list(experiment_list_t* list) {
	for (uint32_t i = 0; i != list->count; ++i) {
		free(list->experiments[i].quick_save_path);
		free(list->experiments[i].screenshot_path);
		free(list->experiments[i].base_dir);
		free(list->experiments[i].timings_path);
		free(list->experiments[i].screenshots_dir);
		free(list->experiments[i].ext);
		free(list->experiments[i].exp_name);
	}
	free(list->experiments);
	memset(list, 0, sizeof(*list));
}
