#pragma once

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "r600d_common.h"
#include <amdgpu.h>

#include "radv_radeon_winsys.h"

#include "radv_amdgpu_winsys.h"
struct amdgpu_ctx {
  amdgpu_context_handle ctx;
  uint64_t last_seq_no;
};

static inline struct amdgpu_ctx *
amdgpu_ctx(struct radeon_winsys_ctx *base)
{
   return (struct amdgpu_ctx *)base;
}
			       

void radv_amdgpu_cs_init_functions(struct amdgpu_winsys *ws);
