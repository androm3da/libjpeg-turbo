/*
 * Downsampling (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* Expand the right edge of a sample row by replicating the last
 * sample value.  This is needed when the image width is not a
 * multiple of the downsampling factor, since the SIMD path does
 * not call expand_right_edge() in jcsample.c.
 */
static void
expand_right_edge_simd(JSAMPARRAY image_data, int num_rows,
                       JDIMENSION image_width, JDIMENSION output_width)
{
  int numcols = (int)(output_width - image_width);
  int row;

  if (numcols > 0) {
    for (row = 0; row < num_rows; row++) {
      JSAMPROW ptr = image_data[row] + image_width;
      JSAMPLE pixval = ptr[-1];
      int count;
      for (count = numcols; count > 0; count--)
        *ptr++ = pixval;
    }
  }
}


/* Downsample component values from a single plane.
 * This version handles the common case of 2:1 horizontal and
 * 1:1 vertical, without smoothing.
 *
 * A note about the "bias" calculations: when rounding fractional
 * values to integer, we do not want to always round 0.5 up to the
 * next integer.  Instead, this code is arranged so that 0.5 will
 * be rounded up or down at alternate pixel locations (a simple
 * ordered dither pattern).
 */

HIDDEN void
jsimd_h2v1_downsample_hvx(JDIMENSION image_width,
                                int max_v_samp_factor,
                                JDIMENSION v_samp_factor,
                                JDIMENSION width_in_blocks,
                                JSAMPARRAY input_data,
                                JSAMPARRAY output_data)
{
  JSAMPROW inptr, outptr;
  JDIMENSION output_width = width_in_blocks * DCTSIZE;
  JDIMENSION outcol;
  int row, bias;

  expand_right_edge_simd(input_data, max_v_samp_factor,
                         image_width, output_width * 2);

  for (row = 0; row < (int)v_samp_factor; row++) {
    outptr = output_data[row];
    inptr = input_data[row];
    bias = 0;  /* bias = 0,1,0,1,... for successive samples */
    for (outcol = 0; outcol < output_width; outcol++) {
      *outptr++ =
        (JSAMPLE)((inptr[0] + inptr[1] + bias) >> 1);
      bias ^= 1;
      inptr += 2;
    }
  }
}


/* Downsample component values from a single plane.
 * This version handles the standard case of 2:1 horizontal and
 * 2:1 vertical, without smoothing.
 */

HIDDEN void
jsimd_h2v2_downsample_hvx(JDIMENSION image_width,
                                int max_v_samp_factor,
                                JDIMENSION v_samp_factor,
                                JDIMENSION width_in_blocks,
                                JSAMPARRAY input_data,
                                JSAMPARRAY output_data)
{
  JSAMPROW inptr0, inptr1, outptr;
  JDIMENSION output_width = width_in_blocks * DCTSIZE;
  JDIMENSION outcol;
  int inrow, outrow, bias;

  expand_right_edge_simd(input_data, max_v_samp_factor,
                         image_width, output_width * 2);

  inrow = 0;
  for (outrow = 0; outrow < (int)v_samp_factor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow + 1];
    bias = 1;  /* bias = 1,2,1,2,... for successive samples */
    for (outcol = 0; outcol < output_width; outcol++) {
      *outptr++ = (JSAMPLE)
        ((inptr0[0] + inptr0[1] +
          inptr1[0] + inptr1[1] + bias) >> 2);
      bias ^= 3;
      inptr0 += 2;
      inptr1 += 2;
    }
    inrow += 2;
  }
}
