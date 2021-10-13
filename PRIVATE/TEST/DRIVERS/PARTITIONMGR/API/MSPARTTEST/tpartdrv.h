//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TuxMain.H"

#ifndef __TPARTDRV_H__
#define __TPARTDRV_H__

// These function pointer definitions and function wrappers exist to make this
// test able to test a partition driver without linking to it.
//
// The partition driver dll name can be specified on the command line.

// some definitions for clarity
typedef DWORD STOREID, *PSTOREID;
typedef DWORD SEARCHID, *PSEARCHID;
typedef DWORD PARTID, *PPARTID;

#define INVALID_STOREID     0xFFFFFFFF
#define INVALID_SEARCHID    0xFFFFFFFF
#define INVALID_PARTID      0xFFFFFFFF

// function pointer definitions for partition driver exports
typedef DWORD (*PFN_OPENSTORE) (HANDLE, LPDWORD);
typedef DWORD (*PFN_CLOSESTORE) (DWORD);
typedef DWORD (*PFN_FORMATSTORE) (DWORD);
typedef DWORD (*PFN_ISSTOREFORMATTED) (DWORD);
typedef DWORD (*PFN_GETSTOREINFO) (DWORD, PPD_STOREINFO);
typedef DWORD (*PFN_CREATEPARTITION) (DWORD, LPCTSTR, BYTE, SECTORNUM, BOOL);
typedef DWORD (*PFN_DELETEPARTITION) (DWORD, LPCTSTR);
typedef DWORD (*PFN_RENAMEPARTITION) (DWORD, LPCTSTR, LPCTSTR);
typedef DWORD (*PFN_SETPARTITIONATTRS) (DWORD, LPCTSTR, DWORD);
typedef DWORD (*PFN_FORMATPARTITION) (DWORD, LPCTSTR, BYTE, BOOL);
typedef DWORD (*PFN_GETPARTITIONINFO) (DWORD, LPCTSTR, PPD_PARTINFO);
typedef DWORD (*PFN_FINDPARTITIONSTART) (DWORD, LPDWORD);
typedef DWORD (*PFN_FINDPARTITIONNEXT) (DWORD, PPD_PARTINFO);
typedef VOID  (*PFN_FINDPARTITIONCLOSE) (DWORD);
typedef DWORD (*PFN_OPENPARTITION) (DWORD, LPCTSTR, LPDWORD);
typedef VOID  (*PFN_CLOSEPARTITION) (DWORD);
typedef DWORD (*PFN_DEVICEIOCONTROL) (DWORD, DWORD, PBYTE, DWORD, PBYTE, DWORD, PDWORD);

// load and unload functions
BOOL LoadPartitionDriver(LPCTSTR szDriverName);
VOID FreePartitionDriver();

// these are really just wrapper functions for the partition driver calls 
// to make them easier to deal with
STOREID T_OpenStore(HANDLE hDisk);
VOID T_CloseStore(STOREID storeId);
BOOL T_FormatStore(STOREID storeId);
BOOL T_IsStoreFormatted(STOREID storeId);
BOOL T_GetStoreInfo(STOREID storeId, PD_STOREINFO *pInfo);
BOOL T_CreatePartition(STOREID storeId, LPCTSTR szPartName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto);
BOOL T_DeletePartition(STOREID storeId, LPCTSTR szPartName);
BOOL T_RenamePartition(STOREID storeId, LPCTSTR szOldName, LPCTSTR szNewName);
BOOL T_SetPartitionAttrs(STOREID storeId, LPCTSTR szPartName, DWORD dwAttr);
BOOL T_FormatPartition(STOREID storeId, LPCTSTR szPartName, BYTE bPartType, BOOL bAuto);
BOOL T_GetPartitionInfo(STOREID storeId, LPCTSTR szPartName, PD_PARTINFO *pInfo);
SEARCHID T_FindPartitionStart(STOREID storeId);
BOOL T_FindPartitionNext(SEARCHID searchId, PD_PARTINFO *pInfo);
VOID T_FindPartitionClose(SEARCHID searchId);
PARTID T_OpenPartition(STOREID storeId, LPCTSTR szPartName);
VOID T_ClosePartition(PARTID partId);
BOOL T_DeviceIoControl(PARTID partId, DWORD dwCode, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);

#endif // __TPARTDRV_H__
