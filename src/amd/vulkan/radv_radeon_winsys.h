#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#define CALLOC(_count, _size) calloc(_count, _size)
#ifndef CALLOC_STRUCT
#define CALLOC_STRUCT(T)   (struct T *) CALLOC(1, sizeof(struct T))
#define FREE(_ptr) free(_ptr)

#endif

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
