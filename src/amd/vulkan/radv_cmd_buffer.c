#include "radv_private.h"
#include "radv_radeon_winsys.h"
#include "radv_cs.h"
#include "sid.h"

static VkResult radv_create_cmd_buffer(
    struct radv_device *                         device,
    struct radv_cmd_pool *                       pool,
    VkCommandBufferLevel                        level,
    VkCommandBuffer*                            pCommandBuffer)
{
   struct radv_cmd_buffer *cmd_buffer;
   VkResult result;

   cmd_buffer = radv_alloc(&pool->alloc, sizeof(*cmd_buffer), 8,
                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (cmd_buffer == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   cmd_buffer->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   cmd_buffer->device = device;
   cmd_buffer->pool = pool;
   cmd_buffer->level = level;

   if (pool) {
      list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);
   } else {
      /* Init the pool_link so we can safefly call list_del when we destroy
       * the command buffer
       */
      list_inithead(&cmd_buffer->pool_link);
   }

   cmd_buffer->cs = device->ws->cs_create(device->ws, RING_GFX);
   *pCommandBuffer = radv_cmd_buffer_to_handle(cmd_buffer);

   return VK_SUCCESS;

 fail:
   radv_free(&cmd_buffer->pool->alloc, cmd_buffer);

   return result;
}

VkResult radv_AllocateCommandBuffers(
    VkDevice                                    _device,
    const VkCommandBufferAllocateInfo*          pAllocateInfo,
    VkCommandBuffer*                            pCommandBuffers)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
     result = radv_create_cmd_buffer(device, pool, pAllocateInfo->level,
				     &pCommandBuffers[i]);
     if (result != VK_SUCCESS)
       break;
   }

   if (result != VK_SUCCESS)
      radv_FreeCommandBuffers(_device, pAllocateInfo->commandPool,
                             i, pCommandBuffers);

   return result;
}

static void
radv_cmd_buffer_destroy(struct radv_cmd_buffer *cmd_buffer)
{
   list_del(&cmd_buffer->pool_link);

   cmd_buffer->device->ws->cs_destroy(cmd_buffer->cs);
   radv_free(&cmd_buffer->pool->alloc, cmd_buffer);
}

void radv_FreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, pCommandBuffers[i]);

      radv_cmd_buffer_destroy(cmd_buffer);
   }
}

VkResult radv_ResetCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    VkCommandBufferResetFlags                   flags)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   return VK_SUCCESS;//radv_cmd_buffer_reset(cmd_buffer);
}

VkResult radv_BeginCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    const VkCommandBufferBeginInfo*             pBeginInfo)
{
   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   /* setup initial configuration into command buffer */
   si_init_config(&cmd_buffer->device->instance->physicalDevice, cmd_buffer->cs);
   return VK_SUCCESS;
}

void radv_CmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

}

void radv_CmdBindDescriptorSets(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            _layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
  //   RADV_FROM_HANDLE(radv_pipeline_layout, layout, _layout);

}

VkResult radv_EndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
  //   struct radv_device *device = cmd_buffer->device;
   return VK_SUCCESS;
}

static void
radv_bind_compute_pipeline(struct radv_cmd_buffer *cmd_buffer,
                           struct radv_pipeline *pipeline)
{
   struct radeon_winsys *ws = cmd_buffer->device->ws;
   struct radv_shader_variant *compute_shader = pipeline->shaders[MESA_SHADER_COMPUTE];
   uint64_t va = ws->buffer_get_va(compute_shader->bo);

   ws->cs_add_buffer(cmd_buffer->cs, compute_shader->bo, 8);

   radeon_set_sh_reg_seq(cmd_buffer->cs, R_00B830_COMPUTE_PGM_LO, 2);
   radeon_emit(cmd_buffer->cs, va >> 8);
   radeon_emit(cmd_buffer->cs, va >> 40);

   radeon_set_sh_reg_seq(cmd_buffer->cs, R_00B848_COMPUTE_PGM_RSRC1, 2);
   radeon_emit(cmd_buffer->cs, compute_shader->rsrc1);
   radeon_emit(cmd_buffer->cs, compute_shader->rsrc2);

   /* change these once we have scratch support */
   radeon_set_sh_reg(cmd_buffer->cs, R_00B860_COMPUTE_TMPRING_SIZE,
                     S_00B860_WAVES(32) | S_00B860_WAVESIZE(0));

   radeon_set_sh_reg_seq(cmd_buffer->cs, R_00B81C_COMPUTE_NUM_THREAD_X, 3);
   radeon_emit(cmd_buffer->cs,
               S_00B81C_NUM_THREAD_FULL(pipeline->compute.block_size[0]));
   radeon_emit(cmd_buffer->cs,
               S_00B81C_NUM_THREAD_FULL(pipeline->compute.block_size[1]));
   radeon_emit(cmd_buffer->cs,
               S_00B81C_NUM_THREAD_FULL(pipeline->compute.block_size[2]));
}

void radv_CmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  _pipeline)
{
   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   RADV_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);

   if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
	   radv_bind_compute_pipeline(cmd_buffer, pipeline);
   }

}

void radv_CmdSetViewport(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstViewport,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports)
{
  RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
  si_write_viewport(cmd_buffer->cs, firstViewport, viewportCount, pViewports);
}

void radv_CmdSetScissor(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
  // RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);


}

void radv_CmdSetLineWidth(
    VkCommandBuffer                             commandBuffer,
    float                                       lineWidth)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

}

void radv_CmdSetDepthBias(
    VkCommandBuffer                             commandBuffer,
    float                                       depthBiasConstantFactor,
    float                                       depthBiasClamp,
    float                                       depthBiasSlopeFactor)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

}

void radv_CmdSetBlendConstants(
    VkCommandBuffer                             commandBuffer,
    const float                                 blendConstants[4])
{

}
void radv_CmdExecuteCommands(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCmdBuffers)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, primary, commandBuffer);

  //   assert(primary->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);

   for (uint32_t i = 0; i < commandBufferCount; i++) {
     //      RADV_FROM_HANDLE(radv_cmd_buffer, secondary, pCmdBuffers[i]);

     //      assert(secondary->level == VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      //.     radv_cmd_buffer_add_secondary(primary, secondary);
   }
}
VkResult radv_CreateCommandPool(
    VkDevice                                    _device,
    const VkCommandPoolCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkCommandPool*                              pCmdPool)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_cmd_pool *pool;

   pool = radv_alloc2(&device->alloc, pAllocator, sizeof(*pool), 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pool == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      pool->alloc = *pAllocator;
   else
      pool->alloc = device->alloc;

   list_inithead(&pool->cmd_buffers);

   *pCmdPool = radv_cmd_pool_to_handle(pool);

   return VK_SUCCESS;

}

void radv_DestroyCommandPool(
    VkDevice                                    _device,
    VkCommandPool                               commandPool,
    const VkAllocationCallbacks*                pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_cmd_pool, pool, commandPool);

   list_for_each_entry_safe(struct radv_cmd_buffer, cmd_buffer,
                            &pool->cmd_buffers, pool_link) {
     //      radv_cmd_buffer_destroy(cmd_buffer);
   }

   radv_free2(&device->alloc, pAllocator, pool);
}

VkResult radv_ResetCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolResetFlags                     flags)
{
   RADV_FROM_HANDLE(radv_cmd_pool, pool, commandPool);

   list_for_each_entry(struct radv_cmd_buffer, cmd_buffer,
                       &pool->cmd_buffers, pool_link) {
     //      radv_cmd_buffer_reset(cmd_buffer);
   }

   return VK_SUCCESS;
}

void radv_CmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
  // RADV_FROM_HANDLE(radv_render_pass, pass, pRenderPassBegin->renderPass);
  //RADV_FROM_HANDLE(radv_framebuffer, framebuffer, pRenderPassBegin->framebuffer);

}

void radv_CmdDraw(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
}

void radv_CmdDispatch(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    x,
    uint32_t                                    y,
    uint32_t                                    z)
{
   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   radeon_emit(cmd_buffer->cs, PKT3(PKT3_DISPATCH_DIRECT, 3, 0) |
                   PKT3_SHADER_TYPE_S(1));
   radeon_emit(cmd_buffer->cs, x);
   radeon_emit(cmd_buffer->cs, y);
   radeon_emit(cmd_buffer->cs, z);
   radeon_emit(cmd_buffer->cs, 1);
}

void radv_CmdEndRenderPass(
    VkCommandBuffer                             commandBuffer)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   //   radv_cmd_buffer_resolve_subpass(cmd_buffer);
}

void radv_CmdPipelineBarrier(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        destStageMask,
    VkBool32                                    byRegion,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
  //   RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

}
