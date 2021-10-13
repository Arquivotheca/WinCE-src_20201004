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


//=============================================================================
//  BEGIN fragment of netmon.h
//=============================================================================

#ifndef MAKE_IDENTIFIER
#define MAKE_IDENTIFIER(a, b, c, d)     ((DWORD) MAKELONG(MAKEWORD(a, b), MAKEWORD(c, d)))
#endif // MAKE_IDENTIFIER

//=============================================================================
//  Frame type.
//=============================================================================
//  TimeStamp is in 1/1,000,000th seconds.
typedef struct _FRAME
    {
    __int64 TimeStamp;
    DWORD FrameLength;
    DWORD nBytesAvail;
    /* [size_is] */ BYTE MacFrame[ 1 ];
    } 	FRAME;

typedef FRAME *LPFRAME;

typedef FRAME UNALIGNED *ULPFRAME;
#define	FRAME_SIZE	( sizeof( FRAME  ) )

// This structure is used to decode file data and so needs to be packed

#pragma pack(push, 1)
#define	CAPTUREFILE_VERSION_MAJOR	( 2 )

#define	CAPTUREFILE_VERSION_MINOR	( 0 )

#define MakeVersion(Major, Minor)   ((DWORD) MAKEWORD(Minor, Major))
#define GetCurrentVersion()         MakeVersion(CAPTUREFILE_VERSION_MAJOR, CAPTUREFILE_VERSION_MINOR)
#define NETMON_1_0_CAPTUREFILE_SIGNATURE     MAKE_IDENTIFIER('R', 'T', 'S', 'S')
#define NETMON_2_0_CAPTUREFILE_SIGNATURE     MAKE_IDENTIFIER('G', 'M', 'B', 'U')
typedef struct _CAPTUREFILE_HEADER_VALUES
    {
    DWORD Signature;
    BYTE BCDVerMinor;
    BYTE BCDVerMajor;
    WORD MacType;
    SYSTEMTIME TimeStamp;
    DWORD FrameTableOffset;
    DWORD FrameTableLength;
    DWORD UserDataOffset;
    DWORD UserDataLength;
    DWORD CommentDataOffset;
    DWORD CommentDataLength;
    DWORD StatisticsOffset;
    DWORD StatisticsLength;
    DWORD NetworkInfoOffset;
    DWORD NetworkInfoLength;
    DWORD ConversationStatsOffset;
    DWORD ConversationStatsLength;
    } 	CAPTUREFILE_HEADER_VALUES;

typedef CAPTUREFILE_HEADER_VALUES *LPCAPTUREFILE_HEADER_VALUES;

#define	CAPTUREFILE_HEADER_VALUES_SIZE	( sizeof( CAPTUREFILE_HEADER_VALUES  ) )


#pragma pack(pop)

// This structure is used to decode file data and so needs to be packed

#pragma pack(push, 1)
typedef struct _CAPTUREFILE_HEADER
    {
    union 
        {
        CAPTUREFILE_HEADER_VALUES ActualHeader;
        BYTE Buffer[ 72 ];
        } 	;
    BYTE Reserved[ 56 ];
    } 	CAPTUREFILE_HEADER;

typedef CAPTUREFILE_HEADER *LPCAPTUREFILE_HEADER;

#define	CAPTUREFILE_HEADER_SIZE	( sizeof( CAPTUREFILE_HEADER  ) )

#pragma pack(pop)

//=============================================================================
//  END fragment of Netmon.h
//=============================================================================