#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main/macros.h"

#define FREE(x) free(x)

enum radeon_bo_domain { /* bitfield */
    RADEON_DOMAIN_GTT  = 2,
    RADEON_DOMAIN_VRAM = 4,
    RADEON_DOMAIN_VRAM_GTT = RADEON_DOMAIN_VRAM | RADEON_DOMAIN_GTT
};

enum radeon_bo_flag { /* bitfield */
    RADEON_FLAG_GTT_WC =        (1 << 0),
    RADEON_FLAG_CPU_ACCESS =    (1 << 1),
    RADEON_FLAG_NO_CPU_ACCESS = (1 << 2),
};

enum radeon_bo_usage { /* bitfield */
    RADEON_USAGE_READ = 2,
    RADEON_USAGE_WRITE = 4,
    RADEON_USAGE_READWRITE = RADEON_USAGE_READ | RADEON_USAGE_WRITE
};

enum radeon_family {
    CHIP_UNKNOWN = 0,
    CHIP_R300, /* R3xx-based cores. */
    CHIP_R350,
    CHIP_RV350,
    CHIP_RV370,
    CHIP_RV380,
    CHIP_RS400,
    CHIP_RC410,
    CHIP_RS480,
    CHIP_R420,     /* R4xx-based cores. */
    CHIP_R423,
    CHIP_R430,
    CHIP_R480,
    CHIP_R481,
    CHIP_RV410,
    CHIP_RS600,
    CHIP_RS690,
    CHIP_RS740,
    CHIP_RV515,    /* R5xx-based cores. */
    CHIP_R520,
    CHIP_RV530,
    CHIP_R580,
    CHIP_RV560,
    CHIP_RV570,
    CHIP_R600,
    CHIP_RV610,
    CHIP_RV630,
    CHIP_RV670,
    CHIP_RV620,
    CHIP_RV635,
    CHIP_RS780,
    CHIP_RS880,
    CHIP_RV770,
    CHIP_RV730,
    CHIP_RV710,
    CHIP_RV740,
    CHIP_CEDAR,
    CHIP_REDWOOD,
    CHIP_JUNIPER,
    CHIP_CYPRESS,
    CHIP_HEMLOCK,
    CHIP_PALM,
    CHIP_SUMO,
    CHIP_SUMO2,
    CHIP_BARTS,
    CHIP_TURKS,
    CHIP_CAICOS,
    CHIP_CAYMAN,
    CHIP_ARUBA,
    CHIP_TAHITI,
    CHIP_PITCAIRN,
    CHIP_VERDE,
    CHIP_OLAND,
    CHIP_HAINAN,
    CHIP_BONAIRE,
    CHIP_KAVERI,
    CHIP_KABINI,
    CHIP_HAWAII,
    CHIP_MULLINS,
    CHIP_TONGA,
    CHIP_ICELAND,
    CHIP_CARRIZO,
    CHIP_FIJI,
    CHIP_STONEY,
    CHIP_POLARIS10,
    CHIP_POLARIS11,
    CHIP_LAST,
};

enum chip_class {
    CLASS_UNKNOWN = 0,
    R300,
    R400,
    R500,
    R600,
    R700,
    EVERGREEN,
    CAYMAN,
    SI,
    CIK,
    VI,
};

enum ring_type {
    RING_GFX = 0,
    RING_COMPUTE,
    RING_DMA,
    RING_UVD,
    RING_VCE,
    RING_LAST,
};

struct radeon_winsys_cs {
    unsigned cdw;  /* Number of used dwords. */
    unsigned max_dw; /* Maximum number of dwords. */
    uint32_t *buf; /* The base pointer of the chunk. */
};

struct radeon_info {
    /* PCI info: domain:bus:dev:func */
    uint32_t                    pci_domain;
    uint32_t                    pci_bus;
    uint32_t                    pci_dev;
    uint32_t                    pci_func;

    /* Device info. */
    uint32_t                    pci_id;
    enum radeon_family          family;
    enum chip_class             chip_class;
    uint32_t                    gart_page_size;
    uint64_t                    gart_size;
    uint64_t                    vram_size;
    bool                        has_dedicated_vram;
    bool                     has_virtual_memory;
    bool                        gfx_ib_pad_with_type2;
    bool                     has_sdma;
    bool                     has_uvd;
    uint32_t                    vce_fw_version;
    uint32_t                    vce_harvest_config;
    uint32_t                    clock_crystal_freq;

    /* Kernel info. */
    uint32_t                    drm_major; /* version */
    uint32_t                    drm_minor;
    uint32_t                    drm_patchlevel;
    bool                     has_userptr;

    /* Shader cores. */
    uint32_t                    r600_max_quad_pipes; /* wave size / 16 */
    uint32_t                    max_shader_clock;
    uint32_t                    num_good_compute_units;
    uint32_t                    max_se; /* shader engines */
    uint32_t                    max_sh_per_se; /* shader arrays per shader engine */

    /* Render backends (color + depth blocks). */
    uint32_t                    r300_num_gb_pipes;
    uint32_t                    r300_num_z_pipes;
    uint32_t                    r600_gb_backend_map; /* R600 harvest config */
    bool                     r600_gb_backend_map_valid;
    uint32_t                    r600_num_banks;
    uint32_t                    num_render_backends;
    uint32_t                    num_tile_pipes; /* pipe count from PIPE_CONFIG */
    uint32_t                    pipe_interleave_bytes;
    uint32_t                    enabled_rb_mask; /* GCN harvest config */

    /* Tile modes. */
    uint32_t                    si_tile_mode_array[32];
    uint32_t                    cik_macrotile_mode_array[16];
};

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

struct radeon_winsys_bo;
struct radeon_winsys_fence;

struct radeon_winsys {
   void (*destroy)(struct radeon_winsys *ws);

   void (*query_info)(struct radeon_winsys *ws,
		      struct radeon_info *info);

   struct radeon_winsys_bo *(*buffer_create)(struct radeon_winsys *ws,
					     uint64_t size,
					     unsigned alignment,
					     enum radeon_bo_domain domain,
					     enum radeon_bo_flag flags);

   void (*buffer_destroy)(struct radeon_winsys_bo *bo);
   void *(*buffer_map)(struct radeon_winsys_bo *bo);


   void (*buffer_unmap)(struct radeon_winsys_bo *bo);

   struct radeon_winsys_ctx *(*ctx_create)(struct radeon_winsys *ws);
   void (*ctx_destroy)(struct radeon_winsys_ctx *ctx);

   struct radeon_winsys_cs *(*cs_create)(struct radeon_winsys *ws,
					 enum ring_type ring_type);

   void (*cs_destroy)(struct radeon_winsys_cs *cs);

   int (*cs_submit)(struct radeon_winsys_ctx *ctx,
		    struct radeon_winsys_cs *cs,
		    struct radeon_winsys_fence *fence);

   void (*cs_add_buffer)(struct radeon_winsys_cs *cs,
			 struct radeon_winsys_bo *bo,
			 uint8_t priority);

   int (*surface_init)(struct radeon_winsys *ws,
		       struct radeon_surf *surf);

   int (*surface_best)(struct radeon_winsys *ws,
		       struct radeon_surf *surf);

   struct radeon_winsys_fence *(*create_fence)();
   void (*destroy_fence)(struct radeon_winsys_fence *fence);
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

