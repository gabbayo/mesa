#pragma once

#include <amdgpu.h>
#include "addrlib/addrinterface.h"
#define RADEON_SURF_MAX_LEVEL                   32

#define RADEON_SURF_TYPE_MASK                   0xFF
#define RADEON_SURF_TYPE_SHIFT                  0
#define     RADEON_SURF_TYPE_1D                     0
#define     RADEON_SURF_TYPE_2D                     1
#define     RADEON_SURF_TYPE_3D                     2
#define     RADEON_SURF_TYPE_CUBEMAP                3
#define     RADEON_SURF_TYPE_1D_ARRAY               4
#define     RADEON_SURF_TYPE_2D_ARRAY               5
#define RADEON_SURF_MODE_MASK                   0xFF
#define RADEON_SURF_MODE_SHIFT                  8
#define     RADEON_SURF_MODE_LINEAR_ALIGNED         1
#define     RADEON_SURF_MODE_1D                     2
#define     RADEON_SURF_MODE_2D                     3
#define RADEON_SURF_SCANOUT                     (1 << 16)
#define RADEON_SURF_ZBUFFER                     (1 << 17)
#define RADEON_SURF_SBUFFER                     (1 << 18)
#define RADEON_SURF_Z_OR_SBUFFER                (RADEON_SURF_ZBUFFER | RADEON_SURF_SBUFFER)
#define RADEON_SURF_HAS_SBUFFER_MIPTREE         (1 << 19)
#define RADEON_SURF_HAS_TILE_MODE_INDEX         (1 << 20)
#define RADEON_SURF_FMASK                       (1 << 21)
#define RADEON_SURF_DISABLE_DCC                 (1 << 22)

#define RADEON_SURF_GET(v, field)   (((v) >> RADEON_SURF_ ## field ## _SHIFT) & RADEON_SURF_ ## field ## _MASK)
#define RADEON_SURF_SET(v, field)   (((v) & RADEON_SURF_ ## field ## _MASK) << RADEON_SURF_ ## field ## _SHIFT)
#define RADEON_SURF_CLR(v, field)   ((v) & ~(RADEON_SURF_ ## field ## _MASK << RADEON_SURF_ ## field ## _SHIFT))

struct radeon_surf_level {
    uint64_t                    offset;
    uint64_t                    slice_size;
    uint32_t                    npix_x;
    uint32_t                    npix_y;
    uint32_t                    npix_z;
    uint32_t                    nblk_x;
    uint32_t                    nblk_y;
    uint32_t                    nblk_z;
    uint32_t                    pitch_bytes;
    uint32_t                    mode;
    uint64_t                    dcc_offset;
    uint64_t                    dcc_fast_clear_size;
    bool                        dcc_enabled;
};


/* surface defintions from the winsys */
struct radeon_surf {
    /* These are inputs to the calculator. */
    uint32_t                    npix_x;
    uint32_t                    npix_y;
    uint32_t                    npix_z;
    uint32_t                    blk_w;
    uint32_t                    blk_h;
    uint32_t                    blk_d;
    uint32_t                    array_size;
    uint32_t                    last_level;
    uint32_t                    bpe;
    uint32_t                    nsamples;
    uint32_t                    flags;

    /* These are return values. Some of them can be set by the caller, but
     * they will be treated as hints (e.g. bankw, bankh) and might be
     * changed by the calculator.
     */
    uint64_t                    bo_size;
    uint64_t                    bo_alignment;
    /* This applies to EG and later. */
    uint32_t                    bankw;
    uint32_t                    bankh;
    uint32_t                    mtilea;
    uint32_t                    tile_split;
    uint32_t                    stencil_tile_split;
    uint64_t                    stencil_offset;
    struct radeon_surf_level    level[RADEON_SURF_MAX_LEVEL];
    struct radeon_surf_level    stencil_level[RADEON_SURF_MAX_LEVEL];
    uint32_t                    tiling_index[RADEON_SURF_MAX_LEVEL];
    uint32_t                    stencil_tiling_index[RADEON_SURF_MAX_LEVEL];
    uint32_t                    pipe_config;
    uint32_t                    num_banks;
    uint32_t                    macro_tile_index;
    uint32_t                    micro_tile_mode; /* displayable, thin, depth, rotated */

    uint64_t                    dcc_size;
    uint64_t                    dcc_alignment;
};

ADDR_HANDLE radv_amdgpu_addr_create(struct amdgpu_gpu_info *amdinfo, int family, int rev_id);
int radv_amdgpu_surface_init(ADDR_HANDLE addrlib, struct radeon_surf *surf);
