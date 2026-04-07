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

    /* Prefetch next Y, Cb, Cr input rows into L2 */
    if (num_rows > 0) {
      hvx_prefetch_row(input_buf[0][input_row],
                        out_width);
      hvx_prefetch_row(input_buf[1][input_row],
                        out_width);
      hvx_prefetch_row(input_buf[2][input_row],
                        out_width);
    }

    JDIMENSION col;
    /* HVX main loop: process 128 pixels per iteration */
    for (col = 0; col + 128 <= out_width; col += 128) {
      HVX_Vector v_y  = vmemu(inptr0 + col);
      HVX_Vector v_cb = vmemu(inptr1 + col);
      HVX_Vector v_cr = vmemu(inptr2 + col);

      HVX_Vector v_r, v_g, v_b;
      hvx_ycc_to_rgb(v_y, v_cb, v_cr, &v_r, &v_g, &v_b);

#if RGB_PIXELSIZE == 4
      /* 4-byte RGBX: byte-interleave R,G then B,A, then
       * halfword-interleave the pairs.
       * Result: 4 output vectors (512 bytes = 128 pixels).
       */
      HVX_Vector v_a = Q6_Vb_vsplat_R(0xFF);
      HVX_VectorPair rg =
        Q6_W_vshuff_VVR(v_g, v_r, -1);
      HVX_VectorPair ba =
        Q6_W_vshuff_VVR(v_a, v_b, -1);
      HVX_VectorPair out01 = Q6_W_vshuff_VVR(
        Q6_V_lo_W(ba), Q6_V_lo_W(rg), -2);
      HVX_VectorPair out23 = Q6_W_vshuff_VVR(
        Q6_V_hi_W(ba), Q6_V_hi_W(rg), -2);
      vmemu(outptr + col * 4)       = Q6_V_lo_W(out01);
      vmemu(outptr + col * 4 + 128) = Q6_V_hi_W(out01);
      vmemu(outptr + col * 4 + 256) = Q6_V_lo_W(out23);
      vmemu(outptr + col * 4 + 384) = Q6_V_hi_W(out23);
#else /* RGB_PIXELSIZE == 3 */
      /* 3-byte RGB: produce RGBX into a temp buffer,
       * then compact 3-of-4 bytes to output.
       */
      HVX_Vector v_x = Q6_V_vzero();
      HVX_VectorPair rg =
        Q6_W_vshuff_VVR(v_g, v_r, -1);
      HVX_VectorPair bx =
        Q6_W_vshuff_VVR(v_x, v_b, -1);
      HVX_VectorPair out01 = Q6_W_vshuff_VVR(
        Q6_V_lo_W(bx), Q6_V_lo_W(rg), -2);
      HVX_VectorPair out23 = Q6_W_vshuff_VVR(
        Q6_V_hi_W(bx), Q6_V_hi_W(rg), -2);

      JSAMPLE HVX_ALIGN tmp[512];
      vmem(tmp)       = Q6_V_lo_W(out01);
      vmem(tmp + 128) = Q6_V_hi_W(out01);
      vmem(tmp + 256) = Q6_V_lo_W(out23);
      vmem(tmp + 384) = Q6_V_hi_W(out23);

      /* Compact: copy 3 out of every 4 bytes.
       * 128 pixels * 3 bytes = 384 bytes output.
       */
      JSAMPLE *dst = outptr + col * 3;
      int i;
      for (i = 0; i < 128; i++) {
        dst[i * 3 + 0] = tmp[i * 4 + 0];
        dst[i * 3 + 1] = tmp[i * 4 + 1];
        dst[i * 3 + 2] = tmp[i * 4 + 2];
      }
#endif
    }

    /* Tail: scalar fallback for remaining pixels */
    for (; col < out_width; col++) {
      int y  = inptr0[col];
      int cb = inptr1[col] - 128;
      int cr = inptr2[col] - 128;

      int r = y + ((F_1_402 * cr + (1 << 13)) >> 14);
      int g = y - ((F_0_344 * cb +
                     F_0_714 * cr + (1 << 14)) >> 15);
      int b = y + ((F_1_772 * cb + (1 << 13)) >> 14);

      outptr[col * RGB_PIXELSIZE + RGB_RED] =
        (JSAMPLE)(r < 0 ? 0 : (r > 255 ? 255 : r));
      outptr[col * RGB_PIXELSIZE + RGB_GREEN] =
        (JSAMPLE)(g < 0 ? 0 : (g > 255 ? 255 : g));
      outptr[col * RGB_PIXELSIZE + RGB_BLUE] =
        (JSAMPLE)(b < 0 ? 0 : (b > 255 ? 255 : b));
#if RGB_PIXELSIZE == 4
      outptr[col * RGB_PIXELSIZE + RGB_ALPHA] = 0xFF;
#endif
    }
  }
}
