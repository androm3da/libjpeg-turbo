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

/* Compatibility: older Hexagon SDK toolchains define Q6_V_vsplat_R
 * (word splat) but not Q6_Vw_vsplat_R.  Newer LLVM cross-compilers
 * provide both.
 */
#ifndef Q6_Vw_vsplat_R
#define Q6_Vw_vsplat_R(x)  Q6_V_vsplat_R(x)
#endif

/* Token pasting helpers for generating unique function names in
 * multi-included ext files (jdcolext-hvx.c, jdmrgext-hvx.c).
 */
#define _HVX_CONCAT2(a, b)  a ## _ ## b
#define _HVX_CONCAT(a, b)   _HVX_CONCAT2(a, b)

/* Q6_Ww_vmpy_VhRh(Vu, Rt) multiplies even halfwords of Vu by Rt.h[0]
 * and odd halfwords by Rt.h[1].  When using a single scalar constant,
 * it must be replicated into both halfword slots of the Rt register.
 */
#define VMPY_CONST(x)  ((x) | ((x) << 16))

/* Clamp s16 lanes to [0, 255] and pack to u8 via vpacke (concatenation).
 * Unlike Q6_Vub_vsat_VhVh, vpacke does NOT interleave the two halves:
 *   output[0..63]    = low bytes of lo.h[0..63]
 *   output[64..127]  = low bytes of hi.h[0..63]
 */
static __inline HVX_Vector
hvx_pack_u8(HVX_Vector lo, HVX_Vector hi)
{
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  HVX_Vector v0 = Q6_V_vzero();
  lo = Q6_Vh_vmax_VhVh(lo, v0);
  lo = Q6_Vh_vmin_VhVh(lo, v255);
  hi = Q6_Vh_vmax_VhVh(hi, v0);
  hi = Q6_Vh_vmin_VhVh(hi, v255);
  return Q6_Vb_vpacke_VhVh(hi, lo);
}

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
