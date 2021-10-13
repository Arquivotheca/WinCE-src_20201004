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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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