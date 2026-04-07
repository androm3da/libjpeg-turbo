/*
 * YCbCr to RGB colorspace conversion (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"

#define ALIGN(alignment)  __attribute__((aligned(alignment)))


/* YCbCr -> RGB conversion constants */

#define F_0_344  11277  /* 0.3441467 = 11277 * 2^-15 */
#define F_0_714  23401  /* 0.7141418 = 23401 * 2^-15 */
#define F_1_402  22971  /* 1.4020386 = 22971 * 2^-14 */
#define F_1_772  29033  /* 1.7720337 = 29033 * 2^-14 */


/* Process 128 pixels of YCbCr -> RGB color conversion using HVX.
 * Computes R, G, B channel vectors from Y, Cb, Cr input vectors.
 *
 * Parameters (all HVX_Vector, 128 x u8):
 *   v_y, v_cb, v_cr  -- input channels
 * Outputs (HVX_Vector pointers, 128 x u8 each):
 *   v_r, v_g, v_b    -- output channels, saturated to [0, 255]
 */
static __inline void
hvx_ycc_to_rgb(HVX_Vector v_y, HVX_Vector v_cb, HVX_Vector v_cr,
               HVX_Vector *v_r, HVX_Vector *v_g, HVX_Vector *v_b)
{
  /* Unpack Y from u8 to s16 (two halves of 64 elements each) */
  HVX_VectorPair y_wide = Q6_Wuh_vunpack_Vub(v_y);
  HVX_Vector y_lo = Q6_V_lo_W(y_wide);
  HVX_Vector y_hi = Q6_V_hi_W(y_wide);

  /* Unpack Cb, Cr from u8 to s16, then subtract 128 */
  HVX_Vector v128 = Q6_Vh_vsplat_R(128);
  HVX_VectorPair cb_wide = Q6_Wuh_vunpack_Vub(v_cb);
  HVX_Vector cb_lo = Q6_Vh_vsub_VhVh(Q6_V_lo_W(cb_wide), v128);
  HVX_Vector cb_hi = Q6_Vh_vsub_VhVh(Q6_V_hi_W(cb_wide), v128);

  HVX_VectorPair cr_wide = Q6_Wuh_vunpack_Vub(v_cr);
  HVX_Vector cr_lo = Q6_Vh_vsub_VhVh(Q6_V_lo_W(cr_wide), v128);
  HVX_Vector cr_hi = Q6_Vh_vsub_VhVh(Q6_V_hi_W(cr_wide), v128);

  /* R = Y + (22971 * Cr + 8192) >> 14 */
  HVX_VectorPair r_prod_lo =
    Q6_Ww_vmpy_VhRh(cr_lo, VMPY_CONST(F_1_402));
  HVX_VectorPair r_prod_hi =
    Q6_Ww_vmpy_VhRh(cr_hi, VMPY_CONST(F_1_402));
  HVX_Vector rnd14 = Q6_Vw_vsplat_R(1 << 13);
  HVX_Vector r_add_lo = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(r_prod_lo), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(r_prod_lo), rnd14), 14);
  HVX_Vector r_add_hi = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(r_prod_hi), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(r_prod_hi), rnd14), 14);
  HVX_Vector r_lo = Q6_Vh_vadd_VhVh(y_lo, r_add_lo);
  HVX_Vector r_hi = Q6_Vh_vadd_VhVh(y_hi, r_add_hi);

  /* B = Y + (29033 * Cb + 8192) >> 14 */
  HVX_VectorPair b_prod_lo =
    Q6_Ww_vmpy_VhRh(cb_lo, VMPY_CONST(F_1_772));
  HVX_VectorPair b_prod_hi =
    Q6_Ww_vmpy_VhRh(cb_hi, VMPY_CONST(F_1_772));
  HVX_Vector b_add_lo = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(b_prod_lo), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(b_prod_lo), rnd14), 14);
  HVX_Vector b_add_hi = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(b_prod_hi), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(b_prod_hi), rnd14), 14);
  HVX_Vector b_lo = Q6_Vh_vadd_VhVh(y_lo, b_add_lo);
  HVX_Vector b_hi = Q6_Vh_vadd_VhVh(y_hi, b_add_hi);

  /* G = Y - (11277 * Cb + 23401 * Cr + 16384) >> 15 */
  HVX_VectorPair gcb_lo =
    Q6_Ww_vmpy_VhRh(cb_lo, VMPY_CONST(F_0_344));
  HVX_VectorPair gcb_hi =
    Q6_Ww_vmpy_VhRh(cb_hi, VMPY_CONST(F_0_344));
  HVX_VectorPair gcr_lo =
    Q6_Ww_vmpy_VhRh(cr_lo, VMPY_CONST(F_0_714));
  HVX_VectorPair gcr_hi =
    Q6_Ww_vmpy_VhRh(cr_hi, VMPY_CONST(F_0_714));
  HVX_Vector rnd15 = Q6_Vw_vsplat_R(1 << 14);

  HVX_Vector g_sum_lo_lo = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(gcb_lo), Q6_V_lo_W(gcr_lo)),
    rnd15);
  HVX_Vector g_sum_lo_hi = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(gcb_lo), Q6_V_hi_W(gcr_lo)),
    rnd15);
  HVX_Vector g_sum_hi_lo = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(gcb_hi), Q6_V_lo_W(gcr_hi)),
    rnd15);
  HVX_Vector g_sum_hi_hi = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(gcb_hi), Q6_V_hi_W(gcr_hi)),
    rnd15);

  HVX_Vector g_sub_lo =
    Q6_Vh_vasr_VwVwR_sat(g_sum_lo_hi, g_sum_lo_lo, 15);
  HVX_Vector g_sub_hi =
    Q6_Vh_vasr_VwVwR_sat(g_sum_hi_hi, g_sum_hi_lo, 15);

  HVX_Vector g_lo = Q6_Vh_vsub_VhVh(y_lo, g_sub_lo);
  HVX_Vector g_hi = Q6_Vh_vsub_VhVh(y_hi, g_sub_hi);

  /* Clamp to [0,255] and pack s16 -> u8 (vpacke, no interleave) */
  *v_r = hvx_pack_u8(r_lo, r_hi);
  *v_g = hvx_pack_u8(g_lo, g_hi);
  *v_b = hvx_pack_u8(b_lo, b_hi);
}


/* Include inline routines for colorspace extensions. */

#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED  EXT_RGB_RED
#define RGB_GREEN  EXT_RGB_GREEN
#define RGB_BLUE  EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extrgb_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_RGBX_RED
#define RGB_GREEN  EXT_RGBX_GREEN
#define RGB_BLUE  EXT_RGBX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extrgbx_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_BGR_RED
#define RGB_GREEN  EXT_BGR_GREEN
#define RGB_BLUE  EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extbgr_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_BGRX_RED
#define RGB_GREEN  EXT_BGRX_GREEN
#define RGB_BLUE  EXT_BGRX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extbgrx_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_XBGR_RED
#define RGB_GREEN  EXT_XBGR_GREEN
#define RGB_BLUE  EXT_XBGR_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extxbgr_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_XRGB_RED
#define RGB_GREEN  EXT_XRGB_GREEN
#define RGB_BLUE  EXT_XRGB_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extxrgb_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx
