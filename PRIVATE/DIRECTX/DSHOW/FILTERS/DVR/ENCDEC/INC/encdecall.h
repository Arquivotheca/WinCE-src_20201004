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
/*++


    Module Name:

        inc/EncDecAll.h

    Abstract:

        This module is the main header for all ts/dvr

    Revision History:

        01-Feb-2001     created

--*/

#ifndef __EncDec__EncDecAll_h
#define __EncDec__EncDecAll_h

// ATl

#define _ATL_APARTMENT_THREADED
#define _ATL_STATIC_REGISTRY

#include <memory>

#include <strmif.h>
#include <streams.h>        // CBaseOutputPin, CBasePin, CCritSec, ...

#include <atlbase.h>		// CComQIPtr

//  dshow

#include <dvdmedia.h>       //  MPEG2VIDEOINFO

#include "EncDecTrace.h"	// tracing macros

#ifndef UNDER_CE
#define	DBG_NEW
#endif // !UNDER_CE
#ifdef	DBG_NEW
	#include "crtdbg.h"
   #define DEBUG_NEW	new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
   #define DEBUG_NEW	new
#endif // _DEBUG

#ifdef INITGUID_FOR_ENCDEC
#include <initguid.h>         // do this after all the above includes... else lots of redefinitions
#endif
#include "EncDec.h"         // Compiled from IDL file.  Holds PackedTvRating definition
#include "PackTvRat.h"
#include "TimeIt.h"

    // passed between Encrypter and Decrypter filters (and stored in the file)
typedef enum
{
    Encrypt_None        = 0,
    Encrypt_XOR_Even    = 1,
    Encrypt_XOR_Odd     = 2,
    Encrypt_XOR_DogFood = 3,
    Encrypt_DRMv1       = 4,
    Encrypt_PDRM        = 5
} Encryption_Method;

#if defined(USE_PDRM)
    #define Encrypt_DRM Encrypt_PDRM
#else
    #define Encrypt_DRM Encrypt_DRMv1
#endif

    // also passed between Encrypter and Decrypter filters (and stored in the file)
typedef struct
{
    PackedTvRating          m_PackedRating;     // the actual rating
    long                    m_cPacketSeqID;     // N'th rating we got (incremented by new rating)
    long                    m_cCallSeqID;       // which version of the rating (incrmented by encrypter)
    long                    m_dwFlags;          // Flags (is Fresh, ...)  If not defined, bits should be zero
} StoredTvRating;

const int StoredTVRat_Fresh  = 0x1;             // set of rating when its an update of a duplicate one

const int   kStoredTvRating_Version = 1001;      // version number (major minor)

const int kMinPacketSizeForDRMEncrypt = 17;      // avoid encrypting really short packets (if 2, don't do CC packets)

        // 100 nano-second units to useful sizes.
const   int kMicroSecsToUnits   = 10;
const   int kMilliSecsToUnits   = 10000;
const   int kSecsToUnits        = 10000000;

extern TCHAR * EventIDToString(const GUID &pGuid);      // in DTFilter.cpp


//----------------- XDS States -------
//  TODO - move these into their own file eventually...

enum XDS_Classes
{
    XDS_Cls_Invalid                 = 0x0,
    XDS_Cls_Current_Start           = 0x1,
    XDS_Cls_Current_Continue        = 0x2,
    XDS_Cls_Future_Start            = 0x3,
    XDS_Cls_Future_Continue         = 0x4,
    XDS_Cls_Channel_Start           = 0x5,
    XDS_Cls_Channel_Continue        = 0x6,
    XDS_Cls_Miscellaneous_Start     = 0x7,
    XDS_Cls_Miscellaneous_Continue  = 0x8,
    XDS_Cls_PublicService_Start     = 0x9,
    XDS_Cls_PublicService_Continue  = 0xA,
    XDS_Cls_Reserved_Start          = 0xB,
    XDS_Cls_Reserved_Continue       = 0xC,
    XDS_Cls_PrivateData_Start       = 0xD,
    XDS_Cls_PrivateData_Continue    = 0xE,
    XDS_Cls_End                     = 0xF
};

const DWORD XDS_Type_Invalid            = 0x0;

    // types as ints, since different classes use the same flags
const DWORD XDS_1Type_PID               = 0x1;      // program identification number
const DWORD XDS_1Type_Length            = 0x2;      // length, time in show
const DWORD XDS_1Type_ProgramName       = 0x3;      // title
const DWORD XDS_1Type_ProgramType       = 0x4;      // program type
const DWORD XDS_1Type_ContentAdvisory   = 0x5;      // Content Advisory
const DWORD XDS_1Type_AudioServices     = 0x6;
const DWORD XDS_1Type_CaptionServices   = 0x7;
const DWORD XDS_1Type_CGMS_A            = 0x8;      // Copy Generation Management Services
const DWORD XDS_1Type_AspectRatio       = 0x9;      // Aspect Ration Information
const DWORD XDS_1Type_CompositePacket1  = 0xC;
const DWORD XDS_1Type_CompositePacket2  = 0xD;
const DWORD XDS_1Type_ProgDescription1  = 0x10;
const DWORD XDS_1Type_ProgDescription2  = 0x11;
const DWORD XDS_1Type_ProgDescription3  = 0x12;
const DWORD XDS_1Type_ProgDescription4  = 0x13;
const DWORD XDS_1Type_ProgDescription5  = 0x14;
const DWORD XDS_1Type_ProgDescription6  = 0x15;
const DWORD XDS_1Type_ProgDescription7  = 0x16;
const DWORD XDS_1Type_ProgDescription8  = 0x17;


const DWORD XDS_CMGS_A_B1               = (1<<4);
const DWORD XDS_CMGS_A_B2               = (1<<3);

#endif  //  __EncDec__EncDecAll_h
