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
#pragma once
#ifndef OSAXS_COMMON_H
#define OSAXS_COMMON_H

#pragma pack (push, 4)

/* Types for GetFPTMI */
enum _FLEXI_REQ_TYPES
{
    FLEXI_REQ_PROCESS_HDR   = 0x00000001,
    FLEXI_REQ_PROCESS_DESC  = 0x00000002,
    FLEXI_REQ_PROCESS_DATA  = 0x00000004,
    FLEXI_REQ_PROCESS_ALL   = 0x00000007,
    FLEXI_REQ_PROCESS_TOTAL = 0x00010000,

    FLEXI_REQ_THREAD_HDR    = 0x00000008,
    FLEXI_REQ_THREAD_DESC   = 0x00000010,
    FLEXI_REQ_THREAD_DATA   = 0x00000020,
    FLEXI_REQ_THREAD_ALL    = 0x00000038,
    FLEXI_REQ_THREAD_TOTAL  = 0x00020000,

    FLEXI_REQ_MODULE_HDR    = 0x00000040,
    FLEXI_REQ_MODULE_DESC   = 0x00000080,
    FLEXI_REQ_MODULE_DATA   = 0x00000100,
    FLEXI_REQ_MODULE_ALL    = 0x000001c0,
    FLEXI_REQ_MODULE_TOTAL  = 0x00040000,

    FLEXI_REQ_CONTEXT_HDR   = 0x00000200,
    FLEXI_REQ_CONTEXT_DESC  = 0x00000400,
    FLEXI_REQ_CONTEXT_DATA  = 0x00000800,
    FLEXI_REQ_CONTEXT_ALL   = 0x00000e00,
    FLEXI_REQ_CONTEXT_TOTAL = 0x00080000,

    FLEXI_REQ_MODPROC_HDR   = 0x00001000,
    FLEXI_REQ_MODPROC_DESC  = 0x00002000,
    FLEXI_REQ_MODPROC_DATA  = 0x00004000,
    FLEXI_REQ_MODPROC_ALL   = 0x00007000,
    FLEXI_REQ_MODPROC_TOTAL = 0x00100000,

    FLEXI_FILTER_PROCESS_HANDLE   = 0x80000000,
    FLEXI_FILTER_THREAD_HANDLE    = 0x40000000,
    FLEXI_FILTER_MODULE_HANDLE    = 0x20000000,
    FLEXI_FILTER_PROCESS_POINTER  = 0x10000000,
    FLEXI_FILTER_THREAD_POINTER   = 0x08000000,
    FLEXI_FILTER_MODULE_POINTER   = 0x04000000,
    FLEXI_FILTER_INC_NK           = 0x02000000,      // Include nk.exe in modules list or processes list
    FLEXI_FILTER_INC_PROC         = 0x01000000,      // Include additional process in modules list or processes list (handle == dwHMod)
    FLEXI_FILTER_PROCESS_INC_NK   = 0x00800000,      // Include nk.exe in process list 
    FLEXI_FILTER_PROCESS_INC_PROC = 0x00400000,      // Include additional process in process list (handle == dwHMod)
};


typedef struct _FLEXI_RANGE
{
    DWORD dwStart;
    DWORD dwEnd;
} FLEXI_RANGE;

typedef struct _FLEXI_REQUEST
{
    DWORD dwRequest;
    FLEXI_RANGE Proc;
    DWORD dwHProc;
    FLEXI_RANGE Thread;
    DWORD dwHThread;
    FLEXI_RANGE Mod;
    DWORD dwHMod;
} FLEXI_REQUEST;


typedef struct FLEXI_FIELD_DESC
{
    DWORD dwId;
    DWORD dwSize;
    DWORD dwLabelRVA;
    DWORD dwFormatRVA;
} FLEXI_FIELD_DESC;


// NOTE on format strings for field descriptors
// the printf format is supported except the following:
// -Exceptions:
//      -no I64 in the prefix
//      -no * for width
//      -no * for precision
// -Additions:
//      -%T{N=BitFieldNameN, M=BitFieldNameM...} for bitfield description
//          where bitnumbers (N and M) are in [0..63] and BitFieldNameN and BitFieldNameM are strings of char with no ","
//          if bitnumber in [0..31], the BitfieldName will be display for bitnumber == 1
//          if bitnumber in [32..63], the BitfieldName will be display for bitnumber == 0
//          Any non described bit will be ignored
//          Will display all set bitfield separated by a ,
//      -%N{N=EnumElementNameN, M=EnumElementNameM...} for enumeration description
//          where N and M are decimal DWORD value and EnumElementNameN and EnumElementNameM are strings of char with no ","
//          Any non described enum value will be ignored


#define CCH_CMDLINE 128
#define CCH_PROCNAME 260
#define CCH_MODULENAME 260
#define CCH_PDBNAME 260

enum
{
    CE600_SMP_CAPABLE = 0x80000000
};

//
// Version 2 of the data block just includes an additional flag
//
// Future changes should just add additional members to the end
// of this structure. OsAxs will determine functionality based on cbSize.
//

enum OSAXS_SUBVERSION
{
    OSAXS_SUBVER_BASE              = 0,     // Base version
    /* Windows CE Embedded 6.0 */
    OSAXS_SUBVER_600_CURTHREADPCSTKTOP = 1, // Support for current thread keeping frame for CaptureContext.  Kernel needs it to function correctly.

    /* Currently supported OSAXS subversion */
    OSAXS_SUBVER_CURRENT = OSAXS_SUBVER_BASE
};

// {A839A8E7-77E2-49e3-BD98-F6D91559A609}
#define SIGNATURE_OSAXS_KERN_POINTERS_2 { 0xa839a8e7, 0x77e2, 0x49e3, { 0xbd, 0x98, 0xf6, 0xd9, 0x15, 0x59, 0xa6, 0x9 } }
typedef struct _OSAXS_KERN_POINTERS_2
{
    GUID guidSignature;
    DWORD cbSize;                       // Size of this struct (_OSAXS_KERN_POINTERS_2)
    DWORD dwPtrKDataOffset;             // 16
    DWORD dwProcArrayOffset;            // g_pprcNK
    DWORD dwPtrHdstubNotifOffset;
    DWORD dwPtrHdstubEventOffset;       // Unused, now located on stack.
    DWORD dwPtrHdstubTaintOffset;       // 32
    DWORD dwPtrHdstubFilterOffset;
    DWORD dwPre600_MD_CBRtnOffset;
    DWORD dwSystemAPISetsOffset;
    DWORD dwMemoryInfoOffset;           
    DWORD dwPtrRomHdrOffset;            // 48
    BOOL  fHdstubLoaded;                // Must equal TRUE, and nothing else.
    DWORD dwPtrOEMAddressTable;
    DWORD dwPtrHdSharedGlobals;
    DWORD dwOsMajorVersion;             // Version of OS
    DWORD dwOsMinorVersion;
    DWORD dwOsAxsSubVersion;            // Track changes between daily builds of OS.

    /* For SMP support */
    DWORD dwPointerToPcbArray;
    DWORD dwMaximumPcbs;
    DWORD dwImageBase;
    DWORD dwImageSize;
    DWORD dwRelocDataSec;
    DWORD dwDataSecLen;
} OSAXS_KERN_POINTERS_2;
 
// {0xf2e8583c-0x37e2-0x4fb6-a1048b4c7bf2ae4c}
#define SIGNATURE_OSAXS_HWTRAP { 0xf2e8583c, 0x37e2, 0x4fb6, { 0xa1, 0x4, 0x8b, 0x4c, 0x7b, 0xf2, 0xae, 0x4c } }
typedef struct _OSAXS_HWTRAP
{
    GUID guidSignature;
    DWORD cbSize;
    DWORD rgHwTrap[16];
    DWORD dwImageBase;
    DWORD dwImageSize;
    DWORD dwRelocDataSec;
    DWORD dwDateSecLen;
    DWORD dwPtrOsAxsKernPointers;
    DWORD __Unused;
} OSAXS_HWTRAP;

#pragma pack (pop)

#endif
