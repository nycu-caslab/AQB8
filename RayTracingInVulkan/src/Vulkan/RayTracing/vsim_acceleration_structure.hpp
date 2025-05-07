#ifndef VSIM_ACCELERATION_STRUCTURE_H
#define VSIM_ACCELERATION_STRUCTURE_H

#include <iostream>

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

   uint8_t primitive_count;
   uint32_t primitive_index[7];
};

struct vsim_bvh_data
{
   vsim_bvh_node *vsim_bvh_nodes;
   size_t node_count;

   float *lower_x, *lower_y, *lower_z;
   float *upper_x, *upper_y, *upper_z;
};

#endif /* VSIM_ACCELERATION_STRUCTURE_H */
