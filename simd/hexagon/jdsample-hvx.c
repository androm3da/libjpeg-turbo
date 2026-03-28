/*
 * Upsampling (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* Simple h2v1 upsampling: each input sample is duplicated
 * horizontally to produce two output samples.
 */

HIDDEN void
jsimd_h2v1_upsample_hvx(int max_v_samp_factor,
                              JDIMENSION output_width,
                              JSAMPARRAY input_data,
                              JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr, outptr;
  JSAMPLE invalue;
  JDIMENSION outend;
  int inrow;

  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data_ptr[0][inrow];
    outend = output_width;

    /* Prefetch next input row into L2 */
    if (inrow + 1 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        output_width / 2);

    JDIMENSION outcol;
    for (outcol = 0; outcol < outend; outcol += 2) {
      invalue = *inptr++;
      outptr[outcol] = invalue;
      outptr[outcol + 1] = invalue;
    }
  }
}


/* Simple h2v2 upsampling: each input sample is duplicated both
 * horizontally and vertically to produce a 2x2 block of output
 * samples.
 */

HIDDEN void
jsimd_h2v2_upsample_hvx(int max_v_samp_factor,
                              JDIMENSION output_width,
                              JSAMPARRAY input_data,
                              JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr, outptr0, outptr1;
  JSAMPLE invalue;
  int inrow, outrow;
  JDIMENSION outcol;

  inrow = 0;
  outrow = 0;
  while (outrow < max_v_samp_factor) {
    inptr = input_data[inrow];
    outptr0 = output_data_ptr[0][outrow];
    outptr1 = output_data_ptr[0][outrow + 1];

    /* Prefetch next input row into L2 */
    if (outrow + 2 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        output_width / 2);

    for (outcol = 0; outcol < output_width; outcol += 2) {
      invalue = *inptr++;
      outptr0[outcol] = invalue;
      outptr0[outcol + 1] = invalue;
      outptr1[outcol] = invalue;
      outptr1[outcol + 1] = invalue;
    }

    inrow++;
    outrow += 2;
  }
}


/* Fancy h2v1 upsampling: each output component value is computed
 * by blending the sample containing the component center with the
 * nearest neighboring sample, in the ratio 3:1.  For example:
 *     c1(upsampled) = 3/4 * s0 + 1/4 * s1
 *     c2(upsampled) = 3/4 * s1 + 1/4 * s0
 * The first and last component values are not blended:
 *     c0(upsampled) = s0
 *     cN(upsampled) = sLast
 */

HIDDEN void
jsimd_h2v1_fancy_upsample_hvx(
  int max_v_samp_factor,
  JDIMENSION downsampled_width,
  JSAMPARRAY input_data,
  JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr, outptr;
  int invalue;
  JDIMENSION colctr;
  int inrow;

  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data_ptr[0][inrow];

    /* Prefetch next input row into L2 */
    if (inrow + 1 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        downsampled_width);

    /* Special case for first column */
    invalue = *inptr++;
    *outptr++ = (JSAMPLE)invalue;
    *outptr++ =
      (JSAMPLE)((invalue * 3 + inptr[0] + 2) >> 2);

    for (colctr = downsampled_width - 2;
         colctr > 0; colctr--) {
      /* General case: 3/4 * nearer + 1/4 * further */
      invalue = (*inptr++) * 3;
      *outptr++ =
        (JSAMPLE)((invalue + inptr[-2] + 1) >> 2);
      *outptr++ =
        (JSAMPLE)((invalue + inptr[0] + 2) >> 2);
    }

    /* Special case for last column */
    invalue = *inptr;
    *outptr++ =
      (JSAMPLE)((invalue * 3 + inptr[-1] + 1) >> 2);
    *outptr++ = (JSAMPLE)invalue;
  }
}


/* Fancy h2v2 upsampling: 2D triangle filter.  Each output
 * component value is computed by first blending the sample
 * containing the component center with the nearest neighboring
 * samples in the same column, in the ratio 3:1, and then blending
 * each column sum with the nearest neighboring column sum, in the
 * ratio 3:1.
 */

HIDDEN void
jsimd_h2v2_fancy_upsample_hvx(
  int max_v_samp_factor,
  JDIMENSION downsampled_width,
  JSAMPARRAY input_data,
  JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr0, inptr1, outptr;
  int inrow, outrow, v;
  int thiscolsum, lastcolsum, nextcolsum;
  JDIMENSION colctr;

  inrow = outrow = 0;
  while (outrow < max_v_samp_factor) {
    /* Prefetch next neighbor row into L2 */
    if (outrow + 2 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        downsampled_width);

    for (v = 0; v < 2; v++) {
      inptr0 = input_data[inrow];
      if (v == 0)
        inptr1 = input_data[inrow - 1];
      else
        inptr1 = input_data[inrow + 1];
      outptr = output_data_ptr[0][outrow++];

      /* Special case for first column */
      thiscolsum = (*inptr0++) * 3 + (*inptr1++);
      nextcolsum = (*inptr0++) * 3 + (*inptr1++);
      *outptr++ =
        (JSAMPLE)((thiscolsum * 4 + 8) >> 4);
      *outptr++ =
        (JSAMPLE)((thiscolsum * 3 + nextcolsum + 7) >> 4);
      lastcolsum = thiscolsum;
      thiscolsum = nextcolsum;

      for (colctr = downsampled_width - 2;
           colctr > 0; colctr--) {
        nextcolsum = (*inptr0++) * 3 + (*inptr1++);
        *outptr++ =
          (JSAMPLE)((thiscolsum * 3 +
                     lastcolsum + 8) >> 4);
        *outptr++ =
          (JSAMPLE)((thiscolsum * 3 +
                     nextcolsum + 7) >> 4);
        lastcolsum = thiscolsum;
        thiscolsum = nextcolsum;
      }

      /* Special case for last column */
      *outptr++ =
        (JSAMPLE)((thiscolsum * 3 +
                   lastcolsum + 8) >> 4);
      *outptr++ =
        (JSAMPLE)((thiscolsum * 4 + 7) >> 4);

      if (outrow >= max_v_samp_factor)
        break;
    }
    inrow++;
  }
}


/* Fancy h1v2 upsampling: vertical-only triangle filter. */

HIDDEN void
jsimd_h1v2_fancy_upsample_hvx(
  int max_v_samp_factor,
  JDIMENSION downsampled_width,
  JSAMPARRAY input_data,
  JSAMPARRAY *output_data_ptr)
{
  JSAMPROW inptr0, inptr1, outptr;
  int inrow, outrow, v;
  int thiscolsum, bias;
  JDIMENSION colctr;

  inrow = 0;
  outrow = 0;
  while (outrow < max_v_samp_factor) {
    /* Prefetch next neighbor row into L2 */
    if (outrow + 2 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        downsampled_width);

    for (v = 0; v < 2; v++) {
      inptr0 = input_data[inrow];
      if (v == 0) {
        inptr1 = input_data[inrow - 1];
        bias = 1;
      } else {
        inptr1 = input_data[inrow + 1];
        bias = 2;
      }
      outptr = output_data_ptr[0][outrow++];

      for (colctr = 0; colctr < downsampled_width;
           colctr++) {
        thiscolsum = (*inptr0++) * 3 + (*inptr1++);
        *outptr++ =
          (JSAMPLE)((thiscolsum + bias) >> 2);
      }
    }
    inrow++;
  }
}
