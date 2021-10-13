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
#ifndef BINFS_H
#define BINFS_H

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif



#include <windows.h>
#include <tchar.h>
#include <types.h>
#include <excpt.h>
#include <memory.h>
#include <diskio.h>
#include <storemgr.h>
#include <pehdr.h>
#include <romldr.h>
#include <fsdmgr.h>
#include <debug.h>
#include <pwinbase.h> // MatchesWildcardMask

#define DEFAULT_VOLUME_NAME     TEXT("BINFS")
#define MAX_REGIONS             64


#define COMP_BLOCK_SIZE 4096

#define CHAIN_TYPE_XIP  1
#define CHAIN_TYPE_BIN  2

#define VOL_FLAG_XIP    1

// Because the high 4-bits of dwLowOffset passed to ReadFileWithSeek are used to
// indicate the module section to read, there can be at most 16 addressable
// portable executable file sections in a BINFS module.
#define MAX_O32_SECTIONS 16

typedef struct _ChainData {
    DWORD dwAddress;
    DWORD dwLength;
    WORD  wOrder;
    WORD  wFlags;
    DWORD dwReserved;
    DWORD dwType;
    ROMHDR Toc;
    LPVOID pDirectory;
    LPVOID pDirectoryLast;
    DWORD  dwBinOffset;
} ChainData;

// Structures
typedef struct _BinDirList {
    WCHAR   *szFileName;
    DWORD   dwRegion;
    DWORD   dwRealFileSize;
    DWORD   dwCompFileSize;
    DWORD   dwAttributes;
    FILETIME ft;
    DWORD   dwAddress;
    e32_rom *pe32;
    o32_rom *po32;
    struct _BinDirList *pNext;
} BinDirList, *PBinDirList;


typedef struct
{
    HVOL            hVolume;
    HDSK            hDsk;
    ChainData       *pChain;        
    DISK_INFO       diskInfo;
    DWORD           dwVolFlags;     // per-volume flags, currently VOL_FLAG_XIP
    DWORD           dwNumRegions;   // Number of Bin Regions in the volume
    BinDirList      *pDirectory;    // Directory List
                                    // Compression support
    CRITICAL_SECTION csComp;        // Protects current compression state
    BYTE            *pReadBuf; // Compressed buffer
    BYTE            pDecBuf[COMP_BLOCK_SIZE]; // Decompression Buffer;
    BinDirList      *pCurCompDir;    // Current directory entry being decompressed
    DWORD           dwCurCompBlock;  // Current Block being decompressed
} BinVolume;

typedef struct _SEARCHHANDLE {
    BinDirList *pDirectory;
    WCHAR       szFileMask[MAX_PATH];
} SearchHandle;

typedef struct _FILEHANDLE {
    BinDirList  *pDirectory;
    BinVolume   *pVolume;
    DWORD       dwPosition;
    DWORD       dwShareMode;
} FileHandle;
// Globals

#define SYSTEM_DIR L"windows"
#define SYSTEM_DIR_LEN 7

// Function Prototypes
DWORD AdjustOffset(BinVolume *pVolume, DWORD dwRegion, DWORD dwOffset);
BOOL ReadAtOffset(BinVolume *pVolume, LPVOID buffer,DWORD nBytesToRead, LPDWORD lpNumBytesRead, DWORD dwOffset);
DWORD GetFileData(BinVolume *pVolume, BinDirList *pDirectory, DWORD dwPosition, BYTE *pBuffer, DWORD dwBytesToRead);

#endif /* BINFS_H */
