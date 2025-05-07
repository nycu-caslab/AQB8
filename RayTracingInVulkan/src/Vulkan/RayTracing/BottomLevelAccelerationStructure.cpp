#include "BottomLevelAccelerationStructure.hpp"
#include "DeviceProcedures.hpp"
#include "Assets/Scene.hpp"
#include "Assets/Vertex.hpp"
#include "Utilities/Exception.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/BufferUtil.hpp"

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

		// Build the compressed BVH
		auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(trigs.data(), trigs.size());
		auto global_bbox = bvh::compute_bounding_boxes_union(bboxes.get(), trigs.size());

		bvh_t bvh_quant;
		builder_t builder_quant(bvh_quant);
		builder_quant.max_leaf_size = max_trig_in_leaf_size;
		builder_quant.build(global_bbox, bboxes.get(), centers.get(), trigs.size());

		std::queue<size_t> queue;
		queue.emplace(0);

		while (!queue.empty())
		{
			size_t curr_idx = queue.front();
			node_t &curr_node = bvh_quant.nodes[curr_idx];
			queue.pop();

			if (curr_node.is_leaf())
				continue;

			size_t left_idx = curr_node.first_child_or_primitive;
			node_t &left_node = bvh_quant.nodes[left_idx];
			size_t right_idx = left_idx + 1;
			node_t &right_node = bvh_quant.nodes[right_idx];

			bbox_t curr_bbox = curr_node.bounding_box_proxy().to_bounding_box();

			float lower_bound[3] = {
				curr_bbox.min[0],
				curr_bbox.min[1],
				curr_bbox.min[2]};

			float upper_bound[3] = {
				curr_bbox.max[0],
				curr_bbox.max[1],
				curr_bbox.max[2]};

			float extent[3] = {
				upper_bound[0] - lower_bound[0],
				upper_bound[1] - lower_bound[1],
				upper_bound[2] - lower_bound[2]};

			for (int i = 0; i < 3; i++)
			{
				if (extent[i] == 0.0f)
					extent[i] = 1.0f;

				int exp;
				float m = frexp(extent[i], &exp);

				if (m > 255.0f / 256.0f)
					exp++;

				curr_node.exp[i] = std::clamp(exp, INT8_MIN, INT8_MAX);
			}

			for (auto &node : std::array<node_t *, 2>{&left_node, &right_node})
			{
				for (int i = 0; i < 3; i++)
				{
					// 1. 計算縮放因子
					float scale = ldexpf(1.0f, 8 - curr_node.exp[i]);

					// 2. 量化 (Quantization)
					float bound_quant_min_fp32 = floorf((node->bounds[i * 2] - lower_bound[i]) * scale);
					float bound_quant_max_fp32 = ceilf((node->bounds[i * 2 + 1] - lower_bound[i]) * scale);

					// 3. 確保 `bounds_quant` 在合法範圍
					assert(bound_quant_min_fp32 >= 0.0f && bound_quant_min_fp32 <= 255.0f);
					assert(bound_quant_max_fp32 >= 0.0f && bound_quant_max_fp32 <= 255.0f);

					// 4. 解壓縮 (Dequantization)
					float old_bound_min = node->bounds[i * 2];
					float old_bound_max = node->bounds[i * 2 + 1];

					node->bounds[i * 2] = lower_bound[i] + bound_quant_min_fp32 / scale;
					node->bounds[i * 2 + 1] = lower_bound[i] + bound_quant_max_fp32 / scale;

					// 5. 確保量化後的範圍仍然覆蓋原始範圍
					assert(node->bounds[i * 2] <= old_bound_min + 1e-5f);
					assert(node->bounds[i * 2 + 1] >= old_bound_max - 1e-5f);

					// 6. 儲存量化數值
					node->bounds_quant[i * 2] = static_cast<uint8_t>(bound_quant_min_fp32);
					node->bounds_quant[i * 2 + 1] = static_cast<uint8_t>(bound_quant_max_fp32);
				}
			}

			queue.emplace(left_idx);
			queue.emplace(right_idx);
		}

		bvh_t bvh;
		builder_t builder(bvh);
		builder.max_leaf_size = max_trig_in_leaf_size;
		builder.build(global_bbox, bboxes.get(), centers.get(), trigs.size());

		bvh_quant.convert_nodes(bvh_quant.nodes, bvh_quant.node_count);
		std::cout << "(ycpin) BVH node count: " << bvh.node_count << std::endl;
		std::cout << "(ycpin) BVH_v2 node count: " << bvh_quant.node_count_v2 << std::endl;

		check_correctness(bvh, bvh_quant);
		printf("(ycpin) Check correctness\n");

		create_quant_bvh_buffer(commandPool, bvh_quant);
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

	void BottomLevelAccelerationStructure::check_correctness(bvh::Bvh<float> &bvh, bvh::Bvh<float> &bvh_quant)
	{
		traverser_t traverser(bvh);
		traverser_quant_t traverser_quant(bvh_quant);

		primitive_intersector_t primitive_intersector(bvh, trigs.data());
		primitive_intersector_t primitive_intersector_quant(bvh_quant, trigs.data());

		traverser_t::Statistics statistics;
		traverser_quant_t::Statistics statistics_quant;

		intmax_t correct_rays = 0;
		intmax_t total_rays = 0;

		std::ifstream ray_fs("../../../assets/ray/kitchen.ray");
		assert(ray_fs.is_open());

		for (float r[7]; ray_fs.read((char *)r, 7 * sizeof(float)); total_rays++)
		{
			// std::cout << total_rays << std::endl;

			float magnitude = std::sqrt(r[3] * r[3] +
										r[4] * r[4] +
										r[5] * r[5]);
			r[3] /= magnitude;
			r[4] /= magnitude;
			r[5] /= magnitude;

			ray_t ray(
				vector_t(r[0], r[1], r[2]),
				vector_t(r[3], r[4], r[5]),
				0.f,
				r[6]);

			// std::cout << r[0] << ", " << r[1] << ", " << r[2] << std::endl;
			// std::cout << r[3] << ", " << r[4] << ", " << r[5] << std::endl;

			auto result = traverser.traverse(ray, primitive_intersector, statistics);
			auto result_quant = traverser_quant.traverse(ray, primitive_intersector_quant, statistics_quant);

			if (result.has_value())
			{
				if (result_quant.has_value() &&
					result_quant->intersection.t == result->intersection.t &&
					result_quant->intersection.u == result->intersection.u &&
					result_quant->intersection.v == result->intersection.v)
					correct_rays++;
			}
			else if (!result_quant.has_value())
			{
				correct_rays++;
			}
		}

		std::cout << "(vanilla)" << std::endl;
		std::cout << "  traversal_steps: " << statistics.traversal_steps << std::endl;
		std::cout << "  both_intersected: " << statistics.both_intersected << std::endl;
		std::cout << "  intersections_a: " << statistics.intersections_a << std::endl;
		std::cout << "  intersections_b: " << statistics.intersections_b << std::endl;
		std::cout << "  finalize: " << statistics.finalize << std::endl;

		std::cout << "(compressed)" << std::endl;
		std::cout << "  traversal_steps: " << statistics_quant.traversal_steps << std::endl;
		std::cout << "  both_intersected: " << statistics_quant.both_intersected << std::endl;
		std::cout << "  intersections_a: " << statistics_quant.intersections_a << std::endl;
		std::cout << "  intersections_b: " << statistics_quant.intersections_b << std::endl;
		std::cout << "  finalize: " << statistics_quant.finalize << std::endl;

		std::cout << "total_rays: " << total_rays << std::endl;
		std::cout << "correct_rays: " << correct_rays << std::endl;
	}

	void BottomLevelAccelerationStructure::create_quant_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh_quant)
	{
		compress_bvh_trigs_Buffer_.reset();
		compress_bvh_trigs_BufferMemory_.reset();
		compress_bvh_nodes_Buffer_.reset();
		compress_bvh_nodes_BufferMemory_.reset();
		compress_bvh_primitive_indices_Buffer_.reset();
		compress_bvh_primitive_indices_BufferMemory_.reset();

		constexpr auto flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
							   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		// Create buffer for trigs
		std::cout << "(ycpin) Size of trigs: " << trigs.size() << std::endl;
		std::vector<compress_trig_t> trigsVector;

		for (size_t i = 0; i < trigs.size(); ++i)
		{
			auto &item = trigs[i];
			compress_trig_t triangle;

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
			"compress_bvh_trigs",
			flags,
			trigsVector,
			compress_bvh_trigs_Buffer_,
			compress_bvh_trigs_BufferMemory_);

		// Create buffer for nodes
		std::cout << "(ycpin) Size of compress_bvh_nodes: " << bvh_quant.node_count_v2 << std::endl;
		std::vector<compress_node_v2_t> nodesVector;

		for (size_t i = 0; i < bvh_quant.node_count_v2; ++i)
		{
			auto &item = bvh_quant.nodes_v2[i];
			nodesVector.emplace_back(item);
		}

		Vulkan::BufferUtil::CreateDeviceBuffer(
			commandPool,
			"compress_bvh_nodes",
			flags,
			nodesVector,
			compress_bvh_nodes_Buffer_,
			compress_bvh_nodes_BufferMemory_);

		// Create buffer for root node
		std::cout << "(ycpin) Size of compress_bvh_root: 1" << std::endl;
		std::vector<compress_root_t> rootVector;

		auto &item = bvh_quant.nodes[0];
		compress_root_t root_node;

		root_node.bounds[0] = (float)item.bounds[0];
		root_node.bounds[1] = (float)item.bounds[1];
		root_node.bounds[2] = (float)item.bounds[2];
		root_node.bounds[3] = (float)item.bounds[3];
		root_node.bounds[4] = (float)item.bounds[4];
		root_node.bounds[5] = (float)item.bounds[5];
		root_node.bounds_quant[0] = (uint8_t)item.bounds_quant[0];
		root_node.bounds_quant[1] = (uint8_t)item.bounds_quant[1];
		root_node.bounds_quant[2] = (uint8_t)item.bounds_quant[2];
		root_node.bounds_quant[3] = (uint8_t)item.bounds_quant[3];
		root_node.bounds_quant[4] = (uint8_t)item.bounds_quant[4];
		root_node.bounds_quant[5] = (uint8_t)item.bounds_quant[5];
		root_node.exp[0] = item.exp[0];
		root_node.exp[1] = item.exp[1];
		root_node.exp[2] = item.exp[2];
		root_node.primitive_count = (size_t)item.primitive_count;
		root_node.first_child_or_primitive = (size_t)item.first_child_or_primitive;
		rootVector.emplace_back(root_node);

		Vulkan::BufferUtil::CreateDeviceBuffer(
			commandPool,
			"compress_bvh_root",
			flags,
			rootVector,
			compress_bvh_root_Buffer_,
			compress_bvh_root_BufferMemory_);

		// Create buffer for primitive indices
		std::cout << "(ycpin) Size of compress_bvh_primitive_indices: " << trigs.size() << std::endl;
		std::vector<uint32_t> primitiveIndicesVector;

		for (size_t i = 0; i < trigs.size(); ++i)
		{
			auto &item = bvh_quant.primitive_indices[i];
			primitiveIndicesVector.emplace_back((uint32_t)item);
		}

		Vulkan::BufferUtil::CreateDeviceBuffer(
			commandPool,
			"compress_bvh_primitive_indices",
			flags,
			primitiveIndicesVector,
			compress_bvh_primitive_indices_Buffer_,
			compress_bvh_primitive_indices_BufferMemory_);
	}
}
