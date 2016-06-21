#include "util/mesa-sha1.h"
#include "radv_private.h"
#include "nir/nir.h"
#include "spirv/nir_spirv.h"
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

   module->nir = NULL;
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

static nir_shader *
radv_shader_compile_to_nir(struct radv_device *device,
                          struct radv_shader_module *module,
                          const char *entrypoint_name,
                          gl_shader_stage stage,
                          const VkSpecializationInfo *spec_info)
{
   if (strcmp(entrypoint_name, "main") != 0) {
      radv_finishme("Multiple shaders per module not really supported");
   }

#if 0
   const struct brw_compiler *compiler =
      device->instance->physicalDevice.compiler;
   const nir_shader_compiler_options *nir_options =
      compiler->glsl_compiler_options[stage].NirOptions;
#endif
   const nir_shader_compiler_options *nir_options = NULL;
   nir_shader *nir;
   nir_function *entry_point;
   if (module->nir) {
      /* Some things such as our meta clear/blit code will give us a NIR
       * shader directly.  In that case, we just ignore the SPIR-V entirely
       * and just use the NIR shader */
      nir = module->nir;
      nir->options = nir_options;
      nir_validate_shader(nir);

      assert(exec_list_length(&nir->functions) == 1);
      struct exec_node *node = exec_list_get_head(&nir->functions);
      entry_point = exec_node_data(nir_function, node, node);
   } else {
      uint32_t *spirv = (uint32_t *) module->data;
      assert(module->size % 4 == 0);

      uint32_t num_spec_entries = 0;
      struct nir_spirv_specialization *spec_entries = NULL;
      if (spec_info && spec_info->mapEntryCount > 0) {
         num_spec_entries = spec_info->mapEntryCount;
         spec_entries = malloc(num_spec_entries * sizeof(*spec_entries));
         for (uint32_t i = 0; i < num_spec_entries; i++) {
            VkSpecializationMapEntry entry = spec_info->pMapEntries[i];
            const void *data = spec_info->pData + entry.offset;
            assert(data + entry.size <= spec_info->pData + spec_info->dataSize);

            spec_entries[i].id = spec_info->pMapEntries[i].constantID;
            spec_entries[i].data = *(const uint32_t *)data;
         }
      }

      entry_point = spirv_to_nir(spirv, module->size / 4,
                                 spec_entries, num_spec_entries,
                                 stage, entrypoint_name, nir_options);
      nir = entry_point->shader;
      assert(nir->stage == stage);
      nir_validate_shader(nir);

      free(spec_entries);

      nir_print_shader(nir, stderr);
      if (stage == MESA_SHADER_FRAGMENT) {
         nir_lower_wpos_center(nir);
         nir_validate_shader(nir);
      }

      nir_lower_returns(nir);
      nir_validate_shader(nir);

      nir_inline_functions(nir);
      nir_validate_shader(nir);

      /* Pick off the single entrypoint that we want */
      foreach_list_typed_safe(nir_function, func, node, &nir->functions) {
         if (func != entry_point)
            exec_node_remove(&func->node);
      }
      assert(exec_list_length(&nir->functions) == 1);
      entry_point->name = ralloc_strdup(entry_point, "main");

      nir_remove_dead_variables(nir, nir_var_shader_in);
      nir_remove_dead_variables(nir, nir_var_shader_out);
      nir_remove_dead_variables(nir, nir_var_system_value);
      nir_validate_shader(nir);

      nir_lower_io_to_temporaries(entry_point->shader, entry_point, true, false);

      nir_lower_system_values(nir);
      nir_validate_shader(nir);
   }

   /* Vulkan uses the separate-shader linking model */
   nir->info.separate_shader = true;

   //   nir = brw_preprocess_nir(compiler, nir);

   nir_shader_gather_info(nir, entry_point->impl);

   nir_variable_mode indirect_mask = 0;
   //   if (compiler->glsl_compiler_options[stage].EmitNoIndirectInput)
      indirect_mask |= nir_var_shader_in;
      //   if (compiler->glsl_compiler_options[stage].EmitNoIndirectTemp)
      indirect_mask |= nir_var_local;

   nir_lower_indirect_derefs(nir, indirect_mask);

   return nir;
}

static nir_shader *
radv_pipeline_compile(struct radv_pipeline *pipeline,
		      struct radv_shader_module *module,
		      const char *entrypoint,
		      gl_shader_stage stage,
		      const VkSpecializationInfo *spec_info)
{
   nir_shader *nir = radv_shader_compile_to_nir(pipeline->device,
						module, entrypoint, stage,
						spec_info);
   if (nir == NULL)
      return NULL;
   return nir;
}


VkResult
radv_pipeline_init(struct radv_pipeline *pipeline,
                  struct radv_device *device,
                  struct radv_pipeline_cache *cache,
                  const VkGraphicsPipelineCreateInfo *pCreateInfo,
                  const struct radv_graphics_pipeline_create_info *extra,
                  const VkAllocationCallbacks *alloc)
{
   VkResult result;

   if (alloc == NULL)
     alloc = &device->alloc;

   pipeline->device = device;

   const VkPipelineShaderStageCreateInfo *pStages[MESA_SHADER_STAGES] = { 0, };
   struct radv_shader_module *modules[MESA_SHADER_STAGES] = { 0, };
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      gl_shader_stage stage = ffs(pCreateInfo->pStages[i].stage) - 1;
      pStages[stage] = &pCreateInfo->pStages[i];
      modules[stage] = radv_shader_module_from_handle(pStages[stage]->module);
   }

   /* */
   if (modules[MESA_SHADER_VERTEX]) {
     radv_pipeline_compile(pipeline, modules[MESA_SHADER_VERTEX],
			   pStages[MESA_SHADER_VERTEX]->pName,
			   MESA_SHADER_VERTEX,
			   pStages[MESA_SHADER_VERTEX]->pSpecializationInfo);
   }

   if (modules[MESA_SHADER_FRAGMENT]) {
     radv_pipeline_compile(pipeline, modules[MESA_SHADER_FRAGMENT],
			   pStages[MESA_SHADER_FRAGMENT]->pName,
			   MESA_SHADER_FRAGMENT,
			   pStages[MESA_SHADER_FRAGMENT]->pSpecializationInfo);
   }
   //   nir_shader *nir = _pipeline
   return VK_SUCCESS;
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
   struct radv_pipeline *pipeline;
   VkResult result;

   //   if (cache == NULL)
   //      cache = &device->default_pipeline_cache;
   pipeline = radv_alloc2(&device->alloc, pAllocator, sizeof(*pipeline), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pipeline == NULL)
     return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   result = radv_pipeline_init(pipeline, device, cache,
                              pCreateInfo, extra, pAllocator);
   if (result != VK_SUCCESS) {
      radv_free2(&device->alloc, pAllocator, pipeline);
      return result;
   }

   *pPipeline = radv_pipeline_to_handle(pipeline);

   return VK_SUCCESS;
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
