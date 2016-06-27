#pragma once

#include <amdgpu.h>

void radv_amdgpu_surface_init_functions(struct amdgpu_winsys *ws);
ADDR_HANDLE radv_amdgpu_addr_create(struct amdgpu_gpu_info *amdinfo, int family, int rev_id);
