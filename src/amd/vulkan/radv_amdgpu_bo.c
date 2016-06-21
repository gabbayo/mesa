#include <stdio.h>

#include "radv_amdgpu_bo.h"

#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <inttypes.h>

void amdgpu_bo_destroy(struct amdgpu_winsys_bo *bo)
{
   amdgpu_bo_va_op(bo->bo, 0, bo->size, bo->va, 0, AMDGPU_VA_OP_UNMAP);
   amdgpu_va_range_free(bo->va_handle);
   amdgpu_bo_free(bo->bo);
   FREE(bo);
}

struct amdgpu_winsys_bo *amdgpu_create_bo(struct amdgpu_winsys *ws,
					  uint64_t size,
					  unsigned alignment,
					  unsigned usage,
					  enum radeon_bo_domain initial_domain,
					  unsigned flags)
{
   struct amdgpu_winsys_bo *bo;
   struct amdgpu_bo_alloc_request request = {0};
   amdgpu_bo_handle buf_handle;
   uint64_t va = 0;
   amdgpu_va_handle va_handle;
   int r;
   bo = CALLOC_STRUCT(amdgpu_winsys_bo);
   if (!bo) {
      return NULL;
   }

   request.alloc_size = size;
   request.phys_alignment = alignment;

   if (initial_domain & RADEON_DOMAIN_VRAM)
      request.preferred_heap |= AMDGPU_GEM_DOMAIN_VRAM;
   if (initial_domain & RADEON_DOMAIN_GTT)
      request.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;

   if (flags & RADEON_FLAG_CPU_ACCESS)
      request.flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
   if (flags & RADEON_FLAG_NO_CPU_ACCESS)
      request.flags |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
   if (flags & RADEON_FLAG_GTT_WC)
      request.flags |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;

   r = amdgpu_bo_alloc(ws->dev, &request, &buf_handle);
   if (r) {
      fprintf(stderr, "amdgpu: Failed to allocate a buffer:\n");
      fprintf(stderr, "amdgpu:    size      : %"PRIu64" bytes\n", size);
      fprintf(stderr, "amdgpu:    alignment : %u bytes\n", alignment);
      fprintf(stderr, "amdgpu:    domains   : %u\n", initial_domain);
      goto error_bo_alloc;
   }

   r = amdgpu_va_range_alloc(ws->dev, amdgpu_gpu_va_range_general,
                             size, alignment, 0, &va, &va_handle, 0);
   if (r)
      goto error_va_alloc;

   r = amdgpu_bo_va_op(buf_handle, 0, size, va, 0, AMDGPU_VA_OP_MAP);
   if (r)
      goto error_va_map;

   bo->bo = buf_handle;
   bo->va = va;
   bo->va_handle = va_handle;
   bo->initial_domain = initial_domain;
   bo->size = size;
   return bo;
error_va_map:   
   amdgpu_va_range_free(va_handle);

error_va_alloc:
   amdgpu_bo_free(buf_handle);

 error_bo_alloc:
   FREE(bo);
   return NULL;
}

int amdgpu_bo_map(struct amdgpu_winsys_bo *bo, void **data)
{
   return amdgpu_bo_cpu_map(bo->bo, data);
}

void amdgpu_bo_unmap(struct amdgpu_winsys_bo *bo)
{
   amdgpu_bo_cpu_unmap(bo->bo);
}
