#pragma once

#include <string.h>
#include <stdint.h>
struct radeon_winsys_cs_chunk {
    unsigned cdw;  /* Number of used dwords. */
    unsigned max_dw; /* Maximum number of dwords. */
    uint32_t *buf; /* The base pointer of the chunk. */
};

struct radeon_winsys_cs {
    struct radeon_winsys_cs_chunk current;
    struct radeon_winsys_cs_chunk *prev;
    unsigned                      num_prev; /* Number of previous chunks. */
    unsigned                      max_prev; /* Space in array pointed to by prev. */
    unsigned                      prev_dw; /* Total number of dwords in previous chunks. */
};

static inline void radeon_emit(struct radeon_winsys_cs *cs, uint32_t value)
{
    cs->current.buf[cs->current.cdw++] = value;
}

static inline void radeon_emit_array(struct radeon_winsys_cs *cs,
				     const uint32_t *values, unsigned count)
{
    memcpy(cs->current.buf + cs->current.cdw, values, count * 4);
    cs->current.cdw += count;
}


