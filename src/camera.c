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


#include "camera.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "math_utilities.h"
#include <GLFW/glfw3.h>

void get_world_to_view_space(float world_to_view_space[4][4], const first_person_camera_t* camera) {
	// Construct a view to world space rotation matrix
	float cos_x = cosf(camera->rotation_x), sin_x = sinf(camera->rotation_x);
	float cos_z = cosf(camera->rotation_z), sin_z = sinf(camera->rotation_z);
	float rotation_x[3][3] = {
		{1.0f, 0.0f, 0.0f},
		{0.0f, cos_x, sin_x},
		{0.0f, -sin_x, cos_x}
	};
	float rotation_z[3][3] = {
		{cos_z, sin_z, 0.0f},
		{-sin_z, cos_z, 0.0f},
		{0.0f, 0.0f, 1.0f}
	};
	float rotation[3][3] = {0.0f};
	for (uint32_t i = 0; i != 3; ++i)
		for (uint32_t j = 0; j != 3; ++j)
			for (uint32_t l = 0; l != 3; ++l)
				rotation[i][j] += rotation_z[i][l] * rotation_x[l][j];
	// Construct the location of the world space origin in view space
	float origin_view_space[3] = {0.0f};
	for (uint32_t i = 0; i != 3; ++i)
		for (uint32_t j = 0; j != 3; ++j)
			origin_view_space[i] -= rotation[j][i] * camera->position_world_space[j];
	// Build the whole matrix
	float result[4][4] = {
		{rotation[0][0], rotation[1][0], rotation[2][0], origin_view_space[0]},
		{rotation[0][1], rotation[1][1], rotation[2][1], origin_view_space[1]},
		{rotation[0][2], rotation[1][2], rotation[2][2], origin_view_space[2]},
		{0.0f, 0.0f, 0.0f, 1.0f}
	};
	memcpy(world_to_view_space, result, sizeof(float) * 4 * 4);
}


void get_view_to_projection_space(float view_to_projection_space[4][4], const first_person_camera_t* camera, float aspect_ratio) {
	float near = camera->near;
	float far = camera->far;
	float top = tanf(0.5f * camera->vertical_fov);
	float right = aspect_ratio * top;
	float result[4][4] = {
		{-1.0f / right, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f / top, 0.0f, 0.0f},
		{0.0f, 0.0f, -(far + near) / (far - near), -2.0f * far * near / (far - near)},
		{0.0f, 0.0f, -1.0f, 0.0f},
	};
	memcpy(view_to_projection_space, result, sizeof(float) * 4 * 4);
}


void get_world_to_projection_space(float world_to_projection_space[4][4], const first_person_camera_t* camera, float aspect_ratio) {
	memset(world_to_projection_space, 0, sizeof(float) * 4 * 4);
	float world_to_view_space[4][4], view_to_projection_space[4][4];
	get_world_to_view_space(world_to_view_space, camera);
	get_view_to_projection_space(view_to_projection_space, camera, aspect_ratio);
	for (uint32_t i = 0; i != 4; ++i)
		for (uint32_t j = 0; j != 4; ++j)
			for (uint32_t l = 0; l != 4; ++l)
				world_to_projection_space[i][j] += view_to_projection_space[i][l] * world_to_view_space[l][j];
}

#define keyPress(a) (glfwGetKey(window, (a)) == GLFW_PRESS)

void control_camera(first_person_camera_t* camera, GLFWwindow* window, int *need_update) {
	// Implement camera rotation
	static const float mouse_radians_per_pixel = 1.0f * M_PI_F / 1000.0f;
	static const float kb_radians_per_pixel = 75.0f * M_PI_F / 1000.0f;

	float forward = 0.0f, right = 0.0f, vertical = 0.0f, rotX = 0.0f, rotZ = 0.0, fov = 0.0;

	int left_mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	int right_mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
	int wheel_mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
	double mouse_position_double[2];

	glfwGetCursorPos(window, &mouse_position_double[0], &mouse_position_double[1]);
	float mouse_position[2] = {(float)mouse_position_double[0], (float)mouse_position_double[1]};

	if (camera->rotate_camera == 0 && left_mouse_state == GLFW_PRESS)
	{
		camera->rotate_camera = 1;
		camera->rotation_x_0 = camera->rotation_x + mouse_position[1] * mouse_radians_per_pixel;
		camera->rotation_z_0 = camera->rotation_z - mouse_position[0] * mouse_radians_per_pixel;
	}
	/*
	else
	if (camera->rotate_camera == 0 && right_mouse_state == GLFW_PRESS) {
		*need_update = 1;
		forward  += mouse_position[0] * 0.0001;
	}
	else
	if (camera->rotate_camera == 0 && wheel_mouse_state == GLFW_PRESS) {
		*need_update = 1;

		right  += mouse_position[0] * 0.0001;
		vertical += mouse_position[1] * 0.0001;
	}
	*/

	if (left_mouse_state == GLFW_RELEASE)
		camera->rotate_camera = 0;

	if (camera->rotate_camera) {
		camera->rotation_x = camera->rotation_x_0 - mouse_radians_per_pixel * mouse_position[1];
		camera->rotation_z = camera->rotation_z_0 + mouse_radians_per_pixel * mouse_position[0];
		*need_update = 1;
	}

	// Figure out how much time has passed since the last invocation
	static double last_time = 0.0;
	double now = glfwGetTime();
	double elapsed_time = (last_time == 0.0) ? 0.0 : (now - last_time);
	float time_delta = (float)elapsed_time;
	last_time = now;

	float mult_speed = 1.0f;

	int kb_rotate_camera = 0;
	int kb_move_camera = 0;
	int kb_fov_changed = 0;

	//additional keys - ctrl, alt, shift and combinations
	int ctrls = keyPress(GLFW_KEY_LEFT_SHIFT) + keyPress(GLFW_KEY_RIGHT_SHIFT);
	int alts = keyPress(GLFW_KEY_LEFT_ALT) + keyPress(GLFW_KEY_RIGHT_ALT);
	int shifts = keyPress(GLFW_KEY_LEFT_SHIFT) + keyPress(GLFW_KEY_RIGHT_SHIFT);

	int additional_keys = ctrls + alts + shifts;

	if (additional_keys == 1)
	{
		if (shifts)
			mult_speed = 10.0f;
		if (ctrls)
			mult_speed = 0.1f;
		if (alts)
			mult_speed = 0.5f;
	}
	else if (additional_keys == 2)
	{
		if (shifts + ctrls == 2)
			mult_speed = 100.0f;
	}

	float step = time_delta * camera->speed * mult_speed;

	// Determine camera movement
	if(keyPress(GLFW_KEY_W) || keyPress(GLFW_KEY_UP))
	{
		kb_move_camera = 1;
		forward += step;
	}
	else
	if(keyPress(GLFW_KEY_S) || keyPress(GLFW_KEY_DOWN))
	{
		kb_move_camera = 1;
		forward -= step;
	}
	else
	if(keyPress(GLFW_KEY_D) || keyPress(GLFW_KEY_RIGHT))
	{
		kb_move_camera = 1;
		right += step;
	}
	else
	if(keyPress(GLFW_KEY_A) || keyPress(GLFW_KEY_LEFT))
	{
		kb_move_camera = 1;
		right -= step;
	}
	else
	if(keyPress(GLFW_KEY_R) || keyPress(GLFW_KEY_PAGE_UP))
	{
		kb_move_camera = 1;
		vertical += step;
	}
	else
	if(keyPress(GLFW_KEY_F) || keyPress(GLFW_KEY_PAGE_DOWN))
	{
		kb_move_camera = 1;
		vertical -= step;
	}
	else
	if(keyPress(GLFW_KEY_J))
	{
		kb_rotate_camera = 1;
		rotZ += step * kb_radians_per_pixel;
	}
	else
	if(keyPress(GLFW_KEY_L))
	{
		kb_rotate_camera = 1;
		rotZ -= step * kb_radians_per_pixel;
	}
	else
	if(keyPress(GLFW_KEY_I))
	{
		kb_rotate_camera = 1;
		rotX += step * kb_radians_per_pixel;
	}
	else
	if(keyPress(GLFW_KEY_K))
	{
		kb_rotate_camera = 1;
		rotX -= step * kb_radians_per_pixel;
	}
	else
	if(keyPress(GLFW_KEY_PERIOD))
	{
		fov += step * kb_radians_per_pixel;
		kb_fov_changed = 1;
	}
	else
	if(keyPress(GLFW_KEY_COMMA))
	{
		fov -= step * kb_radians_per_pixel;
		kb_fov_changed = 1;
	}

	// Implement camera movement
	if(kb_move_camera)
	{
		float cos_z = cosf(camera->rotation_z), sin_z = sinf(camera->rotation_z);
		camera->position_world_space[0] -= sin_z * forward;
		camera->position_world_space[1] -= cos_z * forward;
		camera->position_world_space[0] -= cos_z * right;
		camera->position_world_space[1] += sin_z * right;
		camera->position_world_space[2] += vertical;
		*need_update = 1;
	}

	if(kb_fov_changed)
	{
		fov += camera->vertical_fov;
		if(fov >= 0.0f && fov <= M_PI_F)
		{
			camera->vertical_fov = fov;
			// printf("FOV=%.2f\n", camera->vertical_fov);
			*need_update = 1;
		}
	}

	if(kb_rotate_camera)
	{
		camera->rotation_x = camera->rotation_x + rotX;
		camera->rotation_z = camera->rotation_z - rotZ;
		*need_update = 1;
	}
}
