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


#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795f
#endif

#ifndef M_PI_F
	#define M_PI_F M_PI
#endif

#ifndef M_TWO_PI
	#define M_TWO_PI 6.283185307179586476925286766559f
#endif

#ifndef M_INV_PI
	#define M_INV_PI 0.31830988618379067153776752674503f
#endif

#ifndef M_HALF_INV_PI
	#define M_HALF_INV_PI 0.15915494309189533576888376337251f
#endif

#ifndef M_HALF_PI
	#define M_HALF_PI 1.5707963267948966192313216916398f
#endif

#ifndef M_180_DIV_PI
	#define M_180_DIV_PI 57.295779513082320876798154814105f
#endif

#ifndef M_PI_DIV_180
	#define M_PI_DIV_180 0.01745329251994329576923690768489f
#endif

#ifndef M_INFINITY
	#define M_INFINITY (1.0f / 0.0f)
#endif

#define INV_3		 0.33333333333333333333333333333333f
#define TWO_INV_3	 0.66666666666666666666666666666666f

#define INV_255		 0.0039215686274509803921568627451f

//! is TRACE_SHADOW_RAYS ?
#define rtx_bits_TRACE_SHADOW_RAYS	1

#define luminance_weights vec3(0.21263901f, 0.71516868f, 0.07219232f)