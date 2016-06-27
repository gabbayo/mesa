#pragma once
#include "radv_amdgpu_winsys.h"
struct amdgpu_winsys_bo {
   amdgpu_bo_handle bo;
   amdgpu_va_handle va_handle;

   uint64_t va;
   enum radeon_bo_domain initial_domain;
   uint64_t size;
};

static inline
struct amdgpu_winsys_bo *amdgpu_winsys_bo(struct radeon_winsys_bo *bo)
{
   return (struct amdgpu_winsys_bo *)bo;
}

void radv_amdgpu_bo_init_functions(struct amdgpu_winsys *ws);

