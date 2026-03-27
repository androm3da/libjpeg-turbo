/*
 * Fast integer forward DCT (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* jsimd_fdct_ifast_hvx() performs a fast, not so accurate forward DCT
 * (Discrete Cosine Transform) on one block of samples.  It uses the same
 * calculations and produces exactly the same output as IJG's original
 * jpeg_fdct_ifast() function, which can be found in jfdctfst.c.
 *
 * Scaled integer constants (CONST_BITS = 8) are used to avoid
 * floating-point arithmetic:
 *    0.382683433 = 98  * 2^-8
 *    0.541196100 = 139 * 2^-8
 *    0.707106781 = 181 * 2^-8
 *    1.306562965 = 334 * 2^-8
 *
 * See jfdctfst.c for further details of the DCT algorithm.  Where
 * possible, the variable names and comments here in
 * jsimd_fdct_ifast_hvx() match up with those in jpeg_fdct_ifast().
 */

#define CONST_BITS  8

#define FIX_0_382683433  ((int)98)
#define FIX_0_541196100  ((int)139)
#define FIX_0_707106781  ((int)181)
#define FIX_1_306562965  ((int)334)

#define MULTIPLY(var, c)  ((DCTELEM)(((int)(var) * (c)) >> CONST_BITS))


HIDDEN void
jsimd_fdct_ifast_hvx(DCTELEM *data)
{
  DCTELEM *dataptr;
  int ctr;

  /* Pass 1: process rows. */

  dataptr = data;
  for (ctr = DCTSIZE - 1; ctr >= 0; ctr--) {
    int tmp0 = dataptr[0] + dataptr[7];
    int tmp7 = dataptr[0] - dataptr[7];
    int tmp1 = dataptr[1] + dataptr[6];
    int tmp6 = dataptr[1] - dataptr[6];
    int tmp2 = dataptr[2] + dataptr[5];
    int tmp5 = dataptr[2] - dataptr[5];
    int tmp3 = dataptr[3] + dataptr[4];
    int tmp4 = dataptr[3] - dataptr[4];

    /* Even part */

    int tmp10 = tmp0 + tmp3;        /* phase 2 */
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    dataptr[0] = (DCTELEM)(tmp10 + tmp11); /* phase 3 */
    dataptr[4] = (DCTELEM)(tmp10 - tmp11);

    int z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    dataptr[2] = (DCTELEM)(tmp13 + z1);    /* phase 5 */
    dataptr[6] = (DCTELEM)(tmp13 - z1);

    /* Odd part */

    tmp10 = tmp4 + tmp5;            /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    int z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    int z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5;    /* c2-c6 */
    int z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5;    /* c2+c6 */
    int z3 = MULTIPLY(tmp11, FIX_0_707106781);          /* c4 */

    int z11 = tmp7 + z3;            /* phase 5 */
    int z13 = tmp7 - z3;

    dataptr[5] = (DCTELEM)(z13 + z2); /* phase 6 */
    dataptr[3] = (DCTELEM)(z13 - z2);
    dataptr[1] = (DCTELEM)(z11 + z4);
    dataptr[7] = (DCTELEM)(z11 - z4);

    dataptr += DCTSIZE;             /* advance pointer to next row */
  }

  /* Pass 2: process columns. */

  dataptr = data;
  for (ctr = DCTSIZE - 1; ctr >= 0; ctr--) {
    int tmp0 = dataptr[DCTSIZE * 0] + dataptr[DCTSIZE * 7];
    int tmp7 = dataptr[DCTSIZE * 0] - dataptr[DCTSIZE * 7];
    int tmp1 = dataptr[DCTSIZE * 1] + dataptr[DCTSIZE * 6];
    int tmp6 = dataptr[DCTSIZE * 1] - dataptr[DCTSIZE * 6];
    int tmp2 = dataptr[DCTSIZE * 2] + dataptr[DCTSIZE * 5];
    int tmp5 = dataptr[DCTSIZE * 2] - dataptr[DCTSIZE * 5];
    int tmp3 = dataptr[DCTSIZE * 3] + dataptr[DCTSIZE * 4];
    int tmp4 = dataptr[DCTSIZE * 3] - dataptr[DCTSIZE * 4];

    /* Even part */

    int tmp10 = tmp0 + tmp3;        /* phase 2 */
    int tmp13 = tmp0 - tmp3;
    int tmp11 = tmp1 + tmp2;
    int tmp12 = tmp1 - tmp2;

    dataptr[DCTSIZE * 0] = (DCTELEM)(tmp10 + tmp11); /* phase 3 */
    dataptr[DCTSIZE * 4] = (DCTELEM)(tmp10 - tmp11);

    int z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    dataptr[DCTSIZE * 2] = (DCTELEM)(tmp13 + z1); /* phase 5 */
    dataptr[DCTSIZE * 6] = (DCTELEM)(tmp13 - z1);

    /* Odd part */

    tmp10 = tmp4 + tmp5;            /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    int z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    int z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5;    /* c2-c6 */
    int z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5;    /* c2+c6 */
    int z3 = MULTIPLY(tmp11, FIX_0_707106781);          /* c4 */

    int z11 = tmp7 + z3;            /* phase 5 */
    int z13 = tmp7 - z3;

    dataptr[DCTSIZE * 5] = (DCTELEM)(z13 + z2); /* phase 6 */
    dataptr[DCTSIZE * 3] = (DCTELEM)(z13 - z2);
    dataptr[DCTSIZE * 1] = (DCTELEM)(z11 + z4);
    dataptr[DCTSIZE * 7] = (DCTELEM)(z11 - z4);

    dataptr++;                      /* advance pointer to next column */
  }
}
