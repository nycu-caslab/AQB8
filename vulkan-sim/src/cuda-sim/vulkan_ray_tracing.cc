// Copyright (c) 2022, Mohammadreza Saed, Yuan Hsi Chou, Lufei Liu, Tor M.
// Aamodt, The University of British Columbia All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. Neither the name of
// The University of British Columbia nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// clang-format off

#include "vulkan_ray_tracing.h"
#include "vulkan_rt_thread_data.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

#define __CUDA_RUNTIME_API_H__
#include "host_defines.h"
#include "builtin_types.h"
#include "driver_types.h"
#include "../../libcuda/cuda_api.h"
#include "cudaProfiler.h"
#if (CUDART_VERSION < 8000)
#include "__cudaFatFormat.h"
#endif

#include "../../libcuda/gpgpu_context.h"
#include "../../libcuda/cuda_api_object.h"
#include "../gpgpu-sim/gpu-sim.h"
#include "../cuda-sim/ptx_loader.h"
#include "../cuda-sim/cuda-sim.h"
#include "../cuda-sim/ptx_ir.h"
#include "../cuda-sim/ptx_parser.h"
#include "../gpgpusim_entrypoint.h"
#include "../stream_manager.h"
#include "../abstract_hardware_model.h"
#include "vulkan_acceleration_structure_util.h"
#include "../gpgpu-sim/vector-math.h"

#if defined(MESA_USE_LVPIPE_DRIVER)
#include "lvp_private.h"
#endif 
//#include "intel_image_util.h"
#include "astc_decomp.h"

// #define HAVE_PTHREAD
// #define UTIL_ARCH_LITTLE_ENDIAN 1
// #define UTIL_ARCH_BIG_ENDIAN 0
// #define signbit signbit

// #define UINT_MAX 65535
// #define GLuint MESA_GLuint
// // #include "isl/isl.h"
// // #include "isl/isl_tiled_memcpy.c"
// #include "vulkan/anv_private.h"
// #undef GLuint

// #undef HAVE_PTHREAD
// #undef UTIL_ARCH_LITTLE_ENDIAN
// #undef UTIL_ARCH_BIG_ENDIAN
// #undef signbit

// #include "vulkan/anv_public.h"

#if defined(MESA_USE_INTEL_DRIVER)
#include "intel_image.h"
#elif defined(MESA_USE_LVPIPE_DRIVER)
// #include "lvp_image.h"
#endif

// #include "anv_include.h"

VkRayTracingPipelineCreateInfoKHR* VulkanRayTracing::pCreateInfos = NULL;
VkAccelerationStructureGeometryKHR* VulkanRayTracing::pGeometries = NULL;
uint32_t VulkanRayTracing::geometryCount = 0;
VkAccelerationStructureKHR VulkanRayTracing::topLevelAS = NULL;
std::vector<std::vector<Descriptor> > VulkanRayTracing::descriptors;
std::ofstream VulkanRayTracing::imageFile;
std::map<std::string, std::string> outputImages;
bool VulkanRayTracing::firstTime = true;
std::vector<shader_stage_info> VulkanRayTracing::shaders;
// RayDebugGPUData VulkanRayTracing::rayDebugGPUData[2000][2000] = {0};
struct DESCRIPTOR_SET_STRUCT* VulkanRayTracing::descriptorSet = NULL;
void* VulkanRayTracing::launcher_descriptorSets[MAX_DESCRIPTOR_SETS][MAX_DESCRIPTOR_SET_BINDINGS] = {NULL};
void* VulkanRayTracing::launcher_deviceDescriptorSets[MAX_DESCRIPTOR_SETS][MAX_DESCRIPTOR_SET_BINDINGS] = {NULL};
std::vector<void*> VulkanRayTracing::child_addrs_from_driver;
std::map<void*, void*> VulkanRayTracing::blas_addr_map;

void *VulkanRayTracing::tlas_addr;

void *VulkanRayTracing::int_bvh_addr;
void *VulkanRayTracing::int_bvh_clusters_addr;
void *VulkanRayTracing::int_bvh_trigs_addr;
void *VulkanRayTracing::int_bvh_nodes_addr;
void *VulkanRayTracing::int_bvh_primitive_indices_addr;

int_bvh_t VulkanRayTracing::int_bvh;

bool VulkanRayTracing::dumped = false;

bool use_external_launcher = false;
const bool dump_trace = false;

bool VulkanRayTracing::_init_ = false;
warp_intersection_table *** VulkanRayTracing::intersection_table;
warp_intersection_table *** VulkanRayTracing::anyhit_table;
IntersectionTableType VulkanRayTracing::intersectionTableType = IntersectionTableType::Baseline;

float get_norm(float4 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}
float get_norm(float3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float4 normalized(float4 v)
{
    float norm = get_norm(v);
    return {v.x / norm, v.y / norm, v.z / norm, v.w / norm};
}
float3 normalized(float3 v)
{
    float norm = get_norm(v);
    return {v.x / norm, v.y / norm, v.z / norm};
}

Ray make_transformed_ray(Ray &ray, float4x4 matrix, float *worldToObject_tMultiplier)
{
    Ray transformedRay;
    float4 transformedOrigin4 = matrix * float4({ray.get_origin().x, ray.get_origin().y, ray.get_origin().z, 1});
    float4 transformedDirection4 = matrix * float4({ray.get_direction().x, ray.get_direction().y, ray.get_direction().z, 0});

    float3 transformedOrigin = {transformedOrigin4.x / transformedOrigin4.w, transformedOrigin4.y / transformedOrigin4.w, transformedOrigin4.z / transformedOrigin4.w};
    float3 transformedDirection = {transformedDirection4.x, transformedDirection4.y, transformedDirection4.z};
    *worldToObject_tMultiplier = get_norm(transformedDirection);
    transformedDirection = normalized(transformedDirection);

    transformedRay.make_ray(transformedOrigin, transformedDirection, ray.get_tmin() * (*worldToObject_tMultiplier), ray.get_tmax() * (*worldToObject_tMultiplier));
    return transformedRay;
}

float magic_max7(float a0, float a1, float b0, float b1, float c0, float c1, float d)
{
	float t1 = MIN_MAX(a0, a1, d);
	float t2 = MIN_MAX(b0, b1, t1);
	float t3 = MIN_MAX(c0, c1, t2);
	return t3;
}

float magic_min7(float a0, float a1, float b0, float b1, float c0, float c1, float d)
{
	float t1 = MAX_MIN(a0, a1, d);
	float t2 = MAX_MIN(b0, b1, t1);
	float t3 = MAX_MIN(c0, c1, t2);
	return t3;
}

float3 get_t_bound(float3 box, float3 origin, float3 idirection)
{
    // // Avoid div by zero, returns 1/2^80, an extremely small number
    // const float ooeps = exp2f(-80.0f);

    // // Calculate inverse direction
    // float3 idir;
    // idir.x = 1.0f / (fabsf(direction.x) > ooeps ? direction.x : copysignf(ooeps, direction.x));
    // idir.y = 1.0f / (fabsf(direction.y) > ooeps ? direction.y : copysignf(ooeps, direction.y));
    // idir.z = 1.0f / (fabsf(direction.z) > ooeps ? direction.z : copysignf(ooeps, direction.z));

    // Calculate bounds
    float3 result;
    result.x = (box.x - origin.x) * idirection.x;
    result.y = (box.y - origin.y) * idirection.y;
    result.z = (box.z - origin.z) * idirection.z;

    // Return
    return result;
}

float3 calculate_idir(float3 direction) {
    // Avoid div by zero, returns 1/2^80, an extremely small number
    const float ooeps = exp2f(-80.0f);

    // Calculate inverse direction
    float3 idir;
    // TODO: is this wrong?
    idir.x = 1.0f / (fabsf(direction.x) > ooeps ? direction.x : copysignf(ooeps, direction.x));
    idir.y = 1.0f / (fabsf(direction.y) > ooeps ? direction.y : copysignf(ooeps, direction.y));
    idir.z = 1.0f / (fabsf(direction.z) > ooeps ? direction.z : copysignf(ooeps, direction.z));

    // idir.x = fabsf(direction.x) > ooeps ? 1.0f / direction.x : copysignf(ooeps, direction.x);
    // idir.y = fabsf(direction.y) > ooeps ? 1.0f / direction.y : copysignf(ooeps, direction.y);
    // idir.z = fabsf(direction.z) > ooeps ? 1.0f / direction.z : copysignf(ooeps, direction.z);
    return idir;
}

bool ray_box_test(float3 low, float3 high, float3 idirection, float3 origin, float tmin, float tmax, float& thit)
{
	// const float3 lo = Low * InvDir - Ood;
	// const float3 hi = High * InvDir - Ood;
    float3 lo = get_t_bound(low, origin, idirection);
    float3 hi = get_t_bound(high, origin, idirection);

    // QUESTION: max value does not match rtao benchmark, rtao benchmark converts float to int with __float_as_int
    // i.e. __float_as_int: -110.704826 => -1025677090, -24.690834 => -1044019502

	// const float slabMin = tMinFermi(lo.x, hi.x, lo.y, hi.y, lo.z, hi.z, TMin);
	// const float slabMax = tMaxFermi(lo.x, hi.x, lo.y, hi.y, lo.z, hi.z, TMax);
    float min = magic_max7(lo.x, hi.x, lo.y, hi.y, lo.z, hi.z, tmin);
    float max = magic_min7(lo.x, hi.x, lo.y, hi.y, lo.z, hi.z, tmax);

	// OutIntersectionDist = slabMin;
    thit = min;

	// return slabMin <= slabMax;
    return (min <= max);
}

typedef struct StackEntry {
    uint8_t* addr;
    bool topLevel;
    bool leaf;
    StackEntry(uint8_t* addr, bool topLevel, bool leaf): addr(addr), topLevel(topLevel), leaf(leaf) {}
} StackEntry;


std::ofstream print_tree;
void traverse_tree(volatile uint8_t* address, bool isTopLevel = true, bool isLeaf = false, bool isRoot = true)
{
    if(isRoot)
    {
        GEN_RT_BVH topBVH;
        GEN_RT_BVH_unpack(&topBVH, (uint8_t*)address);

        uint8_t* topRootAddr = (uint8_t*)address + topBVH.RootNodeOffset;

        if (print_tree.is_open())
        {
            print_tree << "traversing bvh , isTopLevel = " << isTopLevel << (void *)(address) << ", RootNodeOffset = (" << topBVH.RootNodeOffset << std::endl;
        }

        traverse_tree(topRootAddr, isTopLevel, false, false);
    }
    
    else if(!isLeaf) // internal nodes
    {
        struct GEN_RT_BVH_INTERNAL_NODE node;
        GEN_RT_BVH_INTERNAL_NODE_unpack(&node, address);

        if (print_tree.is_open())
        {
            uint8_t *child_addrs[6];
            child_addrs[0] = address + (node.ChildOffset * 64);
            for(int i = 0; i < 5; i++)
                child_addrs[i + 1] = child_addrs[i] + node.ChildSize[i] * 64;
            
            print_tree << "traversing internal node " << (void *)address;
            print_tree << ", isTopLevel = " << isTopLevel << ", child offset = " << node.ChildOffset << ", node type = " << node.NodeType;
            print_tree << ", child size = (" << node.ChildSize[0] << ", " << node.ChildSize[1] << ", " << node.ChildSize[2] << ", " << node.ChildSize[3] << ", " << node.ChildSize[4] << ", " << node.ChildSize[5] << ")";
            print_tree << ", child type = (" << node.ChildType[0] << ", " << node.ChildType[1] << ", " << node.ChildType[2] << ", " << node.ChildType[3] << ", " << node.ChildType[4] << ", " << node.ChildType[5] << ")";
            print_tree << ", child addresses = (" << (void*)(child_addrs[0]) << ", " << (void*)(child_addrs[1]) << ", " << (void*)(child_addrs[2]) << ", " << (void*)(child_addrs[3]) << ", " << (void*)(child_addrs[4]) << ", " << (void*)(child_addrs[5]) << ")";
            print_tree << std::endl;
        }

        uint8_t *child_addr = address + (node.ChildOffset * 64);
        for(int i = 0; i < 6; i++)
        {
            if(node.ChildSize[i] > 0)
            {
                if(node.ChildType[i] != NODE_TYPE_INTERNAL)
                    isLeaf = true;
                else
                    isLeaf = false;

                traverse_tree(child_addr, isTopLevel, isLeaf, false);
            }

            child_addr += node.ChildSize[i] * 64;
        }
    }

    else // leaf nodes
    {
        if(isTopLevel)
        {
            GEN_RT_BVH_INSTANCE_LEAF instanceLeaf;
            GEN_RT_BVH_INSTANCE_LEAF_unpack(&instanceLeaf, address);

            float4x4 worldToObjectMatrix = instance_leaf_matrix_to_float4x4(&instanceLeaf.WorldToObjectm00);
            float4x4 objectToWorldMatrix = instance_leaf_matrix_to_float4x4(&instanceLeaf.ObjectToWorldm00);

            assert(instanceLeaf.BVHAddress != NULL);

            if (print_tree.is_open())
            {
                print_tree << "traversing top level leaf node " << (void *)address << ", instanceID = " << instanceLeaf.InstanceID << ", BVHAddress = " << instanceLeaf.BVHAddress << ", ShaderIndex = " << instanceLeaf.ShaderIndex << std::endl;
            }

            traverse_tree(address + instanceLeaf.BVHAddress, false, false, true);
        }
        else
        {
            struct GEN_RT_BVH_PRIMITIVE_LEAF_DESCRIPTOR leaf_descriptor;
            GEN_RT_BVH_PRIMITIVE_LEAF_DESCRIPTOR_unpack(&leaf_descriptor, address);
            
            if (leaf_descriptor.LeafType == TYPE_QUAD)
            {
                struct GEN_RT_BVH_QUAD_LEAF leaf;
                GEN_RT_BVH_QUAD_LEAF_unpack(&leaf, address);

                float3 p[3];
                for(int i = 0; i < 3; i++)
                {
                    p[i].x = leaf.QuadVertex[i].X;
                    p[i].y = leaf.QuadVertex[i].Y;
                    p[i].z = leaf.QuadVertex[i].Z;
                }

                assert(leaf.PrimitiveIndex1Delta == 0);

                if (print_tree.is_open())
                {
                    print_tree << "quad node " << (void*)address << " ";
                    print_tree << "primitiveID = " << leaf.PrimitiveIndex0 << "\n";

                    print_tree << "p[0] = (" << p[0].x << ", " << p[0].y << ", " << p[0].z << ") ";
                    print_tree << "p[1] = (" << p[1].x << ", " << p[1].y << ", " << p[1].z << ") ";
                    print_tree << "p[2] = (" << p[2].x << ", " << p[2].y << ", " << p[2].z << ") ";
                    print_tree << "p[3] = (" << p[3].x << ", " << p[3].y << ", " << p[3].z << ")" << std::endl;
                }
            }
            else
            {
                struct GEN_RT_BVH_PROCEDURAL_LEAF leaf;
                GEN_RT_BVH_PROCEDURAL_LEAF_unpack(&leaf, address);

                if (print_tree.is_open())
                {
                    print_tree << "PROCEDURAL node " << (void*)address << " ";
                    print_tree << "NumPrimitives = " << leaf.NumPrimitives << ", LastPrimitive = " << leaf.LastPrimitive << ", PrimitiveIndex[0]" << leaf.PrimitiveIndex[0] << "\n";
                }
            }
        }
    }
}

void VulkanRayTracing::init(uint32_t launch_width, uint32_t launch_height)
{
    if(_init_)
        return;
    _init_ = true;

    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    uint32_t width = (launch_width + 31) / 32;
    uint32_t height = launch_height;

    if(ctx->the_gpgpusim->g_the_gpu->getShaderCoreConfig()->m_rt_intersection_table_type == 0)
        intersectionTableType = IntersectionTableType::Baseline;
    else if(ctx->the_gpgpusim->g_the_gpu->getShaderCoreConfig()->m_rt_intersection_table_type == 1)
        intersectionTableType = IntersectionTableType::Function_Call_Coalescing;
    else
        assert(0);

    if(intersectionTableType == IntersectionTableType::Baseline)
    {
        intersection_table = new Baseline_warp_intersection_table**[width];
        for(int i = 0; i < width; i++)
        {
            intersection_table[i] = new Baseline_warp_intersection_table*[height];
            for(int j = 0; j < height; j++)
                intersection_table[i][j] = new Baseline_warp_intersection_table();
        }
    }
    else
    {
        intersection_table = new Coalescing_warp_intersection_table**[width];
        for(int i = 0; i < width; i++)
        {
            intersection_table[i] = new Coalescing_warp_intersection_table*[height];
            for(int j = 0; j < height; j++)
                intersection_table[i][j] = new Coalescing_warp_intersection_table();
        }

    }
    anyhit_table = new Baseline_warp_intersection_table**[width];
    for(int i = 0; i < width; i++)
    {
        anyhit_table[i] = new Baseline_warp_intersection_table*[height];
        for(int j = 0; j < height; j++)
            anyhit_table[i][j] = new Baseline_warp_intersection_table();
    }
}

// clang-format on

int32_t VulkanRayTracing::floor_to_int32(float x)
{
    assert(!std::isnan(x));
    if (x < -2147483648.0f)
        return -2147483648;
    if (x >= 2147483648.0f)
        return 2147483647;
    return (int)floorf(x);
}

int32_t VulkanRayTracing::ceil_to_int32(float x)
{
    assert(!std::isnan(x));
    if (x <= -2147483649.0f)
        return -2147483648;
    if (x > 2147483647.0f)
        return 2147483647;
    return (int)ceilf(x);
}

int_w_t VulkanRayTracing::get_int_w(const std::array<float, 3> &w)
{
    int_w_t int_w{};

    for (int i = 0; i < 3; i++)
    {
        auto &wi = reinterpret_cast<const uint32_t &>(w[i]);

        bool sign = wi & 0x80000000;
        uint32_t exponent = (wi >> 23) & 0xff;
        uint32_t mantissa = wi & 0x7fffff;

        uint32_t near_exponent = (exponent - 127) & 0b11111;
        uint32_t near_mantissa = mantissa >> 16;
        uint32_t near = (near_exponent << 7) | near_mantissa;

        uint32_t far = near + 1;
        uint32_t far_exponent = far >> 7;
        uint32_t far_mantissa = far & 0b1111111;

        int_w.iw[i] = sign;
        near_mantissa |= 0b10000000;
        far_mantissa |= 0b10000000;

        if (sign)
        {
            int_w.rw_l[i] = far_exponent;
            int_w.qw_l[i] = far_mantissa;
            int_w.rw_h[i] = near_exponent;
            int_w.qw_h[i] = near_mantissa;
        }
        else
        {
            int_w.rw_l[i] = near_exponent;
            int_w.qw_l[i] = near_mantissa;
            int_w.rw_h[i] = far_exponent;
            int_w.qw_h[i] = far_mantissa;
        }
    }

    return int_w;
}

decoded_data_t VulkanRayTracing::decode_data(uint16_t data)
{
    decoded_data_t decoded_data{};
    if (data & 0x8000)
    {
        int field_b = ((data & 0x7fff) >> field_c_bits);
        int field_c = (data & ((1 << field_c_bits) - 1));
        if (field_b == 0)
        {
            decoded_data.child_type = child_type_t::INTERNAL;
        }
        else
        {
            decoded_data.child_type = child_type_t::LEAF;
            decoded_data.num_trigs = field_b;
        }
        decoded_data.idx = field_c;
    }
    else
    {
        decoded_data.child_type = child_type_t::SWITCH;
        decoded_data.idx = data;
    }
    return decoded_data;
}

std::pair<bool, float> VulkanRayTracing::intersect_bbox(const std::array<bool, 3> &octant,
                                                        const std::array<float, 3> &w,
                                                        const float *x,
                                                        const std::array<float, 3> &b,
                                                        float tmax)
{
    float entry[3], exit[3];
    entry[0] = w[0] * x[0 + octant[0]] + b[0];
    entry[1] = w[1] * x[2 + octant[1]] + b[1];
    entry[2] = w[2] * x[4 + octant[2]] + b[2];
    exit[0] = w[0] * x[1 - octant[0]] + b[0];
    exit[1] = w[1] * x[3 - octant[1]] + b[1];
    exit[2] = w[2] * x[5 - octant[2]] + b[2];

    float entry_ = fmaxf(entry[0], fmaxf(entry[1], fmaxf(entry[2], 0.0f)));
    float exit_ = fminf(exit[0], fminf(exit[1], fminf(exit[2], tmax)));

    if (entry_ <= exit_)
        return std::make_pair(true, entry_);
    else
        return std::make_pair(false, 0);
}

std::pair<bool, int32_t> VulkanRayTracing::intersect_int_bbox(int32_t qy_max,
                                                              const int_w_t &int_w,
                                                              const uint8_t *qx,
                                                              const int32_t *qb_l,
                                                              const int32_t *qb_h)
{
    const int32_t qx_a[3] = {
        int_w.iw[0] ? qx[1] : qx[0],
        int_w.iw[1] ? qx[3] : qx[2],
        int_w.iw[2] ? qx[5] : qx[4]};

    const int32_t qx_b[3] = {
        int_w.iw[0] ? qx[0] : qx[1],
        int_w.iw[1] ? qx[2] : qx[3],
        int_w.iw[2] ? qx[4] : qx[5]};

    int32_t entry[3], exit[3];

    entry[0] = (int_w.qw_l[0] * qx_a[0]) << int_w.rw_l[0];
    entry[0] = (int_w.iw[0] ? -entry[0] : entry[0]) + qb_l[0];

    entry[1] = (int_w.qw_l[1] * qx_a[1]) << int_w.rw_l[1];
    entry[1] = (int_w.iw[1] ? -entry[1] : entry[1]) + qb_l[1];

    entry[2] = (int_w.qw_l[2] * qx_a[2]) << int_w.rw_l[2];
    entry[2] = (int_w.iw[2] ? -entry[2] : entry[2]) + qb_l[2];

    exit[0] = (int_w.qw_h[0] * qx_b[0]) << int_w.rw_h[0];
    exit[0] = (int_w.iw[0] ? -exit[0] : exit[0]) + qb_h[0];

    exit[1] = (int_w.qw_h[1] * qx_b[1]) << int_w.rw_h[1];
    exit[1] = (int_w.iw[1] ? -exit[1] : exit[1]) + qb_h[1];

    exit[2] = (int_w.qw_h[2] * qx_b[2]) << int_w.rw_h[2];
    exit[2] = (int_w.iw[2] ? -exit[2] : exit[2]) + qb_h[2];

    int32_t entry_ = std::max({entry[0], entry[1], entry[2], 0});
    int32_t exit_ = std::min({exit[0], exit[1], exit[2], qy_max});

    if (entry_ <= exit_)
        return std::make_pair(true, entry_);
    else
        return std::make_pair(false, 0);
}

std::pair<bool, float> VulkanRayTracing::intersect_trig(int_trig_t *trigs, const Ray &ray)
{
    bool LeftHandedNormal = true;
    bool NonZeroTolerance = false;

    auto negate_when_right_handed = [LeftHandedNormal](float x)
    {
        return LeftHandedNormal ? x : -x;
    };

    auto robust_max = [](float x, float y)
    {
        return x > y ? x : y;
    };

    float3 p0 = {trigs->v[0][0], trigs->v[0][1], trigs->v[0][2]};
    float3 p1 = {trigs->v[1][0], trigs->v[1][1], trigs->v[1][2]};
    float3 p2 = {trigs->v[2][0], trigs->v[2][1], trigs->v[2][2]};

    float3 e1 = p0 - p1;
    float3 e2 = p2 - p0;
    float3 n = LeftHandedNormal ? cross(e1, e2) : cross(e2, e1);

    float3 c = p0 - ray.get_origin();
    float3 r = cross(ray.get_direction(), c);
    float inv_det = negate_when_right_handed(1.0f) / dot(n, ray.get_direction());

    float u = dot(r, e2) * inv_det;
    float v = dot(r, e1) * inv_det;
    float w = 1.0f - u - v;

    const float tolerance = NonZeroTolerance ? -std::numeric_limits<float>::epsilon() : 0.0f;
    if (u >= tolerance && v >= tolerance && w >= tolerance)
    {
        float t = negate_when_right_handed(dot(n, c)) * inv_det;
        if (t >= ray.get_tmin() && t <= ray.get_tmax())
        {
            if (NonZeroTolerance)
            {
                u = robust_max(u, 0.0f);
                v = robust_max(v, 0.0f);
            }
            return std::make_pair(true, t);
        }
    }

    return std::make_pair(false, 0.0f);
}

bool debugTraversal = false;
bool found_AS = false;
VkAccelerationStructureKHR topLevelAS_first = NULL;
std::string time_offset = "";

void VulkanRayTracing::traceRay(VkAccelerationStructureKHR _topLevelAS,
                                uint rayFlags,
                                uint cullMask,
                                uint sbtRecordOffset,
                                uint sbtRecordStride,
                                uint missIndex,
                                float3 origin,
                                float Tmin,
                                float3 direction,
                                float Tmax,
                                int payload,
                                const ptx_instruction *pI,
                                ptx_thread_info *thread)
{
    //``` use for memory dump
    if (time_offset == "")
    {
        std::time_t raw_time = std::time(0);
        struct tm *time_info;
        char time_buf[30];

        time_info = localtime(&raw_time);
        strftime(time_buf, sizeof(time_buf), "%d-%m-%Y-%H-%M-%S", time_info);
        time_offset = time_buf;
    }
    //```

    uint32_t best_trig_offset = -1;
    Traversal_data traversal_data;

    traversal_data.n_all_hits = 0;
    traversal_data.ray_world_direction = direction;
    traversal_data.ray_world_origin = origin;
    traversal_data.sbtRecordOffset = sbtRecordOffset;
    traversal_data.sbtRecordStride = sbtRecordStride;
    traversal_data.missIndex = missIndex;
    traversal_data.Tmin = Tmin;
    traversal_data.Tmax = Tmax;

    bool hit_procedural = false;
    bool terminateOnFirstHit = rayFlags & SpvRayFlagsTerminateOnFirstHitKHRMask;
    bool skipClosestHitShader = rayFlags & SpvRayFlagsSkipClosestHitShaderKHRMask;
    bool skipAnyHitShader = rayFlags & SpvRayFlagsOpaqueKHRMask;

    std::vector<MemoryTransactionRecord> transactions;
    std::vector<MemoryStoreTransactionRecord> store_transactions;

    gpgpu_context *ctx = GPGPU_Context();

    if (terminateOnFirstHit)
        ctx->func_sim->g_n_anyhit_rays++;
    else
        ctx->func_sim->g_n_closesthit_rays++;

    unsigned total_nodes_accessed = 0;
    unsigned total_traverse_steps = 0;
    // std::map<uint8_t *, unsigned> tree_level_map;

    // Convert the direction vector length into 1
    float magnitude = std::sqrt(direction.x * direction.x +
                                direction.y * direction.y +
                                direction.z * direction.z);

    direction.x /= magnitude;
    direction.y /= magnitude;
    direction.z /= magnitude;

    // Create ray
    Ray ray;
    ray.make_ray(origin, direction, Tmin, Tmax);
    thread->add_ray_properties(ray);

    // Preprocess transform
    float4x4 worldToObjectMatrix = {
        {{1.0000f, 0.0000f, 0.0000f, 0.0000f},
         {0.0000f, 1.0000f, 0.0000f, 0.0000f},
         {0.0000f, 0.0000f, 1.0000f, 0.0000f},
         {0.0000f, 0.0000f, 0.0000f, 1.0000f}}};

    float4x4 objectToWorldMatrix = {
        {{1.0000f, 0.0000f, 0.0000f, 0.0000f},
         {0.0000f, 1.0000f, 0.0000f, 0.0000f},
         {0.0000f, 0.0000f, 1.0000f, 0.0000f},
         {0.0000f, 0.0000f, 0.0000f, 1.0000f}}};

    float worldToObject_tMultiplier;
    Ray objectRay = make_transformed_ray(ray, worldToObjectMatrix, &worldToObject_tMultiplier);
    float original_tmax = objectRay.get_tmax();

    // Preprocess ray
    std::array<bool, 3> octant = {
        std::signbit(objectRay.get_direction().x),
        std::signbit(objectRay.get_direction().y),
        std::signbit(objectRay.get_direction().z)};
    std::array<float, 3> w = {
        1.0f / objectRay.get_direction().x,
        1.0f / objectRay.get_direction().y,
        1.0f / objectRay.get_direction().z};
    std::array<float, 3> b = {
        -objectRay.get_origin().x * w[0],
        -objectRay.get_origin().y * w[1],
        -objectRay.get_origin().z * w[2]};
    int_w_t int_w = get_int_w(w);
    uint8_t global_tmax_version = 0;

    // Set thit to max
    float min_thit = ray.dir_tmax.w;
    struct GEN_RT_BVH_QUAD_LEAF closest_leaf;
    struct GEN_RT_BVH_INSTANCE_LEAF closest_instanceLeaf;
    float4x4 closest_worldToObject, closest_objectToWorld;
    Ray closest_objectRay;
    float min_thit_object;

    auto memory_dump = [&](uint64_t index, TransactionType type)
    {
        std::ofstream outfile(time_offset + "_memdump.txt", std::ios_base::app);

        if (type == TransactionType::INT_BVH_CLUSTER)
        {
            outfile << "CLUS " << index << std::endl;
        }

        if (type == TransactionType::INT_BVH_TRIG)
        {
            outfile << "TRIG " << index << std::endl;
        }

        if (type == TransactionType::INT_BVH_NODE)
        {
            outfile << "NODE " << index << std::endl;
        }

        if (type == TransactionType::INT_BVH_PRIMITIVE_INSTANCE)
        {
            outfile << "IDX " << index << std::endl;
        }

        outfile.close();
    };

    auto transaction_record = [&](uint64_t index, TransactionType type)
    {
        memory_dump(index, type);

        uint64_t base_addr;
        uint32_t length;

        if (type == TransactionType::INT_BVH_CLUSTER)
        {
            base_addr = int_bvh_clusters_addr;
            length = INT_BVH_CLUSTER_length;
        }

        if (type == TransactionType::INT_BVH_TRIG)
        {
            base_addr = int_bvh_trigs_addr;
            length = INT_BVH_TRIG_length;
        }

        if (type == TransactionType::INT_BVH_NODE)
        {
            base_addr = int_bvh_nodes_addr;
            length = INT_BVH_NODE_length;
        }

        if (type == TransactionType::INT_BVH_PRIMITIVE_INSTANCE)
        {
            base_addr = int_bvh_primitive_indices_addr;
            length = INT_BVH_PRIMITIVE_INSTANCE_length;
        }

        uint64_t target_addr = base_addr + index * length;
        uint64_t align_addr = target_addr & ~(INT_BVH_ALIGNMENT - 1);
        uint64_t next_addr = align_addr + INT_BVH_ALIGNMENT;

        if (next_addr - target_addr > length)
        {
            transactions.push_back(MemoryTransactionRecord((uint8_t *)((uint64_t)align_addr),
                                                           INT_BVH_ALIGNMENT, type));
            ctx->func_sim->g_rt_mem_access_type[static_cast<int>(type)] += 1;
        }
        else
        {
            transactions.push_back(MemoryTransactionRecord((uint8_t *)((uint64_t)align_addr),
                                                           INT_BVH_ALIGNMENT, type));
            transactions.push_back(MemoryTransactionRecord((uint8_t *)((uint64_t)next_addr),
                                                           INT_BVH_ALIGNMENT, type));
            ctx->func_sim->g_rt_mem_access_type[static_cast<int>(type)] += 2;
        }
    };

    cluster_data_t cluster_data = {.num_nodes_in_stk_2 = 0};
    std::stack<cluster_data_t> stk_1;
    std::stack<std::pair<uint16_t, uint16_t>> stk_2; // [local_node_idx, cluster_idx]

    auto update_cluster_data = [&](uint16_t cluster_idx) -> bool
    {
        int_cluster_t cluster = int_bvh.clusters[cluster_idx];
        transaction_record(cluster_idx, TransactionType::INT_BVH_CLUSTER);

        // Get root cluster and set min/max
        if (!ctx->func_sim->g_rt_world_set && cluster_idx == 0)
        {
            int_cluster_t root_cluster = cluster;

            float3 lo, hi;
            lo.x = root_cluster.ref_bounds[0];
            lo.y = root_cluster.ref_bounds[2];
            lo.z = root_cluster.ref_bounds[4];

            hi.x = root_cluster.ref_bounds[1];
            hi.y = root_cluster.ref_bounds[3];
            hi.z = root_cluster.ref_bounds[5];

            ctx->func_sim->g_rt_world_min = min(ctx->func_sim->g_rt_world_min, lo);
            ctx->func_sim->g_rt_world_max = min(ctx->func_sim->g_rt_world_max, hi);
            ctx->func_sim->g_rt_world_set = true;
        }

        std::pair<bool, float> y_ref_pair = intersect_bbox(octant, w, cluster.ref_bounds, b, objectRay.get_tmax());
        if (!y_ref_pair.first)
            return false;

        if (cluster_data.num_nodes_in_stk_2 != 0)
            stk_1.push(cluster_data);

        cluster_data.cluster_idx = cluster_idx;
        cluster_data.local_nodes = &int_bvh.nodes[cluster.node_offset];
        cluster_data.local_trigs = &int_bvh.trigs[cluster.trig_offset];

        cluster_data.node_offset = cluster.node_offset;
        cluster_data.trig_offset = cluster.trig_offset;

        cluster_data.inv_sx_inv_sw = cluster.inv_sx_inv_sw;
        cluster_data.y_ref = y_ref_pair.second;

        for (int i = 0; i < 3; i++)
        {
            cluster_data.qb_l[i] = floor_to_int32((b[i] - cluster_data.y_ref + cluster.ref_bounds[2 * i] * w[i]) *
                                                  cluster_data.inv_sx_inv_sw);
            cluster_data.qb_h[i] = cluster_data.qb_l[i] + 1;
        }

        cluster_data.tmax_version = global_tmax_version;
        cluster_data.qy_max = ceil_to_int32((objectRay.get_tmax() - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
        cluster_data.num_nodes_in_stk_2 = 0;
        return true;
    };

    // intersect root cluster
    bool start_tracing = update_cluster_data(0);

    uint16_t curr_local_node_idx = 0;
    auto update_node_and_cluster = [&](const decoded_data_t &decoded_data) -> bool
    {
        switch (decoded_data.child_type)
        {
        case child_type_t::INTERNAL:
            curr_local_node_idx = decoded_data.idx;
            return true;
        case child_type_t::SWITCH:
            curr_local_node_idx = 0;
            return update_cluster_data(decoded_data.idx);
        default:
            assert(false);
        }
    };

    auto intersect_leaf = [&](const decoded_data_t &decoded_data, const cluster_data_t &curr_cluster, const Ray &ray) -> uint32_t
    {
        assert(decoded_data.child_type == child_type_t::LEAF);
        uint32_t best_trig_offset = -1;

        for (int i = 0; i < decoded_data.num_trigs; i++)
        {
            total_nodes_accessed++;

            uint32_t trig_offset = curr_cluster.trig_offset + decoded_data.idx + i;
            int_trig_t *tmp_trigs = &int_bvh.trigs[trig_offset];
            transaction_record(trig_offset, TransactionType::INT_BVH_TRIG);

            auto hit = intersect_trig(tmp_trigs, ray);

            if (hit.first)
            {
                ray.set_tmax(hit.second);
                best_trig_offset = trig_offset;
            }
        }

        return best_trig_offset;
    };

    auto intersect_ray = [&](uint32_t &trig_offset)
    {
        int_trig_t *tmp_trigs = &int_bvh.trigs[trig_offset];
        transaction_record(trig_offset, TransactionType::INT_BVH_TRIG);

        float3 p[3]; // Extract vertices from leaf
        for (int i = 0; i < 3; i++)
        {
            p[i].x = tmp_trigs->v[i][0];
            p[i].y = tmp_trigs->v[i][1];
            p[i].z = tmp_trigs->v[i][2];
        }

        // Perform triangle intersection test
        float thit;
        bool hit = VulkanRayTracing::mt_ray_triangle_test(p[0], p[1], p[2], objectRay, &thit);

        float world_thit = thit / worldToObject_tMultiplier;

        if (hit && Tmin <= world_thit && world_thit <= Tmax)
        {
            uint32_t prim_id = (uint32_t)int_bvh.primitive_indices[trig_offset];
            transaction_record(trig_offset, TransactionType::INT_BVH_PRIMITIVE_INSTANCE);

            if (skipAnyHitShader && world_thit < min_thit)
            {
                min_thit = world_thit;
                min_thit_object = thit;
                closest_leaf.LeafDescriptor.GeometryIndex = 0;
                closest_leaf.PrimitiveIndex0 = prim_id;
                closest_instanceLeaf.InstanceID = 0;

                closest_worldToObject = worldToObjectMatrix;
                closest_objectToWorld = objectToWorldMatrix;
                closest_objectRay = objectRay;

                for (int i = 0; i < 3; i++)
                {
                    closest_leaf.QuadVertex[i].X = tmp_trigs->v[i][0];
                    closest_leaf.QuadVertex[i].Y = tmp_trigs->v[i][1];
                    closest_leaf.QuadVertex[i].Z = tmp_trigs->v[i][2];
                }

                thread->add_ray_intersect();
            }
        }
    };

    uint64_t buffer_addr = 0; // 64 byte buffer

    while (start_tracing)
    {
        total_nodes_accessed++;
        total_traverse_steps++;

        int_node_t *curr_node = &cluster_data.local_nodes[curr_local_node_idx];
        transaction_record(curr_local_node_idx, TransactionType::INT_BVH_NODE);

        decoded_data_t left_decoded_data = decode_data(curr_node->left_child_data);
        decoded_data_t right_decoded_data = decode_data(curr_node->right_child_data);

        // optional, but can reduce traversal steps
        if (cluster_data.tmax_version != global_tmax_version)
        {
            cluster_data.tmax_version = global_tmax_version;
            cluster_data.qy_max = ceil_to_int32((objectRay.get_tmax() - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
        }

        auto distance_left = intersect_int_bbox(cluster_data.qy_max, int_w, curr_node->left_bounds,
                                                cluster_data.qb_l, cluster_data.qb_h);
        auto distance_right = intersect_int_bbox(cluster_data.qy_max, int_w, curr_node->right_bounds,
                                                 cluster_data.qb_l, cluster_data.qb_h);

        bool left_hit = false;
        bool right_hit = false;

        if (distance_left.first)
        {
            if (left_decoded_data.child_type == child_type_t::LEAF)
            {
                uint32_t trig_offset = intersect_leaf(left_decoded_data, cluster_data, objectRay);

                if (trig_offset != -1)
                {
                    best_trig_offset = trig_offset;
                    global_tmax_version++;
                }
            }
            else
            {
                left_hit = true;
            }
        }

        if (distance_right.first)
        {
            if (right_decoded_data.child_type == child_type_t::LEAF)
            {
                uint32_t trig_offset = intersect_leaf(right_decoded_data, cluster_data, objectRay);

                if (trig_offset != -1)
                {
                    best_trig_offset = trig_offset;
                    global_tmax_version++;
                }
            }
            else
            {
                right_hit = true;
            }
        }

        if (left_hit)
        {
            if (right_hit)
            {
                // ensure left_decoded_data is closer
                if (distance_left.second > distance_right.second)
                    std::swap(left_decoded_data, right_decoded_data);

                // push to stk_2
                switch (right_decoded_data.child_type)
                {
                case child_type_t::INTERNAL:
                    cluster_data.num_nodes_in_stk_2++;
                    stk_2.emplace(right_decoded_data.idx, cluster_data.cluster_idx);
                    break;
                case child_type_t::SWITCH:
                    stk_2.emplace(0, right_decoded_data.idx);
                    break;
                default:
                    assert(false);
                }
            }

            if (update_node_and_cluster(left_decoded_data))
                continue;
        }
        else if (right_hit)
        {
            if (update_node_and_cluster(right_decoded_data))
                continue;
        }

        // pop from stk_2 until we found a valid node
        while (true)
        {
            if (stk_2.empty())
                goto end;

            curr_local_node_idx = stk_2.top().first;
            int cluster_idx = stk_2.top().second;
            stk_2.pop();

            if (cluster_data.cluster_idx == cluster_idx)
            {
                cluster_data.num_nodes_in_stk_2--;
                break;
            }

            if ((!stk_1.empty() && stk_1.top().cluster_idx == cluster_idx))
            {
                cluster_data = stk_1.top();
                stk_1.pop();
                cluster_data.num_nodes_in_stk_2--;
                break;
            }

            if (update_cluster_data(cluster_idx))
                break;
        }
    }

end:
    assert(stk_1.empty());

    if (best_trig_offset != -1)
    {
        objectRay.set_tmax(original_tmax);
        intersect_ray(best_trig_offset);
    }

    if (min_thit < ray.dir_tmax.w)
    {
        traversal_data.hit_geometry = true;
        ctx->func_sim->g_rt_num_hits++;
        traversal_data.closest_hit.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        traversal_data.closest_hit.geometry_index = closest_leaf.LeafDescriptor.GeometryIndex;
        traversal_data.closest_hit.primitive_index = closest_leaf.PrimitiveIndex0;
        traversal_data.closest_hit.instance_index = closest_instanceLeaf.InstanceID;

        float3 intersection_point = ray.get_origin() + make_float3(ray.get_direction().x * min_thit, ray.get_direction().y * min_thit, ray.get_direction().z * min_thit);
        float3 rayatinter = ray.at(min_thit);

        // assert(intersection_point.x == ray.at(min_thit).x && intersection_point.y == ray.at(min_thit).y && intersection_point.z == ray.at(min_thit).z);
        traversal_data.closest_hit.intersection_point = intersection_point;
        traversal_data.closest_hit.worldToObjectMatrix = closest_worldToObject;
        traversal_data.closest_hit.objectToWorldMatrix = closest_objectToWorld;
        traversal_data.closest_hit.world_min_thit = min_thit;

        // printf("(ycpin) gpgpusim: Ray [%d] awaiting %d anyhit shader calls\n", thread->get_uid(), traversal_data.n_all_hits);
        // printf("(ycpin) gpgpusim: Ray hit geomID = %d primID = %d instID = %d\n",
        //        traversal_data.closest_hit.geometry_index,
        //        traversal_data.closest_hit.primitive_index,
        //        traversal_data.closest_hit.instance_index);
        // fflush(stdout);

        assert(thread->RT_thread_data->all_hit_data.size() == traversal_data.n_all_hits);

        float3 p[3];
        for (int i = 0; i < 3; i++)
        {
            p[i].x = closest_leaf.QuadVertex[i].X;
            p[i].y = closest_leaf.QuadVertex[i].Y;
            p[i].z = closest_leaf.QuadVertex[i].Z;
        }

        float3 object_intersection_point = closest_objectRay.get_origin() + make_float3(closest_objectRay.get_direction().x * min_thit_object, closest_objectRay.get_direction().y * min_thit_object, closest_objectRay.get_direction().z * min_thit_object);
        // closest_objectRay.at(min_thit_object);
        float3 barycentric = Barycentric(object_intersection_point, p[0], p[1], p[2]);
        traversal_data.closest_hit.barycentric_coordinates = barycentric;
        thread->RT_thread_data->set_hitAttribute(barycentric, pI, thread);
        // store_transactions.push_back(MemoryStoreTransactionRecord(&traversal_data, sizeof(traversal_data), StoreTransactionType::Traversal_Results));
    }
    else if (hit_procedural)
    {
        VSIM_DPRINTF("gpgpusim: Ray hit procedural geometry; requires intersection shader.\n");
        traversal_data.hit_geometry = false;
    }
    else
    {
        VSIM_DPRINTF("gpgpusim: Ray [%d] missed.\n", thread->get_uid());
        traversal_data.hit_geometry = false;
    }

    memory_space *mem = thread->get_global_memory();
    Traversal_data *device_traversal_data = (Traversal_data *)VulkanRayTracing::gpgpusim_alloc(sizeof(Traversal_data));
    mem->write(device_traversal_data, sizeof(Traversal_data), &traversal_data, thread, pI);
    thread->RT_thread_data->traversal_data.push_back(device_traversal_data);

    thread->set_rt_transactions(transactions);
    thread->set_rt_store_transactions(store_transactions);

    if (total_nodes_accessed > ctx->func_sim->g_max_nodes_per_ray)
    {
        ctx->func_sim->g_max_nodes_per_ray = total_nodes_accessed;
    }

    ctx->func_sim->g_tot_nodes_per_ray += total_nodes_accessed;
    ctx->func_sim->g_tot_traversal_steps += total_traverse_steps;

    // unsigned level = 0;
    // for (auto it = tree_level_map.begin(); it != tree_level_map.end(); it++)
    // {
    //     if (it->second > level)
    //     {
    //         level = it->second;
    //     }
    // }
    // if (level > ctx->func_sim->g_max_tree_depth)
    // {
    //     ctx->func_sim->g_max_tree_depth = level;
    // }

    // printf("Traversal: \n");
    // for (auto t : transactions)
    // {
    //     printf("\ttransaction %d, address %p, size %d\n", t.type, t.address, t.size);
    // }

    // fflush(stdout);
}

// clang-format off

void VulkanRayTracing::endTraceRay(const ptx_instruction *pI, ptx_thread_info *thread)
{
    assert(thread->RT_thread_data->traversal_data.size() > 0);
    thread->RT_thread_data->traversal_data.pop_back();
    thread->RT_thread_data->all_hit_data.clear();
    warp_intersection_table* itable = intersection_table[thread->get_ctaid().x][thread->get_ctaid().y];
    itable->clear(pI, thread);
    warp_intersection_table* atable = anyhit_table[thread->get_ctaid().x][thread->get_ctaid().y];
    atable->clear(pI, thread);
}

bool VulkanRayTracing::mt_ray_triangle_test(float3 p0, float3 p1, float3 p2, Ray ray_properties, float* thit)
{
    // Moller Trumbore algorithm (from scratchapixel.com)
    float3 v0v1 = p1 - p0;
    float3 v0v2 = p2 - p0;
    float3 pvec = cross(ray_properties.get_direction(), v0v2);
    float det = dot(v0v1, pvec);

    float idet = 1 / det;

    float3 tvec = ray_properties.get_origin() - p0;
    float u = dot(tvec, pvec) * idet;

    if (u < 0 || u > 1) return false;

    float3 qvec = cross(tvec, v0v1);
    float v = dot(ray_properties.get_direction(), qvec) * idet;

    if (v < 0 || (u + v) > 1) return false;

    *thit = dot(v0v2, qvec) * idet;
    return true;
}

float3 VulkanRayTracing::Barycentric(float3 p, float3 a, float3 b, float3 c)
{
    //source: https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
    float3 v0 = b - a;
    float3 v1 = c - a;
    float3 v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return {v, w, u};
}

void VulkanRayTracing::load_descriptor(const ptx_instruction *pI, ptx_thread_info *thread)
{

}


void VulkanRayTracing::setPipelineInfo(VkRayTracingPipelineCreateInfoKHR* pCreateInfos)
{
    VulkanRayTracing::pCreateInfos = pCreateInfos;
	std::cout << "gpgpusim: set pipeline" << std::endl;
}


void VulkanRayTracing::setGeometries(VkAccelerationStructureGeometryKHR* pGeometries, uint32_t geometryCount)
{
    VulkanRayTracing::pGeometries = pGeometries;
    VulkanRayTracing::geometryCount = geometryCount;
	std::cout << "gpgpusim: set geometry" << std::endl;
}

void VulkanRayTracing::setAccelerationStructure(VkAccelerationStructureKHR accelerationStructure)
{
    GEN_RT_BVH topBVH; //TODO: test hit with world before traversal
    GEN_RT_BVH_unpack(&topBVH, (uint8_t *)accelerationStructure);

    std::cout << "gpgpusim: set AS" << std::endl;
    VulkanRayTracing::topLevelAS = accelerationStructure;
}

std::string base_name(std::string & path)
{
  return path.substr(path.find_last_of("/") + 1);
}

void VulkanRayTracing::setDescriptorSet(struct DESCRIPTOR_SET_STRUCT *set)
{
    if (VulkanRayTracing::descriptorSet == NULL) {
        printf("gpgpusim: set descriptor set 0x%x\n", set);
        VulkanRayTracing::descriptorSet = set;
    }
    // TODO: Figure out why it sets the descriptor set twice
    else {
        printf("gpgpusim: descriptor set already set; ignoring update.\n");
    }
}

// clang-format on

void VulkanRayTracing::iterateDescriptorSet(struct DESCRIPTOR_SET_STRUCT *set)
{
    uint32_t descriptorCount = set->layout->binding_count;

    printf("Descriptor Set: 0x%x\n", set);
    printf("Descriptor Count: %d\n", descriptorCount);

    gpgpu_context *ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);
    memory_space *mem = context->get_device()->get_gpgpu()->get_global_memory();

    // Iterate all descriptors
    for (uint32_t i = 0; i < descriptorCount; i++)
    {
        struct lvp_descriptor *desc = &set->descriptors[i];

        printf("Descriptor %d: \n", i);

        if (i == 12)
        {
            int_bvh_clusters_addr = desc->info.ubo.pmem;
            size_t num_clusters = desc->info.ubo.buffer_size / INT_BVH_CLUSTER_length;

            printf("  (ycpin) Address of int_bvh_clusters: 0x%lx\n", int_bvh_clusters_addr);
            printf("  (ycpin) Size of int_bvh_clusters: %zu\n", num_clusters);

            int_bvh.num_clusters = num_clusters;
            int_bvh.clusters = std::make_unique<int_cluster_t[]>(num_clusters);

            for (size_t i = 0; i < num_clusters; ++i)
            {
                mem->read((mem_addr_t)(int_bvh_clusters_addr + i * INT_BVH_CLUSTER_length),
                          INT_BVH_CLUSTER_length, (void *)&int_bvh.clusters[i]);
            }
        }
        else if (i == 13)
        {
            int_bvh_trigs_addr = desc->info.ubo.pmem;
            size_t num_trigs = desc->info.ubo.buffer_size / INT_BVH_TRIG_length;

            printf("  (ycpin) Address of int_bvh_trigs: 0x%lx\n", int_bvh_trigs_addr);
            printf("  (ycpin) Size of int_bvh_trigs: %zu\n", num_trigs);

            int_bvh.trigs = std::make_unique<int_trig_t[]>(num_trigs);

            for (size_t i = 0; i < num_trigs; ++i)
            {
                mem->read((mem_addr_t)(int_bvh_trigs_addr + i * INT_BVH_TRIG_length),
                          INT_BVH_TRIG_length, (void *)&int_bvh.trigs[i]);
            }
        }
        else if (i == 14)
        {
            int_bvh_nodes_addr = desc->info.ubo.pmem;
            size_t node_count = desc->info.ubo.buffer_size / INT_BVH_NODE_length;

            printf("  (ycpin) Address of int_bvh_nodes: 0x%lx\n", int_bvh_nodes_addr);
            printf("  (ycpin) Size of int_bvh_nodes: %zu\n", node_count);

            int_bvh.nodes = std::make_unique<int_node_t[]>(node_count);

            for (size_t i = 0; i < node_count; ++i)
            {
                mem->read((mem_addr_t)(int_bvh_nodes_addr + i * INT_BVH_NODE_length),
                          INT_BVH_NODE_length, (void *)&int_bvh.nodes[i]);
            }
        }
        else if (i == 15)
        {
            int_bvh_primitive_indices_addr = desc->info.ubo.pmem;
            size_t num_primitive_indices = desc->info.ubo.buffer_size / INT_BVH_PRIMITIVE_INSTANCE_length;

            printf("  (ycpin) Address of int_bvh_primitive_indices: 0x%lx\n", int_bvh_primitive_indices_addr);
            printf("  (ycpin) Size of int_bvh_primitive_indices: %zu\n", num_primitive_indices);

            int_bvh.primitive_indices = std::make_unique<size_t[]>(num_primitive_indices);

            for (size_t i = 0; i < num_primitive_indices; ++i)
            {
                mem->read((mem_addr_t)(int_bvh_primitive_indices_addr + i * INT_BVH_PRIMITIVE_INSTANCE_length),
                          INT_BVH_PRIMITIVE_INSTANCE_length, (void *)&int_bvh.primitive_indices[i]);
            }
        }

        // Process according to different descriptor types
        switch (desc->type)
        {
        case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR:
            printf("  Acceleration Structure at %p\n", desc->info.ubo.pmem);
            break;

        case VK_DESCRIPTOR_TYPE_SAMPLER:
            printf("  Sampler at %p\n", desc->info.sampler);
            break;

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            printf("  Sampler View at %p, Sampler at %p\n", desc->info.sampler_view,
                   desc->info.sampler);
            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            printf("  Image View at %p (the image pointer)\n",
                   desc->info.image_view.image);
            break;

        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            printf(
                "  Storage Buffer (SSBO) at %p (the memory pointer), size = %ld\n",
                desc->info.ssbo.pmem, desc->info.ssbo.buffer_size);
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            printf("  Uniform Buffer (UBO) at %p (the memory pointer)\n",
                   desc->info.ubo.pmem);
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            printf("  Uniform data pointer: %p\n", desc->info.uniform);
            break;

        default:
            printf("  Unknown descriptor type\n");
            break;
        }
    }
}

// clang-format off

static bool invoked = false;

void copyHardCodedShaders()
{
    std::ifstream  src;
    std::ofstream  dst;

    // src.open("/home/mrs/emerald-ray-tracing/hardcodeShader/MESA_SHADER_MISS_2.ptx", std::ios::binary);
    // dst.open("/home/mrs/emerald-ray-tracing/mesagpgpusimShaders/MESA_SHADER_MISS_2.ptx", std::ios::binary);
    // dst << src.rdbuf();
    // src.close();
    // dst.close();
    
    // src.open("/home/mrs/emerald-ray-tracing/hardcodeShader/MESA_SHADER_CLOSEST_HIT_2.ptx", std::ios::binary);
    // dst.open("/home/mrs/emerald-ray-tracing/mesagpgpusimShaders/MESA_SHADER_CLOSEST_HIT_2.ptx", std::ios::binary);
    // dst << src.rdbuf();
    // src.close();
    // dst.close();

    // src.open("/home/mrs/emerald-ray-tracing/hardcodeShader/MESA_SHADER_RAYGEN_0.ptx", std::ios::binary);
    // dst.open("/home/mrs/emerald-ray-tracing/mesagpgpusimShaders/MESA_SHADER_RAYGEN_0.ptx", std::ios::binary);
    // dst << src.rdbuf();
    // src.close();
    // dst.close();

    // src.open("/home/mrs/emerald-ray-tracing/hardcodeShader/MESA_SHADER_INTERSECTION_4.ptx", std::ios::binary);
    // dst.open("/home/mrs/emerald-ray-tracing/mesagpgpusimShaders/MESA_SHADER_INTERSECTION_4.ptx", std::ios::binary);
    // dst << src.rdbuf();
    // src.close();
    // dst.close();

    // {
    //     std::ifstream  src("/home/mrs/emerald-ray-tracing/MESA_SHADER_MISS_0.ptx", std::ios::binary);
    //     std::ofstream  dst("/home/mrs/emerald-ray-tracing/mesagpgpusimShaders/MESA_SHADER_MISS_1.ptx",   std::ios::binary);
    //     dst << src.rdbuf();
    //     src.close();
    //     dst.close();
    // }
}

uint32_t VulkanRayTracing::registerShaders(char * shaderPath, gl_shader_stage shaderType)
{
    printf("gpgpusim: register shaders\n");
    copyHardCodedShaders();

    VulkanRayTracing::invoke_gpgpusim();
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    // Register all the ptx files in $MESA_ROOT/gpgpusimShaders by looping through them
    // std::vector <std::string> ptx_list;

    // Add ptx file names in gpgpusimShaders folder to ptx_list
    char *mesa_root = getenv("MESA_ROOT");
    char *gpgpusim_root = getenv("GPGPUSIM_ROOT");
    // char *filePath = "gpgpusimShaders/";
    // char fullPath[200];
    // snprintf(fullPath, sizeof(fullPath), "%s%s", mesa_root, filePath);
    // std::string fullPathString(fullPath);

    // for (auto &p : fs::recursive_directory_iterator(fullPathString))
    // {
    //     if (p.path().extension() == ".ptx")
    //     {
    //         //std::cout << p.path().string() << '\n';
    //         ptx_list.push_back(p.path().string());
    //     }
    // }

    std::string fullpath(shaderPath);
    std::string fullfilename = base_name(fullpath);
    std::string filenameNoExt;
    size_t start = fullfilename.find_first_not_of('.', 0);
    size_t end = fullfilename.find('.', start);
    filenameNoExt = fullfilename.substr(start, end - start);
    std::string idInString = filenameNoExt.substr(filenameNoExt.find_last_of("_") + 1);
    // Register each ptx file in ptx_list
    shader_stage_info shader;
    //shader.ID = VulkanRayTracing::shaders.size();
    shader.ID = std::stoi(idInString);
    shader.type = shaderType;
    shader.function_name = (char*)malloc(200 * sizeof(char));

    std::string deviceFunction;

    switch(shaderType) {
        case MESA_SHADER_RAYGEN:
            // shader.function_name = "raygen_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "raygen_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "MESA_SHADER_RAYGEN";
            break;
        case MESA_SHADER_ANY_HIT:
            // shader.function_name = "anyhit_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "anyhit_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "MESA_SHADER_ANY_HIT";
            break;
        case MESA_SHADER_CLOSEST_HIT:
            // shader.function_name = "closesthit_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "closesthit_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "MESA_SHADER_CLOSEST_HIT";
            break;
        case MESA_SHADER_MISS:
            // shader.function_name = "miss_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "miss_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "MESA_SHADER_MISS";
            break;
        case MESA_SHADER_INTERSECTION:
            // shader.function_name = "intersection_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "intersection_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "MESA_SHADER_INTERSECTION";
            break;
        case MESA_SHADER_CALLABLE:
            // shader.function_name = "callable_" + std::to_string(shader.ID);
            strcpy(shader.function_name, "callable_");
            strcat(shader.function_name, std::to_string(shader.ID).c_str());
            deviceFunction = "";
            assert(0);
            break;
    }
    deviceFunction += "_func" + std::to_string(shader.ID) + "_main";
    // deviceFunction += "_main";

    symbol_table *symtab;
    unsigned num_ptx_versions = 0;
    unsigned max_capability = 20;
    unsigned selected_capability = 20;
    bool found = false;
    
    unsigned long long fat_cubin_handle = shader.ID;

    // PTX File
    //std::cout << itr << std::endl;
    symtab = ctx->gpgpu_ptx_sim_load_ptx_from_filename(shaderPath);
    context->add_binary(symtab, fat_cubin_handle);
    // need to add all the magic registers to ptx.l to special_register, reference ayub ptx.l:225

    // PTX info
    // Run the python script and get ptxinfo
    std::cout << "GPGPUSIM: Generating PTXINFO for" << shaderPath << "info" << std::endl;
    char command[400];
    snprintf(command, sizeof(command), "python3 %s/scripts/generate_rt_ptxinfo.py %s", gpgpusim_root, shaderPath);
    int result = system(command);
    if (result != 0) {
        printf("GPGPU-Sim PTX: ERROR ** while loading PTX (b) %d\n", result);
        printf("               Ensure ptxas is in your path.\n");
        exit(1);
    }
    
    char ptxinfo_filename[400];
    snprintf(ptxinfo_filename, sizeof(ptxinfo_filename), "%sinfo", shaderPath);
    ctx->gpgpu_ptx_info_load_from_external_file(ptxinfo_filename); // TODO: make a version where it just loads my ptxinfo instead of generating a new one

    context->register_function(fat_cubin_handle, shader.function_name, deviceFunction.c_str());

    VulkanRayTracing::shaders.push_back(shader);

    return shader.ID;

    // if (itr.find("RAYGEN") != std::string::npos)
    // {
    //     printf("############### registering %s\n", shaderPath);
    //     context->register_function(fat_cubin_handle, "raygen_shader", "MESA_SHADER_RAYGEN_main");
    // }

    // if (itr.find("MISS") != std::string::npos)
    // {
    //     printf("############### registering %s\n", shaderPath);
    //     context->register_function(fat_cubin_handle, "miss_shader", "MESA_SHADER_MISS_main");
    // }

    // if (itr.find("CLOSEST") != std::string::npos)
    // {
    //     printf("############### registering %s\n", shaderPath);
    //     context->register_function(fat_cubin_handle, "closest_hit_shader", "MESA_SHADER_CLOSEST_HIT_main");
    // }
}


void VulkanRayTracing::invoke_gpgpusim()
{
    printf("gpgpusim: invoking gpgpusim\n");
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    if(!invoked)
    {
        //registerShaders();
        invoked = true;
    }
}

// int CmdTraceRaysKHRID = 0;

const bool writeImageBinary = true;

void VulkanRayTracing::vkCmdTraceRaysKHR(
                      void *raygen_sbt,
                      void *miss_sbt,
                      void *hit_sbt,
                      void *callable_sbt,
                      bool is_indirect,
                      uint32_t launch_width,
                      uint32_t launch_height,
                      uint32_t launch_depth,
                      uint64_t launch_size_addr) {
    printf("gpgpusim: launching cmd trace ray\n");
    // launch_width = 224;
    // launch_height = 160;

    std::cout << "  Launch Width: " << launch_width << std::endl;
    std::cout << "  Launch Height: " << launch_height << std::endl;
    std::cout << "  Launch Depth: " << launch_depth << std::endl;
    std::cout << "  Launch Size Address: " << launch_size_addr << std::endl;

    init(launch_width, launch_height);
    
    // Dump Descriptor Sets
    if (dump_trace) 
    {
        dump_descriptor_sets(VulkanRayTracing::descriptorSet);
        dump_callparams_and_sbt(raygen_sbt, miss_sbt, hit_sbt, callable_sbt, is_indirect, launch_width, launch_height, launch_depth, launch_size_addr);
    }

    // CmdTraceRaysKHRID++;
    // if(CmdTraceRaysKHRID != 1)
    //     return;
    // launch_width = 420;
    // launch_height = 320;

    if(writeImageBinary && !imageFile.is_open())
    {
        char* imageFileName;
        char defaultFileName[40] = "image.binary";
        if(getenv("VULKAN_IMAGE_FILE_NAME"))
            imageFileName = getenv("VULKAN_IMAGE_FILE_NAME");
        else
            imageFileName = defaultFileName;
        imageFile.open(imageFileName, std::ios::out | std::ios::binary);
        
        // imageFile.open("image.txt", std::ios::out);
    }
    // memset(((uint8_t*)descriptors[0][1].address), uint8_t(127), launch_height * launch_width * 4);
    // return;

    // {
    //     std::ifstream infile("debug_printf.log");
    //     std::string line;
    //     while (std::getline(infile, line))
    //     {
    //         if(line == "")
    //             continue;

    //         RayDebugGPUData data;
    //         // sscanf(line.c_str(), "LaunchID:(%d,%d), InstanceCustomIndex = %d, primitiveID = %d, v0 = (%f, %f, %f), v1 = (%f, %f, %f), v2 = (%f, %f, %f), hitAttribute = (%f, %f), normalWorld = (%f, %f, %f), objectIntersection = (%f, %f, %f), worldIntersection = (%f, %f, %f), objectNormal = (%f, %f, %f), worldNormal = (%f, %f, %f), NdotL = %f",
    //         //             &data.launchIDx, &data.launchIDy, &data.instanceCustomIndex, &data.primitiveID, &data.v0pos.x, &data.v0pos.y, &data.v0pos.z, &data.v1pos.x, &data.v1pos.y, &data.v1pos.z, &data.v2pos.x, &data.v2pos.y, &data.v2pos.z, &data.attribs.x, &data.attribs.y, &data.N.x, &data.N.y, &data.N.z, &data.P_object.x, &data.P_object.y, &data.P_object.z, &data.P.x, &data.P.y, &data.P.z, &data.N_object.x, &data.N_object.y, &data.N_object.z, &data.N.x, &data.N.y, &data.N.z, &data.NdotL);
    //         sscanf(line.c_str(), "launchID = (%d, %d), hitValue = (%f, %f, %f)",
    //                     &data.launchIDx, &data.launchIDy, &data.hitValue.x, &data.hitValue.y, &data.hitValue.z);
    //         data.valid = true;
    //         assert(data.launchIDx < 2000 && data.launchIDy < 2000);
    //         // printf("#### (%d, %d)\n", data.launchIDx, data.launchIDy);
    //         // fflush(stdout);
    //         rayDebugGPUData[data.launchIDx][data.launchIDy] = data;

    //     }
    // }

    assert(launch_depth == 1);

#if defined(MESA_USE_INTEL_DRIVER)
    struct DESCRIPTOR_STRUCT desc;
    desc.image_view = NULL;
#endif

    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    unsigned long shaderId = *(uint64_t*)raygen_sbt;
    int index = 0;
    for (int i = 0; i < shaders.size(); i++) {
        if (shaders[i].ID == 0){
            index = i;
            break;
        }
    }
    ctx->func_sim->g_total_shaders = shaders.size();

    shader_stage_info raygen_shader = shaders[index];
    function_info *entry = context->get_kernel(raygen_shader.function_name);
    // printf("################ number of args = %d\n", entry->num_args());

    if (entry->is_pdom_set()) {
        printf("GPGPU-Sim PTX: PDOM analysis already done for %s \n",
            entry->get_name().c_str());
    } else {
        printf("GPGPU-Sim PTX: finding reconvergence points for \'%s\'...\n",
            entry->get_name().c_str());
        /*
        * Some of the instructions like printf() gives the gpgpusim the wrong
        * impression that it is a function call. As printf() doesnt have a body
        * like functions do, doing pdom analysis for printf() causes a crash.
        */
        if (entry->get_function_size() > 0) entry->do_pdom();
        entry->set_pdom();
    }

    // check that number of args and return match function requirements
    //if (pI->has_return() ^ entry->has_return()) {
    //    printf(
    //        "GPGPU-Sim PTX: Execution error - mismatch in number of return values "
    //        "between\n"
    //        "               call instruction and function declaration\n");
    //    abort();
    //}
    unsigned n_return = entry->has_return();
    unsigned n_args = entry->num_args();
    //unsigned n_operands = pI->get_num_operands();

    // launch_width = 192;
    // launch_height = 32;

    dim3 blockDim = dim3(1, 1, 1);
    dim3 gridDim = dim3(1, launch_height, launch_depth);
    if(launch_width <= 32) {
        blockDim.x = launch_width;
        gridDim.x = 1;
    }
    else {
        blockDim.x = 32;
        gridDim.x = launch_width / 32;
        if(launch_width % 32 != 0)
            gridDim.x++;
    }
    printf("gpgpusim: launch dimensions %d x %d x %d\n", gridDim.x, gridDim.y, gridDim.z);

    gpgpu_ptx_sim_arg_list_t args;
    // kernel_info_t *grid = ctx->api->gpgpu_cuda_ptx_sim_init_grid(
    //   raygen_shader.function_name, args, dim3(4, 128, 1), dim3(32, 1, 1), context);
    kernel_info_t *grid = ctx->api->gpgpu_cuda_ptx_sim_init_grid(
      raygen_shader.function_name, args, gridDim, blockDim, context);
    grid->vulkan_metadata.raygen_sbt = raygen_sbt;
    grid->vulkan_metadata.miss_sbt = miss_sbt;
    grid->vulkan_metadata.hit_sbt = hit_sbt;
    grid->vulkan_metadata.callable_sbt = callable_sbt;
    grid->vulkan_metadata.launch_width = launch_width;
    grid->vulkan_metadata.launch_height = launch_height;
    grid->vulkan_metadata.launch_depth = launch_depth;
    
    printf("gpgpusim: SBT: raygen %p, miss %p, hit %p, callable %p\n", 
            raygen_sbt, miss_sbt, hit_sbt, callable_sbt);

    printf("gpgpusim: blas address\n");
    for (auto mapping : blas_addr_map) {
        printf("\t[%p] -> %p\n", mapping.first, mapping.second);
    }

    printf("gpgpusim: tlas address %p\n", tlas_addr);

    printf("(ycpin) gpgpusim: clusters address %p\n", int_bvh_clusters_addr);

    printf("(ycpin) gpgpusim: trigs address %p\n", int_bvh_trigs_addr);
    
    printf("(ycpin) gpgpusim: nodes address %p\n", int_bvh_nodes_addr);

    printf("(ycpin) gpgpusim: primitive indices address %p\n", int_bvh_primitive_indices_addr);
            
    struct CUstream_st *stream = 0;
    stream_operation op(grid, ctx->func_sim->g_ptx_sim_mode, stream);
    ctx->the_gpgpusim->g_stream_manager->push(op);

    fflush(stdout);

    while(!op.is_done() && !op.get_kernel()->done()) {
        printf("waiting for op to finish\n");
        sleep(1);
        continue;
    }
    // for (unsigned i = 0; i < entry->num_args(); i++) {
    //     std::pair<size_t, unsigned> p = entry->get_param_config(i);
    //     cudaSetupArgumentInternal(args[i], p.first, p.second);
    // }
}

void VulkanRayTracing::callMissShader(const ptx_instruction *pI, ptx_thread_info *thread) {
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    memory_space *mem = thread->get_global_memory();
    Traversal_data* traversal_data = thread->RT_thread_data->traversal_data.back();

    bool hit_geometry;
    mem->read(&(traversal_data->hit_geometry), sizeof(bool), &hit_geometry);
    assert(!hit_geometry);

    int32_t current_shader_counter = -1;
    mem->write(&(traversal_data->current_shader_counter), sizeof(traversal_data->current_shader_counter), &current_shader_counter, thread, pI);

    int32_t current_shader_type = -1;
    mem->write(&(traversal_data->current_shader_type), sizeof(traversal_data->current_shader_type), &current_shader_type, thread, pI);

    uint32_t missIndex;
    mem->read(&(traversal_data->missIndex), sizeof(traversal_data->missIndex), &missIndex);

    uint32_t shaderID = *((uint32_t *)(thread->get_kernel().vulkan_metadata.miss_sbt) + 8 * missIndex);
    VSIM_DPRINTF("gpgpusim: Calling Miss Shader at ID %d\n", shaderID);

    shader_stage_info miss_shader = shaders[shaderID];

    function_info *entry = context->get_kernel(miss_shader.function_name);
    callShader(pI, thread, entry);
}

void VulkanRayTracing::callClosestHitShader(const ptx_instruction *pI, ptx_thread_info *thread) {
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    memory_space *mem = thread->get_global_memory();
    Traversal_data* traversal_data = thread->RT_thread_data->traversal_data.back();

    bool hit_geometry;
    mem->read(&(traversal_data->hit_geometry), sizeof(bool), &hit_geometry);
    assert(hit_geometry);

    int32_t current_shader_counter = -1;
    mem->write(&(traversal_data->current_shader_counter), sizeof(traversal_data->current_shader_counter), &current_shader_counter, thread, pI);

    int32_t current_shader_type = -1;
    mem->write(&(traversal_data->current_shader_type), sizeof(traversal_data->current_shader_type), &current_shader_type, thread, pI);

    VkGeometryTypeKHR geometryType;
    mem->read(&(traversal_data->closest_hit.geometryType), sizeof(traversal_data->closest_hit.geometryType), &geometryType);

    shader_stage_info closesthit_shader;
    if(geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR) {
        uint32_t shaderID = *((uint32_t *)(thread->get_kernel().vulkan_metadata.hit_sbt));
        closesthit_shader = shaders[shaderID];
        VSIM_DPRINTF("gpgpusim: Calling Closest Hit Shader at ID %d\n", shaderID);

    }
    else {
        int32_t hitGroupIndex;
        mem->read(&(traversal_data->closest_hit.hitGroupIndex), sizeof(traversal_data->closest_hit.hitGroupIndex), &hitGroupIndex);
        uint32_t shaderID = *((uint32_t *)(thread->get_kernel().vulkan_metadata.hit_sbt) + 8 * hitGroupIndex);
        closesthit_shader = shaders[shaderID];
        VSIM_DPRINTF("gpgpusim: Calling Closest Hit Shader at ID %d\n", shaderID);
    }

    function_info *entry = context->get_kernel(closesthit_shader.function_name);
    callShader(pI, thread, entry);
}

void VulkanRayTracing::callIntersectionShader(const ptx_instruction *pI, ptx_thread_info *thread, uint32_t shader_counter) {
    VSIM_DPRINTF("gpgpusim: Calling Intersection Shader\n");
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);
    
    memory_space *mem = thread->get_global_memory();
    Traversal_data* traversal_data = thread->RT_thread_data->traversal_data.back();
    mem->write(&(traversal_data->current_shader_counter), sizeof(traversal_data->current_shader_counter), &shader_counter, thread, pI);

    int32_t current_shader_type = 1;
    mem->write(&(traversal_data->current_shader_type), sizeof(traversal_data->current_shader_type), &current_shader_type, thread, pI);

    warp_intersection_table* table = VulkanRayTracing::intersection_table[thread->get_ctaid().x][thread->get_ctaid().y];
    uint32_t hitGroupIndex = table->get_hitGroupIndex(shader_counter, thread->get_tid().x, pI, thread);

    shader_stage_info intersection_shader = shaders[*((uint32_t *)(thread->get_kernel().vulkan_metadata.hit_sbt) + 8 * hitGroupIndex + 1)];
    function_info *entry = context->get_kernel(intersection_shader.function_name);
    callShader(pI, thread, entry);
}

void VulkanRayTracing::callAnyHitShader(const ptx_instruction *pI, ptx_thread_info *thread, uint32_t shader_counter) {
    VSIM_DPRINTF("gpgpusim: Calling Any Hit Shader\n");
    gpgpu_context *ctx;
    ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);

    memory_space *mem = thread->get_global_memory();
    Traversal_data* traversal_data = thread->RT_thread_data->traversal_data.back();
    mem->write(&(traversal_data->current_shader_counter), sizeof(traversal_data->current_shader_counter), &shader_counter, thread, pI);

    int32_t current_shader_type = 2;
    mem->write(&(traversal_data->current_shader_type), sizeof(traversal_data->current_shader_type), &current_shader_type, thread, pI);

    warp_intersection_table* table = VulkanRayTracing::anyhit_table[thread->get_ctaid().x][thread->get_ctaid().y];
    uint32_t hitGroupIndex = table->get_hitGroupIndex(shader_counter, thread->get_tid().x, pI, thread);

    shader_stage_info anyhit_shader = shaders[*((uint32_t *)(thread->get_kernel().vulkan_metadata.hit_sbt) + 8 * hitGroupIndex + 1)];
    function_info *entry = context->get_kernel(anyhit_shader.function_name);
    callShader(pI, thread, entry);
}

void VulkanRayTracing::callShader(const ptx_instruction *pI, ptx_thread_info *thread, function_info *target_func) {
    static unsigned call_uid_next = 1;

  if (target_func->is_pdom_set()) {
    // printf("GPGPU-Sim PTX: PDOM analysis already done for %s \n",
    //        target_func->get_name().c_str());
  } else {
    printf("GPGPU-Sim PTX: finding reconvergence points for \'%s\'...\n",
           target_func->get_name().c_str());
    /*
     * Some of the instructions like printf() gives the gpgpusim the wrong
     * impression that it is a function call. As printf() doesnt have a body
     * like functions do, doing pdom analysis for printf() causes a crash.
     */
    if (target_func->get_function_size() > 0) target_func->do_pdom();
    target_func->set_pdom();
  }

  thread->set_npc(target_func->get_start_PC());

  // check that number of args and return match function requirements
  if (pI->has_return() ^ target_func->has_return()) {
    printf(
        "GPGPU-Sim PTX: Execution error - mismatch in number of return values "
        "between\n"
        "               call instruction and function declaration\n");
    abort();
  }
  unsigned n_return = target_func->has_return();
  unsigned n_args = target_func->num_args();
  unsigned n_operands = pI->get_num_operands();

  // TODO: why this fails?
//   if (n_operands != (n_return + 1 + n_args)) {
//     printf(
//         "GPGPU-Sim PTX: Execution error - mismatch in number of arguements "
//         "between\n"
//         "               call instruction and function declaration\n");
//     abort();
//   }

  // handle intrinsic functions
//   std::string fname = target_func->get_name();
//   if (fname == "vprintf") {
//     gpgpusim_cuda_vprintf(pI, thread, target_func);
//     return;
//   }
// #if (CUDART_VERSION >= 5000)
//   // Jin: handle device runtime apis for CDP
//   else if (fname == "cudaGetParameterBufferV2") {
//     target_func->gpgpu_ctx->device_runtime->gpgpusim_cuda_getParameterBufferV2(
//         pI, thread, target_func);
//     return;
//   } else if (fname == "cudaLaunchDeviceV2") {
//     target_func->gpgpu_ctx->device_runtime->gpgpusim_cuda_launchDeviceV2(
//         pI, thread, target_func);
//     return;
//   } else if (fname == "cudaStreamCreateWithFlags") {
//     target_func->gpgpu_ctx->device_runtime->gpgpusim_cuda_streamCreateWithFlags(
//         pI, thread, target_func);
//     return;
//   }
// #endif

  // read source arguements into register specified in declaration of function
  arg_buffer_list_t arg_values;
  copy_args_into_buffer_list(pI, thread, target_func, arg_values);

  // record local for return value (we only support a single return value)
  const symbol *return_var_src = NULL;
  const symbol *return_var_dst = NULL;
  if (target_func->has_return()) {
    return_var_dst = pI->dst().get_symbol();
    return_var_src = target_func->get_return_var();
  }

  gpgpu_sim *gpu = thread->get_gpu();
  unsigned callee_pc = 0, callee_rpc = 0;
  /*if (gpu->simd_model() == POST_DOMINATOR)*/ { //MRS_TODO: why this fails?
    thread->get_core()->get_pdom_stack_top_info(thread->get_hw_wid(),
                                                &callee_pc, &callee_rpc);
    assert(callee_pc == thread->get_pc());
  }

  thread->callstack_push(callee_pc + pI->inst_size(), callee_rpc,
                         return_var_src, return_var_dst, call_uid_next++);

  copy_buffer_list_into_frame(thread, arg_values);

  thread->set_npc(target_func);
}

void VulkanRayTracing::setDescriptor(uint32_t setID, uint32_t descID, void *address, uint32_t size, VkDescriptorType type)
{
    printf("gpgpusim: set descriptor\n");
    if(descriptors.size() <= setID)
        descriptors.resize(setID + 1);
    if(descriptors[setID].size() <= descID)
        descriptors[setID].resize(descID + 1);
    
    descriptors[setID][descID].setID = setID;
    descriptors[setID][descID].descID = descID;
    descriptors[setID][descID].address = address;
    descriptors[setID][descID].size = size;
    descriptors[setID][descID].type = type;
}


void VulkanRayTracing::setDescriptorSetFromLauncher(void *address, void *deviceAddress, uint32_t setID, uint32_t descID)
{
    launcher_deviceDescriptorSets[setID][descID] = deviceAddress;
    launcher_descriptorSets[setID][descID] = address;
}

void* VulkanRayTracing::getDescriptorAddress(uint32_t setID, uint32_t binding)
{
#if defined(MESA_USE_INTEL_DRIVER)
    if (use_external_launcher)
    {
        return launcher_deviceDescriptorSets[setID][binding];
        // return launcher_descriptorSets[setID][binding];
    }
    else 
    {
        // assert(setID < descriptors.size());
        // assert(binding < descriptors[setID].size());

        struct anv_descriptor_set* set = VulkanRayTracing::descriptorSet;

        const struct anv_descriptor_set_binding_layout *bind_layout = &set->layout->binding[binding];
        struct anv_descriptor *desc = &set->descriptors[bind_layout->descriptor_index];
        void *desc_map = set->desc_mem.map + bind_layout->descriptor_offset;

        assert(desc->type == bind_layout->type);

        switch (desc->type)
        {
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                return (void *)(desc);
            }
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
                return desc;
            }

            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                assert(0);
                break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
                if (desc->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    desc->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                {
                    // MRS_TODO: account for desc->offset?
                    return anv_address_map(desc->buffer->address);
                }
                else
                {
                    struct anv_buffer_view *bview = &set->buffer_views[bind_layout->buffer_view_index];
                    return anv_address_map(bview->address);
                }
            }

            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                assert(0);
                break;

            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            {
                struct anv_address_range_descriptor *desc_data = desc_map;
                return (void *)(desc_data->address);
            }

            default:
                assert(0);
                break;
        }

        // return descriptors[setID][binding].address;
    }
#elif defined(MESA_USE_LVPIPE_DRIVER)
    VSIM_DPRINTF("gpgpusim: getDescriptorAddress for binding %d\n", binding);
    struct lvp_descriptor_set* set = VulkanRayTracing::descriptorSet;
    const struct lvp_descriptor_set_binding_layout *bind_layout = &set->layout->binding[binding];
    struct lvp_descriptor *desc = &set->descriptors[bind_layout->descriptor_index];

    // printf("DESCRIPTOR TYPE: %d\n", desc->type);
    switch (desc->type) {
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            VSIM_DPRINTF("gpgpusim: storage image; descriptor address %p\n", desc);
            return (void *) desc;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            VSIM_DPRINTF("gpgpusim: uniform buffer; buffer mem address %p\n", (void *) desc->info.ubo.pmem);
            return (void *) desc->info.ubo.pmem;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            VSIM_DPRINTF("gpgpusim: storage buffer; buffer mem address %p\n", (void *) desc->info.ssbo.pmem);
            return (void *) desc->info.ssbo.pmem;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            VSIM_DPRINTF("gpgpusim: accel struct; root address %p\n", (void *)desc->info.ubo.pmem + desc->info.ubo.buffer_offset);
            return (void *)desc->info.ubo.pmem + desc->info.ubo.buffer_offset;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            VSIM_DPRINTF("gpgpusim: image sampler; descriptor address %p\n", desc);
            return (void *) desc;
            break;
        default:
            VSIM_DPRINTF("gpgpusim: unimplemented descriptor type\n");
            abort();
    }
#endif
}

void VulkanRayTracing::getTexture(struct DESCRIPTOR_STRUCT *desc, 
                                    float x, float y, float lod, 
                                    float &c0, float &c1, float &c2, float &c3, 
                                    std::vector<ImageMemoryTransactionRecord>& transactions,
                                    uint64_t launcher_offset)
{
#if defined(MESA_USE_INTEL_DRIVER)
    Pixel pixel;

    if (use_external_launcher)
    {
        pixel = get_interpolated_pixel((anv_image_view*) desc, (anv_sampler*) desc, x, y, transactions, launcher_offset); // cast back to metadata later
    }
    else 
    {
        struct anv_image_view *image_view =  desc->image_view;
        struct anv_sampler *sampler = desc->sampler;

        const struct anv_image *image = image_view->image;
        assert(image->n_planes == 1);
        assert(image->samples == 1);
        assert(image->tiling == VK_IMAGE_TILING_OPTIMAL);
        assert(image->planes[0].surface.isl.tiling == ISL_TILING_Y0);
        assert(sampler->conversion == NULL);

        pixel = get_interpolated_pixel(image_view, sampler, x, y, transactions);
    }

    TXL_DPRINTF("Setting transaction type to TEXTURE_LOAD\n");
    for(int i = 0; i < transactions.size(); i++)
        transactions[i].type = ImageTransactionType::TEXTURE_LOAD;
    
    c0 = pixel.c0;
    c1 = pixel.c1;
    c2 = pixel.c2;
    c3 = pixel.c3;


    // uint8_t* address = anv_address_map(image->planes[0].address);

    // for(int x = 0; x < image->extent.width; x++)
    // {
    //     for(int y = 0; y < image->extent.height; y++)
    //     {
    //         int blockX = x / 8;
    //         int blockY = y / 8;

    //         uint32_t offset = (blockX + blockY * (image->extent.width / 8)) * (128 / 8);

    //         uint8_t dst_colors[100];
    //         basisu::astc::decompress(dst_colors, address + offset, true, 8, 8);
    //         uint8_t* pixel_color = &dst_colors[0] + (x % 8 + (y % 8) * 8) * 4;

    //         uint32_t bit_map_offset = x + y * image->extent.width;

    //         float data[4];
    //         data[0] = pixel_color[0] / 255.0;
    //         data[1] = pixel_color[1] / 255.0;
    //         data[2] = pixel_color[2] / 255.0;
    //         data[3] = pixel_color[3] / 255.0;
    //         imageFile.write((char*) data, 3 * sizeof(float));
    //         imageFile.write((char*) (&bit_map_offset), sizeof(uint32_t));
    //         imageFile.flush();
    //     }
    // }
#elif defined(MESA_USE_LVPIPE_DRIVER)
    // printf("gpgpusim: getTexture not implemented for lavapipe.\n");
    //
    // printf("GIVEN DESC: %p\n", desc);

    if (x < 0 || x > 1)
        x -= std::floor(x);
    if (y < 0 || y > 1)
        y -= std::floor(y);

    // printf("X: %f, Y: %f\n", x, y);

    struct lvp_descriptor d = *(struct lvp_descriptor*) desc;
    const struct lvp_image *img = d.info.sampler_view->image;
    uint32_t width = img->vk.extent.width;
    uint32_t height = img->vk.extent.height;
    void *i = img->pmem;

    uint32_t x_int = std::floor(x * width);
    uint32_t y_int = std::floor(y * height);
    if(x_int >= width)
        x_int -= width;
    if(y_int >= height)
        y_int -= height;

    void *c = i + (y_int * height + x_int) * 4;

    ImageMemoryTransactionRecord transaction;
    transaction.type = ImageTransactionType::TEXTURE_LOAD;
    transaction.address = c;
    transaction.size = 4;
    transactions.push_back(transaction);

    uint8_t *colors = (uint8_t*) c;
    c0 = colors[0] / 255.0;
    c1 = colors[1] / 255.0;
    c2 = colors[2] / 255.0;
    c3 = colors[3] / 255.0;

    // abort();
#endif
}

#if defined(MESA_USE_LVPIPE_DRIVER)
FILE *img_bin = nullptr;
#endif

void VulkanRayTracing::image_load(struct DESCRIPTOR_STRUCT *desc, uint32_t x, uint32_t y, float &c0, float &c1, float &c2, float &c3)
{
#if defined(MESA_USE_INTEL_DRIVER)
    ImageMemoryTransactionRecord transaction;

    struct anv_image_view *image_view =  desc->image_view;
    struct anv_sampler *sampler = desc->sampler;

    const struct anv_image *image = image_view->image;
    assert(image->n_planes == 1);
    assert(image->samples == 1);
    assert(image->tiling == VK_IMAGE_TILING_OPTIMAL);
    assert(image->planes[0].surface.isl.tiling == ISL_TILING_Y0);
    assert(sampler->conversion == NULL);

    Pixel pixel = load_image_pixel(image, x, y, 0, transaction);

    transaction.type = ImageTransactionType::IMAGE_LOAD;
    
    c0 = pixel.c0;
    c1 = pixel.c1;
    c2 = pixel.c2;
    c3 = pixel.c3;

#elif defined(MESA_USE_LVPIPE_DRIVER)
    VSIM_DPRINTF("gpgpusim: image_load not implemented for lavapipe.\n");
    abort();

#endif
}

void VulkanRayTracing::image_store(struct DESCRIPTOR_STRUCT* desc, uint32_t gl_LaunchIDEXT_X, uint32_t gl_LaunchIDEXT_Y, uint32_t gl_LaunchIDEXT_Z, uint32_t gl_LaunchIDEXT_W, 
              float hitValue_X, float hitValue_Y, float hitValue_Z, float hitValue_W, const ptx_instruction *pI, ptx_thread_info *thread)
{
#if defined(MESA_USE_INTEL_DRIVER)
    ImageMemoryTransactionRecord transaction;
    Pixel pixel = Pixel(hitValue_X, hitValue_Y, hitValue_Z, hitValue_W);

    VkFormat vk_format;
    if (use_external_launcher)
    {
        storage_image_metadata *metadata = (storage_image_metadata*) desc;
        vk_format = metadata->format;
        store_image_pixel((anv_image*) desc, gl_LaunchIDEXT_X, gl_LaunchIDEXT_Y, 0, pixel, transaction);
    }
    else
    {
        assert(desc->sampler == NULL);

        struct anv_image_view *image_view = desc->image_view;
        assert(image_view != NULL);
        struct anv_image * image = image_view->image;

        vk_format = image->vk_format;

        store_image_pixel(image, gl_LaunchIDEXT_X, gl_LaunchIDEXT_Y, 0, pixel, transaction);
    }

    
    transaction.type = ImageTransactionType::IMAGE_STORE;

    if(writeImageBinary && vk_format != VK_FORMAT_R32G32B32A32_SFLOAT)
    {
        uint32_t image_width = thread->get_kernel().vulkan_metadata.launch_width;
        uint32_t offset = 0;
        offset += gl_LaunchIDEXT_Y * image_width;
        offset += gl_LaunchIDEXT_X;

        float data[4];
        data[0] = hitValue_X;
        data[1] = hitValue_Y;
        data[2] = hitValue_Z;
        data[3] = hitValue_W;
        imageFile.write((char*) data, 3 * sizeof(float));
        imageFile.write((char*) (&offset), sizeof(uint32_t));
        imageFile.flush();

        // imageFile << "(" << gl_LaunchIDEXT_X << ", " << gl_LaunchIDEXT_Y << ") : (";
        // imageFile << hitValue_X << ", " << hitValue_Y << ", " << hitValue_Z << ", " << hitValue_W << ")\n";
    }

    TXL_DPRINTF("Setting transaction for image_store\n");
    thread->set_txl_transactions(transaction);

    // // if(std::abs(hitValue_X - rayDebugGPUData[gl_LaunchIDEXT_X][gl_LaunchIDEXT_Y].hitValue.x) > 0.0001 || 
    // //     std::abs(hitValue_Y - rayDebugGPUData[gl_LaunchIDEXT_X][gl_LaunchIDEXT_Y].hitValue.y) > 0.0001 ||
    // //     std::abs(hitValue_Z - rayDebugGPUData[gl_LaunchIDEXT_X][gl_LaunchIDEXT_Y].hitValue.z) > 0.0001)
    // //     {
    // //         printf("wrong value. (%d, %d): (%f, %f, %f)\n"
    // //                 , gl_LaunchIDEXT_X, gl_LaunchIDEXT_Y, hitValue_X, hitValue_Y, hitValue_Z);
    // //     }
    
    // // if (gl_LaunchIDEXT_X == 1070 && gl_LaunchIDEXT_Y == 220)
    // //     printf("this one has wrong value\n");

    // // if(hitValue_X > 1 || hitValue_Y > 1 || hitValue_Z > 1)
    // // {
    // //     printf("this one has wrong value.\n");
    // // }
#elif defined(MESA_USE_LVPIPE_DRIVER)
    assert(desc->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    struct lvp_image *image = (struct lvp_image *)desc->info.image_view.image;
    VkFormat vk_format = image->vk.format;
    assert(image != NULL);
    VSIM_DPRINTF("gpgpusim: image_store to %s at %p\n", image->vk.base.object_name, image->pmem_gpgpusim);

    Pixel pixel = Pixel(hitValue_X, hitValue_Y, hitValue_Z, hitValue_W);

    uint32_t width = image->vk.extent.width;
    uint32_t height = image->vk.extent.height;

    if (writeImageBinary) {
        // TODO: fix the bottom, is NULL
        // assert(image->vk.base.object_name);
        // std::string img_name(image->vk.base.object_name);
        std::string img_name("SCENE");

        if (outputImages.find(img_name) == outputImages.end()) {
            std::time_t raw_time = std::time(0);
            struct tm *time_info;
            char time_buf[30];

            time_info = localtime(&raw_time);

            strftime(time_buf, sizeof(time_buf), "%d-%m-%Y-%H-%M-%S-", time_info);

            std::string time_offset(time_buf);
            std::string new_img_file_name = "quantize_" + time_offset + img_name;

            outputImages[img_name] = new_img_file_name + ".ppm";
            printf("gpgpusim: saving image %s to file %s\n", img_name.c_str(), outputImages[img_name].c_str());

            img_bin = fopen(outputImages[img_name].c_str(), "w");
            fprintf(img_bin, "P3\n%d %d\n255\n", width, height);
        }

        uint32_t header_offset = 
            strlen("P3\n \n255\n") + std::to_string(width).length() + std::to_string(height).length();
        uint32_t value_offset = (gl_LaunchIDEXT_X + gl_LaunchIDEXT_Y * width) * (3*3 + 3);
        fseeko(img_bin, header_offset + value_offset, SEEK_SET);
        fprintf(img_bin, "%3.0f %3.0f %3.0f\n", 
                hitValue_X * 255, hitValue_Y * 255, hitValue_Z * 255);
    }

    // Setup transaction record for timing model
    ImageMemoryTransactionRecord transaction;
    transaction.type = ImageTransactionType::IMAGE_STORE;

    VkImageTiling tiling = image->vk.tiling;
    uint32_t pixelX = gl_LaunchIDEXT_X;
    uint32_t pixelY = gl_LaunchIDEXT_Y;

    // Size of image_store content depends on data type
    switch (vk_format) {
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            transaction.size = 16;
            break; 

        case VK_FORMAT_B8G8R8A8_UNORM:
            transaction.size = 4;
            break;

        default:
            printf("gpgpusim: unsupported image format option %d\n", vk_format);
            abort();
    }

    switch (tiling) {
        // Just an arbitrary tiling (TODO: Find a better tiling option)
        case VK_IMAGE_TILING_OPTIMAL:
        {
            uint32_t tileWidth = 16;
            uint32_t tileHeight = 16;

            uint32_t nTileX = ceil(width / tileWidth);
            uint32_t tileX = floor(pixelX / tileWidth);
            uint32_t tileY = floor(pixelY / tileHeight);

            uint32_t tileOffset = tileWidth * tileHeight * (tileY * nTileX + tileX);
            uint32_t pixelOffset = (pixelY % tileHeight) * tileWidth + (pixelX % tileWidth);

            transaction.address = image->pmem_gpgpusim + ((tileOffset + pixelOffset) * transaction.size);
            break;
        }
        // Linear
        case VK_IMAGE_TILING_LINEAR:
        {
            uint32_t offset = pixelY * width + pixelX;
            transaction.address = image->pmem_gpgpusim + offset * transaction.size;
            break;
        }
        default:
        {
            printf("gpgpusim: unsupported image tiling option %d\n", tiling);
            abort();
        }
    }

    TXL_DPRINTF("Setting transaction for image_store\n");
    thread->set_txl_transactions(transaction);

    // store_image_pixel(image, gl_LaunchIDEXT_X, gl_LaunchIDEXT_Y, 0, pixel, transaction);
#endif
}

// variable_decleration_entry* VulkanRayTracing::get_variable_decleration_entry(std::string name, ptx_thread_info *thread)
// {
//     std::vector<variable_decleration_entry>& table = thread->RT_thread_data->variable_decleration_table;
//     for (int i = 0; i < table.size(); i++) {
//         if (table[i].name == name) {
//             assert (table[i].address != NULL);
//             return &(table[i]);
//         }
//     }
//     return NULL;
// }

// void VulkanRayTracing::add_variable_decleration_entry(uint64_t type, std::string name, uint64_t address, uint32_t size, ptx_thread_info *thread)
// {
//     variable_decleration_entry entry;

//     entry.type = type;
//     entry.name = name;
//     entry.address = address;
//     entry.size = size;
//     thread->RT_thread_data->variable_decleration_table.push_back(entry);
// }


void VulkanRayTracing::dumpTextures(struct DESCRIPTOR_STRUCT *desc, uint32_t setID, uint32_t binding, VkDescriptorType type)
{
#if defined(MESA_USE_INTEL_DRIVER)
    DESCRIPTOR_STRUCT *desc_offset = ((DESCRIPTOR_STRUCT*)((void*)desc)); // offset for raytracing_extended
    struct anv_image_view *image_view =  desc_offset->image_view;
    struct anv_sampler *sampler = desc_offset->sampler;

    const struct anv_image *image = image_view->image;
    assert(image->n_planes == 1);
    assert(image->samples == 1);
    assert(image->tiling == VK_IMAGE_TILING_OPTIMAL);
    assert(image->planes[0].surface.isl.tiling == ISL_TILING_Y0);
    assert(sampler->conversion == NULL);

    uint8_t* address = anv_address_map(image->planes[0].address);
    uint32_t image_extent_width = image->extent.width;
    uint32_t image_extent_height = image->extent.height;
    VkFormat format = image->vk_format;
    uint64_t size = image->size;

    VkFilter filter;
    if(sampler->conversion == NULL)
        filter = VK_FILTER_NEAREST;

    // Data to dump
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    char *filePath = "gpgpusimShaders/";
    char *extension = ".vkdescrptorsettexturedata";

    int VkDescriptorTypeNum;

    switch (type)
    {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            VkDescriptorTypeNum = 0;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            VkDescriptorTypeNum = 1;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            VkDescriptorTypeNum = 2;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            VkDescriptorTypeNum = 10;
            break;
        default:
            abort(); // should not be here!
    }

    // Texture data
    char fullPath[200];
    snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.vktexturedata", mesa_root, filePath, setID, binding);
    // File name format: setID_descID.vktexturedata

    fp = fopen(fullPath, "wb+");
    fwrite(address, 1, size, fp);
    fclose(fp);

    // Texture metadata
    snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.vktexturemetadata", mesa_root, filePath, setID, binding);
    fp = fopen(fullPath, "w+");
    // File name format: setID_descID.vktexturemetadata

    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", size, 
                                                 image_extent_width, 
                                                 image_extent_height, 
                                                 format, 
                                                 VkDescriptorTypeNum, 
                                                 image->n_planes, 
                                                 image->samples, 
                                                 image->tiling, 
                                                 image->planes[0].surface.isl.tiling,
                                                 image->planes[0].surface.isl.row_pitch_B,
                                                 filter);
    fclose(fp);
#elif defined(MESA_USE_LVPIPE_DRIVER)
    printf("gpgpusim: dumpTextures not implemented for lavapipe.\n");
    abort();

#endif

}


void VulkanRayTracing::dumpStorageImage(struct DESCRIPTOR_STRUCT *desc, uint32_t setID, uint32_t binding, VkDescriptorType type)
{
#if defined(MESA_USE_INTEL_DRIVER)
    assert(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    assert(desc->sampler == NULL);

    struct anv_image_view *image_view = desc->image_view;
    assert(image_view != NULL);
    struct anv_image * image = image_view->image;
    assert(image->n_planes == 1);
    assert(image->samples == 1);

    void* mem_address = anv_address_map(image->planes[0].address);

    VkFormat format = image->vk_format;
    VkImageTiling tiling = image->tiling;
    isl_tiling isl_tiling_mode = image->planes[0].surface.isl.tiling;
    uint32_t row_pitch_B  = image->planes[0].surface.isl.row_pitch_B;

    uint32_t width = image->extent.width;
    uint32_t height = image->extent.height;

    // Dump storage image metadata
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    char *filePath = "gpgpusimShaders/";
    char *extension = ".vkdescrptorsetdata";

    int VkDescriptorTypeNum = 3;

    char fullPath[200];
    snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.vkstorageimagemetadata", mesa_root, filePath, setID, binding);
    fp = fopen(fullPath, "w+");
    // File name format: setID_descID.vktexturemetadata

    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d",   width, 
                                                height, 
                                                format, 
                                                VkDescriptorTypeNum, 
                                                image->n_planes, 
                                                image->samples, 
                                                tiling, 
                                                isl_tiling_mode,
                                                row_pitch_B);
    fclose(fp);
#elif defined(MESA_USE_LVPIPE_DRIVER)
    printf("gpgpusim: dumpStorageImage not implemented for lavapipe.\n");
    abort();

#endif
}


void VulkanRayTracing::dump_descriptor_set_for_AS(uint32_t setID, uint32_t descID, void *address, uint32_t desc_size, VkDescriptorType type, uint32_t backwards_range, uint32_t forward_range, bool split_files, VkAccelerationStructureKHR _topLevelAS)
{
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    char *filePath = "gpgpusimShaders/";
    char *extension = ".vkdescrptorsetdata";

    int VkDescriptorTypeNum;

    switch (type)
    {
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            VkDescriptorTypeNum = 1000150000;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            VkDescriptorTypeNum = 1000165000;
            break;
        default:
            abort(); // should not be here!
    }

    char fullPath[200];
    int result;

    int64_t max_backwards; // negative number
    int64_t min_backwards; // negative number
    int64_t min_forwards;
    int64_t max_forwards;
    int64_t back_buffer_amount = 0; //20kB buffer just in case
    int64_t front_buffer_amount = 1024*20; //20kB buffer just in case
    findOffsetBounds(max_backwards, min_backwards, min_forwards, max_forwards, _topLevelAS);

    bool haveBackwards = (max_backwards != 0) && (min_backwards != 0);
    bool haveForwards = (min_forwards != 0) && (max_forwards != 0);
    
    if (split_files) // Used when the AS is too far apart between top tree and BVHAddress and cant just dump the whole thing
    {
        // Main Top Level
        snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.asmain", mesa_root, filePath, setID, descID);
        fp = fopen(fullPath, "wb+");
        result = fwrite(address, 1, desc_size, fp);
        assert(result == desc_size);
        fclose(fp);

        // Bot level whose address is smaller than top level
        if (haveBackwards)
        {
            snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.asback", mesa_root, filePath, setID, descID);
            fp = fopen(fullPath, "wb+");
            result = fwrite(address + max_backwards, 1, min_backwards - max_backwards + back_buffer_amount, fp);
            assert(result == min_backwards - max_backwards + back_buffer_amount);
            fclose(fp);
        }

        // Bot level whose address is larger than top level
        if (haveForwards)
        {
            snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.asfront", mesa_root, filePath, setID, descID);
            fp = fopen(fullPath, "wb+");
            result = fwrite(address + min_forwards, 1, max_forwards - min_forwards + front_buffer_amount, fp);
            assert(result == max_forwards - min_forwards + front_buffer_amount);
            fclose(fp);
        }

        // AS metadata
        snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d.asmetadata", mesa_root, filePath, setID, descID);
        fp = fopen(fullPath, "w+");
        fprintf(fp, "%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d", desc_size,
                                                            VkDescriptorTypeNum,
                                                            max_backwards,
                                                            min_backwards,
                                                            min_forwards,
                                                            max_forwards,
                                                            back_buffer_amount,
                                                            front_buffer_amount,
                                                            haveBackwards,
                                                            haveForwards);
        fclose(fp);

        
        // uint64_t total_size = (desc_size + backwards_range + forward_range);
        // uint64_t chunk_size = 1024*1024*20; // 20MB chunks
        // int totalFiles =  (total_size + chunk_size) / chunk_size; // rounds up

        // for (int i = 0; i < totalFiles; i++)
        // {
        //     // if split_files is 1, then look at the next number to see what the file part number is
        //     snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d_%d_%d_%d_%d_%d_%d%s", mesa_root, filePath, setID, descID, desc_size, VkDescriptorTypeNum, backwards_range, forward_range, split_files, i, extension);
        //     fp = fopen(fullPath, "wb+");
        //     int result = fwrite(address-(uint64_t)backwards_range + chunk_size * i, 1, chunk_size, fp);
        //     printf("File part %d, %d bytes written, starting address 0x%.12" PRIXPTR "\n", i, result, (uintptr_t)(address-(uint64_t)backwards_range + chunk_size * i));
        //     fclose(fp);
        // }
    }
    else 
    {
        snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d_%d_%d_%d_%d%s", mesa_root, filePath, setID, descID, desc_size, VkDescriptorTypeNum, backwards_range, forward_range, extension);
        // File name format: setID_descID_SizeInBytes_VkDescriptorType_desired_range.vkdescrptorsetdata

        fp = fopen(fullPath, "wb+");
        int result = fwrite(address-(uint64_t)backwards_range, 1, desc_size + backwards_range + forward_range, fp);
        fclose(fp);
    }
}


void VulkanRayTracing::dump_descriptor_set(uint32_t setID, uint32_t descID, void *address, uint32_t size, VkDescriptorType type)
{
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    char *filePath = "gpgpusimShaders/";
    char *extension = ".vkdescrptorsetdata";

    int VkDescriptorTypeNum;

    switch (type)
    {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            VkDescriptorTypeNum = 0;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            VkDescriptorTypeNum = 1;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            VkDescriptorTypeNum = 2;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            VkDescriptorTypeNum = 3;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            VkDescriptorTypeNum = 4;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            VkDescriptorTypeNum = 5;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            VkDescriptorTypeNum = 6;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            VkDescriptorTypeNum = 7;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            VkDescriptorTypeNum = 8;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            VkDescriptorTypeNum = 9;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            VkDescriptorTypeNum = 10;
            break;
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            VkDescriptorTypeNum = 1000138000;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            VkDescriptorTypeNum = 1000150000;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            VkDescriptorTypeNum = 1000165000;
            break;
        case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
            VkDescriptorTypeNum = 1000351000;
            break;
        case VK_DESCRIPTOR_TYPE_MAX_ENUM:
            VkDescriptorTypeNum = 0x7FFFFFF;
            break;
        default:
            abort(); // should not be here!
    }

    char fullPath[200];
    snprintf(fullPath, sizeof(fullPath), "%s%s%d_%d_%d_%d%s", mesa_root, filePath, setID, descID, size, VkDescriptorTypeNum, extension);
    // File name format: setID_descID_SizeInBytes_VkDescriptorType.vkdescrptorsetdata

    fp = fopen(fullPath, "wb+");
    fwrite(address, 1, size, fp);
    fclose(fp);
}


void VulkanRayTracing::dump_descriptor_sets(struct DESCRIPTOR_SET_STRUCT *set)
{
#if defined(MESA_USE_INTEL_DRIVER)
   for(int i = 0; i < set->descriptor_count; i++)
   {
       if(i == 3 || i > 9)
       {
            // for some reason raytracing_extended skipped binding = 3
            // and somehow they have 34 descriptor sets but only 10 are used
            // so we just skip those
            continue;
       }

        struct DESCRIPTOR_SET_STRUCT* set = VulkanRayTracing::descriptorSet;

        const struct DESCRIPTOR_LAYOUT_STRUCT *bind_layout = &set->layout->binding[i];
        struct DESCRIPTOR_STRUCT *desc = &set->descriptors[bind_layout->descriptor_index];
        void *desc_map = set->desc_mem.map + bind_layout->descriptor_offset;

        assert(desc->type == bind_layout->type);

        switch (desc->type)
        {
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                //return (void *)(desc);
                dumpStorageImage(desc, 0, i, desc->type);
                break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
                //return desc;
                dumpTextures(desc, 0, i, desc->type);
                break;
            }

            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                assert(0);
                break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
                if (desc->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    desc->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                {
                    // MRS_TODO: account for desc->offset?
                    //return anv_address_map(desc->buffer->address);
                    dump_descriptor_set(0, i, anv_address_map(desc->buffer->address), set->descriptors[i].buffer->size, set->descriptors[i].type);
                    break;
                }
                else
                {
                    struct anv_buffer_view *bview = &set->buffer_views[bind_layout->buffer_view_index];
                    //return anv_address_map(bview->address);
                    dump_descriptor_set(0, i, anv_address_map(bview->address), bview->range, set->descriptors[i].type);
                    break;
                }
            }

            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                assert(0);
                break;

            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            {
                struct anv_address_range_descriptor *desc_data = desc_map;
                //return (void *)(desc_data->address);
                //dump_descriptor_set_for_AS(0, i, (void *)(desc_data->address), desc_data->range, set->descriptors[i].type, 1024*1024*10, 1024*1024*10, true);
                break;
            }

            default:
                assert(0);
                break;
        }
   }
#elif defined(MESA_USE_LVPIPE_DRIVER)
    printf("gpgpusim: dump_descriptor_sets not implemented for lavapipe.\n");
    abort();

#endif
}

void VulkanRayTracing::dump_AS(struct DESCRIPTOR_SET_STRUCT *set, VkAccelerationStructureKHR _topLevelAS)
{
#if defined(MESA_USE_INTEL_DRIVER)
   for(int i = 0; i < set->descriptor_count; i++)
   {
       if(i == 3 || i > 9)
       {
            // for some reason raytracing_extended skipped binding = 3
            // and somehow they have 34 descriptor sets but only 10 are used
            // so we just skip those
            continue;
       }

        struct DESCRIPTOR_SET_STRUCT* set = VulkanRayTracing::descriptorSet;

        const struct DESCRIPTOR_LAYOUT_STRUCT *bind_layout = &set->layout->binding[i];
        struct DESCRIPTOR_STRUCT *desc = &set->descriptors[bind_layout->descriptor_index];
        void *desc_map = set->desc_mem.map + bind_layout->descriptor_offset;

        assert(desc->type == bind_layout->type);

        switch (desc->type)
        {
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            {
                struct anv_address_range_descriptor *desc_data = desc_map;
                //return (void *)(desc_data->address);
                dump_descriptor_set_for_AS(0, i, (void *)(desc_data->address), desc_data->range, set->descriptors[i].type, 1024*1024*10, 1024*1024*10, true, _topLevelAS);
                break;
            }

            default:
                break;
        }
    }
#elif defined(MESA_USE_LVPIPE_DRIVER)
    printf("gpgpusim: dump_AS not implemented for lavapipe.\n");
    abort();

#endif
}

void VulkanRayTracing::dump_callparams_and_sbt(void *raygen_sbt, void *miss_sbt, void *hit_sbt, void *callable_sbt, bool is_indirect, uint32_t launch_width, uint32_t launch_height, uint32_t launch_depth, uint32_t launch_size_addr)
{
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    char *filePath = "gpgpusimShaders/";

    char call_params_filename [200];
    int trace_rays_call_count = 0; // just a placeholder for now
    snprintf(call_params_filename, sizeof(call_params_filename), "%s%s%d.callparams", mesa_root, filePath, trace_rays_call_count);
    fp = fopen(call_params_filename, "w+");
    fprintf(fp, "%d,%d,%d,%d,%lu", is_indirect, launch_width, launch_height, launch_depth, launch_size_addr);
    fclose(fp);

    // TODO: Is the size always 32?
    int sbt_size = 64 *sizeof(uint64_t);
    if (raygen_sbt) {
        char raygen_sbt_filename [200];
        snprintf(raygen_sbt_filename, sizeof(raygen_sbt_filename), "%s%s%d.raygensbt", mesa_root, filePath, trace_rays_call_count);
        fp = fopen(raygen_sbt_filename, "wb+");
        fwrite(raygen_sbt, 1, sbt_size, fp); // max is 32 bytes according to struct anv_rt_shader_group.handle
        fclose(fp);
    }

    if (miss_sbt) {
        char miss_sbt_filename [200];
        snprintf(miss_sbt_filename, sizeof(miss_sbt_filename), "%s%s%d.misssbt", mesa_root, filePath, trace_rays_call_count);
        fp = fopen(miss_sbt_filename, "wb+");
        fwrite(miss_sbt, 1, sbt_size, fp); // max is 32 bytes according to struct anv_rt_shader_group.handle
        fclose(fp);
    }

    if (hit_sbt) {
        char hit_sbt_filename [200];
        snprintf(hit_sbt_filename, sizeof(hit_sbt_filename), "%s%s%d.hitsbt", mesa_root, filePath, trace_rays_call_count);
        fp = fopen(hit_sbt_filename, "wb+");
        fwrite(hit_sbt, 1, sbt_size, fp); // max is 32 bytes according to struct anv_rt_shader_group.handle
        fclose(fp);
    }

    if (callable_sbt) {
        char callable_sbt_filename [200];
        snprintf(callable_sbt_filename, sizeof(callable_sbt_filename), "%s%s%d.callablesbt", mesa_root, filePath, trace_rays_call_count);
        fp = fopen(callable_sbt_filename, "wb+");
        fwrite(callable_sbt, 1, sbt_size, fp); // max is 32 bytes according to struct anv_rt_shader_group.handle
        fclose(fp);
    }
}

void VulkanRayTracing::setStorageImageFromLauncher(void *address, 
                                                void *deviceAddress, 
                                                uint32_t setID, 
                                                uint32_t descID, 
                                                uint32_t width,
                                                uint32_t height,
                                                VkFormat format,
                                                uint32_t VkDescriptorTypeNum,
                                                uint32_t n_planes,
                                                uint32_t n_samples,
                                                VkImageTiling tiling,
                                                uint32_t isl_tiling_mode, 
                                                uint32_t row_pitch_B)
{
    storage_image_metadata *storage_image = new storage_image_metadata;
    storage_image->address = address;
    storage_image->setID = setID;
    storage_image->descID = descID;
    storage_image->width = width;
    storage_image->height = height;
    storage_image->format = format;
    storage_image->VkDescriptorTypeNum = VkDescriptorTypeNum;
    storage_image->n_planes = n_planes;
    storage_image->n_samples = n_samples;
    storage_image->tiling = tiling;
    storage_image->isl_tiling_mode = isl_tiling_mode; 
    storage_image->row_pitch_B = row_pitch_B;
    storage_image->deviceAddress = deviceAddress;

    launcher_descriptorSets[setID][descID] = (void*) storage_image;
    launcher_deviceDescriptorSets[setID][descID] = (void*) storage_image;
}

void VulkanRayTracing::setTextureFromLauncher(void *address, 
                                            void *deviceAddress, 
                                            uint32_t setID, 
                                            uint32_t descID, 
                                            uint64_t size,
                                            uint32_t width,
                                            uint32_t height,
                                            VkFormat format,
                                            uint32_t VkDescriptorTypeNum,
                                            uint32_t n_planes,
                                            uint32_t n_samples,
                                            VkImageTiling tiling,
                                            uint32_t isl_tiling_mode,
                                            uint32_t row_pitch_B,
                                            uint32_t filter)
{
    texture_metadata *texture = new texture_metadata;
    texture->address = address;
    texture->setID = setID;
    texture->descID = descID;
    texture->size = size;
    texture->width = width;
    texture->height = height;
    texture->format = format;
    texture->VkDescriptorTypeNum = VkDescriptorTypeNum;
    texture->n_planes = n_planes;
    texture->n_samples = n_samples;
    texture->tiling = tiling;
    texture->isl_tiling_mode = isl_tiling_mode;
    texture->row_pitch_B = row_pitch_B;
    texture->filter = filter;
    texture->deviceAddress = deviceAddress;

    launcher_descriptorSets[setID][descID] = (void*) texture;
    launcher_deviceDescriptorSets[setID][descID] = (void*) texture;
}

void VulkanRayTracing::pass_child_addr(void *address)
{
    child_addrs_from_driver.push_back(address);
}

void VulkanRayTracing::allocBLAS(void* rootAddr, uint64_t bufferSize, void* gpgpusimAddr) {
    printf("gpgpusim: set BLAS address for 0x%lx at %p to %p\n", bufferSize, rootAddr, gpgpusimAddr);
    blas_addr_map[rootAddr] = gpgpusimAddr;
}

void VulkanRayTracing::allocTLAS(void* rootAddr, uint64_t bufferSize, void* gpgpusimAddr) {
    printf("gpgpusim: set TLAS address %p to %p\n", rootAddr, gpgpusimAddr);
    tlas_addr = gpgpusimAddr;
}

void VulkanRayTracing::findOffsetBounds(int64_t &max_backwards, int64_t &min_backwards, int64_t &min_forwards, int64_t &max_forwards, VkAccelerationStructureKHR _topLevelAS)
{
    // uint64_t current_min_backwards = 0;
    // uint64_t current_max_backwards = 0;
    // uint64_t current_min_forwards = 0;
    // uint64_t current_max_forwards = 0;
    int64_t offset;

    std::vector<int64_t> positive_offsets;
    std::vector<int64_t> negative_offsets;

    for (auto addr : child_addrs_from_driver)
    {
        offset = (uint64_t)addr - (uint64_t)_topLevelAS;
        if (offset >= 0)
            positive_offsets.push_back(offset);
        else
            negative_offsets.push_back(offset);
    }

    sort(positive_offsets.begin(), positive_offsets.end());
    sort(negative_offsets.begin(), negative_offsets.end());

    if (negative_offsets.size() > 0)
    {
        max_backwards = negative_offsets.front();
        min_backwards = negative_offsets.back();
    }
    else
    {
        max_backwards = 0;
        min_backwards = 0;
    }

    if (positive_offsets.size() > 0)
    {
        min_forwards = positive_offsets.front();
        max_forwards = positive_offsets.back();
    }
    else
    {
        min_forwards = 0;
        max_forwards = 0;
    }
}


void* VulkanRayTracing::gpgpusim_alloc(uint32_t size)
{
    gpgpu_context *ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);
    void* devPtr = context->get_device()->get_gpgpu()->gpu_malloc(size);
    if (g_debug_execution >= 3) {
        printf("GPGPU-Sim PTX: gpgpusim_allocing %zu bytes starting at 0x%llx..\n",
            size, (unsigned long long)devPtr);
        ctx->api->g_mallocPtr_Size[(unsigned long long)devPtr] = size;
    }
    assert(devPtr);

    if(!use_external_launcher) {
        void* bufferAddr = malloc(size);
        memory_space *mem = context->get_device()->get_gpgpu()->get_global_memory();
        mem->bind_vulkan_buffer(bufferAddr, size, devPtr);
    }

    return devPtr;
}

void* VulkanRayTracing::allocBuffer(void* bufferAddr, uint64_t bufferSize)
{
    gpgpu_context *ctx = GPGPU_Context();
    CUctx_st *context = GPGPUSim_Context(ctx);
    void* devPtr = context->get_device()->get_gpgpu()->gpu_malloc(bufferSize);
    assert(devPtr);

    memory_space *mem = context->get_device()->get_gpgpu()->get_global_memory();
    
    printf("gpgpusim: binding gpgpusim buffer %p (size %d) to vulkan buffer %p\n", devPtr, bufferSize, bufferAddr);
    mem->bind_vulkan_buffer(bufferAddr, bufferSize, devPtr);
    return devPtr;
}

