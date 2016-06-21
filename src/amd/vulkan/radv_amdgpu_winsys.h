
#pragma once

#include "radv_radeon_winsys.h"
#include "addrlib/addrinterface.h"
#include <amdgpu.h>

struct amdgpu_winsys {
  amdgpu_device_handle dev;

  struct radeon_info info;
  struct amdgpu_gpu_info amdinfo;
  ADDR_HANDLE addrlib;

  uint32_t rev_id;
  unsigned family;
};


struct amdgpu_winsys *
amdgpu_winsys_create(int fd);
