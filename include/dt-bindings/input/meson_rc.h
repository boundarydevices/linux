
#ifndef _DT_BINDINGS_INPUT_MESON_RC_H
#define _DT_BINDINGS_INPUT_MESON_RC_H

#define REMOTE_KEY(scancode, keycode)\
		((((scancode) & 0xFFFF)<<16) | ((keycode) & 0xFFFF))
/**
 *GXM GXL GXTVBB TXL platform integrated with two IR controllers.
 *1. legacy IR contoller(Only support NEC and Time Measurement)
 *2. multi-format IR controller
 *
 *There are multiple decode mode based on two IR controllers.
 *using bit[8-15] of 'type' to identify legacy IR.
 *using bit[0-7] of 'type' to identify multi-format IR.
 */

/*hardware decode one protocol by using multi-format IR controller*/
#define     REMOTE_TYPE_UNKNOWN      0x00
#define     REMOTE_TYPE_NEC          0x01
#define     REMOTE_TYPE_DUOKAN       0x02
#define     REMOTE_TYPE_XMP_1        0x03
#define     REMOTE_TYPE_RC5          0x04
#define     REMOTE_TYPE_RC6          0x05

/*hardware decode one protocol by using legacy IR controller*/
#define     REMOTE_TYPE_LEGACY_NEC  0xff

/**
 *software decode multiple protocols by using Time measurement
 *of multi-format IR controller
 *using bit[7] to identify the software decode
 */
#define     REMOTE_TYPE_RAW_NEC     0x81
#define     REMOTE_TYPE_RAW_XMP_1   0x83

/**
 *hardware decode two protocols
 *1. legacy IR controller decode NEC protocol
 *2. multi-format IR controller decode other protocol
 */
#define REMOTE_TYPE_NEC_RC6  ((REMOTE_TYPE_LEGACY_NEC << 8) | REMOTE_TYPE_RC6)

#endif
