/* command buffer handling for SI */

#include "radv_private.h"
#include "radv_amdgpu_cs.h"
#include "sid.h"
#include "radv_util.h"
#include "main/macros.h"

#define SI_GS_PER_ES 128

static void
si_write_harvested_raster_configs(struct amdgpu_winsys *ws,
                                  struct radeon_winsys_cs *cs,
				  unsigned raster_config,
				  unsigned raster_config_1)
{
	unsigned sh_per_se = MAX2(ws->info.max_sh_per_se, 1);
	unsigned num_se = MAX2(ws->info.max_se, 1);
	unsigned rb_mask = ws->info.enabled_rb_mask;
	unsigned num_rb = MIN2(ws->info.num_render_backends, 16);
	unsigned rb_per_pkr = MIN2(num_rb / num_se / sh_per_se, 2);
	unsigned rb_per_se = num_rb / num_se;
	unsigned se_mask[4];
	unsigned se;

	se_mask[0] = ((1 << rb_per_se) - 1) & rb_mask;
	se_mask[1] = (se_mask[0] << rb_per_se) & rb_mask;
	se_mask[2] = (se_mask[1] << rb_per_se) & rb_mask;
	se_mask[3] = (se_mask[2] << rb_per_se) & rb_mask;

	assert(num_se == 1 || num_se == 2 || num_se == 4);
	assert(sh_per_se == 1 || sh_per_se == 2);
	assert(rb_per_pkr == 1 || rb_per_pkr == 2);

	/* XXX: I can't figure out what the *_XSEL and *_YSEL
	 * fields are for, so I'm leaving them as their default
	 * values. */

	if ((num_se > 2) && ((!se_mask[0] && !se_mask[1]) ||
			     (!se_mask[2] && !se_mask[3]))) {
		raster_config_1 &= C_028354_SE_PAIR_MAP;

		if (!se_mask[0] && !se_mask[1]) {
			raster_config_1 |=
				S_028354_SE_PAIR_MAP(V_028354_RASTER_CONFIG_SE_PAIR_MAP_3);
		} else {
			raster_config_1 |=
				S_028354_SE_PAIR_MAP(V_028354_RASTER_CONFIG_SE_PAIR_MAP_0);
		}
	}

	for (se = 0; se < num_se; se++) {
		unsigned raster_config_se = raster_config;
		unsigned pkr0_mask = ((1 << rb_per_pkr) - 1) << (se * rb_per_se);
		unsigned pkr1_mask = pkr0_mask << rb_per_pkr;
		int idx = (se / 2) * 2;

		if ((num_se > 1) && (!se_mask[idx] || !se_mask[idx + 1])) {
			raster_config_se &= C_028350_SE_MAP;

			if (!se_mask[idx]) {
				raster_config_se |=
					S_028350_SE_MAP(V_028350_RASTER_CONFIG_SE_MAP_3);
			} else {
				raster_config_se |=
					S_028350_SE_MAP(V_028350_RASTER_CONFIG_SE_MAP_0);
			}
		}

		pkr0_mask &= rb_mask;
		pkr1_mask &= rb_mask;
		if (rb_per_se > 2 && (!pkr0_mask || !pkr1_mask)) {
			raster_config_se &= C_028350_PKR_MAP;

			if (!pkr0_mask) {
				raster_config_se |=
					S_028350_PKR_MAP(V_028350_RASTER_CONFIG_PKR_MAP_3);
			} else {
				raster_config_se |=
					S_028350_PKR_MAP(V_028350_RASTER_CONFIG_PKR_MAP_0);
			}
		}

		if (rb_per_se >= 2) {
			unsigned rb0_mask = 1 << (se * rb_per_se);
			unsigned rb1_mask = rb0_mask << 1;

			rb0_mask &= rb_mask;
			rb1_mask &= rb_mask;
			if (!rb0_mask || !rb1_mask) {
				raster_config_se &= C_028350_RB_MAP_PKR0;

				if (!rb0_mask) {
					raster_config_se |=
						S_028350_RB_MAP_PKR0(V_028350_RASTER_CONFIG_RB_MAP_3);
				} else {
					raster_config_se |=
						S_028350_RB_MAP_PKR0(V_028350_RASTER_CONFIG_RB_MAP_0);
				}
			}

			if (rb_per_se > 2) {
				rb0_mask = 1 << (se * rb_per_se + rb_per_pkr);
				rb1_mask = rb0_mask << 1;
				rb0_mask &= rb_mask;
				rb1_mask &= rb_mask;
				if (!rb0_mask || !rb1_mask) {
					raster_config_se &= C_028350_RB_MAP_PKR1;

					if (!rb0_mask) {
						raster_config_se |=
							S_028350_RB_MAP_PKR1(V_028350_RASTER_CONFIG_RB_MAP_3);
					} else {
						raster_config_se |=
							S_028350_RB_MAP_PKR1(V_028350_RASTER_CONFIG_RB_MAP_0);
					}
				}
			}
		}

		/* GRBM_GFX_INDEX has a different offset on SI and CI+ */
		if (ws->info.chip_class < CIK)
			radeon_set_config_reg(cs, GRBM_GFX_INDEX,
				       SE_INDEX(se) | SH_BROADCAST_WRITES |
				       INSTANCE_BROADCAST_WRITES);
		else
			radeon_set_uconfig_reg(cs, R_030800_GRBM_GFX_INDEX,
				       S_030800_SE_INDEX(se) | S_030800_SH_BROADCAST_WRITES(1) |
				       S_030800_INSTANCE_BROADCAST_WRITES(1));
		radeon_set_context_reg(cs, R_028350_PA_SC_RASTER_CONFIG, raster_config_se);
		if (ws->info.chip_class >= CIK)
                   radeon_set_context_reg(cs, R_028354_PA_SC_RASTER_CONFIG_1, raster_config_1);
	}

	/* GRBM_GFX_INDEX has a different offset on SI and CI+ */
	if (ws->info.chip_class < CIK)
           radeon_set_config_reg(cs, GRBM_GFX_INDEX,
			       SE_BROADCAST_WRITES | SH_BROADCAST_WRITES |
			       INSTANCE_BROADCAST_WRITES);
	else
           radeon_set_uconfig_reg(cs, R_030800_GRBM_GFX_INDEX,
			       S_030800_SE_BROADCAST_WRITES(1) | S_030800_SH_BROADCAST_WRITES(1) |
			       S_030800_INSTANCE_BROADCAST_WRITES(1));
}

void si_init_config(struct amdgpu_winsys *ws,
		    struct radeon_winsys_cs *cs)
{
   unsigned num_rb = MIN2(ws->info.num_render_backends, 16);
   unsigned rb_mask = ws->info.enabled_rb_mask;
   unsigned raster_config, raster_config_1;
   int i;

   radeon_emit(cs, PKT3(PKT3_CONTEXT_CONTROL, 1, 0));
   radeon_emit(cs, CONTEXT_CONTROL_LOAD_ENABLE(1));
   radeon_emit(cs, CONTEXT_CONTROL_SHADOW_ENABLE(1));

   radeon_set_context_reg(cs, R_028A18_VGT_HOS_MAX_TESS_LEVEL, fui(64));
   radeon_set_context_reg(cs, R_028A1C_VGT_HOS_MIN_TESS_LEVEL, fui(0));

   /* FIXME calculate these values somehow ??? */
   radeon_set_context_reg(cs, R_028A54_VGT_GS_PER_ES, SI_GS_PER_ES);
   radeon_set_context_reg(cs, R_028A58_VGT_ES_PER_GS, 0x40);
   radeon_set_context_reg(cs, R_028A5C_VGT_GS_PER_VS, 0x2);

   radeon_set_context_reg(cs, R_028A8C_VGT_PRIMITIVEID_RESET, 0x0);
   radeon_set_context_reg(cs, R_028B28_VGT_STRMOUT_DRAW_OPAQUE_OFFSET, 0);

   radeon_set_context_reg(cs, R_028B98_VGT_STRMOUT_BUFFER_CONFIG, 0x0);
   radeon_set_context_reg(cs, R_028AB8_VGT_VTX_CNT_EN, 0x0);
   if (ws->info.chip_class < CIK)
      radeon_set_config_reg(cs, R_008A14_PA_CL_ENHANCE, S_008A14_NUM_CLIP_SEQ(3) |
                     S_008A14_CLIP_VTX_REORDER_ENA(1));

   radeon_set_context_reg(cs, R_028BD4_PA_SC_CENTROID_PRIORITY_0, 0x76543210);
   radeon_set_context_reg(cs, R_028BD8_PA_SC_CENTROID_PRIORITY_1, 0xfedcba98);

   radeon_set_context_reg(cs, R_02882C_PA_SU_PRIM_FILTER_CNTL, 0);

   for (i = 0; i < 16; i++) {
      radeon_set_context_reg(cs, R_0282D0_PA_SC_VPORT_ZMIN_0 + i*8, 0);
      radeon_set_context_reg(cs, R_0282D4_PA_SC_VPORT_ZMAX_0 + i*8, fui(1.0));
   }

   switch (ws->info.family) {
   case CHIP_TAHITI:
   case CHIP_PITCAIRN:
      raster_config = 0x2a00126a;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_VERDE:
      raster_config = 0x0000124a;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_OLAND:
      raster_config = 0x00000082;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_HAINAN:
      raster_config = 0x00000000;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_BONAIRE:
      raster_config = 0x16000012;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_HAWAII:
      raster_config = 0x3a00161a;
      raster_config_1 = 0x0000002e;
      break;
   case CHIP_FIJI:
      if (ws->info.cik_macrotile_mode_array[0] == 0x000000e8) {
         /* old kernels with old tiling config */
         raster_config = 0x16000012;
         raster_config_1 = 0x0000002a;
      } else {
         raster_config = 0x3a00161a;
         raster_config_1 = 0x0000002e;
      }
      break;
   case CHIP_POLARIS10:
      raster_config = 0x16000012;
      raster_config_1 = 0x0000002a;
      break;
   case CHIP_POLARIS11:
      raster_config = 0x16000012;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_TONGA:
      raster_config = 0x16000012;
      raster_config_1 = 0x0000002a;
      break;
   case CHIP_ICELAND:
      if (num_rb == 1)
         raster_config = 0x00000000;
      else
         raster_config = 0x00000002;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_CARRIZO:
      raster_config = 0x00000002;
      raster_config_1 = 0x00000000;
      break;
   case CHIP_KAVERI:
      /* KV should be 0x00000002, but that causes problems with radeon */
      raster_config = 0x00000000; /* 0x00000002 */
      raster_config_1 = 0x00000000;
      break;
   case CHIP_KABINI:
   case CHIP_MULLINS:
   case CHIP_STONEY:
      raster_config = 0x00000000;
      raster_config_1 = 0x00000000;
      break;
   default:
      fprintf(stderr,
              "radeonsi: Unknown GPU, using 0 for raster_config\n");
      raster_config = 0x00000000;
      raster_config_1 = 0x00000000;
      break;
   }

   /* Always use the default config when all backends are enabled
    * (or when we failed to determine the enabled backends).
    */
   if (!rb_mask || util_bitcount(rb_mask) >= num_rb) {
      radeon_set_context_reg(cs, R_028350_PA_SC_RASTER_CONFIG,
                     raster_config);
      if (ws->info.chip_class >= CIK)
         radeon_set_context_reg(cs, R_028354_PA_SC_RASTER_CONFIG_1,
                        raster_config_1);
   } else {
      si_write_harvested_raster_configs(ws, cs, raster_config, raster_config_1);
   }

   radeon_set_context_reg(cs, R_028204_PA_SC_WINDOW_SCISSOR_TL, S_028204_WINDOW_OFFSET_DISABLE(1));
   radeon_set_context_reg(cs, R_028240_PA_SC_GENERIC_SCISSOR_TL, S_028240_WINDOW_OFFSET_DISABLE(1));
   radeon_set_context_reg(cs, R_028244_PA_SC_GENERIC_SCISSOR_BR,
                  S_028244_BR_X(16384) | S_028244_BR_Y(16384));
   radeon_set_context_reg(cs, R_028030_PA_SC_SCREEN_SCISSOR_TL, 0);
   radeon_set_context_reg(cs, R_028034_PA_SC_SCREEN_SCISSOR_BR,
                  S_028034_BR_X(16384) | S_028034_BR_Y(16384));

   radeon_set_context_reg(cs, R_02820C_PA_SC_CLIPRECT_RULE, 0xFFFF);
   radeon_set_context_reg(cs, R_028230_PA_SC_EDGERULE, 0xAAAAAAAA);
   /* PA_SU_HARDWARE_SCREEN_OFFSET must be 0 due to hw bug on SI */
   radeon_set_context_reg(cs, R_028234_PA_SU_HARDWARE_SCREEN_OFFSET, 0);
   radeon_set_context_reg(cs, R_028820_PA_CL_NANINF_CNTL, 0);
   radeon_set_context_reg(cs, R_028AC0_DB_SRESULTS_COMPARE_STATE0, 0x0);
   radeon_set_context_reg(cs, R_028AC4_DB_SRESULTS_COMPARE_STATE1, 0x0);
   radeon_set_context_reg(cs, R_028AC8_DB_PRELOAD_CONTROL, 0x0);
   radeon_set_context_reg(cs, R_02800C_DB_RENDER_OVERRIDE,
                  S_02800C_FORCE_HIS_ENABLE0(V_02800C_FORCE_DISABLE) |
                  S_02800C_FORCE_HIS_ENABLE1(V_02800C_FORCE_DISABLE));

   radeon_set_context_reg(cs, R_028400_VGT_MAX_VTX_INDX, ~0);
   radeon_set_context_reg(cs, R_028404_VGT_MIN_VTX_INDX, 0);
   radeon_set_context_reg(cs, R_028408_VGT_INDX_OFFSET, 0);

   if (ws->info.chip_class >= CIK) {
      radeon_set_sh_reg(cs, R_00B41C_SPI_SHADER_PGM_RSRC3_HS, 0);
      radeon_set_sh_reg(cs, R_00B31C_SPI_SHADER_PGM_RSRC3_ES, S_00B31C_CU_EN(0xffff));
      radeon_set_sh_reg(cs, R_00B21C_SPI_SHADER_PGM_RSRC3_GS, S_00B21C_CU_EN(0xffff));

      if (ws->info.num_good_compute_units /
          (ws->info.max_se * ws->info.max_sh_per_se) <= 4) {
         /* Too few available compute units per SH. Disallowing
          * VS to run on CU0 could hurt us more than late VS
          * allocation would help.
          *
          * LATE_ALLOC_VS = 2 is the highest safe number.
          */
         radeon_set_sh_reg(cs, R_00B51C_SPI_SHADER_PGM_RSRC3_LS, S_00B51C_CU_EN(0xffff));
         radeon_set_sh_reg(cs, R_00B118_SPI_SHADER_PGM_RSRC3_VS, S_00B118_CU_EN(0xffff));
         radeon_set_sh_reg(cs, R_00B11C_SPI_SHADER_LATE_ALLOC_VS, S_00B11C_LIMIT(2));
      } else {
         /* Set LATE_ALLOC_VS == 31. It should be less than
          * the number of scratch waves. Limitations:
          * - VS can't execute on CU0.
          * - If HS writes outputs to LDS, LS can't execute on CU0.
          */
         radeon_set_sh_reg(cs, R_00B51C_SPI_SHADER_PGM_RSRC3_LS, S_00B51C_CU_EN(0xfffe));
         radeon_set_sh_reg(cs, R_00B118_SPI_SHADER_PGM_RSRC3_VS, S_00B118_CU_EN(0xfffe));
         radeon_set_sh_reg(cs, R_00B11C_SPI_SHADER_LATE_ALLOC_VS, S_00B11C_LIMIT(31));
      }

      radeon_set_sh_reg(cs, R_00B01C_SPI_SHADER_PGM_RSRC3_PS, S_00B01C_CU_EN(0xffff));
   }

   if (ws->info.chip_class >= VI) {
      radeon_set_context_reg(cs, R_028424_CB_DCC_CONTROL,
                     S_028424_OVERWRITE_COMBINER_MRT_SHARING_DISABLE(1) |
                     S_028424_OVERWRITE_COMBINER_WATERMARK(4));
      radeon_set_context_reg(cs, R_028C58_VGT_VERTEX_REUSE_BLOCK_CNTL, 30);
      radeon_set_context_reg(cs, R_028C5C_VGT_OUT_DEALLOC_CNTL, 32);
      radeon_set_context_reg(cs, R_028B50_VGT_TESS_DISTRIBUTION,
                     S_028B50_ACCUM_ISOLINE(32) |
                     S_028B50_ACCUM_TRI(11) |
                     S_028B50_ACCUM_QUAD(11) |
                     S_028B50_DONUT_SPLIT(16));
   }

   if (ws->info.family == CHIP_STONEY)
      radeon_set_context_reg(cs, R_028C40_PA_SC_SHADER_CONTROL, 0);

}
