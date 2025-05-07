embree_lib = 'embree3'
embree_lib_dir = '/opt/embree-3.12.2.x86_64.linux/lib'
embree_header_dir = '/opt/embree-3.12.2.x86_64.linux/include'
dep_embree = declare_dependency(
    link_args : ['-L' + embree_lib_dir, '-l' + embree_lib],
    include_directories : include_directories(embree_header_dir))

gpgpusim_lib = 'cudart'
gpgpusim_lib_dir = '/home/lucy/sim/vulkan-simulator/gpgpu-sim_emerald/lib/gcc-5.5.0/cuda-10000/release'
dep_gpgpusim = declare_dependency(
   link_args : ['-L' + gpgpusim_lib_dir, '-l' + gpgpusim_lib])

lvp_entrypoints = custom_target(
  'lvp_entrypoints',
  input : [vk_entrypoints_gen, vk_api_xml],
  output : ['lvp_entrypoints.h', 'lvp_entrypoints.c'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--proto', '--weak',
    '--out-h', '@OUTPUT0@', '--out-c', '@OUTPUT1@', '--prefix', 'lvp',
    '--beta', with_vulkan_beta.to_string(),
  ],
  depend_files : vk_entrypoints_gen_depend_files,
)

intel_dir = '../../../intel'
dep_intel = declare_dependency(
  include_directories : include_directories(intel_dir)
)

liblvp_files = files(
    'gpgpusim_bvh.h',
    'lvp_acceleration_structure.h',
    'lvp_acceleration_structure.c',
    'lvp_device.c',
    'lvp_cmd_buffer.c',
    'lvp_descriptor_set.c',
    'lvp_execute.c',
    'lvp_util.c',
    'lvp_image.c',
    'lvp_formats.c',
    'lvp_inline_uniforms.c',
    'lvp_lower_vulkan_resource.c',
    'lvp_lower_vulkan_resource.h',
    'lvp_lower_input_attachments.c',
    'lvp_pipe_sync.c',
    'lvp_pipeline.c',
    'lvp_pipeline.h',
    'lvp_pipeline_rt.c',
    'lvp_pipeline_cache.c',
    'lvp_query.c',
    'lvp_wsi.c') + [vk_cmd_enqueue_entrypoints[0]]

compile_args_vsim = [
  '-Wno-pointer-arith'
]

lvp_deps = [dep_intel, dep_embree, dep_gpgpusim]
lvp_flags = [compile_args_vsim]

if with_platform_x11
  lvp_deps += dep_xcb_dri3
endif

if with_platform_wayland
  lvp_deps += dep_wayland_client
endif

liblavapipe_st = static_library(
  'lavapipe_st',
  [liblvp_files, lvp_entrypoints, sha1_h],
  c_args : [ c_msvc_compat_args, lvp_flags],
  gnu_symbol_visibility : 'hidden',
  include_directories : [ inc_include, inc_src, inc_util, inc_gallium, inc_compiler, inc_gallium_aux ],
  dependencies : [ dep_llvm, idep_nir, idep_mesautil, idep_vulkan_util, idep_vulkan_wsi,
                   idep_vulkan_runtime, lvp_deps ]
)
