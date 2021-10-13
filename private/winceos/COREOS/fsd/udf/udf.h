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

#pragma once

#define WINCEOEM 1

#include <windows.h>
#include <extfile.h>
#include <fsdmgr.h>
#include <cdioctl.h>
#include <dvdioctl.h>
#include <udfdbg.h>
#include <dbt.h>
#include <storemgr.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <ntddmmc.h>

#include <types.h>
#include <fsioctl.h>
#include <intsafe.h>
#include "osta.h"

#pragma warning( disable : 4200 )
#include <iso13346.h>
#pragma warning( default : 4200 )

#define FS_MOUNT_POINT_NAME L"CD-ROM"
#define FS_MOUNT_POINT_NAME_LENGTH 6

#define DEFAULT_ALLOC_SEGMENTS_PER_CHUNK 10
#define DEFAULT_INITIAL_HEAP_SIZE        20 * 1024
#define DEFAULT_MAX_HEAP_SIZE            0
#define DEFAULT_HASH_SIZE                47
#define DEFAULT_MAX_SPARING_SIZE         64 * 1024

typedef ULONGLONG Uint64;
typedef ULONG  Uint32;
typedef USHORT Uint16;
typedef UCHAR  Uint8;

extern CRITICAL_SECTION g_csMain;
extern HANDLE g_hHeap;
extern const Uint16 INVALID_REFERENCE;

//
//  This is the version of UDF that we recognize, per the Domain Identifier
//  specification in UDF 2.1.5.3.
//
//  The values below indicate we understand UDF 2.01.  We will also define
//  specific revisions so that we can assert correctness for some structures
//  that we know appeared for the first time in certain specifications.
//
//

#define UDF_VERSION_100         0x0100
#define UDF_VERSION_101         0x0101
#define UDF_VERSION_102         0x0102
#define UDF_VERSION_150         0x0150
#define UDF_VERSION_200         0x0200
#define UDF_VERSION_201         0x0201
#define UDF_VERSION_250         0x0250

#define UDF_VERSION_RECOGNIZED  UDF_VERSION_250

#define UDF_VERSION_MINIMUM     UDF_VERSION_100

typedef struct _CD_HEADER
{
    BYTE MSF[3];
    BYTE Mode : 2;
    BYTE Reserved : 3;
    BYTE BlockType : 3;
} CD_HEADER, *PCD_HEADER;

typedef struct _PASS_THROUGH_DIRECT
{
    SCSI_PASS_THROUGH_DIRECT PassThrough;
    SENSE_DATA SenseData;
} PASS_THROUGH_DIRECT, *PPASS_THROUGH_DIRECT;

//
//  This macro copies an src longword to a dst longword,
//  performing an little/big endian swap.
//
#define SwapCopyUchar4(Dst,Src) {                                        \
    *((UCHAR *)(Dst)) = *((UCHAR *)(Src) + 3);     \
    *((UCHAR *)(Dst) + 1) = *((UCHAR *)(Src) + 2); \
    *((UCHAR *)(Dst) + 2) = *((UCHAR *)(Src) + 1); \
    *((UCHAR *)(Dst) + 3) = *((UCHAR *)(Src));     \
}

#define CD_SECTOR_SIZE             2048
#define CD_RAW_SECTOR_SIZE         2352
#define CDRW_PACKET_LENGTH         32
#define DVDRW_PACKET_LENGTH        16
#define CDRW_RUNOUT_LENGTH         7
#define CDRW_MIN_TRACK_LENGTH      (4*75) // 4 seconds

#define UdfPacketSizeForMedia( X)  (((X) & MED_FLAG_DVD) ? DVDRW_PACKET_LENGTH : CDRW_PACKET_LENGTH)

#define UDF_VAT_UNUSED_ENTRY 0xffffffff

//  ICBTAG_FILE_T_... - Values for icbtag_FileType

#define ICBTAG_FILE_T_VAT           248     // VAT (new format - UDF 2.00 and later)
#define ICBTAG_FILE_T_REALTIME      249     // Real Time File (UDF 2.01, 2.3.5.2.1)
#define UDF_CDUDF_TRAILING_DATA_SIZE    (sizeof(REGID) + sizeof(ULONG))

//
//  A UDF 1.50 VAT minimally contains a mapping for a single block, the REGID identifying
//  the VAT, and the identification of a previous VAT ICB location.
//
//  A UDF 2.0x VAT minimally contains a VAT header,  and a mapping for a single block.
//
//  We also identify
//  an arbitrary sanity limit that the VAT isn't bigger than 8mb since it is extremely
//  difficult to imagine such a VAT existing in practice since each sector describes
//  (on most of our media) 2048/4 = 512 entries ... meaning at 8mb the VAT would
//  describe ~2^21 blocks.  4/12/01 Increased for DVD-R.
//

#define UDF_CDUDF_MINIMUM_150_VAT_SIZE  (sizeof(ULONG) + UDF_CDUDF_TRAILING_DATA_SIZE)
#define UDF_CDUDF_MINIMUM_20x_VAT_SIZE  (sizeof(ULONG) + sizeof( VAT_HEADER))

#define UDF_CDUDF_MAXIMUM_VAT_SIZE      (16 * 1024 * 1024)

//
//  Fixed values for original sectors, indicating that either the
//  mapped packet is avaliable for sparing use or is defective.
//

#define UDF_SPARING_AVALIABLE           0xffffffff
#define UDF_SPARING_DEFECTIVE           0xfffffff0

//
//  The unit of media in each sparing packet is fixed at 32 physical sectors.
//

#define UDF_SPARING_PACKET_LENGTH_CDRW     CDRW_PACKET_LENGTH
#define UDF_SPARING_PACKET_LENGTH_DVDRW    16

const DWORD MAX_READ_SIZE = 16 * 1024;

Uint8 GetChecksum( const DESTAG& Tag );
BOOL VerifyDescriptor( Uint32 dwSector, BYTE* pBuffer, Uint32 dwBufferSize );

#include "UdfTag.h"
#include "UdfMisc.h"
#include "UdfMedia.h"
#include "UdfFile.h"
#include "UdfVolume.h"
#include "UdfPartition.h"

//
// Overriding the new operator to use the volume heap and to add a pool-type tag
// to each allocation.
//
void *operator new(size_t stAllocateBlock, POOL_TYPE Tag);
void *operator new[](size_t stAllocateBlock, POOL_TYPE Tag);

