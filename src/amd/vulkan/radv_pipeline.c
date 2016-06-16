#include "util/mesa-sha1.h"
#include "radv_private.h"

VkResult radv_CreateShaderModule(
    VkDevice                                    _device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkShaderModule*                             pShaderModule)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_shader_module *module;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);

   module = radv_alloc2(&device->alloc, pAllocator,
                       sizeof(*module) + pCreateInfo->codeSize, 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (module == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   module->size = pCreateInfo->codeSize;
   memcpy(module->data, pCreateInfo->pCode, module->size);

   _mesa_sha1_compute(module->data, module->size, module->sha1);

   *pShaderModule = radv_shader_module_to_handle(module);

   return VK_SUCCESS;
}

void radv_DestroyShaderModule(
    VkDevice                                    _device,
    VkShaderModule                              _module,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_shader_module, module, _module);

   radv_free2(&device->alloc, pAllocator, module);
}

void radv_DestroyPipeline(
    VkDevice                                    _device,
    VkPipeline                                  _pipeline,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);
#if 0
   radv_reloc_list_finish(&pipeline->batch_relocs,
                         pAllocator ? pAllocator : &device->alloc);
   if (pipeline->blend_state.map)
      radv_state_pool_free(&device->dynamic_state_pool, pipeline->blend_state);
#endif
   radv_free2(&device->alloc, pAllocator, pipeline);
}

VkResult
radv_graphics_pipeline_create(
   VkDevice _device,
   VkPipelineCache _cache,
   const VkGraphicsPipelineCreateInfo *pCreateInfo,
   const struct radv_graphics_pipeline_create_info *extra,
   const VkAllocationCallbacks *pAllocator,
   VkPipeline *pPipeline)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_pipeline_cache, cache, _cache);

   //   if (cache == NULL)
   //      cache = &device->default_pipeline_cache;

}

VkResult radv_CreateGraphicsPipelines(
    VkDevice                                    _device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
   VkResult result = VK_SUCCESS;

      unsigned i = 0;
   for (; i < count; i++) {
      result = radv_graphics_pipeline_create(_device,
                                            pipelineCache,
                                            &pCreateInfos[i],
                                            NULL, pAllocator, &pPipelines[i]);
      if (result != VK_SUCCESS) {
         for (unsigned j = 0; j < i; j++) {
            radv_DestroyPipeline(_device, pPipelines[j], pAllocator);
         }

         return result;
      }
   }

   return VK_SUCCESS;
}

static VkResult radv_compute_pipeline_create(
    VkDevice                                    _device,
    VkPipelineCache                             _cache,
    const VkComputePipelineCreateInfo*          pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipeline)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_pipeline_cache, cache, _cache);

   //   if (cache == NULL)
   //      cache = &device->default_pipeline_cache;

}
VkResult radv_CreateComputePipelines(
    VkDevice                                    _device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
   VkResult result = VK_SUCCESS;

   unsigned i = 0;
   for (; i < count; i++) {
      result = radv_compute_pipeline_create(_device, pipelineCache,
                                           &pCreateInfos[i],
                                           pAllocator, &pPipelines[i]);
      if (result != VK_SUCCESS) {
         for (unsigned j = 0; j < i; j++) {
            radv_DestroyPipeline(_device, pPipelines[j], pAllocator);
         }

         return result;
      }
   }

   return VK_SUCCESS;
}
