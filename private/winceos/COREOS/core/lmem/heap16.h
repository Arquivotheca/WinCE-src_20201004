//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _HEAP16_H
#define _HEAP16_H

#ifdef __cplusplus
extern "C" {
#endif

// heap signature
#define HEAPSIG_16                          0x50616544UL

// default block align shift (16 bytes)
#define RHEAP_BLOCK_SIZE_SHIFT_16           4

// rgn header align sizes
#define RHEAP_FIXBLK_HDR_ALIGN_1_16         0x0F
#define RHEAP_FIXBLK_HDR_ALIGN_2_16         0x1F
#define RHEAP_FIXBLK_HDR_ALIGN_4_16         0x3F

//
// Number of blks unused in a fix alloc rgn.
// This depends on number of subrgns.
//
// Each subrgn has "2" blocks unused - except 
// the last one which has only 1 blk unused 
// as rgn size is < 64k.
//
//
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_1_16   30
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_2_16   15
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_4_16   7

//
// Number of sub rgns within a fix alloc rgn
// for 16 byte allocation sizes.
//
// Each rgn size is 60k (last page in 64k
// block is not committed).
//
// Each subrgn is given by 256 blocks where
// each block size is 16, 32, or 64 bytes.
//  
//
#define RHEAP_FIXBLK_NUM_SUBRGNS_1_16       15
#define RHEAP_FIXBLK_NUM_SUBRGNS_2_16       8
#define RHEAP_FIXBLK_NUM_SUBRGNS_4_16       4

// generic symbols (used by rheap.cpp)
#define HEAPSIG                             HEAPSIG_16
#define RHEAP_BLOCK_SIZE_SHIFT              RHEAP_BLOCK_SIZE_SHIFT_16
#define RHEAP_FIXBLK_HDR_ALIGN_1            RHEAP_FIXBLK_HDR_ALIGN_1_16
#define RHEAP_FIXBLK_HDR_ALIGN_2            RHEAP_FIXBLK_HDR_ALIGN_2_16
#define RHEAP_FIXBLK_HDR_ALIGN_4            RHEAP_FIXBLK_HDR_ALIGN_4_16
#define RHEAP_FIXBLK_NUM_SUBRGNS_1          RHEAP_FIXBLK_NUM_SUBRGNS_1_16
#define RHEAP_FIXBLK_NUM_SUBRGNS_2          RHEAP_FIXBLK_NUM_SUBRGNS_2_16
#define RHEAP_FIXBLK_NUM_SUBRGNS_4          RHEAP_FIXBLK_NUM_SUBRGNS_4_16
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_1      RHEAP_FIXBLK_NUM_UNUSED_BLKS_1_16
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_2      RHEAP_FIXBLK_NUM_UNUSED_BLKS_2_16
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_4      RHEAP_FIXBLK_NUM_UNUSED_BLKS_4_16



#ifdef __cplusplus
}
#endif

#endif //  _HEAP16_H

