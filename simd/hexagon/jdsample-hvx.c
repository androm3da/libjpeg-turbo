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
  int inrow;

  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data_ptr[0][inrow];

    /* Prefetch next input row into L2 */
    if (inrow + 1 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        output_width / 2);

    JDIMENSION incol;
    JDIMENSION in_width = (output_width + 1) / 2;

    /* HVX main loop: 128 input samples → 256 output samples */
    for (incol = 0; incol + 128 <= in_width; incol += 128) {
      HVX_Vector v_in = vmemu(inptr + incol);
      /* Byte-interleave with self → duplicate each byte */
      HVX_VectorPair dup =
        Q6_W_vshuff_VVR(v_in, v_in, -1);
      vmemu(outptr + incol * 2)       = Q6_V_lo_W(dup);
      vmemu(outptr + incol * 2 + 128) = Q6_V_hi_W(dup);
    }

    /* Scalar tail */
    for (; incol < in_width; incol++) {
      JSAMPLE invalue = inptr[incol];
      outptr[incol * 2]     = invalue;
      outptr[incol * 2 + 1] = invalue;
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
  int inrow, outrow;

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

    JDIMENSION incol;
    JDIMENSION in_width = (output_width + 1) / 2;

    /* HVX main loop: 128 input → 256 output per row × 2 rows */
    for (incol = 0; incol + 128 <= in_width; incol += 128) {
      HVX_Vector v_in = vmemu(inptr + incol);
      HVX_VectorPair dup =
        Q6_W_vshuff_VVR(v_in, v_in, -1);
      HVX_Vector lo = Q6_V_lo_W(dup);
      HVX_Vector hi = Q6_V_hi_W(dup);
      /* Write same data to both output rows */
      vmemu(outptr0 + incol * 2)       = lo;
      vmemu(outptr0 + incol * 2 + 128) = hi;
      vmemu(outptr1 + incol * 2)       = lo;
      vmemu(outptr1 + incol * 2 + 128) = hi;
    }

    /* Scalar tail */
    for (; incol < in_width; incol++) {
      JSAMPLE invalue = inptr[incol];
      outptr0[incol * 2]     = invalue;
      outptr0[incol * 2 + 1] = invalue;
      outptr1[incol * 2]     = invalue;
      outptr1[incol * 2 + 1] = invalue;
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
  int inrow;

  for (inrow = 0; inrow < max_v_samp_factor; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data_ptr[0][inrow];

    /* Prefetch next input row into L2 */
    if (inrow + 1 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        downsampled_width);

    /* Special case for first sample */
    outptr[0] = inptr[0];
    outptr[1] =
      (JSAMPLE)((inptr[0] * 3 + inptr[1] + 2) >> 2);

    /* HVX main loop: process 128 input samples at a time.
     * For each input sample s[i] (i=1..N-2), produce:
     *   left  = (3 * s[i] + s[i-1] + 1) >> 2
     *   right = (3 * s[i] + s[i+1] + 2) >> 2
     */
    JDIMENSION col;
    JDIMENSION inner_end = downsampled_width - 1;

    for (col = 1; col + 128 <= inner_end; col += 128) {
      /* Load current, previous, and next samples */
      HVX_Vector v_cur  = vmemu(inptr + col);
      HVX_Vector v_prev = vmemu(inptr + col - 1);
      HVX_Vector v_next = vmemu(inptr + col + 1);

      /* Unpack to s16 for arithmetic */
      HVX_VectorPair cur_w  = Q6_Wuh_vunpack_Vub(v_cur);
      HVX_VectorPair prev_w = Q6_Wuh_vunpack_Vub(v_prev);
      HVX_VectorPair next_w = Q6_Wuh_vunpack_Vub(v_next);

      HVX_Vector cur_lo  = Q6_V_lo_W(cur_w);
      HVX_Vector cur_hi  = Q6_V_hi_W(cur_w);
      HVX_Vector prev_lo = Q6_V_lo_W(prev_w);
      HVX_Vector prev_hi = Q6_V_hi_W(prev_w);
      HVX_Vector next_lo = Q6_V_lo_W(next_w);
      HVX_Vector next_hi = Q6_V_hi_W(next_w);

      /* 3 * cur = cur + cur + cur */
      HVX_Vector cur3_lo = Q6_Vh_vadd_VhVh(
        Q6_Vh_vadd_VhVh(cur_lo, cur_lo), cur_lo);
      HVX_Vector cur3_hi = Q6_Vh_vadd_VhVh(
        Q6_Vh_vadd_VhVh(cur_hi, cur_hi), cur_hi);

      /* left = (3*cur + prev + 1) >> 2 */
      HVX_Vector one = Q6_Vh_vsplat_R(1);
      HVX_Vector two = Q6_Vh_vsplat_R(2);
      HVX_Vector left_lo = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(cur3_lo, prev_lo), one), 2);
      HVX_Vector left_hi = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(cur3_hi, prev_hi), one), 2);

      /* right = (3*cur + next + 2) >> 2 */
      HVX_Vector right_lo = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(cur3_lo, next_lo), two), 2);
      HVX_Vector right_hi = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(cur3_hi, next_hi), two), 2);

      /* Pack back to u8 via clamp + vpacke (no interleave) */
      HVX_Vector v_left = hvx_pack_u8(left_lo, left_hi);
      HVX_Vector v_right = hvx_pack_u8(right_lo, right_hi);

      /* Interleave left/right pairs:
       * byte-interleave produces l0 r0 l1 r1 ...
       */
      HVX_VectorPair interleaved =
        Q6_W_vshuff_VVR(v_right, v_left, -1);
      vmemu(outptr + col * 2) =
        Q6_V_lo_W(interleaved);
      vmemu(outptr + col * 2 + 128) =
        Q6_V_hi_W(interleaved);
    }

    /* Scalar tail for remaining inner samples */
    for (; col < inner_end; col++) {
      int invalue = inptr[col] * 3;
      outptr[col * 2] =
        (JSAMPLE)((invalue + inptr[col - 1] + 1) >> 2);
      outptr[col * 2 + 1] =
        (JSAMPLE)((invalue + inptr[col + 1] + 2) >> 2);
    }

    /* Special case for last sample */
    col = downsampled_width - 1;
    outptr[col * 2] =
      (JSAMPLE)((inptr[col] * 3 + inptr[col - 1] + 1) >> 2);
    outptr[col * 2 + 1] = inptr[col];
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
      int thiscolsum = inptr0[0] * 3 + inptr1[0];
      int nextcolsum = inptr0[1] * 3 + inptr1[1];
      outptr[0] =
        (JSAMPLE)((thiscolsum * 4 + 8) >> 4);
      outptr[1] =
        (JSAMPLE)((thiscolsum * 3 + nextcolsum + 7) >> 4);

      /* HVX main loop: process 128 columns at a time.
       * For each column i (1..N-2), compute:
       *   colsum = near[i]*3 + far[i]
       * Then:
       *   left  = (colsum*3 + prev_colsum + 8) >> 4
       *   right = (colsum*3 + next_colsum + 7) >> 4
       *
       * Operate in s16 since colsum can be up to 4*255=1020.
       */
      JDIMENSION col;
      JDIMENSION inner_end = downsampled_width - 1;

      for (col = 1; col + 128 <= inner_end; col += 128) {
        /* Load near and far rows */
        HVX_Vector v_near = vmemu(inptr0 + col);
        HVX_Vector v_far  = vmemu(inptr1 + col);
        HVX_Vector v_near_prev = vmemu(inptr0 + col - 1);
        HVX_Vector v_far_prev  = vmemu(inptr1 + col - 1);
        HVX_Vector v_near_next = vmemu(inptr0 + col + 1);
        HVX_Vector v_far_next  = vmemu(inptr1 + col + 1);

        /* Unpack to s16 (low halves only for 64 elements) */
        HVX_VectorPair near_w = Q6_Wuh_vunpack_Vub(v_near);
        HVX_VectorPair far_w  = Q6_Wuh_vunpack_Vub(v_far);
        HVX_VectorPair np_w = Q6_Wuh_vunpack_Vub(v_near_prev);
        HVX_VectorPair fp_w = Q6_Wuh_vunpack_Vub(v_far_prev);
        HVX_VectorPair nn_w = Q6_Wuh_vunpack_Vub(v_near_next);
        HVX_VectorPair fn_w = Q6_Wuh_vunpack_Vub(v_far_next);

        /* Process both halves (lo=0..63, hi=64..127) */
        int half;
        for (half = 0; half < 2; half++) {
          HVX_Vector near_h, far_h, np_h, fp_h, nn_h, fn_h;
          if (half == 0) {
            near_h = Q6_V_lo_W(near_w);
            far_h  = Q6_V_lo_W(far_w);
            np_h   = Q6_V_lo_W(np_w);
            fp_h   = Q6_V_lo_W(fp_w);
            nn_h   = Q6_V_lo_W(nn_w);
            fn_h   = Q6_V_lo_W(fn_w);
          } else {
            near_h = Q6_V_hi_W(near_w);
            far_h  = Q6_V_hi_W(far_w);
            np_h   = Q6_V_hi_W(np_w);
            fp_h   = Q6_V_hi_W(fp_w);
            nn_h   = Q6_V_hi_W(nn_w);
            fn_h   = Q6_V_hi_W(fn_w);
          }

          /* colsum = near*3 + far */
          HVX_Vector near3 = Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(near_h, near_h), near_h);
          HVX_Vector cs = Q6_Vh_vadd_VhVh(near3, far_h);

          /* prev_colsum */
          HVX_Vector np3 = Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(np_h, np_h), np_h);
          HVX_Vector pcs = Q6_Vh_vadd_VhVh(np3, fp_h);

          /* next_colsum */
          HVX_Vector nn3 = Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(nn_h, nn_h), nn_h);
          HVX_Vector ncs = Q6_Vh_vadd_VhVh(nn3, fn_h);

          /* cs3 = colsum * 3 */
          HVX_Vector cs3 = Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(cs, cs), cs);

          /* left = (cs3 + prev_colsum + 8) >> 4 */
          HVX_Vector eight = Q6_Vh_vsplat_R(8);
          HVX_Vector seven = Q6_Vh_vsplat_R(7);
          HVX_Vector left_h = Q6_Vh_vasr_VhR(
            Q6_Vh_vadd_VhVh(
              Q6_Vh_vadd_VhVh(cs3, pcs), eight), 4);

          /* right = (cs3 + next_colsum + 7) >> 4 */
          HVX_Vector right_h = Q6_Vh_vasr_VhR(
            Q6_Vh_vadd_VhVh(
              Q6_Vh_vadd_VhVh(cs3, ncs), seven), 4);

          /* Pack to u8 — values are in [0..255] range.
           * Pack 64 elements into the low half of a vector
           * (high half is zero), then interleave left/right.
           */
          HVX_Vector zero = Q6_V_vzero();
          HVX_Vector v_left = hvx_pack_u8(left_h, zero);
          HVX_Vector v_right = hvx_pack_u8(right_h, zero);

          /* Interleave left/right pairs */
          HVX_VectorPair interleaved =
            Q6_W_vshuff_VVR(v_right, v_left, -1);

          /* Store 128 bytes (64 pixel pairs) */
          vmemu(outptr + (col + half * 64) * 2) =
            Q6_V_lo_W(interleaved);
        }
      }

      /* Scalar tail for remaining inner columns */
      int lastcolsum_s, thiscolsum_s, nextcolsum_s;
      if (col == 1) {
        thiscolsum_s = inptr0[0] * 3 + inptr1[0];
      } else if (col > 1) {
        thiscolsum_s =
          inptr0[col - 1] * 3 + inptr1[col - 1];
      } else {
        thiscolsum_s = 0;
      }
      for (; col < inner_end; col++) {
        lastcolsum_s = thiscolsum_s;
        thiscolsum_s = inptr0[col] * 3 + inptr1[col];
        nextcolsum_s =
          inptr0[col + 1] * 3 + inptr1[col + 1];
        outptr[col * 2] =
          (JSAMPLE)((thiscolsum_s * 3 +
                     lastcolsum_s + 8) >> 4);
        outptr[col * 2 + 1] =
          (JSAMPLE)((thiscolsum_s * 3 +
                     nextcolsum_s + 7) >> 4);
      }

      /* Special case for last column */
      col = downsampled_width - 1;
      {
        int lcs = inptr0[col - 1] * 3 + inptr1[col - 1];
        int tcs = inptr0[col] * 3 + inptr1[col];
        outptr[col * 2] =
          (JSAMPLE)((tcs * 3 + lcs + 8) >> 4);
        outptr[col * 2 + 1] =
          (JSAMPLE)((tcs * 4 + 7) >> 4);
      }

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

  inrow = 0;
  outrow = 0;
  while (outrow < max_v_samp_factor) {
    /* Prefetch next neighbor row into L2 */
    if (outrow + 2 < max_v_samp_factor)
      hvx_prefetch_row(input_data[inrow + 1],
                        downsampled_width);

    for (v = 0; v < 2; v++) {
      inptr0 = input_data[inrow];
      int bias;
      if (v == 0) {
        inptr1 = input_data[inrow - 1];
        bias = 1;
      } else {
        inptr1 = input_data[inrow + 1];
        bias = 2;
      }
      outptr = output_data_ptr[0][outrow++];

      JDIMENSION col;

      /* HVX main loop: 128 pixels per iteration.
       * out = (3 * near + far + bias) >> 2
       */
      HVX_Vector v_bias = Q6_Vh_vsplat_R(bias);

      for (col = 0; col + 128 <= downsampled_width;
           col += 128) {
        HVX_Vector v_near = vmemu(inptr0 + col);
        HVX_Vector v_far  = vmemu(inptr1 + col);

        HVX_VectorPair near_w =
          Q6_Wuh_vunpack_Vub(v_near);
        HVX_VectorPair far_w =
          Q6_Wuh_vunpack_Vub(v_far);

        /* Process low half */
        HVX_Vector n_lo = Q6_V_lo_W(near_w);
        HVX_Vector n3_lo = Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(n_lo, n_lo), n_lo);
        HVX_Vector out_lo = Q6_Vh_vasr_VhR(
          Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(n3_lo, Q6_V_lo_W(far_w)),
            v_bias), 2);

        /* Process high half */
        HVX_Vector n_hi = Q6_V_hi_W(near_w);
        HVX_Vector n3_hi = Q6_Vh_vadd_VhVh(
          Q6_Vh_vadd_VhVh(n_hi, n_hi), n_hi);
        HVX_Vector out_hi = Q6_Vh_vasr_VhR(
          Q6_Vh_vadd_VhVh(
            Q6_Vh_vadd_VhVh(n3_hi, Q6_V_hi_W(far_w)),
            v_bias), 2);

        /* Pack to u8 and store (clamp + vpacke, no interleave) */
        vmemu(outptr + col) = hvx_pack_u8(out_lo, out_hi);
      }

      /* Scalar tail */
      for (; col < downsampled_width; col++) {
        int thiscolsum = inptr0[col] * 3 + inptr1[col];
        outptr[col] =
          (JSAMPLE)((thiscolsum + bias) >> 2);
      }
    }
    inrow++;
  }
}
