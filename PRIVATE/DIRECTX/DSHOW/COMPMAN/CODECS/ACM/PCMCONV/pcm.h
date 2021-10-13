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
//  pcm.h
//
//  Description:
//
//
//==========================================================================;


//
//  misc. defines
//
//
#define VERSION_CODEC_MAJOR     3
#define VERSION_CODEC_MINOR     50
#define VERSION_CODEC_BUILD     0

#define VERSION_CODEC       MAKE_ACM_VERSION(VERSION_CODEC_MAJOR,   \
                                             VERSION_CODEC_MINOR,   \
                                             VERSION_CODEC_BUILD)

#define ICON_CODEC              RCID(12)

#define MSPCM_MAX_CHANNELS          2           // max channels we deal with


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
#define PCM_BYTESTOSAMPLES(pwf, dw) (DWORD)(dw / PCM_BLOCKALIGNMENT(pwf))
#define PCM_SAMPLESTOBYTES(pwf, dw) (DWORD)(dw * PCM_BLOCKALIGNMENT(pwf))



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;
//
//
//
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ;

#ifndef IDS_MSPCM_TAG
    #define IDS_MSPCM_TAG           0
#endif

#define IDS_CODEC_SHORTNAME         (IDS_MSPCM_TAG+0)
#define IDS_CODEC_LONGNAME          (IDS_MSPCM_TAG+1)
#define IDS_CODEC_COPYRIGHT         (IDS_MSPCM_TAG+2)
#define IDS_CODEC_LICENSING         (IDS_MSPCM_TAG+3)
#define IDS_CODEC_FEATURES          (IDS_MSPCM_TAG+5)

LRESULT pcmDriverProc(
    DWORD                   dwId,
    HACMDRIVERID            hadid,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
    );


//
// DEBUG message stuff
//

//#if 0
#ifdef DEBUG
//
// For debug builds, use the real zones.
//
#define ZONE_TEST       DEBUGZONE(0)
#define ZONE_PARAMS     DEBUGZONE(1)
#define ZONE_VERBOSE    DEBUGZONE(2)
#define ZONE_INTERRUPT  DEBUGZONE(3)
#define ZONE_WODM       DEBUGZONE(4)
#define ZONE_WIDM       DEBUGZONE(5)
#define ZONE_PDD        DEBUGZONE(6)
#define ZONE_MDD        DEBUGZONE(7)
#define ZONE_SPS        DEBUGZONE(8)
#define ZONE_MISC       DEBUGZONE(9)
#define ZONE_ACM        DEBUGZONE(10)
#define ZONE_IOCTL      DEBUGZONE(11)
#define ZONE_ALLOC      DEBUGZONE(12)
#define ZONE_FUNCTION   DEBUGZONE(13)
#define ZONE_WARN       DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)

#define PRINTMSG DEBUGMSG

#else
//
// For retail builds, use forced messages based on the zones turned on below.
//
#define BIG_SWITCH  1
#define ZONE_TEST       BIG_SWITCH
#define ZONE_PARAMS     BIG_SWITCH
#define ZONE_VERBOSE    BIG_SWITCH
#define ZONE_INTERRUPT  BIG_SWITCH
#define ZONE_WODM       BIG_SWITCH
#define ZONE_WIDM       BIG_SWITCH
#define ZONE_PDD        BIG_SWITCH
#define ZONE_MDD        BIG_SWITCH
#define ZONE_SPS        BIG_SWITCH
#define ZONE_MISC       BIG_SWITCH
#define ZONE_ACM        BIG_SWITCH
#define ZONE_IOCTL      BIG_SWITCH
#define ZONE_ALLOC      BIG_SWITCH
#define ZONE_FUNCTION   BIG_SWITCH
#define ZONE_WARN       BIG_SWITCH
#define ZONE_ERROR      1

#define PRINTMSG RETAILMSG

#endif

#ifdef FILELOGGING
#pragma message ("Using the PPLOG functions")
HANDLE pplog_open  (PTCHAR szFilename);
BOOL   pplog_close (HANDLE hLog);
BOOL   pplog_write (HANDLE hLog, DWORD dwCondition, PTCHAR szString);
BOOL   pplog_flush (HANDLE hLog);
#else
#pragma message ("Stubbing the PPLOG functions")
#define pplog_open(x)          0
#define pplog_close(x)         0
#define pplog_write(x,y,z)     0
#define pplog_flush(x)         0
#endif


extern HANDLE hLog;
extern HANDLE hLog2;

#ifdef DEBUG
//-----------------------------------------------------------------------------
// DEBUG MACROS
//
//#define FUNC(x)    PRINTMSG(ZONE_FUNCTION, (TEXT("%a(%d) : [FUNCTION] %a()\r\n"), __FILE__, __LINE__, x))
#ifdef FILELOGGING
#define FUNC(x)       {TCHAR szTmpBuff[256]; wsprintf(szTmpBuff, (TEXT("[FUNC] ")TEXT( #x )TEXT("()\r\n"))); pplog_write(hLog, 1, szTmpBuff);}
#define FUNC_ACM(x)   {TCHAR szTmpBuff[256]; wsprintf(szTmpBuff, (TEXT("[FUNC] ")TEXT( #x )TEXT("()\r\n"))); pplog_write(hLog, 1, szTmpBuff);}
#define FUNC_VERBOSE(x)  {TCHAR szTmpBuff[256]; wsprintf(szTmpBuff, (TEXT("[FUNC] ")TEXT( #x )TEXT("()\r\n"))); pplog_write(hLog, 1, szTmpBuff);}
#else
#define FUNC(x)         PRINTMSG(ZONE_FUNCTION,             (TEXT("[FUNC PCMCONV] %a()\r\n"), x))
#define FUNC_ACM(x)     PRINTMSG(ZONE_FUNCTION || ZONE_ACM, (TEXT("[FUNC PCMCONV] %a()\r\n"), x))
#define FUNC_VERBOSE(x)  PRINTMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("[FUNC] %a()\r\n"), x))
#endif

#define MSG1(x)    PRINTMSG(ZONE_MISC,     (TEXT("%a(%d) : [MSG] %a\r\n"), __FILE__, __LINE__, x))
#define WRNMSG1(x) PRINTMSG(ZONE_WARN,     (TEXT("[WARNING] %a\r\n"), x))
#define TESTMSG(x) PRINTMSG(ZONE_TEST,     (TEXT("[TEST] %a\r\n"), x))

#define ERRMSG(str)            PRINTMSG(ZONE_ERROR, (TEXT("[ERROR] ")TEXT( #str )TEXT("\r\n")))

#define DECVALUE(x) PRINTMSG(ZONE_MISC, (TEXT("\tVALUE : ")TEXT( #x )TEXT(" = %d\r\n"), x))
#define HEXVALUE(x) PRINTMSG(ZONE_MISC, (TEXT("\tVALUE : ")TEXT( #x )TEXT(" = 0x%X\r\n"), x))

#define DECPARAM(x) PRINTMSG(ZONE_FUNCTION && ZONE_PARAMS, (TEXT("\tPARAM : ")TEXT( #x )TEXT(" = %d\r\n"), x))
#define HEXPARAM(x) PRINTMSG(ZONE_FUNCTION && ZONE_PARAMS, (TEXT("\tPARAM : ")TEXT( #x )TEXT(" = 0x%X\r\n"), x))

#else

#define FUNC_ACM(x)				  (0)
#define MSG1(x)                   (0)
#define WRNMSG1(x)                (0)
#define TESTMSG(x)                (0)
#define WARNMSG(x)                (0)
#define INTMSG(x)                 (0)
#define MISC1(str,x1)             (0)
#define MISC2(str,x1, x2)         (0)
#define ERRMSG(str)               (0)
#define ERRMSG1(str, x1)          (0)
#define ERRMSG2(str, x1, x2)      (0)
#define DECVALUE(x)               (0)
#define HEXVALUE(x)               (0)
#define STRVALUE(x)               (0)
#define DECVALUE_ACM(x)           (0)
#define HEXVALUE_ACM(x)           (0)
#define DECVALUEZ(z, x)           (0)
#define HEXVALUEZ(z, x)           (0)
#define DECPARAM(x)               (0)
#define HEXPARAM(x)               (0)
#define FUNC(x)                   (0)
#define FUNC_EXIT()               (0)

#endif

