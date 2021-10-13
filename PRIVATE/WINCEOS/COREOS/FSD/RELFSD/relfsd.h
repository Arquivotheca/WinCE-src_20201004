//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#define KITL_MAX_DATA_SIZE 1446


#ifdef UNDER_CE
#define GetSystemTimeAsFileTime(pft) { \
        SYSTEMTIME st; \
        GetSystemTime(&st); \
        SystemTimeToFileTime(&st, pft); \
}
#endif


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


#define BLOCK_SIZE              512     


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

#define EnterCriticalSectionMM(pcs) { \
    EnterCriticalSection (pcs); \
}                                   

#define LeaveCriticalSectionMM(pcs) {  \
    LeaveCriticalSection (pcs); \
}   

#endif /* _RELFSD_H */
