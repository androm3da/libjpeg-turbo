/*
 * Accurate integer inverse DCT (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* jsimd_idct_islow_hvx() performs a slower but more accurate inverse DCT
 * (Discrete Cosine Transform) on one block of coefficients.  It uses the same
 * calculations and produces exactly the same output as IJG's original
 * jpeg_idct_islow() function, which can be found in jidctint.c.
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.298631336 =  2446 * 2^-13
 *    0.390180644 =  3196 * 2^-13
 *    0.541196100 =  4433 * 2^-13
 *    0.765366865 =  6270 * 2^-13
 *    0.899976223 =  7373 * 2^-13
 *    1.175875602 =  9633 * 2^-13
 *    1.501321110 = 12299 * 2^-13
 *    1.847759065 = 15137 * 2^-13
 *    1.961570560 = 16069 * 2^-13
 *    2.053119869 = 16819 * 2^-13
 *    2.562915447 = 20995 * 2^-13
 *    3.072711026 = 25172 * 2^-13
 *
 * See jidctint.c for further details of the IDCT algorithm.  Where possible,
 * the variable names and comments here in jsimd_idct_islow_hvx() match up
 * with those in jpeg_idct_islow().
 */

#define CONST_BITS  13
#define PASS1_BITS  2

#define F_0_298  2446
#define F_0_390  3196
#define F_0_541  4433
#define F_0_765  6270
#define F_0_899  7373
#define F_1_175  9633
#define F_1_501  12299
#define F_1_847  15137
#define F_1_961  16069
#define F_2_053  16819
#define F_2_562  20995
#define F_3_072  25172

#undef DESCALE
#define DESCALE(x, n)  (((x) + (1 << ((n) - 1))) >> (n))
#define RANGE_LIMIT(x)  ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))


HIDDEN void
jsimd_idct_islow_hvx(void *dct_table, JCOEFPTR coef_block,
                           JSAMPARRAY output_buf, JDIMENSION output_col)
{
  ISLOW_MULT_TYPE *quantptr = (ISLOW_MULT_TYPE *)dct_table;
  int workspace[DCTSIZE2];
  int *wsptr;
  JSAMPROW outptr;
  int ctr;

  /* Pass 1: process columns from input, store into work array.
   * Results are scaled up by sqrt(8) compared to a true IDCT;
   * furthermore, we scale the results by 2^PASS1_BITS.
   */

  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any column in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     */
    if (coef_block[DCTSIZE * 1 + ctr] == 0 &&
        coef_block[DCTSIZE * 2 + ctr] == 0 &&
        coef_block[DCTSIZE * 3 + ctr] == 0 &&
        coef_block[DCTSIZE * 4 + ctr] == 0 &&
        coef_block[DCTSIZE * 5 + ctr] == 0 &&
        coef_block[DCTSIZE * 6 + ctr] == 0 &&
        coef_block[DCTSIZE * 7 + ctr] == 0) {
      /* AC terms all zero */
      int dcval = ((int)coef_block[DCTSIZE * 0 + ctr] *
                   quantptr[DCTSIZE * 0 + ctr]) << PASS1_BITS;

      workspace[DCTSIZE * 0 + ctr] = dcval;
      workspace[DCTSIZE * 1 + ctr] = dcval;
      workspace[DCTSIZE * 2 + ctr] = dcval;
      workspace[DCTSIZE * 3 + ctr] = dcval;
      workspace[DCTSIZE * 4 + ctr] = dcval;
      workspace[DCTSIZE * 5 + ctr] = dcval;
      workspace[DCTSIZE * 6 + ctr] = dcval;
      workspace[DCTSIZE * 7 + ctr] = dcval;
      continue;
    }

    /* Even part: reverse the even part of the forward DCT.
     * The rotator is sqrt(2)*c(-6).
     */

    int z2 = (int)coef_block[DCTSIZE * 2 + ctr] *
             quantptr[DCTSIZE * 2 + ctr];
    int z3 = (int)coef_block[DCTSIZE * 6 + ctr] *
             quantptr[DCTSIZE * 6 + ctr];

    int z1 = (z2 + z3) * F_0_541;
    int tmp2 = z1 + z3 * (-F_1_847);
    int tmp3 = z1 + z2 * F_0_765;

    z2 = (int)coef_block[DCTSIZE * 0 + ctr] * quantptr[DCTSIZE * 0 + ctr];
    z3 = (int)coef_block[DCTSIZE * 4 + ctr] * quantptr[DCTSIZE * 4 + ctr];

    int tmp0 = (z2 + z3) << CONST_BITS;
    int tmp1 = (z2 - z3) << CONST_BITS;

    int tmp10 = tmp0 + tmp3;
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

    tmp0 = (int)coef_block[DCTSIZE * 7 + ctr] * quantptr[DCTSIZE * 7 + ctr];
    tmp1 = (int)coef_block[DCTSIZE * 5 + ctr] * quantptr[DCTSIZE * 5 + ctr];
    tmp2 = (int)coef_block[DCTSIZE * 3 + ctr] * quantptr[DCTSIZE * 3 + ctr];
    tmp3 = (int)coef_block[DCTSIZE * 1 + ctr] * quantptr[DCTSIZE * 1 + ctr];

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    int z4 = tmp1 + tmp3;
    int z5 = (z3 + z4) * F_1_175;        /* sqrt(2) * c3 */

    tmp0 = tmp0 * F_0_298;               /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 = tmp1 * F_2_053;               /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 = tmp2 * F_3_072;               /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 = tmp3 * F_1_501;               /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = z1 * (-F_0_899);                /* sqrt(2) * ( c7-c3) */
    z2 = z2 * (-F_2_562);                /* sqrt(2) * (-c1-c3) */
    z3 = z3 * (-F_1_961);                /* sqrt(2) * (-c3-c5) */
    z4 = z4 * (-F_0_390);                /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    tmp0 += z1 + z3;
    tmp1 += z2 + z4;
    tmp2 += z2 + z3;
    tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    workspace[DCTSIZE * 0 + ctr] = DESCALE(tmp10 + tmp3,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 7 + ctr] = DESCALE(tmp10 - tmp3,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 1 + ctr] = DESCALE(tmp11 + tmp2,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 6 + ctr] = DESCALE(tmp11 - tmp2,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 2 + ctr] = DESCALE(tmp12 + tmp1,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 5 + ctr] = DESCALE(tmp12 - tmp1,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 3 + ctr] = DESCALE(tmp13 + tmp0,
                                            CONST_BITS - PASS1_BITS);
    workspace[DCTSIZE * 4 + ctr] = DESCALE(tmp13 - tmp0,
                                            CONST_BITS - PASS1_BITS);
  }

  /* Pass 2: process rows from work array, store into output array.
   * Note that we must descale the results by a factor of 8 == 2^3,
   * and also undo the PASS1_BITS scaling.
   */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    outptr = output_buf[ctr] + output_col;

    /* Rows of zeroes can be exploited in the same way as we did with
     * columns.  However, the column calculation has created many nonzero AC
     * terms, so the simplification applies less often (typically 5% to 10%
     * of the time).  On machines with very fast multiplication, it's
     * possible that the test takes more time than it's worth.  In that case
     * this section may be commented out.
     */
    if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 && wsptr[4] == 0 &&
        wsptr[5] == 0 && wsptr[6] == 0 && wsptr[7] == 0) {
      /* AC terms all zero */
      JSAMPLE dcval = (JSAMPLE)RANGE_LIMIT(DESCALE(wsptr[0],
                                                    PASS1_BITS + 3) + 128);

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

    /* Even part: reverse the even part of the forward DCT.
     * The rotator is sqrt(2)*c(-6).
     */

    int z2 = wsptr[2];
    int z3 = wsptr[6];

    int z1 = (z2 + z3) * F_0_541;
    int tmp2 = z1 + z3 * (-F_1_847);
    int tmp3 = z1 + z2 * F_0_765;

    int tmp0 = (wsptr[0] + wsptr[4]) << CONST_BITS;
    int tmp1 = (wsptr[0] - wsptr[4]) << CONST_BITS;

    int tmp10 = tmp0 + tmp3;
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

    tmp0 = wsptr[7];
    tmp1 = wsptr[5];
    tmp2 = wsptr[3];
    tmp3 = wsptr[1];

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    int z4 = tmp1 + tmp3;
    int z5 = (z3 + z4) * F_1_175;        /* sqrt(2) * c3 */

    tmp0 = tmp0 * F_0_298;               /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 = tmp1 * F_2_053;               /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 = tmp2 * F_3_072;               /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 = tmp3 * F_1_501;               /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = z1 * (-F_0_899);                /* sqrt(2) * ( c7-c3) */
    z2 = z2 * (-F_2_562);                /* sqrt(2) * (-c1-c3) */
    z3 = z3 * (-F_1_961);                /* sqrt(2) * (-c3-c5) */
    z4 = z4 * (-F_0_390);                /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    tmp0 += z1 + z3;
    tmp1 += z2 + z4;
    tmp2 += z2 + z3;
    tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    outptr[0] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp10 + tmp3,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[7] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp10 - tmp3,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[1] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp11 + tmp2,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[6] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp11 - tmp2,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[2] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp12 + tmp1,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[5] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp12 - tmp1,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[3] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp13 + tmp0,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);
    outptr[4] = (JSAMPLE)RANGE_LIMIT(DESCALE(tmp13 - tmp0,
                                              CONST_BITS + PASS1_BITS + 3) +
                                     128);

    wsptr += DCTSIZE;                     /* advance pointer to next row */
  }
}
