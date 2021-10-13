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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//
//--------------------------------------------------------------------------;
// Copyright (c) Microsoft Corporation 1994-1995. All Rights Reserved

/*
    mpegdef.h

    This file defines the externals for interfacing with MPEG
    components
*/

/*

    MPEG constants

*/

#define MPEG_TIME_DIVISOR (90000)
#define MPEG_MAX_TIME ((LONGLONG)0x200000000)

#define PICTURE_START_CODE       0x00000100
#define USER_DATA_START_CODE     0x000001B2
#define SEQUENCE_HEADER_CODE     0x000001B3
#define SEQUENCE_ERROR_CODE      0x000001B4
#define EXTENSION_START_CODE     0x000001B5
#define SEQUENCE_END_CODE        0x000001B7
#define SEQUENCE_END_CODE_LOW_BYTE 0xB7
#define GROUP_START_CODE         0x000001B8

#define ISO_11172_END_CODE       0x000001B9
#define PACK_START_CODE          0x000001BA
#define SYSTEM_HEADER_START_CODE 0x000001BB
#define PADDING_START_CODE       0x000001BE
#define PACKET_START_CODE_MIN    0x000001BC
#define PACKET_START_CODE_MAX    0x000001FF

#define AUDIO_GLOBAL             0xB8
#define VIDEO_GLOBAL             0xB9
#define RESERVED_STREAM          0xBC
#define PRIVATE_STREAM_1         0xBD
#define PADDING_STREAM           0xBE
#define PRIVATE_STREAM_2         0xBF
#define AUDIO_STREAM             0xC0
#define AUDIO_STREAM_MASK        0xE0
#define VIDEO_STREAM             0xE0
#define VIDEO_STREAM_MASK        0xF0
#define DATA_STREAM              0xF0
#define DATA_STREAM_MASK         0xF0

/*  MPEG-2 stuff */
#define PROGRAM_STREAM_DIRECTORY 0xFF
#define PROGRAM_STREAM_MAP       0xBC
#define ANCILLIARY_STREAM        0xF9
#define ECM_STREAM               0xF0
#define EMM_STREAM               0xF1

#define VALID_PACKET(data)      (((data) >= PACKET_START_CODE_MIN)  \
                              && ((data) <= PACKET_START_CODE_MAX))

#define VALID_SYSTEM_START_CODE(data)     \
       (VALID_PACKET(data)                \
    ||  (data) == SYSTEM_HEADER_START_CODE\
    ||  (data) == PACK_START_CODE         \
    ||  (data) == ISO_11172_END_CODE)


/*  Types of stream */
inline BOOL IsVideoStreamId(BYTE StreamId)
{
    return (StreamId & 0xF0) == 0xE0;
} ;
inline BOOL IsAudioStreamId(BYTE StreamId)
{
    return (StreamId & 0xE0) == 0xC0;
} ;

#define MAX_MPEG_PACKET_SIZE (65535+6)

/*  Lengths of the various structures */
#define PACK_HEADER_LENGTH 12
#define SYSTEM_HEADER_BASIC_LENGTH 12


#if 0
// obsoleted by ByteSwap() in DVDBitops.H
inline DWORD DWORD_SWAP(DWORD x)
{
    return
     ((DWORD)( ((x) << 24) | ((x) >> 24) |
               (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8)));
}
#endif

/*  Video definitions */

/*  Frame types as defined in a picture header */
#define I_Frame 1
#define D_Frame 4
#define P_Frame 2
#define B_Frame 3
