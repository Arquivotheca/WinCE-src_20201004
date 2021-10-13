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
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//
//--------------------------------------------------------------------------;
//
//  codec.h
//
//  Description:
//      This file contains codec definitions, Win16/Win32 compatibility
//      definitions, and instance structure definitions.
//
//
//==========================================================================;

#ifndef _INC_CODEC
#define _INC_CODEC                  // #defined if codec.h has been included

#ifndef RC_INVOKED
#pragma pack(1)                     // assume byte packing throughout
#endif

#ifndef EXTERN_C
#ifdef __cplusplus
    #define EXTERN_C extern "C"
#else
    #define EXTERN_C extern 
#endif
#endif

#ifdef __cplusplus
extern "C"                          // assume C declarations for C++
{
#endif


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  ACM Driver Version:
//
//  the version is a 32 bit number that is broken into three parts as 
//  follows:
//
//      bits 24 - 31:   8 bit _major_ version number
//      bits 16 - 23:   8 bit _minor_ version number
//      bits  0 - 15:   16 bit build number
//
//  this is then displayed as follows (in decimal form):
//
//      bMajor = (BYTE)(dwVersion >> 24)
//      bMinor = (BYTE)(dwVersion >> 16) & 
//      wBuild = LOWORD(dwVersion)
//
//  VERSION_ACM_DRIVER is the version of this driver.
//  VERSION_MSACM is the version of the ACM that this driver
//  was designed for (requires).
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 

#define VERSION_ACM_DRIVER  MAKE_ACM_VERSION(0,  1, 0)
#define VERSION_MSACM       MAKE_ACM_VERSION(3, 50, 0)


//
//  for compiling Unicode
//
#ifdef UNICODE
    #define SIZEOF(x)   (sizeof(x)/sizeof(WCHAR))
#else
    #define SIZEOF(x)   sizeof(x)
#endif
#define SIZEOFACMSTR(x)	(sizeof(x)/sizeof(WCHAR))

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  misc defines for misc sizes and things...
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 


#define SIZEOF_ARRAY(ar)            (sizeof(ar)/sizeof((ar)[0]))


//
//  macros to compute block alignment and convert between samples and bytes
//  of PCM data. note that these macros assume:
//
//      wBitsPerSample  =  8 or 16
//      nChannels       =  1 or 2
//
//  the pwf argument is a pointer to a PCMWAVEFORMAT structure.
//
#define PCM_BLOCKALIGNMENT(pwf)     (UINT)(((pwf)->wBitsPerSample >> 3) << ((pwf)->wf.nChannels >> 1))
#define PCM_AVGBYTESPERSEC(pwf)     (DWORD)((pwf)->wf.nSamplesPerSec * (pwf)->wf.nBlockAlign)
#define PCM_BYTESTOSAMPLES(pwf, dw) (DWORD)(dw / PCM_BLOCKALIGNMENT(pwf))
#define PCM_SAMPLESTOBYTES(pwf, dw) (DWORD)(dw * PCM_BLOCKALIGNMENT(pwf))



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 

typedef struct tDRIVERINSTANCE
{
    //
    //  although not required, it is suggested that the first two members
    //  of this structure remain as fccType and DriverProc _in this order_.
    //  the reason for this is that the driver will be easier to combine
    //  with other types of drivers (defined by AVI) in the future.
    //
    FOURCC          fccType;        // type of driver: 'audc'
    DRIVERPROC      fnDriverProc;   // driver proc for the instance

    //
    //  the remaining members of this structure are entirely open to what
    //  your driver requires.
    //
    HDRVR           hdrvr;          // driver handle we were opened with
    HINSTANCE       hinst;          // DLL module handle.
    DWORD           vdwACM;         // current version of ACM opening you
    DWORD           fdwOpen;        // flags from open description

    DWORD           fdwConfig;      // stream instance configuration flags
    
} DRIVERINSTANCE, *PDRIVERINSTANCE, FAR *LPDRIVERINSTANCE;



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 


//
//  Structure used for storing configuration setting.
//  See codec.c for a description of this structure and its use.
//
typedef struct tRATELISTFORMAT
{
    UINT        uFormatType;
    UINT        idsFormat;
    DWORD       dwMonoRate;
} RATELISTFORMAT;
typedef RATELISTFORMAT *PRATELISTFORMAT;

#define CONFIG_RLF_NONUMBER     1
#define CONFIG_RLF_MONOONLY     2
#define CONFIG_RLF_STEREOONLY   3
#define CONFIG_RLF_MONOSTEREO   4


//
//
//
//
typedef LRESULT (FNGLOBAL *STREAMCONVERTPROC)
(
    LPACMDRVSTREAMINSTANCE  padsi,
    LPACMDRVSTREAMHEADER    padsh
);



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  resource id's
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 

#define IDS_ACM_DRIVER_SHORTNAME    (1)     // ACMDRIVERDETAILS.szShortName
#define IDS_ACM_DRIVER_LONGNAME     (2)     // ACMDRIVERDETAILS.szLongName
#define IDS_ACM_DRIVER_COPYRIGHT    (3)     // ACMDRIVERDETAILS.szCopyright
#define IDS_ACM_DRIVER_LICENSING    (4)     // ACMDRIVERDETAILS.szLicensing
#define IDS_ACM_DRIVER_FEATURES     (5)     // ACMDRIVERDETAILS.szFeatures


#define IDS_ERROR                   (30)
#define IDS_ERROR_NOMEM             (31)
#define IDS_CONFIG_NORATES          (32)
#define IDS_CONFIG_ALLRATES         (33)
#define IDS_CONFIG_MONOONLY         (34)


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  resource id's for gsm 610 configuration dialog box
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

#define IDD_CONFIG                      RCID(100)
#define IDC_BTN_AUTOCONFIG              1001
#define IDC_BTN_HELP                    1002
#define IDC_COMBO_MAXRTENCODE           1003
#define IDC_COMBO_MAXRTDECODE           1004

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  global variables, etc...
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

extern const UINT   gauFormatIndexToSampleRate[];
extern const UINT   ACM_DRIVER_MAX_SAMPLE_RATES;
extern const RATELISTFORMAT gaRateListFormat[];
extern const UINT   MSGSM610_CONFIG_NUMSETTINGS;


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//  function prototypes
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

BOOL FNGLOBAL acmdDriverConfigInit
(
    PDRIVERINSTANCE    pdi,
    LPCTSTR            pszAliasName
);

BOOL FNWCALLBACK acmdDlgProcConfigure
(
    HWND                    hwnd,
    UINT                    uMsg,
    WPARAM                  wParam,
    LPARAM                  lParam
);


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ; 

#ifndef RC_INVOKED
#pragma pack()                      // revert to default packing
#endif

#ifdef __cplusplus
}                                   // end of extern "C" { 
#endif

#endif // _INC_CODEC
