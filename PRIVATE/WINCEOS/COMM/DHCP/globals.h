//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#endif	// _GLOBALS_H_


