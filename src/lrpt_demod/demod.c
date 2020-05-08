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

#include "demod.h"
#include "../common/shared.h"

static BOOLEAN (*Demod_PSK)(complex double, int8_t *);
//static int8_t    *deint_buf = NULL; /* De-interleave buffer */

/*------------------------------------------------------------------------*/

/* Demod_QPSK()
 *
 * Demodulate QPSK signal from Meteor
 */
  static BOOLEAN
Demod_QPSK( complex double fdata, int8_t *buffer )
{
  static complex double
    before = 0.0,
    mid    = 0.0,
    cur    = 0.0;

  static double
    resync_offset = 0.0,
    sym_period    = 0.0,
    sp2, sp2p1;

  double resync_error, delta;

  static int buf_idx = 0;

  int8_t *buf_lowr = buffer + DEMOD_BUF_LOWR;
  int8_t *buf_midl = buffer + DEMOD_BUF_MIDL;


  /* Static values initialization */
  if( sym_period == 0.0 )
  {
    sym_period = demodulator->sym_period;
    sp2        = sym_period / 2.0;
    sp2p1      = sp2 + 1.0;
  }

  /* Symbol timing recovery (Gardner) */
  if( (resync_offset >= sp2) && (resync_offset < sp2p1) )
  {
    mid = Agc_Apply( demodulator->agc, fdata );
  }
  else if( resync_offset >= sym_period )
  {
    cur = Agc_Apply( demodulator->agc, fdata );
    resync_offset -= sym_period;
    resync_error   = ( cimag(cur) - cimag(before) ) * cimag(mid);
    resync_offset += ( resync_error * sym_period / RESYNC_SCALE_QPSK );
    before = cur;

    /* Costas loop frequency/phase tuning */
    cur = Costas_Mix( demodulator->costas, cur );
    delta = Costas_Delta( cur, cur );
    Costas_Correct_Phase( demodulator->costas, delta );

    resync_offset += 1.0;

    /* Save result in demod buffer */
    buf_lowr[buf_idx++] = Clamp_Int8( creal(cur) / 2.0 );
    buf_lowr[buf_idx++] = Clamp_Int8( cimag(cur) / 2.0 );

    /* Copy symbols in the local buffer to
     * the Demodulator buffer and return */
    if( buf_idx >= SOFT_FRAME_LEN )
    {
      /* Move the 2 lower parts of Demodulator buffer to the top */
      memmove( buffer, buf_midl, DEMOD_BUF_LOWR );
      buf_idx = 0;
      return( TRUE );
    }

    return( FALSE );
  } /* else if( resync_offset >= sym_period ) */

  resync_offset += 1.0;
  return( FALSE );

} /* Demod_QPSK() */

/*------------------------------------------------------------------------*/

/* Demod_DOQPSK()
 *
 * Demodulate DOQPSK signal from Meteor
 */
  static BOOLEAN
Demod_DOQPSK( complex double fdata, int8_t *buffer )
{
  complex double quad, agc;

  static complex double
    inphase = 0.0,
    before  = 0.0,
    mid     = 0.0,
    cur     = 0.0;

  static double
    resync_offset = 0.0,
    prev_i        = 0.0,
    sym_period    = 0.0,
    sp2, sp2p1;

  double resync_error, delta;
  static int buf_idx = 0;

  int8_t *buf_lowr = buffer + DEMOD_BUF_LOWR;
  int8_t *buf_midl = buffer + DEMOD_BUF_MIDL;


  /* Static values initialization */
  if( sym_period != demodulator->sym_period )
  {
    sym_period = demodulator->sym_period;
    sp2        = sym_period / 2.0;
    sp2p1      = sp2 + 1.0;
  }

  /* Symbol timing recovery (Gardner) */
  if( (resync_offset >= sp2) && (resync_offset < sp2p1) )
  {
    agc     = Agc_Apply( demodulator->agc, fdata );
    inphase = Costas_Mix( demodulator->costas, agc );
    mid     = prev_i + (complex double)I * cimag( inphase );
    prev_i  = creal( inphase );
  }
  else if( resync_offset >= sym_period )
  {
    /* Symbol timing recovery (Gardner) */
    agc    = Agc_Apply( demodulator->agc, fdata );
    quad   = Costas_Mix( demodulator->costas, agc );
    cur    = prev_i + (complex double)I * cimag( quad );
    prev_i = creal( quad );

    resync_offset -= sym_period;
    resync_error   = ( cimag(quad) - cimag(before) ) * cimag( mid );
    resync_offset += resync_error * sym_period / RESYNC_SCALE_DOQPSK;
    before = cur;

    /* Carrier tracking */
    delta = Costas_Delta( inphase, quad );
    Costas_Correct_Phase( demodulator->costas, delta );

    resync_offset += 1.0;

    /* Save result in demod buffer */
    buf_lowr[buf_idx++] = Clamp_Int8( creal(cur) / 2.0 );
    buf_lowr[buf_idx++] = Clamp_Int8( cimag(cur) / 2.0 );

    /* Copy symbols in the local buffer to
     * the Demodulator buffer and return */
    if( buf_idx >= SOFT_FRAME_LEN )
    {
      /* Move the 2 lower parts of Demodulator buffer to the top */
      De_Diffcode( buf_lowr, SOFT_FRAME_LEN );
      memmove( buffer, buf_midl, DEMOD_BUF_LOWR );
      buf_idx = 0;
      return( TRUE );
    }

    return( FALSE );
  } /* else if( resync_offset >= sym_period ) */

  resync_offset += 1.0;
  return( FALSE );

} /* Demod_DOQPSK() */

/*------------------------------------------------------------------------*/

/* Demod_IDOQPSK()
 *
 * Demodulate Interleaved DOQPSK signal from Meteor
 */
  static BOOLEAN
Demod_IDOQPSK( complex double fdata, int8_t *out_buf )
{
  static int out_buf_idx  = 0;

  static uint8_t *raw_buf = NULL;
  static int raw_buf_size = RAW_BUF_REALLOC, raw_buf_idx = 0;

  static uint8_t *resync_buf = NULL;
  static int resync_siz = 0, resync_idx = 0, copy_siz = 0;

  complex double quad, agc;

  static complex double
    inphase = 0.0,
    before  = 0.0,
    mid     = 0.0,
    cur     = 0.0;

  static double
    resync_offset = 0.0,
    prev_i        = 0.0,
    sym_period    = 0.0,
    sp2, sp2p1;

  double resync_error, delta;

  int8_t *buf_lowr = out_buf + DEMOD_BUF_LOWR;
  int8_t *buf_midl = out_buf + DEMOD_BUF_MIDL;

  static uint8_t deint_done = 0;


  /* Static values initialization */
  if( sym_period != demodulator->sym_period )
  {
    sym_period = demodulator->sym_period;
    sp2        = sym_period / 2.0;
    sp2p1      = sp2 + 1.0;

    /* Allocate raw buffer */
    mem_alloc( (void **)&raw_buf, (size_t)raw_buf_size );
  }

  if( isFlagClear(ACTION_IDOQPSK_STOP) )
  {
    /* Symbol timing recovery (Gardner) */
    if( (resync_offset >= sp2) && (resync_offset < sp2p1) )
    {
      agc     = Agc_Apply( demodulator->agc, fdata );
      inphase = Costas_Mix( demodulator->costas, agc );
      mid     = prev_i + (complex double)I * cimag( inphase );
      prev_i  = creal( inphase );
    }
    else if( resync_offset >= sym_period )
    {
      /* Symbol timing recovery (Gardner) */
      agc    = Agc_Apply( demodulator->agc, fdata );
      quad   = Costas_Mix( demodulator->costas, agc );
      cur    = prev_i + (complex double)I * cimag( quad );
      prev_i = creal( quad );

      resync_offset -= sym_period;
      resync_error   = ( cimag(quad) - cimag(before) ) * cimag( mid );
      resync_offset += resync_error * sym_period / RESYNC_SCALE_IDOQPSK;
      before = cur;

      /* Carrier tracking */
      delta = Costas_Delta( inphase, quad );
      Costas_Correct_Phase( demodulator->costas, delta );

      /* Save result in raw buffer */
      raw_buf[raw_buf_idx++] = (uint8_t)Clamp_Int8( creal(cur) / 2.0 );
      raw_buf[raw_buf_idx++] = (uint8_t)Clamp_Int8( cimag(cur) / 2.0 );

      if( raw_buf_idx >= raw_buf_size )
      {
        raw_buf_size += RAW_BUF_REALLOC;
        mem_realloc( (void **)&raw_buf, (size_t)raw_buf_size );
      }

      resync_offset += 1.0;
      return( FALSE );
    } /* else if( resync_offset >= sym_period ) */

    resync_offset += 1.0;
    return( FALSE );
  } /* if( isFlagClear(ACTION_IDOQPSK_STOP) ) */

  /* De-interleave raw symbols buffer */
  if( !deint_done )
  {
    De_Interleave( raw_buf, raw_buf_size, &resync_buf, &resync_siz );
    raw_buf_idx = 0;
    resync_idx  = 0;
    deint_done  = 1;
  }

  /* Incrementally transfer data to the demod buffer */
  if( resync_siz )
  {
    copy_siz = SOFT_FRAME_LEN - out_buf_idx;
    if( copy_siz > resync_siz ) copy_siz = resync_siz;
    memcpy(
        buf_lowr   + out_buf_idx,
        resync_buf + resync_idx,
        (size_t)copy_siz );
    out_buf_idx += copy_siz;
    resync_siz  -= copy_siz;
    resync_idx  += copy_siz;
  }

  /* Copy symbols in the local buffer to
   * the Demodulator buffer and return */
  if( out_buf_idx >= SOFT_FRAME_LEN )
  {
    /* Undo differential modulation */
    De_Diffcode( buf_lowr, SOFT_FRAME_LEN );

    /* Move the 2 lower parts of Demodulator buffer to the top */
    memmove( out_buf, buf_midl, DEMOD_BUF_LOWR );
    out_buf_idx = 0;
    return( TRUE );
  }

  ClearFlag( ACTION_FLAGS_ALL );
  return( FALSE );
} /* Demod_IDOQPSK() */

/*------------------------------------------------------------------------*/

/* Demod_Init()
 *
 * Initializes Demodulator Object
 */
  void
Demod_Init( void )
{
  /* Create and allocate a Demodulator object */
  mem_alloc( (void **)&demodulator, sizeof(Demod_t) );

  /* Initialize the AGC */
  demodulator->agc = Agc_Init();

  /* Initialize Costas loop */
  double pll_bw =
    M_2PI * rc_data.costas_bandwidth / (double)rc_data.symbol_rate;
  demodulator->costas = Costas_Init( pll_bw, rc_data.psk_mode );
  demodulator->mode   = rc_data.psk_mode;

  /* Initialize the timing recovery variables */
  demodulator->sym_rate   = rc_data.symbol_rate;
  demodulator->sym_period = (double)rc_data.interp_factor *
    rc_data.demod_samplerate / (double)rc_data.symbol_rate;

  /* Initialize RRC filter */
  double osf = rc_data.demod_samplerate / (double)rc_data.symbol_rate;
  demodulator->rrc = Filter_RRC(
      rc_data.rrc_order, rc_data.interp_factor, osf, rc_data.rrc_alpha );

  /* Select demodulator (QPSK|DOQPSK|IDOQPSK) function */
  //mem_alloc( (void **)&demod_buf, SOFT_FRAME_LEN );
  switch( rc_data.psk_mode )
  {
    case QPSK:
      Free_Isqrt_Table();
      Demod_PSK = Demod_QPSK;
      break;

    case DOQPSK:
      /* Make 16k integer square root table
       * and allocate demod buffer for OQPSK */
      Make_Isqrt_Table();
      Demod_PSK = Demod_DOQPSK;
      break;

    case IDOQPSK:
      /* Make 16k integer square root table
       * and allocate demod buffer for OQPSK */
      Make_Isqrt_Table();
      Demod_PSK = Demod_IDOQPSK;
      break;
  }
} /* Demod_Init() */

/*------------------------------------------------------------------------*/

/* Demod_Deinit()
 *
 * De-initializes (frees) Demodulator Object
 */
  void
Demod_Deinit( void )
{
  Agc_Free( demodulator->agc );
  Costas_Free( demodulator->costas );
  Filter_Free( demodulator->rrc );
  free_ptr( (void **)&demodulator );
} /* Demod_Deinit() */

/*------------------------------------------------------------------------*/

/*
 * These functions return Agc Gain, Signal Level and Costas PLL
 * Average Error in the range of 0.0-1.0 for the level gauges
 */

  double
Agc_Gain( double *gain )
{
  double ret = 0.0;

  /* Return gain if non-null argument */
  if( demodulator )
  {
    ret = - log10( demodulator->agc->gain ) / AGC_RANGE1;
    ret = dClamp( ret, 0.0, 1.0 );
    if( gain ) *gain = demodulator->agc->gain;
  }

  return( ret );
} /* Agc_Gain() */

/*----------------------------------------------------------------------*/

  double
Signal_Level( uint32_t *level )
{
  double ret = 0.0;

  /* Return signal level if non-null argument */
  if( demodulator )
  {
    ret = demodulator->agc->average / AGC_AVE_RANGE;
    ret = dClamp( ret, 0.0, 1.0 );
    if( level ) *level = (uint32_t)demodulator->agc->average;
  }
  return( ret );
} /* Signal_Level() */

/*----------------------------------------------------------------------*/

  double
Pll_Average( void )
{
  double ret = 0.0;

  /* We display a range of 0.1 to 0.5 */
  if( demodulator )
  {
    ret = demodulator->costas->moving_average - PLL_AVE_RANGE1;
    ret = 1.0 - PLL_AVE_RANGE2 * ret;
    ret = dClamp( ret, 0.0, 1.0 );
  }
  return( ret );
} /* Pll_Average() */

/*------------------------------------------------------------------------*/

/* Demodulator_Run()
 *
 * Runs the Demodulator functions and supplies
 * soft symbols to the LRPT decoder functions
 */
  void
Demodulator_Run( void )
{
  uint32_t count, done, idx;
  complex double  cdata, fdata;
  static int8_t  *out_buffer = NULL;


  /* Allocate output buffer on first call. It is 3 sections
   * of SOFT_FRAME_LEN size, top and middle sections are used
   * by the image decoder and the lower for saving new data */
  if( !out_buffer )
    mem_alloc( (void **)&out_buffer, 3 * SOFT_FRAME_LEN );

  /* On user stop action, exit loop */
  while( isFlagSet(ACTION_RECEIVER_ON) )
  {
    /* Wait on DSP data to be ready for processing */
    sem_wait( &demod_semaphore );

    /* Filter samples from SDR receiver */
    DSP_Filter( &filter_data_i );
    DSP_Filter( &filter_data_q );

    /* Process I/Q data from the SDR Receiver */
    count = 0;
    done  = filter_data_i.samples_buf_len;
    while( count < done )
    {
      /* Convert filtered samples to complex variable */
      cdata =
        filter_data_i.samples_buf[count] +
        filter_data_q.samples_buf[count] * (complex double)I;

      /* The interpolation and RRC filtering is now
       * incorporated here in the demodulator code */
      for( idx = 0; idx < rc_data.interp_factor; idx++ )
      {
        /* Pass samples through interpolator RRC filter */
        fdata = Filter_Fwd( demodulator->rrc, cdata );

        /* Demodulate using appropriate function (QPSK|DOQPSK|IDOQPSK).
         * Try to decode one or more LRPT frames when PLL is locked */
        if( Demod_PSK(fdata, out_buffer) &&
            demodulator->costas->locked  &&
            isFlagSet(ACTION_DECODE_IMAGES) )
        {
          /* Try to decode one or more LRPT frames */
          Decode_Image( (uint8_t *)out_buffer, SOFT_FRAME_LEN );

          /* The mtd_record.pos and mtd_record.prev_pos pointers must be
           * decrimented to point back to the same data in the soft buffer */
          mtd_record.pos      -= SOFT_FRAME_LEN;
          mtd_record.prev_pos -= SOFT_FRAME_LEN;

        } /* if( demodulator->costas->locked ... ) */
      } /* for( idx = 0; idx < rc_data.interp_mult; idx++ ) */

      count++;
    } /* while( count < done ) */
  } /* while( isFlagSet(ACTION_RECEIVER_ON) ) */

  Mj_Dump_Image();
  Cleanup();
  free_ptr( (void **)&out_buffer );
  Print_Message( _("Receiving and Decoding Ended"), INFO_MESG );

  return;
} /* Demodulator_Run() */

/*----------------------------------------------------------------------*/
