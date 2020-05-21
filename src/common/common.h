/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
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

#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H

/*****************************************************************************/

#include <limits.h>

/*****************************************************************************/

/* Control flags */
#define ACTION_RECEIVER_ON      0x000001 /* Start SDR Rx and demodulator  */
#define ACTION_DECODE_IMAGES    0x000002 /* Decode images from satellite  */
#define ACTION_IDOQPSK_STOP     0x000004 /* Stop buffering in Demod_IDOQPSK() */
#define ACTION_FLAGS_ALL        0x000007 /* All action flags (clearing)   */
#define IMAGE_RAW               0x000008 /* Save image in raw decoded state */
#define IMAGE_NORMALIZE         0x000010 /* Histogram normalize wx image    */
#define IMAGE_CLAHE             0x000020 /* CLAHE image contrast enhance  */
#define IMAGE_COLORIZE          0x000040 /* Pseudo colorize wx image      */
#define IMAGE_INVERT            0x000080 /* Rotate wx image 180 degrees   */
#define IMAGE_OUT_SPLIT         0x000100 /* Save individual channel image */
#define IMAGE_OUT_COMBO         0x000200 /* Combine & save channel images */
#define ALL_INITIALIZED         0x000400 /* All mlrpt systems initialized */
#define ALARM_ACTION_START      0x000800 /* Start Operation on SIGALRM    */
#define IMAGE_SAVE_JPEG         0x001000 /* Save channel images as JPEG   */
#define IMAGE_SAVE_PPGM         0x002000 /* Save channel image as PGM|PPM */
#define IMAGE_RECTIFY           0x004000 /* Images have been rectified */
#define VERBOSE_MODE            0x008000 /* Run verbose (print messages)  */

/* Number of APID image channels */
#define CHANNEL_IMAGE_NUM   3

/* Indices for normalization range black and white values */
#define NORM_RANGE_BLACK    0
#define NORM_RANGE_WHITE    1

/* Max and min filter bandwidth */
#define MIN_BANDWIDTH   100000
#define MAX_BANDWIDTH   200000

/* Size of char arrays (strings) for text messages */
#define MESG_SIZE   512

/* Maximum time duration in sec of satellite signal processing */
#define MAX_OPERATION_TIME  1000

/* Neoklis Kyriazis' addition, width (in pixels) of image
 * METEOR_IMAGE_WIDTH = MCU_PER_LINE * 8; MCU_PER_LINE = 196
 */
#define METEOR_IMAGE_WIDTH  1568

/* TODO use booleans */
/* Return values */
#define ERROR       1
#define SUCCESS     0

/* General definitions for image processing */
#define MAX_FILE_NAME   PATH_MAX /* Max length for filenames */

/* Safe fallback */
#ifndef M_2PI
#define M_2PI 6.28318530717958647692
#endif

/* DSP Filter Parameters */
#define FILTER_RIPPLE   5.0
#define FILTER_POLES    6

/*****************************************************************************/

/* SDR receiver types */
enum {
    SDR_TYPE_RTLSDR = 1,
    SDR_TYPE_AIRSPY,
    SDR_TYPE_NUM
};

enum {
    INIT_BWORTH_LPF = 1,
    RTL_DAGC_ENABLE
};

/* Filter types */
enum {
    FILTER_LOWPASS = 1,
    FILTER_HIGHPASS,
    FILTER_BANDPASS
};

/* Image channels (0-2) */
enum {
    RED = 0,
    GREEN,
    BLUE
};

/* Flags to select images for output */
enum {
    OUT_COMBO = 1,
    OUT_SPLIT,
    OUT_BOTH
};

/* Flags to indicate image file type to save as */
enum {
    SAVEAS_JPEG = 1,
    SAVEAS_PGM,
    SAVEAS_BOTH
};

/* Flags to indicate images to be saved */
enum {
    CH0 = 0,
    CH1,
    CH2,
    COMBO
};

/* Message types */
enum {
    ERROR_MESG = 1,
    INFO_MESG
};

/*****************************************************************************/

#endif
