cmake_minimum_required(VERSION 3.10)
project(MyProject)  # Replace 'MyProject' with the actual project name

# Ensure C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define the executable name
set(exe_name ${MAIN_PROJECT})

# Source files
set(src_files_assets
    Assets/CornellBox.cpp
    Assets/CornellBox.hpp
    Assets/Material.hpp
    Assets/Model.cpp
    Assets/Model.hpp
    Assets/Procedural.hpp
    Assets/Scene.cpp
    Assets/Scene.hpp
    Assets/Sphere.hpp
    Assets/Texture.cpp
    Assets/Texture.hpp
    Assets/TextureImage.cpp
    Assets/TextureImage.hpp
    Assets/UniformBuffer.cpp
    Assets/UniformBuffer.hpp
    Assets/Vertex.hpp
)

set(src_files_imgui
    ImGui/imgui_freetype.cpp
    ImGui/imgui_freetype.h
    ImGui/imgui_impl_glfw.cpp
    ImGui/imgui_impl_glfw.h
    ImGui/imgui_impl_vulkan.cpp
    ImGui/imgui_impl_vulkan.h
)

set(src_files_utilities
    Utilities/Console.cpp
    Utilities/Console.hpp
    Utilities/Exception.hpp
    Utilities/Glm.hpp
    Utilities/StbImage.cpp
    Utilities/StbImage.hpp
)

# Updated by ycpin for quantize bvh
set(src_files_bvh
    bvh/heuristic_primitive_splitter.hpp
    bvh/sweep_sah_builder.hpp
    bvh/spatial_split_bvh_builder.hpp
    bvh/triangle.hpp
    bvh/single_ray_traverser.hpp
    bvh/locally_ordered_clustering_builder.hpp
    bvh/ray.hpp
    bvh/bvh.hpp
    bvh/utilities.hpp
    bvh/platform.hpp
    bvh/morton.hpp
    bvh/node_layout_optimizer.hpp
    bvh/bottom_up_algorithm.hpp
    bvh/primitive_intersectors.hpp
    bvh/parallel_reinsertion_optimizer.hpp
    bvh/sphere.hpp
    bvh/top_down_builder.hpp
    bvh/binned_sah_builder.hpp
    bvh/radix_sort.hpp
    bvh/vector.hpp
    bvh/leaf_collapser.hpp
    bvh/morton_code_based_builder.hpp
    bvh/bounding_box.hpp
    bvh/prefix_sum.hpp
    bvh/linear_bvh_builder.hpp
    bvh/sah_based_algorithm.hpp
    bvh/hierarchy_refitter.hpp
    bvh/node_intersectors.hpp
)

set(src_files_vulkan
    Vulkan/Application.cpp
    Vulkan/Application.hpp
    Vulkan/Buffer.cpp
    Vulkan/Buffer.hpp
    Vulkan/BufferUtil.hpp
    Vulkan/CommandBuffers.cpp
    Vulkan/CommandBuffers.hpp
    Vulkan/CommandPool.cpp
    Vulkan/CommandPool.hpp
    Vulkan/DebugUtils.cpp
    Vulkan/DebugUtils.hpp
    Vulkan/DebugUtilsMessenger.cpp
    Vulkan/DebugUtilsMessenger.hpp
    Vulkan/DepthBuffer.cpp
    Vulkan/DepthBuffer.hpp
    Vulkan/DescriptorBinding.hpp
    Vulkan/DescriptorPool.cpp
    Vulkan/DescriptorPool.hpp
    Vulkan/DescriptorSetLayout.cpp
    Vulkan/DescriptorSetLayout.hpp
    Vulkan/DescriptorSetManager.cpp
    Vulkan/DescriptorSetManager.hpp
    Vulkan/DescriptorSets.cpp
    Vulkan/DescriptorSets.hpp
    Vulkan/Device.cpp
    Vulkan/Device.hpp
    Vulkan/DeviceMemory.cpp
    Vulkan/DeviceMemory.hpp
    Vulkan/Enumerate.hpp
    Vulkan/Fence.cpp
    Vulkan/Fence.hpp
    Vulkan/FrameBuffer.cpp
    Vulkan/FrameBuffer.hpp
    Vulkan/GraphicsPipeline.cpp
    Vulkan/GraphicsPipeline.hpp
    Vulkan/Image.cpp
    Vulkan/Image.hpp	
    Vulkan/ImageMemoryBarrier.hpp	
    Vulkan/ImageView.cpp
    Vulkan/ImageView.hpp	
    Vulkan/Instance.cpp
    Vulkan/Instance.hpp
    Vulkan/PipelineLayout.cpp
    Vulkan/PipelineLayout.hpp
    Vulkan/RenderPass.cpp
    Vulkan/RenderPass.hpp
    Vulkan/Sampler.cpp
    Vulkan/Sampler.hpp
    Vulkan/Semaphore.cpp
    Vulkan/Semaphore.hpp
    Vulkan/ShaderModule.cpp
    Vulkan/ShaderModule.hpp	
    Vulkan/SingleTimeCommands.hpp
    Vulkan/Strings.cpp
    Vulkan/Strings.hpp	
    Vulkan/Surface.cpp
    Vulkan/Surface.hpp	
    Vulkan/SwapChain.cpp
    Vulkan/SwapChain.hpp
    Vulkan/Version.hpp
    Vulkan/Vulkan.cpp
    Vulkan/Vulkan.hpp
    Vulkan/Window.cpp
    Vulkan/Window.hpp
    Vulkan/WindowConfig.hpp
)

set(src_files_vulkan_raytracing
    Vulkan/RayTracing/AccelerationStructure.cpp
    Vulkan/RayTracing/AccelerationStructure.hpp
    Vulkan/RayTracing/Application.cpp
    Vulkan/RayTracing/Application.hpp
    Vulkan/RayTracing/BottomLevelAccelerationStructure.cpp
    Vulkan/RayTracing/BottomLevelAccelerationStructure.hpp
    Vulkan/RayTracing/BottomLevelGeometry.cpp
    Vulkan/RayTracing/BottomLevelGeometry.hpp
    Vulkan/RayTracing/build.cpp
    Vulkan/RayTracing/build.hpp
    Vulkan/RayTracing/DeviceProcedures.cpp
    Vulkan/RayTracing/DeviceProcedures.hpp
    Vulkan/RayTracing/RayTracingPipeline.cpp
    Vulkan/RayTracing/RayTracingPipeline.hpp
    Vulkan/RayTracing/RayTracingProperties.cpp
    Vulkan/RayTracing/RayTracingProperties.hpp
    Vulkan/RayTracing/ShaderBindingTable.cpp
    Vulkan/RayTracing/ShaderBindingTable.hpp
    Vulkan/RayTracing/TopLevelAccelerationStructure.cpp
    Vulkan/RayTracing/TopLevelAccelerationStructure.hpp
)

set(src_files
    main.cpp
    ModelViewController.cpp
    ModelViewController.hpp
    Options.cpp
    Options.hpp
    RayTracer.cpp
    RayTracer.hpp
    SceneList.cpp
    SceneList.hpp
    UserInterface.cpp
    UserInterface.hpp
    UserSettings.hpp
)

source_group("Assets" FILES ${src_files_assets})
source_group("ImGui" FILES ${src_files_imgui})
source_group("Utilities" FILES ${src_files_utilities})
source_group("bvh" FILES ${src_files_bvh}) # Updated by ycpin for quantize bvh
source_group("Vulkan" FILES ${src_files_vulkan})
source_group("Vulkan.RayTracing" FILES ${src_files_vulkan_raytracing})
source_group("Main" FILES ${src_files})

add_executable(${exe_name})

# Add source files to the executable
target_sources(${exe_name} PRIVATE
    ${src_files_assets}
    ${src_files_imgui}
    ${src_files_utilities}
    ${src_files_bvh}  # Updated by ycpin for quantize bvh
    ${src_files_vulkan}
    ${src_files_vulkan_raytracing}
    ${src_files}
)

if (UNIX)
    # GCC8 needs an extra lib for <filesystem>.
    # This is not needed with GCC9 or higher.
    set(extra_libs -lstdc++fs ${Backtrace_LIBRARIES})
else()
    set(extra_libs "")
endif()

add_dependencies(${exe_name} Assets)
set_target_properties(${exe_name} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
target_include_directories(${exe_name} PRIVATE . ${Boost_INCLUDE_DIRS} ${glfw3_INCLUDE_DIRS} ${glm_INCLUDE_DIRS} ${STB_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})
target_link_directories(${exe_name} PRIVATE ${Vulkan_LIBRARY})
target_link_libraries(${exe_name} PRIVATE ${Boost_LIBRARIES} freetype glfw glm::glm imgui::imgui tinyobjloader::tinyobjloader ${Vulkan_LIBRARIES} ${extra_libs})
