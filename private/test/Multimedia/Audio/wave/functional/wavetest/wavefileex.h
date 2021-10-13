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
/*++
 
Module Name:
 
	WaveFileEx.h
 
Abstract:
 
	This is the header for implementation of an extension of the WAVE file
    handler library.  This extension provides additional functionality, such as
    adding and manipulating chunks within the WAV file.
 
Notes: This Version is for use with the WaveInterOp Audio CETK
 
--*/
#ifndef _WAVEFILEEX_H
#define _WAVEFILEEX_H


#include "TUXMAIN.H" // LOG()
#include "WaveFile.h"

/* ------------------------------------------------------------------------
	Wave RIFF form Definition: See WaveFile.h.
------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------
	Some optional types used in RIFF.
------------------------------------------------------------------------ */

typedef struct _CUECK_TAG
{
    FOURCC ckID;
    DWORD  ckSize;    // = sizeof( numCuePts )
    DWORD  numCuePts; // This member is included in ckSize 
} CUECK, *LPCUECK;

typedef struct _FACTCK_TAG
{
    FOURCC ckID;
    DWORD  ckSize; // = sizeof( ckData )
    DWORD  ckData; // This member is included in ckSize 
} FACTCK, *LPFACTCK;

typedef struct _LISTCK_TAG
{
    FOURCC ckID;
    DWORD  ckSize; // = sizeof( TypeID ) + sizeof list
    FOURCC TypeID; // This member is included in ckSize 
} LISTCK, *LPLISTCK;

typedef struct _PLSTCK_TAG
{
    FOURCC ckID;
    DWORD  ckSize; // = 4 + 12 * numSegs
    DWORD  numSegs;
} PLSTCK, *LPPLSTCK;

void LogWaveFormat( PCMWAVEFORMAT PcmWaveFormat );

/* ------------------------------------------------------------------------
	Four character codes
------------------------------------------------------------------------ */
#define adlt_FOURCC MAKEFOURCC( 'a', 'd', 'l', 't' )
#define  cue_FOURCC MAKEFOURCC( 'c', 'u', 'e', ' ' )
#define plst_FOURCC MAKEFOURCC( 'p', 'l', 's', 't' )

class CWaveFileEx : public CWaveFile
{
public:    
    CWaveFileEx();
    CWaveFileEx(
        LPCTSTR lpFileName,	            // pointer to name of the file 
        DWORD   dwDesiredAccess,	    // access (read-write) mode 
        DWORD   dwCreationDistribution,	// how to create 
        DWORD   dwFlagsAndAttributes,	// file attributes 
        LPVOID* lplpWaveFormat,         // Wave Format data
        LPVOID* lplpOptChunk            // Pointer to a pointer to a structure
                                        // containing optional chunk
                                        // information. The second word must
                                        // contain the size - 8 (in bytes) of
                                        // the chunk.
    ); 
    
    ~CWaveFileEx();
    int  AddRandomChunk();
    BOOL WriteCueChunk();
    BOOL WriteFactChunk();
    BOOL WritePlstChunk();
protected:
    DWORD dwNoOfCueChunks;
    DWORD dwNoOfFactChunks;
    DWORD dwNoOfPlstChunks;
};
#endif _WAVEFILEEX_H

