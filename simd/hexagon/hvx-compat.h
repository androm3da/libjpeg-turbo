/*
 * Hexagon HVX compatibility and helper macros
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#ifndef HVX_COMPAT_H
#define HVX_COMPAT_H

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

/* HVX vector length in bytes */
#define HVX_VLEN  128

/* Alignment macro for HVX vectors */
#define HVX_ALIGN  __attribute__((aligned(HVX_VLEN)))

/* Aligned and unaligned vector memory access.
 * vmemu() uses HVX_UVector for unaligned access --
 * the compiler generates vlalign/valign pairs
 * automatically.
 */
#define vmem(A)   *((HVX_Vector *)(A))
#define vmemu(A)  *((HVX_UVector *)(A))

/* Prefetch a contiguous buffer into L2 cache (non-blocking hint).
 * Step size is __GCC_DESTRUCTIVE_SIZE (hardware destructive
 * interference size), which matches the L1 cache line on Hexagon.
 */
static __inline void
hvx_prefetch_row(const void *addr, int len)
{
  const char *p = (const char *)addr;
  int i;
  for (i = 0; i < len; i += __GCC_DESTRUCTIVE_SIZE)
    __builtin_prefetch(p + i, 0, 0);
}

#endif /* HVX_COMPAT_H */
