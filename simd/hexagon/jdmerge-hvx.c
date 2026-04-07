/*
 * Merged upsampling/color conversion (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* YCbCr -> RGB conversion constants */

#define F_0_344  11277  /* 0.3441467 = 11277 * 2^-15 */
#define F_0_714  23401  /* 0.7141418 = 23401 * 2^-15 */
#define F_1_402  22971  /* 1.4020386 = 22971 * 2^-14 */
#define F_1_772  29033  /* 1.7720337 = 29033 * 2^-14 */


LOCAL(JSAMPLE)
range_limit(int val)
{
  if (val < 0) return 0;
  if (val > 255) return 255;
  return (JSAMPLE)val;
}


/* Compute chroma deltas for 64 Cb/Cr samples, producing r_add, g_sub,
 * b_add as s16 vectors (64 elements each).
 */
static __inline void
hvx_chroma_deltas(HVX_Vector cb_s16, HVX_Vector cr_s16,
                  HVX_Vector *r_add, HVX_Vector *g_sub,
                  HVX_Vector *b_add)
{
  HVX_Vector rnd14 = Q6_Vw_vsplat_R(1 << 13);
  HVX_Vector rnd15 = Q6_Vw_vsplat_R(1 << 14);

  /* r_add = (22971 * Cr + 8192) >> 14 */
  HVX_VectorPair rp =
    Q6_Ww_vmpy_VhRh(cr_s16, VMPY_CONST(F_1_402));
  *r_add = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(rp), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(rp), rnd14), 14);

  /* b_add = (29033 * Cb + 8192) >> 14 */
  HVX_VectorPair bp =
    Q6_Ww_vmpy_VhRh(cb_s16, VMPY_CONST(F_1_772));
  *b_add = Q6_Vh_vasr_VwVwR_sat(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(bp), rnd14),
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(bp), rnd14), 14);

  /* g_sub = (11277 * Cb + 23401 * Cr + 16384) >> 15 */
  HVX_VectorPair gcb =
    Q6_Ww_vmpy_VhRh(cb_s16, VMPY_CONST(F_0_344));
  HVX_VectorPair gcr =
    Q6_Ww_vmpy_VhRh(cr_s16, VMPY_CONST(F_0_714));
  HVX_Vector gs_lo = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_lo_W(gcb), Q6_V_lo_W(gcr)), rnd15);
  HVX_Vector gs_hi = Q6_Vw_vadd_VwVw(
    Q6_Vw_vadd_VwVw(Q6_V_hi_W(gcb), Q6_V_hi_W(gcr)), rnd15);
  *g_sub = Q6_Vh_vasr_VwVwR_sat(gs_hi, gs_lo, 15);
}


/* Apply chroma deltas to 128 Y pixels (two 64-element s16 halves)
 * and produce packed u8 R, G, B vectors.
 */
static __inline void
hvx_apply_chroma(HVX_Vector y_lo, HVX_Vector y_hi,
                 HVX_Vector r_add_lo, HVX_Vector r_add_hi,
                 HVX_Vector g_sub_lo, HVX_Vector g_sub_hi,
                 HVX_Vector b_add_lo, HVX_Vector b_add_hi,
                 HVX_Vector *v_r, HVX_Vector *v_g,
                 HVX_Vector *v_b)
{
  HVX_Vector r_lo = Q6_Vh_vadd_VhVh(y_lo, r_add_lo);
  HVX_Vector r_hi = Q6_Vh_vadd_VhVh(y_hi, r_add_hi);
  HVX_Vector g_lo = Q6_Vh_vsub_VhVh(y_lo, g_sub_lo);
  HVX_Vector g_hi = Q6_Vh_vsub_VhVh(y_hi, g_sub_hi);
  HVX_Vector b_lo = Q6_Vh_vadd_VhVh(y_lo, b_add_lo);
  HVX_Vector b_hi = Q6_Vh_vadd_VhVh(y_hi, b_add_hi);

  *v_r = hvx_pack_u8(r_lo, r_hi);
  *v_g = hvx_pack_u8(g_lo, g_hi);
  *v_b = hvx_pack_u8(b_lo, b_hi);
}


/* Include inline routines for colorspace extensions. */

#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED  EXT_RGB_RED
#define RGB_GREEN  EXT_RGB_GREEN
#define RGB_BLUE  EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extrgb_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extrgb_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx

#define RGB_RED  EXT_RGBX_RED
#define RGB_GREEN  EXT_RGBX_GREEN
#define RGB_BLUE  EXT_RGBX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extrgbx_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extrgbx_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx

#define RGB_RED  EXT_BGR_RED
#define RGB_GREEN  EXT_BGR_GREEN
#define RGB_BLUE  EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extbgr_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extbgr_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx

#define RGB_RED  EXT_BGRX_RED
#define RGB_GREEN  EXT_BGRX_GREEN
#define RGB_BLUE  EXT_BGRX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extbgrx_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extbgrx_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx

#define RGB_RED  EXT_XBGR_RED
#define RGB_GREEN  EXT_XBGR_GREEN
#define RGB_BLUE  EXT_XBGR_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extxbgr_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extxbgr_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx

#define RGB_RED  EXT_XRGB_RED
#define RGB_GREEN  EXT_XRGB_GREEN
#define RGB_BLUE  EXT_XRGB_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_h2v1_merged_upsample_hvx  jsimd_h2v1_extxrgb_merged_upsample_hvx
#define jsimd_h2v2_merged_upsample_hvx  jsimd_h2v2_extxrgb_merged_upsample_hvx
#include "jdmrgext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_h2v1_merged_upsample_hvx
#undef jsimd_h2v2_merged_upsample_hvx
