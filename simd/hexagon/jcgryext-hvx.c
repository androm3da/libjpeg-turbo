/*
 * Grayscale colorspace conversion (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

/* This file is included by jcgray-hvx.c */


/* RGB -> Grayscale conversion is defined by the following
 * equation:
 *    Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *
 * Avoid floating point arithmetic by using shifted integer
 * constants:
 *    0.29899597 = 19595 * 2^-16
 *    0.58700561 = 38470 * 2^-16
 *    0.11399841 =  7471 * 2^-16
 * These constants are defined in jcgray-hvx.c
 *
 * This is the same computation as the Y portion of RGB -> YCbCr.
 */

HIDDEN void
jsimd_rgb_gray_convert_hvx(JDIMENSION image_width,
                                 JSAMPARRAY input_buf,
                                 JSAMPIMAGE output_buf,
                                 JDIMENSION output_row,
                                 int num_rows)
{
  JSAMPROW inptr;
  JSAMPROW outptr0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    output_row++;

    JDIMENSION col;
    for (col = 0; col < image_width; col++) {
      unsigned int r = inptr[col * RGB_PIXELSIZE + RGB_RED];
      unsigned int g = inptr[col * RGB_PIXELSIZE + RGB_GREEN];
      unsigned int b = inptr[col * RGB_PIXELSIZE + RGB_BLUE];

      unsigned int y = (r * F_0_298 + g * F_0_587 +
                        b * F_0_113 + (1u << 15)) >> 16;

      outptr0[col] = (JSAMPLE)y;
    }
  }
}
