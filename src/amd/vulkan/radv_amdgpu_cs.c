#include "radv_amdgpu_cs.h"
#include "radv_amdgpu_bo.h"
#include <stdlib.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <assert.h>

struct amdgpu_cs {
  struct radeon_winsys_cs base;
  struct amdgpu_winsys *ws;

  struct amdgpu_cs_request    request;
  struct amdgpu_cs_ib_info    ib;

  struct amdgpu_winsys_bo     *ib_buffer;
  uint8_t                 *ib_mapped;
  unsigned                    max_num_buffers;
  unsigned                    num_buffers;
  amdgpu_bo_handle            *handles;
  uint8_t                     *priorities;
  //  struct amdgpu_cs_buffer     *buffers;
};

static inline struct amdgpu_cs *
amdgpu_cs(struct radeon_winsys_cs *base)
{
   return (struct amdgpu_cs*)base;
}

void radv_amdgpu_cs_destroy(struct radeon_winsys_cs *rcs)
{


}

static boolean amdgpu_init_cs(struct amdgpu_cs *cs,
			      enum ring_type ring_type)
{
   switch (ring_type) {
   case RING_DMA:
      cs->request.ip_type = AMDGPU_HW_IP_DMA;
      break;

   case RING_UVD:
      cs->request.ip_type = AMDGPU_HW_IP_UVD;
      break;

   case RING_VCE:
      cs->request.ip_type = AMDGPU_HW_IP_VCE;
      break;

   case RING_COMPUTE:
      cs->request.ip_type = AMDGPU_HW_IP_COMPUTE;
      break;

   default:
   case RING_GFX:
      cs->request.ip_type = AMDGPU_HW_IP_GFX;
      break;
   }
   cs->request.number_of_ibs = 1;
   cs->request.ibs = &cs->ib;
   return true;
}

struct radeon_winsys_cs *radv_amdgpu_cs_create(struct amdgpu_winsys *ws)
{
  struct amdgpu_cs *cs;
  uint32_t ib_size = 20 * 1024 * 4;
  int r;
  cs = calloc(1, sizeof(struct amdgpu_cs));
  if (!cs)
    return NULL;

  cs->ws = ws;

  cs->ib_buffer = amdgpu_create_bo(ws, ib_size, 0, 0,
				   RADEON_DOMAIN_GTT,
				   RADEON_FLAG_CPU_ACCESS);
  if (!cs->ib_buffer)
    return NULL;

  amdgpu_init_cs(cs, RING_GFX);
  r = amdgpu_bo_map(cs->ib_buffer, (void **)&cs->ib_mapped);
  if (r != 0) {
    amdgpu_bo_destroy(cs->ib_buffer);
    free(cs);
    return NULL;
  }

  cs->ib.ib_mc_address = cs->ib_buffer->va;
  cs->base.buf = (uint32_t *)cs->ib_mapped;
  cs->base.max_dw = ib_size / 4;

  radv_amdgpu_cs_add_buffer(&cs->base, cs->ib_buffer, 8);
  return &cs->base;
}

void radv_amdgpu_cs_add_buffer(struct radeon_winsys_cs *rcs,
                               struct amdgpu_winsys_bo *bo,
                               uint8_t priority)
{
   struct amdgpu_cs *cs = amdgpu_cs(rcs);

   for (unsigned i = 0; i < cs->num_buffers; ++i) {
      if (cs->handles[i] == bo->bo) {
         cs->priorities[i] = MAX2(cs->priorities[i], priority);
         return;
      }
   }

   if (cs->num_buffers == cs->max_num_buffers) {
      unsigned new_count = cs->max_num_buffers * 2;
      cs->handles = realloc(cs->handles, new_count * sizeof(amdgpu_bo_handle));
      cs->priorities = realloc(cs->priorities, new_count * sizeof(uint8_t));
      cs->max_num_buffers = new_count;
   }

   cs->handles[cs->num_buffers] = bo->bo;
   cs->priorities[cs->num_buffers] = priority;
   ++cs->num_buffers;
}

int radv_amdgpu_cs_submit(amdgpu_context_handle hw_ctx,
			  struct radeon_winsys_cs *rcs,
			  struct amdgpu_cs_fence *fence)
{
   int r;
   struct amdgpu_cs *cs = amdgpu_cs(rcs);
   amdgpu_bo_list_handle bo_list;

   if (!cs->base.cdw)
      return true;

   r = amdgpu_bo_list_create(cs->ws->dev, cs->num_buffers, cs->handles,
                             cs->priorities, &bo_list);
   if (r) {
      fprintf(stderr, "amdgpu: Failed to created the BO list for submission\n");
      return r;
   }

   cs->request.resources = bo_list;
   cs->ib.size = cs->base.cdw;

   r = amdgpu_cs_submit(hw_ctx, 0, &cs->request, 1);
   if (r) {
      if (r == -ENOMEM)
	 fprintf(stderr, "amdgpu: Not enough memory for command submission.\n");
      else
         fprintf(stderr, "amdgpu: The CS has been rejected, "
                 "see dmesg for more information.\n");
   }

   amdgpu_bo_list_destroy(bo_list);

   if (fence) {
      fence->context = hw_ctx;
      fence->ip_type = cs->request.ip_type;
      fence->ip_instance = cs->request.ip_instance;
      fence->ring = cs->request.ring;
      fence->fence = cs->request.seq_no;
   }

   return 0;
}
