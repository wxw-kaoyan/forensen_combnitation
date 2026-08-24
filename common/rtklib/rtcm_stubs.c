/* Stubs for unused RTKLIB functions to avoid pulling in additional source files.
 * This project only needs RTCM 3.x decoding (input_rtcm3). */

#include "rtklib.h"

/* RTCM 2 decoder - not needed */
extern int decode_rtcm2(rtcm_t *rtcm)
{
    (void)rtcm;
    return 0;
}

/* RTCM 3 encoder - not needed */
extern int encode_rtcm3(rtcm_t *rtcm, int type, int sync)
{
    (void)rtcm;
    (void)type;
    (void)sync;
    return 0;
}
