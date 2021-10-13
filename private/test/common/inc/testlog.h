//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Module Name:

    testlog.h

Abstract:

    This module provides the core services of the logging
    services library.

Author:

    Hon Keat Chan (honkeatc) 4-Jan-1995

--*/


#ifndef  TESTLOG_h
#define  TESTLOG_h

#include <wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Definitions for logging.
//
#define LOG_CON     0x01    // output to console.
#define LOG_RES     0x02    // hi-level log of tests.
#define LOG_SUM     0x04    // lo-level log.
#define LOG_TWIN    0x08    // comm between test pieces on host and target.
                            // app defined protocol.
#define LOG_FSAUXIN 0x10    // FSAuxin file for PPFS
#define LOG_DBG      0x20    // FSAuxin file for PPFS

#define LOG_CS    (LOG_CON | LOG_SUM)
#define LOG_CR    (LOG_CON | LOG_RES)
#define LOG_SR    (LOG_SUM | LOG_RES)
#define LOG_CSR   (LOG_CON | LOG_SUM | LOG_RES)

/* extern int      LSDebugLevel; */

extern void     LSInit( void );
extern void     LSDeInit( void );
extern BOOL     LSConfig(LPTSTR pszConfig);
extern BOOL     LSConfigEx(LPTSTR pszConfig);
extern void     LSClose(void);
extern void     LSDelChannel(int ChanID);
extern int      LSNewChannel(LPTSTR pszName);
extern BOOL     LSDebug(int Level, LPCTSTR szFmt, ...);
extern BOOL     LSDebugEx(int Level, LPCTSTR szFmt, ...);
extern BOOL     LSTrace( LPCTSTR lpszFormat, ... );
extern int      GetLSDebugLevel(void);
extern void     SetLSDebugLevel( int Level );
extern BOOL     LSPrintf(int Channel, LPCTSTR szFmt, ...);
extern BOOL     LSvPrintf( int ChannelID, LPCTSTR szFmt, va_list vArgs );
extern int      LSReadBin(int ChanID, PBYTE pBuf, int cLen);
extern BOOL     LSWriteBin(int ChanID, PBYTE pBuf, int cLen);
extern BOOL     LSSeek( int ChannelID, long offset, int mode );
extern void     LSAssertPrint(LPSTR lpszFile, int iLine, LPSTR lpszCause);



#define LSAssert(x)   if ( x ) /* nothing */; \
                      else {LSAssertPrint(__FILE__,__LINE__,#x), DebugBreak();}


// LSWriteSZ: Write free format data, allowing caller to specify 
//      both MBS and Unicode data (should by analogous).
extern BOOL LSWriteSZ(int ChanID, PTSTR ptsz, LPSTR psz, int cbMBSLen);


                      
#ifdef __cplusplus
}


#endif


#endif  // TESTLOG_h
