#include "BottomLevelAccelerationStructure.hpp"
#include "DeviceProcedures.hpp"
#include "Assets/Scene.hpp"
#include "Assets/Vertex.hpp"
#include "Utilities/Exception.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/BufferUtil.hpp"

#include "bvh/traverse.hpp"
#include "bvh/single_ray_traverser.hpp"
#include "bvh/primitive_intersectors.hpp"

typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::ClosestPrimitiveIntersector<bvh_t, trig_t> primitive_intersector_t;

namespace Vulkan::RayTracing
{

	BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(
		const class DeviceProcedures &deviceProcedures,
		const class RayTracingProperties &rayTracingProperties,
		const BottomLevelGeometry &geometries) : AccelerationStructure(deviceProcedures, rayTracingProperties),
												 geometries_(geometries)
	{
		buildGeometryInfo_.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildGeometryInfo_.flags = flags_;
		buildGeometryInfo_.geometryCount = static_cast<uint32_t>(geometries_.Geometry().size());
		buildGeometryInfo_.pGeometries = geometries_.Geometry().data();
		buildGeometryInfo_.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildGeometryInfo_.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildGeometryInfo_.srcAccelerationStructure = nullptr;

		retrieve_triangles();

		std::vector<uint32_t> maxPrimCount(geometries_.BuildOffsetInfo().size());

		for (size_t i = 0; i != maxPrimCount.size(); ++i)
		{
			maxPrimCount[i] = geometries_.BuildOffsetInfo()[i].primitiveCount;
			printf("(ycpin) maxPrimCount[%ld] = %d\n", i, maxPrimCount[i]);
		}

		buildSizesInfo_ = GetBuildSizes(maxPrimCount.data());
	}

	BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(BottomLevelAccelerationStructure &&other) noexcept : AccelerationStructure(std::move(other)),
																															geometries_(std::move(other.geometries_))
	{
	}

	BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
	{
	}

	void BottomLevelAccelerationStructure::Generate(
		VkCommandBuffer commandBuffer,
		CommandPool &commandPool,
		Buffer &scratchBuffer,
		const VkDeviceSize scratchOffset,
		Buffer &resultBuffer,
		const VkDeviceSize resultOffset,
		const size_t geometry_id)
	{
		// Create the acceleration structure
		CreateAccelerationStructure(resultBuffer, resultOffset);

		// Set up build geometry info
		const auto *pBuildOffsetInfo = geometries_.BuildOffsetInfo().data();
		buildGeometryInfo_.dstAccelerationStructure = Handle();
		buildGeometryInfo_.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress() + scratchOffset;
		printf("RTV: Building BLAS into %p\n", buildGeometryInfo_.dstAccelerationStructure);

		// Build the bottom - level acceleration structure(BLAS)
		deviceProcedures_.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo_, &pBuildOffsetInfo);

		// Build the BVH and convert to VSIM BVH
		bvh_t bvh = build_bvh(trigs);
		size_t node_count = bvh.node_count;
		printf("(ycpin) Build BVH, node_count = %ld\n", node_count);

		float t_trv_int = 0.5;
		float t_switch = 1;
		float t_ist = 1;

		int_bvh_t int_bvh = build_int_bvh(t_trv_int, t_switch, t_ist, trigs, bvh);
		int_bvh_v2_t int_bvh_v2 = convert_nodes(int_bvh, bvh, trigs);
		printf("(ycpin) Build INT BVH, Clustering...\n");

		check_correctness(bvh, int_bvh);
		printf("(ycpin) Check correctness\n");

		check_correctness_v2(bvh, int_bvh_v2);
		printf("(ycpin) Check correctness v2\n");

		create_int_bvh_buffer(commandPool, bvh, int_bvh_v2);
		printf("(ycpin) Create buffer & device memory for INT BVH\n");
	}

	void BottomLevelAccelerationStructure::retrieve_triangles()
	{
		const auto &geometries = geometries_.Geometry();
		const auto &buildOffsetInfos = geometries_.BuildOffsetInfo();

		printf("(ycpin) Geometries size: %ld\n", geometries.size());

		for (size_t i = 0; i < geometries.size(); ++i)
		{
			const auto &geometry = geometries[i];
			const auto &buildOffsetInfo = buildOffsetInfos[i];

			// Check if the geometry is a triangle
			if (geometry.geometryType != VK_GEOMETRY_TYPE_TRIANGLES_KHR)
			{
				printf("(ycpin) Geometry %zu is not a triangle geometry!\n", i);
				continue;
			}

			const auto &triangleData = geometry.geometry.triangles;

			const float *vertexData = reinterpret_cast<const float *>(triangleData.vertexData.deviceAddress);
			VkDeviceSize vertexStride = triangleData.vertexStride;

			const void *indexData = reinterpret_cast<const void *>(triangleData.indexData.deviceAddress);
			VkIndexType indexType = triangleData.indexType;

			// Currently, only handle UINT32 index type
			if (indexType != VK_INDEX_TYPE_UINT32)
			{
				printf("(ycpin) Unsupported index type!\n");
				continue;
			}

			const uint32_t *indices = reinterpret_cast<const uint32_t *>(indexData);

			for (uint32_t j = 0; j < buildOffsetInfo.primitiveCount; ++j)
			{
				std::array<vector_t, 3> triangleVertices;

				// Fetch triangle vertices
				// printf("Triangle %d:\n", j);

				for (uint32_t k = 0; k < 3; ++k)
				{
					uint32_t index = indices[j * 3 + k];
					const float *vertex = reinterpret_cast<const float *>(
						reinterpret_cast<const char *>(vertexData) + index * vertexStride);

					triangleVertices[k] = vector_t(vertex[0], vertex[1], vertex[2]);

					// Print the triangle vertex information
					// printf("  Vertex %d: (%f, %f, %f)\n", k, vertex[0], vertex[1], vertex[2]);
				}

				// Add the triangle to the list
				trigs.emplace_back(triangleVertices[0], triangleVertices[1], triangleVertices[2]);
			}
		}
	}

	void BottomLevelAccelerationStructure::check_correctness(bvh::Bvh<float> &bvh, int_bvh_t &int_bvh)
	{
		intmax_t correct_rays = 0;
		intmax_t total_rays = 0;

		traverser_t full_traverser(bvh);
		primitive_intersector_t primitive_intersector(bvh, trigs.data());
		traverser_t::Statistics full_statistics;
		statistics_t int_statistics;
		std::ifstream ray_fs("../../../assets/ray/kitchen.ray");

		for (float r[7]; ray_fs.read((char *)r, 7 * sizeof(float)); total_rays++)
		{
			ray_t ray(
				vector_t(r[0], r[1], r[2]),
				vector_t(r[3], r[4], r[5]),
				0.f,
				r[6]);

			auto full_result = full_traverser.traverse(ray, primitive_intersector, full_statistics);
			auto int_result = int_traverse(int_bvh, trigs.data(), ray, int_statistics);

			if (full_result.has_value())
			{
				if (int_result.has_value() &&
					int_result->t == full_result->intersection.t &&
					int_result->u == full_result->intersection.u &&
					int_result->v == full_result->intersection.v)
					correct_rays++;
			}
			else if (!int_result.has_value())
			{
				correct_rays++;
			}
		}

		std::cout << "  (vanilla)" << std::endl;
		std::cout << "    traversal_steps: " << full_statistics.traversal_steps << std::endl;
		std::cout << "    both_intersected: " << full_statistics.both_intersected << std::endl;
		std::cout << "    intersections_a: " << full_statistics.intersections_a << std::endl;
		std::cout << "    intersections_b: " << full_statistics.intersections_b << std::endl;
		std::cout << "    finalize: " << full_statistics.finalize << std::endl;

		std::cout << "  (quantized)" << std::endl;
		std::cout << "    intersect_bbox: " << int_statistics.intersect_bbox << std::endl;
		std::cout << "    push_cluster: " << int_statistics.push_cluster << std::endl;
		std::cout << "    recompute_qymax: " << int_statistics.recompute_qymax << std::endl;
		std::cout << "    traversal_steps: " << int_statistics.traversal_steps << std::endl;
		std::cout << "    both_intersected: " << int_statistics.both_intersected << std::endl;
		std::cout << "    intersections_a: " << int_statistics.bvh_statistics.intersections_a << std::endl;
		std::cout << "    intersections_b: " << int_statistics.bvh_statistics.intersections_b << std::endl;
		std::cout << "    finalize: " << int_statistics.finalize << std::endl;

		std::cout << "  total_rays: " << total_rays << std::endl;
		std::cout << "  correct_rays: " << correct_rays << std::endl;
	}

	void BottomLevelAccelerationStructure::check_correctness_v2(bvh::Bvh<float> &bvh, int_bvh_v2_t &int_bvh_v2)
	{
		intmax_t correct_rays = 0;
		intmax_t total_rays = 0;

		traverser_t full_traverser(bvh);
		primitive_intersector_t primitive_intersector(bvh, trigs.data());
		traverser_t::Statistics full_statistics;
		statistics_t int_statistics;
		std::ifstream ray_fs("../../../assets/ray/kitchen.ray");

		for (float r[7]; ray_fs.read((char *)r, 7 * sizeof(float)); total_rays++)
		{
			ray_t ray(
				vector_t(r[0], r[1], r[2]),
				vector_t(r[3], r[4], r[5]),
				0.f,
				r[6]);

			auto full_result = full_traverser.traverse(ray, primitive_intersector, full_statistics);
			auto int_result = int_traverse_v2(int_bvh_v2, trigs.data(), ray, int_statistics);

			if (full_result.has_value())
			{
				if (int_result.has_value() &&
					int_result->t == full_result->intersection.t &&
					int_result->u == full_result->intersection.u &&
					int_result->v == full_result->intersection.v)
					correct_rays++;
			}
			else if (!int_result.has_value())
			{
				correct_rays++;
			}
		}

		std::cout << "  (vanilla)" << std::endl;
		std::cout << "    traversal_steps: " << full_statistics.traversal_steps << std::endl;
		std::cout << "    both_intersected: " << full_statistics.both_intersected << std::endl;
		std::cout << "    intersections_a: " << full_statistics.intersections_a << std::endl;
		std::cout << "    intersections_b: " << full_statistics.intersections_b << std::endl;
		std::cout << "    finalize: " << full_statistics.finalize << std::endl;

		std::cout << "  (quantized)" << std::endl;
		std::cout << "    intersect_bbox: " << int_statistics.intersect_bbox << std::endl;
		std::cout << "    push_cluster: " << int_statistics.push_cluster << std::endl;
		std::cout << "    recompute_qymax: " << int_statistics.recompute_qymax << std::endl;
		std::cout << "    traversal_steps: " << int_statistics.traversal_steps << std::endl;
		std::cout << "    both_intersected: " << int_statistics.both_intersected << std::endl;
		std::cout << "    intersections_a: " << int_statistics.bvh_statistics.intersections_a << std::endl;
		std::cout << "    intersections_b: " << int_statistics.bvh_statistics.intersections_b << std::endl;
		std::cout << "    finalize: " << int_statistics.finalize << std::endl;

		std::cout << "  total_rays: " << total_rays << std::endl;
		std::cout << "  correct_rays: " << correct_rays << std::endl;
	}

	void BottomLevelAccelerationStructure::create_int_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh, int_bvh_v2_t &int_bvh)
	{
		int_bvh_clusters_Buffer_.reset();
		int_bvh_clusters_BufferMemory_.reset();
		int_bvh_trigs_Buffer_.reset();
		int_bvh_trigs_BufferMemory_.reset();
		int_bvh_nodes_Buffer_.reset();
		int_bvh_nodes_BufferMemory_.reset();
		int_bvh_primitive_indices_Buffer_.reset();
		int_bvh_primitive_indices_BufferMemory_.reset();

		constexpr auto flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
							   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		// Create buffer for clusters
		if (int_bvh.clusters)
		{
			std::cout << "(ycpin) Size of int_bvh_clusters: " << int_bvh.num_clusters << std::endl;
			std::vector<int_cluster_t> clustersVector;

			for (size_t i = 0; i < int_bvh.num_clusters; ++i)
			{
				auto &item = int_bvh.clusters[i];
				clustersVector.emplace_back(item);
			}

			Vulkan::BufferUtil::CreateDeviceBuffer(
				commandPool,
				"int_bvh_clusters",
				flags,
				clustersVector,
				int_bvh_clusters_Buffer_,
				int_bvh_clusters_BufferMemory_);
		}

		// Create buffer for trigs
		if (int_bvh.trigs)
		{
			std::cout << "(ycpin) Size of int_bvh_trigs: " << trigs.size() << std::endl;
			std::vector<triangle_t> trigsVector;

			for (size_t i = 0; i < trigs.size(); ++i)
			{
				auto &item = int_bvh.trigs[i];
				triangle_t triangle;

				triangle.v[0][0] = item.p0[0];
				triangle.v[0][1] = item.p0[1];
				triangle.v[0][2] = item.p0[2];
				triangle.v[1][0] = item.p1()[0];
				triangle.v[1][1] = item.p1()[1];
				triangle.v[1][2] = item.p1()[2];
				triangle.v[2][0] = item.p2()[0];
				triangle.v[2][1] = item.p2()[1];
				triangle.v[2][2] = item.p2()[2];

				trigsVector.emplace_back(triangle);
			}

			Vulkan::BufferUtil::CreateDeviceBuffer(
				commandPool,
				"int_bvh_trigs",
				flags,
				trigsVector,
				int_bvh_trigs_Buffer_,
				int_bvh_trigs_BufferMemory_);
		}

		// Create buffer for nodes
		if (int_bvh.nodes_v2)
		{
			std::cout << "(ycpin) Size of int_bvh_nodes: " << bvh.node_count << std::endl;
			std::vector<int_node_v2_t> nodesVector;

			for (size_t i = 0; i < bvh.node_count; ++i)
			{
				auto &item = int_bvh.nodes_v2[i];
				nodesVector.emplace_back(item);
			}

			Vulkan::BufferUtil::CreateDeviceBuffer(
				commandPool,
				"int_bvh_nodes",
				flags,
				nodesVector,
				int_bvh_nodes_Buffer_,
				int_bvh_nodes_BufferMemory_);
		}

		// Create buffer for primitive indices
		if (int_bvh.primitive_indices)
		{
			std::cout << "(ycpin) Size of int_bvh_primitive_indices: " << trigs.size() << std::endl;
			std::vector<uint32_t> primitiveIndicesVector;

			for (size_t i = 0; i < trigs.size(); ++i)
			{
				auto &item = int_bvh.primitive_indices[i];
				primitiveIndicesVector.emplace_back((uint32_t)item);
			}

			Vulkan::BufferUtil::CreateDeviceBuffer(
				commandPool,
				"int_bvh_primitive_indices",
				flags,
				primitiveIndicesVector,
				int_bvh_primitive_indices_Buffer_,
				int_bvh_primitive_indices_BufferMemory_);
		}
	}
}
