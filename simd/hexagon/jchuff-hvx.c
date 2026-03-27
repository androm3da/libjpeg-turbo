/*
 * Huffman entropy encoding of one DCT block (Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 *
 * NOTE: All referenced figures are from
 * Recommendation ITU-T T.81 (1992) | ISO/IEC 10918-1:1994.
 */

#include "../jsimdint.h"
#include "hvx-compat.h"
/* jchuff.h is already included through jsimdint.h */


/* Zigzag order table for 8x8 DCT block */
static const int jpeg_natural_order_hvx[DCTSIZE2] = {
   0,  1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63
};


/* Expanded entropy encoder object for Huffman encoding.
 *
 * The savable_state subrecord contains fields that change within an MCU,
 * but must not be updated permanently until we complete the MCU.
 */

/* Must match struct layout in jchuff.c exactly.
 * On 32-bit Hexagon with WITH_SIMD, simd_bit_buf_type is
 * unsigned long long (8 bytes) due to the #if in jchuff.c
 * that only exempts Arm.
 */
#define BIT_BUF_SIZE  64

typedef unsigned long long simd_bit_buf_type;

typedef struct {
  union {
    size_t c;
    simd_bit_buf_type simd;
  } put_buffer;
  int free_bits;
  int last_dc_val[MAX_COMPS_IN_SCAN];
} savable_state;

typedef struct {
  JOCTET *next_output_byte;
  size_t free_in_buffer;
  savable_state cur;
  j_compress_ptr cinfo;
  int simd;
} working_state;


/* Outputting bits to the file */

/* Output byte b and, speculatively, an additional 0 byte.  0xFF must be
 * encoded as 0xFF 0x00, so the output buffer pointer is advanced by 2 if the
 * byte is 0xFF.  Otherwise, the output buffer pointer is advanced by 1, and
 * the speculative 0 byte will be overwritten by the next byte.
 */
#define EMIT_BYTE(b) { \
  buffer[0] = (JOCTET)(b); \
  buffer[1] = 0; \
  buffer -= -2 + ((JOCTET)(b) < 0xFF); \
}

/* Output the entire bit buffer.  If there are no 0xFF bytes in it, then write
 * directly to the output buffer.  Otherwise, use the EMIT_BYTE() macro to
 * encode 0xFF as 0xFF 0x00.
 */
#define FLUSH() { \
  if (put_buffer & 0x8080808080808080ULL & \
      ~(put_buffer + 0x0101010101010101ULL)) { \
    EMIT_BYTE(put_buffer >> 56) \
    EMIT_BYTE(put_buffer >> 48) \
    EMIT_BYTE(put_buffer >> 40) \
    EMIT_BYTE(put_buffer >> 32) \
    EMIT_BYTE(put_buffer >> 24) \
    EMIT_BYTE(put_buffer >> 16) \
    EMIT_BYTE(put_buffer >>  8) \
    EMIT_BYTE(put_buffer      ) \
  } else { \
    buffer[0] = (JOCTET)(put_buffer >> 56); \
    buffer[1] = (JOCTET)(put_buffer >> 48); \
    buffer[2] = (JOCTET)(put_buffer >> 40); \
    buffer[3] = (JOCTET)(put_buffer >> 32); \
    buffer[4] = (JOCTET)(put_buffer >> 24); \
    buffer[5] = (JOCTET)(put_buffer >> 16); \
    buffer[6] = (JOCTET)(put_buffer >>  8); \
    buffer[7] = (JOCTET)(put_buffer      ); \
    buffer += 8; \
  } \
}

/* Fill the bit buffer to capacity with the leading bits from code, then output
 * the bit buffer and put the remaining bits from code into the bit buffer.
 */
#define PUT_AND_FLUSH(code, size) { \
  put_buffer = (put_buffer << (size + free_bits)) | (code >> -free_bits); \
  FLUSH() \
  free_bits += BIT_BUF_SIZE; \
  put_buffer = code; \
}

/* Insert code into the bit buffer and output the bit buffer if needed.
 * NOTE: We can't flush with free_bits == 0, since the left shift in
 * PUT_AND_FLUSH() would have undefined behavior.
 */
#define PUT_BITS(code, size) { \
  free_bits -= size; \
  if (free_bits < 0) \
    PUT_AND_FLUSH(code, size) \
  else \
    put_buffer = (put_buffer << size) | code; \
}

#define PUT_CODE(code, size, diff) { \
  diff |= code << nbits; \
  nbits += size; \
  PUT_BITS(diff, nbits) \
}


JOCTET *jsimd_huff_encode_one_block_hvx(void *state, JOCTET *buffer,
                                         JCOEFPTR block, int last_dc_val,
                                         c_derived_tbl *dctbl,
                                         c_derived_tbl *actbl)
{
  unsigned char block_nbits[DCTSIZE2];
  unsigned short block_diff[DCTSIZE2];

  /* Compute nbits and diff values for all 64 coefficients.
   * The first coefficient (DC) uses the difference from the last DC value.
   */
  int k;
  short coef;
  short abs_coef;
  int lz;

  /* DC coefficient */
  coef = (short)(block[0] - last_dc_val);
  abs_coef = (coef < 0) ? -coef : coef;
  lz = (abs_coef == 0) ? 16 : __builtin_clz((unsigned int)abs_coef) - 16;
  block_nbits[0] = (unsigned char)(16 - lz);
  /* For negative coefficients: diff = abs(coef) ^ mask, where mask has
   * (16 - lz) bits set.  This is equivalent to abs(coef) - 1 for negative
   * values, yielding the one's complement representation.
   */
  {
    unsigned short mask = (coef < 0) ? (unsigned short)(((short)-1) >> lz) : 0;
    block_diff[0] = (unsigned short)abs_coef ^ mask;
  }

  /* AC coefficients in zig-zag order */
  for (k = 1; k < DCTSIZE2; k++) {
    coef = (short)block[jpeg_natural_order_hvx[k]];
    abs_coef = (coef < 0) ? -coef : coef;
    lz = (abs_coef == 0) ? 16 : __builtin_clz((unsigned int)abs_coef) - 16;
    block_nbits[k] = (unsigned char)(16 - lz);
    {
      unsigned short mask =
        (coef < 0) ? (unsigned short)(((short)-1) >> lz) : 0;
      block_diff[k] = (unsigned short)abs_coef ^ mask;
    }
  }

  /* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
   * means that the corresponding coefficient != 0.
   */
  unsigned int bitmap_1_32 = 0;
  unsigned int bitmap_33_63 = 0;
  for (k = 1; k <= 32; k++) {
    if (block_nbits[k] != 0)
      bitmap_1_32 |= (unsigned int)1U << (32 - k);
  }
  for (k = 33; k < 64; k++) {
    if (block_nbits[k] != 0)
      bitmap_33_63 |= (unsigned int)1U << (63 - k);
  }

  /* Set up state and bit buffer for output bitstream. */
  working_state *state_ptr = (working_state *)state;
  int free_bits = state_ptr->cur.free_bits;
  simd_bit_buf_type put_buffer =
    state_ptr->cur.put_buffer.simd;

  /* Encode DC coefficient. */

  unsigned int nbits = block_nbits[0];
  /* Emit Huffman-coded symbol and additional diff bits. */
  unsigned int diff = block_diff[0];
  PUT_CODE(dctbl->ehufco[nbits], dctbl->ehufsi[nbits], diff)

  /* Encode AC coefficients. */

  unsigned int r = 0;  /* r = run length of zeros */
  unsigned int i = 1;  /* i = number of coefficients encoded */
  /* Code and size information for a run length of 16 zero coefficients */
  const unsigned int code_0xf0 = actbl->ehufco[0xf0];
  const unsigned int size_0xf0 = actbl->ehufsi[0xf0];

  while (bitmap_1_32 != 0) {
    r = __builtin_clz(bitmap_1_32);
    i += r;
    bitmap_1_32 <<= r;
    nbits = block_nbits[i];
    diff = block_diff[i];
    while (r > 15) {
      /* If run length > 15, emit special run-length-16 codes. */
      PUT_BITS(code_0xf0, size_0xf0)
      r -= 16;
    }
    /* Emit Huffman symbol for run length / number of bits. (F.1.2.2.1) */
    unsigned int rs = (r << 4) + nbits;
    PUT_CODE(actbl->ehufco[rs], actbl->ehufsi[rs], diff)
    i++;
    bitmap_1_32 <<= 1;
  }

  r = 33 - i;
  i = 33;

  while (bitmap_33_63 != 0) {
    unsigned int leading_zeros = __builtin_clz(bitmap_33_63);
    r += leading_zeros;
    i += leading_zeros;
    bitmap_33_63 <<= leading_zeros;
    nbits = block_nbits[i];
    diff = block_diff[i];
    while (r > 15) {
      /* If run length > 15, emit special run-length-16 codes. */
      PUT_BITS(code_0xf0, size_0xf0)
      r -= 16;
    }
    /* Emit Huffman symbol for run length / number of bits. (F.1.2.2.1) */
    unsigned int rs = (r << 4) + nbits;
    PUT_CODE(actbl->ehufco[rs], actbl->ehufsi[rs], diff)
    r = 0;
    i++;
    bitmap_33_63 <<= 1;
  }

  /* If the last coefficient(s) were zero, emit an end-of-block (EOB) code.
   * The value of RS for the EOB code is 0.
   */
  if (i != 64) {
    PUT_BITS(actbl->ehufco[0], actbl->ehufsi[0])
  }

  state_ptr->cur.put_buffer.simd = put_buffer;
  state_ptr->cur.free_bits = free_bits;

  return buffer;
}
