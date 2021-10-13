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
//
// additional media type definitions for DVD
//

#ifndef __DVDTYPES_H__
#define __DVDTYPES_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// extended VIDEOINFO structure
// that includes only the data found in the VTS_V_ATR structure on the disk, without
// parsing a specific sequence header

typedef struct tagDVDVIDEOINFO {

    VIDEOINFOHEADER hdr;                    // Compatible with VIDEOINFO
    LONG		    lVBV;	    // calculated based on mpeg1 vs mpeg2 mode
    BOOL		    bTV_525_60;	    // TRUE if 525/60, else 625/50
} DVDVIDEOINFO;


#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __DVDTYPES_H_


// -- must be outside multiple-include protection ---

// FORMAT guid that defines the untyped format block in a media type as being a
// DVDVIDEOINFO
// {35980200-8FC9-11d0-A5C9-0060B0543D09}
DEFINE_GUID(FORMAT_DVDVIDEOINFO, 
0x35980200, 0x8fc9, 0x11d0, 0xa5, 0xc9, 0x0, 0x60, 0xb0, 0x54, 0x3d, 0x9);

// MajorType GUID for DVD Pack format (2048 byte blocks)
// {27CB4360-23AA-11d1-9503-00A0C967A239}
DEFINE_GUID(MAJORTYPE_MM_DVD_PACK, 
0x27cb4360, 0x23aa, 0x11d1, 0x95, 0x3, 0x0, 0xa0, 0xc9, 0x67, 0xa2, 0x39);


// MEDIASUBTYPE_ByteSwappedPCM {C79DF001-ED74-11d0-B855-006097917460}
DEFINE_GUID(MEDIASUBTYPE_ByteSwappedPCM, 
0xc79df001, 0xed74, 0x11d0, 0xb8, 0x55, 0x0, 0x60, 0x97, 0x91, 0x74, 0x60);


//
// Splitter 'MPEG-2' Types.
//
// Until MS define (in AM20?) MPEG-2 types, we will use our own. These
// use the MPEG1 format bocks, etc, just changing the subtype guid.

// MMSubTypeMPEG2Packet
// {AD5E5D10-CBD7-11d0-8586-00A024D181B8}
DEFINE_GUID(MMSubTypeMPEG2Packet, 
0xad5e5d10, 0xcbd7, 0x11d0, 0x85, 0x86, 0x0, 0xa0, 0x24, 0xd1, 0x81, 0xb8);

// MMSubTypeMPEG2Payload
// {AD5E5D11-CBD7-11d0-8586-00A024D181B8}
DEFINE_GUID(MMSubTypeMPEG2Payload, 
0xad5e5d11, 0xcbd7, 0x11d0, 0x85, 0x86, 0x0, 0xa0, 0x24, 0xd1, 0x81, 0xb8);


//
// Decoder Types
//

//
// MEDIASUBTYPE_MMFatPCM
//
// 'FAT' Wave format, so that 3D filter gets a chance to downmix.
// In all respects identical to MEDIASUBTYPE_PCMAudio, except
// its FormatGIUD (it uses FormatType_MMFatPCM)
DEFINE_GUID(MEDIASUBTYPE_MMFatPCM, 
0x5c376810, 0xdb7b, 0x11d0, 0xbe, 0xa7, 0x0, 0xa0, 0x24, 0xd1, 0x81, 0xb8);


//
// FormatType_MMFatPCM
//
// For use with MEDIASUBTYPE_MMFatPCM to make the format block distinctive.
// actually this conatins a WAVEFORMATEX, just like FormatWaveFormatEx.
// {2505ACE0-E2AB-11d0-BEC2-00A024D181B8}
DEFINE_GUID(FormatType_MMFatPCM, 
0x2505ace0, 0xe2ab, 0x11d0, 0xbe, 0xc2, 0x0, 0xa0, 0x24, 0xd1, 0x81, 0xb8);
