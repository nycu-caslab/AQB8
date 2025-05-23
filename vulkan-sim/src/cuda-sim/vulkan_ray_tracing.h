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

#ifndef VULKAN_RAY_TRACING_H
#define VULKAN_RAY_TRACING_H

#include "vulkan/vulkan.h"
#if defined(MESA_USE_INTEL_DRIVER)
#include "vulkan/anv_acceleration_structure.h"
#include "vulkan/vulkan_intel.h"

#elif defined(MESA_USE_LVPIPE_DRIVER)
#include "lvp_acceleration_structure.h"
#endif

#include "compiler/spirv/spirv.h"
#include "intersection_table.h"

// #include "ptx_ir.h"
#include <cmath>
#include <fstream>
#include "../../libcuda/gpgpu_context.h"
#include "../abstract_hardware_model.h"
#include "compiler/shader_enums.h"
#include "ptx_ir.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MIN_MAX(a, b, c) MAX(MIN((a), (b)), (c))
#define MAX_MIN(a, b, c) MIN(MAX((a), (b)), (c))

#define MAX_DESCRIPTOR_SETS 1
#define MAX_DESCRIPTOR_SET_BINDINGS 32

// enum class TransactionType {
//     BVH_STRUCTURE,
//     BVH_INTERNAL_NODE,
//     BVH_INSTANCE_LEAF,
//     BVH_PRIMITIVE_LEAF_DESCRIPTOR,
//     BVH_QUAD_LEAF,
//     BVH_PROCEDURAL_LEAF,
//     Intersection_Table_Load,
// };

// typedef struct MemoryTransactionRecord {
//     MemoryTransactionRecord(void* address, uint32_t size, TransactionType
//     type) : address(address), size(size), type(type) {} void* address;
//     uint32_t size;
//     TransactionType type;
// } MemoryTransactionRecord;
// typedef struct float4 {
//     float x, y, z, w;
// } float4;

// enum class StoreTransactionType {
//     Intersection_Table_Store,
//     Traversal_Results,
// };

// typedef struct MemoryStoreTransactionRecord {
//     MemoryStoreTransactionRecord(void* address, uint32_t size,
//     StoreTransactionType type) : address(address), size(size), type(type) {}
//     void* address;
//     uint32_t size;
//     StoreTransactionType type;
// } MemoryStoreTransactionRecord;

extern bool use_external_launcher;

typedef struct float4x4
{
    float m[4][4];

    float4 operator*(const float4 &_vec) const
    {
        float vec[] = {_vec.x, _vec.y, _vec.z, _vec.w};
        float res[] = {0, 0, 0, 0};
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                res[i] += this->m[j][i] * vec[j];
        return {res[0], res[1], res[2], res[3]};
    }
} float4x4;

typedef struct RayDebugGPUData
{
    bool valid;
    int launchIDx;
    int launchIDy;
    int instanceCustomIndex;
    int primitiveID;
    float3 v0pos;
    float3 v1pos;
    float3 v2pos;
    float3 attribs;
    float3 P_object;
    float3 P; // world intersection point
    float3 N_object;
    float3 N;
    float NdotL;
    float3 hitValue;
} RayDebugGPUData;

// float4 operator*(const float4& _vec, const float4x4& matrix)
// {
//     float vec[] = {_vec.x, _vec.y, _vec.z, _vec.w};
//     float res[] = {0, 0, 0, 0};
//     for(int i = 0; i < 4; i++)
//         for(int j = 0; j < 4; j++)
//             res[i] += matrix.m[j][i] * vec[j];
//     return {res[0], res[1], res[2], res[3]};
// }

typedef struct Descriptor
{
    uint32_t setID;
    uint32_t descID;
    void *address;
    uint32_t size;
    VkDescriptorType type;
} Descriptor;

typedef struct shader_stage_info
{
    uint32_t ID;
    gl_shader_stage type;
    char *function_name;
} shader_stage_info;

typedef struct Pixel
{
    Pixel(float c0, float c1, float c2, float c3)
        : c0(c0), c1(c1), c2(c2), c3(c3) {}
    Pixel() {}

    union
    {
        float r;
        float c0;
    };
    union
    {
        float g;
        float c1;
    };
    union
    {
        float b;
        float c2;
    };
    union
    {
        float a;
        float c3;
    };
} Pixel;

// For launcher
typedef struct storage_image_metadata
{
    void *address;
    void *deviceAddress;
    uint32_t setID;
    uint32_t descID;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    uint32_t VkDescriptorTypeNum;
    uint32_t n_planes;
    uint32_t n_samples;
    VkImageTiling tiling;
    uint32_t isl_tiling_mode;
    uint32_t row_pitch_B;
} storage_image_metadata;

typedef struct texture_metadata
{
    void *address;
    void *deviceAddress;
    uint32_t setID;
    uint32_t descID;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    uint32_t VkDescriptorTypeNum;
    uint32_t n_planes;
    uint32_t n_samples;
    VkImageTiling tiling;
    uint32_t isl_tiling_mode;
    uint32_t row_pitch_B;
    VkFilter filter;
} texture_metadata;

#if defined(MESA_USE_INTEL_DRIVER)
#define DESCRIPTOR_SET_STRUCT anv_descriptor_set
#define DESCRIPTOR_STRUCT anv_descriptor
#define DESCRIPTOR_LAYOUT_STRUCT anv_descriptor_set_binding_layout

#define VSIM_DEBUG_PRINT 0
struct anv_descriptor_set;
struct anv_descriptor;

#elif defined(MESA_USE_LVPIPE_DRIVER)
#define DESCRIPTOR_SET_STRUCT lvp_descriptor_set
#define DESCRIPTOR_STRUCT lvp_descriptor
#define DESCRIPTOR_LAYOUT_STRUCT lvp_descriptor_set_binding_layout

struct lvp_descriptor_set;
struct lvp_descriptor;

#endif

#define VSIM_DPRINTF(...)    \
    if (VSIM_DEBUG_PRINT)    \
    {                        \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    }

#include "bvh/int_bvh.hpp"
#include "bvh/int_traverse.hpp"

using namespace bvh_quantize;

class VulkanRayTracing
{
private:
    static VkRayTracingPipelineCreateInfoKHR *pCreateInfos;
    static VkAccelerationStructureGeometryKHR *pGeometries;
    static uint32_t geometryCount;
    static VkAccelerationStructureKHR topLevelAS;
    static std::vector<std::vector<Descriptor>> descriptors;
    static std::ofstream imageFile;
    static bool firstTime;
    static struct DESCRIPTOR_SET_STRUCT *descriptorSet;

    // For Launcher
    static void *launcher_descriptorSets[MAX_DESCRIPTOR_SETS]
                                        [MAX_DESCRIPTOR_SET_BINDINGS];
    static void *launcher_deviceDescriptorSets[MAX_DESCRIPTOR_SETS]
                                              [MAX_DESCRIPTOR_SET_BINDINGS];
    static std::vector<void *> child_addrs_from_driver;
    static std::map<void *, void *> VulkanRayTracing::blas_addr_map;

    static void *tlas_addr;

    static void *int_bvh_addr;
    static void *int_bvh_clusters_addr;
    static void *int_bvh_trigs_addr;
    static void *int_bvh_nodes_addr;
    static void *int_bvh_primitive_indices_addr;

    static int_bvh_t int_bvh;

    static bool dumped;
    static bool _init_;

public:
    // static RayDebugGPUData rayDebugGPUData[2000][2000];
    static warp_intersection_table ***intersection_table;
    static warp_intersection_table ***anyhit_table;
    static IntersectionTableType intersectionTableType;

private:
    static bool mt_ray_triangle_test(float3 p0, float3 p1, float3 p2,
                                     Ray ray_properties, float *thit);
    static float3 Barycentric(float3 p, float3 a, float3 b, float3 c);
    static std::vector<shader_stage_info> shaders;

    static void init(uint32_t launch_width, uint32_t launch_height);

public:
    static int32_t floor_to_int32(float x);
    static int32_t ceil_to_int32(float x);
    static int_w_t get_int_w(const std::array<float, 3> &w);
    static decoded_data_t decode_data(uint16_t data);

    static std::pair<bool, float> intersect_bbox(const std::array<bool, 3> &octant,
                                                 const std::array<float, 3> &w,
                                                 const float *x,
                                                 const std::array<float, 3> &b,
                                                 float tmax);
    static std::pair<bool, int32_t> intersect_int_bbox(int32_t qy_max,
                                                       const int_w_t &int_w,
                                                       const uint8_t *qx,
                                                       const int32_t *qb_l,
                                                       const int32_t *qb_h);
    static std::pair<bool, float> intersect_trig(int_trig_t *trigs, const Ray &ray);

    static void traceRay( // called by raygen shader
        VkAccelerationStructureKHR _topLevelAS, uint rayFlags, uint cullMask,
        uint sbtRecordOffset, uint sbtRecordStride, uint missIndex, float3 origin,
        float Tmin, float3 direction, float Tmax, int payload,
        const ptx_instruction *pI, ptx_thread_info *thread);

    static void endTraceRay(const ptx_instruction *pI, ptx_thread_info *thread);

    static void load_descriptor(const ptx_instruction *pI,
                                ptx_thread_info *thread);

    static void setPipelineInfo(VkRayTracingPipelineCreateInfoKHR *pCreateInfos);
    static void setGeometries(VkAccelerationStructureGeometryKHR *pGeometries,
                              uint32_t geometryCount);
    static void setAccelerationStructure(
        VkAccelerationStructureKHR accelerationStructure);
    static void setDescriptorSet(struct DESCRIPTOR_SET_STRUCT *set);

    static void iterateDescriptorSet(struct DESCRIPTOR_SET_STRUCT *set);

    static void invoke_gpgpusim();
    static uint32_t registerShaders(char *shaderPath, gl_shader_stage shaderType);

    static void vkCmdTraceRaysKHR( // called by vulkan application
        void *raygen_sbt, void *miss_sbt, void *hit_sbt, void *callable_sbt,
        bool is_indirect, uint32_t launch_width, uint32_t launch_height,
        uint32_t launch_depth, uint64_t launch_size_addr);
    static void callShader(const ptx_instruction *pI, ptx_thread_info *thread,
                           function_info *target_func);
    static void callMissShader(const ptx_instruction *pI,
                               ptx_thread_info *thread);
    static void callClosestHitShader(const ptx_instruction *pI,
                                     ptx_thread_info *thread);
    static void callIntersectionShader(const ptx_instruction *pI,
                                       ptx_thread_info *thread,
                                       uint32_t shader_counter);
    static void callAnyHitShader(const ptx_instruction *pI,
                                 ptx_thread_info *thread,
                                 uint32_t shader_counter);
    static void setDescriptor(uint32_t setID, uint32_t descID, void *address,
                              uint32_t size, VkDescriptorType type);
    static void *getDescriptorAddress(uint32_t setID, uint32_t binding);

    static void image_store(struct DESCRIPTOR_STRUCT *desc,
                            uint32_t gl_LaunchIDEXT_X, uint32_t gl_LaunchIDEXT_Y,
                            uint32_t gl_LaunchIDEXT_Z, uint32_t gl_LaunchsIDEXT_W,
                            float hitValue_X, float hitValue_Y, float hitValue_Z,
                            float hitValue_W, const ptx_instruction *pI,
                            ptx_thread_info *thread);
    static void getTexture(
        struct DESCRIPTOR_STRUCT *desc, float x, float y, float lod, float &c0,
        float &c1, float &c2, float &c3,
        std::vector<ImageMemoryTransactionRecord> &transactions,
        uint64_t launcher_offset = 0);
    static void image_load(struct DESCRIPTOR_STRUCT *desc, uint32_t x, uint32_t y,
                           float &c0, float &c1, float &c2, float &c3);

    static void dump_descriptor_set(uint32_t setID, uint32_t descID,
                                    void *address, uint32_t size,
                                    VkDescriptorType type);
    static void dump_descriptor_set_for_AS(
        uint32_t setID, uint32_t descID, void *address, uint32_t desc_size,
        VkDescriptorType type, uint32_t backwards_range, uint32_t forward_range,
        bool split_files, VkAccelerationStructureKHR _topLevelAS);
    static void dump_descriptor_sets(struct DESCRIPTOR_SET_STRUCT *set);
    static void dump_AS(struct DESCRIPTOR_SET_STRUCT *set,
                        VkAccelerationStructureKHR _topLevelAS);
    static void dump_callparams_and_sbt(void *raygen_sbt, void *miss_sbt,
                                        void *hit_sbt, void *callable_sbt,
                                        bool is_indirect, uint32_t launch_width,
                                        uint32_t launch_height,
                                        uint32_t launch_depth,
                                        uint32_t launch_size_addr);
    static void dumpTextures(struct DESCRIPTOR_STRUCT *desc, uint32_t setID,
                             uint32_t binding, VkDescriptorType type);
    static void dumpStorageImage(struct DESCRIPTOR_STRUCT *desc, uint32_t setID,
                                 uint32_t binding, VkDescriptorType type);
    static void setDescriptorSetFromLauncher(void *address, void *deviceAddress,
                                             uint32_t setID, uint32_t descID);
    static void setStorageImageFromLauncher(
        void *address, void *deviceAddress, uint32_t setID, uint32_t descID,
        uint32_t width, uint32_t height, VkFormat format,
        uint32_t VkDescriptorTypeNum, uint32_t n_planes, uint32_t n_samples,
        VkImageTiling tiling, uint32_t isl_tiling_mode, uint32_t row_pitch_B);
    static void setTextureFromLauncher(
        void *address, void *deviceAddress, uint32_t setID, uint32_t descID,
        uint64_t size, uint32_t width, uint32_t height, VkFormat format,
        uint32_t VkDescriptorTypeNum, uint32_t n_planes, uint32_t n_samples,
        VkImageTiling tiling, uint32_t isl_tiling_mode, uint32_t row_pitch_B,
        uint32_t filter);
    static void pass_child_addr(void *address);

    static void allocBLAS(void *rootAddr, uint64_t bufferSize,
                          void *gpgpusimAddr);
    static void allocTLAS(void *rootAddr, uint64_t bufferSize,
                          void *gpgpusimAddr);
    static void *allocBuffer(void *bufferAddr, uint64_t bufferSize);
    static void findOffsetBounds(int64_t &max_backwards, int64_t &min_backwards,
                                 int64_t &min_forwards, int64_t &max_forwards,
                                 VkAccelerationStructureKHR _topLevelAS);
    static void *gpgpusim_alloc(uint32_t size);
};

#endif /* VULKAN_RAY_TRACING_H */
