#pragma once

#include <string.h>
#include <stdint.h>

struct amdgpu_winsys;
struct radeon_winsys_cs {
    unsigned cdw;  /* Number of used dwords. */
    unsigned max_dw; /* Maximum number of dwords. */
    uint32_t *buf; /* The base pointer of the chunk. */
};

static inline void radeon_emit(struct radeon_winsys_cs *cs, uint32_t value)
{
    cs->buf[cs->cdw++] = value;
}

static inline void radeon_emit_array(struct radeon_winsys_cs *cs,
				     const uint32_t *values, unsigned count)
{
    memcpy(cs->buf + cs->cdw, values, count * 4);
    cs->cdw += count;
}


struct radeon_winsys_cs *radv_amdgpu_cs_create(struct amdgpu_winsys *ws);

void radv_amdgpu_cs_destroy(struct radeon_winsys_cs *rcs);
