/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 1998 Microsoft Corporation

Module Name:

	nkcompr.h

Abstract:

	Header defining the structures used in nkcompr.c and nkdecompr.c



	Brian Wahlert (t-brianw) 8-Jul-1998

Environment:

	Non-specific

Revision History:

	8-Jul-1998 Brian Wahlert (t-brianw)

-------------------------------------------------------------------*/

#include "cecompr.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef UNDER_CE
//#define ASSERT(x) if (!(x)) printf("ASSERT!\n")
#define ASSERT(x) do { if(!(x)) DebugBreak(); } while (0)
#endif

#define TOKEN_DATA          0
#define TOKEN_COPY          1
#define TOKEN_END           3

#define MAX_OUTPUT_LEN (6 * 1024 * 1024)
#define NO_COMPRESS 0xffffffff
#define NUM_COMPRESSED          10
#define NUM_COMPRESSED_SCN      1
#define IMAGE_SCN_COMPRESSED 0x00002000
#define FILE_ATTRIBUTE_COMPRESSED 0x00000800
#define NUM_SECTIONS 1
#define ONE_CHAR 0xffffffff
#define ALL_DATA 0
#define SEC_DIV_CHAR 'q'
#define NUM_MATCHED 4096

#ifndef CECOMPRESS_FAILED
#define CECOMPRESS_FAILED       0xffffffff
#endif

#ifndef CEDECOMPRESS_FAILED
#define CEDECOMPRESS_FAILED     0xffffffff
#endif

#define BUF_SIZE 4096
#define MAX_SEC_BYTES_COMPR (2 * 1024 * 1024)
#define MAX_SEC_BYTES_UNCOMPR (3 * 1024 * 1024)
#define COMPRESSED_INDICATOR 'C'
#define ROM_IMAGE_SIGNATURE {'B','0','0','0','F','F','\n'}

//////////////////////////////////////////////////
// My typedefs
//////////////////////////////////////////////////

typedef short INT16;
//typedef long INT32;
typedef unsigned short UINT16;
//typedef unsigned long UINT32;
typedef unsigned char UCHAR;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef unsigned char* LPBYTE;
//typedef unsigned char BOOL;
typedef unsigned short WORD;
typedef unsigned long DWORD;

#define FALSE 0
#define TRUE 1

//////////////////////////////////////////////////
// NK Image structures
//////////////////////////////////////////////////

// NK ROM image header structure
#pragma pack(1)
typedef struct _ROMIMAGE_HEADER {

    UCHAR Signature[7];
    ULONG PhysicalStartAddress;
    ULONG PhysicalSize;

} ROMIMAGE_HEADER, *PROMIMAGE_HEADER;

// ROM image section header structure
typedef struct _ROMIMAGE_SECTION {
   
    ULONG Address;

    union {
        ULONG Size;
        ULONG EntryPoint;
    };

    ULONG CheckSum;

} ROMIMAGE_SECTION, *PROMIMAGE_SECTION;

#pragma pack()

////////////////////////////////////////////////////////////
// Structure defining a compressed region
////////////////////////////////////////////////////////////

typedef struct _COMPR_RGN
{
	UINT32 iAddress;
	UINT32 cBytesCompressed;
	UINT32 cBytesUncompressed;
} COMPR_RGN;

////////////////////////////////////////////////////////////
// Structure defining the command to compress a region.  If
// "cBytesCompressed" and "cBytesUncompressed" are equal,
// then the region does not need to be compressed
////////////////////////////////////////////////////////////

typedef struct _COMPR_CMD
{
	UINT32 cBytesCompressed;
	UINT32 cBytesUncompressed;
} COMPR_CMD;

typedef struct _ADDRESS
{
	UINT32 iAddr;
	UINT32 iOffset;
} ADDRESS;

