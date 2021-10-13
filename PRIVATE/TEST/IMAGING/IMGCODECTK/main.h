//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include <kato.h>
#include <tux.h>
#include <imaging.h>
#include <string>
#include <clparse.h>

// universal logging
enum infoType {
   FAIL,
   ECHO,
   DETAIL,
   ABORT,
   SKIP,
   INDENT = 0x80000000,
   UNINDENT = 0x40000000,
   RESETINDENT = 0x20000000
};

enum AllocType {
    atGLOBAL = 0,
    atCTM = 1,
    atFM = 2
};

// A lot of the helper functions call info(DETAIL, ...), which by default uses
// Kato. However, the stress test framework uses a different system.
// The stress test (and other tests can if needed) sets g_pfnLogger to its own
// PFNLoggerFunction, which uses LogComment, LogVerbose, LogFail, etc.
typedef UINT (*PFNLoggerFunction)(int iFailCode, const TCHAR * tszMessage);
typedef std::basic_string<TCHAR> tstring;

#define countof(x) (sizeof(x)/sizeof(*x))
#define PTRPRESET ((void*)0xAAAAAAAA)

// TODO: Convert all calls of SAFERELEASE to calls to SafeRelease
#define SAFERELEASE(x) SafeRelease(x)

// Through the joys of concatenation, we can easily print the function name
#define _FT(x) TEXT(__FUNCTION__) TEXT(": ") TEXT(x)
#define _LOCATIONT TEXT("File: (") TEXT(__FILE__) TEXT("); Line: %d"), __LINE__

// The following two macros exist specifically to allow easy output of GUIDs to the
// debug stream. They would be used as such:
// info(xxx, TEXT("guid") _GUIDT, _GUIDEXP(pguid));

#define _GUIDT TEXT("{%08x, %04x, %04x, %02x%02x%02x%02x%02x%02x%02x%02x}")
#define _GUIDEXP(x) \
    (x).Data1, \
    (x).Data2, \
    (x).Data3, \
    (x).Data4[0], \
    (x).Data4[1], \
    (x).Data4[2], \
    (x).Data4[3], \
    (x).Data4[4], \
    (x).Data4[5], \
    (x).Data4[6], \
    (x).Data4[7]


/***********************************************************************************
***
***   From main.cpp
***
************************************************************************************/
#define NO_MESSAGES    {if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;}
extern void info(int, LPCTSTR,...);

// getCode will only work if the g_pfnLogger function isn't overridden.
extern TESTPROCAPI getCode(void);

// UseCommandLineParameters is called by both the shellproc in tux tests and the
// InitializeStressTest in the stress tests.
extern bool UseCommandLineParameters(CClParse * pclpParameters);
extern void DisplayUsage();


extern HRESULT InternalVerifyHRSuccess(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, HRESULT hr);
#define VerifyHRSuccess(x) InternalVerifyHRSuccess(TEXT(__FILE__), __LINE__, TEXT(#x), x)

extern HRESULT InternalVerifyHRFailure(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, HRESULT hr);
#define VerifyHRFailure(x) InternalVerifyHRFailure(TEXT(__FILE__), __LINE__, TEXT(#x), x)

extern BOOL InternalVerifyConditionTrue(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, BOOL fBool);
#define VerifyConditionTrue(x) InternalVerifyConditionTrue(TEXT(__FILE__), __LINE__, TEXT(#x), x)

extern BOOL InternalVerifyConditionFalse(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, BOOL fBool);
#define VerifyConditionFalse(x) InternalVerifyConditionFalse(TEXT(__FILE__), __LINE__, TEXT(#x), x)

HRESULT CreateStreamOnFile(const TCHAR * tszFilename, IStream ** ppStream);

template<class _T>
inline void PresetPtr(_T & ptr)
{
    ptr = (_T) PTRPRESET;
}

template<class _T>
inline bool IsPreset(_T ptr)
{
    return (NULL == ptr) || (PTRPRESET == ptr);
}

template<class _T>
inline int SafeRelease(_T & ptr)
{
    int iRet = (int)0x80000000;
    if ((ptr) && PTRPRESET != (ptr)) 
    { 
        iRet = (ptr)->Release(); 
        (ptr) = NULL;
    }
    return iRet;
}

extern PFNLoggerFunction g_pfnLogger;

extern FUNCTION_TABLE_ENTRY g_lpFTE[];

extern HINSTANCE g_hInst;

#define BEGINNAMEVALMAP(x) NameValuePair x[] = {
#define NAMEVALENTRY(x)    { (LONG)(x), TEXT(#x) },
#define ENDNAMEVALMAP()    {0,0}};

struct NameValuePair {
    LONG pValue;
    TCHAR *szName;
};


// CMapClass
//   This is a class used to map 32-bit values to strings. This implementation
//   allows me to look up either the name based on the value or the value 
//   based on the name (which is good for parsing configuration files).
class CMapClass
{
public:
    CMapClass() {}
    CMapClass(struct NameValuePair *nvp):m_funcEntryStruct(nvp) {}
    ~CMapClass() {}
    
    TCHAR * operator [](const GUID index);
    TCHAR * operator[](const LONG index);
    LONG operator[](const TCHAR * index);
private:
    struct NameValuePair *m_funcEntryStruct;
};

extern CMapClass g_mapIMGFMT;
extern CMapClass g_mapDECODER;
extern CMapClass g_mapPIXFMT;
extern CMapClass g_mapINTERP;
extern CMapClass g_mapDECODERINIT;
extern CMapClass g_mapAllocType;
extern CMapClass g_mapFRAMEDIM;
extern CMapClass g_mapTAG;
extern CMapClass g_mapTAG_TYPE;

// Command line parameter variables
extern CClParse * g_pclpParameters;

// source and dest default to \release. Can be overridden with /Source and /Dest
extern tstring g_tsImageSource;
extern tstring g_tsImageDest;

// The name and port of the ImageVerifier server (if any). Port default is 4321
// and there is no server default.
extern tstring g_tsCompServ;
extern UINT g_uiCompPort;

extern tstring g_tsCodecType;

extern BOOL g_fBadParams;
extern BOOL g_fBuiltinFirst;
extern BOOL g_fNoBuiltin;
extern BOOL g_fNoUser;


extern const GUID * g_rgImgFmt[];
extern const int g_cImgFmt;

extern const GUID * g_rgDecoder[];
extern const int g_cDecoder;

extern const PixelFormat g_rgPixelFormats[];
extern const int g_cPixelFormats;

extern const InterpolationHint g_rgInterpHints[];
extern const int g_cInterpHints;

extern const UINT g_rgPropertyTags[];
extern const int g_cPropertyTags;

extern const UINT g_rgrgInvalidSizes[][2];
extern const int g_cInvalidSizes;

extern const UINT g_rgrgValidSizes[][2];
extern const int g_cValidSizes;
