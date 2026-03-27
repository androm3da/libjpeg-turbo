/*
 * Reduced-size inverse DCT (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"


/* jsimd_idct_4x4_hvx() and jsimd_idct_2x2_hvx() perform dequantization and
 * inverse DCT on one block of coefficients, producing reduced-size 4x4 or
 * 2x2 output respectively.  They use the same calculations and produce
 * exactly the same output as IJG's original jpeg_idct_4x4() and
 * jpeg_idct_2x2() functions, which can be found in jidctred.c.
 *
 * The implementation is based on the Loeffler, Ligtenberg and Moschytz (LL&M)
 * algorithm used in jidctint.c.  Each 8-to-8 1-D IDCT step is replaced with
 * an 8-to-4 step (for 4x4) or 8-to-2 step (for 2x2) that produces averages
 * of adjacent outputs.
 *
 * Scaled integer constants are used to avoid floating-point arithmetic:
 *    0.211164243 =  1730 * 2^-13
 *    0.509795579 =  4176 * 2^-13
 *    0.601344887 =  4926 * 2^-13
 *    0.720959822 =  5906 * 2^-13
 *    0.765366865 =  6270 * 2^-13
 *    0.850430095 =  6967 * 2^-13
 *    0.899976223 =  7373 * 2^-13
 *    1.061594337 =  8697 * 2^-13
 *    1.272758580 = 10426 * 2^-13
 *    1.451774981 = 11893 * 2^-13
 *    1.847759065 = 15137 * 2^-13
 *    2.172734803 = 17799 * 2^-13
 *    2.562915447 = 20995 * 2^-13
 *    3.624509785 = 29692 * 2^-13
 *
 * See jidctred.c for further details of the IDCT algorithm.
 */

#define CONST_BITS  13
#define PASS1_BITS  2

#undef DESCALE
#define DESCALE(x, n)  (((x) + (1 << ((n) - 1))) >> (n))
#define RANGE_LIMIT(x)  ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

#define FIX_0_211164243  1730     /* FIX(0.211164243) */
#define FIX_0_509795579  4176     /* FIX(0.509795579) */
#define FIX_0_601344887  4926     /* FIX(0.601344887) */
#define FIX_0_720959822  5906     /* FIX(0.720959822) */
#define FIX_0_765366865  6270     /* FIX(0.765366865) */
#define FIX_0_850430095  6967     /* FIX(0.850430095) */
#define FIX_0_899976223  7373     /* FIX(0.899976223) */
#define FIX_1_061594337  8697     /* FIX(1.061594337) */
#define FIX_1_272758580  10426    /* FIX(1.272758580) */
#define FIX_1_451774981  11893    /* FIX(1.451774981) */
#define FIX_1_847759065  15137    /* FIX(1.847759065) */
#define FIX_2_172734803  17799    /* FIX(2.172734803) */
#define FIX_2_562915447  20995    /* FIX(2.562915447) */
#define FIX_3_624509785  29692    /* FIX(3.624509785) */

/* Dequantize a coefficient by multiplying it by the multiplier-table
 * entry; produce an int result.  In this module, both inputs and result
 * are 16 bits or less, so either int or short multiply will work.
 */
#define DEQUANTIZE(coef, quantval)  (((ISLOW_MULT_TYPE)(coef)) * (quantval))

/* Multiply a variable by a constant and return the full product. */
#define MULTIPLY(var, const)  ((var) * (const))


/*
 * Perform dequantization and inverse DCT on one block of coefficients,
 * producing a reduced-size 4x4 output block.
 */

HIDDEN void
jsimd_idct_4x4_hvx(void *dct_table, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  ISLOW_MULT_TYPE *quantptr;
  JCOEFPTR inptr;
  int *wsptr;
  int ctr;
  int workspace[DCTSIZE * 4];   /* buffers data between passes */

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (ISLOW_MULT_TYPE *)dct_table;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; inptr++, quantptr++, wsptr++, ctr--) {
    /* Don't bother to process column 4, because second pass won't use it */
    if (ctr == DCTSIZE - 4)
      continue;
    if (inptr[DCTSIZE * 1] == 0 && inptr[DCTSIZE * 2] == 0 &&
        inptr[DCTSIZE * 3] == 0 && inptr[DCTSIZE * 5] == 0 &&
        inptr[DCTSIZE * 6] == 0 && inptr[DCTSIZE * 7] == 0) {
      /* AC terms all zero; we need not examine term 4 for 4x4 output */
      int dcval = DEQUANTIZE(inptr[DCTSIZE * 0],
                             quantptr[DCTSIZE * 0]) << PASS1_BITS;

      wsptr[DCTSIZE * 0] = dcval;
      wsptr[DCTSIZE * 1] = dcval;
      wsptr[DCTSIZE * 2] = dcval;
      wsptr[DCTSIZE * 3] = dcval;

      continue;
    }

    /* Even part */

    int tmp0 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
    tmp0 = tmp0 << (CONST_BITS + 1);

    int z2 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
    int z3 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);

    int tmp2 = MULTIPLY(z2, FIX_1_847759065) +
               MULTIPLY(z3, -FIX_0_765366865);

    int tmp10 = tmp0 + tmp2;
    int tmp12 = tmp0 - tmp2;

    /* Odd part */

    int z1 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);
    z2 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
    z3 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
    int z4 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);

    tmp0 = MULTIPLY(z1, -FIX_0_211164243) + /* sqrt(2) * ( c3-c1) */
           MULTIPLY(z2,  FIX_1_451774981) + /* sqrt(2) * ( c3+c7) */
           MULTIPLY(z3, -FIX_2_172734803) + /* sqrt(2) * (-c1-c5) */
           MULTIPLY(z4,  FIX_1_061594337);  /* sqrt(2) * ( c5+c7) */

    tmp2 = MULTIPLY(z1, -FIX_0_509795579) + /* sqrt(2) * (c7-c5) */
           MULTIPLY(z2, -FIX_0_601344887) + /* sqrt(2) * (c5-c1) */
           MULTIPLY(z3,  FIX_0_899976223) + /* sqrt(2) * (c3-c7) */
           MULTIPLY(z4,  FIX_2_562915447);  /* sqrt(2) * (c1+c3) */

    /* Final output stage */

    wsptr[DCTSIZE * 0] =
      (int)DESCALE(tmp10 + tmp2, CONST_BITS - PASS1_BITS + 1);
    wsptr[DCTSIZE * 3] =
      (int)DESCALE(tmp10 - tmp2, CONST_BITS - PASS1_BITS + 1);
    wsptr[DCTSIZE * 1] =
      (int)DESCALE(tmp12 + tmp0, CONST_BITS - PASS1_BITS + 1);
    wsptr[DCTSIZE * 2] =
      (int)DESCALE(tmp12 - tmp0, CONST_BITS - PASS1_BITS + 1);
  }

  /* Pass 2: process 4 rows from work array, store into output array. */

  wsptr = workspace;
  for (ctr = 0; ctr < 4; ctr++) {
    JSAMPROW outptr = output_buf[ctr] + output_col;

    /* It's not clear whether a zero row test is worthwhile here ... */

    if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 &&
        wsptr[5] == 0 && wsptr[6] == 0 && wsptr[7] == 0) {
      /* AC terms all zero */
      JSAMPLE dcval =
        (JSAMPLE)RANGE_LIMIT(DESCALE(wsptr[0], PASS1_BITS + 3) + 128);

      outptr[0] = dcval;
      outptr[1] = dcval;
      outptr[2] = dcval;
      outptr[3] = dcval;

      wsptr += DCTSIZE;           /* advance pointer to next row */
      continue;
    }

    /* Even part */

    int tmp0 = wsptr[0] << (CONST_BITS + 1);

    int tmp2 = MULTIPLY(wsptr[2],  FIX_1_847759065) +
               MULTIPLY(wsptr[6], -FIX_0_765366865);

    int tmp10 = tmp0 + tmp2;
    int tmp12 = tmp0 - tmp2;

    /* Odd part */

    int z1 = wsptr[7];
    int z2 = wsptr[5];
    int z3 = wsptr[3];
    int z4 = wsptr[1];

    tmp0 = MULTIPLY(z1, -FIX_0_211164243) + /* sqrt(2) * ( c3-c1) */
           MULTIPLY(z2,  FIX_1_451774981) + /* sqrt(2) * ( c3+c7) */
           MULTIPLY(z3, -FIX_2_172734803) + /* sqrt(2) * (-c1-c5) */
           MULTIPLY(z4,  FIX_1_061594337);  /* sqrt(2) * ( c5+c7) */

    tmp2 = MULTIPLY(z1, -FIX_0_509795579) + /* sqrt(2) * (c7-c5) */
           MULTIPLY(z2, -FIX_0_601344887) + /* sqrt(2) * (c5-c1) */
           MULTIPLY(z3,  FIX_0_899976223) + /* sqrt(2) * (c3-c7) */
           MULTIPLY(z4,  FIX_2_562915447);  /* sqrt(2) * (c1+c3) */

    /* Final output stage */

    outptr[0] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp10 + tmp2, CONST_BITS + PASS1_BITS + 3 + 1) + 128);
    outptr[3] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp10 - tmp2, CONST_BITS + PASS1_BITS + 3 + 1) + 128);
    outptr[1] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp12 + tmp0, CONST_BITS + PASS1_BITS + 3 + 1) + 128);
    outptr[2] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp12 - tmp0, CONST_BITS + PASS1_BITS + 3 + 1) + 128);

    wsptr += DCTSIZE;             /* advance pointer to next row */
  }
}


/*
 * Perform dequantization and inverse DCT on one block of coefficients,
 * producing a reduced-size 2x2 output block.
 */

HIDDEN void
jsimd_idct_2x2_hvx(void *dct_table, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  ISLOW_MULT_TYPE *quantptr;
  JCOEFPTR inptr;
  int *wsptr;
  int ctr;
  int workspace[DCTSIZE * 2];   /* buffers data between passes */

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (ISLOW_MULT_TYPE *)dct_table;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; inptr++, quantptr++, wsptr++, ctr--) {
    /* Don't bother to process columns 2,4,6 */
    if (ctr == DCTSIZE - 2 || ctr == DCTSIZE - 4 || ctr == DCTSIZE - 6)
      continue;
    if (inptr[DCTSIZE * 1] == 0 && inptr[DCTSIZE * 3] == 0 &&
        inptr[DCTSIZE * 5] == 0 && inptr[DCTSIZE * 7] == 0) {
      /* AC terms all zero; we need not examine terms 2,4,6 for 2x2 output */
      int dcval = DEQUANTIZE(inptr[DCTSIZE * 0],
                             quantptr[DCTSIZE * 0]) << PASS1_BITS;

      wsptr[DCTSIZE * 0] = dcval;
      wsptr[DCTSIZE * 1] = dcval;

      continue;
    }

    /* Even part */

    int z1 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
    int tmp10 = z1 << (CONST_BITS + 2);

    /* Odd part */

    int tmp0;
    z1 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);
    tmp0 = MULTIPLY(z1, -FIX_0_720959822);  /* sqrt(2) * ( c7-c5+c3-c1) */
    z1 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
    tmp0 += MULTIPLY(z1, FIX_0_850430095);  /* sqrt(2) * (-c1+c3+c5+c7) */
    z1 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
    tmp0 += MULTIPLY(z1, -FIX_1_272758580); /* sqrt(2) * (-c1+c3-c5-c7) */
    z1 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);
    tmp0 += MULTIPLY(z1, FIX_3_624509785);  /* sqrt(2) * ( c1+c3+c5+c7) */

    /* Final output stage */

    wsptr[DCTSIZE * 0] =
      (int)DESCALE(tmp10 + tmp0, CONST_BITS - PASS1_BITS + 2);
    wsptr[DCTSIZE * 1] =
      (int)DESCALE(tmp10 - tmp0, CONST_BITS - PASS1_BITS + 2);
  }

  /* Pass 2: process 2 rows from work array, store into output array. */

  wsptr = workspace;
  for (ctr = 0; ctr < 2; ctr++) {
    JSAMPROW outptr = output_buf[ctr] + output_col;

    /* It's not clear whether a zero row test is worthwhile here ... */

    if (wsptr[1] == 0 && wsptr[3] == 0 && wsptr[5] == 0 && wsptr[7] == 0) {
      /* AC terms all zero */
      JSAMPLE dcval =
        (JSAMPLE)RANGE_LIMIT(DESCALE(wsptr[0], PASS1_BITS + 3) + 128);

      outptr[0] = dcval;
      outptr[1] = dcval;

      wsptr += DCTSIZE;           /* advance pointer to next row */
      continue;
    }

    /* Even part */

    int tmp10 = wsptr[0] << (CONST_BITS + 2);

    /* Odd part */

    int tmp0 = MULTIPLY(wsptr[7], -FIX_0_720959822) +
               MULTIPLY(wsptr[5],  FIX_0_850430095) +
               MULTIPLY(wsptr[3], -FIX_1_272758580) +
               MULTIPLY(wsptr[1],  FIX_3_624509785);

    /* Final output stage */

    outptr[0] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp10 + tmp0, CONST_BITS + PASS1_BITS + 3 + 2) + 128);
    outptr[1] = (JSAMPLE)RANGE_LIMIT(
      DESCALE(tmp10 - tmp0, CONST_BITS + PASS1_BITS + 3 + 2) + 128);

    wsptr += DCTSIZE;             /* advance pointer to next row */
  }
}
