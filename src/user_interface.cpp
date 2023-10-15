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


#include "user_interface.h"
#include "string_utilities.h"
#include "frame_timer.h"
#include "math_utilities.h"
#include <cstring>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <iostream>

void specify_user_interface(application_updates_t* updates, application_t* app, float frame_time, uint32_t* reset_accum) {
	// A few preparations
	ImGui::SetCurrentContext((ImGuiContext*) app->imgui.handle);
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Scene and render settings");
	scene_specification_t* scene = &app->scene_specification;
	render_settings_t* settings = &app->render_settings;

	/*
	// Display some help text
	ImGui::Text("[?]");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"LMB             Rotate camera\n"
			"WASDRF, arrows  Move camera\n"
			"IKJL            Rotate camera\n"
			",.              FOV camera\n"
			"Ctrl            Move slower\n"
			"Shift           Move faster\n"
			"F1              Toggle user interface\n"
			"F2              Toggle v-sync\n"
			"F3              Quick save (camera and lights)\n"
			"F4              Quick load (camera and lights)\n"
			"F5              Reload shaders\n"
			"F10, F12        Take screenshot"
		);
	// Display the frame rate
	ImGui::SameLine();
	ImGui::Text("Frame %d time: %.2f ms", app->accum_num, frame_time * 1000.0f);
	// Display a text that changes each frame to indicate to the user whether
	// the renderer is running

	static uint32_t frame_index = 0;
	++frame_index;
	ImGui::SameLine();
	const char* progress_texts[] = {" ......", ". .....", ".. ....", "... ...", ".... ..", "..... .", "...... "};
	ImGui::Text(progress_texts[frame_index % COUNT_OF(progress_texts)]);
	*/

	// Scene selection
	int scene_index = 0;
	const uint32_t sz_g_scene_paths = COUNT_OF(g_scene_paths);

	for (; scene_index != sz_g_scene_paths; ++scene_index) {
		int offset = (int) strlen(scene->file_path) - (int) strlen(g_scene_paths[scene_index][1]);
		if (offset >= 0 && strcmp(scene->file_path + offset, g_scene_paths[scene_index][1]) == 0)
			break;
	}
	const char* scene_names[sz_g_scene_paths];

	for (uint32_t i = 0; i != sz_g_scene_paths; ++i)
		scene_names[i] = g_scene_paths[i][0];
	if (ImGui::Combo("Scene", &scene_index, scene_names, COUNT_OF(scene_names))) {
		free(scene->file_path);
		free(scene->quick_save_path);
		free(scene->texture_path);
		scene->file_path = copy_string(g_scene_paths[scene_index][1]);
		scene->texture_path = copy_string(g_scene_paths[scene_index][2]);
		scene->quick_save_path = copy_string(g_scene_paths[scene_index][3]);
		updates->quick_load = updates->reload_scene = VK_TRUE;
	}

	//tigra: unneeded settings
	/*
	const char* polygon_sampling_techniques[sample_polygon_count];
	polygon_sampling_techniques[sample_polygon_baseline] = "Baseline (zero cost, bogus results)";
	polygon_sampling_techniques[sample_polygon_area_turk] = "Uniform Area Sampling (Turk)";
	polygon_sampling_techniques[sample_polygon_projected_solid_angle] = "Projected Solid Angle Sampling (Peters)";
	polygon_sampling_techniques[sample_polygon_projected_solid_angle_biased] = "Biased Projected Solid Angle Sampling (Peters)";
	polygon_sampling_techniques[sample_polygon_ltc_cp] = "LTC (Ours)";
	if (ImGui::Combo("Polygon sampling", (int*) &settings->polygon_sampling_technique,  polygon_sampling_techniques, sample_polygon_count))
		updates->change_shading = VK_TRUE;
	*/

	/*
	// Sampling settings
	bool show_mis = settings->polygon_sampling_technique == sample_polygon_projected_solid_angle || settings->polygon_sampling_technique == sample_polygon_projected_solid_angle_biased;
	if (show_mis) {
		const char* mis_heuristics[mis_heuristic_count];
		mis_heuristics[mis_heuristic_balance] = "Balance (Veach)";
		mis_heuristics[mis_heuristic_power] = "Power (exponent 2, Veach)";
		mis_heuristics[mis_heuristic_weighted] = "Weighted balance heuristic (Peters)";
		mis_heuristics[mis_heuristic_optimal_clamped] = "Clamped optimal heuristic (Peters)";
		mis_heuristics[mis_heuristic_optimal] = "Optimal heuristic (Peters)";
		uint32_t mis_heuristic_count = COUNT_OF(mis_heuristics);
		if (ImGui::Combo("MIS heuristic", (int*) &settings->mis_heuristic, mis_heuristics, mis_heuristic_count))
			updates->change_shading = VK_TRUE;
	}
	if (show_mis && (settings->mis_heuristic == mis_heuristic_optimal_clamped || settings->mis_heuristic == mis_heuristic_optimal))
		if(ImGui::DragFloat("MIS visibility estimate", &settings->mis_visibility_estimate, 0.01f, 0.0f, 1.0f, "%.2f")) *reset_accum = 1;
	*/

	{
		// Light sampling strategy
		const char* light_sampling_strategies[3];
		light_sampling_strategies[light_uniform] = "Uniform";
		light_sampling_strategies[light_reservoir] = "RIS";
		// Create the interface and remap outputs
		if (ImGui::Combo("Light sampling", (int *) &settings->light_sampling, light_sampling_strategies, 2))
			updates->change_shading = VK_TRUE;
	}

	// Switching vertical synchronization
	if (ImGui::Checkbox("Vsync", (bool*) &settings->v_sync))
		updates->recreate_swapchain = VK_TRUE;
	// Use framebuffer accumulation
	if (ImGui::Checkbox("Accumlation", (bool*) &settings->accum))
		*reset_accum = 1;
	// Changing the sample count
	if (ImGui::InputInt("Sample count", (int*) &settings->sample_count, 1, 10)) {
		if (settings->sample_count < 1) settings->sample_count = 1;
		updates->change_shading = VK_TRUE;
	}
	if (ImGui::InputInt("Sample count light", (int*) &settings->sample_count_light, 1, 10)) {
		if (settings->sample_count_light < 1) settings->sample_count_light = 1;
		updates->change_shading = VK_TRUE;
	}
	// Various rendering settings
	if(ImGui::DragFloat("Exposure", &settings->exposure_factor, 0.05f, 0.0f, 200.0f, "%.2f")) *reset_accum = 1;

	if(ImGui::DragFloat("Roughness factor", &settings->roughness_factor, 0.01f, 0.0f, 2.0f, "%.2f")) *reset_accum = 1;

	// Show buttons for quick save and quick load
	if (ImGui::Button("Quick save"))
		updates->quick_save = VK_TRUE;
	ImGui::SameLine();
	if (ImGui::Button("Quick load"))
		updates->quick_load = VK_TRUE;

	/*
	// A button to reproduce experiments from the publication
	if (ImGui::Button("Reproduce experiments"))
		app->experiment_list.next = 0;
	*/

	// That's all
	ImGui::End();
	ImGui::EndFrame();
}
