/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 *
 *  http://www.gnu.org/copyleft/gpl.txt
 */

/*****************************************************************************/

#include "met_to_data.h"

#include "../mlrpt/utils.h"
#include "bitop.h"
#include "correlator.h"
#include "ecc.h"
#include "viterbi27.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*****************************************************************************/

#define MIN_CORRELATION 45

/*****************************************************************************/

static void Do_Full_Correlate(mtd_rec_t *mtd, uint8_t *raw, uint8_t *aligned);
static void Do_Next_Correlate(mtd_rec_t *mtd, uint8_t *raw, uint8_t *aligned);
static bool Try_Frame(mtd_rec_t *mtd, uint8_t *aligned);

/*****************************************************************************/

static const uint8_t prand[255] = {
    0xff, 0x48, 0x0e, 0xc0, 0x9a, 0x0d, 0x70, 0xbc,
    0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce,
    0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e,
    0x0a, 0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1,
    0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79,
    0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c,
    0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c,
    0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63,
    0xfd, 0x20, 0x3b, 0x02, 0x68, 0x35, 0xc2, 0xf2,
    0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39,
    0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8,
    0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7,
    0xfa, 0x40, 0x76, 0x04, 0xd0, 0x6b, 0x85, 0xe4,
    0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72,
    0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0,
    0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f,
    0xf4, 0x80, 0xec, 0x09, 0xa0, 0xd7, 0x0b, 0xc8,
    0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5,
    0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0,
    0xa1, 0x0f, 0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f,
    0xe9, 0x01, 0xd8, 0x13, 0x41, 0xae, 0x17, 0x91,
    0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb,
    0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1,
    0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f,
    0xd2, 0x03, 0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23,
    0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96,
    0xa5, 0xdf, 0x73, 0x0c, 0xa8, 0xaf, 0xcf, 0x82,
    0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f,
    0xa4, 0x07, 0x60, 0x4d, 0x06, 0xb8, 0x5e, 0x47,
    0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d,
    0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x05,
    0x08, 0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58
};

static uint8_t *decoded = NULL;

/*****************************************************************************/

void Mtd_Init(mtd_rec_t *mtd) {
  //sync is $1ACFFC1D,  00011010 11001111 11111100 00011101
  Correlator_Init( &(mtd->c), (uint64_t)0xfca2b63db00d9794 );
  Mk_Viterbi27( &(mtd->v) );
  mtd->pos  = 0;
  mtd->cpos = 0;
  mtd->word = 0;
  mtd->corr = 64;
}

/*****************************************************************************/

static void Do_Full_Correlate(mtd_rec_t *mtd, uint8_t *raw, uint8_t *aligned) {
  mtd->word = (uint16_t)
    ( Corr_Correlate(&(mtd->c), &(raw[mtd->pos]), SOFT_FRAME_LEN) );
  mtd->cpos = (uint16_t)( mtd->c.position[mtd->word] );
  mtd->corr = (uint16_t)( mtd->c.correlation[mtd->word] );

  if( mtd->corr < MIN_CORRELATION )
  {
    mtd->prev_pos = mtd->pos;
    memmove( aligned, &(raw[mtd->pos]), SOFT_FRAME_LEN );
    mtd->pos += SOFT_FRAME_LEN / 4;
  }
  else
  {
    mtd->prev_pos = mtd->pos + (int)mtd->cpos;

    memmove(
        aligned,
        &(raw[mtd->pos + (int)mtd->cpos]),
        SOFT_FRAME_LEN - mtd->cpos );
    memmove(
        &(aligned[SOFT_FRAME_LEN - mtd->cpos]),
        &(raw[mtd->pos + SOFT_FRAME_LEN]),
        mtd->cpos );
    mtd->pos += SOFT_FRAME_LEN + mtd->cpos;

    Fix_Packet( aligned, SOFT_FRAME_LEN, (int)mtd->word );
  }
}

/*****************************************************************************/

static void Do_Next_Correlate(mtd_rec_t *mtd, uint8_t *raw, uint8_t *aligned) {
  mtd->cpos = 0;
  memmove( aligned, &(raw[mtd->pos]), SOFT_FRAME_LEN );
  mtd->prev_pos = mtd->pos;
  mtd->pos += SOFT_FRAME_LEN;

  Fix_Packet( aligned, SOFT_FRAME_LEN, (int)mtd->word );
}

/*****************************************************************************/

static bool Try_Frame(mtd_rec_t *mtd, uint8_t *aligned) {
  int j;
  uint8_t ecc_buf[256];
  uint32_t temp;

  if( decoded == NULL )
    mem_alloc( (void **)&decoded, HARD_FRAME_LEN );

  Vit_Decode( &(mtd->v), aligned, decoded );

  temp =
    ((uint32_t)decoded[3] << 24) +
    ((uint32_t)decoded[2] << 16) +
    ((uint32_t)decoded[1] <<  8) +
    (uint32_t)decoded[0];
  mtd->last_sync = temp;
  mtd->sig_q = (int)( round(100.0 - (Vit_Get_Percent_BER(&(mtd->v)) * 10.0)) );

  //Curiously enough, you can flip all bits in a packet
  //and get a correct ECC anyway. Check for that case
  if( Bitop_CountBits(mtd->last_sync ^ 0xE20330E5) <
      Bitop_CountBits(mtd->last_sync ^ 0x1DFCCF1A) )
  {
    for( j = 0; j < HARD_FRAME_LEN; j++ )
      decoded[j] ^= 0xFF;
    temp =
      ((uint32_t)decoded[3] << 24) +
      ((uint32_t)decoded[2] << 16) +
      ((uint32_t)decoded[1] <<  8) +
      (uint32_t)decoded[0];
    mtd->last_sync = temp;
  }

  for( j = 0; j < HARD_FRAME_LEN - 4; j++ )
    decoded[4 + j] ^= prand[j % 255];

  for( j = 0; j <= 3; j++ )
  {
    Ecc_Deinterleave( &(decoded[4]), ecc_buf, j, 4 );
    mtd->r[j] = Ecc_Decode( ecc_buf, 0 );
    Ecc_Interleave( ecc_buf, mtd->ecced_data, j, 4 );
  }

  return (mtd->r[0] && mtd->r[1] && mtd->r[2] && mtd->r[3]);
}

/*****************************************************************************/

bool Mtd_One_Frame(mtd_rec_t *mtd, uint8_t *raw) {
    uint8_t aligned[SOFT_FRAME_LEN];
    bool result = false;

    if (mtd->cpos == 0) {
        Do_Next_Correlate(mtd, raw, aligned);
        result = Try_Frame(mtd, aligned);

        if (!result)
            mtd->pos -= SOFT_FRAME_LEN;
    }

    if (!result) {
        Do_Full_Correlate(mtd, raw, aligned);
        result = Try_Frame(mtd, aligned);
    }

    return result;
}

/*****************************************************************************/

uint8_t ** ret_decoded(void) {
  return( &decoded );
}
