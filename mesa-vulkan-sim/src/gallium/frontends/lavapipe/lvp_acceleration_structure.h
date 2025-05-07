#ifndef VSIM_ACCELERATION_STRUCTURE_H
#define VSIM_ACCELERATION_STRUCTURE_H

#include <vulkan/vulkan.h>
#include "gpgpusim_bvh.h"

#include "embree3/rtcore.h"
#include "embree3/rtcore_device.h"
#include "embree3/rtcore_builder.h"

// #include "vk_object.h"
// #include <vulkan/vulkan_intel.h>
// #include <vulkan/vk_icd.h>

struct vsim_bvh_vec4f
{
   float v[4];
};

struct vsim_bvh_vec3f
{
   float v[3];
};

struct vsim_bvh_triangle
{
   struct vsim_bvh_vec3f v[3];
};

struct vsim_bvh_node
{
   uint8_t is_leaf;

   int8_t exp_x;
   int8_t exp_y;
   int8_t exp_z;

   float origin_x;
   float origin_y;
   float origin_z;

   uint8_t lower_x[6];
   uint8_t upper_x[6];
   uint8_t lower_y[6];
   uint8_t upper_y[6];
   uint8_t lower_z[6];
   uint8_t upper_z[6];

   struct vsim_bvh_node *children[6];
};

struct vsim_bvh_leaf
{
   uint8_t is_leaf;
   uint32_t geometry_id;
   uint32_t primitive_id;
};

void print_vec4(struct vsim_bvh_vec4f val);
void print_vec3(struct vsim_bvh_vec3f val);
void print_tri(struct vsim_bvh_triangle val);
void print_node(struct vsim_bvh_node val);
void print_leaf(struct vsim_bvh_leaf val);
void embree_error_function(void *userPtr, enum RTCError error, const char *str);

#endif /* VSIM_ACCELERATION_STRUCTURE_H */
