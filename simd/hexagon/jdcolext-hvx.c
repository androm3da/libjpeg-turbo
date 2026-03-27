/*
 * YCbCr to RGB colorspace conversion (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

/* This file is included by jdcolor-hvx.c */


/* YCbCr -> RGB conversion is defined by the following equations:
 *    R = Y                        + 1.40200 * (Cr - 128)
 *    G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)
 *    B = Y + 1.77200 * (Cb - 128)
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.3441467 = 11277 * 2^-15
 *    0.7141418 = 23401 * 2^-15
 *    1.4020386 = 22971 * 2^-14
 *    1.7720337 = 29033 * 2^-14
 * These constants are defined in jdcolor-hvx.c.
 *
 * To ensure correct results, rounding is used when descaling.
 */

/* Notes on safe memory access for YCbCr -> RGB conversion routines:
 *
 * Input memory buffers can be safely overread up to the next multiple
 * of ALIGN_SIZE bytes, since they are always allocated by
 * alloc_sarray() in jmemmgr.c.
 *
 * The output buffer cannot safely be written beyond out_width, since
 * output_buf points to a possibly unpadded row in the decompressed
 * image buffer allocated by the calling program.
 */

HIDDEN void
jsimd_ycc_rgb_convert_hvx(JDIMENSION out_width,
                                JSAMPIMAGE input_buf,
                                JDIMENSION input_row,
                                JSAMPARRAY output_buf,
                                int num_rows)
{
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;

    JDIMENSION col;
    JDIMENSION tail_start = out_width & ~(JDIMENSION)7;

    /* Main scalar loop: process one pixel per iteration */
    for (col = 0; col < tail_start; col++) {
      int y  = inptr0[col];
      int cb = inptr1[col] - 128;
      int cr = inptr2[col] - 128;

      int r = y + ((F_1_402 * cr + (1 << 13)) >> 14);
      int g = y - ((F_0_344 * cb +
                     F_0_714 * cr + (1 << 14)) >> 15);
      int b = y + ((F_1_772 * cb + (1 << 13)) >> 14);

      outptr[RGB_RED] =
        (JSAMPLE)(r < 0 ? 0 : (r > 255 ? 255 : r));
      outptr[RGB_GREEN] =
        (JSAMPLE)(g < 0 ? 0 : (g > 255 ? 255 : g));
      outptr[RGB_BLUE] =
        (JSAMPLE)(b < 0 ? 0 : (b > 255 ? 255 : b));
#if RGB_PIXELSIZE == 4
      outptr[RGB_ALPHA] = 0xFF;
#endif
      outptr += RGB_PIXELSIZE;
    }

    /* Tail: use tmp_buf to avoid writing past end of output */
    if (col < out_width) {
      ALIGN(16) JSAMPLE tmp_buf[8 * RGB_PIXELSIZE];
      JDIMENSION remaining = out_width - col;
      JDIMENSION i;

      for (i = 0; i < remaining; i++) {
        int y  = inptr0[col + i];
        int cb = inptr1[col + i] - 128;
        int cr = inptr2[col + i] - 128;

        int r = y + ((F_1_402 * cr + (1 << 13)) >> 14);
        int g = y - ((F_0_344 * cb +
                       F_0_714 * cr + (1 << 14)) >> 15);
        int b = y + ((F_1_772 * cb + (1 << 13)) >> 14);

        tmp_buf[i * RGB_PIXELSIZE + RGB_RED] =
          (JSAMPLE)(r < 0 ? 0 : (r > 255 ? 255 : r));
        tmp_buf[i * RGB_PIXELSIZE + RGB_GREEN] =
          (JSAMPLE)(g < 0 ? 0 : (g > 255 ? 255 : g));
        tmp_buf[i * RGB_PIXELSIZE + RGB_BLUE] =
          (JSAMPLE)(b < 0 ? 0 : (b > 255 ? 255 : b));
#if RGB_PIXELSIZE == 4
        tmp_buf[i * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
      }

      __builtin_memcpy(outptr, tmp_buf,
                        remaining * RGB_PIXELSIZE);
    }
  }
}
