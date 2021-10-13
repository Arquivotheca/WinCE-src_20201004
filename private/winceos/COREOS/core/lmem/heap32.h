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
#ifndef _HEAP32_H
#define _HEAP32_H

#ifdef __cplusplus
extern "C" {
#endif

// heap signature
#define HEAPSIG_32                          0x50616548UL

// default block align shift (32 bytes)
#define RHEAP_BLOCK_SIZE_SHIFT_32           5

// rgn header align sizes
#define RHEAP_FIXBLK_HDR_ALIGN_1_32         0x1F
#define RHEAP_FIXBLK_HDR_ALIGN_2_32         0x3F
#define RHEAP_FIXBLK_HDR_ALIGN_4_32         0x7F

//
// Number of blks unused in a fix alloc rgn.
// This depends on number of subrgns.
//
// Each subrgn has "2" blocks unused - except 
// the last one which has only 1 blk unused 
// as rgn size is < 64k.
//
//
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_1_32   15
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_2_32   7
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_4_32   3

//
// Number of sub rgns within a fix alloc rgn
// for 32 byte allocation sizes.
//
// Each rgn size is 60k (last page in 64k
// block is not committed).
//
// Each subrgn is given by 256 blocks where
// each block size is 16, 32, or 64 bytes.
//  
//
#define RHEAP_FIXBLK_NUM_SUBRGNS_1_32       8
#define RHEAP_FIXBLK_NUM_SUBRGNS_2_32       4
#define RHEAP_FIXBLK_NUM_SUBRGNS_4_32       2

// generic symbols (used by rheap.cpp)
#define HEAPSIG                             HEAPSIG_32
#define RHEAP_BLOCK_SIZE_SHIFT              RHEAP_BLOCK_SIZE_SHIFT_32
#define RHEAP_FIXBLK_HDR_ALIGN_1            RHEAP_FIXBLK_HDR_ALIGN_1_32
#define RHEAP_FIXBLK_HDR_ALIGN_2            RHEAP_FIXBLK_HDR_ALIGN_2_32
#define RHEAP_FIXBLK_HDR_ALIGN_4            RHEAP_FIXBLK_HDR_ALIGN_4_32
#define RHEAP_FIXBLK_NUM_SUBRGNS_1          RHEAP_FIXBLK_NUM_SUBRGNS_1_32
#define RHEAP_FIXBLK_NUM_SUBRGNS_2          RHEAP_FIXBLK_NUM_SUBRGNS_2_32
#define RHEAP_FIXBLK_NUM_SUBRGNS_4          RHEAP_FIXBLK_NUM_SUBRGNS_4_32
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_1      RHEAP_FIXBLK_NUM_UNUSED_BLKS_1_32
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_2      RHEAP_FIXBLK_NUM_UNUSED_BLKS_2_32
#define RHEAP_FIXBLK_NUM_UNUSED_BLKS_4      RHEAP_FIXBLK_NUM_UNUSED_BLKS_4_32

#ifdef __cplusplus
}
#endif

#endif //  _HEAP32_H

