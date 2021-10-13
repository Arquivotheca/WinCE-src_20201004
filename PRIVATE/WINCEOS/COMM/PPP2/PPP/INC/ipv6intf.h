//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
//
//	This file declares the interfaces for the IPv6 to PPP component.
//


#ifndef IPV6INTF_H
#define IPV6INTF_H


DWORD PPPIPV6InterfaceInitialize();
BOOL  PPPAddIPV6Interface(PPP_CONTEXT *pContext);
void  PPPDeleteIPV6Interface(PPP_CONTEXT *pContext);

void
PppIPV6Receive(
	void		*context,
	pppMsg_t	*pMsg);

extern HINSTANCE g_hIPV6Module;

#endif
