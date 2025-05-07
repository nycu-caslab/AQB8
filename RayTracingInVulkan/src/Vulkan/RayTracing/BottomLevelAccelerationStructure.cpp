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
		struct vsim_bvh_node *root = (struct vsim_bvh_node *)buildGeometryInfo_.pNext;

		// Collect the node and leaf data using BFS
		std::vector<uint64_t> collect_node;
		collect_data(root, collect_node);

		// Build BVH
		bvh_t bvh;
		size_t node_count = collect_node.size();
		size_t prim_count = trigs.size();

		std::vector<node_t> nodes(node_count);
		std::vector<size_t> primitive_indices(prim_count);

		convert_bvh(collect_node, nodes, primitive_indices);

		// Finalize the BVH structure with the nodes and primitive indices
		bvh.node_count = node_count;
		bvh.nodes = std::make_unique<node_t[]>(node_count);
		for (size_t i = 0; i < node_count; ++i)
		{
			bvh.nodes[i] = nodes[i];
		}
		bvh.primitive_indices = std::make_unique<size_t[]>(prim_count);
		for (size_t i = 0; i < prim_count; ++i)
		{
			bvh.primitive_indices[i] = primitive_indices[i];
		}

		reset_bvh_bounds(bvh);
		bvh.convert_nodes(bvh.nodes, bvh.node_count);
		std::cout << "(ycpin) BVH node count: " << bvh.node_count << std::endl;
		std::cout << "(ycpin) BVH_v2 node count: " << bvh.node_count_v2 << std::endl;

		// Build Original BVH
		auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(trigs.data(), trigs.size());
		auto global_bbox = bvh::compute_bounding_boxes_union(bboxes.get(), trigs.size());

		bvh_t bvh_original;
		builder_t builder(bvh_original);
		builder.max_leaf_size = max_trig_in_leaf_size;
		builder.build(global_bbox, bboxes.get(), centers.get(), trigs.size());

		check_correctness(bvh_original, bvh);
		printf("(ycpin) Check correctness\n");

		create_bvh_buffer(commandPool, bvh);
		printf("(ycpin) Create buffer & device memory BVH\n");
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

	void BottomLevelAccelerationStructure::collect_data(struct vsim_bvh_node *root,
														std::vector<uint64_t> &collect_node)
	{
		std::queue<vsim_bvh_node *> queue;
		queue.push(root);

		while (!queue.empty())
		{
			vsim_bvh_node *current = queue.front();
			queue.pop();

			collect_node.emplace_back((uint64_t)current);

			if (!current->is_leaf)
			{
				for (unsigned i = 0; i < 6; i++)
				{
					if (current->children[i])
					{
						queue.push(current->children[i]);
					}
				}
			}
		}
	}

	void BottomLevelAccelerationStructure::convert_bvh(std::vector<uint64_t> &collect_node,
													   std::vector<node_t> &nodes,
													   std::vector<size_t> &primitive_indices)
	{
		int node_count = collect_node.size();
		size_t leaf_i = 0;

		std::unordered_map<uint64_t, size_t> node_index;
		std::unordered_map<uint64_t, size_t> leaf_index;

		for (size_t id = 0; id < node_count; ++id)
		{
			vsim_bvh_node *bvh_node = reinterpret_cast<vsim_bvh_node *>(collect_node[id]);

			if (node_index.find(collect_node[id]) != node_index.end())
				std::cerr << "Duplicate node found!" << std::endl;

			node_index[collect_node[id]] = id;

			if (bvh_node->is_leaf)
			{
				vsim_bvh_leaf *leaf = reinterpret_cast<vsim_bvh_leaf *>(bvh_node);
				leaf_index[id] = leaf_i;

				for (size_t i = 0; i < leaf->primitive_count; ++i)
				{
					primitive_indices[leaf_i] = leaf->primitive_index[i];
					leaf_i++;
				}
			}

			node_t &node = nodes[id];
			memset(&node, 0, sizeof(node_t));
		}

		for (size_t id = 0; id < node_count; ++id)
		{
			node_t &node = nodes[id];
			vsim_bvh_node *bvh_node = reinterpret_cast<vsim_bvh_node *>(collect_node[id]);

			if (bvh_node->is_leaf)
			{
				continue;
			}

			uint8_t max_child_x = 0, max_child_y = 0, max_child_z = 0;
			node.first_child_or_primitive = -1;
			node.primitive_count = 0;
			node.is_leaf_val = false;

			for (size_t i = 0; i < 6; ++i)
			{
				if (bvh_node->children[i])
				{
					vsim_bvh_node *child = bvh_node->children[i];
					size_t index = node_index[reinterpret_cast<uint64_t>(child)];

					if (node.first_child_or_primitive == -1)
					{
						node.first_child_or_primitive = index;
					}

					if (child->is_leaf)
					{
						vsim_bvh_leaf *leaf = reinterpret_cast<vsim_bvh_leaf *>(child);

						nodes[index].first_child_or_primitive = leaf_index[index];
						nodes[index].primitive_count = leaf->primitive_count;
						nodes[index].is_leaf_val = true;
					}

					node.primitive_count += 1;
				}
			}
		}
	}

	void BottomLevelAccelerationStructure::reset_bvh_bounds(bvh::Bvh<float> &bvh)
	{
		if (bvh.node_count == 0)
			return;

		std::function<bvh::BoundingBox<float>(size_t)> reset_bounds_recursive =
			[&](size_t node_index) -> bvh::BoundingBox<float>
		{
			auto &node = bvh.nodes[node_index];

			if (node.is_leaf())
			{
				bbox_t bbox = bbox_t::empty();

				for (size_t j = 0; j < node.primitive_count; ++j)
				{
					size_t trig_idx = bvh.primitive_indices[node.first_child_or_primitive + j];
					bbox.extend(trigs[trig_idx].bounding_box());
				}

				node.bounding_box_proxy() = bbox;
				return bbox;
			}

			bbox_t bbox = bbox_t::empty();

			for (size_t i = 0; i < node.primitive_count; ++i)
			{
				size_t child_index = node.first_child_or_primitive + i;
				bbox.extend(reset_bounds_recursive(child_index)); // 遞迴計算子節點邊界
			}

			node.bounding_box_proxy() = bbox;
			return bbox;
		};

		reset_bounds_recursive(0);
	}

	void BottomLevelAccelerationStructure::check_correctness(bvh::Bvh<float> &bvh_original, bvh::Bvh<float> &bvh)
	{
		traverser_t traverser(bvh_original);
		traverser_v2_t traverser_v2(bvh);

		primitive_intersector_t primitive_intersector(bvh_original, trigs.data());
		primitive_intersector_t primitive_intersector_v2(bvh, trigs.data());

		traverser_t::Statistics statistics;
		traverser_v2_t::Statistics statistics_v2;

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
			auto result_v2 = traverser_v2.traverse(ray, primitive_intersector_v2, statistics_v2);

			if (result.has_value())
			{
				if (result_v2.has_value() &&
					result_v2->intersection.t == result->intersection.t &&
					result_v2->intersection.u == result->intersection.u &&
					result_v2->intersection.v == result->intersection.v)
					correct_rays++;
			}
			else if (!result_v2.has_value())
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

		std::cout << "(v2)" << std::endl;
		std::cout << "  traversal_steps: " << statistics_v2.traversal_steps << std::endl;
		std::cout << "  both_intersected: " << statistics_v2.both_intersected << std::endl;
		std::cout << "  intersections_a: " << statistics_v2.intersections_a << std::endl;
		std::cout << "  intersections_b: " << statistics_v2.intersections_b << std::endl;
		std::cout << "  finalize: " << statistics_v2.finalize << std::endl;

		std::cout << "total_rays: " << total_rays << std::endl;
		std::cout << "correct_rays: " << correct_rays << std::endl;
	}

	void BottomLevelAccelerationStructure::create_bvh_buffer(CommandPool &commandPool, bvh::Bvh<float> &bvh)
	{
		bvh_trigs_Buffer_.reset();
		bvh_trigs_BufferMemory_.reset();
		bvh_nodes_Buffer_.reset();
		bvh_nodes_BufferMemory_.reset();
		bvh_primitive_indices_Buffer_.reset();
		bvh_primitive_indices_BufferMemory_.reset();

		constexpr auto flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
							   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		// Create buffer for trigs
		std::cout << "(ycpin) Size of trigs: " << trigs.size() << std::endl;
		std::vector<triangle_t> trigsVector;

		for (size_t i = 0; i < trigs.size(); ++i)
		{
			auto &item = trigs[i];
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
			"bvh_trigs",
			flags,
			trigsVector,
			bvh_trigs_Buffer_,
			bvh_trigs_BufferMemory_);

		// Create buffer for nodes
		std::cout << "(ycpin) Size of bvh_nodes: " << bvh.node_count_v2 << std::endl;
		std::vector<node_v2_t> nodesVector;

		for (size_t i = 0; i < bvh.node_count_v2; ++i)
		{
			auto &item = bvh.nodes_v2[i];
			nodesVector.emplace_back(item);
		}

		Vulkan::BufferUtil::CreateDeviceBuffer(
			commandPool,
			"bvh_nodes",
			flags,
			nodesVector,
			bvh_nodes_Buffer_,
			bvh_nodes_BufferMemory_);

		// Create buffer for primitive indices
		std::cout << "(ycpin) Size of bvh_primitive_indices: " << trigs.size() << std::endl;
		std::vector<uint32_t> primitiveIndicesVector;

		for (size_t i = 0; i < trigs.size(); ++i)
		{
			auto &item = bvh.primitive_indices[i];
			primitiveIndicesVector.emplace_back((uint32_t)item);
		}

		Vulkan::BufferUtil::CreateDeviceBuffer(
			commandPool,
			"bvh_primitive_indices",
			flags,
			primitiveIndicesVector,
			bvh_primitive_indices_Buffer_,
			bvh_primitive_indices_BufferMemory_);
	}
}
