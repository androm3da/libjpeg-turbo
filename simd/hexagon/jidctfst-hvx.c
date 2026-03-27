/*
 * Fast integer inverse DCT (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* jsimd_idct_ifast_hvx() performs a fast, not so accurate inverse DCT
 * (Discrete Cosine Transform) on one block of coefficients.  It uses
 * the same calculations and produces exactly the same output as IJG's
 * original jpeg_idct_ifast() function, which can be found in jidctfst.c.
 *
 * Scaled integer constants (CONST_BITS = 8):
 *    1.082392200 = 277 * 2^-8
 *    1.414213562 = 362 * 2^-8
 *    1.847759065 = 473 * 2^-8
 *    2.613125930 = 669 * 2^-8
 *
 * See jidctfst.c for further details of the IDCT algorithm.  Where
 * possible, the variable names and comments here in
 * jsimd_idct_ifast_hvx() match up with those in jpeg_idct_ifast().
 */

#define CONST_BITS  8
#define PASS1_BITS  2

#define FIX_1_082392200  ((int)277)
#define FIX_1_414213562  ((int)362)
#define FIX_1_847759065  ((int)473)
#define FIX_2_613125930  ((int)669)

#define MULTIPLY(var, c) \
  ((DCTELEM)(((int)(var) * (c)) >> CONST_BITS))

#define RANGE_LIMIT(x) \
  ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

#define IDESCALE(x, n)  ((int)((x) >> (n)))


HIDDEN void
jsimd_idct_ifast_hvx(void *dct_table, JCOEFPTR coef_block,
                           JSAMPARRAY output_buf, JDIMENSION output_col)
{
  IFAST_MULT_TYPE *quantptr;
  DCTELEM workspace[DCTSIZE2];
  DCTELEM *wsptr;
  DCTELEM tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  DCTELEM tmp10, tmp11, tmp12, tmp13;
  DCTELEM z5, z10, z11, z12, z13;
  int ctr;

  /* Pass 1: process columns from input, store into work array. */

  quantptr = (IFAST_MULT_TYPE *)dct_table;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; ctr--) {
    /* Due to quantization, we will usually find that many of the
     * input coefficients are zero, especially the AC terms.  We
     * can exploit this by short-circuiting the IDCT calculation
     * for any column in which all the AC terms are zero.  In that
     * case each output is equal to the DC coefficient (with scale
     * factor as needed).
     */

    if (coef_block[DCTSIZE * 1] == 0 &&
        coef_block[DCTSIZE * 2] == 0 &&
        coef_block[DCTSIZE * 3] == 0 &&
        coef_block[DCTSIZE * 4] == 0 &&
        coef_block[DCTSIZE * 5] == 0 &&
        coef_block[DCTSIZE * 6] == 0 &&
        coef_block[DCTSIZE * 7] == 0) {
      /* AC terms all zero */
      DCTELEM dcval =
        (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 0] *
                  quantptr[DCTSIZE * 0]);

      wsptr[DCTSIZE * 0] = dcval;
      wsptr[DCTSIZE * 1] = dcval;
      wsptr[DCTSIZE * 2] = dcval;
      wsptr[DCTSIZE * 3] = dcval;
      wsptr[DCTSIZE * 4] = dcval;
      wsptr[DCTSIZE * 5] = dcval;
      wsptr[DCTSIZE * 6] = dcval;
      wsptr[DCTSIZE * 7] = dcval;

      coef_block++;
      quantptr++;
      wsptr++;
      continue;
    }

    /* Even part */

    tmp0 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 0] *
                     quantptr[DCTSIZE * 0]);
    tmp1 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 2] *
                     quantptr[DCTSIZE * 2]);
    tmp2 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 4] *
                     quantptr[DCTSIZE * 4]);
    tmp3 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 6] *
                     quantptr[DCTSIZE * 6]);

    tmp10 = tmp0 + tmp2;            /* phase 3 */
    tmp11 = tmp0 - tmp2;

    tmp13 = tmp1 + tmp3;            /* phases 5-3 */
    tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13;

    tmp0 = tmp10 + tmp13;           /* phase 2 */
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    /* Odd part */

    tmp4 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 1] *
                     quantptr[DCTSIZE * 1]);
    tmp5 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 3] *
                     quantptr[DCTSIZE * 3]);
    tmp6 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 5] *
                     quantptr[DCTSIZE * 5]);
    tmp7 = (DCTELEM)((IFAST_MULT_TYPE)coef_block[DCTSIZE * 7] *
                     quantptr[DCTSIZE * 7]);

    z13 = tmp6 + tmp5;              /* phase 6 */
    z10 = tmp6 - tmp5;
    z11 = tmp4 + tmp7;
    z12 = tmp4 - tmp7;

    tmp7 = z11 + z13;               /* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562);

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065);
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5;
    tmp12 = MULTIPLY(z10, -FIX_2_613125930) + z5;

    tmp6 = tmp12 - tmp7;            /* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    wsptr[DCTSIZE * 0] = (DCTELEM)(tmp0 + tmp7);
    wsptr[DCTSIZE * 7] = (DCTELEM)(tmp0 - tmp7);
    wsptr[DCTSIZE * 1] = (DCTELEM)(tmp1 + tmp6);
    wsptr[DCTSIZE * 6] = (DCTELEM)(tmp1 - tmp6);
    wsptr[DCTSIZE * 2] = (DCTELEM)(tmp2 + tmp5);
    wsptr[DCTSIZE * 5] = (DCTELEM)(tmp2 - tmp5);
    wsptr[DCTSIZE * 4] = (DCTELEM)(tmp3 + tmp4);
    wsptr[DCTSIZE * 3] = (DCTELEM)(tmp3 - tmp4);

    coef_block++;
    quantptr++;
    wsptr++;
  }

  /* Pass 2: process rows from work array, store into output array.
   * Note that we must descale the results by a factor of 8 == 2**3,
   * and also undo the PASS1_BITS scaling.
   */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    JSAMPROW outptr = output_buf[ctr] + output_col;

    /* Rows of zeroes can be exploited in the same way as we did
     * with columns.  However, the column calculation has created
     * many nonzero AC terms, so the simplification applies less
     * often (typically 5% to 10% of the time).
     */

    if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 &&
        wsptr[4] == 0 && wsptr[5] == 0 && wsptr[6] == 0 &&
        wsptr[7] == 0) {
      /* AC terms all zero */
      JSAMPLE dcval = (JSAMPLE)RANGE_LIMIT(
        IDESCALE((int)wsptr[0], PASS1_BITS + 3) + 128);

      outptr[0] = dcval;
      outptr[1] = dcval;
      outptr[2] = dcval;
      outptr[3] = dcval;
      outptr[4] = dcval;
      outptr[5] = dcval;
      outptr[6] = dcval;
      outptr[7] = dcval;

      wsptr += DCTSIZE;
      continue;
    }

    /* Even part */

    tmp10 = wsptr[0] + wsptr[4];
    tmp11 = wsptr[0] - wsptr[4];

    tmp13 = wsptr[2] + wsptr[6];
    tmp12 = MULTIPLY(wsptr[2] - wsptr[6],
                     FIX_1_414213562) - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    /* Odd part */

    z13 = wsptr[5] + wsptr[3];
    z10 = wsptr[5] - wsptr[3];
    z11 = wsptr[1] + wsptr[7];
    z12 = wsptr[1] - wsptr[7];

    tmp7 = z11 + z13;               /* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562);

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065);
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5;
    tmp12 = MULTIPLY(z10, -FIX_2_613125930) + z5;

    tmp6 = tmp12 - tmp7;            /* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    /* Final output stage: scale down and range-limit */

    outptr[0] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp0 + tmp7), PASS1_BITS + 3) + 128);
    outptr[7] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp0 - tmp7), PASS1_BITS + 3) + 128);
    outptr[1] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp1 + tmp6), PASS1_BITS + 3) + 128);
    outptr[6] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp1 - tmp6), PASS1_BITS + 3) + 128);
    outptr[2] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp2 + tmp5), PASS1_BITS + 3) + 128);
    outptr[5] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp2 - tmp5), PASS1_BITS + 3) + 128);
    outptr[4] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp3 + tmp4), PASS1_BITS + 3) + 128);
    outptr[3] = (JSAMPLE)RANGE_LIMIT(
      IDESCALE((int)(tmp3 - tmp4), PASS1_BITS + 3) + 128);

    wsptr += DCTSIZE;
  }
}
