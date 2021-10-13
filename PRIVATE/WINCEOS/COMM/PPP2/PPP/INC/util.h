//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//


#ifndef UTIL_H
#define UTIL_H


BOOL
utilFindEUI(
	IN	DWORD	dwSkipCount,
	IN	BOOL	bNeedDhcpEnabled,
	OUT	PBYTE	pMacAddress,
	IN	DWORD	bmMacAddressSizesWanted,
	OUT	PDWORD	pcbMacAddress,
	OUT	PDWORD	pdwIfIndex OPTIONAL);

BOOL
utilGetEUI(
	IN	DWORD	dwSkipCount,
	OUT	PBYTE	pMacAddress,
	OUT	PDWORD	pcbMacAddress);

BOOL
utilGetDeviceIDHash(
	OUT	    PBYTE	pID,
	IN  OUT	PDWORD	pcbID);

#endif
