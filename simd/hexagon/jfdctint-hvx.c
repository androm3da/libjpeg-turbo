/*
 * Accurate integer forward DCT (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* jsimd_fdct_islow_hvx() performs a slower but more accurate forward DCT
 * (Discrete Cosine Transform) on one block of samples.  It uses the same
 * calculations and produces exactly the same output as IJG's original
 * jpeg_fdct_islow() function, which can be found in jfdctint.c.
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
 * See jfdctint.c for further details of the DCT algorithm.  Where possible,
 * the variable names and comments here in jsimd_fdct_islow_hvx() match up
 * with those in jpeg_fdct_islow().
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


HIDDEN void
jsimd_fdct_islow_hvx(DCTELEM *data)
{
  DCTELEM *dataptr;
  int ctr;

  /* Pass 1: process rows.
   * Results are scaled up by sqrt(8) compared to a true DCT;
   * furthermore, we scale the results by 2^PASS1_BITS.
   */

  dataptr = data;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    int tmp0 = dataptr[0] + dataptr[7];
    int tmp7 = dataptr[0] - dataptr[7];
    int tmp1 = dataptr[1] + dataptr[6];
    int tmp6 = dataptr[1] - dataptr[6];
    int tmp2 = dataptr[2] + dataptr[5];
    int tmp5 = dataptr[2] - dataptr[5];
    int tmp3 = dataptr[3] + dataptr[4];
    int tmp4 = dataptr[3] - dataptr[4];

    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
     */

    int tmp10 = tmp0 + tmp3;
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    dataptr[0] = (DCTELEM)((tmp10 + tmp11) << PASS1_BITS);
    dataptr[4] = (DCTELEM)((tmp10 - tmp11) << PASS1_BITS);

    int z1 = (tmp12 + tmp13) * F_0_541;
    dataptr[2] = (DCTELEM)DESCALE(z1 + tmp13 * F_0_765,
                                  CONST_BITS - PASS1_BITS);
    dataptr[6] = (DCTELEM)DESCALE(z1 - tmp12 * F_1_847,
                                  CONST_BITS - PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * cK represents cos(K*pi/16).
     * i0..i3 in the paper are tmp4..tmp7 here.
     */

    int z1o = tmp4 + tmp7;
    int z2 = tmp5 + tmp6;
    int z3 = tmp4 + tmp6;
    int z4 = tmp5 + tmp7;
    int z5 = (z3 + z4) * F_1_175;   /* sqrt(2) * c3 */

    tmp4 = tmp4 * F_0_298;          /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 = tmp5 * F_2_053;          /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 = tmp6 * F_3_072;          /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 = tmp7 * F_1_501;          /* sqrt(2) * ( c1+c3-c5-c7) */
    z1o = z1o * (-F_0_899);         /* sqrt(2) * ( c7-c3) */
    z2 = z2 * (-F_2_562);           /* sqrt(2) * (-c1-c3) */
    z3 = z3 * (-F_1_961);           /* sqrt(2) * (-c3-c5) */
    z4 = z4 * (-F_0_390);           /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[7] = (DCTELEM)DESCALE(tmp4 + z1o + z3, CONST_BITS - PASS1_BITS);
    dataptr[5] = (DCTELEM)DESCALE(tmp5 + z2 + z4, CONST_BITS - PASS1_BITS);
    dataptr[3] = (DCTELEM)DESCALE(tmp6 + z2 + z3, CONST_BITS - PASS1_BITS);
    dataptr[1] = (DCTELEM)DESCALE(tmp7 + z1o + z4, CONST_BITS - PASS1_BITS);

    dataptr += DCTSIZE;             /* advance pointer to next row */
  }

  /* Pass 2: process columns.
   * We remove the PASS1_BITS scaling, but leave the results scaled up
   * by an overall factor of 8.
   */

  dataptr = data;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    int tmp0 = dataptr[DCTSIZE * 0] + dataptr[DCTSIZE * 7];
    int tmp7 = dataptr[DCTSIZE * 0] - dataptr[DCTSIZE * 7];
    int tmp1 = dataptr[DCTSIZE * 1] + dataptr[DCTSIZE * 6];
    int tmp6 = dataptr[DCTSIZE * 1] - dataptr[DCTSIZE * 6];
    int tmp2 = dataptr[DCTSIZE * 2] + dataptr[DCTSIZE * 5];
    int tmp5 = dataptr[DCTSIZE * 2] - dataptr[DCTSIZE * 5];
    int tmp3 = dataptr[DCTSIZE * 3] + dataptr[DCTSIZE * 4];
    int tmp4 = dataptr[DCTSIZE * 3] - dataptr[DCTSIZE * 4];

    /* Even part */

    int tmp10 = tmp0 + tmp3;
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    dataptr[DCTSIZE * 0] = (DCTELEM)DESCALE(tmp10 + tmp11, PASS1_BITS);
    dataptr[DCTSIZE * 4] = (DCTELEM)DESCALE(tmp10 - tmp11, PASS1_BITS);

    int z1 = (tmp12 + tmp13) * F_0_541;
    dataptr[DCTSIZE * 2] = (DCTELEM)DESCALE(z1 + tmp13 * F_0_765,
                                             CONST_BITS + PASS1_BITS);
    dataptr[DCTSIZE * 6] = (DCTELEM)DESCALE(z1 - tmp12 * F_1_847,
                                             CONST_BITS + PASS1_BITS);

    /* Odd part */

    int z1o = tmp4 + tmp7;
    int z2 = tmp5 + tmp6;
    int z3 = tmp4 + tmp6;
    int z4 = tmp5 + tmp7;
    int z5 = (z3 + z4) * F_1_175;   /* sqrt(2) * c3 */

    tmp4 = tmp4 * F_0_298;          /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 = tmp5 * F_2_053;          /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 = tmp6 * F_3_072;          /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 = tmp7 * F_1_501;          /* sqrt(2) * ( c1+c3-c5-c7) */
    z1o = z1o * (-F_0_899);         /* sqrt(2) * ( c7-c3) */
    z2 = z2 * (-F_2_562);           /* sqrt(2) * (-c1-c3) */
    z3 = z3 * (-F_1_961);           /* sqrt(2) * (-c3-c5) */
    z4 = z4 * (-F_0_390);           /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[DCTSIZE * 7] = (DCTELEM)DESCALE(tmp4 + z1o + z3,
                                             CONST_BITS + PASS1_BITS);
    dataptr[DCTSIZE * 5] = (DCTELEM)DESCALE(tmp5 + z2 + z4,
                                             CONST_BITS + PASS1_BITS);
    dataptr[DCTSIZE * 3] = (DCTELEM)DESCALE(tmp6 + z2 + z3,
                                             CONST_BITS + PASS1_BITS);
    dataptr[DCTSIZE * 1] = (DCTELEM)DESCALE(tmp7 + z1o + z4,
                                             CONST_BITS + PASS1_BITS);

    dataptr++;                      /* advance pointer to next column */
  }
}
