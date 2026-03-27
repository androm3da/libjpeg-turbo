/*
 * Hexagon HVX compatibility and helper macros
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#ifndef HVX_COMPAT_H
#define HVX_COMPAT_H

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

/* HVX vector length in bytes */
#define HVX_VLEN  128

/* Alignment macro for HVX vectors */
#define HVX_ALIGN  __attribute__((aligned(HVX_VLEN)))

/* Aligned and unaligned vector memory access.
 * vmemu() uses HVX_UVector for unaligned access --
 * the compiler generates vlalign/valign pairs
 * automatically.
 */
#define vmem(A)   *((HVX_Vector *)(A))
#define vmemu(A)  *((HVX_UVector *)(A))

#endif /* HVX_COMPAT_H */
