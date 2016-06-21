#include "radv_amdgpu_cs.h"
#include <stdlib.h>
#include <amdgpu.h>
#include <assert.h>
enum ib_type {
   IB_CONST_PREAMBLE = 0,
   IB_CONST = 1, /* the const IB must be first */
   IB_MAIN = 2,
   IB_NUM
};

struct amdgpu_ib {
   struct radeon_winsys_cs base;
};

struct amdgpu_cs {
  struct amdgpu_ib main;
  struct amdgpu_cs_request    request;
  struct amdgpu_cs_ib_info    ib[IB_NUM];

  unsigned                    max_num_buffers;
  unsigned                    num_buffers;
  amdgpu_bo_handle            *handles;
  uint8_t                     *flags;
  //  struct amdgpu_cs_buffer     *buffers;
};

static inline struct amdgpu_cs *
amdgpu_cs(struct radeon_winsys_cs *base)
{
   assert(amdgpu_ib(base)->ib_type == IB_MAIN);
   return (struct amdgpu_cs*)base;
}

void radv_amdgpu_cs_destroy(struct radeon_winsys_cs *rcs)
{
   struct amdgpu_cs *cs = amdgpu_cs(rcs);

}

struct radeon_winsys_cs *radv_amdgpu_cs_create(void)
{
  struct amdgpu_cs *cs;

  cs = calloc(1, sizeof(struct amdgpu_cs));
  if (!cs)
    return NULL;

  return &cs->main.base;

}

static void radv_amdgpu_cs_submit_ib(struct amdgpu_cs *acs)
{
  
}
