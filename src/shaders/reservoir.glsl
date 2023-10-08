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

struct reservoir_t {
    // The running sum of all the weights
    float w_sum;
    // Keep track of chosen position
    vec3 light_sample;
    // Keep track of current chosen light
    int light_index;
    // The probabilty with which this sample was chosen
    float sample_value;
};

void initialize_reservoir(inout reservoir_t reservoir) {
    reservoir.w_sum = 0;
    reservoir.light_index = -1;
    reservoir.light_sample = vec3(0);
    reservoir.sample_value = 0;
}

void insert_in_reservoir(inout reservoir_t reservoir, float w, int light_index, vec3 light_sample, float sample_value, float rand) {
    reservoir.w_sum += w;
    if (w > 0 && rand < (w / reservoir.w_sum)) {
        reservoir.light_sample = light_sample;
        reservoir.light_index = light_index;
        reservoir.sample_value = sample_value;
    }
}