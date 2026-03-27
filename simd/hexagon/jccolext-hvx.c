/*
 * RGB to YCbCr colorspace conversion (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

/* This file is included by jccolor-hvx.c */


/* RGB -> YCbCr conversion is defined by the following equations:
 *    Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *    Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + 128
 *    Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + 128
 *
 * Avoid floating point arithmetic by using shifted integer
 * constants:
 *    0.29899597 = 19595 * 2^-16
 *    0.58700561 = 38470 * 2^-16
 *    0.11399841 =  7471 * 2^-16
 *    0.16874695 = 11059 * 2^-16
 *    0.33125305 = 21709 * 2^-16
 *    0.50000000 = 32768 * 2^-16
 *    0.41868592 = 27439 * 2^-16
 *    0.08131409 =  5329 * 2^-16
 * These constants are defined in jccolor-hvx.c
 *
 * We add the fixed-point equivalent of 0.5 to Cb and Cr, which
 * effectively rounds up or down the result via integer
 * truncation.
 */

HIDDEN void
jsimd_rgb_ycc_convert_hvx(JDIMENSION image_width,
                                JSAMPARRAY input_buf,
                                JSAMPIMAGE output_buf,
                                JDIMENSION output_row,
                                int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW outptr0, outptr1, outptr2;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;

    JDIMENSION col;
    for (col = 0; col < image_width; col++) {
      unsigned int r = inptr[col * RGB_PIXELSIZE + RGB_RED];
      unsigned int g = inptr[col * RGB_PIXELSIZE + RGB_GREEN];
      unsigned int b = inptr[col * RGB_PIXELSIZE + RGB_BLUE];

      unsigned int y = (r * F_0_298 + g * F_0_587 +
                        b * F_0_113 + (1u << 15)) >> 16;
      unsigned int cb = ((128u << 16) + 32767 -
                         r * F_0_168 - g * F_0_331 +
                         b * F_0_500) >> 16;
      unsigned int cr = ((128u << 16) + 32767 +
                         r * F_0_500 - g * F_0_418 -
                         b * F_0_081) >> 16;

      outptr0[col] = (JSAMPLE)y;
      outptr1[col] = (JSAMPLE)cb;
      outptr2[col] = (JSAMPLE)cr;
    }
  }
}
