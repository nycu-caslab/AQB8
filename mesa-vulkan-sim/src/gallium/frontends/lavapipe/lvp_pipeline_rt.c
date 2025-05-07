#include "nir/nir.h"
#include "lvp_pipeline.h"
#include "lvp_private.h"
#include "vk_pipeline.h"
#include "vk_pipeline_cache.h"

#include "gpgpusim_calls_from_mesa.h"

struct vsim_pipeline_stage {
   gl_shader_stage stage;
   const VkPipelineShaderStageCreateInfo *info;
   nir_shader *nir;
   const char *entrypoint;
   struct anv_shader_bin *bin;
   const VkSpecializationInfo *spec_info;
};


static bool gpgpusim_initialized = false;
static int shader_ID = 0;

static void translate_nir_to_ptx(nir_shader *shader, char* shaderPath)
{
   FILE *pFile;
   char *mesa_root = getenv("MESA_ROOT");
   char *filePath = "gpgpusimShaders/";
   char fileName[50];
   char *label; // in case there are multiple variants of the same shader
   char *extension = ".ptx";
   
   label = shader->info.label;
   if (!label){
      label = "0";
   }

   switch (shader->info.stage) {
      case MESA_SHADER_RAYGEN:
         strcpy(fileName, "MESA_SHADER_RAYGEN");
         break;
      case MESA_SHADER_ANY_HIT:
         strcpy(fileName, "MESA_SHADER_ANY_HIT");
         break;
      case MESA_SHADER_CLOSEST_HIT:
         strcpy(fileName, "MESA_SHADER_CLOSEST_HIT");
         break;
      case MESA_SHADER_MISS:
         strcpy(fileName, "MESA_SHADER_MISS");
         break;
      case MESA_SHADER_INTERSECTION:
         strcpy(fileName, "MESA_SHADER_INTERSECTION");
         break;
      case MESA_SHADER_CALLABLE:
         strcpy(fileName, "MESA_SHADER_CALLABLE");
         break;
      default:
         unreachable("Invalid shader type");
   }

   char fullPath[200];
   snprintf(fullPath, sizeof(fullPath), "%s%s%s_%d%s", mesa_root, filePath, fileName, shader_ID++, extension);
   
   char command[200];

   if(!gpgpusim_initialized){
      snprintf(command, sizeof(command), "rm -rf %s%s", mesa_root, filePath);
      system(command);
      gpgpusim_initialized = true;
   }

   snprintf(command, sizeof(command), "mkdir -p %s%s", mesa_root, filePath);
   system(command);
   
   pFile = fopen (fullPath , "w");
   printf("GPGPU-SIM VULKAN: Translating NIR %s to PTX\n", fileName);
   nir_translate_shader_to_ptx(shader, pFile, fullPath);

   strcpy(shaderPath, fullPath);
}

static void run_rt_translation_passes()
{
   char *mesa_root = getenv("MESA_ROOT");
   char *filePath = "gpgpusimShaders/";

   char command[400];
   snprintf(command, sizeof(command), "python3 %s/src/compiler/ptx/ptx_lower_instructions.py %s%s", mesa_root, mesa_root, filePath);
   int result = system(command);

   if (result != 0)
   {
      printf("MESA: ERROR ** while translating nir to PTX %d\n", result);
      exit(1);
   }
}

static nir_shader *
vsim_pipeline_stage_get_nir(
   struct lvp_pipeline *pipeline,
   const VkPipelineShaderStageCreateInfo *sinfo)
{
   nir_shader *nir;

   nir = vsim_shader_spirv_to_nir(pipeline, sinfo);
   if (nir) {
      return nir;
   }

   return NULL;
}

static VkResult
vsim_compile_ray_tracing_pipeline(
   struct lvp_pipeline *pipeline,
   const VkRayTracingPipelineCreateInfoKHR *info)
{
   printf("LVP: Compiling ray tracing pipeline...\n");
   VkResult result;
   LVP_FROM_HANDLE(lvp_pipeline_layout, layout, info->layout);

   void *pipeline_ctx = ralloc_context(NULL);
   struct vsim_pipeline_stage *stages =
      rzalloc_array(pipeline_ctx, struct vsim_pipeline_stage, info->stageCount);

   char shaderPaths[20][200];
   for (uint32_t i = 0; i < info->stageCount; i++) {
      printf("LVP: Compiling shader stage %d\n", i);
      const VkPipelineShaderStageCreateInfo *sinfo = &info->pStages[i];
      if (sinfo->module == VK_NULL_HANDLE)
         continue;

      stages[i] = (struct vsim_pipeline_stage) {
         .stage = vk_to_mesa_shader_stage(sinfo->stage),
         .entrypoint = sinfo->pName,
         .spec_info = sinfo->pSpecializationInfo,
      };

      stages[i].nir = vsim_pipeline_stage_get_nir(pipeline, sinfo);

      if (stages[i].nir == NULL) {
         printf("LVP: NIR missing\n");
         ralloc_free(pipeline_ctx);
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }

      // Insert NIR to PTX translator here for each different ray tracing shaders, the lowered shaders under have too many intel specific intrinsics
      if(stages[i].stage >= MESA_SHADER_RAYGEN && stages[i].stage <= MESA_SHADER_CALLABLE) { // shader type from 8 to 13
         printf("LVP: Translating shader %d (type %d)\n", i, stages[i].stage);
         translate_nir_to_ptx(stages[i].nir, shaderPaths[i]);
      }
      pipeline->shaders[i].pipeline_nir = ralloc(NULL, struct lvp_pipeline_nir);
      pipeline->shaders[i].pipeline_nir->nir = stages[i].nir;
      pipeline->shaders[i].pipeline_nir->ref_cnt = 1;
   }

   // Vulkan-Sim additions
   printf("LVP: run_rt_translation_passes\n");
   run_rt_translation_passes();

   for (uint32_t i = 0; i < info->stageCount; i++) {
      if(stages[i].stage >= MESA_SHADER_RAYGEN && stages[i].stage <= MESA_SHADER_CALLABLE) {
         printf("LVP: Registering shader stage %d with GPGPU-Sim\n", i);
         stages[i].bin = (void *)gpgpusim_registerShader(shaderPaths[i], (uint32_t)(stages[i].stage));
         assert((uint64_t)(stages[i].bin) == i);
      }
   }

   return result;
}


static VkResult
lvp_ray_tracing_pipeline_create(
    VkDevice                                    _device,
    struct lvp_pipeline_cache *                  cache,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipeline)
{
   LVP_FROM_HANDLE(lvp_device, device, _device);
   LVP_FROM_HANDLE(lvp_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   VkResult result;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);

   // Create ray tracing pipeline
   struct lvp_pipeline *pipeline;
   pipeline = vk_zalloc(&device->vk.alloc, sizeof(*pipeline), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   pipeline->group_count = pCreateInfo->groupCount;
   pipeline->group_handles = vk_zalloc(&device->vk.alloc, 
         sizeof(*pipeline->group_handles) * pipeline->group_count,
         8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   
   
   if (pipeline == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &pipeline->base,
                       VK_OBJECT_TYPE_PIPELINE);

   result = lvp_ray_tracing_pipeline_init(pipeline, device, cache, pCreateInfo);
   if (result != VK_SUCCESS) {
      vk_free(&device->vk.alloc, pipeline);
      return result;
   }


   // Ray tracing shaders
   vsim_compile_ray_tracing_pipeline(pipeline, pCreateInfo);

   // Allocate memory for shader groups
   // Don't need the actual binary since GPGPU-Sim runs PTX

   // Need pipeline group handles
   for (unsigned i=0; i<pipeline->group_count; i++) {
      const VkRayTracingShaderGroupCreateInfoKHR *group_info = &pCreateInfo->pGroups[i];
      switch (group_info->type) {
      // TODO: AMD adds 2 to each index (not sure why...)
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
         printf("LVP: Adding group handle for general group: \n");
         if (group_info->generalShader != VK_SHADER_UNUSED_KHR) {
            pipeline->group_handles[i].general_index = group_info->generalShader;
            printf("\tgeneral_index %d\n", group_info->generalShader);
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
         printf("LVP: Adding group handle for procedural hit group: \n");
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            pipeline->group_handles[i].closest_hit_index = group_info->closestHitShader;
            printf("\tclosest_hit_index %d\n", group_info->closestHitShader);
         }
         if (group_info->intersectionShader != VK_SHADER_UNUSED_KHR) {
            pipeline->group_handles[i].intersection_index = group_info->intersectionShader;
            printf("\tintersection_index %d\n", group_info->intersectionShader);
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
         printf("LVP: Adding group handle for triangle hit group: \n");
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            pipeline->group_handles[i].closest_hit_index = group_info->closestHitShader;
            printf("\tclosest_hit_index %d\n", group_info->closestHitShader);
         }
         if (group_info->anyHitShader != VK_SHADER_UNUSED_KHR) {
            pipeline->group_handles[i].any_hit_index = group_info->anyHitShader;
            printf("\tany_hit_index %d\n", group_info->anyHitShader);
         }
         break;
      case VK_SHADER_GROUP_SHADER_MAX_ENUM_KHR:
         unreachable("VK_SHADER_GROUP_SHADER_MAX_ENUM_KHR");
         break;
      default:
         unreachable("Undefined hit group type");
         break;
      }
   }

   // TODO: Add VK_PIPELINE_CREATE_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR

   gpgpusim_setPipelineInfo(pCreateInfo);
   *pPipeline = lvp_pipeline_to_handle(pipeline);

   return result;
}


VkResult
lvp_CreateRayTracingPipelinesKHR(
    VkDevice                                    _device,
    VkDeferredOperationKHR                      deferredOperation,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
   printf("LVP: Creating ray tracing pipeline...\n");
   LVP_FROM_HANDLE(lvp_pipeline_cache, pipeline_cache, pipelineCache);

   // Assume only 1 pipeline currently
   if (createInfoCount > 1) {
      unreachable("Unimplemented");
   }

   VkResult result = lvp_ray_tracing_pipeline_create(_device, pipeline_cache,
                                                     &pCreateInfos[0],
                                                     pAllocator, &pPipelines[0]);

   return result;
}


VkResult
lvp_GetRayTracingShaderGroupHandlesKHR(
    VkDevice                                    device,
    VkPipeline                                  _pipeline,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData)
{
   printf("LVP: Get ray tracing shader group handles...\n");
   LVP_FROM_HANDLE(lvp_pipeline, pipeline, _pipeline);

   // Handle size is 32 (from GetDeviceProperties); matches Intel and AMD
   #define PIPELINE_HANDLE_SIZE 32
   assert(sizeof(*pipeline->group_handles) <= PIPELINE_HANDLE_SIZE);

   // Copy handles to pData
   memset(pData, 0, groupCount * PIPELINE_HANDLE_SIZE);
   for (unsigned i = 0; i < groupCount; i++) {
      printf("LVP: Copying handle %d to %p\n", i, pData + i * PIPELINE_HANDLE_SIZE);
      memcpy(pData + i * PIPELINE_HANDLE_SIZE, 
             &pipeline->group_handles[firstGroup + i], 
             sizeof(*pipeline->group_handles));
   }

   return VK_SUCCESS;
}