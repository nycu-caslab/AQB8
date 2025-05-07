/*
 * Copyright © 2020 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "lvp_private.h"
#include "lvp_acceleration_structure.h"
#include "vsim_private.h"
#include "gen_rt_pack.h"

#include <math.h>

#include "../util/u_debug.h"
#include "../util/half_float.h"
#include "../util/u_atomic.h"

// #include "genxml/gen_rt_pack.h"
// #include "../intel/vulkan/anv_private.h"
// #include "anv_private.h"
// #include "../intel/genxml/gen125_rt_pack.h"
#include "gpgpusim_bvh.h"

#include "gpgpusim_calls_from_mesa.h"

VK_DEFINE_NONDISP_HANDLE_CASTS(lvp_acceleration_structure, base,
                               VkAccelerationStructureKHR,
                               VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR)

void print_vec4(struct vsim_bvh_vec4f val)
{
   printf("(%5.3f, %5.3f, %5.3f, %5.3f)", val.v[0], val.v[1], val.v[2], val.v[3]);
}
void print_vec3(struct vsim_bvh_vec3f val)
{
   printf("(%5.3f, %5.3f, %5.3f)", val.v[0], val.v[1], val.v[2]);
}
void print_tri(struct vsim_bvh_triangle val)
{
   printf("Triangle: \t");
   print_vec3(val.v[0]);
   print_vec3(val.v[1]);
   print_vec3(val.v[2]);
   printf("\n");
}
void print_node(struct vsim_bvh_node val)
{
   printf("Compressed Node: %c\t", val.is_leaf ? 'l' : 'i');
   printf("exp: (%d, %d, %d)\t", val.exp_x, val.exp_y, val.exp_z);
   printf("origin: (%f, %f, %f)\n", val.origin_x, val.origin_y, val.origin_z);
   printf("Children:\n");
   for (unsigned i = 0; i < 6; i++)
   {
      printf("\t%p: (%d, %d, %d), (%d, %d, %d)\n", val.children[i],
             val.lower_x[i], val.lower_y[i], val.lower_z[i],
             val.upper_x[i], val.upper_y[i], val.upper_z[i]);
   }
}
void print_leaf(struct vsim_bvh_leaf val)
{
   printf("Leaf: %c\t", val.is_leaf ? 'y' : 'n');
   printf("geomID: %d\t primID: %d\n", val.geometry_id, val.primitive_id);
}

void embree_error_function(void *userPtr, enum RTCError error, const char *str)
{
   printf("EMBREE: Error %d: %s\n", error, str);
}

static uint64_t
lvp_bvh_max_size(VkAccelerationStructureTypeKHR type, uint64_t leaf_count)
{
   uint64_t max_size_B = ALIGN(GEN_RT_BVH_length * 4, 256);

   /* Assuming a 2-ary tree in which no node has only one child, the number of
    * internal nodes is always going to be leaf_count - 1 (this is an easy
    * inductive proof).  While we can do somewhat better with the 6-ary trees
    * that make up our BVH, Embree doesn't guarantee that all internal nodes
    * are full and so it's a bit harder to calculate exactly.  Assuming no
    * internal node is left with exactly one leaf (that would be silly), we
    * can use the 2-ary tree node count as an upper bound.  Due to
    * restrictions on the BVH data structure, we always have at least one
    * internal node.
    */
   uint64_t internal_node_count = MAX2(2, leaf_count) - 1;
   max_size_B += internal_node_count * GEN_RT_BVH_INTERNAL_NODE_length * 4;

   switch (type)
   {
   case VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR:
      max_size_B += leaf_count * GEN_RT_BVH_INSTANCE_LEAF_length * 4;
      break;

   case VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR:
      max_size_B += leaf_count * GEN_RT_BVH_QUAD_LEAF_length * 4;
      break;

   case VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR:
      unreachable("VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR is not allowed "
                  "in vkGetAccelerationStructureBuildSizesKHR");

   default:
      unreachable("Invalid VkAccelerationStructureTypeKHR");
   }

   printf("LVP: Size of BVH structure is 0x%lx\n", max_size_B);
   return max_size_B;
}

struct lvp_bvh_build_geometry_state
{
   const VkAccelerationStructureGeometryKHR *pGeometry;
   struct vsim_bvh_triangle *triangles;
   const void *instance_data;
};

struct lvp_bvh_build_state
{
   struct lvp_device *device;
   bool is_host_build;

   struct lvp_bvh_build_geometry_state *geometries;

   void *nodes_map;
   void *leaves_map;
};

// static struct lvp_bo *
// find_bo_for_address(struct lvp_device *device,
//                     uint64_t addr)
// {
//    // pthread_mutex_lock(&device->mutex);

//    struct lvp_bo *bo = NULL;
//    list_for_each_entry(struct lvp_device_memory, mem,
//                        &device->memory_objects, link) {
//       if (!(mem->bo->flags & EXEC_OBJECT_PINNED))
//          continue;

//       if (mem->bo->offset <= addr &&
//           addr < mem->bo->offset + mem->bo->size) {
//          bo = mem->bo;
//          break;
//       }
//    }

//    // pthread_mutex_unlock(&device->mutex);

//    return bo;
// }

// static void *
// doha_to_host(VkDeviceOrHostAddressKHR addr,
//              struct lvp_bvh_build_state *state)
// {
//    if (likely(state->is_host_build))
//       return addr.hostAddress;

//    struct lvp_bo *bo = find_bo_for_address(state->device, addr.deviceAddress);
//    if (bo == NULL)
//       return NULL;

//    return bo->map + (addr.deviceAddress - bo->offset);
// }

// static const void *
// doha_to_host_const(VkDeviceOrHostAddressConstKHR const_addr,
//                    struct lvp_bvh_build_state *state)
// {
//    VkDeviceOrHostAddressKHR addr;
//    memcpy(&addr, &const_addr, sizeof(addr));
//    return doha_to_host(addr, state);
// }

#define doha_to_host(addr, state) ((addr).hostAddress)
#define doha_to_host_const(addr, state) ((addr).hostAddress)

void lvp_GetAccelerationStructureBuildSizesKHR(
    VkDevice device,
    VkAccelerationStructureBuildTypeKHR buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo,
    const uint32_t *pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
   printf("lvp_GetAccelerationStructureBuildSizesKHR\n");
   assert(pSizeInfo->sType ==
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);

   uint64_t max_prim_count = 0;
   for (uint32_t i = 0; i < pBuildInfo->geometryCount; i++)
      max_prim_count += pMaxPrimitiveCounts[i];

   pSizeInfo->accelerationStructureSize =
       lvp_bvh_max_size(pBuildInfo->type, max_prim_count);

   uint64_t cpu_build_scratch_size = 0;
   cpu_build_scratch_size += pBuildInfo->geometryCount *
                             sizeof(struct lvp_bvh_build_geometry_state);
   cpu_build_scratch_size += max_prim_count * sizeof(struct RTCBuildPrimitive);
   cpu_build_scratch_size += max_prim_count * 9 * sizeof(float);

   uint64_t cpu_update_scratch_size = cpu_build_scratch_size; /* TODO */

   switch (buildType)
   {
   case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR:
   case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR:
   case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_OR_DEVICE_KHR:
      pSizeInfo->buildScratchSize = cpu_build_scratch_size;
      pSizeInfo->updateScratchSize = cpu_update_scratch_size;
      break;

   default:
      unreachable("Invalid acceleration structure build type");
   }
}

VkResult
lvp_CreateAccelerationStructureKHR(
    VkDevice _device,
    const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkAccelerationStructureKHR *pAccelerationStructure)
{
   printf("lvp_CreateAccelerationStructureKHR\n");
   LVP_FROM_HANDLE(lvp_device, device, _device);
   LVP_FROM_HANDLE(lvp_buffer, buffer, pCreateInfo->buffer);
   struct lvp_acceleration_structure *accel;

   accel = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*accel), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (accel == NULL)
      return vk_error(accel, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &accel->base,
                       VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);

   // TODO: Check that the sizes match
   accel->size = pCreateInfo->size;
   accel->address = (struct anv_address){
       .bo = (struct anv_bo *)buffer->pmem,
       .offset = (buffer->offset + pCreateInfo->offset),
   };
   printf("LVP: Accel structure (size 0x%lx) created from lvp_buffer (size 0x%lx). \n", accel->size, buffer->total_size);
   printf("LVP: Buffer %p + 0x%lx = %p allocated to accel structure %p\n", accel->address.bo, accel->address.offset, (void *)accel->address.bo + accel->address.offset, accel);

   *pAccelerationStructure = lvp_acceleration_structure_to_handle(accel);

   if (pCreateInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
   {
      gpgpusim_allocTLAS((void *)accel->address.bo + accel->address.offset, buffer->total_size, (void *)buffer->pBuffer_gpgpusim + accel->address.offset);
   }
   else
   {
      gpgpusim_allocBLAS((void *)accel->address.bo + accel->address.offset, buffer->total_size, (void *)buffer->pBuffer_gpgpusim + accel->address.offset);
   }

   return VK_SUCCESS;
}

void lvp_DestroyAccelerationStructureKHR(
    VkDevice _device,
    VkAccelerationStructureKHR accelerationStructure,
    const VkAllocationCallbacks *pAllocator)
{
   printf("lvp_DestroyAccelerationStructureKHR\n");
   LVP_FROM_HANDLE(lvp_device, device, _device);
   LVP_FROM_HANDLE(lvp_acceleration_structure, accel, accelerationStructure);

   if (!accel)
      return;

   vk_object_base_finish(&accel->base);
   vk_free2(&device->vk.alloc, pAllocator, accel);
}

VkDeviceAddress
lvp_GetAccelerationStructureDeviceAddressKHR(
    VkDevice device,
    const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
   printf("lvp_GetAccelerationStructureDeviceAddressKHR\n");
   LVP_FROM_HANDLE(lvp_acceleration_structure, accel,
                   pInfo->accelerationStructure);

   // Copied from Intel; might be unnecessary
   assert(!anv_address_is_null(accel->address));

   printf("LVP: Returning BLAS %p address %p\n", accel, (void *)accel->address.bo + accel->address.offset);
   return (VkDeviceAddress)((void *)accel->address.bo + accel->address.offset);
}

void lvp_GetDeviceAccelerationStructureCompatibilityKHR(
    VkDevice device,
    const VkAccelerationStructureVersionInfoKHR *pVersionInfo,
    VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
   unreachable("Unimplemented");
}

__attribute__((always_inline)) static inline uint32_t
get_index_for_type(const void *index_data, VkIndexType index_type, uint32_t i)
{
   switch (index_type)
   {
   case VK_INDEX_TYPE_NONE_KHR:
      return i;

   case VK_INDEX_TYPE_UINT8_EXT:
      return ((uint8_t *)index_data)[i];

   case VK_INDEX_TYPE_UINT16:
      return ((uint16_t *)index_data)[i];

   case VK_INDEX_TYPE_UINT32:
      return ((uint32_t *)index_data)[i];

   default:
      unreachable("Invalid index type");
   }
}

__attribute__((always_inline)) static inline float
snorm16_to_float(int16_t x)
{
   if (x <= -INT16_MAX)
      return -1.0f;
   else
      return x * (1.0f / (float)INT16_MAX);
}

__attribute__((always_inline)) static inline struct vsim_bvh_vec4f
get_vertex_for_format(const void *vertex_data, VkFormat format)
{
   /* TODO: This seriously needs some SSE */

   switch (format)
   {
   case VK_FORMAT_R32G32_SFLOAT:
      return (struct vsim_bvh_vec4f){
          .v = {
              ((float *)vertex_data)[0],
              ((float *)vertex_data)[1],
              0.0f,
              1.0f,
          }};

   case VK_FORMAT_R32G32B32_SFLOAT:
      return (struct vsim_bvh_vec4f){
          .v = {
              ((float *)vertex_data)[0],
              ((float *)vertex_data)[1],
              ((float *)vertex_data)[2],
              1.0f,
          }};

   case VK_FORMAT_R16G16_SFLOAT:
      return (struct vsim_bvh_vec4f){
          .v = {
              _mesa_half_to_float(((uint16_t *)vertex_data)[0]),
              _mesa_half_to_float(((uint16_t *)vertex_data)[1]),
              0.0f,
              1.0f,
          },
      };

   case VK_FORMAT_R16G16B16A16_SFLOAT:
      return (struct vsim_bvh_vec4f){
          .v = {
              _mesa_half_to_float(((uint16_t *)vertex_data)[0]),
              _mesa_half_to_float(((uint16_t *)vertex_data)[1]),
              _mesa_half_to_float(((uint16_t *)vertex_data)[2]),
              _mesa_half_to_float(((uint16_t *)vertex_data)[3]),
          },
      };

   case VK_FORMAT_R16G16_SNORM:
      return (struct vsim_bvh_vec4f){
          .v = {
              snorm16_to_float(((uint16_t *)vertex_data)[0]),
              snorm16_to_float(((uint16_t *)vertex_data)[1]),
              0.0f,
              1.0f,
          },
      };

   case VK_FORMAT_R16G16B16A16_SNORM:
      return (struct vsim_bvh_vec4f){
          .v = {
              snorm16_to_float(((uint16_t *)vertex_data)[0]),
              snorm16_to_float(((uint16_t *)vertex_data)[1]),
              snorm16_to_float(((uint16_t *)vertex_data)[2]),
              snorm16_to_float(((uint16_t *)vertex_data)[3]),
          },
      };

   default:
      unreachable("Unsupported acceleration structure vertex format");
   }
}

__attribute__((always_inline)) static inline struct vsim_bvh_vec3f
xform_vertex(const VkTransformMatrixKHR *mat, struct vsim_bvh_vec4f vert)
{
   struct vsim_bvh_vec3f out = {.v = {0, 0, 0}};
   for (unsigned r = 0; r < 3; r++)
   {
      for (unsigned c = 0; c < 4; c++)
         out.v[r] += mat->matrix[r][c] * vert.v[c];
   }
   return out;
}

__attribute__((always_inline)) static inline void
_lvp_bvh_parse_triangles(struct vsim_bvh_triangle *triangles_out,
                         uint32_t triangle_count,
                         const void *index_data,
                         VkIndexType index_type,
                         const void *vertex_data,
                         size_t vertex_stride,
                         VkFormat vertex_format,
                         uint32_t first_vertex,
                         bool should_transform,
                         const VkTransformMatrixKHR *transform)
{
   for (uint32_t t = 0; t < triangle_count; t++)
   {
      for (unsigned i = 0; i < 3; i++)
      {
         uint32_t index = get_index_for_type(index_data, index_type, t * 3 + i);
         const void *vertex = vertex_data +
                              (index + first_vertex) * vertex_stride;
         struct vsim_bvh_vec4f raw_vert =
             get_vertex_for_format(vertex, vertex_format);
         if (!isnan(raw_vert.v[0]) && should_transform)
         {
            triangles_out[t].v[i] = xform_vertex(transform, raw_vert);
         }
         else
         {
            triangles_out[t].v[i] = (struct vsim_bvh_vec3f){
                .v = {
                    raw_vert.v[0],
                    raw_vert.v[1],
                    raw_vert.v[2],
                },
            };
         }
      }
   }
}

static void
lvp_bvh_parse_triangles(struct vsim_bvh_triangle *triangles_out,
                        uint32_t triangle_count,
                        const void *index_data,
                        VkIndexType index_type,
                        const void *vertex_data,
                        size_t vertex_stride,
                        VkFormat vertex_format,
                        uint32_t first_vertex,
                        bool should_transform,
                        const VkTransformMatrixKHR *transform)
{
   if (index_data == NULL)
      index_type = VK_INDEX_TYPE_NONE_KHR;

#define BVH_TRI_CASE(c_index_type, c_vertex_format, c_should_transform)   \
   case VK_FORMAT_##c_vertex_format:                                      \
      _lvp_bvh_parse_triangles(triangles_out, triangle_count,             \
                               index_data, VK_INDEX_TYPE_##c_index_type,  \
                               vertex_data, vertex_stride,                \
                               VK_FORMAT_##c_vertex_format, first_vertex, \
                               c_should_transform, transform);            \
      break;

   if (should_transform)
   {
      switch (index_type)
      {
      case VK_INDEX_TYPE_NONE_KHR:
         switch (vertex_format)
         {
            BVH_TRI_CASE(NONE_KHR, R32G32_SFLOAT, true)
            BVH_TRI_CASE(NONE_KHR, R32G32B32_SFLOAT, true)
            BVH_TRI_CASE(NONE_KHR, R16G16_SFLOAT, true)
            BVH_TRI_CASE(NONE_KHR, R16G16B16A16_SFLOAT, true)
            BVH_TRI_CASE(NONE_KHR, R16G16_SNORM, true)
            BVH_TRI_CASE(NONE_KHR, R16G16B16A16_SNORM, true)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT8_EXT:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT8_EXT, R32G32_SFLOAT, true)
            BVH_TRI_CASE(UINT8_EXT, R32G32B32_SFLOAT, true)
            BVH_TRI_CASE(UINT8_EXT, R16G16_SFLOAT, true)
            BVH_TRI_CASE(UINT8_EXT, R16G16B16A16_SFLOAT, true)
            BVH_TRI_CASE(UINT8_EXT, R16G16_SNORM, true)
            BVH_TRI_CASE(UINT8_EXT, R16G16B16A16_SNORM, true)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT16:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT16, R32G32_SFLOAT, true)
            BVH_TRI_CASE(UINT16, R32G32B32_SFLOAT, true)
            BVH_TRI_CASE(UINT16, R16G16_SFLOAT, true)
            BVH_TRI_CASE(UINT16, R16G16B16A16_SFLOAT, true)
            BVH_TRI_CASE(UINT16, R16G16_SNORM, true)
            BVH_TRI_CASE(UINT16, R16G16B16A16_SNORM, true)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT32:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT32, R32G32_SFLOAT, true)
            BVH_TRI_CASE(UINT32, R32G32B32_SFLOAT, true)
            BVH_TRI_CASE(UINT32, R16G16_SFLOAT, true)
            BVH_TRI_CASE(UINT32, R16G16B16A16_SFLOAT, true)
            BVH_TRI_CASE(UINT32, R16G16_SNORM, true)
            BVH_TRI_CASE(UINT32, R16G16B16A16_SNORM, true)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      default:
         unreachable("Invalid index type");
      }
   }
   else
   {
      switch (index_type)
      {
      case VK_INDEX_TYPE_NONE_KHR:
         switch (vertex_format)
         {
            BVH_TRI_CASE(NONE_KHR, R32G32_SFLOAT, false)
            BVH_TRI_CASE(NONE_KHR, R32G32B32_SFLOAT, false)
            BVH_TRI_CASE(NONE_KHR, R16G16_SFLOAT, false)
            BVH_TRI_CASE(NONE_KHR, R16G16B16A16_SFLOAT, false)
            BVH_TRI_CASE(NONE_KHR, R16G16_SNORM, false)
            BVH_TRI_CASE(NONE_KHR, R16G16B16A16_SNORM, false)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT8_EXT:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT8_EXT, R32G32_SFLOAT, false)
            BVH_TRI_CASE(UINT8_EXT, R32G32B32_SFLOAT, false)
            BVH_TRI_CASE(UINT8_EXT, R16G16_SFLOAT, false)
            BVH_TRI_CASE(UINT8_EXT, R16G16B16A16_SFLOAT, false)
            BVH_TRI_CASE(UINT8_EXT, R16G16_SNORM, false)
            BVH_TRI_CASE(UINT8_EXT, R16G16B16A16_SNORM, false)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT16:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT16, R32G32_SFLOAT, false)
            BVH_TRI_CASE(UINT16, R32G32B32_SFLOAT, false)
            BVH_TRI_CASE(UINT16, R16G16_SFLOAT, false)
            BVH_TRI_CASE(UINT16, R16G16B16A16_SFLOAT, false)
            BVH_TRI_CASE(UINT16, R16G16_SNORM, false)
            BVH_TRI_CASE(UINT16, R16G16B16A16_SNORM, false)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      case VK_INDEX_TYPE_UINT32:
         switch (vertex_format)
         {
            BVH_TRI_CASE(UINT32, R32G32_SFLOAT, false)
            BVH_TRI_CASE(UINT32, R32G32B32_SFLOAT, false)
            BVH_TRI_CASE(UINT32, R16G16_SFLOAT, false)
            BVH_TRI_CASE(UINT32, R16G16B16A16_SFLOAT, false)
            BVH_TRI_CASE(UINT32, R16G16_SNORM, false)
            BVH_TRI_CASE(UINT32, R16G16B16A16_SNORM, false)
         default:
            unreachable("Unsupported acceleration structure vertex format");
         }
         break;

      default:
         unreachable("Invalid index type");
      }
   }
}

static uint32_t
add_bvh_triangle_geometry(uint32_t geometry_id,
                          struct RTCBuildPrimitive *prims_out,
                          const struct vsim_bvh_triangle *triangles,
                          uint32_t first_primitive_id,
                          uint32_t primitive_count)
{
   uint32_t idx = 0;
   for (uint32_t p = 0; p < primitive_count; p++)
   {
      if (isnan(triangles[p].v[0].v[0]))
         continue;

      struct RTCBuildPrimitive prim = {
          .geomID = geometry_id,
          .primID = first_primitive_id + p,
          .lower_x = triangles[p].v[0].v[0],
          .lower_y = triangles[p].v[0].v[1],
          .lower_z = triangles[p].v[0].v[2],
          .upper_x = triangles[p].v[0].v[0],
          .upper_y = triangles[p].v[0].v[1],
          .upper_z = triangles[p].v[0].v[2],
      };
      for (unsigned i = 1; i < 3; i++)
      {
         prim.lower_x = MIN2(prim.lower_x, triangles[p].v[i].v[0]);
         prim.lower_y = MIN2(prim.lower_y, triangles[p].v[i].v[1]);
         prim.lower_z = MIN2(prim.lower_z, triangles[p].v[i].v[2]);
         prim.upper_x = MAX2(prim.upper_x, triangles[p].v[i].v[0]);
         prim.upper_y = MAX2(prim.upper_y, triangles[p].v[i].v[1]);
         prim.upper_z = MAX2(prim.upper_z, triangles[p].v[i].v[2]);
      }
      prims_out[idx++] = prim;
   }
   return idx;
}

static uint32_t
add_bvh_aabbs_geometry(uint32_t geometry_id,
                       struct RTCBuildPrimitive *prims_out,
                       uint32_t first_primitive_id,
                       uint32_t primitive_count,
                       const void *aabbs_data,
                       size_t aabbs_stride)
{
   uint32_t idx = 0;
   for (uint32_t p = 0; p < primitive_count; p++)
   {
      const VkAabbPositionsKHR *pos = aabbs_data + p * aabbs_stride;
      if (isnan(pos->minX))
         continue;

      prims_out[idx++] = (struct RTCBuildPrimitive){
          .geomID = geometry_id,
          .primID = first_primitive_id + p,
          .lower_x = pos->minX,
          .lower_y = pos->minY,
          .lower_z = pos->minZ,
          .upper_x = pos->maxX,
          .upper_y = pos->maxY,
          .upper_z = pos->maxZ,
      };
      MESA_VSIM_DPRINTF("EMBREE: Add AABB geometry: geomID: %d, primID %d, box (%5.3f, %5.3f, %5.3f), (%5.3f, %5.3f, %5.3f)\n",
                        geometry_id, first_primitive_id + p,
                        pos->minX, pos->minY, pos->minZ,
                        pos->maxX, pos->maxY, pos->maxZ);
   }
   return idx;
}

static void
get_accel_instance(const void *instance_data,
                   bool array_of_pointers,
                   uint32_t primitive_id,
                   const VkAccelerationStructureInstanceKHR **instance,
                   const void **accel_map,
                   struct anv_address *accel_addr,
                   struct lvp_bvh_build_state *state)
{
   if (array_of_pointers)
   {
      const VkDeviceOrHostAddressConstKHR *instance_addrs = instance_data;
      *instance = doha_to_host_const(instance_addrs[primitive_id], state);
   }
   else
   {
      const VkAccelerationStructureInstanceKHR *instances = instance_data;
      *instance = &instances[primitive_id];
   }
   uint64_t reference = (*instance)->accelerationStructureReference;

   printf("EMBREE: Get accel instance -> ");
   if (reference == 0)
   {
      *accel_map = NULL;
      *accel_addr = ANV_NULL_ADDRESS;
      printf("empty\n");
   }
   else if (state->is_host_build)
   {
      LVP_FROM_HANDLE(lvp_acceleration_structure, child_accel,
                      (VkAccelerationStructureKHR)reference);
      *accel_map = anv_address_map(child_accel->address);
      *accel_addr = child_accel->address;
      printf("host %p\n", accel_map);
   }
   else
   {
      const VkDeviceOrHostAddressConstKHR accel_doha = {
          .deviceAddress = reference,
      };
      //*accel_map = doha_to_host_const(accel_doha, state);
      *accel_map = (void *)reference;
      *accel_addr = anv_address_from_u64(reference);
      printf("device %p\n", accel_map);
   }
}

static uint32_t
add_bvh_instances(uint32_t geometry_id,
                  struct RTCBuildPrimitive *prims_out,
                  uint32_t first_primitive_id,
                  uint32_t primitive_count,
                  bool array_of_pointers,
                  const void *instance_data,
                  struct lvp_bvh_build_state *state)
{
   uint32_t idx = 0;
   for (uint32_t p = 0; p < primitive_count; p++)
   {
      const VkAccelerationStructureInstanceKHR *instance;
      UNUSED struct anv_address child_addr;
      const void *child_map;
      get_accel_instance(instance_data, array_of_pointers, p,
                         &instance, &child_map, &child_addr,
                         state);
      if (child_map == NULL)
         continue;

      /* TODO: Unpack with GenXML? */
      struct vsim_bvh_vec3f child_bounds[2] = {
          {.v = {
               ((const float *)child_map)[2],
               ((const float *)child_map)[3],
               ((const float *)child_map)[4],
           }},
          {.v = {
               ((const float *)child_map)[5],
               ((const float *)child_map)[6],
               ((const float *)child_map)[7],
           }}};

      struct vsim_bvh_vec3f child_lower, child_upper;

      /* Transform each corner of the child's bounding box to compute the
       * bounding box in the parent's space.
       */
      for (unsigned x_i = 0; x_i < 2; x_i++)
      {
         for (unsigned y_i = 0; y_i < 2; y_i++)
         {
            for (unsigned z_i = 0; z_i < 2; z_i++)
            {
               struct vsim_bvh_vec4f corner = {
                   .v = {
                       child_bounds[x_i].v[0],
                       child_bounds[y_i].v[1],
                       child_bounds[z_i].v[2],
                       1.0f,
                   },
               };
               struct vsim_bvh_vec3f xform_corner =
                   xform_vertex(&instance->transform, corner);
               if (x_i == 0 && y_i == 0 && z_i == 0)
               {
                  child_lower = child_upper = xform_corner;
               }
               else
               {
                  for (unsigned i = 0; i < 3; i++)
                  {
                     child_lower.v[i] = MIN2(child_lower.v[i], xform_corner.v[i]);
                     child_upper.v[i] = MAX2(child_upper.v[i], xform_corner.v[i]);
                  }
               }
            }
         }
      }

      prims_out[idx++] = (struct RTCBuildPrimitive){
          .geomID = geometry_id,
          .primID = first_primitive_id + p,
          .lower_x = child_lower.v[0],
          .lower_y = child_lower.v[1],
          .lower_z = child_lower.v[2],
          .upper_x = child_upper.v[0],
          .upper_y = child_upper.v[1],
          .upper_z = child_upper.v[2],
      };
      printf("EMBREE: Add AABB geometry: geomID: %d, primID %d, box (%5.3f, %5.3f, %5.3f), (%5.3f, %5.3f, %5.3f)\n",
             geometry_id, first_primitive_id + p,
             child_lower.v[0], child_lower.v[1], child_lower.v[2],
             child_upper.v[0], child_upper.v[1], child_upper.v[2]);
   }
   return idx;
}

static void
vsim_bvh_node_init(struct vsim_bvh_node *node)
{
   *node = (struct vsim_bvh_node){
       .is_leaf = false,
   };
}

static void
vsim_bvh_node_set_children(struct vsim_bvh_node *node,
                           struct vsim_bvh_node **children,
                           unsigned child_count)
{
   assert(child_count <= ARRAY_SIZE(node->children));
   for (unsigned i = 0; i < child_count; i++)
      node->children[i] = children[i];
}

static void
calc_bounds(float *origin_out, int8_t *exp_out,
            uint8_t *lower_out, uint8_t *upper_out,
            float *lower_in, float *upper_in, unsigned count)
{
   if (count == 0)
   {
      *exp_out = 0;
      return;
   }

   float lower_bound = *lower_in;
   float upper_bound = *upper_in;
   for (unsigned i = 1; i < count; i++)
   {
      assert(lower_in[i] <= upper_in[i]);
      lower_bound = MIN2(lower_bound, lower_in[i]);
      upper_bound = MAX2(upper_bound, upper_in[i]);
   }

   assert(lower_bound <= upper_bound);
   float total_bound = upper_bound - lower_bound;

   if (isinf(total_bound))
   {
      *origin_out = -FLT_MAX;
      *exp_out = INT8_MAX;
      for (unsigned i = 0; i < count; i++)
      {
         lower_out[i] = 0;
         upper_out[i] = UINT8_MAX;
      }
      return;
   }

   int exp;
   float m = frexp(total_bound, &exp);
   /* We're guaranteed that m < 1 but we don't know that m < 255/256 */
   if (m > 255.0f / 256.0f)
      exp++;

   /* Clamp out exponent to be in-range */
   assert(exp < INT8_MAX);
   if (exp < INT8_MIN)
      exp = INT8_MIN;

   *origin_out = lower_bound;
   *exp_out = exp;
   for (unsigned i = 0; i < count; i++)
   {
      lower_out[i] = floorf(ldexpf(lower_in[i] - lower_bound, 8 - exp));
      upper_out[i] = ceilf(ldexpf(upper_in[i] - lower_bound, 8 - exp));
   }
}

/* Callback to set the bounds of all children */
static void
vsim_bvh_node_set_child_bounds(struct vsim_bvh_node *node,
                               const struct RTCBounds **bounds,
                               unsigned child_count)
{
   float lower[6], upper[6];

   for (unsigned i = 0; i < child_count; i++)
   {
      lower[i] = bounds[i]->lower_x;
      upper[i] = bounds[i]->upper_x;
   }
   calc_bounds(&node->origin_x, &node->exp_x,
               node->lower_x, node->upper_x,
               lower, upper, child_count);

   for (unsigned i = 0; i < child_count; i++)
   {
      lower[i] = bounds[i]->lower_y;
      upper[i] = bounds[i]->upper_y;
   }
   calc_bounds(&node->origin_y, &node->exp_y,
               node->lower_y, node->upper_y,
               lower, upper, child_count);

   for (unsigned i = 0; i < child_count; i++)
   {
      lower[i] = bounds[i]->lower_z;
      upper[i] = bounds[i]->upper_z;
   }
   calc_bounds(&node->origin_z, &node->exp_z,
               node->lower_z, node->upper_z,
               lower, upper, child_count);
}

static void
vsim_bvh_leaf_init(struct vsim_bvh_leaf *leaf,
                   const struct RTCBuildPrimitive *primitive)
{
   *leaf = (struct vsim_bvh_leaf){
       .is_leaf = true,
       .geometry_id = primitive->geomID,
       .primitive_id = primitive->primID,
   };
}

static void *
rtc_create_node_cb(RTCThreadLocalAllocator alloc,
                   unsigned int childCount, void *userPtr)
{
   struct vsim_bvh_node *node = rtcThreadLocalAlloc(alloc, sizeof(*node), 8);
   vsim_bvh_node_init(node);
   return node;
}

/* Callback to set the pointer to all children */
static void
rtc_set_node_children_cb(void *nodePtr, void **children,
                         unsigned int childCount, void *userPtr)
{
   vsim_bvh_node_set_children(nodePtr, (struct vsim_bvh_node **)children,
                              childCount);
}

/* Callback to set the bounds of all children */
static void
rtc_set_node_bounds_cb(void *nodePtr, const struct RTCBounds **bounds,
                       unsigned int childCount, void *userPtr)
{
   vsim_bvh_node_set_child_bounds(nodePtr, bounds, childCount);
}

/* Callback to create a leaf node */
static void *
rtc_create_leaf_cb(RTCThreadLocalAllocator alloc,
                   const struct RTCBuildPrimitive *primitives,
                   size_t primitiveCount, void *userPtr)
{
   // assert(primitiveCount == 1);
   struct vsim_bvh_leaf *leaf = rtcThreadLocalAlloc(alloc, sizeof(*leaf), 4);
   vsim_bvh_leaf_init(leaf, primitives);
   return leaf;
}

static void
node_type_size(struct vsim_bvh_node *node, uint32_t *type, uint32_t *size,
               struct lvp_bvh_build_state *state)
{
   if (node->is_leaf)
   {
      struct vsim_bvh_leaf *leaf = (struct vsim_bvh_leaf *)node;
      struct lvp_bvh_build_geometry_state *geometry =
          &state->geometries[leaf->geometry_id];
      switch (geometry->pGeometry->geometryType)
      {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
         *type = NODE_TYPE_QUAD;
         *size = GEN_RT_BVH_QUAD_LEAF_length / 16;
         break;

      case VK_GEOMETRY_TYPE_AABBS_KHR:
         *type = NODE_TYPE_PROCEDURAL;
         *size = GEN_RT_BVH_PROCEDURAL_LEAF_length / 16;
         break;

      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
         *type = NODE_TYPE_INSTANCE;
         *size = GEN_RT_BVH_INSTANCE_LEAF_length / 16;
         break;

      default:
         unreachable("Invalid geometry type");
      }
   }
   else
   {
      *type = NODE_TYPE_INTERNAL;
      *size = GEN_RT_BVH_INTERNAL_NODE_length / 16;
   }
}

static void
pack_node(struct vsim_bvh_node *node, bool is_root, void *out,
          struct lvp_bvh_build_state *state)
{
   if (node->is_leaf)
   {
      struct vsim_bvh_leaf *leaf = (struct vsim_bvh_leaf *)node;
      struct lvp_bvh_build_geometry_state *geometry =
          &state->geometries[leaf->geometry_id];

      uint32_t geometry_flags = 0;
      if (geometry->pGeometry->flags & VK_GEOMETRY_OPAQUE_BIT_KHR)
         geometry_flags |= GEOMETRY_OPAQUE;

      switch (geometry->pGeometry->geometryType)
      {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
         struct vsim_bvh_triangle triangle =
             geometry->triangles[leaf->primitive_id];

         struct GEN_RT_BVH_QUAD_LEAF bql = {};
         bql.LeafDescriptor.ShaderIndex = leaf->geometry_id;
         bql.LeafDescriptor.GeometryRayMask = 0xff;
         bql.LeafDescriptor.GeometryIndex = leaf->geometry_id;
         bql.LeafDescriptor.LeafType = TYPE_QUAD;
         bql.LeafDescriptor.GeometryFlags = geometry_flags;
         bql.PrimitiveIndex0 = leaf->primitive_id;
         bql.LastQuad = true;

         for (unsigned i = 0; i < 3; i++)
         {
            bql.QuadVertex[i].X = triangle.v[i].v[0];
            bql.QuadVertex[i].Y = triangle.v[i].v[1];
            bql.QuadVertex[i].Z = triangle.v[i].v[2];
         }
         GEN_RT_BVH_QUAD_LEAF_pack(NULL, out, &bql);
         break;
      }

      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
         struct GEN_RT_BVH_PROCEDURAL_LEAF bpl = {};
         bpl.LeafDescriptor.ShaderIndex = leaf->geometry_id;
         bpl.LeafDescriptor.GeometryRayMask = 0xff;
         bpl.LeafDescriptor.GeometryIndex = leaf->geometry_id;
         // MRS_TODO: what this has to do with culling? why it was disabled?
         // bpl.LeafDescriptor.LeafType = TYPE_OPAQUE_CULLING_ENABLED;
         bpl.LeafDescriptor.LeafType = TYPE_OPAQUE_CULLING_DISABLED;
         bpl.LeafDescriptor.GeometryFlags = geometry_flags;
         bpl.NumPrimitives = 1;
         bpl.PrimitiveIndex[0] = leaf->primitive_id;
         GEN_RT_BVH_PROCEDURAL_LEAF_pack(NULL, out, &bpl);
         break;
      }

      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
         const VkAccelerationStructureInstanceKHR *instance;
         struct anv_address child_addr;
         const void *child_map;
         get_accel_instance(geometry->instance_data,
                            geometry->pGeometry->geometry.instances.arrayOfPointers,
                            leaf->primitive_id,
                            &instance, &child_map, &child_addr,
                            state);

         /* TODO: Unpack with GenXML? */
         uint64_t child_start_offset = *(uint64_t *)child_map;

         float o2w4x4[16], w2o4x4[16];
         memcpy(o2w4x4, instance->transform.matrix, 3 * 4 * sizeof(float));
         o2w4x4[12] = o2w4x4[13] = o2w4x4[14] = 0.0f;
         o2w4x4[15] = 1.0f;
         util_invert_mat4x4(w2o4x4, o2w4x4);

         struct GEN_RT_BVH_INSTANCE_LEAF bil = {};
         bil.ShaderIndex = instance->instanceCustomIndex;
         bil.GeometryRayMask = instance->mask;
         bil.InstanceContributionToHitGroupIndex =
             instance->instanceShaderBindingTableRecordOffset;
         bil.LeafType = 0 /* TODO */;
         bil.GeometryFlags = geometry_flags;
         bil.StartNodeAddress =
             anv_address_add(child_addr, child_start_offset);
         /* The hardware flags are the same as the Vulkan flags */
         bil.InstanceFlags = instance->flags;
         gpgpusim_pass_child_addr((void *)child_addr.offset);
         child_addr.offset -= (uint64_t)out; // if something breaks, undo this and remove "leaf_addr + " in vulkan_ray_tracing.cc:594,595,609
         bil.BVHAddress = child_addr;
         bil.InstanceID = instance->instanceCustomIndex;
         bil.InstanceIndex = leaf->primitive_id;

         bil.WorldToObjectm00 = w2o4x4[0];
         bil.WorldToObjectm10 = w2o4x4[1];
         bil.WorldToObjectm20 = w2o4x4[2];
         bil.WorldToObjectm30 = w2o4x4[3];
         bil.WorldToObjectm01 = w2o4x4[4];
         bil.WorldToObjectm11 = w2o4x4[5];
         bil.WorldToObjectm21 = w2o4x4[6];
         bil.WorldToObjectm31 = w2o4x4[7];
         bil.WorldToObjectm02 = w2o4x4[8];
         bil.WorldToObjectm12 = w2o4x4[9];
         bil.WorldToObjectm22 = w2o4x4[10];
         bil.WorldToObjectm32 = w2o4x4[11];

         bil.ObjectToWorldm00 = o2w4x4[0];
         bil.ObjectToWorldm10 = o2w4x4[1];
         bil.ObjectToWorldm20 = o2w4x4[2];
         bil.ObjectToWorldm30 = o2w4x4[3];
         bil.ObjectToWorldm01 = o2w4x4[4];
         bil.ObjectToWorldm11 = o2w4x4[5];
         bil.ObjectToWorldm21 = o2w4x4[6];
         bil.ObjectToWorldm31 = o2w4x4[7];
         bil.ObjectToWorldm02 = o2w4x4[8];
         bil.ObjectToWorldm12 = o2w4x4[9];
         bil.ObjectToWorldm22 = o2w4x4[10];
         bil.ObjectToWorldm32 = o2w4x4[11];

         GEN_RT_BVH_INSTANCE_LEAF_pack(NULL, out, &bil);
         break;
      }

      default:
         unreachable("Invalid geometry type");
      }
   }
   else
   {
      struct GEN_RT_BVH_INTERNAL_NODE bin = {};
      bin.Origin.X = node->origin_x;
      bin.Origin.Y = node->origin_y;
      bin.Origin.Z = node->origin_z;
      bin.NodeType = NODE_TYPE_INTERNAL;
      bin.ChildBoundsExponentX = node->exp_x;
      bin.ChildBoundsExponentY = node->exp_y;
      bin.ChildBoundsExponentZ = node->exp_z;
      bin.NodeRayMask = 0xff;

      unsigned num_children = 0;
      unsigned children_size = 0;
      for (unsigned i = 0; i < 6; i++)
      {
         if (node->children[i] == NULL)
         {
            bin.ChildSize[i] = 0;
            bin.ChildLowerXBound[i] = 0x80;
            bin.ChildLowerYBound[i] = 0x80;
            bin.ChildLowerZBound[i] = 0x80;
            bin.ChildUpperXBound[i] = 0x00;
            bin.ChildUpperYBound[i] = 0x00;
            bin.ChildUpperZBound[i] = 0x00;
         }
         else
         {
            num_children++;
            node_type_size(node->children[i], &bin.ChildType[i],
                           &bin.ChildSize[i], state);
            bin.ChildLowerXBound[i] = node->lower_x[i];
            bin.ChildUpperXBound[i] = node->upper_x[i];
            bin.ChildLowerYBound[i] = node->lower_y[i];
            bin.ChildUpperYBound[i] = node->upper_y[i];
            bin.ChildLowerZBound[i] = node->lower_z[i];
            bin.ChildUpperZBound[i] = node->upper_z[i];
         }
         children_size += bin.ChildSize[i] * 64;
      }

      if (unlikely(num_children <= 1))
      {
         /* This should only happen in the case of a root with either no
          * children or exactly one leaf child.
          */
         assert(is_root);
      }

      void *children_map = state->nodes_map;
      state->nodes_map += children_size;
      uint64_t child_offset = children_map - out;
      assert(child_offset % 64 == 0 && child_offset / 64 <= UINT32_MAX);
      bin.ChildOffset = (children_map - out) / 64;

      GEN_RT_BVH_INTERNAL_NODE_pack(NULL, out, &bin);

      for (unsigned i = 0; i < 6; i++)
      {
         if (node->children[i] == NULL)
            break;

         pack_node(node->children[i], false, children_map, state);
         children_map += bin.ChildSize[i] * 64;
      }
   }
}

static VkResult
lvp_cpu_build_acceleration_structures(
    struct lvp_device *device,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos,
    bool is_host_build)
{
   printf("LVP: Building acceleration structure\n");
   RTCDevice rtc = rtcNewDevice("ignore_config_files=1");
   rtcSetDeviceErrorFunction(rtc, embree_error_function, NULL);

   for (uint32_t i = 0; i < infoCount; i++)
   {
      const VkAccelerationStructureBuildGeometryInfoKHR *pInfo = &pInfos[i];
      gpgpusim_setGeometries(pInfo->pGeometries, pInfo->geometryCount);
      const VkAccelerationStructureBuildRangeInfoKHR *pBuildRangeInfos =
          ppBuildRangeInfos[i];
      LVP_FROM_HANDLE(lvp_acceleration_structure, src_accel,
                      pInfo->srcAccelerationStructure);
      LVP_FROM_HANDLE(lvp_acceleration_structure, dst_accel,
                      pInfo->dstAccelerationStructure);

      struct lvp_bvh_build_state build_state = {
          .device = device,
          .is_host_build = is_host_build,
      };

      uint64_t total_prim_count = 0;
      for (unsigned g = 0; g < pInfo->geometryCount; g++)
         total_prim_count += pBuildRangeInfos[i].primitiveCount;

      void *scratch = doha_to_host(pInfo->scratchData, &build_state);
      struct RTCBuildPrimitive *scratch_rtc_prims = scratch;
      scratch += total_prim_count * sizeof(*scratch_rtc_prims);
      struct lvp_bvh_build_geometry_state *scratch_geometries = scratch;
      scratch += pInfo->geometryCount * sizeof(*scratch_geometries);
      struct vsim_bvh_triangle *scratch_triangles = scratch;
      scratch += total_prim_count * sizeof(*scratch_triangles);

      build_state.geometries = scratch_geometries;

      uint32_t prim_count = 0;

      for (unsigned g = 0; g < pInfo->geometryCount; g++)
      {
         const VkAccelerationStructureGeometryKHR *pGeometry =
             pInfo->pGeometries ? &pInfo->pGeometries[g] : pInfo->ppGeometries[g];

         build_state.geometries[g] = (struct lvp_bvh_build_geometry_state){
             .pGeometry = pGeometry,
             .triangles = &scratch_triangles[prim_count],
         };

         switch (pGeometry->geometryType)
         {
         case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
         {
            const VkAccelerationStructureGeometryTrianglesDataKHR *triangles =
                &pGeometry->geometry.triangles;

            assert(infoCount == 1); // these g indices down here probably should be i

            lvp_bvh_parse_triangles(build_state.geometries[g].triangles, // triangles_out
                                    pBuildRangeInfos[g].primitiveCount,  // triangle_count
                                    doha_to_host_const(triangles->indexData,
                                                       &build_state) +
                                        pBuildRangeInfos[g].primitiveOffset,
                                    triangles->indexType,
                                    doha_to_host_const(triangles->vertexData,
                                                       &build_state) +
                                        0 /*pBuildRangeInfos[g].primitiveOffset*/,
                                    triangles->vertexStride,
                                    triangles->vertexFormat,
                                    pBuildRangeInfos[g].firstVertex,
                                    doha_to_host_const(triangles->transformData,
                                                       &build_state),
                                    doha_to_host_const(triangles->transformData,
                                                       &build_state) +
                                        pBuildRangeInfos[g].transformOffset);

            uint32_t tri_added = add_bvh_triangle_geometry(g, &scratch_rtc_prims[prim_count],
                                                           build_state.geometries[g].triangles,
                                                           0, pBuildRangeInfos[g].primitiveCount);
            printf("EMBREE: %d triangles added; total: %d\n", tri_added, prim_count + tri_added);
            prim_count += tri_added;
            break;
         }

         case VK_GEOMETRY_TYPE_AABBS_KHR:
         {
            const VkAccelerationStructureGeometryAabbsDataKHR *aabbs =
                &pGeometry->geometry.aabbs;
            prim_count +=
                add_bvh_aabbs_geometry(g, &scratch_rtc_prims[prim_count],
                                       0, pBuildRangeInfos[g].primitiveCount,
                                       doha_to_host_const(aabbs->data,
                                                          &build_state) +
                                           pBuildRangeInfos[g].primitiveOffset,
                                       aabbs->stride);
            break;
         }

         case VK_GEOMETRY_TYPE_INSTANCES_KHR:
         {
            const VkAccelerationStructureGeometryInstancesDataKHR *instances =
                &pGeometry->geometry.instances;
            build_state.geometries[g].instance_data =
                doha_to_host_const(instances->data, &build_state) +
                pBuildRangeInfos[g].primitiveOffset;
            prim_count +=
                add_bvh_instances(g, &scratch_rtc_prims[prim_count],
                                  0, pBuildRangeInfos[g].primitiveCount,
                                  instances->arrayOfPointers,
                                  build_state.geometries[g].instance_data,
                                  &build_state);
            break;
         }

         default:
            unreachable("Unimplemented");
         }
      }

      RTCBVH rtc_bvh = rtcNewBVH(rtc);
      MESA_VSIM_DPRINTF("EMBREE: New rtcBVH structure created at %p\n", rtc_bvh);

      struct vsim_bvh_node tmp_root;
      struct vsim_bvh_leaf tmp_leaf;

      struct vsim_bvh_node *root;
      if (prim_count <= 1)
      {
         /* In this case, we don't aclvp_cpu_build_acceleration_structurestually need Embree to build a tree for us
          * because we only have one leaf.  It's easier to just construct the
          * two nodes we need manually.
          *
          * Note: We need every BVH to start with an internal node, not a
          * leaf.
          */
         vsim_bvh_node_init(&tmp_root);
         if (prim_count > 0)
            vsim_bvh_leaf_init(&tmp_leaf, scratch_rtc_prims);
         vsim_bvh_node_set_children(&tmp_root,
                                    (struct vsim_bvh_node *[]){
                                        (struct vsim_bvh_node *)&tmp_leaf,
                                    },
                                    prim_count);
         vsim_bvh_node_set_child_bounds(&tmp_root,
                                        (const struct RTCBounds *[]){
                                            (const struct RTCBounds *)scratch_rtc_prims,
                                        },
                                        prim_count);
         root = &tmp_root;
      }
      else
      {
         struct RTCBuildArguments args = rtcDefaultBuildArguments();
         args.byteSize = sizeof(args);
         args.maxBranchingFactor = 6;
         args.minLeafSize = 1,
         args.maxLeafSize = 7,
         args.bvh = rtc_bvh;
         args.primitives = scratch_rtc_prims;
         args.primitiveCount = prim_count;
         args.primitiveArrayCapacity = prim_count;
         args.createNode = rtc_create_node_cb;
         args.setNodeChildren = rtc_set_node_children_cb;
         args.setNodeBounds = rtc_set_node_bounds_cb;
         args.createLeaf = rtc_create_leaf_cb;
         args.userPtr = &build_state;

         const char *env_build_args = getenv("MESA_BVH_BUILD_ARGS");
         if (env_build_args)
         {
            printf("EMBREE: Using custom build args: %s\n", env_build_args);
            sscanf(env_build_args, "%u,%u,%u", &args.maxBranchingFactor, &args.maxDepth, &args.buildQuality);
         }

         printf("EMBREE: Building %d wide tree (max depth %d) using quality %d\n", args.maxBranchingFactor, args.maxDepth, args.buildQuality);
         root = rtcBuildBVH(&args);

         if (!root)
         {
            abort();
         }

         MESA_VSIM_DPRINTF("EMBREE: Build BVH with root %p\n", root);
         print_node(*root);
      }
      assert(!root->is_leaf);

      UNUSED uint32_t root_type, root_size;
      node_type_size(root, &root_type, &root_size, &build_state);

      void *dst_map = (uint8_t *)dst_accel->address.bo + dst_accel->address.offset;
      MESA_VSIM_DPRINTF("EMBREE: Set dst_map %p = accel->address.bo %p + accel->address.offset 0x%lx for accel %p\n",
                        dst_map, dst_accel->address.bo, dst_accel->address.offset, dst_accel);

      struct GEN_RT_BVH bvh = {};
      bvh.RootNodeOffset = GEN_RT_BVH_length * 4;
      uint8_t max_child_x = 0, max_child_y = 0, max_child_z = 0;
      for (unsigned i = 0; i < 6; i++)
      {
         max_child_x = MAX2(max_child_x, root->upper_x[i]);
         max_child_y = MAX2(max_child_y, root->upper_y[i]);
         max_child_z = MAX2(max_child_z, root->upper_z[i]);
      }
      bvh.BoundsMin.X = root->origin_x;
      bvh.BoundsMin.Y = root->origin_y;
      bvh.BoundsMin.Z = root->origin_z;
      bvh.BoundsMax.X = root->origin_x + ldexpf(max_child_x, root->exp_x - 8);
      bvh.BoundsMax.Y = root->origin_y + ldexpf(max_child_y, root->exp_y - 8);
      bvh.BoundsMax.Z = root->origin_z + ldexpf(max_child_z, root->exp_z - 8);
      GEN_RT_BVH_pack(NULL, dst_map, &bvh);
      printf("EMBREE: Pack bvh %p to dst_map %p\n", &bvh, dst_map);

      void *root_map = dst_map + bvh.RootNodeOffset;
      build_state.nodes_map = root_map + root_size * 64;
      pack_node(root, true, root_map, &build_state);
      printf("EMBREE: Pack root %p to root_map %p\n", root, root_map);
      if (pInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
         gpgpusim_addTreelets(dst_map);

      rtcReleaseBVH(rtc_bvh);
      MESA_VSIM_DPRINTF("EMBREE: Release rtcBVH structure created at %p\n", rtc_bvh);
   }

   rtcReleaseDevice(rtc);

   return VK_SUCCESS;
}

VkResult
lvp_BuildAccelerationStructuresKHR(
    VkDevice _device,
    VkDeferredOperationKHR deferredOperation,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
   LVP_FROM_HANDLE(lvp_device, device, _device);
   return lvp_cpu_build_acceleration_structures(device, infoCount, pInfos,
                                                ppBuildRangeInfos, true);
}

VkResult
lvp_CopyAccelerationStructureKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return vk_error(device, VK_ERROR_FEATURE_NOT_PRESENT);
}

VkResult
lvp_CopyAccelerationStructureToMemoryKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return vk_error(device, VK_ERROR_FEATURE_NOT_PRESENT);
}

VkResult
lvp_CopyMemoryToAccelerationStructureKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return vk_error(device, VK_ERROR_FEATURE_NOT_PRESENT);
}

VkResult
lvp_WriteAccelerationStructuresPropertiesKHR(
    VkDevice device,
    uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR *pAccelerationStructures,
    VkQueryType queryType,
    size_t dataSize,
    void *pData,
    size_t stride)
{
   unreachable("Unimplemented");
   return vk_error(device, VK_ERROR_FEATURE_NOT_PRESENT);
}

void lvp_CmdBuildAccelerationStructuresKHR(
    VkCommandBuffer commandBuffer,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
   LVP_FROM_HANDLE(lvp_cmd_buffer, cmd_buffer, commandBuffer);

   // if (lvp_batch_has_error(&cmd_buffer->batch))
   //    return;

   VkResult result =
       lvp_cpu_build_acceleration_structures(cmd_buffer->device,
                                             infoCount, pInfos,
                                             ppBuildRangeInfos, false);
   // if (result != VK_SUCCESS)
   //    lvp_batch_set_error(&cmd_buffer->batch, result);
   return;
}

void lvp_CmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer commandBuffer,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkDeviceAddress *pIndirectDeviceAddresses,
    const uint32_t *pIndirectStrides,
    const uint32_t *const *ppMaxPrimitiveCounts)
{
   unreachable("Unimplemented");
}

void lvp_CmdCopyAccelerationStructureKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
}

void lvp_CmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
   unreachable("Unimplemented");
}

void lvp_CmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
}

void lvp_CmdWriteAccelerationStructuresPropertiesKHR(
    VkCommandBuffer commandBuffer,
    uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR *pAccelerationStructures,
    VkQueryType queryType,
    VkQueryPool queryPool,
    uint32_t firstQuery)
{
   unreachable("Unimplemented");
}
