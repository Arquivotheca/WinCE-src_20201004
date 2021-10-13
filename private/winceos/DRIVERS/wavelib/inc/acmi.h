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
//==========================================================================;
//
//  acmi.h
//
//  Copyright (c) 1991-2000 Microsoft Corporation
//
//  Description:
//      Internal Audio Compression Manager header file. Defines internal
//      data structures and things not needed outside of the ACM itself.
//
//  History:
//
//==========================================================================;

#pragma once

#include <windows.h>
#include <windowsx.h>
#include <wtypes.h>
#include <mmddk.h>
#include <memory.h>
#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>
#include <debug.h>
#include <wavedev.h>
#include <unknwn.h>
#include "handleobj.h"

extern HINSTANCE g_hInstance;

#define MSE(x)   MMSYSERR_##x

#define GOTO_EXIT(dosomething)  { dosomething; goto EXIT; }

#define VEC_TEXT(cond, retval, txt) \
    { if (cond) { WARNMSG(txt); mmRet = retval; goto EXIT; }}


#define VEC_PARAM(cond)  VEC_TEXT(cond, MSE(INVALPARAM),   "Invalid Parameter")
#define VEC_HANDLE(cond) VEC_TEXT(cond, MSE(INVALHANDLE),  "Invalid Handle")
#define VEC_NSUP(cond)   VEC_TEXT(cond, MSE(NOTSUPPORTED), "Not Supported")
#define VEC_ERR(cond)    VEC_TEXT(cond, MSE(ERROR),        "General Error")
#define VEC_ALLOC(cond)  VEC_TEXT(cond, MSE(ALLOCATED),    "Allocated")
#define VEC_FLAG(cond)   VEC_TEXT(cond, MSE(INVALFLAG),    "Invalid Flag")
#define VEC_NODRV(cond)  VEC_TEXT(cond, MSE(NODRIVER),     "No driver")
#define VEC_NOTEN(cond)  VEC_TEXT(cond, MSE(NOTENABLED),   "Not enabled")
#define VEC_NOMEM(cond)  VEC_TEXT(cond, MSE(NOMEM),        "No memory")
#define VEC_NOERR(cond)  VEC_TEXT(cond, MSE(NOERROR),      "No Error")


#define VE_WPOINTER(ptr, size) \
    VE_COND_TEXT(!(ptr), MSE(INVALPARAM), "Bad Write Pointer")

#define VE_RPOINTER(ptr, size) \
    VE_COND_TEXT(!(ptr), MSE(INVALPARAM), "Bad Read Pointer")

#define VE_FLAGS(flags, valid) VE_COND_FLAG((flags) & ~(valid))
#define VE_HANDLE(handle, type) VE_COND_HANDLE(!acm_IsValidHandle(handle, type))

#define VE_RWAVEFILTER(pwfx) \
    { if ((pwfx)->cbStruct<sizeof(*(pwfx))) { \
        WARNMSG("Invalid Wave Filter");      mmRet = MSE(INVALPARAM); goto EXIT; }}

#define VE_CALLBACK(fncallback) \
    { if (!acm_IsValidCallback(fncallback)) { \
        WARNMSG("Invalid Callback");    mmRet = MSE(INVALPARAM); goto EXIT; }}

#define VE_COND(cond, retval) \
    { if (cond) { \
        WARNMSG("cond not met"); mmRet = retval; goto EXIT; }}

#define VE_EXIT_ON_ERROR() { if (mmRet != MMSYSERR_NOERROR) goto EXIT; }

#define VE_COND_PARAM(cond)   VEC_PARAM(cond)
#define VE_COND_HANDLE(cond)  VEC_HANDLE(cond)
#define VE_COND_NSUP(cond)    VEC_NSUP(cond)
#define VE_COND_ERR(cond)     VEC_ERR(cond)
#define VE_COND_ALLOC(cond)   VEC_ALLOC(cond)
#define VE_COND_FLAG(cond)    VEC_FLAG(cond)
#define VE_COND_NODRV(cond)   VEC_NODRV(cond)
#define VE_COND_NOTEN(cond)   VEC_NOTEN(cond)
#define VE_COND_NOMEM(cond)   VEC_NOMEM(cond)
#define VE_COND_NOERR(cond)   VEC_NOERR(cond)
#define VE_COND_TEXT(x,y,z)   VEC_TEXT(x,y,z)

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

// BUGBUG what here?
#define VERSION_MSACM           0x02001997

//#define VERSION_MSACM           MAKE_ACM_VERSION(VERSION_MSACM_MAJOR,   \
//                                                 VERSION_MSACM_MINOR,   \
//                                                 VERSION_MSACM_BUILD)

typedef ULONG ACMHANDLE, * PACMHANDLE;

__inline DWORD SIZEOF_WAVEFORMATEX(LPWAVEFORMATEX pwfx)
{
    if (pwfx->wFormatTag==WAVE_FORMAT_PCM)
    {
        return sizeof(PCMWAVEFORMAT);
    }
    else
    {
        // Only support sizes < ~64k
        DWORD cbSize = pwfx->cbSize;
        if (cbSize<0x10000)
        {
            return sizeof(WAVEFORMATEX) + cbSize;
        }

        // Error case
        return 0;
    }
}

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

class ACMOBJ :  public CHandleObj
{
public:

    ACMOBJ() : uHandleType(0)
    {
    }

    virtual ~ACMOBJ()
    {
    }

    UINT                    uHandleType;    // for param validation (TYPE_HACMSTREAM)
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  ACM Stream Instance Structure
//
//  this structure is used to maintain an open stream (acmStreamOpen)
//  and maps directly to the HACMSTREAM returned to the caller. this is
//  an internal structure to the ACM and will not be exposed.
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
class ACMSTREAM;
typedef ACMSTREAM *PACMSTREAM;
class ACMSTREAM : public ACMOBJ
{
public:

    ACMSTREAM() :
        fdwStream(0),
        pasNext(0),
        had(0),
        cPrepared(0)
    {
        memset(&adsi,0,sizeof(adsi));
    }

    virtual ~ACMSTREAM()
    {
        LocalFree(adsi.pwfxSrc);
        LocalFree(adsi.pwfxDst);
        LocalFree(adsi.pwfltr);
    }

    DWORD                   fdwStream;      // stream state flags, etc.
    PACMSTREAM              pasNext;        // next stream for driver instance (had)
    HACMDRIVER              had;            // handle to driver.
    UINT                    cPrepared;      // number of headers prepared
    ACMDRVSTREAMINSTANCE    adsi;           // passed to driver

};

#define ACMSTREAM_STREAMF_USERSUPPLIEDDRIVER    0x00000001L
#define ACMSTREAM_STREAMF_ASYNCTOSYNC           0x00000002L


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  ACM Driver Instance Structure
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

class ACMDRIVER;
typedef ACMDRIVER *PACMDRIVER;
class ACMDRIVER : public ACMOBJ
{
public:

    ACMDRIVER() :
        padNext(0),
        pasFirst(0),
        hadid(0),
        fdwOpen(0),
        hdrvr(0),
        fnDriverProc(0),
        dwInstance(0)
    {
    }

    virtual ~ACMDRIVER()
    {
    }


    PACMDRIVER          padNext;        //
    PACMSTREAM          pasFirst;       //

    HACMDRIVERID        hadid;          // identifier to driver
    HTASK               htask;          // task handle of client
    DWORD               fdwOpen;        // flags used when opened

    HDRVR               hdrvr;          // open driver handle (if driver)
    ACMDRIVERPROC       fnDriverProc;   // function entry (if not driver)
    DWORD               dwInstance;     // instance data for functions..

};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  ACM Driver Identifier Structure
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

typedef struct tACMGARB        *PACMGARB;

class ACMDRIVERID;
typedef ACMDRIVERID *PACMDRIVERID;
class ACMDRIVERID : public ACMOBJ
{
public:
    ACMDRIVERID() :
        pag(0),
        fRemove(0),
        uPriority(0),
        padidNext(0),
        padFirst(0),
        lParam(0),
        fdwAdd(0),
        fdwDriver(0),
        fdwSupport(0),
        cFormatTags(0),
        paFormatTagCache(0),
        cFilterTags(0),
        paFilterTagCache(0),
        hDev(0),
        hdrvr(0),
        fnDriverProc(0),
        dwInstance(0),
        pszSection(0),
        pstrPnpDriverFilename(0),
        dnDevNode(0)
        {
            szAlias[0]=0;
        }

    virtual ~ACMDRIVERID()
    {
    }


    //
    //
    //
    PACMGARB            pag;            // ptr back to garb

    BOOL                fRemove;        // this driver needs to be removed

    UINT                uPriority;      // current global priority
    PACMDRIVERID        padidNext;      // next driver identifier in list
    PACMDRIVER          padFirst;       // first open instance of driver


    HTASK               htask;          // task handle if driver is local

    LPARAM              lParam;         // lParam used when 'added'
    DWORD               fdwAdd;         // flags used when 'added'

    DWORD               fdwDriver;      // ACMDRIVERID_DRIVERF_* info bits

    //
    //  The following members of this structure are cached in the
    //  registry for each driver alias.
    //
    //      fdwSupport
    //      cFormatTags
    //      *paFormatTagCache (for each format tag)
    //      cFilterTags
    //      *paFilterTagCache (for each filter tag)
    //

    DWORD               fdwSupport;     // ACMDRIVERID_SUPPORTF_* info bits

    UINT                cFormatTags;
    PACMFORMATTAGCACHE  paFormatTagCache;

    UINT                cFilterTags;    // from aci.cFilterTags
    PACMFILTERTAGCACHE  paFilterTagCache;

    //
    //
    //
    HANDLE              hDev;
    HDRVR               hdrvr;          // open driver handle (if driver)
    ACMDRIVERPROC       fnDriverProc;   // function entry (if not driver)
    DWORD               dwInstance;     // instance data for functions..

    LPCWSTR             pszSection;
    WCHAR               szAlias[MAX_DRIVER_NAME_CHARS];

    PWSTR               pstrPnpDriverFilename;
    DWORD               dnDevNode;
};

#define ACMDRIVERID_DRIVERF_LOADED      0x00000001L // driver has been loaded
#define ACMDRIVERID_DRIVERF_CONFIGURE   0x00000002L // supports configuration
#define ACMDRIVERID_DRIVERF_ABOUT       0x00000004L // supports custom about
#define ACMDRIVERID_DRIVERF_NOTIFY      0x10000000L // notify only proc
#define ACMDRIVERID_DRIVERF_LOCAL       0x40000000L //
#define ACMDRIVERID_DRIVERF_DISABLED    0x80000000L //

int __inline IComboBox_InsertString(HWND hwndCtl, int index, LPCTSTR lpsz)
{
    return ComboBox_InsertString(hwndCtl, index, lpsz);
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//  Internal ACM Driver Manager API's in ACM.C
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

#ifdef DEBUG
VOID acm_PrintList();
#else
#define acm_PrintList() 0
#endif

PACMDRIVERID
acm_GetPadidFromHad(
    HACMDRIVER had
    );

LRESULT IDriverMessageId
(
    HACMDRIVERID            hadid,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
);

LRESULT IDriverConfigure
(
    HACMDRIVERID            hadid,
    HWND                    hwnd
);

MMRESULT IDriverGetNext
(
    LPHACMDRIVERID          phadidNext,
    HACMDRIVERID            hadid,
    DWORD                   fdwGetNext
);

MMRESULT IDriverAdd
(
    LPHACMDRIVERID          phadidNew,
    HINSTANCE               hinstModule,
    LPARAM                  lParam,
    DWORD                   dwPriority,
    DWORD                   fdwAdd,
    WCHAR                   *szAlias
);

MMRESULT IDriverRemove
(
    HACMDRIVERID            hadid,
    DWORD                   fdwRemove
);

MMRESULT IDriverOpen
(
    LPHACMDRIVER            phadNew,
    HACMDRIVERID            hadid,
    DWORD                   fdwOpen
);

MMRESULT IDriverClose
(
    HACMDRIVER              had,
    DWORD                   fdwClose
);


MMRESULT
IDriverLoad(
    HACMDRIVERID            hadid,
    DWORD                   fdwLoad
    );


LRESULT IDriverMessage
(
    HACMDRIVER              had,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
);

MMRESULT IDriverDetails
(
    HACMDRIVERID            hadid,
    LPACMDRIVERDETAILS      padd,
    DWORD                   fdwDetails
);


MMRESULT IDriverPriority
(
    PACMDRIVERID            padid,
    DWORD                   dwPriority,
    DWORD                   fdwPriority
);

MMRESULT IDriverSupport
(
    HACMDRIVERID            hadid,
    LPDWORD                 pfdwSupport,
    BOOL                    fFullSupport
);

MMRESULT IDriverCount
(
    LPDWORD                 pdwCount,
    DWORD                   fdwSupport,
    DWORD                   fdwEnum
);

MMRESULT IDriverGetWaveIdentifier
(
    HACMDRIVERID            hadid,
    LPDWORD                 pdw,
    BOOL                    fInput
);


MMRESULT acmBootPnpDrivers();
MMRESULT acmBootDrivers();
BOOL acmFreeDrivers();

VOID IDriverRefreshPriority();
BOOL IDriverPrioritiesRestore();
BOOL IDriverPrioritiesSave();
BOOL IDriverBroadcastNotify();
DWORD IDriverCountGlobal();

MMRESULT IMetricsMaxSizeFormat(
    HACMDRIVER          had,
    LPDWORD             pdwSize
);

MMRESULT IMetricsMaxSizeFilter(
    HACMDRIVER          had,
    LPDWORD             pdwSize
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

BOOL
IDriverLockPriority(
    UINT                    uRequest
    );


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

#ifndef IDS_MSPCM_TAG
    #define IDS_MSPCM_TAG           800
#endif

#define IDS_CODEC_SHORTNAME         (IDS_MSPCM_TAG+0)
#define IDS_CODEC_LONGNAME          (IDS_MSPCM_TAG+1)
#define IDS_CODEC_COPYRIGHT         (IDS_MSPCM_TAG+2)
#define IDS_CODEC_LICENSING         (IDS_MSPCM_TAG+3)
#define IDS_CODEC_FEATURES          (IDS_MSPCM_TAG+5)



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

typedef struct tACMGARB
{
    PACMGARB        pagNext;            // next garb structure
    DWORD           pid;                // process id associated with this garb
    UINT            cUsage;             // usage count for this process

    //
    //  boot flags
    //
    BOOL            fDriversBooted;     // have we booted drivers?

    //
    //
    //
    HINSTANCE       hinst;              // hinst of ACM module

    PACMDRIVERID    padidFirst;         // list of installed drivers

    HACMDRIVERID    hadidDestroy;       // driver being destroyed
    HACMDRIVER      hadDestroy;         // driver handle being destroyed

    HTASK           htaskPriority;      //

} ACMGARB, *PACMGARB, FAR *LPACMGARB;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

#define pagFind() gplag
#define pagFindAndBoot() gplag
PACMGARB pagNew(void);
void     pagDelete(PACMGARB pag);


//
//
//
extern PACMGARB         pag;
extern CONST TCHAR      gszKeyDrivers[];
extern CONST TCHAR      gszDevNode[];
extern CONST TCHAR      gszSecDrivers[];
extern CONST WCHAR      gszSecDriversW[];
extern CONST TCHAR      gszSecDrivers32[];
extern CONST TCHAR      gszIniSystem[];


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
BOOL acm_IsValidCallback(PVOID fnCallback);

//
//
//
BOOL ValidateHandle(HANDLE h, UINT uType);
BOOL ValidateReadWaveFormat(LPWAVEFORMATEX pwfx);
BOOL ValidateReadPointer(LPVOID ptr, UINT size);
BOOL ValidateWritePointer(const void FAR* p, DWORD len);
BOOL ValidateDriverCallback(DWORD dwCallback, UINT uFlags);
BOOL ValidateCallback(FARPROC lpfnCallback);

BOOL ValidateStringW(LPCWSTR lsz, UINT cchMaxLen);


//
//  unless we decide differently, ALWAYS do parameter validation--even
//  in retail. this is the 'safest' thing we can do. note that we do
//  not LOG parameter errors in retail (see prmvalXX).
//

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

#define NewHandle(length) LocalAlloc(LMEM_FIXED, length)
#define DeleteHandle(h)   LocalFree(h)
#define EnterHandle(h)
#define LeaveHandle(h)
#define ENTER_LIST_EXCLUSIVE()  (0)
#define LEAVE_LIST_EXCLUSIVE()  (0)
#define ENTER_LIST_SHARED()     (0)
#define LEAVE_LIST_SHARED()     (0)

// BUGBUGBUGBUG do something here.
#define LOCK_HANDLE(x)  (0)
#define UNLOCK_HANDLE(x)  (0)

DWORD
wapi_DoFunctionCallback(
    PVOID  pfnCallback,
    DWORD  dwNumParams,
    DWORD  dwParam1,
    DWORD  dwParam2,
    DWORD  dwParam3,
    DWORD  dwParam4,
    DWORD  dwParam5
    );

#define IS_ALIGNED(x)   ((((DWORD) x) & 3) == 0)

//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;           // number of valid bytes in structure

    FOURCC          fccType;            // compressor type 'audc'
    FOURCC          fccComp;            // sub-type (not used; reserved)

    WORD            wMid;               // manufacturer id
    WORD            wPid;               // product id

    DWORD           vdwACM;             // version of the ACM *compiled* for
    DWORD           vdwDriver;          // version of the driver

    DWORD           fdwSupport;         // misc. support flags
    DWORD           cFormatTags;        // total unique format tags supported
    DWORD           cFilterTags;        // total unique filter tags supported

    HICON           hicon;              // handle to custom icon

    WCHAR           szShortName[ACMDRIVERDETAILS_SHORTNAME_CHARS];
    WCHAR           szLongName[ACMDRIVERDETAILS_LONGNAME_CHARS];
    WCHAR           szCopyright[ACMDRIVERDETAILS_COPYRIGHT_CHARS];
    WCHAR           szLicensing[ACMDRIVERDETAILS_LICENSING_CHARS];
    WCHAR           szFeatures[ACMDRIVERDETAILS_FEATURES_CHARS];

} ACMDRIVERDETAILS_INT, *PACMDRIVERDETAILS_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;
    DWORD           dwFormatTagIndex;
    DWORD           dwFormatTag;
    DWORD           cbFormatSize;
    DWORD           fdwSupport;
    DWORD           cStandardFormats;
    WCHAR           szFormatTag[ACMFORMATTAGDETAILS_FORMATTAG_CHARS];

} ACMFORMATTAGDETAILS_INT, *PACMFORMATTAGDETAILS_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;
    DWORD           dwFormatIndex;
    DWORD           dwFormatTag;
    DWORD           fdwSupport;
    LPWAVEFORMATEX  pwfx;
    DWORD           cbwfx;
    WCHAR           szFormat[ACMFORMATDETAILS_FORMAT_CHARS];

} ACMFORMATDETAILS_INT, *PACMFORMATDETAILS_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;           // sizeof(ACMFORMATCHOOSE)
    DWORD           fdwStyle;           // chooser style flags

    HWND            hwndOwner;          // caller's window handle

    LPWAVEFORMATEX  pwfx;               // ptr to wfx buf to receive choice
    DWORD           cbwfx;              // size of mem buf for pwfx
    LPCWSTR         pszTitle;           // dialog box title bar

    WCHAR           szFormatTag[ACMFORMATTAGDETAILS_FORMATTAG_CHARS];
    WCHAR           szFormat[ACMFORMATDETAILS_FORMAT_CHARS];

    LPWSTR          pszName;            // custom name selection
    DWORD           cchName;            // size in chars of mem buf for pszName

    DWORD           fdwEnum;            // format enumeration restrictions
    LPWAVEFORMATEX  pwfxEnum;           // format describing restrictions

    HINSTANCE       hInstance;          // app instance containing dlg template
    LPCWSTR         pszTemplateName;    // custom template name
    LPARAM          lCustData;          // data passed to hook fn.
    ACMFORMATCHOOSEHOOKPROC pfnHook;    // ptr to hook function

} ACMFORMATCHOOSE_INT, *PACMFORMATCHOOSE_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;
    DWORD           dwFilterTagIndex;
    DWORD           dwFilterTag;
    DWORD           cbFilterSize;
    DWORD           fdwSupport;
    DWORD           cStandardFilters;
    WCHAR           szFilterTag[ACMFILTERTAGDETAILS_FILTERTAG_CHARS];

} ACMFILTERTAGDETAILS_INT, *PACMFILTERTAGDETAILS_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;
    DWORD           dwFilterIndex;
    DWORD           dwFilterTag;
    DWORD           fdwSupport;
    LPWAVEFILTER    pwfltr;
    DWORD           cbwfltr;
    WCHAR           szFilter[ACMFILTERDETAILS_FILTER_CHARS];

} ACMFILTERDETAILS_INT, *PACMFILTERDETAILS_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;           // sizeof(ACMFILTERCHOOSE)
    DWORD           fdwStyle;           // chooser style flags

    HWND            hwndOwner;          // caller's window handle

    LPWAVEFILTER    pwfltr;             // ptr to wfltr buf to receive choice
    DWORD           cbwfltr;            // size of mem buf for pwfltr

    LPCWSTR         pszTitle;

    WCHAR           szFilterTag[ACMFILTERTAGDETAILS_FILTERTAG_CHARS];
    WCHAR           szFilter[ACMFILTERDETAILS_FILTER_CHARS];
    LPWSTR          pszName;            // custom name selection
    DWORD           cchName;            // size in chars of mem buf for pszName

    DWORD           fdwEnum;            // filter enumeration restrictions
    LPWAVEFILTER    pwfltrEnum;         // filter describing restrictions

    HINSTANCE       hInstance;          // app instance containing dlg template
    LPCWSTR         pszTemplateName;    // custom template name
    LPARAM          lCustData;          // data passed to hook fn.
    ACMFILTERCHOOSEHOOKPROC pfnHook;    // ptr to hook function

} ACMFILTERCHOOSE_INT, *PACMFILTERCHOOSE_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;               // sizeof(ACMSTREAMHEADER)
    DWORD           fdwStatus;              // ACMSTREAMHEADER_STATUSF_*
    DWORD           dwUser;                 // user instance data for hdr
    LPBYTE          pbSrc;
    DWORD           cbSrcLength;
    DWORD           cbSrcLengthUsed;
    DWORD           dwSrcUser;              // user instance data for src
    LPBYTE          pbDst;
    DWORD           cbDstLength;
    DWORD           cbDstLengthUsed;
    DWORD           dwDstUser;              // user instance data for dst
    DWORD           dwReservedDriver[10];   // driver reserved work space

} ACMSTREAMHEADER_INT, *PACMSTREAMHEADER_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD           cbStruct;       // sizeof(ACMDRVOPENDESC)
    FOURCC          fccType;        // 'audc'
    FOURCC          fccComp;        // sub-type (not used--must be 0)
    DWORD           dwVersion;      // current version of ACM opening you
    DWORD           dwFlags;        //
    DWORD           dwError;        // result from DRV_OPEN request
    LPCTSTR         pszSectionName; // see DRVCONFIGINFO.lpszDCISectionName
    LPCTSTR         pszAliasName;   // see DRVCONFIGINFO.lpszDCIAliasName
    DWORD           dnDevNode;      // devnode id for pnp drivers.

} ACMDRVOPENDESC_INT, *PACMDRVOPENDESC_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD               cbStruct;
    LPWAVEFORMATEX      pwfxSrc;
    LPWAVEFORMATEX      pwfxDst;
    LPWAVEFILTER        pwfltr;
    DWORD               dwCallback;
    DWORD               dwInstance;
    DWORD               fdwOpen;
    DWORD               fdwDriver;
    DWORD               dwDriver;
    HACMSTREAM          has;

} ACMDRVSTREAMINSTANCE_INT, *PACMDRVSTREAMINSTANCE_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD                   cbStruct;
    DWORD                   fdwStatus;
    DWORD                   dwUser;
    LPBYTE                  pbSrc;
    DWORD                   cbSrcLength;
    DWORD                   cbSrcLengthUsed;
    DWORD                   dwSrcUser;
    LPBYTE                  pbDst;
    DWORD                   cbDstLength;
    DWORD                   cbDstLengthUsed;
    DWORD                   dwDstUser;
    DWORD                   fdwConvert;     // flags passed from convert func
    LPACMDRVSTREAMHEADER    padshNext;      // for async driver queueing
    DWORD                   fdwDriver;      // driver instance flags
    DWORD                   dwDriver;       // driver instance data
    DWORD                   fdwPrepared;
    DWORD                   dwPrepared;
    LPBYTE                  pbPreparedSrc;
    DWORD                   cbPreparedSrcLength;
    LPBYTE                  pbPreparedDst;
    DWORD                   cbPreparedDstLength;

} ACMDRVSTREAMHEADER_INT, *PACMDRVSTREAMHEADER_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD               cbStruct;
    DWORD               fdwSize;
    DWORD               cbSrcLength;
    DWORD               cbDstLength;

} ACMDRVSTREAMSIZE_INT, *PACMDRVSTREAMSIZE_INT;


//------------------------------------------------------------------------------
typedef struct
{
    DWORD               cbStruct;           // sizeof(ACMDRVFORMATSUGGEST)
    DWORD               fdwSuggest;         // Suggest flags
    LPWAVEFORMATEX      pwfxSrc;            // Source Format
    DWORD               cbwfxSrc;           // Source Size
    LPWAVEFORMATEX      pwfxDst;            // Dest format
    DWORD               cbwfxDst;           // Dest Size

} ACMDRVFORMATSUGGEST_INT, *PACMDRVFORMATSUGGEST_INT;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------



#define ACM_DRIVERIDF_VALID         (0L)        /* ;Internal */
#define ACM_DRIVERENUMF_NOTIFY      0x10000000L     // ;Internal
#define ACM_DRIVERENUMF_REMOVED     0x20000000L     // ;Internal
#define ACM_DRIVERENUMF_VALID       0xF0000000L     // ;Internal

//
// From the PUBLIC header (unsupported flags)
//
#define ACM_DRIVERADDF_FUNCTION     0x00000003L  // lParam is a procedure
#define ACM_DRIVERADDF_TYPEMASK     0x00000007L  // driver type mask
#define ACM_DRIVERADDF_LOCAL        0x00000000L  // is local to current task
#define ACM_DRIVERADDF_GLOBAL       0x00000008L  // is global

#define ACM_DRIVERADDF_NAME         0x00000001L  // ;Internal
#define ACM_DRIVERADDF_NOTIFY       0x00000002L  // ;Internal
#define ACM_DRIVERADDF_32BIT        0x80000000L  // ;Internal
#define ACM_DRIVERADDF_PNP          0x40000000L  // ;Internal
#define ACM_DRIVERADDF_VALID        (ACM_DRIVERADDF_TYPEMASK | /* ;Internal */ \
                                     ACM_DRIVERADDF_GLOBAL) /* ;Internal */

#define ACM_DRIVERREMOVEF_UNINSTALL 0x00000001L     // ;Internal
#define ACM_DRIVERREMOVEF_VALID     (1L)            // ;Internal
#define ACM_DRIVEROPENF_VALID       (0L)        // ;Internal
#define ACM_DRIVERCLOSEF_VALID      (0L)        // ;Internal
#define ACM_DRIVERPRIORITYF_VALID       0x00030003L     // ;Internal
#define ACMDRIVERDETAILS_SUPPORTF_NOTIFY    0x10000000L     // ;Internal
#define ACM_DRIVERDETAILSF_VALID    (0L)        // ;Internal
#define ACM_FORMATTAGDETAILSF_VALID         (ACM_FORMATTAGDETAILSF_QUERYMASK)   /* ;Internal */
#define ACM_FORMATTAGENUMF_VALID    (0L)        // ;Internal
#define ACM_FORMATDETAILSF_VALID        (ACM_FORMATDETAILSF_QUERYMASK)  // ;Internal
#define ACM_FORMATENUMF_VALID           (0x01FF0000L)    // ;Internal
#define ACM_FORMATSUGGESTF_VALID            (ACM_FORMATSUGGESTF_TYPEMASK) // ;Internal
#define FORMATCHOOSE_FORMATTAG_ADD      (FORMATCHOOSE_MESSAGE+3)    // ;Internal
#define FORMATCHOOSE_FORMAT_ADD         (FORMATCHOOSE_MESSAGE+4)    // ;Internal
#define ACMFORMATCHOOSE_STYLEF_VALID                (0x000000FCL) // ;Internal
#define ACM_FILTERTAGDETAILSF_VALID         (ACM_FILTERTAGDETAILSF_QUERYMASK)  // ;Internal
#define ACM_FILTERTAGENUMF_VALID        (0L)        // ;Internal
#define ACM_FILTERDETAILSF_VALID        (ACM_FILTERDETAILSF_QUERYMASK)  // ;Internal
#define ACM_FILTERENUMF_VALID               0x00010000L     // ;Internal
#define FILTERCHOOSE_FILTERTAG_ADD      (FILTERCHOOSE_MESSAGE+3)    // ;Internal
#define ACMFILTERCHOOSE_STYLEF_VALID                (0x000000FCL) // ;Internal
#define ACMSTREAMHEADER_STATUSF_VALID       0x00130000L     // ;Internal
#define ACM_STREAMOPENF_VALID           (CALLBACK_TYPEMASK | 0x00000007L) // ;Internal
#define ACM_STREAMCLOSEF_VALID          (0L)        // ;Internal
#define ACM_STREAMSIZEF_VALID           (ACM_STREAMSIZEF_QUERYMASK) // ;Internal
#define ACM_STREAMRESETF_VALID          (0L)        // ;Internal
#define ACM_STREAMCONVERTF_VALID        (ACM_STREAMCONVERTF_BLOCKALIGN | /* ;Internal */ \
                                         ACM_STREAMCONVERTF_END | /* ;Internal */ \
                                         ACM_STREAMCONVERTF_START) /* ;Internal */
#define ACM_STREAMPREPAREF_VALID        (0L)        // ;Internal
#define ACM_STREAMUNPREPAREF_VALID      (0L)        // ;Internal


#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif  /* __cplusplus */

// -------------------------------
//  WAVEUI functions
// -------------------------------
void waveui_Init();
void waveui_DeInit();
void waveui_BeforeDialogBox(
    LPVOID pChooseStruct,
    BOOL   fIsFormatDialog
    );
void waveui_AfterDialogBox(
    LPVOID pChooseStruct,
    BOOL   fIsFormatDialog
    );
BOOL waveui_AcmDlgProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
    );

#ifdef __cplusplus
}                       /* End of extern "C" { */
#endif  /* __cplusplus */


