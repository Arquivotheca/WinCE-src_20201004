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
/*****************************************************************************/
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	globals.h

  DESCRIPTION:
	global Dhcp data


*/

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "dhcp.h"


extern PFNVOID *v_apSocketFns;
extern PFNVOID *v_apAfdFns;

EXTERNAL_LOCK(v_GlobalListLock)
extern LIST_ENTRY v_EstablishedList;
extern HANDLE v_hListEvent;

extern CRITICAL_SECTION v_ProtocolListLock;
extern PDHCP_PROTOCOL_CONTEXT v_ProtocolList;

//extern PFNSetDHCPNTE	pfnSetDHCPNTE;
//extern PFNIPSetNTEAddr pfnIPSetNTEAddr;


typedef BOOL  (*PFNAfdAddInterface)(
	PTSTR pAdapter, void *Nte, DWORD Context, int Flags,
	uint IpAddr, uint SubnetMask,
	int cDns, uint *pDns, int cWins, uint *pWins);

extern PFNAfdAddInterface pfnAfdAddInterface;

#endif	// _GLOBALS_H_


