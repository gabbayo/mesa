#include <stdlib.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <assert.h>

#include "radv_radeon_winsys.h"
#include "radv_amdgpu_cs.h"
#include "radv_amdgpu_bo.h"

struct amdgpu_cs {
  struct radeon_winsys_cs base;
  struct amdgpu_winsys *ws;

  struct amdgpu_cs_request    request;
  struct amdgpu_cs_ib_info    ib;

  struct radeon_winsys_bo     *ib_buffer;
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

static void amdgpu_cs_destroy(struct radeon_winsys_cs *rcs)
{
   struct amdgpu_cs *cs = amdgpu_cs(rcs);
   cs->ws->base.buffer_destroy(cs->ib_buffer);
   free(cs->handles);
   free(cs->priorities);
   free(cs);
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

static struct radeon_winsys_cs *
amdgpu_cs_create(struct radeon_winsys *ws,
		 enum ring_type ring_type)
{
  struct amdgpu_cs *cs;
  uint32_t ib_size = 20 * 1024 * 4;
  int r;
  cs = calloc(1, sizeof(struct amdgpu_cs));
  if (!cs)
    return NULL;

  cs->ws = amdgpu_winsys(ws);

  cs->ib_buffer = ws->buffer_create(ws, ib_size, 0,
				    RADEON_DOMAIN_GTT,
				    RADEON_FLAG_CPU_ACCESS);
  if (!cs->ib_buffer)
    return NULL;

  amdgpu_init_cs(cs, RING_GFX);
  cs->ib_mapped = ws->buffer_map(cs->ib_buffer);
  if (!cs->ib_mapped) {
    ws->buffer_destroy(cs->ib_buffer);
    free(cs);
    return NULL;
  }

  cs->ib.ib_mc_address = amdgpu_winsys_bo(cs->ib_buffer)->va;
  cs->base.buf = (uint32_t *)cs->ib_mapped;
  cs->base.max_dw = ib_size / 4;

  ws->cs_add_buffer(&cs->base, cs->ib_buffer, 8);
  return &cs->base;
}

static void amdgpu_cs_add_buffer(struct radeon_winsys_cs *_cs,
				 struct radeon_winsys_bo *_bo,
				 uint8_t priority)
{
   struct amdgpu_cs *cs = amdgpu_cs(_cs);
   struct amdgpu_winsys_bo *bo = amdgpu_winsys_bo(_bo);

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

static int amdgpu_winsys_cs_submit(struct radeon_winsys_ctx *_ctx,
				   struct radeon_winsys_cs *_cs,
				   struct radeon_winsys_fence *_fence)
{
   int r;
   struct amdgpu_cs *cs = amdgpu_cs(_cs);
   struct amdgpu_ctx *ctx = amdgpu_ctx(_ctx);
   struct amdgpu_cs_fence *fence = (struct amdgpu_cs_fence *)_fence;
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

   r = amdgpu_cs_submit(ctx->ctx, 0, &cs->request, 1);
   if (r) {
      if (r == -ENOMEM)
	 fprintf(stderr, "amdgpu: Not enough memory for command submission.\n");
      else
         fprintf(stderr, "amdgpu: The CS has been rejected, "
                 "see dmesg for more information.\n");
   }

   amdgpu_bo_list_destroy(bo_list);

   if (fence) {
      fence->context = ctx->ctx;
      fence->ip_type = cs->request.ip_type;
      fence->ip_instance = cs->request.ip_instance;
      fence->ring = cs->request.ring;
      fence->fence = cs->request.seq_no;
   }

   return 0;
}

/* add buffer */
/* validate */
/* check space? */
/* is buffer referenced */

static struct radeon_winsys_ctx *amdgpu_ctx_create(struct radeon_winsys *_ws)
{
   struct amdgpu_winsys *ws = amdgpu_winsys(_ws);
   struct amdgpu_ctx *ctx = CALLOC_STRUCT(amdgpu_ctx);
   int r;

   if (!ctx)
      return NULL;
   r = amdgpu_cs_ctx_create(ws->dev, &ctx->ctx);
   if (r) {
      fprintf(stderr, "amdgpu: amdgpu_cs_ctx_create failed. (%i)\n", r);
      goto error_create;
   }
   return (struct radeon_winsys_ctx *)ctx;
 error_create:
   return NULL;
}

void radv_amdgpu_cs_init_functions(struct amdgpu_winsys *ws)
{
   ws->base.ctx_create = amdgpu_ctx_create;
   ws->base.cs_create = amdgpu_cs_create;
   ws->base.cs_destroy = amdgpu_cs_destroy;
   ws->base.cs_add_buffer = amdgpu_cs_add_buffer;
   ws->base.cs_submit = amdgpu_winsys_cs_submit;
}
