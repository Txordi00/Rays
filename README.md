# Rays - A Vulkan path tracer for GLTF visualization
Rays is a real-time Monte-Carlo path tracer for GLTF visualization. It is based on an energy-conserving BRDF that admits PBR materials. It uses the Vulkan RT pipeline and features some optimizations like instancing and a presampling pass in seek of efficiency. This is a personal research project and as is, it is still very much work in progress and lacks polish and some important features (broader GLTF compatibility and denoising stick out the most).

The project relies on the following libraries:
- Vulkan (in its hpp form)
- C++ standard library
- GLM
- SDL3
- Dear ImGui
- Vk-Bootstrap
- FastGLTF
- Vulkan Memory Allocator
- Stb
- NativeFileDialog Extended

## System requirements
Regarding Vulkan, the project makes extensive use of the Vulkan RT pipeline and relies on the following extensions:
#### Device extensions
- `VK_KHR_RAY_TRACING_PIPELINE_EXTENSION`: Required to use the rt pipeline.
- `VK_KHR_ACCELERATION_STRUCTURE_EXTENSION`: Required to create and update the acceleration structures used to compute the ray intersections.
- `VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION`: Required by the previous
- `VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION`: For acquire-after-present. It simplifies the decoupling between frames in flight and the swapchain.
- `VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION`: Used in order to skip many annoying image layout transitions and keep most GPU images in the general layout. It can arguably be disabled and the program should still work in most GPUs, albeit for some validation warnings.
#### Instance extensions
- `VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION`: Required by VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION.
- `VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION`: Required by the previous.

If your device drivers do not support any of these extensions, the program won't launch. You can check the supported extensions of your device with the `vulkaninfo` command. E.g.: `vulkaninfo | grep -i VK_KHR_RAY_TRACING_PIPELINE` will check whether you can use the Vulkan rt pipeline extension in your GPU and driver.

## Compilation
The project uses cmake as the build tool. It expects to find Vulkan, GLM and SDL3 installed in your system, and all the rest of the libraries are fetched from their repositories using the cmake's `FetchContent` mechanism.

I tested the compilation in Linux using all the possible combinations between `(ninja, make)` and `(g++, clang++)`. Outside of Linux I have not tested anywhere, but it should work with minor changes in Windows. The build process is the typical with cmake:
```shell
cd lrt
mkdir build && cd build
cmake -G <build_generator> -DCMAKE_BUILD_TYPE=<build_type> -DCMAKE_CXX_COMPILER=<compiler>
<build_generator_exec>
./lrt <path_to_gltf_scene>
```
It has been tested with `<build_generator>=Ninja, Unix\ Makefiles`, `<compiler>=g++, clang++` and `<build_generator_exec>=ninja, make`.

> [!NOTE]
> There is an experimental and currently broken compilation path using the Vulkan-hpp CPP20 module. It is switched on by setting `set(USE_VULKANHPP_CPP20_MODULE ON)` in the main `CMakeLists.txt`. Currently it is broken due to Vulkan forcing the import of the std module, which results in many redefinition errors coming from the include of glm headers. Furthermore, the `vulkan` hpp module forcefully imports the `vulkan:video` module incorrectly, and you have to manually rename `vulkan_hpp:video` to `vulkan:video` in the `vulkan_video.cppm` source file. This path cannot be used at the moment with `<build_generator>=Unix\ Makefiles` and `<build_generator_exec>=make`.

### Features ###
- **Vulkan rt pipeline:** Extensive use of the Vulkan RT pipeline for efficient ray generation and intersection in GPU. Runtime update of the acceleration structures (BVHs).
- **GLTF loader:** GLTF loader worked on top of fastgltf. 
- **Instancing:** Baked within the GLTF loader and the top-level acceleration structure.
- **PBR materials with normal maps:** Standard PBR parameters from constants and/or textures (base color, perceptual roughness, metallic factor). Default value for reflectance. Admits normal maps. If the GLTF loader does not find the normal textures, it defaults to interpolated vertex normals.
- **Importance sampling:** Implemented by balancing cosine-weighted hemisphere samples (diffuse pass) and microfacet ggx samples (specular pass) depending on their pdf values:
```
total_luminance_contribution = pdf_hemisphere(hemisphere_sample)^2 /
                (pdf_hemisphere(hemisphere_sample)^2 + pdf_ggx(hemisphere_sample)^2) * contribution(hemisphere_sample)
 + pdf_ggx(ggx_sample)^2 /
                (pdf_hemisphere(ggx_sample)^2 + pdf_ggx(ggx_sample)^2) * contribution(ggx_sample)
```
- **Presampling:** Optional discretisation of the sampling space into GPU memory. Instead of computing the bounce directions on-line, they are loaded in from memory. It avoids many non-linear in-shader computations but adds a lot of random memory reads. In my computer (laptop with integrated AMD Radeon 780M graphics) it is unfortunately slower than on-line sampling. But maybe in dedicated GPU setups with higher bandwidth it will be beneficial.
- **Lights manager and other controls with imgui:** Runtime addition/removal/modification of point lights and directional lights (I have limited them to 10 but the limit can be changed at compile time). Other controls: Background color picker, environment map selection, random sampling toggle (recommended to leave this on, otherwise you get a biased Monte-Carlo integration), rt recursion depth, number of bounces (samples) after each intersection, scene scale and rotation.

### REFERENCES ###
- [Vulkan Guide](https://vkguide.dev/) (Victor Blanco) - Overall engine structure and my main source of knowledge of the Vulkan API
- [NVIDIA Vulkan Ray Tracing Tutorial](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/) (v1) - Vulkan RT pipeline setup
- [Filament](https://google.github.io/filament/Filament.md.html) - PBR shader functions
- [Some notes on importance sampling of a hemisphere](https://www.mathematik.uni-marburg.de/~thormae/lectures/graphics1/code/ImportanceSampling/importance_sampling_notes.pdf) (Thorsten Thormählen, 2020) - GGX sampling 
- [Building an orthonormal basis from a 3D unit vector without normalization](https://www.tandfonline.com/doi/abs/10.1080/2165347X.2012.689606) (Frisvad J. R., 2012) - Fast (linear) world to normal local basis transformation
- [Global Illumination & Path Tracing](https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing/introduction-global-illumination-path-tracing.html) - Insight into Monte-Carlo path tracing
- [Physically Based Rendering: From Theory To Implementation](https://www.pbr-book.org/) - Cosine-weighted hemisphere sampling and more insight into Monte Carlo path tracing
- [Understandable RayTracing in 256 lines of bare C++](https://github.com/ssloy/tinyraytracer) - Before proceeding with the Vulkan RT work, I convinced myself that I truly understand ray tracing by trying to reproduce in CPU the results from that project.

### TODO ###
- **Improve GLTF compatibility:** Top on the list. Currently, many GLTF files fail to load, probably due to some wrong assumptions on my end about the way the data is delivered. I should explore why and fix it while keeping the current baked-in instancing within the GLTF loader and the acceleration structures builder.
- **Denoising:** Critical for image quality. Maybe [Intel's oidn](https://github.com/RenderKit/oidn) is a good starting point.
- **More efficient algorithm:** I am eager to implement a ReSTIR-like algorithm with intelligent caching in the future. I have to study these [notes](https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Notes.pdf) before that.
- **Area lights:** I think that this should come after the previous step, since I cannot imagine the current Monte-Carlo implementation working in real-time with emissive surfaces.
- **Refractive materials and caustics:** Handle refraction and the GLTF extensions `KHR_materials_transmission`, `KHR_materials_volume` and `KHR_materials_ior`.

### ACKNOWLEDGMENTS ###
I would like to acknowledge [Beaumanvienna (JC)](https://github.com/beaumanvienna/vulkan) and his [videos](https://www.youtube.com/@beaumanvienna6844) for insight into the GLTF format and for general guidance and motivation, [Natalie Vock](https://pixelcluster.github.io/) for helping me debug a couple of shady bugs in my code and [Tom Clabault](https://tomclabault.github.io/) for helping me correct my Monte-Carlo implementation and keep it energy-conserving. I don't want to forget to mention the participants of the [Vulkan Game Engine Design server](https://discord.gg/sG9hbJ499f) for their company and support along the way.

## License
MIT License - feel free to use this code in your projects with attribution.
