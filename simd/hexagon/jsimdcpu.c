/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * Qualcomm Hexagon processor with HVX support.
 */

#include "../jsimdint.h"


HIDDEN unsigned int
jpeg_simd_cpu_support(void)
{
  /* HVX is always present when the code is compiled with -mhvx. */
  return JSIMD_HVX;
}
