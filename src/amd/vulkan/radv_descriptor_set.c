#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "radv_private.h"

VkResult radv_CreateDescriptorSetLayout(
    VkDevice                                    _device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorSetLayout*                      pSetLayout)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_descriptor_set_layout *set_layout;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);

   uint32_t max_binding = 0;
   uint32_t immutable_sampler_count = 0;
   for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
      max_binding = MAX(max_binding, pCreateInfo->pBindings[j].binding);
      if (pCreateInfo->pBindings[j].pImmutableSamplers)
         immutable_sampler_count += pCreateInfo->pBindings[j].descriptorCount;
   }

   size_t size = sizeof(struct radv_descriptor_set_layout) +
                 (max_binding + 1) * sizeof(set_layout->binding[0]) +
                 immutable_sampler_count * sizeof(struct radv_sampler *);

   set_layout = radv_alloc2(&device->alloc, pAllocator, size, 8,
                           VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!set_layout)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* We just allocate all the samplers at the end of the struct */
   struct radv_sampler **samplers =
      (struct radv_sampler **)&set_layout->binding[max_binding + 1];

   set_layout->binding_count = max_binding + 1;
   set_layout->shader_stages = 0;
   set_layout->size = 0;

   memset(set_layout->binding, 0, size);

   uint32_t buffer_count = 0;
   uint32_t dynamic_offset_count = 0;

   for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
      const VkDescriptorSetLayoutBinding *binding = &pCreateInfo->pBindings[j];
      uint32_t b = binding->binding;

      assert(binding->descriptorCount > 0);
      set_layout->binding[b].type = binding->descriptorType;
      set_layout->binding[b].array_size = binding->descriptorCount;
      set_layout->binding[b].offset = set_layout->size;
      set_layout->binding[b].buffer_offset = buffer_count;
      set_layout->binding[b].dynamic_offset_offset = dynamic_offset_count;

      switch (binding->descriptorType) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         set_layout->binding[b].dynamic_offset_count = 1;
         /* fall through */
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         set_layout->binding[b].size = 16;
         set_layout->binding[b].buffer_count = 1;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         /* main descriptor + fmask descriptor */
         set_layout->binding[b].size = 64;
         set_layout->binding[b].buffer_count = 1;
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         /* main descriptor + fmask descriptor + sampler */
         set_layout->binding[b].size = 80;
         set_layout->binding[b].buffer_count = 1;
         break;
      default:
         break;
      }

      set_layout->size += binding->descriptorCount * set_layout->binding[b].size;
      buffer_count += binding->descriptorCount * set_layout->binding[b].buffer_count;
      dynamic_offset_count += binding->descriptorCount *
                                 set_layout->binding[b].dynamic_offset_count;


      if (binding->pImmutableSamplers) {
         set_layout->binding[b].immutable_samplers = samplers;
         samplers += binding->descriptorCount;

         for (uint32_t i = 0; i < binding->descriptorCount; i++)
            set_layout->binding[b].immutable_samplers[i] =
               radv_sampler_from_handle(binding->pImmutableSamplers[i]);
      } else {
         set_layout->binding[b].immutable_samplers = NULL;
      }

      set_layout->shader_stages |= binding->stageFlags;
   }

   set_layout->buffer_count = buffer_count;
   set_layout->dynamic_offset_count = dynamic_offset_count;

   *pSetLayout = radv_descriptor_set_layout_to_handle(set_layout);

   return VK_SUCCESS;
}

void radv_DestroyDescriptorSetLayout(
    VkDevice                                    _device,
    VkDescriptorSetLayout                       _set_layout,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_descriptor_set_layout, set_layout, _set_layout);

   radv_free2(&device->alloc, pAllocator, set_layout);
}

/*
 * Pipeline layouts.  These have nothing to do with the pipeline.  They are
 * just muttiple descriptor set layouts pasted together
 */

VkResult radv_CreatePipelineLayout(
    VkDevice                                    _device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineLayout*                           pPipelineLayout)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_pipeline_layout *layout;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

   layout = radv_alloc2(&device->alloc, pAllocator, sizeof(*layout), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (layout == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   layout->num_sets = pCreateInfo->setLayoutCount;

   unsigned dynamic_offset_count = 0;

   memset(layout->stage, 0, sizeof(layout->stage));
   for (uint32_t set = 0; set < pCreateInfo->setLayoutCount; set++) {
      RADV_FROM_HANDLE(radv_descriptor_set_layout, set_layout,
                      pCreateInfo->pSetLayouts[set]);
      layout->set[set].layout = set_layout;

      layout->set[set].dynamic_offset_start = dynamic_offset_count;
      for (uint32_t b = 0; b < set_layout->binding_count; b++) {
         if (set_layout->binding[b].dynamic_offset_index < 0)
            continue;

         dynamic_offset_count += set_layout->binding[b].array_size;
         for (gl_shader_stage s = 0; s < MESA_SHADER_STAGES; s++) {
         }
      }
   }

   *pPipelineLayout = radv_pipeline_layout_to_handle(layout);

   return VK_SUCCESS;
}

void radv_DestroyPipelineLayout(
    VkDevice                                    _device,
    VkPipelineLayout                            _pipelineLayout,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, _pipelineLayout);

   radv_free2(&device->alloc, pAllocator, pipeline_layout);
}

#define EMPTY 1

VkResult radv_CreateDescriptorPool(
    VkDevice                                    _device,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorPool*                           pDescriptorPool)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_descriptor_pool *pool;
   int size = 0;
   pool = radv_alloc2(&device->alloc, pAllocator, size, 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!pool)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   return VK_SUCCESS;
}

void radv_DestroyDescriptorPool(
    VkDevice                                    _device,
    VkDescriptorPool                            _pool,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_descriptor_pool, pool, _pool);

   radv_free2(&device->alloc, pAllocator, pool);
}

VkResult radv_ResetDescriptorPool(
    VkDevice                                    _device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorPoolResetFlags                  flags)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_descriptor_pool, pool, descriptorPool);

   return VK_SUCCESS;
}

VkResult radv_AllocateDescriptorSets(
    VkDevice                                    _device,
    const VkDescriptorSetAllocateInfo*          pAllocateInfo,
    VkDescriptorSet*                            pDescriptorSets)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_descriptor_pool, pool, pAllocateInfo->descriptorPool);

   VkResult result = VK_SUCCESS;

   return result;
}

VkResult radv_FreeDescriptorSets(
    VkDevice                                    _device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    count,
    const VkDescriptorSet*                      pDescriptorSets)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_descriptor_pool, pool, descriptorPool);

   return VK_SUCCESS;
}

void radv_UpdateDescriptorSets(
    VkDevice                                    _device,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    descriptorCopyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies)
{
   RADV_FROM_HANDLE(radv_device, device, _device);

}
