#pragma once

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "r600d_common.h"

static inline void radeon_set_config_reg_seq(struct radeon_winsys_cs *cs, unsigned reg, unsigned num)
{
        assert(reg < R600_CONTEXT_REG_OFFSET);
        assert(cs->cdw + 2 + num <= cs->max_dw);
        radeon_emit(cs, PKT3(PKT3_SET_CONFIG_REG, num, 0));
        radeon_emit(cs, (reg - R600_CONFIG_REG_OFFSET) >> 2);
}

static inline void radeon_set_config_reg(struct radeon_winsys_cs *cs, unsigned reg, unsigned value)
{
        radeon_set_config_reg_seq(cs, reg, 1);
        radeon_emit(cs, value);
}

static inline void radeon_set_context_reg_seq(struct radeon_winsys_cs *cs, unsigned reg, unsigned num)
{
        assert(reg >= R600_CONTEXT_REG_OFFSET);
        assert(cs->cdw + 2 + num <= cs->max_dw);
        radeon_emit(cs, PKT3(PKT3_SET_CONTEXT_REG, num, 0));
        radeon_emit(cs, (reg - R600_CONTEXT_REG_OFFSET) >> 2);
}

static inline void radeon_set_context_reg(struct radeon_winsys_cs *cs, unsigned reg, unsigned value)
{
        radeon_set_context_reg_seq(cs, reg, 1);
        radeon_emit(cs, value);
}

static inline void radeon_set_sh_reg_seq(struct radeon_winsys_cs *cs, unsigned reg, unsigned num)
{
	assert(reg >= SI_SH_REG_OFFSET && reg < SI_SH_REG_END);
	assert(cs->cdw + 2 + num <= cs->max_dw);
	radeon_emit(cs, PKT3(PKT3_SET_SH_REG, num, 0));
	radeon_emit(cs, (reg - SI_SH_REG_OFFSET) >> 2);
}

static inline void radeon_set_sh_reg(struct radeon_winsys_cs *cs, unsigned reg, unsigned value)
{
	radeon_set_sh_reg_seq(cs, reg, 1);
	radeon_emit(cs, value);
}

static inline void radeon_set_uconfig_reg_seq(struct radeon_winsys_cs *cs, unsigned reg, unsigned num)
{
	assert(reg >= CIK_UCONFIG_REG_OFFSET && reg < CIK_UCONFIG_REG_END);
	assert(cs->cdw + 2 + num <= cs->max_dw);
	radeon_emit(cs, PKT3(PKT3_SET_UCONFIG_REG, num, 0));
	radeon_emit(cs, (reg - CIK_UCONFIG_REG_OFFSET) >> 2);
}

static inline void radeon_set_uconfig_reg(struct radeon_winsys_cs *cs, unsigned reg, unsigned value)
{
	radeon_set_uconfig_reg_seq(cs, reg, 1);
	radeon_emit(cs, value);
}

