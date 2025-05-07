#pragma once

#include "AccelerationStructure.hpp"
#include "BottomLevelGeometry.hpp"

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
#include "bvh/single_ray_traverser_v2.hpp"
#include "bvh/primitive_intersectors.hpp"

constexpr size_t max_trig_in_leaf_size = 7;

typedef bvh::Bvh<float> bvh_t;
typedef bvh::Triangle<float> trig_t;
typedef bvh::Ray<float> ray_t;
typedef bvh::Vector3<float> vector_t;
typedef bvh::BoundingBox<float> bbox_t;
typedef bvh::SweepSahBuilder<bvh_t> builder_t;

typedef bvh_t::Trig triangle_t;
typedef bvh_t::Node node_t;
typedef bvh_t::Node_v2 node_v2_t;
typedef trig_t::Intersection intersection_t;

typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::SingleRayTraverser_v2<bvh_t> traverser_v2_t;
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

		void check_correctness(bvh::Bvh<float> &bvh);
		void create_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh);

		const Vulkan::Buffer &bvh_TrigsBuffer() const { return *bvh_trigs_Buffer_; }
		const Vulkan::Buffer &bvh_NodeBuffer() const { return *bvh_nodes_Buffer_; }
		const Vulkan::Buffer &bvh_PrimitiveIndicesBuffer() const { return *bvh_primitive_indices_Buffer_; }

	private:
		BottomLevelGeometry geometries_;
		std::vector<trig_t> trigs;

		// Declare buffer & device memory for INT BVH
		std::unique_ptr<Buffer> bvh_trigs_Buffer_;
		std::unique_ptr<DeviceMemory> bvh_trigs_BufferMemory_;
		std::unique_ptr<Buffer> bvh_nodes_Buffer_;
		std::unique_ptr<DeviceMemory> bvh_nodes_BufferMemory_;
		std::unique_ptr<Buffer> bvh_primitive_indices_Buffer_;
		std::unique_ptr<DeviceMemory> bvh_primitive_indices_BufferMemory_;
	};
}
