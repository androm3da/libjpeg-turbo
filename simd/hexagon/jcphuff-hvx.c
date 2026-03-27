/*
 * Prepare data for progressive Huffman encoding (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"

#include <limits.h>


/* Data preparation for encode_mcu_AC_first().
 *
 * The equivalent scalar C function (encode_mcu_AC_first_prepare()) can be
 * found in jcphuff.c.
 */

HIDDEN void
jsimd_encode_mcu_AC_first_prepare_hvx
  (const JCOEF *block, const int *jpeg_natural_order_start, int Sl, int Al,
   UJCOEF *values, size_t *zerobits)
{
  UJCOEF *values_ptr = values;
  UJCOEF *diff_values_ptr = values + DCTSIZE2;

  int k, temp, temp2;
  size_t bitmap = 0U;
  int Sl0 = Sl;

  /* On 32-bit Hexagon, size_t is 4 bytes (32 bits).  Process up to 32
   * coefficients per word of the zerobits bitmap.
   */
  if (Sl0 > 32)
    Sl0 = 32;

  for (k = 0; k < Sl0; k++) {
    temp = block[jpeg_natural_order_start[k]];
    if (temp == 0)
      continue;
    /* We must apply the point transform by Al.  For AC coefficients this
     * is an integer division with rounding towards 0.  To do this portably
     * in C, we shift after obtaining the absolute value; so the code is
     * interwoven with finding the abs value (temp) and output bits (temp2).
     */
    temp2 = temp >> (CHAR_BIT * sizeof(int) - 1);
    temp ^= temp2;
    temp -= temp2;              /* temp is abs value of input */
    temp >>= Al;                /* apply the point transform */
    /* Watch out for case that nonzero coef is zero after point transform */
    if (temp == 0)
      continue;
    /* For a negative coef, want temp2 = bitwise complement of abs(coef) */
    temp2 ^= temp;
    values_ptr[k] = (UJCOEF)temp;
    diff_values_ptr[k] = (UJCOEF)temp2;
    bitmap |= ((size_t)1U) << k;
  }

  zerobits[0] = bitmap;

  bitmap = 0U;

  if (Sl > 32) {
    int remaining = Sl - 32;
    const int *natural_order_ptr = jpeg_natural_order_start + 32;
    UJCOEF *val_ptr = values_ptr + 32;
    UJCOEF *diff_ptr = diff_values_ptr + 32;

    for (k = 0; k < remaining; k++) {
      temp = block[natural_order_ptr[k]];
      if (temp == 0)
        continue;
      temp2 = temp >> (CHAR_BIT * sizeof(int) - 1);
      temp ^= temp2;
      temp -= temp2;
      temp >>= Al;
      if (temp == 0)
        continue;
      temp2 ^= temp;
      val_ptr[k] = (UJCOEF)temp;
      diff_ptr[k] = (UJCOEF)temp2;
      bitmap |= ((size_t)1U) << k;
    }
  }

  zerobits[1] = bitmap;
}


/* Data preparation for encode_mcu_AC_refine().
 *
 * The equivalent scalar C function (encode_mcu_AC_refine_prepare()) can be
 * found in jcphuff.c.
 */

HIDDEN int
jsimd_encode_mcu_AC_refine_prepare_hvx
  (const JCOEF *block, const int *jpeg_natural_order_start, int Sl, int Al,
   UJCOEF *absvalues, size_t *bits)
{
  int k, temp, temp2;
  int EOB = 0;
  size_t zerobits = 0U, signbits = 0U;
  int Sl0 = Sl;

  /* On 32-bit Hexagon, size_t is 4 bytes (32 bits).  Process up to 32
   * coefficients per word of the bitmaps.
   */
  if (Sl0 > 32)
    Sl0 = 32;

  /* It is convenient to make a pre-pass to determine the transformed
   * coefficients' absolute values and the EOB position.
   */
  for (k = 0; k < Sl0; k++) {
    temp = block[jpeg_natural_order_start[k]];
    /* We must apply the point transform by Al.  For AC coefficients this
     * is an integer division with rounding towards 0.  To do this portably
     * in C, we shift after obtaining the absolute value.
     */
    temp2 = temp >> (CHAR_BIT * sizeof(int) - 1);
    temp ^= temp2;
    temp -= temp2;              /* temp is abs value of input */
    temp >>= Al;                /* apply the point transform */
    if (temp != 0) {
      zerobits |= ((size_t)1U) << k;
      signbits |= ((size_t)(temp2 + 1)) << k;
    }
    absvalues[k] = (UJCOEF)temp; /* save abs value for main pass */
    if (temp == 1)
      EOB = k;                  /* EOB = index of last newly-nonzero coef */
  }

  bits[0] = zerobits;
  bits[2] = signbits;

  zerobits = 0U;
  signbits = 0U;

  if (Sl > 32) {
    int remaining = Sl - 32;
    const int *natural_order_ptr = jpeg_natural_order_start + 32;
    UJCOEF *abs_ptr = absvalues + 32;

    for (k = 0; k < remaining; k++) {
      temp = block[natural_order_ptr[k]];
      temp2 = temp >> (CHAR_BIT * sizeof(int) - 1);
      temp ^= temp2;
      temp -= temp2;
      temp >>= Al;
      if (temp != 0) {
        zerobits |= ((size_t)1U) << k;
        signbits |= ((size_t)(temp2 + 1)) << k;
      }
      abs_ptr[k] = (UJCOEF)temp;
      if (temp == 1)
        EOB = k + 32;
    }
  }

  bits[1] = zerobits;
  bits[3] = signbits;

  return EOB;
}
