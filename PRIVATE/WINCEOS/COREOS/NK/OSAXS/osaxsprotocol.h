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

    OsAxsProtocol.h

Abstract:

    This file details the protocol used to communicate between the
    host side OsAccess service and the target side OsAccess service.

--*/

#pragma once
#ifndef OSAXS_PROTOCOL_H
#define OSAXS_PROTOCOL_H

#include "osaxs_common.h"

#pragma pack(push, 4)

/*
 * OsAccess error codes
 */
#define OSAXS_E_PROTOCOLVERSION     MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F00)   /* Notify OsAxs Kdbg code that the version is incorrect. */
#define OSAXS_E_APINUMBER           MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F01)   /* Bad api number was passed to OsAccess */
#define OSAXS_E_VANOTMAPPED         MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F02)   /* Unable to find the Virtual address in the address table */
#define OSAXS_E_INVALIDTHREAD       MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F03)   /* Bad thread pointer. */

/*
 * This is the OsAccess Protocol version number.  This version number is automatically transmitted
 * in all packets during communication.
 */
#define OSAXS_PROTOCOL_LATEST_VERSION 0

/*
 * OsAccess API Numbers
 */
enum
{
    OSAXS_API_GET_FLEXIPTMINFO              = 0x0001,
    OSAXS_API_GET_STRUCT                    = 0x0002,
    OSAXS_API_GET_PHYSADDR                  = 0x0003,
    OSAXS_API_SET_PHYSADDR                  = 0x0004,
    OSAXS_API_GET_THREADCTX                 = 0x0005,
    OSAXS_API_SET_THREADCTX                 = 0x0006,
    OSAXS_API_TRANSLATE_RETURN              = 0x0007,
    OSAXS_API_GET_MOD_O32_LITE              = 0x0008,
    OSAXS_API_GET_EXCEPTION_REGISTRATION    = 0x0009,    
};

/*
 * OsAxsGetOsStruct
 *
 * OsAccess allows us to do in one transaction, what would require a few memory reads on
 * OS side
 */
enum
{
    OSAXS_STRUCT_PROCESS        = 1,
    OSAXS_STRUCT_THREAD         = 2,
    OSAXS_STRUCT_MODULE         = 3,
};

typedef struct OSAXS_GETOSSTRUCT
{
    DWORD64 StructAddr;
    DWORD   dwStructType;
    DWORD   dwStructHandle;
} OSAXS_GETOSSTRUCT;

/*
 * OSAXS_VM_ADDR
 */
typedef struct OSAXS_VM_ADDR
{
    DWORD64 VirtualAddr;
    DWORD64 NewPhysicalAddr;
    DWORD64 PhysicalAddr;
} OSAXS_VM_ADDR;

/*
 * OSAXS_MODULE_O32_LITE
 */
typedef struct OSAXS_MODULE_O32_LITE
{
    DWORD in_hmod;
    DWORD out_cO32Lite;
}OSAXS_MODULE_O32_LITE;

/*
 * OSAXS_API_GET_EXCEPTION_REGISTRATION
 */
typedef struct OSAXS_API_GET_EXCEPTION_REGISTRATION
{
    DWORD rgdwBuf[2];
} OSAXS_EXCEPTION_REGISTRATION;

/*
 * OsAxs Protocol Structure.  This structure is padded out to resemble
 * the DBGKD_COMMAND structure.
 */
typedef struct OSAXS_COMMAND
{
    DWORD dwApi;
    DWORD dwVersion;
    DWORD dwPad0;
    HRESULT hr;
    DWORD dwPad1[4];
    union
    {
        FLEXI_REQUEST FlexiReq;
        OSAXS_GETOSSTRUCT Struct;
        OSAXS_VM_ADDR Vm;
        ULONG64 Addr;
        OSAXS_MODULE_O32_LITE ModO32;
        OSAXS_EXCEPTION_REGISTRATION ExReg;
        BYTE rgbPad2[48];
    } u;
    
} OSAXS_COMMAND;

#pragma pack(pop)

#endif
