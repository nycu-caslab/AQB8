#pragma once

#include "AccelerationStructure.hpp"
#include "BottomLevelGeometry.hpp"
#include "vsim_acceleration_structure.hpp"

namespace Assets
{
	class Procedural;
	class Scene;
}

namespace Vulkan
{
	class CommandPool;
	class Buffer;
	class DeviceMemory;
}

#include <iostream>
#include <cassert>
#include <vector>
#include <fstream>

#include "bvh/triangle.hpp"
#include "bvh/bvh.hpp"
#include "bvh/sweep_sah_builder.hpp"
#include "bvh/single_ray_traverser.hpp"
#include "bvh/single_ray_traverser_quant.hpp"
#include "bvh/primitive_intersectors.hpp"

#define COMPRESS_BVH_ALIGNMENT 64
#define COMPRESS_BVH_TRIG_length 36
#define COMPRESS_BVH_NODE_length 80
#define COMPRESS_BVH_ROOT_length 56
#define COMPRESS_BVH_PRIMITIVE_INSTANCE_length 8
constexpr size_t max_trig_in_leaf_size = 7;

typedef bvh::Bvh<float> bvh_t;
typedef bvh::Triangle<float> trig_t;
typedef bvh::Ray<float> ray_t;
typedef bvh::Vector3<float> vector_t;
typedef bvh::BoundingBox<float> bbox_t;
typedef bvh::SweepSahBuilder<bvh_t> builder_t;

typedef bvh_t::Compress_trig compress_trig_t;
typedef bvh_t::Compress_node compress_node_t;
typedef bvh_t::Compress_node_v2 compress_node_v2_t;
typedef bvh_t::Compress_root compress_root_t;
typedef bvh_t::Node node_t;
typedef trig_t::Intersection intersection_t;

typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::SingleRayTraverserQuant<bvh_t> traverser_quant_t;
typedef bvh::ClosestPrimitiveIntersector<bvh_t, trig_t> primitive_intersector_t;

namespace Vulkan::RayTracing
{
	class BottomLevelAccelerationStructure final : public AccelerationStructure
	{
	public:
		BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &) = delete;
		BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &) = delete;
		BottomLevelAccelerationStructure &operator=(BottomLevelAccelerationStructure &&) = delete;

		BottomLevelAccelerationStructure(
			const class DeviceProcedures &deviceProcedures,
			const class RayTracingProperties &rayTracingProperties,
			const BottomLevelGeometry &geometries);
		BottomLevelAccelerationStructure(BottomLevelAccelerationStructure &&other) noexcept;
		~BottomLevelAccelerationStructure();

		void Generate(
			VkCommandBuffer commandBuffer,
			CommandPool &commandPool,
			Buffer &scratchBuffer,
			VkDeviceSize scratchOffset,
			Buffer &resultBuffer,
			VkDeviceSize resultOffset,
			size_t geometry_id);

		void retrieve_triangles();

		void collect_data(struct vsim_bvh_node *root,
						  std::vector<uint64_t> &collect_node);
		void convert_bvh(std::vector<uint64_t> &collect_node,
						 std::vector<node_t> &nodes,
						 std::vector<size_t> &primitive_indices);
		void reset_bvh_bounds(bvh::Bvh<float> &bvh);

		void check_correctness(bvh::Bvh<float> &bvh, bvh::Bvh<float> &bvh_quant);
		void create_quant_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh_quant);

		const Vulkan::Buffer &compress_bvh_TrigsBuffer() const { return *compress_bvh_trigs_Buffer_; }
		const Vulkan::Buffer &compress_bvh_NodeBuffer() const { return *compress_bvh_nodes_Buffer_; }
		const Vulkan::Buffer &compress_bvh_RootBuffer() const { return *compress_bvh_root_Buffer_; }
		const Vulkan::Buffer &compress_bvh_PrimitiveIndicesBuffer() const { return *compress_bvh_primitive_indices_Buffer_; }

	private:
		BottomLevelGeometry geometries_;
		std::vector<trig_t> trigs;

		// Declare buffer & device memory for INT BVH
		std::unique_ptr<Buffer> compress_bvh_trigs_Buffer_;
		std::unique_ptr<DeviceMemory> compress_bvh_trigs_BufferMemory_;
		std::unique_ptr<Buffer> compress_bvh_nodes_Buffer_;
		std::unique_ptr<DeviceMemory> compress_bvh_nodes_BufferMemory_;
		std::unique_ptr<Buffer> compress_bvh_root_Buffer_;
		std::unique_ptr<DeviceMemory> compress_bvh_root_BufferMemory_;
		std::unique_ptr<Buffer> compress_bvh_primitive_indices_Buffer_;
		std::unique_ptr<DeviceMemory> compress_bvh_primitive_indices_BufferMemory_;
	};
}
