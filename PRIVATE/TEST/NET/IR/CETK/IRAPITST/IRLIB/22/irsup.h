//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//  ***********************************************************
//  *** IR Support Library
//  ***********************************************************

extern void
IRPrintIASSet(
	IAS_SET *pSet);

extern void
IRPrintIASQueryRequest(
	IAS_QUERY *pQuery);

extern void
IRPrintIASQueryResult(
	IAS_QUERY *pQuery);

extern void
IRPrintDeviceID(
	u_char	*pDeviceID);

extern void
IRPrintDeviceList(
	DEVICELIST	*pDevList);

extern DEVICELIST *
IREnumDeviceList(
	int		maxDevices);

extern int
IRFindNewestDevice(
	u_char *pDeviceID,
	int		maxDevices);

extern void
IRIasQuery(
	SOCKET	   s,
	IAS_QUERY *pQuery,
	int		  *pResult,
	int		  *pWSAError);

extern void
IRIasSet(
	SOCKET	   s,
	IAS_SET   *pSet,
	int		  *pResult,
	int		  *pWSAError);

extern void
IRBind(
	SOCKET	   s,
	char	  *strServerName,
	int		  *pResult,
	int		  *pWSAError);

extern void
IRConnect(
	SOCKET	   s,
	char	  *strServerName,
	int		  *pResult,
	int		  *pWSAError);

extern int
IRSockInit(void);