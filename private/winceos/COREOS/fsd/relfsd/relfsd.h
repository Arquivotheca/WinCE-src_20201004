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
#ifndef _RELFSD_H
#define _RELFSD_H

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif


#include <windows.h>
#include <tchar.h>
#include <types.h>
#include <excpt.h>
#include <memory.h>
#include <diskio.h>
#include <fsdmgr.h>
#include <fsioctl.h>

#define KITL_MAX_DATA_SIZE 1446


#define ENTER_BREAK_SCOPE switch(TRUE) { case TRUE:
#define LEAVE_BREAK_SCOPE }

#define INVALID_PTR             ((PVOID)0xFFFFFFFF)

#ifdef DEBUG
#define DEBUGONLY(s)            s
#define RETAILONLY(s)           
#define VERIFYTRUE(c)           DEBUGCHK(c)
#define VERIFYNULL(c)           DEBUGCHK(!(c))
#else
#define DEBUGONLY(s)
#define RETAILONLY(s)           s
#define VERIFYTRUE(c)           c
#define VERIFYNULL(c)           c
#endif

#ifndef ERRFALSE
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif


/*  Debug-zone stuff
 */

#ifdef DEBUG


#define DEBUGBREAK(cond)         if (cond) DebugBreak(); else
#define DEBUGMSGBREAK(cond,msg)  if (cond) {DEBUGMSG(TRUE,msg); DebugBreak();} else
#define DEBUGMSGWBREAK(cond,msg) if (cond) {DEBUGMSGW(TRUE,msg); DebugBreak();} else

#else   // !DEBUG

#define DEBUGBREAK(cond)
#define DEBUGMSGBREAK(cond,msg)
#define DEBUGMSGWBREAK(cond,msg)
#endif


#ifdef DEBUG

/*****************************************************************************/

/* debug zones */
#define ZONEID_INIT  0
#define ZONEID_APIS  1
#define ZONEID_ERROR 2
#define ZONEID_CREATE 3

/* zone masks */
#define ZONEMASK_INIT   (1 << ZONEID_INIT)
#define ZONEMASK_APIS   (1 << ZONEID_APIS)
#define ZONEMASK_ERROR  (1 << ZONEID_ERROR)
#define ZONEMASK_CREATE (1 << ZONEID_CREATE)


/* these are used as the first arg to DEBUGMSG */
#define ZONE_INIT   DEBUGZONE(ZONEID_INIT)
#define ZONE_APIS   DEBUGZONE(ZONEID_APIS)
#define ZONE_ERROR  DEBUGZONE(ZONEID_ERROR)
#define ZONE_CREATE DEBUGZONE(ZONEID_CREATE)

/*****************************************************************************/


#endif /* DEBUG_H */

#define _O_RDONLY     0x0000 /* open for reading only */
#define _O_WRONLY     0x0001 /* open for writing only */
#define _O_RDWR       0x0002 /* open for reading and writing */
#define _O_APPEND     0x0008 /* writes done at eof */
#define _O_SEQUENTIAL 0x0020

#define _O_CREAT    0x0100   /* create and open file */
#define _O_TRUNC    0x0200   /* open and truncate */
#define _O_EXCL     0x0400   /* open only if file doesn't already exist */

#define SEEK_SET    0        /* seek to an absolute position */
#define SEEK_CUR    1        /* seek relative to current position */
#define SEEK_END    2        /* seek relative to end of file */


#define CE_SPECIFIC_ATTRIBUTES (FILE_ATTRIBUTE_INROM |        \
                                MODULE_ATTR_NOT_TRUSTED |     \
                                MODULE_ATTR_NODEBUG |         \
                                FILE_ATTRIBUTE_ROMSTATICREF | \
                                FILE_ATTRIBUTE_ROMMODULE )                                                                      

// This is used to mask off file flags for CreateFile
#define CE_ATTRIBUTE_MASK 0xffff

typedef struct
{
    HVOL vs_Handle;
} VolumeState;

typedef struct
{
    int          fs_Handle;
    VolumeState *fs_Volume;
} FileState;

typedef struct {
    int          fs_Handle;
    VolumeState *fs_Volume;
    BYTE    abFindDataBuf [KITL_MAX_DATA_SIZE];
    DWORD   dwFindBytesRemaining;
    BOOL    bFindDone;
    LPBYTE  pFindCurrent;
} SearchState;

extern LPCRITICAL_SECTION g_pcsMain;

/* if this flag is set in the registry: relfsd becomes a read only driver and disallows
*  WriteFile, SetFileTime, Relfsd IOCTLs and CreateFile (except for OPEN_EXISTING). relfsd also
*  does not load dbglist.txt if this flag is set
*/
extern BOOL g_fSecureRelfsd;

#define EnterCriticalSectionMM(pcs) { \
    EnterCriticalSection (pcs); \
}                                   

#define LeaveCriticalSectionMM(pcs) {  \
    LeaveCriticalSection (pcs); \
}   

__inline DWORD 
NtToCEAttributes(DWORD Attr)
{
    return ((Attr == 0xFFFFFFFF) ? 0xFFFFFFFF : (Attr & ~CE_SPECIFIC_ATTRIBUTES));
}

BOOL LoadDbgList (LPCWSTR   pszFile);

#endif /* _RELFSD_H */
