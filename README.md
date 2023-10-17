This fork contain essence of paper "Combining Resampled Importance and projected Solid Angle Samplings for Many Area Light Rendering" without compare with previous methods. 
Also:
* added FPS counter
* added total time counter of rendering
* optimized shaders and c code
* buffered files load and save
* added IJKL keyboard shortcuts for camera rotate
* removed unused code

## 
This renderer is based upon the [vulkan_renderer](https://github.com/MomentsInGraphics/vulkan_renderer)
written by [Christoph Peters](https://momentsingraphics.de).
It contains the implementation for the paper "Combining Resampled
Importance and projected Solid Angle Samplings for Many Area Light Rendering"
The renderer is written in C using Vulkan. For more information see:
https://ishaanshah.github.io/risltc and https://momentsingraphics.de/ToyRendererOverview.html

## Downloading Data
Scenes and other data that are needed to run the renderer are available on the
websites for the paper:
https://ishaanshah.github.io/risltc

## Building the Renderer

The renderer is written in C99 with few dependencies. Imgui, GLFW and VMA are
specified as submodules, so you should be able to clone them alongside this
repository (use git clone --recurse-submodules). You also need the latest
Vulkan SDK (version 1.2.176.1 or later) available here:
https://vulkan.lunarg.com/sdk/home

Ther renderer has been tested on Linux (GCC) and Windows (MSVC).
On Linux the Vulkan SDK should be available via your package manager.
On Windows the Vulkan SDK can be downloaded from https://vulkan.lunarg.com/sdk/home#windows.
Validation layers are needed for a debug build but not for a release build.
You may have to use beta drivers, depending on what your package repositories
provide otherwise.

Once all dependencies are available, use CMake to create project files and
build.


## Running the Renderer

Get data files first (see above). Run the binary with current working directory 
risltc. 

The ray tracing extension VK_KHR_ray_query is necessary. Most modern NVIDIA, AMD
and Intel GPUs support the extension.


## Important Code Files

The GLSL implementation of our techniques is found in:
src/shaders/polygon_sampling.glsl
src/shaders/line_sampling.glsl (on the corresponding branch)

The complete shading pass, including our multiple importance sampling, is part
of:
src/shaders/shading_pass.frag.glsl


## Licenses

Most code in this package is licensed under the terms of the GPLv3. However,
you have the option to use the core of the sampling methods in
polygon_sampling.glsl and line_sampling.glsl under the BSD license. See the 
comments at the top of each file for details.

