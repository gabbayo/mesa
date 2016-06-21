#pragma once
#include "radv_amdgpu_winsys.h"
struct amdgpu_winsys_bo {
   amdgpu_bo_handle bo;
   amdgpu_va_handle va_handle;

   uint64_t va;
   enum radeon_bo_domain initial_domain;
   uint64_t size;
};

void amdgpu_bo_destroy(struct amdgpu_winsys_bo *);
struct amdgpu_winsys_bo *amdgpu_create_bo(struct amdgpu_winsys *ws,
					  uint64_t size,
					  unsigned alignment,
					  unsigned usage,
					  enum radeon_bo_domain initial_domain,
					  unsigned flags);

int amdgpu_bo_map(struct amdgpu_winsys_bo *bo, void **data);
void amdgpu_bo_unmap(struct amdgpu_winsys_bo *bo);
