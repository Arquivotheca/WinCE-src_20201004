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
// Subband Codec - stream and operations on stream
// 

#ifndef _STREAM_HXX_
#define _STREAM_HXX_

#include <msacm.h>
#include <msacmdrv.h>
#include <sbc.hxx>

#include "subbands.hxx"

#ifndef FNLOCAL
    #define FNLOCAL     _stdcall
    #define FNCLOCAL    _stdcall
    #define FNGLOBAL    _stdcall
    #define FNCGLOBAL   _stdcall
    #define FNWCALLBACK CALLBACK
    #define FNEXPORT    CALLBACK
#endif

typedef LRESULT (FNGLOBAL *STREAMCONVERTPROC)
(
    LPACMDRVSTREAMINSTANCE  padsi,
    LPACMDRVSTREAMHEADER    padsh
);

// stream instance data type
typedef struct tSTREAMINSTANCE
{
    STREAMCONVERTPROC   fnConvert;  // stream instance conversion proc
    DWORD               fdwConfig;  // stream instance configuration flags

    //
    //  This SBC codec requires the following parameters
    //  per stream instance.  These parameters are used by
    //  the encode and decode routines.

    SBCWAVEFORMAT format;  // format fields 
    union {
    	Analyzer analyzer[SBCCHANNELS];
#ifdef DECODE_ON

	Synthesizer synthesizer[SBCCHANNELS];
#endif
    }; // analyzers and synthesizers
} STREAMINSTANCE, *PSTREAMINSTANCE, FAR *LPSTREAMINSTANCE;

// decoding and encoding 
LRESULT sbcDecode(LPACMDRVSTREAMINSTANCE padsi, LPACMDRVSTREAMHEADER padsh);
LRESULT sbcEncode(LPACMDRVSTREAMINSTANCE padsi, LPACMDRVSTREAMHEADER padsh);

// reseting stream instance
void sbcDecodeReset(LPSTREAMINSTANCE psi, const SBCWAVEFORMAT *format = NULL);
void sbcEncodeReset(LPSTREAMINSTANCE psi, SBCWAVEFORMAT *format = NULL);

#endif

