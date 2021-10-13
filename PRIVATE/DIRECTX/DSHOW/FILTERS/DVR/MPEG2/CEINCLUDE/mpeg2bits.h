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
/////////////////////////////////////////////////////////////////////////////
//
//
// Module Name:
//
//      Mpeg2Bits.h
//
// Abstract:
//
//      This file defines the MPEG-2 section header bitfields. These are
//      defined here instead of in mpegstructs.idl because of MIDL
//      compiler conflicts with bitfield definitions.
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#pragma pack(push)
#pragma pack(1)


//
// PID structure
//

#ifdef __midl

typedef struct
{
    WORD Bits;
} PID_BITS_MIDL;

#else

typedef struct
{
    WORD Reserved  :  3;
    WORD ProgramId : 13;
} PID_BITS, *PPID_BITS;

#endif



//
// Generic MPEG packet header structure
//

#ifdef __midl

typedef struct
{
    WORD Bits;
} MPEG_HEADER_BITS_MIDL;

#else

typedef struct
{
    WORD SectionLength          : 12;
    WORD Reserved               :  2;
    WORD PrivateIndicator       :  1;
    WORD SectionSyntaxIndicator :  1;
} MPEG_HEADER_BITS, *PMPEG_HEADER_BITS;

#endif



//
// Long MPEG packet header structure
//

#ifdef __midl

typedef struct
{
    BYTE Bits;
} MPEG_HEADER_VERSION_BITS_MIDL;

#else

typedef struct
{
    BYTE CurrentNextIndicator : 1;
    BYTE VersionNumber        : 5;
    BYTE Reserved             : 2;
} MPEG_HEADER_VERSION_BITS, *PMPEG_HEADER_VERSION_BITS;

#endif



#pragma pack(pop)
