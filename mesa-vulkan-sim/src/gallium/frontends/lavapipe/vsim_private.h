#include "vk_buffer.h"

struct lvp_acceleration_structure {
   struct vk_object_base                        base;

   VkDeviceSize                                 size;
   struct anv_address                           address;
};

struct anv_batch {
   const VkAllocationCallbacks *                alloc;

   /**
    * Sum of all the anv_batch_bo sizes allocated for this command buffer.
    * Used to increase allocation size for long command buffers.
    */
   size_t                                       total_batch_size;

   struct anv_address                           start_addr;

   void *                                       start;
   void *                                       end;
   void *                                       next;

   struct anv_reloc_list *                      relocs;

   /* This callback is called (with the associated user data) in the event
    * that the batch runs out of space.
    */
   VkResult (*extend_cb)(struct anv_batch *, uint32_t, void *);
   void *                                       user_data;

   /**
    * Current error status of the command buffer. Used to track inconsistent
    * or incomplete command buffer states that are the consequence of run-time
    * errors such as out of memory scenarios. We want to track this in the
    * batch because the command buffer object is not visible to some parts
    * of the driver.
    */
   VkResult                                     status;
};

struct anv_buffer {
   struct vk_buffer vk;

   /* Set when bound */
   struct anv_address address;
};


static inline uint64_t
intel_canonical_address(uint64_t v)
{
   /* From the Broadwell PRM Vol. 2a, MI_LOAD_REGISTER_MEM::MemoryAddress:
    *
    *    "This field specifies the address of the memory location where the
    *    register value specified in the DWord above will read from. The
    *    address specifies the DWord location of the data. Range =
    *    GraphicsVirtualAddress[63:2] for a DWord register GraphicsAddress
    *    [63:48] are ignored by the HW and assumed to be in correct
    *    canonical form [63:48] == [47]."
    */
   const int shift = 63 - 47;
   return (int64_t)(v << shift) >> shift;
}

static inline struct anv_address
anv_address_add(struct anv_address addr, uint64_t offset)
{
   addr.offset += offset;
   return addr;
}

#define ANV_NULL_ADDRESS ((struct anv_address) { NULL, 0 })

static inline struct anv_address
anv_address_from_u64(uint64_t addr_u64)
{
   assert(addr_u64 == intel_canonical_address(addr_u64));
   return (struct anv_address) {
      .bo = NULL,
      .offset = addr_u64,
   };
}

static inline bool
anv_address_is_null(struct anv_address addr)
{
   return addr.bo == NULL && addr.offset == 0;
}


static inline uint64_t
anv_address_physical(struct anv_address addr)
{
   uint64_t address = (addr.bo ? addr.bo->offset : 0ull) + addr.offset;
   return intel_canonical_address(address);
}


static inline VkResult
lvp_batch_set_error(struct anv_batch *batch, VkResult error)
{
   assert(error != VK_SUCCESS);
   if (batch->status == VK_SUCCESS)
      batch->status = error;
   return batch->status;
}


static inline bool
lvp_batch_has_error(struct anv_batch *batch)
{
   return batch->status != VK_SUCCESS;
}


VkResult anv_reloc_list_add_bo(struct anv_reloc_list *list,
                               const VkAllocationCallbacks *alloc,
                               struct anv_bo *target_bo);

