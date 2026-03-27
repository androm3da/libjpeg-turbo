/*
 * Sample data conversion and quantization (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* convsamp: Load 8x8 sample block, subtract CENTERJSAMPLE,
 * widen to int16.
 *
 * An 8x8 block of int16 = 128 bytes = 1 HVX vector.
 * We gather 8 bytes from each of 8 rows into an aligned
 * buffer, then use HVX to zero-extend and subtract
 * CENTERJSAMPLE in bulk.
 *
 * The equivalent scalar C function convsamp() can be found
 * in jcdctmgr.c.
 */

HIDDEN void
jsimd_convsamp_hvx(JSAMPARRAY sample_data,
                         JDIMENSION start_col,
                         DCTELEM *workspace)
{
  HVX_ALIGN unsigned char tmp[HVX_VLEN];
  int row;

  /* Gather 8 bytes from each row into contiguous buffer.
   * Zero the upper half to avoid garbage after vunpack.
   */
  __builtin_memset(tmp, 0, HVX_VLEN);
  for (row = 0; row < DCTSIZE; row++) {
    JSAMPROW elemptr = sample_data[row] + start_col;
    __builtin_memcpy(&tmp[row * DCTSIZE], elemptr,
                     DCTSIZE);
  }

  /* Load aligned, zero-extend, subtract center */
  HVX_Vector v_pixels = vmem(tmp);
  HVX_VectorPair w_uh = Q6_Wuh_vunpack_Vub(v_pixels);
  HVX_Vector v_uh = Q6_V_lo_W(w_uh);
  HVX_Vector v_center = Q6_Vh_vsplat_R(CENTERJSAMPLE);
  HVX_Vector v_result = Q6_Vh_vsub_VhVh(v_uh, v_center);

  /* Store to workspace (may be unaligned) */
  vmemu(workspace) = v_result;
}


/* quantize: Reciprocal-multiply-shift quantization.
 *
 * The equivalent scalar C function quantize() can be found
 * in jcdctmgr.c.
 *
 * The divisors array layout (each DCTSIZE2 = 64 elements):
 *   [0..63]   = reciprocals (UDCTELEM)
 *   [64..127] = corrections (UDCTELEM)
 *   [128..191] = scale (unused here)
 *   [192..255] = shifts (DCTELEM)
 *
 * All 64 coefficients fit in one HVX vector (64 x int16).
 * Uses vmemu() for unaligned loads/stores following the
 * QHL HVX pattern.
 */

HIDDEN void
jsimd_quantize_hvx(JCOEFPTR coef_block,
                         DCTELEM *divisors,
                         DCTELEM *workspace)
{
  UDCTELEM *recip_ptr = (UDCTELEM *)divisors;
  UDCTELEM *corr_ptr = (UDCTELEM *)divisors + DCTSIZE2;
  DCTELEM *shift_ptr = divisors + 3 * DCTSIZE2;

  /* Unaligned loads via vmemu() */
  HVX_Vector v_data = vmemu(workspace);
  HVX_Vector v_recip = vmemu(recip_ptr);
  HVX_Vector v_corr = vmemu(corr_ptr);
  HVX_Vector v_shift = vmemu(shift_ptr);

  /* Extract sign (0x0000 or 0xFFFF per element) */
  HVX_Vector v_sign = Q6_Vh_vasr_VhR(v_data, 15);

  /* abs + correction */
  HVX_Vector v_abs = Q6_Vh_vabs_Vh(v_data);
  HVX_Vector v_adj = Q6_Vh_vadd_VhVh(v_abs, v_corr);

  /* Widening unsigned multiply: even/odd element split.
   * Q6_Wuw_vmpy_VuhVuh puts even-index products in lo
   * and odd-index products in hi.
   */
  HVX_VectorPair w_prod =
    Q6_Wuw_vmpy_VuhVuh(v_adj, v_recip);

  /* Extract high 16 bits of each 32-bit product and
   * re-interleave even/odd to sequential order.
   *
   * vshuffo extracts the odd (high) halfword of each 32-bit
   * word and interleaves: result[2k] = Vv.w[k].h[1],
   * result[2k+1] = Vu.w[k].h[1].
   */
  HVX_Vector v_hi16 = Q6_Vh_vshuffo_VhVh(
    Q6_V_hi_W(w_prod), Q6_V_lo_W(w_prod));

  /* Per-element variable right shift */
  HVX_Vector v_shifted =
    Q6_Vh_vasr_VhVh(v_hi16, v_shift);

  /* Restore sign: (shifted ^ sign) - sign */
  HVX_Vector v_result = Q6_Vh_vsub_VhVh(
    Q6_V_vxor_VV(v_shifted, v_sign), v_sign);

  /* Unaligned store via vmemu() */
  vmemu(coef_block) = v_result;
}
