#pragma once

#include "AccelerationStructure.hpp"
#include "BottomLevelGeometry.hpp"

#include "build.hpp"
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

using namespace bvh_quantize;

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
		void check_correctness(bvh::Bvh<float> &bvh, int_bvh_t &int_bvh);
		void check_correctness_v2(bvh::Bvh<float> &bvh, int_bvh_v2_t &int_bvh_v2);
		void create_int_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh, int_bvh_v2_t &int_bvh);

		const Vulkan::Buffer &int_bvh_ClustersBuffer() const { return *int_bvh_clusters_Buffer_; }
		const Vulkan::Buffer &int_bvh_TrigsBuffer() const { return *int_bvh_trigs_Buffer_; }
		const Vulkan::Buffer &int_bvh_NodeBuffer() const { return *int_bvh_nodes_Buffer_; }
		const Vulkan::Buffer &int_bvh_PrimitiveIndicesBuffer() const { return *int_bvh_primitive_indices_Buffer_; }

	private:
		BottomLevelGeometry geometries_;
		std::vector<trig_t> trigs;

		// Declare buffer & device memory for INT BVH
		std::unique_ptr<Buffer> int_bvh_clusters_Buffer_;
		std::unique_ptr<DeviceMemory> int_bvh_clusters_BufferMemory_;
		std::unique_ptr<Buffer> int_bvh_trigs_Buffer_;
		std::unique_ptr<DeviceMemory> int_bvh_trigs_BufferMemory_;
		std::unique_ptr<Buffer> int_bvh_nodes_Buffer_;
		std::unique_ptr<DeviceMemory> int_bvh_nodes_BufferMemory_;
		std::unique_ptr<Buffer> int_bvh_primitive_indices_Buffer_;
		std::unique_ptr<DeviceMemory> int_bvh_primitive_indices_BufferMemory_;
	};
}
