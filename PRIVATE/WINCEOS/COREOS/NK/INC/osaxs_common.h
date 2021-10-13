//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#ifndef OSAXS_COMMON_H
#define OSAXS_COMMON_H

#pragma pack (push, 4)

/* Types for GetFPTMI */
enum _FLEXI_REQ_TYPES
{
    FLEXI_REQ_PROCESS_ALL = 0x7,
    FLEXI_REQ_PROCESS_HDR = 0x1,
    FLEXI_REQ_PROCESS_DESC = 0x2,
    FLEXI_REQ_PROCESS_DATA = 0x4,
    FLEXI_REQ_PROCESS_TOTAL = 0x00010000,

    FLEXI_REQ_THREAD_ALL = 0x38,
    FLEXI_REQ_THREAD_HDR = 0x8,
    FLEXI_REQ_THREAD_DESC = 0x10,
    FLEXI_REQ_THREAD_DATA = 0x20,
    FLEXI_REQ_THREAD_TOTAL = 0x00020000,

    FLEXI_REQ_MODULE_ALL = 0x1c0,
    FLEXI_REQ_MODULE_HDR = 0x40,
    FLEXI_REQ_MODULE_DESC = 0x80,
    FLEXI_REQ_MODULE_DATA = 0x100,
    FLEXI_REQ_MODULE_TOTAL = 0x00040000,

    FLEXI_REQ_CONTEXT_ALL = 0xe00,
    FLEXI_REQ_CONTEXT_HDR = 0x200,
    FLEXI_REQ_CONTEXT_DESC = 0x400,
    FLEXI_REQ_CONTEXT_DATA = 0x800,
    FLEXI_REQ_CONTEXT_TOTAL = 0x00080000,

    FLEXI_FILTER_PROCESS_HANDLE = 0x80000000,
    FLEXI_FILTER_THREAD_HANDLE =  0x40000000,
    FLEXI_FILTER_MODULE_HANDLE =  0x20000000,
    FLEXI_FILTER_PROCESS_POINTER = 0x10000000,
    FLEXI_FILTER_THREAD_POINTER  = 0x08000000,
    FLEXI_FILTER_MODULE_POINTER  = 0x04000000,
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


/* Portable structures for OS types */
#pragma pack (push, 1)

#define CCH_PROCNAME 32
#define CCH_CMDLINE 128
typedef struct OSAXS_PORTABLE_PROCESS
{
    BYTE bSlot;
    char szName[CCH_PROCNAME];
    DWORD dwVMBase;
    DWORD dwAccessKey;
    BYTE bTrustLevel;
    DWORD dwHandle;
    DWORD dwBasePtr;
    DWORD dwTlsUseLow;
    DWORD dwTlsUseHigh;
    DWORD dwZoneMask;
    DWORD dwAddr;
    char szCommandLine[CCH_CMDLINE];
} OSAXS_PORTABLE_PROCESS;

#define CCH_MODULENAME 32
typedef struct OSAXS_PORTABLE_MODULE
{
    char szModuleName[CCH_MODULENAME];
    DWORD dwBasePointer;
    DWORD dwModuleSize;
    DWORD dwRdWrDataStart;
    DWORD dwRdWrDataEnd;
    DWORD dwTimeStamp;
    DWORD dwPdbFormat;
    GUID  PdbGuid;
    DWORD dwPdbAge;
    DWORD dwDllHandle;
    DWORD dwInUse;
    WORD  wFlags;
    BYTE  bTrustLevel;
    WORD  rgwRefCount[MAX_PROCESSES];
    DWORD dwStructAddr;
} OSAXS_PORTABLE_MODULE;

typedef struct OSAXS_PORTABLE_THREAD
{
    DWORD dwAddr;
    WORD wRunState;
    WORD wInfo;
    DWORD dwHandle;
    BYTE bWaitState;
    DWORD dwAccessKey;
    DWORD dwCurProcHandle;
    DWORD dwOwnProcHandle;
    BYTE bCurPrio;
    BYTE bBasePrio;
    DWORD dwKernelTime;
    DWORD dwUserTime;
    DWORD dwQuantum;
    DWORD dwQuantumLeft;
    DWORD dwSleepCount;
    BYTE bSuspendCount;
    DWORD dwTlsPtr;
    DWORD dwLastError;
    DWORD dwStackBase;
    DWORD dwStackLowBound;
    DWORD dwCreationTimeHi;
    DWORD dwCreationTimeLo;
    DWORD dwCurrentPC;
} OSAXS_PORTABLE_THREAD;

#pragma pack (pop)

//
// This structure contains all relevant information that OsAccess needs to know in the hardware debugging case
// if the symbolic information for NK.EXE is not available.  This allows OsAccess to gain access to 
// structures within NK.EXE and HD.DLL without symbolic information for NK.EXE
//
typedef struct _OSAXS_KERN_POINTERS_1
{
    DWORD dwSignature;
    DWORD cbSize;
    DWORD dwVersion;
    DWORD dwSum;
    DWORD dwPtrKDataOffset;
    DWORD dwProcArrayOffset;
    DWORD dwPtrHdstubNotifOffset;
    DWORD dwPtrHdstubEventOffset;
    DWORD dwPtrHdstubTaintOffset;
    DWORD dwPtrHdstubFilterOffset;
    DWORD dwMD_CBRtnOffset;
    DWORD dwSystemAPISetsOffset;
    DWORD dwMemoryInfoOffset;
    DWORD dwPtrRomHdrOffset;
} OSAXS_KERN_POINTERS_1;

#define SIGNATURE_OSAXS_KERN_POINTERS   ((DWORD)'DKKN')
// High Word: OS Version
// Low Word: Incremental changes in structure.
#define VERSION_OSAXS_KERN_POINTERS_1     0x05000000


//
// Version 2 of the data block just includes an additional flag
//

// {A839A8E7-77E2-49e3-BD98-F6D91559A609}
#define SIGNATURE_OSAXS_KERN_POINTERS_2 { 0xa839a8e7, 0x77e2, 0x49e3, { 0xbd, 0x98, 0xf6, 0xd9, 0x15, 0x59, 0xa6, 0x9 } }
typedef struct _OSAXS_KERN_POINTERS_2
{
    GUID guidSignature;
    DWORD cbSize;
    DWORD dwPtrKDataOffset;             // 16
    DWORD dwProcArrayOffset;
    DWORD dwPtrHdstubNotifOffset;
    DWORD dwPtrHdstubEventOffset;
    DWORD dwPtrHdstubTaintOffset;       // 32
    DWORD dwPtrHdstubFilterOffset;
    DWORD dwMD_CBRtnOffset;
    DWORD dwSystemAPISetsOffset;
    DWORD dwMemoryInfoOffset;           // 48
    DWORD dwPtrRomHdrOffset;
    BOOL  fHdstubLoaded;                // Must equal TRUE, and nothing else.
    DWORD dwNextDataBlock;              
} OSAXS_KERN_POINTERS_2;


#pragma pack (pop)

#endif
