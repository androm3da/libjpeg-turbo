/*
 * Merged upsampling/color conversion (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

/* This file is included by jdmerge-hvx.c */


/* These routines combine simple (non-fancy, i.e. non-smooth) h2v1 or h2v2
 * chroma upsampling and YCbCr -> RGB color conversion into a single function.
 *
 * As with the standalone functions, YCbCr -> RGB conversion is defined by the
 * following equations:
 *    R = Y                        + 1.40200 * (Cr - 128)
 *    G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)
 *    B = Y + 1.77200 * (Cb - 128)
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.3441467 = 11277 * 2^-15
 *    0.7141418 = 23401 * 2^-15
 *    1.4020386 = 22971 * 2^-14
 *    1.7720337 = 29033 * 2^-14
 * These constants are defined in jdmerge-hvx.c.
 *
 * To ensure correct results, rounding is used when descaling.
 */


/* Generate unique helper names for each colorspace variant.
 * jsimd_h2v1_merged_upsample_hvx is redefined by the wrapper
 * before each inclusion, so concatenating with it gives a
 * unique name per variant.
 */
#undef hvx_store_rgb
#define hvx_store_rgb \
  _HVX_CONCAT(_hvx_store_rgb, jsimd_h2v1_merged_upsample_hvx)
#undef hvx_merge_pixel_scalar
#define hvx_merge_pixel_scalar \
  _HVX_CONCAT(_hvx_merge_pix, jsimd_h2v1_merged_upsample_hvx)


/* Store 128 pixels of interleaved RGB(X) to output buffer. */
static __inline void
hvx_store_rgb(JSAMPLE *outptr, HVX_Vector v_r, HVX_Vector v_g,
              HVX_Vector v_b)
{
#if RGB_PIXELSIZE == 4
  HVX_Vector v_a = Q6_Vb_vsplat_R(0xFF);
  HVX_VectorPair rg = Q6_W_vshuff_VVR(v_g, v_r, -1);
  HVX_VectorPair ba = Q6_W_vshuff_VVR(v_a, v_b, -1);
  HVX_VectorPair out01 = Q6_W_vshuff_VVR(
    Q6_V_lo_W(ba), Q6_V_lo_W(rg), -2);
  HVX_VectorPair out23 = Q6_W_vshuff_VVR(
    Q6_V_hi_W(ba), Q6_V_hi_W(rg), -2);
  vmemu(outptr)       = Q6_V_lo_W(out01);
  vmemu(outptr + 128) = Q6_V_hi_W(out01);
  vmemu(outptr + 256) = Q6_V_lo_W(out23);
  vmemu(outptr + 384) = Q6_V_hi_W(out23);
#else /* RGB_PIXELSIZE == 3 */
  HVX_Vector v_x = Q6_V_vzero();
  HVX_VectorPair rg = Q6_W_vshuff_VVR(v_g, v_r, -1);
  HVX_VectorPair bx = Q6_W_vshuff_VVR(v_x, v_b, -1);
  HVX_VectorPair out01 = Q6_W_vshuff_VVR(
    Q6_V_lo_W(bx), Q6_V_lo_W(rg), -2);
  HVX_VectorPair out23 = Q6_W_vshuff_VVR(
    Q6_V_hi_W(bx), Q6_V_hi_W(rg), -2);

  JSAMPLE HVX_ALIGN tmp[512];
  vmem(tmp)       = Q6_V_lo_W(out01);
  vmem(tmp + 128) = Q6_V_hi_W(out01);
  vmem(tmp + 256) = Q6_V_lo_W(out23);
  vmem(tmp + 384) = Q6_V_hi_W(out23);

  int i;
  for (i = 0; i < 128; i++) {
    outptr[i * 3 + 0] = tmp[i * 4 + 0];
    outptr[i * 3 + 1] = tmp[i * 4 + 1];
    outptr[i * 3 + 2] = tmp[i * 4 + 2];
  }
#endif
}


/* Scalar fallback for one pixel of merged upsample + color convert. */
static __inline void
hvx_merge_pixel_scalar(JSAMPLE *outptr, int y, int cb, int cr)
{
  int r_add = (F_1_402 * cr + (1 << 13)) >> 14;
  int g_sub = (F_0_344 * cb + F_0_714 * cr + (1 << 14)) >> 15;
  int b_add = (F_1_772 * cb + (1 << 13)) >> 14;

  outptr[RGB_RED] = range_limit(y + r_add);
  outptr[RGB_GREEN] = range_limit(y - g_sub);
  outptr[RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
  outptr[RGB_ALPHA] = 0xFF;
#endif
}


/* Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
 */

HIDDEN void
jsimd_h2v1_merged_upsample_hvx(JDIMENSION output_width,
                                     JSAMPIMAGE input_buf,
                                     JDIMENSION in_row_group_ctr,
                                     JSAMPARRAY output_buf)
{
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];

  /* Prefetch Y, Cb, Cr input data into L2 */
  hvx_prefetch_row(inptr0, output_width);
  hvx_prefetch_row(inptr1, (output_width + 1) / 2);
  hvx_prefetch_row(inptr2, (output_width + 1) / 2);

  JDIMENSION col;
  /* HVX main loop: 64 chroma samples → 128 output pixels */
  for (col = 0; col + 128 <= output_width; col += 128) {
    /* Load 64 Cb/Cr samples */
    HVX_Vector v_cb_raw = vmemu(inptr1 + col / 2);
    HVX_Vector v_cr_raw = vmemu(inptr2 + col / 2);

    /* Unpack first 64 bytes to s16, subtract 128 */
    HVX_Vector v128 = Q6_Vh_vsplat_R(128);
    HVX_VectorPair cb_wide = Q6_Wuh_vunpack_Vub(v_cb_raw);
    HVX_Vector cb_s16 = Q6_Vh_vsub_VhVh(
      Q6_V_lo_W(cb_wide), v128);
    HVX_VectorPair cr_wide = Q6_Wuh_vunpack_Vub(v_cr_raw);
    HVX_Vector cr_s16 = Q6_Vh_vsub_VhVh(
      Q6_V_lo_W(cr_wide), v128);

    /* Compute chroma deltas (64 s16 elements) */
    HVX_Vector r_add, g_sub, b_add;
    hvx_chroma_deltas(cb_s16, cr_s16, &r_add, &g_sub, &b_add);

    /* Duplicate each s16 element for 2 Y pixels:
     * vshuff with -2 duplicates each halfword.
     */
    HVX_VectorPair r_dup =
      Q6_W_vshuff_VVR(r_add, r_add, -2);
    HVX_VectorPair g_dup =
      Q6_W_vshuff_VVR(g_sub, g_sub, -2);
    HVX_VectorPair b_dup =
      Q6_W_vshuff_VVR(b_add, b_add, -2);

    /* Load 128 Y pixels, unpack to s16 */
    HVX_Vector v_y = vmemu(inptr0 + col);
    HVX_VectorPair y_wide = Q6_Wuh_vunpack_Vub(v_y);

    /* Apply chroma deltas and store */
    HVX_Vector v_r, v_g, v_b;
    hvx_apply_chroma(
      Q6_V_lo_W(y_wide), Q6_V_hi_W(y_wide),
      Q6_V_lo_W(r_dup), Q6_V_hi_W(r_dup),
      Q6_V_lo_W(g_dup), Q6_V_hi_W(g_dup),
      Q6_V_lo_W(b_dup), Q6_V_hi_W(b_dup),
      &v_r, &v_g, &v_b);

    hvx_store_rgb(outptr + col * RGB_PIXELSIZE,
                  v_r, v_g, v_b);
  }

  /* Scalar tail */
  for (; col < output_width - 1; col += 2) {
    JDIMENSION ccol = col / 2;
    int cb = inptr1[ccol] - 128;
    int cr = inptr2[ccol] - 128;
    hvx_merge_pixel_scalar(
      outptr + col * RGB_PIXELSIZE,
      inptr0[col], cb, cr);
    hvx_merge_pixel_scalar(
      outptr + (col + 1) * RGB_PIXELSIZE,
      inptr0[col + 1], cb, cr);
  }
  /* Handle odd width (last pixel) */
  if (col < output_width) {
    int cb = inptr1[col / 2] - 128;
    int cr = inptr2[col / 2] - 128;
    hvx_merge_pixel_scalar(
      outptr + col * RGB_PIXELSIZE,
      inptr0[col], cb, cr);
  }
}


/* Upsample and color convert for the case of 2:1 horizontal and 2:1 vertical.
 */

HIDDEN void
jsimd_h2v2_merged_upsample_hvx(JDIMENSION output_width,
                                     JSAMPIMAGE input_buf,
                                     JDIMENSION in_row_group_ctr,
                                     JSAMPARRAY output_buf)
{
  JSAMPROW outptr0, outptr1;
  JSAMPROW inptr0_0, inptr0_1, inptr1, inptr2;

  inptr0_0 = input_buf[0][in_row_group_ctr * 2];
  inptr0_1 = input_buf[0][in_row_group_ctr * 2 + 1];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr0 = output_buf[0];
  outptr1 = output_buf[1];

  /* Prefetch Y (both rows), Cb, Cr input data into L2 */
  hvx_prefetch_row(inptr0_0, output_width);
  hvx_prefetch_row(inptr0_1, output_width);
  hvx_prefetch_row(inptr1, (output_width + 1) / 2);
  hvx_prefetch_row(inptr2, (output_width + 1) / 2);

  JDIMENSION col;
  /* HVX main loop: 64 chroma samples → 128 output pixels × 2 rows */
  for (col = 0; col + 128 <= output_width; col += 128) {
    /* Load 64 Cb/Cr samples */
    HVX_Vector v_cb_raw = vmemu(inptr1 + col / 2);
    HVX_Vector v_cr_raw = vmemu(inptr2 + col / 2);

    HVX_Vector v128 = Q6_Vh_vsplat_R(128);
    HVX_VectorPair cb_wide = Q6_Wuh_vunpack_Vub(v_cb_raw);
    HVX_Vector cb_s16 = Q6_Vh_vsub_VhVh(
      Q6_V_lo_W(cb_wide), v128);
    HVX_VectorPair cr_wide = Q6_Wuh_vunpack_Vub(v_cr_raw);
    HVX_Vector cr_s16 = Q6_Vh_vsub_VhVh(
      Q6_V_lo_W(cr_wide), v128);

    HVX_Vector r_add, g_sub, b_add;
    hvx_chroma_deltas(cb_s16, cr_s16,
                      &r_add, &g_sub, &b_add);

    /* Duplicate each s16 for 2 Y pixels */
    HVX_VectorPair r_dup =
      Q6_W_vshuff_VVR(r_add, r_add, -2);
    HVX_VectorPair g_dup =
      Q6_W_vshuff_VVR(g_sub, g_sub, -2);
    HVX_VectorPair b_dup =
      Q6_W_vshuff_VVR(b_add, b_add, -2);

    /* Row 0 */
    HVX_Vector v_y0 = vmemu(inptr0_0 + col);
    HVX_VectorPair y0_wide = Q6_Wuh_vunpack_Vub(v_y0);
    HVX_Vector v_r, v_g, v_b;
    hvx_apply_chroma(
      Q6_V_lo_W(y0_wide), Q6_V_hi_W(y0_wide),
      Q6_V_lo_W(r_dup), Q6_V_hi_W(r_dup),
      Q6_V_lo_W(g_dup), Q6_V_hi_W(g_dup),
      Q6_V_lo_W(b_dup), Q6_V_hi_W(b_dup),
      &v_r, &v_g, &v_b);
    hvx_store_rgb(outptr0 + col * RGB_PIXELSIZE,
                  v_r, v_g, v_b);

    /* Row 1: same chroma deltas, different Y */
    HVX_Vector v_y1 = vmemu(inptr0_1 + col);
    HVX_VectorPair y1_wide = Q6_Wuh_vunpack_Vub(v_y1);
    hvx_apply_chroma(
      Q6_V_lo_W(y1_wide), Q6_V_hi_W(y1_wide),
      Q6_V_lo_W(r_dup), Q6_V_hi_W(r_dup),
      Q6_V_lo_W(g_dup), Q6_V_hi_W(g_dup),
      Q6_V_lo_W(b_dup), Q6_V_hi_W(b_dup),
      &v_r, &v_g, &v_b);
    hvx_store_rgb(outptr1 + col * RGB_PIXELSIZE,
                  v_r, v_g, v_b);
  }

  /* Scalar tail */
  for (; col < output_width - 1; col += 2) {
    JDIMENSION ccol = col / 2;
    int cb = inptr1[ccol] - 128;
    int cr = inptr2[ccol] - 128;
    /* Row 0 */
    hvx_merge_pixel_scalar(
      outptr0 + col * RGB_PIXELSIZE,
      inptr0_0[col], cb, cr);
    hvx_merge_pixel_scalar(
      outptr0 + (col + 1) * RGB_PIXELSIZE,
      inptr0_0[col + 1], cb, cr);
    /* Row 1 */
    hvx_merge_pixel_scalar(
      outptr1 + col * RGB_PIXELSIZE,
      inptr0_1[col], cb, cr);
    hvx_merge_pixel_scalar(
      outptr1 + (col + 1) * RGB_PIXELSIZE,
      inptr0_1[col + 1], cb, cr);
  }
  /* Handle odd width */
  if (col < output_width) {
    int cb = inptr1[col / 2] - 128;
    int cr = inptr2[col / 2] - 128;
    hvx_merge_pixel_scalar(
      outptr0 + col * RGB_PIXELSIZE,
      inptr0_0[col], cb, cr);
    hvx_merge_pixel_scalar(
      outptr1 + col * RGB_PIXELSIZE,
      inptr0_1[col], cb, cr);
  }
}
