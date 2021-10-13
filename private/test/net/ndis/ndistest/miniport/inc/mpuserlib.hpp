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
#ifndef _MP_USERLIB_H
#define _MP_USERLIB_H

#define MAX_GUID_STRING_SZ              80
#define MAX_NAME_STRING_SZ				64

#include "GlbConsts.hpp"

#include "mpdrvstk.hpp"
#include "wincrypt.h"

//This function may be called by a test case to 
// generate a random name of a certain length.
extern BOOL GenerateMiniportName(PWSTR wszMiniportName, UINT uiLength, DWORD *pdwErr);

//This function may be called by a test case to cause NDIS to load
//the virtual miniport driver that uses the specified NDIS version
//Currently only NDIS 5.1 and 6 are supported.
extern BOOL LoadMiniportDriver(VMPVersion version);

//This function may be called by a test case to cause NDIS to initialize
//an adapter instance
extern BOOL LoadMiniportAdapter(VMPVersion version, 
								PWSTR wszAdapterName);

//This function may be called by a test case to cause NDIS to deregister
//an adapter instance
extern BOOL UnloadMiniportAdapter(PWSTR wszAdapterName);

//Use this function to obtain a list of all active virtual miniport adapters
extern NTSTATUS GetActiveMiniportAdapters(PWSTR wszAdapterListBuffer, PDWORD cbBuffer);

//Use this function to obtain a list of specifically named virtual miniport adapters
extern NTSTATUS GetSpecificMiniportAdapters(PWSTR wszAdapterListBuffer, PDWORD cbBuffer);

//Use this function to obtain a context handle to a VMP adapter using its name
extern BOOL GetAdapterContextFromName(VMPVersion version, PWSTR wszAdapterName, PVOID *ppContext);

//Use this function to check if a specified adapter is active
//this is case sensitive
extern BOOL IsAdapterActive(PWSTR wszAdapterName);

BOOL GenerateGUIDString(LPTSTR  szGuidString);

STATIC VOID GetMiniportName(VMPVersion version, PWSTR wszMiniportName, DWORD cchMiniportName);

STATIC LONG GetAdapterGuid(LPTSTR szAdapterName, PTCHAR szAdapterGuid, PDWORD pcbGuid);

extern BOOL SetupAdapters(VMPVersion AdapterVersion, PWSTR AdapterName, int numAdapters, BOOL bUseDefault);

extern BOOL SetupNamedAdapter(VMPVersion AdapterVersion, PWSTR szAdapterName, BOOL bUseDefault);

// Sets up one adapter exactly as named
extern BOOL SetupAdapterExact(VMPVersion AdapterVersion, PWSTR AdapterName, BOOL bUseDefault);

// Causes NDIS to offer all active virtual miniports for binding to protocols
extern BOOL BindAllVirtualMiniports(void);

STATIC BOOL DoNdisIOControl(
	DWORD	dwCommand,
	LPVOID	pInBuffer,
	DWORD	cbInBuffer,
	LPVOID	pOutBuffer,
	DWORD	*pcbOutBuffer	OPTIONAL);

//BOOL CreateFilterInstance(const litexml::XmlBaseElement_t *pFilterInstance, PNDIS_FILTER_INTERFACE pFilter);
#endif //_MP_USERLIB_H