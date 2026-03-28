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


/* Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
 */

HIDDEN void
jsimd_h2v1_merged_upsample_hvx(JDIMENSION output_width,
                                     JSAMPIMAGE input_buf,
                                     JDIMENSION in_row_group_ctr,
                                     JSAMPARRAY output_buf)
{
  JSAMPROW outptr;
  /* Pointers to Y, Cb, and Cr data */
  JSAMPROW inptr0, inptr1, inptr2;

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];

  /* Prefetch Y, Cb, Cr input data into L2 */
  hvx_prefetch_row(inptr0, output_width);
  hvx_prefetch_row(inptr1, (output_width + 1) / 2);
  hvx_prefetch_row(inptr2, (output_width + 1) / 2);

  for (JDIMENSION col = 0; col < output_width / 2; col++) {
    int cb = inptr1[col] - 128;
    int cr = inptr2[col] - 128;
    int r_add = (F_1_402 * cr + (1 << 13)) >> 14;
    int g_sub = (F_0_344 * cb + F_0_714 * cr + (1 << 14)) >> 15;
    int b_add = (F_1_772 * cb + (1 << 13)) >> 14;

    /* Left pixel */
    int y = inptr0[col * 2];
    outptr[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif

    /* Right pixel */
    y = inptr0[col * 2 + 1];
    outptr[(col * 2 + 1) * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr[(col * 2 + 1) * RGB_PIXELSIZE + RGB_GREEN] =
      range_limit(y - g_sub);
    outptr[(col * 2 + 1) * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr[(col * 2 + 1) * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
  }

  /* Handle odd width (last pixel) */
  if (output_width & 1) {
    JDIMENSION col = output_width / 2;
    int cb = inptr1[col] - 128;
    int cr = inptr2[col] - 128;
    int r_add = (F_1_402 * cr + (1 << 13)) >> 14;
    int g_sub = (F_0_344 * cb + F_0_714 * cr + (1 << 14)) >> 15;
    int b_add = (F_1_772 * cb + (1 << 13)) >> 14;

    int y = inptr0[col * 2];
    outptr[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
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
  /* Pointers to Y (both rows), Cb, and Cr data */
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

  for (JDIMENSION col = 0; col < output_width / 2; col++) {
    int cb = inptr1[col] - 128;
    int cr = inptr2[col] - 128;
    int r_add = (F_1_402 * cr + (1 << 13)) >> 14;
    int g_sub = (F_0_344 * cb + F_0_714 * cr + (1 << 14)) >> 15;
    int b_add = (F_1_772 * cb + (1 << 13)) >> 14;

    /* Row 0, left pixel */
    int y = inptr0_0[col * 2];
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif

    /* Row 0, right pixel */
    y = inptr0_0[col * 2 + 1];
    outptr0[(col * 2 + 1) * RGB_PIXELSIZE + RGB_RED] =
      range_limit(y + r_add);
    outptr0[(col * 2 + 1) * RGB_PIXELSIZE + RGB_GREEN] =
      range_limit(y - g_sub);
    outptr0[(col * 2 + 1) * RGB_PIXELSIZE + RGB_BLUE] =
      range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr0[(col * 2 + 1) * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif

    /* Row 1, left pixel */
    y = inptr0_1[col * 2];
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif

    /* Row 1, right pixel */
    y = inptr0_1[col * 2 + 1];
    outptr1[(col * 2 + 1) * RGB_PIXELSIZE + RGB_RED] =
      range_limit(y + r_add);
    outptr1[(col * 2 + 1) * RGB_PIXELSIZE + RGB_GREEN] =
      range_limit(y - g_sub);
    outptr1[(col * 2 + 1) * RGB_PIXELSIZE + RGB_BLUE] =
      range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr1[(col * 2 + 1) * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
  }

  /* Handle odd width (last pixel) */
  if (output_width & 1) {
    JDIMENSION col = output_width / 2;
    int cb = inptr1[col] - 128;
    int cr = inptr2[col] - 128;
    int r_add = (F_1_402 * cr + (1 << 13)) >> 14;
    int g_sub = (F_0_344 * cb + F_0_714 * cr + (1 << 14)) >> 15;
    int b_add = (F_1_772 * cb + (1 << 13)) >> 14;

    /* Row 0 */
    int y = inptr0_0[col * 2];
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr0[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif

    /* Row 1 */
    y = inptr0_1[col * 2];
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_RED] = range_limit(y + r_add);
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_GREEN] = range_limit(y - g_sub);
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_BLUE] = range_limit(y + b_add);
#if RGB_PIXELSIZE == 4
    outptr1[col * 2 * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
  }
}
