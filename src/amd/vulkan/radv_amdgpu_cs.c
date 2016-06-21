#include "radv_amdgpu_cs.h"
#include "radv_amdgpu_bo.h"
#include <stdlib.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <assert.h>

struct amdgpu_cs {
  struct radeon_winsys_cs base;

  struct amdgpu_cs_request    request;
  struct amdgpu_cs_ib_info    ib;

  struct amdgpu_winsys_bo     *ib_buffer;
  uint8_t                 *ib_mapped;
  unsigned                    max_num_buffers;
  unsigned                    num_buffers;
  amdgpu_bo_handle            *handles;
  uint8_t                     *flags;
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

  return &cs->base;
}

int radv_amdgpu_cs_submit(amdgpu_context_handle hw_ctx,
			  struct radeon_winsys_cs *rcs)
{
   int r;
   struct amdgpu_cs *cs = amdgpu_cs(rcs);

   if (!cs->base.cdw)
      return true;
   cs->ib.size = cs->base.cdw;

   r = amdgpu_cs_submit(hw_ctx, 0, &cs->request, 1);
   if (r) {
      if (r == -ENOMEM)
	 fprintf(stderr, "amdgpu: Not enough memory for command submission.\n");
      else
         fprintf(stderr, "amdgpu: The CS has been rejected, "
                 "see dmesg for more information.\n");
   }

   return 0;
}
