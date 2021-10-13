////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//      09/12/96    lblanco     Created for the TUX DLL Wizard.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

////////////////////////////////////////////////////////////////////////////////
// Test selection

#ifdef KATANA

#define TEST_APIS       1
#define TEST_IDD        1
#define TEST_IDD2       1
#define TEST_IDD4       1
#define TEST_IDDP       1
#define TEST_IDDS       1
#define TEST_IDDS2      1
#define TEST_IDDS3      1
#define TEST_IDDS4      1
#define TEST_IDDS5      0
#define TEST_IDDCC      0
#define TEST_IDDC       0
#define TEST_BLT        1
#define TEST_ZBUFFER    1

#else // KATANA

#define TEST_APIS       1
#define TEST_IDD        1
#define TEST_IDD2       1
#define TEST_IDD4       1
#define TEST_IDDP       1
#define TEST_IDDS       1
#define TEST_IDDS2      1
#define TEST_IDDS3      1
#define TEST_IDDS4      1
#define TEST_IDDS5      1
#define TEST_IDDCC      1
#define TEST_IDDC       1
#define TEST_BLT        1
#define TEST_ZBUFFER    0

#endif // KATANA

#define NEED_INITDDS    (TEST_IDDS || TEST_IDDS2 || TEST_IDDS3 || \
                         TEST_IDDCC || TEST_IDDP || TEST_BLT)
#define NEED_INITDD     (TEST_IDD || TEST_IDD2 || NEED_INITDDS)
#define NEED_CREATEDDP  (TEST_IDDP || TEST_IDDS)

////////////////////////////////////////////////////////////////////////////////
// Test configuration

#define SUPPORT_IDDCC_METHODS   0

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifndef __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#else // __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#endif // __GLOBALS_CPP__

////////////////////////////////////////////////////////////////////////////////
// Global macros and constants

#ifdef KATANA
#define IFKATANA(a) (a)
#define IFKATANAELSE(a, b) (a)
#else
#define IFKATANA(a)
#define IFKATANAELSE(a, b) (b)
#endif

#define countof(x)      (sizeof(x)/sizeof(*(x)))

#define DWORDREADBYTE(addr)     ((BYTE)((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC))>>(8*(((DWORD)(addr))&0x3)))&0xFF)
#define DWORDWRITEBYTE(addr, x) ((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC)) = (((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC))&~(0xFF<<(8*(((DWORD)(addr))&0x3))))|(((x)&0xFF)<<(8*(((DWORD)(addr))&0x3)))))
#define DWORDREADWORD(addr)     ((WORD)((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC))>>(8*(((DWORD)(addr))&0x2)))&0xFFFF)
#define DWORDWRITEWORD(addr, x) ((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC)) = (((*(DWORD *)(((DWORD)(addr))&0xFFFFFFFC))&~(0xFFFF<<(8*(((DWORD)(addr))&0x2))))|(((x)&0xFFFF)<<(8*(((DWORD)(addr))&0x2)))))

// Invalid calls. In debug builds, we expect DebugBreaks. In retail builds we
// don't, but we still skip them
//#ifdef DEBUG

// Debug mode calls don't hit debug breaks, so stop failing them. Grr.
#if 0
#define INVALID_CALL_E_RAID(call, iface, mname, text, expected, db, num) \
    do \
    { \
        __try \
        { \
            COUNT_DEBUGBREAKS(hr = call); \
            if(hr != expected) \
            { \
                Report( \
                    RESULT_UNEXPECTED, \
                    iface, \
                    mname, \
                    hr, \
                    text, \
                    NULL, \
                    expected, \
                    db, \
                    num); \
                nITPR |= ITPR_FAIL; \
            } \
            if(FAILED(hr)) \
            { \
                if(GetDebugBreakCount() != 1) \
                { \
                    Report( \
                        RESULT_DEBUGBREAK, \
                        iface, \
                        mname, \
                        0, \
                        text, \
                        NULL, \
                        1, \
                        db, \
                        num); \
                    nITPR |= ITPR_FAIL; \
                } \
                else \
                { \
                    Debug(LOG_COMMENT, TEXT("DebugBreak hit as expected")); \
                } \
            } \
            else \
            { \
                if(GetDebugBreakCount() != 0) \
                { \
                    Report( \
                        RESULT_DEBUGBREAK, \
                        iface, \
                        mname, \
                        0, \
                        text, \
                        TEXT("on success"), \
                        0, \
                        db, \
                        num); \
                    nITPR |= ITPR_FAIL; \
                } \
            } \
        } \
        __except(EXCEPTION_EXECUTE_HANDLER) \
        { \
            Report( \
                RESULT_EXCEPTION, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                0, \
                db, \
                num); \
            nITPR |= ITPR_FAIL; \
        } \
    } while(0)

#define INVALID_CALL_POINTER_E_RAID_FLAGS(call, pointer, iface, mname, text, expected, db, num, flags) \
    do \
    { \
        __try \
        { \
            COUNT_DEBUGBREAKS(hr = call); \
            if(!CheckReturnedPointer( \
                CRP_RELEASE | CRP_ALLOWNONULLOUT | flags, \
                pointer, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                expected, \
                db, \
                num)) \
            { \
                nITPR |= ITPR_FAIL; \
            } \
            if(FAILED(hr)) \
            { \
                if(GetDebugBreakCount() != 1) \
                { \
                    Report( \
                        RESULT_DEBUGBREAK, \
                        iface, \
                        mname, \
                        0, \
                        text, \
                        NULL, \
                        1, \
                        db, \
                        num); \
                    nITPR |= ITPR_FAIL; \
                } \
            } \
            else \
            { \
                if(GetDebugBreakCount() != 0) \
                { \
                    Report( \
                        RESULT_DEBUGBREAK, \
                        iface, \
                        mname, \
                        0, \
                        text, \
                        TEXT("on success"), \
                        0, \
                        db, \
                        num); \
                    nITPR |= ITPR_FAIL; \
                } \
            } \
        } \
        __except(EXCEPTION_EXECUTE_HANDLER) \
        { \
            Report( \
                RESULT_EXCEPTION, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                0, \
                db, \
                num); \
            nITPR |= ITPR_FAIL; \
        } \
    } while(0)

#else // DEBUG
#define INVALID_CALL_E_RAID(call, iface, mname, text, expected, db, num) \
    do \
    { \
        __try \
        { \
            COUNT_DEBUGBREAKS(hr = call); \
            if(hr != expected) \
            { \
                Report( \
                    RESULT_UNEXPECTED, \
                    iface, \
                    mname, \
                    hr, \
                    text, \
                    NULL, \
                    expected, \
                    db, \
                    num); \
                nITPR |= ITPR_FAIL; \
            } \
            if(GetDebugBreakCount() != 0) \
            { \
                Report( \
                    RESULT_DEBUGBREAK, \
                    iface, \
                    mname, \
                    0, \
                    text, \
                    NULL, \
                    0, \
                    db, \
                    num); \
                nITPR |= ITPR_FAIL; \
            } \
        } \
        __except(EXCEPTION_EXECUTE_HANDLER) \
        { \
            Report( \
                RESULT_EXCEPTION, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                0, \
                db, \
                num); \
            nITPR |= ITPR_FAIL; \
        } \
    } while(0)

#define INVALID_CALL_POINTER_E_RAID_FLAGS(call, pointer, iface, mname, text, expected, db, num, flags) \
    do \
    { \
        __try \
        { \
            COUNT_DEBUGBREAKS(hr = call); \
            if(!CheckReturnedPointer( \
                CRP_RELEASE | CRP_ALLOWNONULLOUT | flags, \
                pointer, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                expected, \
                db, \
                num)) \
            { \
                nITPR |= ITPR_FAIL; \
            } \
            if(GetDebugBreakCount() != 0) \
            { \
                Report( \
                    RESULT_DEBUGBREAK, \
                    iface, \
                    mname, \
                    0, \
                    text, \
                    NULL, \
                    0, \
                    db, \
                    num); \
                nITPR |= ITPR_FAIL; \
            } \
        } \
        __except(EXCEPTION_EXECUTE_HANDLER) \
        { \
            Report( \
                RESULT_EXCEPTION, \
                iface, \
                mname, \
                hr, \
                text, \
                NULL, \
                0, \
                db, \
                num); \
            nITPR |= ITPR_FAIL; \
        } \
    } while(0)
#endif // DEBUG

#define INVALID_CALL(call, iface, mname, text) \
    INVALID_CALL_E(call, iface, mname, text, DDERR_INVALIDPARAMS)

#define INVALID_CALL_POINTER(call, pointer, iface, mname, text) \
    INVALID_CALL_POINTER_E(call, pointer, iface, mname, text, DDERR_INVALIDPARAMS)

#define INVALID_CALL_RAID(call, iface, mname, text, db, num) \
    INVALID_CALL_E_RAID(call, iface, mname, text, DDERR_INVALIDPARAMS, db, num)

#define INVALID_CALL_POINTER_RAID(call, pointer, iface, mname, text, db, num) \
    INVALID_CALL_POINTER_E_RAID(call, pointer, iface, mname, text, DDERR_INVALIDPARAMS, db, num)

#define INVALID_CALL_POINTER_RAID_FLAGS(call, pointer, iface, mname, text, db, num, flags) \
    INVALID_CALL_POINTER_E_RAID_FLAGS(call, pointer, iface, mname, text, DDERR_INVALIDPARAMS, db, num, flags)

#define INVALID_CALL_E(call, iface, mname, text, expected) \
    INVALID_CALL_E_RAID(call, iface, mname, text, expected, RAID_INVALID, 0)

#define INVALID_CALL_POINTER_E(call, pointer, iface, mname, text, expected) \
    INVALID_CALL_POINTER_E_RAID(call, pointer, iface, mname, text, expected, RAID_INVALID, 0)

#define INVALID_CALL_POINTER_E_RAID(call, pointer, iface, mname, text, expected, db, num) \
    INVALID_CALL_POINTER_E_RAID_FLAGS(call, pointer, iface, mname, text, expected, db, num, 0)

#define GET_COLORKEY_SRCBLT         26
#define GET_COLORKEY_DESTBLT        27
#if QAHOME_OVERLAY
#define GET_COLORKEY_SRCOVERLAY     28
#define GET_COLORKEY_DESTOVERLAY    29
#endif // QAHOME_OVERLAY
#define SET_COLORKEY_SRCBLT         30
#define SET_COLORKEY_DESTBLT        31
#if QAHOME_OVERLAY
#define SET_COLORKEY_SRCOVERLAY     32
#define SET_COLORKEY_DESTOVERLAY    33
#endif // QAHOME_OVERLAY

// DirectDrawSurface depths and sizes
#define DD_SURFACE_DEPTH    0x00001111
#define _8_                 0x00000001
#define _16_                0x00000010
#define _24_                0x00000011
#define _32_                0x00000100
#define DD_SURFACE_SIZE     0x11110000
#define _640_240_           0x00010000
#define _640_480_           0x00100000
#define _800_600_           0x00110000
#define _1024_768_          0x01000000

////////////////////////////////////////////////////////////////////////////////
// Global types

enum SPECIAL
{
    SPECIAL_NONE = 0,
    SPECIAL_IDDP_QI,
    SPECIAL_IDD_QI,
    SPECIAL_IDDS_QI,
    SPECIAL_BLT_VIDEO_TO_PRIMARY,
    SPECIAL_BLT_TO_VIDEO,
    // a-shende(07.20.1999): adding flag for bug target
    SPECIAL_BUG_FLAG
};

enum DDSTATE
{
    Error = 0,      // For invalid calls to ResetDirectDraw
    Default,        // DD object, coop level = EXCLUSIVE|FULLSCREEN
    DDOnly          // DD object, coop level not set
};

typedef struct IDirectDrawSurfacesStruct
{
    IDirectDraw         *m_pDD;             // DirectDraw object
    DDCAPS                  m_ddcHAL;
    IDirectDrawSurface  *m_pDDSPrimary;     // primary surface
    IDirectDrawSurface  *m_pDDSSysMem;      // offscreen system memory surface
    IDirectDrawSurface  *m_pDDSVidMem;      // offscreen video memory surface
#if QAHOME_OVERLAY
    IDirectDrawSurface  *m_pDDSOverlay1;    // overlay surface
    IDirectDrawSurface  *m_pDDSOverlay2;    // overlay surface
#endif // QAHOME_OVERLAY
    DDSURFACEDESC       m_ddsdPrimary;
    DDSURFACEDESC       m_ddsdSysMem;
    DDSURFACEDESC       m_ddsdVidMem;
#if QAHOME_OVERLAY
    DDSURFACEDESC       m_ddsdOverlay1;
    DDSURFACEDESC       m_ddsdOverlay2;
#endif // QAHOME_OVERLAY
#if QAHOME_OVERLAY
    bool                m_bOverlaysSupported;
#endif // QAHOME_OVERLAY
} IDDS, *LPIDDS;

typedef struct IDirectDrawColorControlStruct
{
    IDirectDrawColorControl *m_pDDCCPrimary;
    bool                    m_bValid;
#if QAHOME_OVERLAY
    IDirectDrawColorControl *m_pDDCCOverlay1;
    IDirectDrawColorControl *m_pDDCCOverlay2;
    bool                    m_bOverlaysSupported;
#endif // QAHOME_OVERLAY
} IDDCC, *LPIDDCC;

typedef void (*FNFINISH)(void);

////////////////////////////////////////////////////////////////////////////////

class CFinishManager
{
public:
    CFinishManager(void);
    ~CFinishManager();

    bool    AddFunction(FNFINISH);
    void    Finish(void);

private:
    struct FMNODE
    {
        FNFINISH    m_pfnFinish;
        FMNODE      *m_pNext;
    };

    FMNODE  *m_pHead;
};

////////////////////////////////////////////////////////////////////////////////
template <class T>
class CTempDynamicData
{
public:
    CTempDynamicData(int nCount = 1) { m_pData = new T[nCount]; }
    ~CTempDynamicData() { if(m_pData) delete[] m_pData; }

    operator T *() { return m_pData; }
    void free(void) { if(m_pData) { delete[] m_pData; m_pData = NULL; } }

private:
    T   *m_pData;
};

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

SHELLPROCAPI    ShellProc(UINT, SPPARAM);
void    ProcessCommandLine(LPCTSTR tszCommandLine);

DDSTATE ResetDirectDraw(DDSTATE);
bool    InitDirectDraw(IDirectDraw *&, FNFINISH pfnFinish = NULL);
bool    InitDirectDrawSurface(LPIDDS &, FNFINISH pfnFinish = NULL);
bool    InitDirectDrawColorControl(LPIDDCC &, FNFINISH pfnFinish = NULL);
bool    InitDirectDrawClipper(IDirectDrawClipper *&, FNFINISH pfnFinish = NULL);
void    FinishDirectDrawClipper(void);
void    FinishDirectDrawColorControl(void);
void    FinishDirectDrawSurface(void);
void    FinishDirectDrawClipper(void);
void    FinishDirectDraw(void);
bool    Help_CreateDDPalette(DWORD, IDirectDrawPalette **);
void    Help_WaitForKeystroke(void);
HRESULT CALLBACK Help_GetBackBuffer_Callback(LPDIRECTDRAWSURFACE, LPDDSURFACEDESC, LPVOID);


bool    InitSuite();
bool    UninitSuite();

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

#include "ft.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

#define TPRFromITPRTableName    g_nTPRFromITPRTable
GLOBAL  ITPR            TPRFromITPRTableName[MAX_ITPR_CODE] INIT(TPRFromITPRTable);
GLOBAL  SPS_SHELL_INFO  *g_pspsShellInfo;
GLOBAL  int             g_iDisplayIndex INIT(-1);
GLOBAL  HINSTANCE       g_hInstance INIT(NULL);
GLOBAL  LPVOID          g_pContext;
GLOBAL  bool            g_bTestPassed;
GLOBAL  int             g_nCallbackCount;
GLOBAL  HWND            g_hMainWnd INIT(NULL);
GLOBAL  bool            g_bKeyHit INIT(false);
GLOBAL  ITPR            g_tprReturnVal INIT(TPR_ABORT);

////////////////////////////////////////////////////////////////////////////////
// Text constants

// Constants are declared (and defined) by two header files, text.h and
// ifacetbl.h. This is done by defining the macros that these files use and
// then including them. This causes a declaration somewhat like this to occur:
// LPCTSTR c_szExample1 INIT(TEXT("Example1")),
//         c_szExample2 INIT(TEXT("Example2")),
//         ...
//         c_szExampleN INIT(TEXT("ExampleN"));

// Get the list started with the constants "IUnknown" and "IID_IUnknown"
GLOBAL LPCTSTR c_szIUnknown INIT(TEXT("IUnknown")),
               c_szIID_IUnknown INIT(TEXT("IID_IUnknown"))

// Next, declare/define all interface names
#define KNOWN_INTERFACE(x) ,c_sz##x INIT(TEXT(#x))
#include "ifacetbl.h"
#undef KNOWN_INTERFACE

// Next, declare/define all interface IDs
#define KNOWN_INTERFACE(x) ,c_szIID_##x INIT(TEXT("IID_") TEXT(#x))
#include "ifacetbl.h"
#undef KNOWN_INTERFACE

// Finally, declare/define other miscellaneous constants (primarily method
// names)
#define NAME_CONSTANT(x) ,c_sz##x INIT(TEXT(#x))
#include "text.h"
#undef NAME_CONSTANT

// The semicolon terminates the list
;

////////////////////////////////////////////////////////////////////////////////
// Known interfaces

// We want to generate two arrays and one enumerated type. They are all declared
// (and defined) by ifacetbl.h. This is done by defining the macros that this
// file uses and then including it. The first array contains pointers to IIDs.
// We will end up with something like this:
// const IID *g_aKnownIID[] = {
//     &IID_IUnknown,
//     &IID_IDirectDraw,
//     ...
// };

// Get the list started with IID_IUnknown
GLOBAL const IID *g_aKnownIID[]
#ifdef __GLOBALS_CPP__
= { &IID_IUnknown
#define KNOWN_INTERFACE(x)  ,&IID_##x
#include "ifacetbl.h"
}
#undef KNOWN_INTERFACE
#endif // __GLOBALS_CPP__
;

// Next, we also generate an array of pointers to strings, each string being
// the name of the interface in the previous array. Again, get the list started,
// with c_szIID_IUnknown in this case:
GLOBAL LPCTSTR g_aKnownIIDName[]
#ifdef __GLOBALS_CPP__
= { c_szIID_IUnknown
#define KNOWN_INTERFACE(x)  ,c_szIID_##x
#include "ifacetbl.h"
}
#undef KNOWN_INTERFACE
#endif // __GLOBALS_CPP__
;

// Finally, we generate an enumerated type, so that we can index the elements of
// the previous two arrays. Also start with INDEX_IID_IUnknown. End with
// INDEX_IID_COUNT, which will contain the number of elements in the arrays.
enum INDEX_IID
{ INDEX_IID_IUnknown = 0
#define KNOWN_INTERFACE(x)  , INDEX_IID_##x
#include "ifacetbl.h"
, INDEX_IID_COUNT };
#undef KNOWN_INTERFACE

// These prototypes couldn't be added before because INDEX_IID wasn't declared
// yet.
bool    Test_AddRefRelease(IUnknown *, LPCTSTR, LPCTSTR pszAdditional = NULL);
bool    Test_QueryInterface(
            IUnknown *,
            LPCTSTR,
            INDEX_IID *,
            int,
            SPECIAL sp = SPECIAL_NONE);

////////////////////////////////////////////////////////////////////////////////

#endif // __GLOBALS_H__
