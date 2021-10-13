//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    fatfs.h

Abstract:

    This file contains internal definitions for the WINCE FAT file system
    implementation.

    Unlike FILESYS.EXE (WINCE's RAM-based object store), one of FATFS'
    goals is to be as re-entrant as possible, so that while one thread is
    blocked waiting for I/O, other threads can still perform FATFS operations.
    The rationale for this is that I/O can, in terms of CPU cycles, take an
    extremely long time, and many types of calls do not need to perform any
    I/O to complete (usually because the data they need is in the buffer pool).

Revision History:

Compilation options:

    BIG_BUFFERS - Ths enables a preferred buffer size equal to the
    operating system's page size.  Currently, the preferred buffer
    size is equal to the sector size of the media being mounted (just
    like real-mode MS-DOS).  Note that if you enable this option, you
    may uncover some bugs (it hasn't been tested a lot this way).

    DEMAND_PAGING - This enables support for demand-paging and the
    associated services (in v2.0, that was ReadFilePagein;  post-v2.0,
    they are ReadFileWithSeek and WriteFileWithSeek).

    DEMAND_PAGING_DEADLOCKS - This enabled code to deal with some
    dead-lock consequences if/when the kernel called our demand-paging
    functions with a variety of critical sections held.  Unfortunately,
    there were still a number caveats we couldn't really address, but
    post-v2.0, the kernel has stopped holding critical sections across
    demand-paging requests, so this code should be moot now.

    FAT32 - This enables support for FAT32 volumes.

    PATH_CACHING - This enables path-caching code, which remembers up
    to 4 recently-used paths per volume (see MAX_CACHE_PER_VOLUME).  This
    code makes a HUGE difference in certain scenarios (most notably our
    tests that create and destroy large directory structures), and does
    not significantly impact other scenarios.

    SHELL_MESSAGE_NOTIFICATION - This enables RegisterFileSystemNotification.
    The function is always present; this simply determines whether the
    underlying code is present or if the function should always return FALSE.
    v1.x was essentially built with SHELL_MESSAGE_NOTIFICATION defined.

    SHELL_CALLBACK_NOTIFICATION - This enables RegisterFileSystemFunction.
    The function is always present; this simply determines whether the
    underlying code is present or if the function should always return FALSE.
    SHELL_CALLBACK_NOTIFICATION is a new notification mechanism for v2.x.

--*/

#ifndef FATFS_H
#define FATFS_H

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifndef INCLUDE_FATFS

#include <windows.h>
#include <tchar.h>

#if defined(UNDER_CE)
//
//  Define CE compilation options here instead of in the 'sources' files,
//  to insure component compilation consistency (aka CCC)
//
#define DEMAND_PAGING
#define FAT32

#define PATH_CACHING
#define SHELL_CALLBACK_NOTIFICATION
#define DISK_CACHING

#define ENABLE_FAT0_CACHE
#define ENABLE_FAT1_CACHE

// #define DEMAND_PAGING_DEADLOCKS

#define USE_FATUTIL
#define HIDDEN_TFAT_ROOT_DIR  TEXT("__TFAT_HIDDEN_ROOT_DIR__")
#define HIDDEN_TFAT_ROOT_DIR_LEN 24

#define FROZEN_CLUS_CACHE_SIZE  16*1024

#include <types.h>

#else
#include <sys\types.h>
#endif

#include <excpt.h>
#include <memory.h>
#include <diskio.h>
#include <fsdmgr.h>
#include <fatutil.h>

#ifdef PVOLUME
#undef PVOLUME
#endif

//#include <windev.h>
//I'm currently unable to include windev.h (which is where the
//prototype for this function appears to reside) because of the include
//file #define conflicts (DEVICE_TYPE is being multiply-defined in
//apparently different ways), so I'm simply importing the prototype here for now. -JTP
DWORD GetDeviceKeys(LPCTSTR DevName, LPTSTR ActiveKey, LPDWORD lpActiveLen, LPTSTR DriverKey, LPDWORD lpDriverLen);

#define TEXTW(s)                L##s
#define ARRAYSIZE(a)            (sizeof(a)/sizeof(a[0]))
#define DBGPRINTF               NKDbgPrintfW
#define DBGPRINTFW              NKDbgPrintfW
#define DBGSCANF
#define DBGSCANFW
#ifndef CRLF
#define DBGTEXT(fmt)            TEXT(fmt)
#define DBGTEXTW(fmt)           TEXTW(fmt)
#else
#define DBGTEXT(fmt)            TEXT(fmt) TEXT("\r")
#define DBGTEXTW(fmt)           TEXTW(fmt) TEXTW("\r")
#endif
#define DBGTEXTNOCRLF(fmt)      TEXT(fmt)
#define DBGTEXTWNOCRLF(fmt)     TEXTW(fmt)
#define DEBUGMSGW               DEBUGMSG
#ifndef ASSERT
#define ASSERT(c)               DEBUGCHK(c)
#endif
#endif  // !INCLUDE_FATFS


/*  General-purpose macros (that really should be elsewhere)
 */

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

#if defined(XREF_CPP_FILE)
#define ERRFALSE(exp)
#elif !defined(ERRFALSE)
//  This macro is used to trigger a compile error if exp is false.
//  If exp is false, i.e. 0, then the array is declared with size 0, triggering a compile error.
//  If exp is true, the array is declared correctly.
//  There is no actual array however.  The declaration is extern and the array is never actually referenced.
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif

#ifdef DEBUG
#define DEBUGALLOC_CS           3       // arbitrary value for tracking InitializeCriticalSection "allocations"
#define DEBUGALLOC_EVENT        5       // arbitrary value for tracking CreateEvent "allocations"
#define DEBUGALLOC_API          7       // arbitrary value for tracking CreateAPISet "allocations"
#define DEBUGALLOC_HANDLE       11      // arbitrary value for tracking CreateFile "allocations"
#define DEBUGALLOC(cb)          { \
        EnterCriticalSection(&csAlloc); \
        cbAlloc += (cb); \
        DEBUGMSG(ZONE_MEM,(DBGTEXT("FATFS!alloc(0x%x)\n"), cb)); \
        LeaveCriticalSection(&csAlloc); \
}
#define DEBUGFREE(cb)           { \
        EnterCriticalSection(&csAlloc); \
        cbAlloc -= (cb); \
        DEBUGMSG(ZONE_MEM,(DBGTEXT("FATFS!free(0x%x)\n"), cb)); \
        LeaveCriticalSection(&csAlloc); \
}
#else
#define DEBUGALLOC(cb)
#define DEBUGFREE(cb)
#endif


/*  Debug-zone stuff
 */

#ifdef DEBUG

#define ZONEID_INIT             0
#define ZONEID_ERRORS           1
#define ZONEID_SHELLMSGS        2
#define ZONEID_TFAT				3
#define ZONEID_MEM              4
#define ZONEID_APIS             5
#define ZONEID_MSGS             6
#define ZONEID_STREAMS          7
#define ZONEID_BUFFERS          8
#define ZONEID_CLUSTERS         9
#define ZONEID_FATIO            10
#define ZONEID_DISKIO           11
#define ZONEID_LOGIO            12
#define ZONEID_READVERIFY       13
#define ZONEID_WRITEVERIFY      14
#define ZONEID_PROMPTS          15

#define ZONEMASK_INIT           (1 << ZONEID_INIT)
#define ZONEMASK_ERRORS         (1 << ZONEID_ERRORS)
#define ZONEMASK_SHELLMSGS      (1 << ZONEID_SHELLMSGS)
#define ZONEMASK_TFAT			(1 << ZONEID_TFAT)
#define ZONEMASK_MEM            (1 << ZONEID_MEM)
#define ZONEMASK_APIS           (1 << ZONEID_APIS)
#define ZONEMASK_MSGS           (1 << ZONEID_MSGS)
#define ZONEMASK_STREAMS        (1 << ZONEID_STREAMS)
#define ZONEMASK_BUFFERS        (1 << ZONEID_BUFFERS)
#define ZONEMASK_CLUSTERS       (1 << ZONEID_CLUSTERS)
#define ZONEMASK_FATIO          (1 << ZONEID_FATIO)
#define ZONEMASK_DISKIO         (1 << ZONEID_DISKIO)
#define ZONEMASK_LOGIO          (1 << ZONEID_LOGIO)
#define ZONEMASK_READVERIFY     (1 << ZONEID_READVERIFY)
#define ZONEMASK_WRITEVERIFY    (1 << ZONEID_WRITEVERIFY)
#define ZONEMASK_PROMPTS        (1 << ZONEID_PROMPTS)

#define ZONE_INIT               DEBUGZONE(ZONEID_INIT)
#define ZONE_ERRORS             DEBUGZONE(ZONEID_ERRORS)
#define ZONE_SHELLMSGS          DEBUGZONE(ZONEID_SHELLMSGS)
#define ZONE_TFAT				DEBUGZONE(ZONEID_TFAT)
#define ZONE_MEM                DEBUGZONE(ZONEID_MEM)
#define ZONE_APIS               DEBUGZONE(ZONEID_APIS)
#define ZONE_MSGS               DEBUGZONE(ZONEID_MSGS)
#define ZONE_STREAMS            DEBUGZONE(ZONEID_STREAMS)
#define ZONE_BUFFERS            DEBUGZONE(ZONEID_BUFFERS)
#define ZONE_CLUSTERS           DEBUGZONE(ZONEID_CLUSTERS)
#define ZONE_FATIO              DEBUGZONE(ZONEID_FATIO)
#define ZONE_DISKIO             DEBUGZONE(ZONEID_DISKIO)
#define ZONE_LOGIO              DEBUGZONE(ZONEID_LOGIO)
#define ZONE_READVERIFY         DEBUGZONE(ZONEID_READVERIFY)
#define ZONE_WRITEVERIFY        DEBUGZONE(ZONEID_WRITEVERIFY)
#define ZONE_PROMPTS            DEBUGZONE(ZONEID_PROMPTS)

#ifndef ZONEMASK_DEFAULT
#define ZONEMASK_DEFAULT        (ZONEMASK_INIT|ZONEMASK_ERRORS|ZONEMASK_SHELLMSGS)
#endif

#define DEBUGBREAK(cond)         if (cond) DebugBreak(); else
#define DEBUGMSGBREAK(cond,msg)  if (cond) {DEBUGMSG(TRUE,msg); DebugBreak();} else
#define DEBUGMSGWBREAK(cond,msg) if (cond) {DEBUGMSGW(TRUE,msg); DebugBreak();} else

#else   // !DEBUG

#define DEBUGBREAK(cond)
#define DEBUGMSGBREAK(cond,msg)
#define DEBUGMSGWBREAK(cond,msg)

#define ZONE_INIT               FALSE
#define ZONE_ERRORS             FALSE
#define ZONE_SHELLMSGS          TRUE
#define ZONE_TFAT               FALSE
#define ZONE_MEM                FALSE
#define ZONE_APIS               FALSE
#define ZONE_MSGS               FALSE
#define ZONE_STREAMS            FALSE
#define ZONE_BUFFERS            FALSE
#define ZONE_CLUSTERS           FALSE
#define ZONE_FATIO              FALSE
#define ZONE_DISKIO             FALSE
#define ZONE_LOGIO              FALSE
#define ZONE_READVERIFY         FALSE
#define ZONE_WRITEVERIFY        FALSE
#define ZONE_PROMPTS            FALSE

#endif  // !DEBUG


#include <fatapi.h>


#define CloseAPIHandle(h)       CloseHandle(h)
#define CloseFileHandle(pfh)    CloseHandle(pfh->fh_h)
#define CloseFindHandle(psh)    CloseHandle(psh->sh_h)
#define FAT_CloseFileHandle(h)  CloseHandle(h)
#define FAT_CloseFindHandle(h)  CloseHandle(h)


#ifdef DEBUG
#ifdef UNDER_CE
#define OWNCRITICALSECTION(cs)  ((cs)->LockCount > 0 && (DWORD)(cs)->OwnerThread == GetCurrentThreadId())
#else
#define OWNCRITICALSECTION(cs)  TRUE    
#endif
#else
#define OWNCRITICALSECTION(cs)  TRUE
#endif

/*  All internal file system operations are performed in fixed size blocks.
 *  Only the low level disk access routines care about the true sector size
 *  on the device.
 */

#define BLOCK_SIZE              512     
#define BLOCK_LOG2              9
#define BLOCK_OFFSET_MASK       (BLOCK_SIZE - 1)
#define BYTESTOBLOCKS(b)        (((b)+BLOCK_SIZE-1)>>BLOCK_LOG2)

#if ((1 << BLOCK_LOG2) != BLOCK_SIZE)
#error  Block size inconsistency
#endif

typedef struct _DLINK DLINK, *PDLINK;
typedef struct _BUF BUF, *PBUF;
typedef struct _DSK DSK, *PDSK;
typedef struct _PARTINFO PARTINFO, *PPARTINFO, *PPI;
typedef struct _CACHE CACHE, *PCACHE;
typedef struct _VOLUME VOLUME, *PVOLUME;
typedef struct _DSTREAM DSTREAM, *PDSTREAM;
typedef struct _FHANDLE FHANDLE, *PFHANDLE;
typedef struct _SHANDLE SHANDLE, *PSHANDLE;

#define FS_DECL(type, api, args) extern type FAT_ ## api args
#define FS_HANDLE PVOLUME pvol

#include <extfile.h>
#include <fatui.h>

#include "fatfmt.h"
#include "dosbpb.h"
#include "bootsec.h"
#include "partiton.h"


/*  Doubly linked lists:
 *
 *  Should be the first element of structure being linked.  It may be used
 *  as the head of a list anywhere in a structure which contains the list.
 *
 *  NOTE: the multiple definitions of the basic DLINK structure are here
 *  simply to provide additional information to the debugger regarding what
 *  kind of structure each link in a particular DLINK points to.
 */

struct _DLINK {
    PDLINK      next;           // ptr to next item in list
    PDLINK      prev;           // ptr to previous item in list
};

typedef struct _BUF_DLINK {
    PBUF        pbufNext;       // ptr to next item in list
    PBUF        pbufPrev;       // ptr to previous item in list
} BUF_DLINK, *PBUF_DLINK;

typedef struct _DSK_DLINK {
    PDSK        pdskNext;       // ptr to next item in list
    PDSK        pdskPrev;       // ptr to previous item in list
} DSK_DLINK, *PDSK_DLINK;

typedef struct _PI_DLINK {
    PPARTINFO   ppiNext;        // ptr to next item in list
    PPARTINFO   ppiPrev;        // ptr to previous item in list
} PI_DLINK, *PPI_DLINK;

typedef struct _CCH_DLINK {
    PCACHE      pcchNext;       // ptr to next item in list
    PCACHE      pcchPrev;       // ptr to previous item in list
} CCH_DLINK, *PCCH_DLINK;

typedef struct _VOL_DLINK {
    PVOLUME     pvolNext;       // ptr to next item in list
    PVOLUME     pvolPrev;       // ptr to previous item in list
} VOL_DLINK, *PVOL_DLINK;

typedef struct _STM_DLINK {
    PDSTREAM    pstmNext;       // ptr to next item in list
    PDSTREAM    pstmPrev;       // ptr to previous item in list
} STM_DLINK, *PSTM_DLINK;

typedef struct _FH_DLINK {
    PFHANDLE    pfhNext;        // ptr to next item in list
    PFHANDLE    pfhPrev;        // ptr to previous item in list
} FH_DLINK, *PFH_DLINK;

typedef struct _SH_DLINK {
    PSHANDLE    pshNext;        // ptr to next item in list
    PSHANDLE    pshPrev;        // ptr to previous item in list
} SH_DLINK, *PSH_DLINK;


/*  Global flags
 */
#define FATFS_UPDATE_ACCESS         0x00000001  // update access times if set
#define FATFS_DISABLE_LOG           0x00000002  // disable event logging if set
#define FATFS_DISABLE_AUTOSCAN      0x00000004  // disable automatic ScanVolume()
#define FATFS_VERIFY_WRITES         0x00000008  // verify all writes (as opposed to a handful)
#define FATFS_ENABLE_BACKUP_FAT     0x00000010  // add a backup FAT to all formats
#define FATFS_FORCE_WRITETHROUGH    0x00000020  // Force fat to be always writethrough
#define FATFS_DISABLE_AUTOFORMAT    0x00000040  // disable automatic formatting of unformatted volumes

#define FATFS_WFWS_NOWRITETHRU      0x00010000  // disable writethrough on WriteFileWithSeek API, improve memory-mapped file performance
#define FATFS_DISABLE_FORMAT        0x00020000  // disable format
#define FATFS_TRANS_DATA            0x00040000  // transact data on a write (i.e. clone cluster on every write)
#define FATFS_TFAT_NONATOMIC_SECTOR 0x00080000  // Use cluster 1 entry in FAT table for TFAT transaction, since sector writes are non-atomic.
												// By default, TFAT uses the NOF field of the boot sector
#define FATFS_DISABLE_TFAT_REDIR    0x00100000  // Indicates to disable redirect the root directory to another hidden director
                                                                                   // for FAT12 or 16, since root dir isn't transacted in those cases
#define FATFS_TFAT_ALWAYS           0x00200000  // Always mark transaction status, even only one sector in FAT is changed
#define FATFS_FORCE_TFAT            0x00400000  // Force TFAT transactioning even if volume isn't formatted as TFAT
#define FATFS_LFN_EXTENDED          0x00800000  // Generate LFN entries for extended characters always
#define FATFS_TFAT_DISABLE_MOVEDIR  0x01000000  // Disable MoveFile on a directory for TFAT because it isn't transaction-safe

#ifdef TFAT
#define FATFS_REGISTRY_FLAGS        (FATFS_UPDATE_ACCESS |      \
                                     FATFS_DISABLE_LOG |        \
                                     FATFS_DISABLE_AUTOSCAN |   \
                                     FATFS_VERIFY_WRITES |      \
                                     FATFS_ENABLE_BACKUP_FAT |  \
                                     FATFS_FORCE_WRITETHROUGH | \
                                     FATFS_DISABLE_AUTOFORMAT | \
                                     FATFS_DISABLE_FORMAT | \
                                     FATFS_TFAT_ALWAYS)
#else
#define FATFS_REGISTRY_FLAGS        (FATFS_UPDATE_ACCESS |      \
                                     FATFS_DISABLE_LOG |        \
                                     FATFS_DISABLE_AUTOSCAN |   \
                                     FATFS_VERIFY_WRITES |      \
                                     FATFS_ENABLE_BACKUP_FAT |  \
                                     FATFS_FORCE_WRITETHROUGH | \
                                     FATFS_DISABLE_AUTOFORMAT | \
                                     FATFS_TFAT_ALWAYS)
#endif


/*  Buffer structure:
 *
 *  This structure describes a buffer of disk data.
 */

// values for b_flags:
#define BUF_CARVED              0x01    // buffer is carved from large VirtualAlloc
#define BUF_UNCERTAIN           0x02    // buffer is in transition (ie, is being read)
#define BUF_BUSY                0x04    
#define BUF_ERROR               0x40    // buffer is uncommitable (currently also implies dirty)
#define BUF_DIRTY               0x80    // buffer is dirty

// values for b_blk:
#define INVALID_BLOCK           0xFFFFFFFF

struct _BUF {
    BUF_DLINK   b_dlink;        // preferred location for dlinks (at offset 0)
    PVOLUME     b_pvol;         // pointer to VOLUME buffer belongs to, if any
    DWORD       b_blk;          // block #
    PBYTE       b_pdata;        // pointer to disk data
    PDSTREAM    b_pstm;         // pointer to stream holding this buffer, if any
    BYTE        b_hold;         // non-zero if buffer is being held
    BYTE        b_flags;        // eg, BUF_DIRTY
#ifdef DEBUG
    DWORD       b_refs;         // usage count for this buffer (DEBUG only)
    BYTE        b_achName[OEMNAMESIZE+1];
#endif
};


/*  Disk structure:
 *
 *  This structure describes a disk device, which has one or more VOLUMEs
 *  associated with it.
 */

#ifdef  UNDER_CE
#define MAX_DISK_PATH           16
#else
#define MAX_DISK_PATH           MAX_PATH    // enough room for a fully-qualified disk image filename
#endif

// values for d_flags:
#define DSKF_NONE               0x00000000
#define DSKF_FROZEN             0x00000004  // disk has been frozen (but not freed)
#define DSKF_REMOUNTED          0x00000008  // disk has been remounted
#define DSKF_RECYCLED           0x00000010  // disk has been recycled (like remounted but worse)
#define DSKF_READONLY           0x00080000  // disk is read-only (eg, write-protected)
#define DSKF_SENDDELETE         0x00100000  // disk driver needs to be sent IOCTL_DISK_DELETE_SECTOR

// private flags in DISK_INFO.di_flags (reserved for use by FATFS)
#define DISK_INFO_FLAGS_FATFS_RESERVED      0xf0000000
#define DISK_INFO_FLAGS_FATFS_SIMULATED     0x10000000
#define DISK_INFO_FLAGS_FATFS_NEW_IOCTLS    0x80000000

struct _DSK {
    DSK_DLINK   d_dlOpenDisks;          // list of open disks in system
    PVOLUME     pVol;                   // Points to the volume that is mounted
    PI_DLINK    d_dlPartitions;         // list of partitions for this disk
    DWORD       d_csecUnused;           // largest contiguous chunk of unpartitioned space (in sectors)
    DWORD       d_csecTotalUnused;      // total unpartitioned space (in sectors)
    HANDLE      d_hdsk;                 // device driver handle to read/write disk
    DWORD       d_flags;                // see DSKF_*
    DISK_INFO   d_diActive;             // active drive geometry for disk
    DWORD       d_cwName;               // actual number of characters allocated in d_wsName (including NULL)
    WCHAR       d_wsName[MAX_PATH];     // name of disk device
};


/*  Partition structure:
 *
 *  This structure describes a partition on a disk device.
 */

// values for pi_flags:
#define PIF_NONE                0x0000

struct _PARTINFO {
    PI_DLINK    pi_dlPartitions;// link in partition list
    PI_DLINK    pi_dlChildren;  // list of child partitions, if any
    PPARTINFO   pi_ppiParent;   // pointer back to parent partition, if this is a child partition
    DWORD       pi_dwSig;       // set to BOOTSECVOLUMESIG
    PVOLUME     pi_pvol;        // points to mounted VOLUME if one exists
    PDSK        pi_pdsk;        // DSK the partition resides on
    DWORD       pi_secPartTable;// sector of partition table
    WORD        pi_idxPartTable;// index in partition table
    WORD        pi_flags;       // see PIF_*
    PARTENTRY   pi_PartEntry;   // copy of partition table entry at that index
    DWORD       pi_secAbsolute; // absolute starting sector of partition
    PWSTR       pi_pwsName;     // pointer to specific name to be associated with this partition, NULL if none
};


/*  Partition search structure:
 *
 *  This structure describes the state of a partition search in progress.
 */

struct _PARTSRCH {
    DWORD       ps_csecMinimum; // minimum requested sectors
    DWORD       ps_csecDesired; // desired requested sectors
    PPI_DLINK   ps_pdlFound;    // pointer to head of partition list containing available sectors
    PPARTINFO   ps_ppiFound;    // pointer to PARTINFO on above list that follows available sectors
    DWORD       ps_csecFound;   // number of available sectors
    DWORD       ps_csecTotalFound;      // total of all available sectors found
};
typedef struct _PARTSRCH PARTSRCH, *PPARTSRCH;


/*  Cache structure:
 *
 *  This structure describes a cache entry.  Entries are linked onto a
 *  volume's cache chain.  The volume's cache critical section should be taken
 *  whenever an entry is created, destroyed, or searched on that volume's cache
 *  chain.
 *
 *  Currently, there is only one kind of entry: CACHE_PATH.  When an entry
 *  is created, an additional ref count is applied to the stream corresponding
 *  to that path;  similarly, that stream's ref count is reduced when the cache
 *  entry is destroyed (or reused).
 *
 *  Like SHANDLEs, when a CACHE structure is allocated, c_awcPath is not really
 *  MAX_PATH WCHARs big.  It is allocated only to the length stored in c_cwPath.
 */

#define MAX_CACHE_PER_VOLUME    50     // max cache entries per volume

#define FAT_CACHE_SECTORS       2       // number of cache FAT sectors

// values for c_flags:
#define CACHE_PATH              0x01    // path cache entry

struct _CACHE {
    CCH_DLINK   c_dlink;        // preferred location for dlinks (at offset 0)
    PDSTREAM    c_pstm;         // pointer to stream
    BYTE        c_flags;        // see CACHE_*
    BYTE        c_reserved;     // not used (for padding only)
    WORD        c_cwPath;       // # characters in c_awcPath (includes room for NULL)
#ifdef DEBUG
    DWORD       c_cbAlloc;      // used to track size of this allocation only
#endif
    WCHAR       c_awcPath[MAX_PATH];
};


/*  File system volume structure:
 *
 *  This structure is used by the file system to hold specific volume
 *  information. E.G.: volume size, block to sector conversions, size of the
 *  FAT, the root directory file, etc.
 */

#define MAX_VOLUMES             99

// values for v_flags:
#define VOLF_NONE               0x00000000
#define VOLF_INVALID            0x00000001      // volume is invalid (eg, unformatted)
#define VOLF_UNMOUNTED          0x00000002      // volume has been unmounted
#define VOLF_FROZEN             DSKF_FROZEN     // volume has been frozen (but not freed)
#define VOLF_REMOUNTED          DSKF_REMOUNTED  // volume has been remounted
#define VOLF_RECYCLED           DSKF_RECYCLED   // volume has been recycled (like remounted but worse)
#define VOLF_READLOCKED         0x00000020      // volume has been locked for read access (can't be written by others)
#define VOLF_WRITELOCKED        0x00000040      // volume has been locked for write access (can't be read *or* written by others)
#define VOLF_LOCKED             (VOLF_READLOCKED | VOLF_WRITELOCKED)
#define VOLF_FORMATTING         0x00000080      // volume is being (re)formatted
#define VOLF_12BIT_FAT          0x00000100      // 12-bit FAT
#define VOLF_16BIT_FAT          0x00000200      // 16-bit FAT
#define VOLF_32BIT_FAT          0x00000400      // 32-bit FAT
#define VOLF_BACKUP_FAT         0x00000800      // one or more additional backup FATs exist
#define VOLF_MODDISKINFO        0x00001000      // volume DISK_INFO has been modified
#define VOLF_BUFFERED           0x00004000      // volume is buffered
#define VOLF_RETAIN             0x00008000      // volume should be retained across an unmount
#define VOLF_UNCERTAIN          0x00010000      // volume state is uncertain (do not assume unformatted!)
#define VOLF_DIRTY              0x00020000      // volume has dirty buffers pending
#define VOLF_CLOSING            0x00040000      // volume being closed, discard all open handles/dirty buffers
#define VOLF_READONLY           DSKF_READONLY   // volume is read-only (eg, write-protected)
#define VOLF_SCANNING           0x00100000      // volume is being scanned for inconsistencies
#define VOLF_MODIFIED           0x00200000      // volume has been modified (ie, 1 or more volume writes)
#define VOLF_LOGINIT            0x00400000      // volume has been "logging enabled"
#define VOLF_INITCOMPLETE       0x00800000      // volume has been fully initialized (no need to scan, etc)
#define VOLF_UPDATE_ACCESS      0x01000000      // per-volume version of FATFS_UPDATE_ACCESS
#define VOLF_DIRTY_WARN         0x02000000      // per-volume dirty data warning flag
#define VOLF_TFAT_REDIR_ROOT         0x04000000  // Indicates to redirect the root directory to another hidden director
                                                                                   // for FAT12 or 16, since root dir isn't transacted in those cases
                                                                                   
#define VOLF_TFAT_CREATING_ROOT         0x08000000  
                                                                                   

#ifdef FAT32
#define VOLF_UNSUPPORTED    (VOLF_UNCERTAIN)
#else
#define VOLF_UNSUPPORTED    (VOLF_32BIT_FAT | VOLF_UNCERTAIN)
#endif

typedef DWORD UNPACKFN(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData);
typedef DWORD PACKFN(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusDataPrev);

struct _VOLUME {
    HVOL        v_hVol;
    VOL_DLINK   v_dlOpenVolumes;// list of open volumes on dsk
    STM_DLINK   v_dlOpenStreams;// list of DSTREAMs for this volume
#ifdef PATH_CACHING
    CCH_DLINK   v_dlCaches;     // list of CACHEs for this volume
#endif
    PDSK        v_pdsk;         // DSK the VOLUME is open on
    PPARTINFO   v_ppi;          // pointer to partition info, if any
    DWORD       v_flags;        // see VOLF_*
    signed char v_volID;        // file system volume ID (aka AFS index), INVALID_AFS if none
    DWORD       v_cCaches;      // # cache entries currently attached
    DWORD       v_cMaxCaches;   // Max number of path cache entries
    //
    //  Beginning of portion reset by InitVolume for INVALID volumes
    //
    BYTE        v_bMediaDesc;   // media descriptor from BPB
    BYTE        v_bVerifyCount; // number of read-after-write verifies left to perform
    DWORD       v_serialID;     // volume serial # and label from boot sector
    BYTE        v_label[OEMNAMESIZE+1];
    BYTE        v_log2cbSec;    // log2(bytes per sector)
    BYTE        v_log2cblkSec;  // log2(blocks per sector)
    BYTE        v_log2csecClus; // log2(sectors per cluster)
    BYTE        v_log2cblkClus; // log2(blocks per cluster)
    DWORD       v_secVolBias;   // sector bias for volume (ie, reserved sectors)
    DWORD       v_secBlkBias;   // sector bias for block 0 (ie, reserved + hidden sectors)
    DWORD       v_blkClusBias;  // block bias for cluster 0 (ie, # blocks in FAT + root dir)
    DWORD       v_cbClus;       // bytes per cluster
    DWORD       v_clusEOF;      // cluster EOF threshold
    DWORD       v_clusMax;      // maximum cluster # for volume (starting w/DATA_CLUSTER)
    DWORD       v_clusAlloc;    // last cluster alloc'ed, -1 if none yet
    DWORD       v_cclusFree;    // # of free clusters, -1 if not known yet
    DWORD       v_csecFAT;      // sectors per FAT
    DWORD       v_secEndFAT;    // ending sector # (+1) of active FAT
    DWORD       v_secEndAllFATs;// ending sector # (+1) of all FATs
    DWORD       v_csecUsed;     // total sectors used in FAT and root
    DWORD       v_csecTotal;    // total sectors on volume
#ifdef FAT32
    DWORD       v_secFSInfo;    // as obtained from BIGFATBPB->BGBPB_FSInfoSec
#endif
    UNPACKFN    *v_unpack;      // FAT unpack function
    PACKFN      *v_pack;        // FAT pack function
    //
    //  End of portion reset by InitVolume for INVALID volumes
    //
    PDSTREAM    v_pstmFAT;      // stream used to read/write FAT
    PDSTREAM    v_pstmRoot;     // stream used to read/write root directory
    PWSTR       v_pwsHostRoot;  // pointer to local name of root (NULL if none)
    DWORD       v_cwsHostRoot;  // length (in WCHARs) of local root name
    CRITICAL_SECTION v_cs;      // critical section for volume
    CRITICAL_SECTION v_csStms;  // critical section for OpenStreams list
#ifdef PATH_CACHING
    CRITICAL_SECTION v_csCaches;// critical section for Caches list
#endif
#ifdef DISK_CACHING
    LPBYTE      v_pFATCacheBuffer;  // Disk Cache Buffer    
    LPDWORD     v_pFATCacheLookup;  // Lookup buffer for Cache
    LPBYTE      v_pFATCacheLRU;  	// LRU for Cache
    DWORD       v_CacheSize;        // Size of the cache
    DWORD       v_MaxCachedFileSize;      // Uncache threshold
    CRITICAL_SECTION v_csFATCache;  // Disk Cache critical section

#ifdef ENABLE_FAT0_CACHE
    BYTE    	v_aFAT0CacheBuffer[DEFAULT_SECTOR_SIZE];
    DWORD    	v_dwFAT0CacheLookup;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
    BYTE    	v_aFAT1CacheBuffer[DEFAULT_SECTOR_SIZE];
    DWORD    	v_dwFAT1CacheLookup;
#endif // ENABLE_FAT1_CACHE

#ifdef DEBUG
    DWORD		dwCacheHit;
    DWORD		dwCacheMiss;
#endif

#endif
    DWORD       v_flFATFS;          // Per Volume FAT Flags
    UINT        v_nCodePage;        // Codepage - Default CP_OEMCP , can be overridden by registry entry CODEPAGE=
    DWORD       v_dwMaxPathCache;       // max cache entries per volume

#ifdef TFAT
    DWORD  v_clusFrozenHead;	  // The head of frozen cluster list

    struct
    {
        DWORD     dwSize;
        LPBYTE      lpBits;				// A bit array to hold dirty clusters in FAT;
    } v_DirtySectorsInFAT;

    // cache some FAT sectors for performance, the buffer is also used for sync fat
    BYTE    v_cFATBuffer[DEFAULT_SECTOR_SIZE * FAT_CACHE_SECTORS];
    DWORD	v_secFATBufferBias;	    // sector bias of first FAT0 sector cached
    LPBYTE  v_ClusBuf;              // A buffer for cloning clusters

    // Keep a copy of boot sector
    union                           
    {
        BIGFATBOOTSEC   v_bgbs;
        BYTE            v_cbs[DEFAULT_SECTOR_SIZE]; 
    };

    BOOL v_fTfat;
    LPDWORD v_pFrozenClusterList;  // cache used to keep track of frozen clusters
#endif									


};

#define NO_CLUSTER              0
#define FREE_CLUSTER            NO_CLUSTER
#define FAT_PSEUDO_CLUSTER      0
#define ROOT_PSEUDO_CLUSTER     1
#define DATA_CLUSTER            2       // first legitimate cluster number
#define EOF_CLUSTER             ((unsigned)-1)
#define UNKNOWN_CLUSTER         EOF_CLUSTER

#define ISROOTDIR(pstm)         ((pstm)->s_clusFirst == (pstm)->s_pvol->v_pstmRoot->s_clusFirst)
#define ISPARENTROOTDIR(pstm)   ((pstm)->s_sid.sid_clusDir == (pstm)->s_pvol->v_pstmRoot->s_clusFirst)

#define ISEOF(pvol, cl)          ((cl) >= (pvol)->v_clusEOF)
#define CLUSTERTOBLOCK(pvol,cl)  (((cl) << (pvol)->v_log2cblkClus) + (pvol)->v_blkClusBias)
#define CLUSTERTOSECTOR(pvol,cl) ((((cl)-DATA_CLUSTER) << (pvol)->v_log2csecClus) + (pvol)->v_csecUsed + (pvol)->v_secBlkBias)
#define BLOCKTOCLUSTER(pvol, bl) (((bl) - (pvol)->v_blkClusBias) >> (pvol)->v_log2cblkClus) 

#define UNPACK(pvol,index,pdata)     ((pvol)->v_unpack((pvol),(index),(pdata)))
#define PACK(pvol,index,data,pold)   ((pvol)->v_pack((pvol),(index),(data),(pold)))



















































// values for lr_id:
#define LOGID_NONE              0
#define LOGID_SCANVOLUME        1       // ScanVolume()
#define LOGID_BUFFERMODIFY      2       // ModifyBuffer()
#define LOGID_BUFFERWRITE       3       // CommitBuffer(PRE)
#define LOGID_BUFFERCOMMIT      4       // CommitBuffer(POST)
#define LOGID_DISKIO            5       // FAT_DeviceIoControl
#define LOGID_CREATEFILE        6       // FAT_CreateFileW
#define LOGID_CLOSEFILE         7       // FAT_CloseFile
#define LOGID_WRITEFILE         8       // FAT_WriteFile
#define LOGID_FLUSHFILEBUFFERS  9       // FAT_FlushFileBuffers
#define LOGID_SETENDOFFILE      10      // FAT_SetEndOfFile
#define LOGID_CREATEDIRECTORY   11      // FAT_CreateDirectoryW
#define LOGID_REMOVEDIRECTORY   12      // FAT_RemoveDirectoryW
#define LOGID_DELETEFILE        13      // FAT_DeleteFileW
#define LOGID_MOVEFILE          14      // FAT_MoveFileW
#define LOGID_TOTAL             15

struct _LOGREC {
    WORD        lr_size;        // total size of LOGREC, including lr_size
    BYTE        lr_id;          // see LOGID_* above
    BYTE        lr_flags;       // reserved for flags (must be zero for now)
    DWORD       lr_owner;       // non-zero thread ID if owned, zero if not
    DWORD       lr_serialID;    // volume serial # from boot sector
    DWORD       lr_blk;         // block # of modification
    DWORD       lr_off;         // offset of modification within block
    DWORD       lr_cb;          // size of modification, in bytes
    DWORD       lr_crc;         // checksum of buffer (on LOGID_BUFFERCOMMIT)
#ifdef DEBUG
    CHAR        lr_desc[18];    // space for descriptive string
#endif
    BYTE        lr_abData[];    // original bytes
};
typedef struct _LOGREC LOGREC, *PLOGREC;


/*  Data run:
 *
 *  This structure is used to describe a contiguous run of data on the disk.
 *  Note that the start and end fields are stream-relative, whereas all the
 *  block and cluster fields are volume-relative.
 *
 *  NOTE that although r_blk and r_clusThis convey similar information, we
 *  can't eliminate either of them.  That's because the FAT (and possibly ROOT)
 *  streams are not cluster-mapped, so the only fields that get used in those
 *  cases are the first three (r_start, r_end and r_blk).
 */

typedef struct _RUN {
    DWORD       r_start;        // starting byte position of run
    DWORD       r_end;          // ending byte of run + 1
    DWORD       r_blk;          // first block of run
    DWORD       r_clusThis;     // first cluster of this run
    DWORD       r_clusNext;     // next cluster after this run
    DWORD       r_clusPrev;     // cluster preceding this run (0 if none)
} RUN, *PRUN;


/*  Open stream structure:
 *
 *  This structure holds the description of an open file or directory. The
 *  SHANDLE & FHANDLE structures for the same file or directory must point to the
 *  same DSTREAM structure.
 */

// values for s_flags:
#define STF_INIT        0x0001  // stream has been initialized by DIRENTRY
#define STF_DEMANDPAGED 0x0002  // stream can be demand-paged
#define STF_CREATE_TIME 0x0004  // an explicit create time has been set
#define STF_ACCESS_TIME 0x0008  // an explicit access time has been set
#define STF_WRITE_TIME  0x0010  // an explicit write time has been set
#define STF_VOLUME      0x0020  // this stream is VOLUME-based (ie, not a file)
#define STF_UNMOUNTED   0x0040  // this stream currently unavailable (volume unmounted)
#define STF_BUFCURHELD  0x0080  // pbufCur is currently in use and being held
#define STF_WRITETHRU   0x0100  // somebody has opened the stream in write-through mode
#define STF_VISITED     0x0200  // stream has been visited (see FAT_CloseAllFiles)
#define STF_DIRTY_INFO  0x0400  // stream info out of sync with DIRENTRY
#define STF_DIRTY_DATA  0x0800  // stream data has been modified
#define STF_DIRTY_CLUS  0x1000  // starting cluster in stream info out of sync with DIRENTRY
#define STF_PRIVATE     0x8000  // stream is private (may be a duplicate, but it's for non-shared read-access only)
#define STF_DIRTY       (STF_DIRTY_INFO | STF_DIRTY_DATA)

#define MAX_DIRSIZE     0x7FFFFFFF

typedef struct _DSID {          // structure for unique data stream IDs (similar to OIDs)
    DWORD       sid_ordDir;     // ordinal in directory of stream's DIRENTRY
    DWORD       sid_clusDir;    // cluster of dir containing stream's DIRENTRY
} DSID;
typedef DSID *PDSID;

#define SETSID(psid,pos,clus)   (psid)->sid_ordDir = (pos)/sizeof(DIRENTRY), (psid)->sid_clusDir = (clus)




#if !defined(OIDHIGH) || !defined(OIDLOW)
#define SETSIDFROMOID(psid,oid) (psid)->sid_ordDir = 0, (psid)->sid_clusDir = UNKNOWN_CLUSTER
#else
#define SETSIDFROMOID(psid,oid) (psid)->sid_ordDir = OIDHIGH(oid), (psid)->sid_clusDir = OIDLOW(oid)
#endif

#if !defined(AFSFROMOID)        // either AFSFROMOID or OIDAFS must be defined externally...
#define AFSFROMOID(oid)         OIDAFS(oid)
#endif

#if !defined(OIDFROMAFS)        // either OIDFROMAFS or PACKOID must be defined externally...
#define OIDFROMAFS(iAFS)        PACKOID(iAFS,0,0)
#endif

#if !defined(INVALID_OID)
#define INVALID_OID             ((CEOID)(INVALID_HANDLE_VALUE))
#endif

#if !defined(PACKOID)
#define OIDFROMSID(pvol,psid)   INVALID_OID
#define OIDFROMSTREAM(pstm)     INVALID_OID
#else
#define OIDFROMSID(pvol,psid)   PACKOID((pvol)->v_volID, (psid)?((PDSID)(psid))->sid_ordDir:0, (psid)?((PDSID)(psid))->sid_clusDir:0)
#define OIDFROMSTREAM(pstm)     OIDFROMSID((pstm)->s_pvol, &(pstm)->s_sid)
#endif


struct _DSTREAM {
    STM_DLINK   s_dlOpenStreams;// linkage for v_dlOpenStreams
    FH_DLINK    s_dlOpenHandles;// list of FHANDLE's or SHANDLE's
    PVOLUME     s_pvol;         // VOLUME the DSTREAM is open on
    long        s_refs;         // usage count for the stream
    WORD        s_flags;        // stream status flags (see STF_*)
    WORD        s_cwPath;       // # characters in path to this stream (excluding NULL)
    RUN         s_run;          // run info from current/last operation
    PBUF        s_pbufCur;      // pointer to stream's current buffer, if any
    DWORD       s_offbufCur;    // stream offset associated with current buffer
    DWORD       s_size;         // length of the stream
    CRITICAL_SECTION s_cs;      // critical section for this stream
#ifdef TFAT
    PDSTREAM	s_pstmParent;	// parent stream: TFAT only
#endif
//
//  Beginning of portion that actually relates to non-STF_VOLUME streams only!
//  (actually, s_cwPath belongs down here as well, but since it's a WORD, it fits
//  better up above)
//
    DSID        s_sid;          // unique stream ID
    DSID        s_sidParent;    // unique stream ID for parent
    DWORD       s_offDir;       // offset in block of stream's DIRENTRY
    DWORD       s_blkDir;       // block of dir containing stream's DIRENTRY
    DWORD       s_clusFirst;    // first cluster in stream
    FILETIME    s_ftCreate;     // creation time from DIRENTRY
    FILETIME    s_ftAccess;     // last access time from DIRENTRY
    FILETIME    s_ftWrite;      // last write time from DIRENTRY
    BYTE        s_attr;         // file attribute byte
    BYTE        s_achOEM[OEMNAMESIZE+1];
};


/*  Open file handle structure:
 *
 *  There is one these for each open file handle on the volume. It holds the
 *  current file position, the file access mode, etc.
 */

// values for fh_flags:
#define FHF_FAILUI      0x01    // Used to determine if we need to put up a UI window on failure
#define FHF_LOCKED      0x10    // volume-level lock granted
#define FHF_VOLUME      0x20    // volume-level access only (ie, CreateFile("VOL:"))
#define FHF_UNMOUNTED   0x40    // this handle currently unavailable (volume has been unmounted)
#define FHF_FHANDLE     0x80    // this bit NOT set for SHANDLEs

struct _FHANDLE {
    FH_DLINK    fh_dlOpenHandles;
    PDSTREAM    fh_pstm;        // DSTREAM for the file
    BYTE        fh_flags;       // handle status flags (see FHF_*)
    BYTE        fh_mode;        // file access & sharing modes
    WORD        fh_pad;
    DWORD       fh_pos;         // current file position
    HANDLE      fh_h;           // kernel handle to this FHANDLE
    HANDLE      fh_hProc;       // process that opened the file
};


/*  Open search handle structure:
 *
 *  There is one of these for each open FindFirst/FindNext. It holds the
 *  current position of the find and the pattern being sought.  For internal
 *  (SHF_BYORD or SHF_BYCLUS) searches, sh_h is overloaded as the desired
 *  DIRENTRY ordinal or cluster.
 *
 *  Also, when an SHANDLE is allocated, sh_awcPattern is not really allocated
 *  to MAX_PATH WCHARs.  It is only allocated to the length stored in sh_cwPattern.
 */

#define INVALID_POS             0xFFFFFFFF
#define INVALID_ATTR            0xFFFFFFFF

// The first three "search by" flags are mutually exclusive searches, hence
// they don't need completely separate bits.

// values for sh_flags:
#define SHF_BYNAME      0x01    // search by name
#define SHF_BYORD       0x02    // search by DIRENTRY ordinal
#define SHF_BYCLUS      0x03    // search by DIRENTRY containing specific cluster
#define SHF_BYMASK      0x03    // mask for the mutually exclusive search types
#define SHF_BYATTR      0x04    
#define SHF_WILD        0x08    // allow wildcards
#define SHF_DOTDOT      0x10    // allow search for "." and ".." entries
#define SHF_CREATE      0x20    // turn on auto-generated short name creation
#define SHF_UNMOUNTED   FHF_UNMOUNTED
#define SHF_NOT_SHANDLE FHF_FHANDLE

struct _SHANDLE {
    SH_DLINK    sh_dlOpenHandles;
    PDSTREAM    sh_pstm;        // DSTREAM for the directory
    BYTE        sh_flags;       // search status flags (see SHF_*)
    BYTE        sh_attr;        
    WORD        sh_cwPattern;   // # characters in sh_awcPattern (includes room for NULL)
    DWORD       sh_pos;         // last position within directory
    HANDLE      sh_h;           // kernel handle to this SHANDLE
    HANDLE      sh_hProc;       // process that opened the file
#ifdef DEBUG
    DWORD       sh_cbAlloc;     // used to track size of this allocation only
#endif
    WCHAR       sh_awcPattern[MAX_PATH];// search pattern
};


ERRFALSE(offsetof(FHANDLE,fh_dlOpenHandles) == offsetof(SHANDLE,sh_dlOpenHandles));
ERRFALSE(offsetof(FHANDLE,fh_pstm)          == offsetof(SHANDLE,sh_pstm));
ERRFALSE(offsetof(FHANDLE,fh_flags)         == offsetof(SHANDLE,sh_flags));
ERRFALSE(offsetof(FHANDLE,fh_h)             == offsetof(SHANDLE,sh_h));
ERRFALSE(offsetof(FHANDLE,fh_hProc)         == offsetof(SHANDLE,sh_hProc));


/*  Define the maximum number of short name (8.3) collisions supported;
 *  note that if you increase this beyond one more order of magnitude (> 9999),
 *  then you will have to change itopch() to support larger numbers.
 */

#define MAX_SHORT_NAMES 999


/*  Directory search information structure:
 *
 *  This structure can be passed to FindNext to return additional information
 *  about the directory it just searched.  Most of this information is essential
 *  only if we need to call CreateName.  Note that for every DIRINFO structure,
 *  the caller should have a corresponding SHANDLE structure, because some of the
 *  information that FindNext records in the DIRINFO is dependent on the SHANDLE
 *  it was given.
 */

#define DIRINFO_OEM     0x01    // name conforms to 8.3

struct _DIRINFO {
    PDIRENTRY   di_pde;         // pointer to matching DIRENTRY (NULL if none)
    DSID        di_sid;
    DWORD       di_clusEntry;   // cluster extracted from DIRENTRY
    DWORD       di_pos;         // position of matching (or last) DIRENTRY
    DWORD       di_posLFN;      // position of 1st matching LFN DIRENTRY, if any
    int         di_cdeNeed;     // number of DIRENTRY's needed for LFN (-1 if unknown)
    DWORD       di_posFree;     // position of first free/deleted DIRENTRY to satisfy
    int         di_cdeFree;     // count of free/deleted DIRENTRY(s), 0 if none
    DWORD       di_posOrphan;   // position of orphaned LFN(s) to recover
    int         di_cdeOrphan;   // count of orphaned DIRENTRY(s), 0 if none
    int         di_cwName;      // size of entire name associated with DIRENTRY
    PCWSTR      di_pwsTail;     // pointer to end-most LFN piece, in associated SHANDLE structure
    WORD        di_cwTail;      // size of end-most LFN piece
    BYTE        di_bNumericTail;// DIRENTRY position (0-6) where numeric tail should start
    BYTE        di_chksum;      // checksum of di_achOEM (valid only if DIRINFO_OEM set)
    BYTE        di_flags;       // see DIRINFO_* above
    BYTE        di_achOEM[OEMNAMESIZE];
};
typedef struct _DIRINFO DIRINFO, *PDIRINFO;

#define GETDIRENTRYCLUSTER(pvol,pde) (                          \
        (pvol)->v_clusEOF <= 0xFFFF?                            \
        (pde)->de_clusFirst :                                   \
        (pde)->de_clusFirst+(DWORD)((pde)->de_clusFirstHigh<<16)\
)

#define SETDIRENTRYCLUSTER(pvol,pde,clus) {                     \
        (pde)->de_clusFirst = (WORD)(clus);                     \
        if ((pvol)->v_clusEOF > 0xFFFF)                         \
            (pde)->de_clusFirstHigh = (WORD)((clus) >> 16);     \
}

#define AccessToMode(dwAccess)  ((dwAccess)>>24)
#define FH_MODE_READ            AccessToMode(GENERIC_READ)      // 0x80
#define FH_MODE_WRITE           AccessToMode(GENERIC_WRITE)     // 0x40

#define ShareToMode(dwShare)    (dwShare)
#define FH_MODE_SHARE_READ      ShareToMode(FILE_SHARE_READ)    // 0x01
#define FH_MODE_SHARE_WRITE     ShareToMode(FILE_SHARE_WRITE)   // 0x02

ERRFALSE(FH_MODE_READ!=0);
ERRFALSE(FH_MODE_WRITE!=0);
ERRFALSE((AccessToMode((GENERIC_READ|GENERIC_WRITE)) & (FILE_SHARE_READ|FILE_SHARE_WRITE)) == 0);


//  OpenName/CreateName options.  These are defined such that ATTR_* flags
//  can be combined with them without conflict (but since FH_MODE_* flags already
//  conflict with ATTR_*, we have no choice but to shift them into place).

#define NAME_ATTR_MASK          0x000000FF      // mask for ATTR_* bits
#define NAME_FILE               0x00000100      // name must be a file
#define NAME_DIR                0x00000200      // name must be a directory
#define NAME_VOLUME             0x00000400      // name may be "VOL:"
#define NAME_NEW                0x00001000      // file must be new
#define NAME_CREATE             0x00002000      // file can be created
#define NAME_TRUNCATE           0x00004000      // file must be truncated
#define NAME_CREATED            0x00008000      // file was actually created
#define NAME_MODE_MASK          0x00FF0000      // mask for FH_MODE_* bits

#define NAME_MODE_SHIFT         16              // shift for FH_MODE_* bits
#define NAME_MODE_READ          (FH_MODE_READ << NAME_MODE_SHIFT)
#define NAME_MODE_WRITE         (FH_MODE_WRITE << NAME_MODE_SHIFT)
#define NAME_MODE_SHARE_READ    (FH_MODE_SHARE_READ << NAME_MODE_SHIFT)
#define NAME_MODE_SHARE_WRITE   (FH_MODE_SHARE_WRITE << NAME_MODE_SHIFT)


/*  Stucture for SetSizePointer in MISC.C
 */
typedef struct _SIZEPTR {
    DWORD   c;                  // count of elements
    PVOID   p;                  // pointer to elements
} SIZEPTR;
typedef SIZEPTR *PSIZEPTR;


/*  API.C functions
 */

BOOL    FAT_NoSupport(void);
BOOL    FAT_CloseAllFiles(PVOLUME pvol, HANDLE hProc);
void    FAT_Notify(PVOLUME pvol, DWORD dwFlags);
BOOL    FATEnter(PVOLUME pvol, BYTE idLog);
void    FATEnterQuick(void);
void    FATExit(BYTE idLog);
void    FATExitQuick(void);
PDSK    FSD_Init(PCWSTR pwsDisk);
BOOL    FSD_Deinit(PDSK pdsk);
BOOL    WINAPI FATMain(HINSTANCE DllInstance, INT Reason, LPVOID Reserved);


/*  BUFFER.C functions
 *
 *  Note that HoldBuffer and UnholdBuffer are actually macros, and moreover,
 *  do not use interlocked inc/dec functions;  such calls would be overkill, because
 *  the only time a buffer's hold count actually transitions from zero -- and the
 *  only time a buffer's hold count is actually depended upon -- is while csBuffers
 *  is held, so the integrity of the hold counts should already be assured.
 *
 *  Uses of HoldBuffer and UnholdBuffer outside the normal buffer manipulation
 *  functions (like those in path.c) are simply applying an *additional* hold to a
 *  buffer that is already known to be held;  without that additional hold, a read to
 *  a different part of the same stream might otherwise release that buffer's contents,
 *  allowing it to be reused and thereby invalidating any pointers the caller had
 *  obtained to the earlier contents of the buffer.
 *
 *  This should help explain the logic in the DEBUG-only assertions embedded in the
 *  macros below.  Also, the hold threshold of 32 is arbitrary;  it is simply an attempt
 *  to validate that hold counts will never exceed the capacity of a BYTE.
 */
#define HoldBuffer(pbuf)        ( \
                                    ASSERT(((pbuf)->b_hold != 0 || OWNCRITICALSECTION(&csBuffers)) && (pbuf)->b_hold < 32), \
                                    (pbuf)->b_hold++ \
                                )
#define UnholdBuffer(pbuf)      ( \
                                    ASSERT((pbuf)->b_hold != 0), \
                                    --(pbuf)->b_hold \
                                )
#define HeldBuffer(pbuf)        ( \
                                    ASSERT(OWNCRITICALSECTION(&csBuffers)), \
                                    (pbuf)->b_hold != 0 \
                                )
#define ASSERTHELDBUFFER(pbuf)  ( \
                                    (pbuf)->b_hold != 0 \
                                )

BOOL    BufInit(void);
void    BufDeinit(void);
BOOL    BufEnter(BOOL fForce);
void    BufExit(void);

BOOL    AllocBufferPool(PVOLUME pvol);
BOOL    FreeBufferPool(PVOLUME pvol);

DWORD   ModifyBuffer(PBUF pbuf, PVOID pMod, int cbMod);
void    DirtyBuffer(PBUF pbuf);
void    DirtyBufferError(PBUF pbuf, PVOID pMod, int cbMod);
DWORD   CommitBuffer(PBUF pbuf, BOOL fCS);
void    CleanBuffer(PBUF pbuf);
DWORD   FindBuffer(PVOLUME pvol, DWORD blk, PDSTREAM pstm, BOOL fNoRead, PBUF *ppbuf);
void    AssignStreamBuffer(PDSTREAM pstm, PBUF pbuf, BOOL fCS);
DWORD   ReleaseStreamBuffer(PDSTREAM pstm, BOOL fCS);
DWORD   ReadStreamBuffer(PDSTREAM pstm, DWORD pos, int lenMod, PVOID *ppvStart, PVOID *ppvEnd);
DWORD   ModifyStreamBuffer(PDSTREAM pstm, PVOID pMod, int cbMod);
#define CommitAndReleaseStreamBuffers(pstm) CommitBufferSet(pstm, 1)
#define CommitStreamBuffers(pstm)           CommitBufferSet(pstm, 0)
#define CommitVolumeBuffers(pvol)           CommitBufferSet((PDSTREAM)(pvol), -1)
#define CommitAllBuffers()                  CommitBufferSet(NULL, -1)
#define CommitOldBuffers()                  CommitBufferSet(NULL, -2)
#define WriteAndReleaseStreamBuffers(pstm)  CommitAndReleaseStreamBuffers(pstm)
#define WriteStreamBuffers(pstm)            CommitStreamBuffers(pstm)
DWORD   CommitBufferSet(PDSTREAM pstm, int iCommit);
void    InvalidateBufferSet(PVOLUME pvol, BOOL fAll);
BOOL    LockBlocks(PDSTREAM pstm, DWORD pos, DWORD len, DWORD *pblk, DWORD *pcblk, BOOL fWrite);
void    UnlockBlocks(DWORD blk, DWORD cblk, BOOL fWrite);


/*  CACHE.C functions
 */

void     PathCacheCreate(PVOLUME pvol, PCWSTR pwsPath, int len, PDSTREAM pstm);
PDSTREAM PathCacheSearch(PVOLUME pvol, PCWSTR *ppwsPath);
BOOL     PathCacheInvalidate(PVOLUME pvol, PCWSTR pwsPath);
int      PathCacheLength(PCWSTR *ppwsPath, int celRemove);
PCACHE   PathCacheFindStream(PVOLUME pvol, PDSTREAM pstm);
PDSTREAM PathCacheDestroy(PVOLUME pvol, PCACHE pcch, BOOL fClose);
void     PathCacheDestroyAll(PVOLUME pvol);
void    PathCanonicalize (PCWSTR pwsOldPath, DWORD dwOldPathLen, PWSTR pwsNewPath, DWORD dwNewPathLen);


/*  DISK.C functions
 */

DWORD   GetDiskInfo(HANDLE hdsk, PDISK_INFO pdi);
DWORD   SetDiskInfo(HANDLE hdsk, PDISK_INFO pdi);
DWORD   ReadWriteDisk(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, PVOID pvBuffer, BOOL fRemapFats);
#ifdef DISK_CACHING
void    SetupDiskCache(PVOLUME pvol);
DWORD   ReadWriteDisk2(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, PVOID pvBuffer);
#endif
PDSK    MountDisk(HANDLE hdsk, PCWSTR pwsDisk, DWORD flVol);
BOOL FreeClustersOnDisk (PVOLUME pvol, DWORD dwStartCluster, DWORD dwNumClusters);
BOOL    UnmountDisk(PDSK pdsk, BOOL fFrozen);
DWORD   UnmountAllDisks(BOOL fFrozen);

/*  FAT.C functions
 */

void    LockFAT(PVOLUME pvol);
DWORD   GetFAT(PVOLUME pvol, DWORD dwOffset, PVOID *ppvEntry, PVOID *ppvEntryEnd);
void    UnlockFAT(PVOLUME pvol);
DWORD   Unpack12(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData);
DWORD   Pack12(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld);
DWORD   Unpack16(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData);
DWORD   Pack16(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld);
DWORD   Unpack32(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData);
DWORD   Pack32(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld);
DWORD   UnpackRun(PDSTREAM pstm);
DWORD   NewCluster(PVOLUME pvol, DWORD clusPrev, PDWORD pclusNew);


/*  FILE.C functions
 */

HANDLE  FAT_CreateFileW(PVOLUME pvol, HANDLE hProc, LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL    FAT_CloseFile(PFHANDLE pfh);
#if NUM_FILE_APIS == 13
BOOL    FAT_ReadFilePagein(PFHANDLE pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped);
#else
BOOL    FAT_ReadFileWithSeek(PFHANDLE pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset);
BOOL    FAT_WriteFileWithSeek(PFHANDLE pfh, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten, LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset);
#endif
BOOL    FAT_ReadFile(PFHANDLE pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped);
BOOL    FAT_WriteFile(PFHANDLE pfh, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten, LPOVERLAPPED lpOverlapped);
DWORD   FAT_SetFilePointer(PFHANDLE pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD   FAT_GetFileSize(PFHANDLE pfh, LPDWORD lpFileSizeHigh);
BOOL    FAT_GetFileInformationByHandle(PFHANDLE pfh, LPBY_HANDLE_FILE_INFORMATION lpFileInfo);
BOOL    FAT_FlushFileBuffers(PFHANDLE pfh);
BOOL    FAT_GetFileTime(PFHANDLE pfh, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL    FAT_SetFileTime(PFHANDLE pfh, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL    FAT_SetEndOfFile(PFHANDLE pfh);
BOOL    FAT_DeviceIoControl(PFHANDLE pfh, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);


/*  FIND.C functions
 */

HANDLE  FAT_FindFirstFileW(PVOLUME pvol, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd);
BOOL    FAT_FindNextFileW(PSHANDLE psh, PWIN32_FIND_DATAW pfd);
BOOL    FAT_FindClose(PSHANDLE psh);

PDSTREAM OpenRoot(PVOLUME pvol);
DWORD    CloseRoot(PVOLUME pvol);
PDSTREAM OpenPath(PVOLUME pvol, PCWSTR pwsPath, PCWSTR *ppwsTail, int *plen, int flName, DWORD clusFail);
PDSTREAM OpenName(PDSTREAM pstmDir, PCWSTR pwsName, int cwName, int *pflName);
DWORD    CreateName(PDSTREAM pstmDir, PCWSTR pwsName, PDIRINFO pdi, PDIRENTRY pdeClone, int flName);
DWORD    DestroyName(PDSTREAM pstmDir, PDIRINFO pdi);
PDSTREAM FindFirst(PVOLUME pvol, PCWSTR pwspath, PSHANDLE psh, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, int flName, DWORD clusFail);
DWORD    FindNext(PSHANDLE psh, PDIRINFO pdi, PWIN32_FIND_DATAW pfd);
void     CreateDirEntry(PDSTREAM pstmDir, PDIRENTRY pde, PDIRENTRY pdeClone, BYTE bAttr, DWORD clusFirst);


/*  FORMAT.C
 */
DWORD   FormatVolume(PVOLUME pvol, PFMTVOLREQ pfv);


/*  MISC.C functions
 */

int     Log2(unsigned int n);
void    InitList(PDLINK pdl);
int     ListIsEmpty(PDLINK pdl);
void    AddItem(PDLINK pdlIns, PDLINK pdlNew);
void    RemoveItem(PDLINK pdl);
int     pchtoi(PCHAR pch, int cchMax);
int     itopch(int i, PCHAR pch);

//  Bit arrays start with two special DWORDs that are not part of
//  of the actual array of bits.  The first DWORD is initialized to
//  the number of bits that can be held, and the second DWORD keeps track
//  of the total number of SET bits.

#define ALLOCA(p,cb) {                                          \
                p = NULL;                                       \
                if (cb <= 32*1024) {                            \
                    __try {                                     \
                        p = _alloca(cb);                        \
                    } __except (EXCEPTION_EXECUTE_HANDLER) {    \
                        ;                                       \
                    }                                           \
                }                                               \
}
#define CreateBitArray(pdwBitArray, cbits) {                    \
            DWORD cb = (((cbits)+31)/32 + 2)*sizeof(DWORD);     \
            pdwBitArray = NULL;                                 \
            if (cb > 2*sizeof(DWORD)) {                         \
                ALLOCA(pdwBitArray, cb);                        \
            }                                                   \
            if (pdwBitArray) {                                  \
                memset(pdwBitArray, 0, cb);                     \
                pdwBitArray[0] = (cbits);                       \
            }                                                   \
}
void    SetBitArray(PDWORD pdwBitArray, int i);
void    ClearBitArray(PDWORD pdwBitArray, int i);
BOOL    TestBitArray(PDWORD pdwBitArray, int i);

BOOL    DOSTimeToFileTime(WORD dosDate, WORD dosTime, BYTE tenMSec, PFILETIME pft);
BOOL    FileTimeToDOSTime(PFILETIME pft, PWORD pdosDate, PWORD pdosTime, PBYTE ptenMSec);
void    SetSizePointer(PSIZEPTR psp, DWORD cb, DWORD c, PVOID pSrc, HANDLE hProc);


/*  NAME.C functions
 */

BYTE    ChkSumName(const BYTE *pchOEM);
BOOL    MatchesWildcard(DWORD lenWild, PCWSTR pchWild, DWORD lenFile, PCWSTR pchFile);
void    InitNumericTail(PDIRINFO pdi);
void    CheckNumericTail(PDIRINFO pdi, PDWORD pdwBitArray);
void    GenerateNumericTail(PDIRINFO pdi, PDWORD pdwBitArray);
int     OEMToUniName(PWSTR pws, PCHAR pchOEM, UINT nCodePage);
BOOL    UniToOEMName(PVOLUME pvol, PCHAR pchOEM, PCWSTR pwsName, int cwName, UINT nCodePage);


/*  PATH.C functions
 */

BOOL    FAT_CreateDirectoryW(PVOLUME pvol, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL    FAT_RemoveDirectoryW(PVOLUME pvol, PCWSTR pwsPathName);
DWORD   FAT_GetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName);
BOOL    FAT_SetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName, DWORD dwAttributes);
BOOL    FAT_DeleteFileW(PVOLUME pvol, PCWSTR pwsFileName);
BOOL    FAT_MoveFileW(PVOLUME pvol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName);
BOOL    FAT_RegisterFileSystemNotification(PVOLUME pvol, HWND hwnd);
BOOL    FAT_RegisterFileSystemFunction(PVOLUME pvol, SHELLFILECHANGEFUNC_t pfn);
BOOL    FAT_PrestoChangoFileName(PVOLUME pvol, PCWSTR pwsOldFile, PCWSTR pwsNewFile);
BOOL    FAT_GetDiskFreeSpaceW(PVOLUME pvol, PCWSTR pwsPathName, PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters);
DWORD   GetSIDInfo(PVOLUME pvol, PDSID psid, CEOIDINFO *poi);

#ifdef SHELL_MESSAGE_NOTIFICATION
#ifndef DEBUG
#define POSTFILESYSTEMMESSAGE(pvol, uMsg, psid, psidParent, pwsCaller) \
        PostFileSystemMessage(pvol, uMsg, psid, psidParent)
void    PostFileSystemMessage(PVOLUME pvol, UINT uMsg, PDSID psid, PDSID psidParent);
#else
#define POSTFILESYSTEMMESSAGE(pvol, uMsg, psid, psidParent, pwsCaller) \
        PostFileSystemMessage(pvol, uMsg, psid, psidParent, pwsCaller)
void    PostFileSystemMessage(PVOLUME pvol, UINT uMsg, PDSID psid, PDSID psidParent, PWSTR pwsCaller);
#endif  // DEBUG
#else
#define POSTFILESYSTEMMESSAGE(pvol, uMsg, psid, psidParent, pwsCaller)
#endif  // SHELL_MESSAGE_NOTIFICATION

#ifdef SHELL_CALLBACK_NOTIFICATION
#ifndef DEBUG
#define CALLFILESYSTEMFUNCTION(pvol, dwSHCNE, psid, psidOld, poiOld, pwsCaller) \
        CallFileSystemFunction(pvol, dwSHCNE, psid, psidOld, poiOld)
void    CallFileSystemFunction(PVOLUME pvol, DWORD dwSHCNE, PDSID psid, PDSID psidOld, CEOIDINFO *poiOld);
#else
#define CALLFILESYSTEMFUNCTION(pvol, dwSHCNE, psid, psidOld, poiOld, pwsCaller) \
        CallFileSystemFunction(pvol, dwSHCNE, psid, psidOld, poiOld, pwsCaller)
void    CallFileSystemFunction(PVOLUME pvol, DWORD dwSHCNE, PDSID psid, PDSID psidOld, CEOIDINFO *poiOld, PWSTR pwsCaller);
#endif  // DEBUG
#else
#define CALLFILESYSTEMFUNCTION(pvol, dwSHCNE, psid, psidOld, poiOld, pwsCaller)
#endif  // SHELL_CALLBACK_NOTIFICATION

#define FILESYSTEMNOTIFICATION(pvol, uMsg, uMsgOld, dwSHCNE, psid, psidParent, psidOld, psidParentOld, poiOld, pwsCaller) \
{ \
        if (uMsgOld) { \
            POSTFILESYSTEMMESSAGE(pvol, uMsgOld, psidOld, psidParentOld, pwsCaller); \
        } \
        if (uMsg) { \
            POSTFILESYSTEMMESSAGE(pvol, uMsg, psid, psidParent, pwsCaller); \
        } \
        CALLFILESYSTEMFUNCTION(pvol, dwSHCNE, psid, psidOld, poiOld, pwsCaller); \
}


/*  SCAN.C
 */

// The private set of SCANERR bits listed here augments the public set of bits in FATAPI.H
#ifdef USE_FATUTIL
DWORD ScanVolume(PVOLUME pvol, DWORD dwScanOptions);
#else
#define SCANERR_REPAIRABLE      0x80000000      // the current error is repairable

#define SCANDATA_NONE           0x00000000
#define SCANDATA_TOODEEP        0x00000001      // set if directory nesting level was too deep

typedef struct _SCANDATA SCANDATA, *PSCANDATA;
typedef BOOL (*PFNSCANFIX)(struct _SCANDATA *psd, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, DWORD dwScanErr);

struct _SCANDATA {
    PVOLUME     sd_pvol;
    DWORD       sd_dwScanVol;
    PFNSCANFIX  sd_pfnScanFix;
    PVOID       sd_pRefData;
    DWORD       sd_cFiles;
    DWORD       sd_cLevels;
    DWORD       sd_flags;
    PDWORD      sd_pdwClusArray;
    WCHAR       sd_wsPath[MAX_PATH];
};

int     ScanFixInteractive(PSCANDATA psd, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, DWORD dwScanErr);
DWORD   ScanVolume(PVOLUME pvol, DWORD dwScanVol, PFNSCANFIX pfnScanFix, PVOID pRefData);
#endif

/*  STREAM.C functions
 */

//  OpenStream flags (NOTE: the CREATE and REFRESH flags can be combined
//  to *create* a stream if one doesn't exist and *refresh* it if it does,
//  all in one easy-to-use call)

#define OPENSTREAM_NONE         0x00000000
#define OPENSTREAM_CREATE       0x00010000      // create a new stream
#define OPENSTREAM_REFRESH      0x00020000      // refresh an existing stream
#define OPENSTREAM_PRIVATE      STF_PRIVATE     // create/refresh a PRIVATE stream

#define RESIZESTREAM_NONE       0x00000000
#define RESIZESTREAM_SHRINK     0x00000001      // if stream is too large, shrink it
#define RESIZESTREAM_UPDATEFAT  0x00000002      // if stream was resized, update the FAT

PDSTREAM OpenStream(PVOLUME pvol, DWORD clusFirst, PDSID psid, PDSTREAM pstmParent, PDIRINFO pdi, DWORD dwFlags);
DWORD   CloseStream(PDSTREAM pstm);
DWORD   CommitStream(PDSTREAM pstm, BOOL fAll);
void    RewindStream(PDSTREAM pstm, DWORD pos);
DWORD   SeekStream(PDSTREAM pstm, DWORD pos);
DWORD   PositionStream(PDSTREAM pstm, DWORD pos, PDWORD pdwClus);
DWORD   ReadStream(PDSTREAM pstm, DWORD pos, PVOID *ppvStart, PVOID *ppvEnd);
DWORD   ReadStreamData(PDSTREAM pstm, DWORD pos, PVOID pvData, DWORD len, PDWORD plenRead);
DWORD   WriteStreamData(PDSTREAM pstm, DWORD pos, PCVOID pvData, DWORD len, PDWORD plenWritten, BOOL fCommit);
DWORD   ResizeStream(PDSTREAM pstm, DWORD cbNew, DWORD dwResizeFlags);
BOOL    CheckStreamHandles(PVOLUME pvol, PDSID psid);
VOID UpdateSourceStream (PVOLUME pvol, PDSID psidSrc, PDIRINFO pdiDst, PDSTREAM pstmDstParent);
BOOL    StreamOpenedForExclAccess(PVOLUME pvol, PDSID psid);
BOOL    CheckStreamSharing(PDSTREAM pstm, int mode);


/*  VOLUME.C functions
 */

DWORD   ReadVolume(PVOLUME pvol, DWORD block, int cBlocks, PVOID pvBuffer);
DWORD   WriteVolume(PVOLUME pvol, DWORD block, int cBlocks, PVOID pvBuffer);
BOOL    InitVolume(PVOLUME pvol, PBIGFATBOOTSEC pbgbs);
BOOL    ValidateFATSector(PVOLUME pvol, PVOID pvSector);
PVOLUME FindVolume(PDSK pdsk, PBIGFATBOOTSEC pbgbs);
DWORD   TestVolume(PVOLUME pvol, PBIGFATBOOTSEC *ppbgbs);
void    RefreshVolume(PVOLUME pvol);
DWORD   LockVolume(PVOLUME pvol, DWORD dwFlags);
void    UnlockVolume(PVOLUME pvol);
PVOLUME OpenVolume(PDSK pdsk, PPARTINFO ppi, PBIGFATBOOTSEC *ppbgbs, PDSTREAM pstmParent);
BOOL    CloseVolume(PVOLUME pvol, PWSTR pwsVolName);
void    QueryVolumeParameters(PVOLUME pvol, PDEVPB pdevpb, BOOL fVolume);
BOOL    RegisterVolume(PVOLUME pvol);
void    DeregisterVolume(PVOLUME pvol);
PVOLUME MountVolume(PDSK pdsk, PBIGFATBOOTSEC *ppbgbs, PPARTINFO ppi, DWORD flVol);
BOOL    UnmountVolume(PVOLUME pvol, BOOL fFrozen);
BOOL    CheckUnformattedVolume(PVOLUME pvol);
PDSK    FindDisk(HANDLE hdsk, PCWSTR pwsDisk, PDISK_INFO pdi);

/*  TRANSACT.C functions
 */
 #ifdef TFAT
void    InitFATs(PVOLUME pvol);
DWORD   SyncFATs(PVOLUME pvol);
void	FreezeClusters( PVOLUME pvol, DWORD clusFirst, DWORD clusLast); 
DWORD	FreeFrozenClusters( PVOLUME pvol ); 
BOOL	IsNewCluster( PVOLUME pvol, DWORD clus); 
DWORD   CloneDirCluster( PVOLUME pvol, PDSTREAM pstm, DWORD blkOld, PDWORD clusNew );
DWORD   UpdateStreamDirBlk( PDSTREAM pstm, DWORD oldBlk, DWORD newBlk );
DWORD   ReplaceCluster( PVOLUME pvol, DWORD clusOld, DWORD clusNew, DWORD clusFirst);
DWORD	CloneStream(PDSTREAM pstm, DWORD pos, DWORD len);
DWORD   ChangeTransactionStatus(PVOLUME pvol, WORD ts);
DWORD   CommitTransactions(PVOLUME pvol);
DWORD	UpdateDirEntryCluster(PDSTREAM pstm);

// Return value for CloneDirCluster
#define CLONE_CLUSTER_COMPLETE -1

#endif

/*  FILESYS.C functions (in FILESYS.EXE)
 */

#if !defined(INVALID_AFS)       
#if OID_FIRST_AFS != 0
#define INVALID_AFS     0
#else
#define INVALID_AFS     -1
#endif
#endif


/*  Globals in API.C
 */

extern  CONST WCHAR awcFATFS[];
extern  CONST WCHAR awcCompVolID[];
extern  CONST WCHAR awcUpdateAccess[];
extern  CONST WCHAR awcPathCacheEntries[];

extern  HINSTANCE   hFATFS;
extern  int         cLoads;             // count of DLL_PROCESS_ATTACH's
#ifdef SHELL_MESSAGE_NOTIFICATION
extern  HWND        hwndShellNotify;    // from FAT_RegisterFileSystemNotification
#endif
#ifdef SHELL_CALLBACK_NOTIFICATION
extern  SHELLFILECHANGEFUNC_t pfnShell; // from FAT_RegisterFileSystemFunction
#endif


extern  DSK_DLINK   dlDisks;            // keeps track of every open FAT device
extern  DWORD       cFATThreads;
extern  HANDLE      hevStartup;
extern  HANDLE      hevShutdown;
extern  CRITICAL_SECTION csFATFS;

#ifdef DEBUG
extern  int         cbAlloc;            // total bytes allocated
extern  CRITICAL_SECTION csAlloc;
#endif


/*  Globals in BUFFER.C
 */

extern  DWORD       cbBuf;              // buffer size (computed at run-time)
extern  DWORD       cBufThreads;        // number of threads buffer pool can handle
extern  DWORD       cBufThreadsOrig;    // number of threads buffer pool can handle
extern  HANDLE      hevBufThreads;      // auto-reset event signalled when another thread can be handled
extern  CRITICAL_SECTION csBuffers;     // buffer pool serialization critical section

#ifdef  DEBUG                           // expose these globals for assertion checks
extern  DWORD       cbufTotal;          // total number of buffers
extern  DWORD       cbufError;          // total buffers with outstanding write errors
#endif

#if (defined(INCLUDE_FATFS) | defined(TFAT))
//  need it in TFAT transact.c
extern  BUF_DLINK   dlBufMRU;           // MRU list of buffers
#endif

//Power break definition for test scenarios
#ifdef TFAT_PWR_TEST
    #define ONE_SECOND                  1000
    #define REG_KEY_POWERTEST           L"\\Software\\Microsoft\\PowerTests"
    #define VALUE_ID                    L"CurrentTestID" 

    #define PWR_BREAK_NOTIFY(x)\
    {\
        HKEY hKey;\
        if(ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, REG_KEY_POWERTEST, 0, KEY_ALL_ACCESS, & hKey))\
        {\
            DWORD dwType; \
            DWORD dwData = 0;\
            DWORD cbData = sizeof(dwData);\
            if(ERROR_SUCCESS == RegQueryValueEx( hKey, VALUE_ID, NULL, &dwType, (PBYTE)&dwData , &cbData))\
                if(x == dwData)\
                {\
                    RETAILMSG(TRUE, (TEXT ("PWR_OUTPUT:Test power interrupt at file %s line %d\r\n"),  TEXT(__FILE__), __LINE__));\
                    RETAILMSG(TRUE,  (TEXT("PWR_OUTPUT:PWR_INTR_ID:%d\r\n"), x));\
                    Sleep(3*ONE_SECOND);  \
                    RETAILMSG( TRUE,  (TEXT("PWR_OUTPUT:PWR_INTR FAILED")));\
                }\
        }\
    }
#else
    #define PWR_BREAK_NOTIFY(x)
#endif


#pragma check_stack(off)                

#endif /* FATFS_H */
