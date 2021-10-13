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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//-***********************************************************************
//
//  This file contains all of the code that sits under an IP interface
//  and does the necessary multiplexing/demultiplexing for WAN connections.
//
//-***********************************************************************


#include <hosts_helper.hxx>

#include "windows.h"
#include "cxport.h"
#include "types.h"



// IP Header files to bind to the LLIP interface
#include "ndis.h"
#include "ntddip.h"
#include "ip.h"
#include "llipif.h"
#include "tdiinfo.h"
#include "ipinfo.h"
#include "llinfo.h"
#include "tdistat.h"
#include "cellip.h"

#include "memory.h"
#include "afdfunc.h"
#include "cclib.h"

#include "iphlpapi.h"

#define _TCPIP_H_NOIP
#include "tcpip.h"
#include "vjcomp.h"

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "mac.h"
#include "ip_intf.h"


extern "C"
void
SetPPPPeerIPAddress(
	DWORD dwPeerIPAddress)
//
//	Set the IP address that the name "ppp_peer" will resolve to.
//	Note that this name is only present for internal use.
//
{
    SetIPForHost (L"ppp_peer", htonl(dwPeerIPAddress));
}

