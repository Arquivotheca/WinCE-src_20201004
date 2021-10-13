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
//  acmp.h
//
//
//  Description:
//      Internal Audio Compression Manager header file. Defines internal
//      data structures and things not needed outside of the ACM itself.
//
//==========================================================================;


#ifndef _INC_ACMI
#define _INC_ACMI       /* #defined if acmi.h has been included */


#ifndef RC_INVOKED
#pragma pack(1)         /* Assume byte packing throughout */
#endif  /* RC_INVOKED */


#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif  /* __cplusplus */

//
//
//
#ifdef DEBUG
    #define RDEBUG
#endif

#ifndef MMREVISION
//#include <verinfo.h>
#endif


#define VERSION_MSACM_MAJOR     MMVERSION
#define VERSION_MSACM_MINOR     MMREVISION

#define VERSION_MSACM_BUILD     MMRELEASE

#define VERSION_MSACM           MAKE_ACM_VERSION(VERSION_MSACM_MAJOR,   \
                                                 VERSION_MSACM_MINOR,   \
                                                 VERSION_MSACM_BUILD)


#ifndef SIZEOF_WAVEFORMATEX
#define SIZEOF_WAVEFORMATEX(pwfx)   ((WAVE_FORMAT_PCM==(pwfx)->wFormatTag)?sizeof(PCMWAVEFORMAT):(sizeof(WAVEFORMATEX)+(pwfx)->cbSize))
#endif


#define XRegCreateKey(a,s,d)   (0)
#define XRegCreateKeyEx(a,s,d,q,w,e,r,t,y)   (0)
#define XRegSetValueEx(a,s,d,f,g,h)    (0)
#define XRegCloseKey(a)      (0)
#define XRegOpenKey(a,b,c)      (0)
#define XRegOpenKeyEx(a,b,c,d,e)  (0)
#define XRegQueryValueEx(a,b,c,d,e,f) (0)
#define XRegEnumValue(a,b,c,d,e,f,g,h)  (0)
#define XRegDeleteValue(a,b)   (0)




//
//
//
#ifndef RC_INVOKED
    #ifndef FNLOCAL
        #define FNLOCAL     _stdcall
        #define FNCLOCAL    _stdcall
        #define FNGLOBAL    _stdcall
        #define FNCGLOBAL   _stdcall
        #define FNWCALLBACK CALLBACK
        #define FNEXPORT    CALLBACK
    #endif

    #ifndef _CRTAPI1
    #define _CRTAPI1    __cdecl
    #endif
    #ifndef _CRTAPI2
    #define _CRTAPI2    __cdecl
    #endif
    #ifndef try
    #define try         __try
    #define leave       __leave
    #define except      __except
    #define finally     __finally
    #endif


    //
    //  there is no reason to have based stuff in win 32
    //
    #define BCODE                   CONST

    #define HUGE
    #define HTASK                   HANDLE
    #define SELECTOROF(a)           (a)
    typedef LRESULT (CALLBACK* DRIVERPROC)(DWORD, HDRVR, UINT, LPARAM, LPARAM);


    //
    //
    //
    #define Edit_GetSelEx(hwndCtl, pnS, pnE)    \
        ((DWORD)SendMessage((hwndCtl), EM_GETSEL, (WPARAM)pnS, (LPARAM)pnE))

    //
    //  for compiling Unicode
    //
    #define SIZEOFW(x) (sizeof(x)/sizeof(WCHAR))
    #define SIZEOFA(x) (sizeof(x))
    #define SIZEOF(x)   (sizeof(x)/sizeof(WCHAR))

    #define GetCurrentTask  (HTASK)GetCurrentThreadId

    //
    //  we need to try to quit using this if possible...
    //
    void WINAPI DirectedYield(HTASK);


#endif // #ifndef RC_INVOKED


//
//  bilingual. this allows the same identifier to be used in resource files
//  and code without having to decorate the id in your code.
//
#ifdef RC_INVOKED
    #define RCID(id)    id
#else
    #define RCID(id)    MAKEINTRESOURCE(id)
#endif


//
//
//
#define MAX_DRIVER_NAME_CHARS           144 // path + name or other...


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Internal structure for driver management
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Format/Filter structures containing minimal infomation
//  about a filter tag
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

typedef struct tACMFORMATTAGCACHE
{
    DWORD           dwFormatTag;
    DWORD           cbFormatSize;
} ACMFORMATTAGCACHE, *PACMFORMATTAGCACHE, FAR *LPACMFORMATTAGCACHE;

typedef struct tACMFILTERTAGCACHE
{
    DWORD           dwFilterTag;
    DWORD           cbFilterSize;
} ACMFILTERTAGCACHE, *PACMFILTERTAGCACHE, FAR *LPACMFILTERTAGCACHE;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  ACM Stream Instance Structure
//
//  this structure is used to maintain an open stream (acmStreamOpen)
//  and maps directly to the HACMSTREAM returned to the caller. this is
//  an internal structure to the ACM and will not be exposed.
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

typedef struct tACMSTREAM      *PACMSTREAM;
typedef struct tACMSTREAM
{
    UINT                    uHandleType;    // for param validation (TYPE_HACMSTREAM)
    DWORD                   fdwStream;      // stream state flags, etc.
    PACMSTREAM              pasNext;        // next stream for driver instance (had)
    HACMDRIVER              had;            // handle to driver.
    UINT                    cPrepared;      // number of headers prepared
    ACMDRVSTREAMINSTANCE    adsi;           // passed to driver

} ACMSTREAM;

#define ACMSTREAM_STREAMF_USERSUPPLIEDDRIVER    0x00000001L
#define ACMSTREAM_STREAMF_ASYNCTOSYNC           0x00000002L


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  ACM Driver Instance Structure
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

typedef struct tACMDRIVER      *PACMDRIVER;
typedef struct tACMDRIVER
{
    UINT                uHandleType;    // param validation (TYPE_HACMDRIVER)

    PACMDRIVER          padNext;        //
    PACMSTREAM          pasFirst;       //

    HACMDRIVERID        hadid;          // identifier to driver
    HTASK               htask;          // task handle of client
    DWORD               fdwOpen;        // flags used when opened

    HDRVR               hdrvr;          // open driver handle (if driver)
    ACMDRIVERPROC       fnDriverProc;   // function entry (if not driver)
    DWORD               dwInstance;     // instance data for functions..

} ACMDRIVER;




#define ACMDRIVERID_DRIVERF_LOADED      0x00000001L // driver has been loaded
#define ACMDRIVERID_DRIVERF_CONFIGURE   0x00000002L // supports configuration
#define ACMDRIVERID_DRIVERF_ABOUT       0x00000004L // supports custom about
#define ACMDRIVERID_DRIVERF_NOTIFY      0x10000000L // notify only proc
#define ACMDRIVERID_DRIVERF_LOCAL       0x40000000L //
#define ACMDRIVERID_DRIVERF_DISABLED    0x80000000L //

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Internal ACM Driver Manager API's in ACM.C
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

LRESULT FNGLOBAL IDriverMessageId
(
    HACMDRIVERID            hadid,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
);

LRESULT FNGLOBAL IDriverConfigure
(
    HACMDRIVERID            hadid,
    HWND                    hwnd
);



//
//  Priorities locking stuff.
//
#define ACMPRIOLOCK_GETLOCK             1
#define ACMPRIOLOCK_RELEASELOCK         2
#define ACMPRIOLOCK_ISMYLOCK            3
#define ACMPRIOLOCK_ISLOCKED            4
#define ACMPRIOLOCK_LOCKISOK            5

#define ACMPRIOLOCK_FIRST               ACMPRIOLOCK_GETLOCK
#define ACMPRIOLOCK_LAST                ACMPRIOLOCK_LOCKISOK

//BOOL IDriverLockPriority
//(
//    PACMGARB                pag,
//    HTASK                   htask,
//    UINT                    uRequest
//);
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Resource defines
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

#define ICON_MSACM                  RCID(10)

#define IDS_TXT_TAG                     150
#define IDS_TXT_NONE                    (IDS_TXT_TAG+0)
#define IDS_TXT_UNTITLED                (IDS_TXT_TAG+1)
#define IDS_TXT_UNAVAILABLE             (IDS_TXT_TAG+2)

#define IDS_FORMAT_TAG_BASE             300
#define IDS_FORMAT_TAG_PCM              (IDS_FORMAT_TAG_BASE + 0)

#define IDS_FORMAT_BASE                     350
#define IDS_FORMAT_FORMAT_MONOSTEREO        (IDS_FORMAT_BASE + 0)
#define IDS_FORMAT_FORMAT_MONOSTEREO_0BIT   (IDS_FORMAT_BASE + 1)
#define IDS_FORMAT_FORMAT_MULTICHANNEL      (IDS_FORMAT_BASE + 2)
#define IDS_FORMAT_FORMAT_MULTICHANNEL_0BIT (IDS_FORMAT_BASE + 3)
#define IDS_FORMAT_CHANNELS_MONO            (IDS_FORMAT_BASE + 4)
#define IDS_FORMAT_CHANNELS_STEREO          (IDS_FORMAT_BASE + 5)
#define IDS_FORMAT_MASH                     (IDS_FORMAT_BASE + 6)



//
//  these are defined in PCM.H
//
#define IDS_PCM_TAG                     500

#define IDS_CHOOSER_TAG                 600

    // unused                           (IDS_CHOOSER_TAG+0)
    // unused                           (IDS_CHOOSER_TAG+1)
    // unused                           (IDS_CHOOSER_TAG+2)
#define IDS_CHOOSEFMT_APPTITLE          (IDS_CHOOSER_TAG+3)
#define IDS_CHOOSEFMT_RATE_FMT          (IDS_CHOOSER_TAG+4)

#define IDS_CHOOSE_FORMAT_DESC          (IDS_CHOOSER_TAG+8)
#define IDS_CHOOSE_FILTER_DESC          (IDS_CHOOSER_TAG+9)

#define IDS_CHOOSE_QUALITY_CD           (IDS_CHOOSER_TAG+10)
#define IDS_CHOOSE_QUALITY_RADIO        (IDS_CHOOSER_TAG+11)
#define IDS_CHOOSE_QUALITY_TELEPHONE    (IDS_CHOOSER_TAG+12)

#define IDS_CHOOSE_QUALITY_DEFAULT      (IDS_CHOOSE_QUALITY_RADIO)

#define IDS_CHOOSE_ERR_TAG              650

#define IDS_ERR_FMTSELECTED             (IDS_CHOOSE_ERR_TAG+0)
#define IDS_ERR_FMTEXISTS               (IDS_CHOOSE_ERR_TAG+1)
#define IDS_ERR_BLANKNAME               (IDS_CHOOSE_ERR_TAG+2)
#define IDS_ERR_INVALIDNAME             (IDS_CHOOSE_ERR_TAG+3)



#define DLG_CHOOSE_SAVE_NAME            RCID(75)
#define IDD_EDT_NAME                    100
#define IDD_STATIC_DESC                 101




//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Parameter Validation stuff
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

//
//  handle types for V_HANDLE (these can be anything except zero!) for
//  HACMOBJ, the parameter validation will test to make sure the handle
//  is one of the three types..
//
#define TYPE_HACMOBJ            0
#define TYPE_HACMDRIVERID       1
#define TYPE_HACMDRIVER         2
#define TYPE_HACMSTREAM         3
#define TYPE_HACMNOTVALID       666


//
//  for parameter validation of flags...
//
#define IDRIVERGETNEXT_VALIDF   (ACM_DRIVERENUMF_VALID)
#define IDRIVERADD_VALIDF       (ACM_DRIVERADDF_VALID)
#define IDRIVERREMOVE_VALIDF    (0L)
#define IDRIVERLOAD_VALIDF      (0L)
#define IDRIVERFREE_VALIDF      (0L)
#define IDRIVEROPEN_VALIDF      (0L)
#define IDRIVERCLOSE_VALIDF     (0L)
#define IDRIVERDETAILS_VALIDF   (0L)

#define PCM_BLOCKALIGNMENT(pwf)     (UINT)(((pwf)->wBitsPerSample >> 3) << ((pwf)->wf.nChannels >> 1))
#define PCM_BYTESTOSAMPLES(pwf, dw) (DWORD)(dw / PCM_BLOCKALIGNMENT(pwf))
#define PCM_SAMPLESTOBYTES(pwf, dw) (DWORD)(dw * PCM_BLOCKALIGNMENT(pwf))


BOOL acm_IsValidHandle(HANDLE hLocal, UINT uType);

//
//
//
BOOL FNGLOBAL ValidateHandle(HANDLE h, UINT uType);
BOOL FNGLOBAL ValidateReadWaveFormat(LPWAVEFORMATEX pwfx);
BOOL FNGLOBAL ValidateReadWaveFilter(LPWAVEFILTER pwf);
BOOL FNGLOBAL ValidateReadPointer(__in_bcount(len) const void FAR* p, DWORD len);
BOOL FNGLOBAL ValidateWritePointer(const void FAR* p, DWORD len);
BOOL FNGLOBAL ValidateDriverCallback(DWORD dwCallback, UINT uFlags);
BOOL FNGLOBAL ValidateCallback(FARPROC lpfnCallback);

BOOL FNGLOBAL ValidateStringA(LPCSTR lsz, UINT cchMaxLen);
BOOL FNGLOBAL ValidateStringW(LPCWSTR lsz, UINT cchMaxLen);
#define ValidateString      ValidateStringW


//
//  unless we decide differently, ALWAYS do parameter validation--even
//  in retail. this is the 'safest' thing we can do. note that we do
//  not LOG parameter errors in retail (see prmvalXX).
//
//  Need to write the validate functions
#if 0

#define V_HANDLE(h, t, r)       { if (!ValidateHandle(h, t)) return (r); }
#define V_RWAVEFORMAT(p, r)     { if (!ValidateReadWaveFormat(p)) return (r); }
#define V_RWAVEFILTER(p, r)     { if (!ValidateReadWaveFilter(p)) return (r); }
#define V_RPOINTER(p, l, r)     { if (!ValidateReadPointer((p), (l))) return (r); }
#define V_WPOINTER(p, l, r)     { if (!ValidateWritePointer((p), (l))) return (r); }
#define V_DCALLBACK(d, w, r)    { if (!ValidateDriverCallback((d), (w))) return (r); }
#define V_CALLBACK(f, r)        { if (!ValidateCallback(f)) return (r); }
#define V_STRING(s, l, r)       { if (!ValidateString(s,l)) return (r); }
#define V_STRINGW(s, l, r)      { if (!ValidateStringW(s,l)) return (r); }
#define _V_STRING(s, l)         (ValidateString(s,l))
#define _V_STRINGW(s, l)        (ValidateStringW(s,l))
#ifdef DEBUG
#define V_DFLAGS(t, b, f, r)    { if ((t) & ~(b)) {LogParamError(ERR_BAD_DFLAGS, (FARPROC)(f), (LPVOID)(DWORD)(t)); return (r); }}
#else
#define V_DFLAGS(t, b, f, r)    { if ((t) & ~(b))  return (r); }
#endif
#define V_FLAGS(t, b, f, r)     V_DFLAGS(t, b, f, r)
#define V_MMSYSERR(e, f, t, r)  { LogParamError(e, (FARPROC)(f), (LPVOID)(DWORD)(t)); return (r); }

#else

#define V_HANDLE(h, t, r)       { if (!(h)) return (r); }
#define V_RWAVEFORMAT(p, r)     { if (!(p)) return (r); }
#define V_RWAVEFILTER(p, r)     { if (!(p)) return (r); }
#define V_RPOINTER(p, l, r)     { if (!(p)) return (r); }
#define V_WPOINTER(p, l, r)     { if (!(p)) return (r); }
#define V_DCALLBACK(d, w, r)    0
#define V_CALLBACK(f, r)        { if (!(f)) return (r); }
#define V_STRING(s, l, r)       { if (!(s)) return (r); }
#define V_STRINGW(s, l, r)      { if (!(s)) return (r); }
#define _V_STRING(s, l)         (s)
#define _V_STRINGW(s, l)        (s)
#define V_DFLAGS(t, b, f, r)    { if ((t) & ~(b))  return (r); }
#define V_FLAGS(t, b, f, r)     V_DFLAGS(t, b, f, r)

#endif


//
//  the DV_xxxx macros are for internal DEBUG builds--aid to debugging.
//  we do 'loose' parameter validation in retail builds...
//
#ifdef DEBUG

#define DV_HANDLE(h, t, r)      V_HANDLE(h, t, r)
#define DV_RWAVEFORMAT(p, r)    V_RWAVEFORMAT(p, r)
#define DV_RPOINTER(p, l, r)    V_RPOINTER(p, l, r)
#define DV_WPOINTER(p, l, r)    V_WPOINTER(p, l, r)
#define DV_DCALLBACK(d, w, r)   V_DCALLBACK(d, w, r)
#define DV_CALLBACK(f, r)       V_CALLBACK(f, r)
#define DV_STRING(s, l, r)      V_STRING(s, l, r)
#define DV_DFLAGS(t, b, f, r)   V_DFLAGS(t, b, f, r)
#define DV_FLAGS(t, b, f, r)    V_FLAGS(t, b, f, r)
#define DV_MMSYSERR(e, f, t, r) V_MMSYSERR(e, f, t, r)

#else

#define DV_HANDLE(h, t, r)      { if (!(h)) return (r); }
#define DV_RWAVEFORMAT(p, r)    { if (!(p)) return (r); }
#define DV_RPOINTER(p, l, r)    { if (!(p)) return (r); }
#define DV_WPOINTER(p, l, r)    { if (!(p)) return (r); }
#define DV_DCALLBACK(d, w, r)   0
#define DV_CALLBACK(f, r)       { if (!(f)) return (r); }
#define DV_STRING(s, l, r)      { if (!(s)) return (r); }
#define DV_DFLAGS(t, b, f, r)   { if ((t) & ~(b))  return (r); }
#define DV_FLAGS(t, b, f, r)    DV_DFLAGS(t, b, f, r)
#define DV_MMSYSERR(e, f, t, r) { return (r); }

#endif

//
//  Locking stuff
//

#if defined(WIN32) && defined(_MT)
    //
    //
    //
    typedef struct {
        CRITICAL_SECTION CritSec;
    } ACM_HANDLE, *PACM_HANDLE;
    #define HtoPh(h) (((PACM_HANDLE)(h)) - 1)
    HLOCAL NewHandleAcm(UINT length);
    VOID   DeleteHandle(HLOCAL h);
    #define EnterHandle(h) EnterCriticalSection(&HtoPh(h)->CritSec)
    #define LeaveHandle(h) LeaveCriticalSection(&HtoPh(h)->CritSec)
    #define ENTER_LIST_EXCLUSIVE AcquireLockExclusive(&pag->lockDriverIds)
    #define LEAVE_LIST_EXCLUSIVE ReleaseLock(&pag->lockDriverIds)
    #define ENTER_LIST_SHARED {AcquireLockShared(&pag->lockDriverIds); threadEnterListShared(pag);}
    #define LEAVE_LIST_SHARED {threadLeaveListShared(pag); ReleaseLock(&pag->lockDriverIds);}
#else
#pragma message("not locking")
    typedef struct {
        CRITICAL_SECTION CritSec;
    } ACM_HANDLE, *PACM_HANDLE;
    #define NewHandle(length) LocalAlloc(LPTR, length)
    #define DeleteHandle(h)   LocalFree(h)
    #define EnterHandle(h)
    #define LeaveHandle(h)
    #define ENTER_LIST_EXCLUSIVE
    #define LEAVE_LIST_EXCLUSIVE
    #define ENTER_LIST_SHARED threadEnterListShared(pag)
    #define LEAVE_LIST_SHARED threadLeaveListShared(pag)
#endif // !(WIN32 && _MT)

;


#ifndef RC_INVOKED
#pragma pack()          /* Revert to default packing */
#endif  /* RC_INVOKED */

#ifdef __cplusplus
}                       /* End of extern "C" { */
#endif  /* __cplusplus */

#endif  /* _INC_ACMI */

//--------------------------------------------------------------------------;
//
//  Simple debug code to help detect memory leaks and other problems
//
//--------------------------------------------------------------------------;
#ifdef RDEBUG
extern int gcLocalAlloc;
extern int gcFixes;

HLOCAL  WINAPI dbgLocalAlloc(UINT, UINT);
HLOCAL  WINAPI dbgLocalReAlloc(HLOCAL, UINT, UINT);
HLOCAL  WINAPI dbgLocalFree(HLOCAL);
#else
#define dbgLocalAlloc   LocalAlloc
#define dbgLocalReAlloc LocalReAlloc
#define dbgLocalFree    LocalFree
#endif



// Need to do something here.
#define LOCK_HANDLE(x)  (0)
#define UNLOCK_HANDLE(x)  (0)
