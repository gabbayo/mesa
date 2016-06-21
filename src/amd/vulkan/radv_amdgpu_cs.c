#include "radv_amdgpu_cs.h"
#include <stdlib.h>
#include <amdgpu.h>
enum ib_type {
   IB_CONST_PREAMBLE = 0,
   IB_CONST = 1, /* the const IB must be first */
   IB_MAIN = 2,
   IB_NUM
};

struct radv_amdgpu_ib {
  
   struct radeon_winsys_cs base;
};

struct radv_amdgpu_cs {
  struct radv_amdgpu_ib main;
  struct amdgpu_cs_request    request;
  struct amdgpu_cs_ib_info    ib[IB_NUM];

  unsigned                    max_num_buffers;
  unsigned                    num_buffers;
  amdgpu_bo_handle            *handles;
  uint8_t                     *flags;
  //  struct amdgpu_cs_buffer     *buffers;
};

static struct radeon_winsys_cs *
radv_amdgpu_cs_create(void)
{
  struct radv_amdgpu_cs *cs;

  cs = calloc(1, sizeof(struct radv_amdgpu_cs));
  if (!cs)
    return NULL;

  return &cs->main.base;

}

static void radv_amdgpu_cs_submit_ib(struct radv_amdgpu_cs *acs)
{
  
}
