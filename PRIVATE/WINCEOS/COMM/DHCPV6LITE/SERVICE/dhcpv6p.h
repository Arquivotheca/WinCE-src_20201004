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
	dhcpp.h

  DESCRIPTION:
	private dhcp.h


*/

#ifndef _DHCPV6P_H_
#define _DHCPV6P_H_

//
//  System include files.
//

#include "precomp.h"
#include <cxport.h>
/*
#include <windows.h>
#include <types.h>
*/
//#include <dhcp.h>
//#include <protocol.h>

// DEBUG ZONES
#ifdef DEBUG
#define ZONE_INIT		DEBUGZONE(0)
#define ZONE_TIMER		DEBUGZONE(1)
#define ZONE_AUTOIP 	DEBUGZONE(2)
#define ZONE_UNUSED3	DEBUGZONE(3)
#define ZONE_RECV		DEBUGZONE(4)
#define ZONE_SEND		DEBUGZONE(5)
#define ZONE_REQUEST	DEBUGZONE(6)
#define ZONE_MEDIA_SENSE	DEBUGZONE(7)
#define ZONE_NOTIFY		DEBUGZONE(8)
#define ZONE_BUFFER		DEBUGZONE(9)
#define ZONE_INTERFACE	DEBUGZONE(10)
#define ZONE_MISC		DEBUGZONE(11)
#define ZONE_ALLOC		DEBUGZONE(12)
#define ZONE_FUNCTION	DEBUGZONE(13)
#define ZONE_WARN		DEBUGZONE(14)
#define ZONE_ERROR		DEBUGZONE(15)
#endif

/*
#include <types.h>
#include <winsock.h>
#include <memory.h>
//#include "ptype.h"
*/
typedef unsigned char UCHAR, *PUCHAR;
typedef unsigned short USHORT, ushort;
typedef unsigned int uint;
typedef void *PVOID;

typedef int STATUS;
/*

#include <string.h>
#include <stdarg.h>
#include "ndis.h"

#include <cxport.h>

#include "tdi.h"
#include <tdistat.h>
#include <tdice.h>
#include <ipexport.h>
#include <tdiinfo.h>
#include <tcpinfo.h>

#include "type.h"

#include "linklist.h"
#include "wsock.h"
#include "wstype.h"

#include "helper.h"	// should be in comm\inc
#include "netbios.h"
*/
//	Return Codes for Dhcp
#define	DHCP_SUCCESS		0
#define DHCP_FAILURE		1
#define DHCP_NOMEM			2
#define DHCP_NACK			3
#define DHCP_NOCARD			4
#define DHCP_COLLISION		5
#define DHCP_NOINTERFACE	6	// specified interface doesn't exist
#define DHCP_MEDIA_DISC		7
#define DHCP_DELETED		8

//	OpCodes for the Dhcp Function

#define	DHCP_REGISTER			0x01	// this is a must for all Helper funcs
#define DHCP_PROBE				0x02	// reserved for all helper funcs


// things which should probably be defined elsewhere
NTSTATUS RtlStringFromGUID(REFGUID Guid,
				  PUNICODE_STRING GuidString);

#define RtlNtStatusToDosError(dwError)                          dwError

VOID
RtlFreeUnicodeString(
    PUNICODE_STRING us);

#define WT_EXECUTELONGFUNCTION 0
/*
BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE Function, PVOID Context, 
    ULONG Flags);
*/


#define MemCAlloc(cBytes)   LocalAlloc(LPTR, cBytes)
#define MemFree(p)          LocalFree(p);

//
// Heap-related functions
//
#define MALLOC(s)               HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (s))
#define FREE(p)                 HeapFree(GetProcessHeap(), 0, (p))

#define QueueUserWorkItem(a, b, c) CE_ScheduleWorkItem((CTEEventRtn)a, b)

BOOL
CE_ScheduleWorkItem(CTEEventRtn pFunction, PVOID pvArg);


// BUGBUG: NYI

#define RtlIpv6StringToAddressEx(a, b, c, d) 0
#define DhcpV6InitWMI() 0
#define DhcpV6DeInitWMI() 0

#if 0
//
//  Project include files.
//

#undef BEGIN_INIT_DATA
#undef END_INIT_DATA

//
//  Local include files.
//

//#include "const.h"


typedef int STATUS;

#ifndef SUCCESS
#define SUCCESS	0
#endif

#ifndef net_short
#define net_short(x) _byteswap_ushort((USHORT)x)
#define net_long(x)  _byteswap_ulong((ULONG)x)
#endif // net_short


#ifdef MEMTRACKING
// Memory types for AddTrackedItem.  Do a MAKELONG(AFDMEM_BUFFER,Info)
// for first user DWORD.
#define	AFDMEM_BUFFER	0
#define AFDMEM_SOCKET	1
#define AFDMEM_QUEUE	2
#define AFDMEM_CONN		3
#define AFDMEM_ENDP		4
#define AFDMEM_TRACK	5
#define AFDMEM_ACCEPTC	6
#define AFDMEM_STACK	7
#define AFDMEM_STACKMAP	8
#endif

#define MAX_REG_STR				128
#define MAX_DHCP_REQ_OPTIONS	32
#define MAX_DOMAIN_NAME			51	// -1 b/c first byte is length

#define DHCP_SERVER_PORT	67
#define DHCP_CLIENT_PORT	68

//
// Registry configurable options (DhcpInfo->Flags)
//
#define DHCPCFG_DHCP_ENABLED_FL		0x0001
#define DHCPCFG_USER_OPTIONS_FL		0x0002
#define DHCPCFG_OPTION_CHANGE_FL	0x0004
#define DHCPCFG_SAVE_OPTIONS_FL		0x0008
#define DHCPCFG_NO_MAC_COMPARE_FL	0x0010
#define DHCPCFG_SEND_OPTIONS_FL     0x0020
#define DHCPCFG_AUTO_IP_ENABLED_FL  0x0040
#define DHCPCFG_FAST_AUTO_IP_FL     0x0080


//
// Operational state flags (DhcpInfo->SFlags)
//
#define DHCPSTATE_T1_EXPIRE         0x0001
#define DHCPSTATE_T2_EXPIRE         0x0002
#define DHCPSTATE_LEASE_EXPIRE      0x0004
#define DHCPSTATE_SAW_DHCP_SRV      0x0008  // There is a DHCP server on the net, don't autoconfigure the IP address
#define DHCPSTATE_AUTO_CFG_IP       0x0010  // IP address was auto configured.
#define DHCPSTATE_RELEASE_REQUEST   0x0020  // User has requested the lease be let go.
#define DHCPSTATE_DIALOGUE_BOX      0x0040
#define DHCPSTATE_DHCPNTE_SET		0x0080	// we have set the dhcpnte...
#define DHCPSTATE_MEDIA_DISC		0x0100
#define DHCPSTATE_WLAN_NOT_AUTH		0x0200
#define DHCPSTATE_AFD_INTF_UP_FL	0x0400
#define DHCPSTATE_WIRELESS_ADHOC    0x0800
#define DHCPSTATE_DELETE            0x1000

#define ETH_ADDR_LEN 6

//
// IP Autoconfiguration defaults
//
#define DHCP_IPAUTOCONFIGURATION_DEFAULT_SUBNET  0x0000FEA9     //"169.254.0.0"
#define DHCP_IPAUTOCONFIGURATION_DEFAULT_MASK    0x0000FFFF     //"255.255.0.0"
#define DHPC_IPAUTOCONFIG_DEFAULT_INTERVAL       300            // 5 minutes in seconds

// define the reserved range of autonet addresses..
#define DHCP_RESERVED_AUTOCFG_SUBNET             0x00FFFEA9     //"169.254.255.0"
#define DHCP_RESERVED_AUTOCFG_MASK               0x00FFFFFF     //"255.255.255.0"

#define ARP_RESPONSE_WAIT   5000    // ms to wait for ARP response indicating collision

#define TEN_M	(10000000)	// 10 million (to convert to seconds in a FILETIME)

#pragma warning(disable:4200) // nonstandard extensions warning

typedef struct DhcpInfo {
    LIST_ENTRY  DhcpList;
    LPSOCK_INFO Socket;    // the socket handle
    PDHCP_PROTOCOL_CONTEXT pProtocolContext;
    void        *Nte;
    unsigned    NteContext;
    ushort      Flags;      // configuration settings (*_*_FL defines)
    uint        cRefs;
    uint        SFlags;     // operational state flags (DHCPSTATE_* defines)

    LIST_ENTRY  EventList;

    char        ClientHWAddr[CHADDR_LEN];   // Client Hardware Address
    uint        ClientHWAddrLen;            // Length of Address (bytes)
    uint        ClientHWType;               // 1 == 10 Mbit Eth
    BYTE        ClientID[CHADDR_LEN];       // Client ID
    uint        ClientIDLen;                // Length of client ID
    BYTE        ClientIDType;               // Type of address, 0 = soft, 1 = ETH
    uint        IPAddr;
    uint        SubnetMask;
    uint        Gateway;        // IP addr of the default gateway
    uint        DNS[2];
    uint        DhcpServer;
    uint        WinsServer[2];  // IP addrs of primary & ssecondary WINS servers
    FILETIME    LeaseObtained;  // in 100 micro-secs
    int         T1;             // these are in sec's
    int         T2;
    int         Lease;          // total length of lease
    uint        cRetryDialogue;
    uint        cMaxRetry;      // times to retry INIT phase
    int         InitDelay;      // in ms
    DEFINE_LOCK_STRUCTURE(Lock)
    CTEFTimer   Timer;
    CTETimer    AutonetTimer;   //  Timer used by AutoIP code.
    uchar       ReqOptions[MAX_DHCP_REQ_OPTIONS];
    uchar       *pSendOptions;
    int         cSendOptions;
    char        aDomainName[MAX_DOMAIN_NAME+1];
    uint        AutoIPAddr;
    uint        AutoSubnet;
    uint        AutoMask;
    uint        AutoSeed;
    uint        AutoInterval;   // Interval at which to check for a DHCP server
    HANDLE      ARPEvent;
    uint        ARPResult;
    TCHAR       Name[];
} DhcpInfo;

#pragma warning(default:4200) // nonstandard extensions warning

//
// DHCP Events
//
#define DHCP_EVENT_OBTAIN_LEASE     1
#define DHCP_EVENT_RELEASE_LEASE    2
#define DHCP_EVENT_RENEW_LEASE      3
#define DHCP_EVENT_MEDIA_CONNECT    4
#define DHCP_EVENT_MEDIA_DISCONNECT 5
#define DHCP_EVENT_DEL_INTERFACE    6

typedef struct DhcpEvent {
    LIST_ENTRY  List;
    DWORD       Code;
    DhcpInfo *  pDhcp;
    PDHCP_PROTOCOL_CONTEXT    pProtocolContext;
    void        *Nte;
    unsigned    NteContext;
    BYTE        ClientID[CHADDR_LEN];       // Client ID
    uint        ClientIDLen;                // Length of client ID
} DhcpEvent;

typedef int STATUS;

// dhcp.c
extern STATUS 
DhcpNotify(
	uint					Code,
	PDHCP_PROTOCOL_CONTEXT	pProtocolContext,
	PTSTR					pAdapter,
	void					*Nte,
	unsigned				Context, 
	char					*pAddr,
	unsigned				cAddr);

void mul64_32_64(const FILETIME *lpnum1, DWORD num2, LPFILETIME lpres);
void add64_32_64(const FILETIME *lpnum1, DWORD num2, LPFILETIME lpres);
void add64_64_64(const FILETIME *lpnum1, LPFILETIME lpnum2, LPFILETIME lpres);
void sub64_64_64(const FILETIME *lpnum1, LPFILETIME lpnum2, LPFILETIME lpres);
void div64_32_64(const FILETIME *lpdividend, DWORD divisor, LPFILETIME lpresult);
unsigned int GetXid(char * pMacAddr, unsigned len);
LPTSTR AddrToString(DWORD Addr, TCHAR *pString);
STATUS DhcpInitSock(DhcpInfo *pInfo, int SrcIP);
void TakeNetDown(DhcpInfo *pDhcp, BOOL fDiscardIPAddr, BOOL fRemoveAfdInterface);
BOOL SetDHCPNTE(DhcpInfo * pDhcp);
BOOL ClearDHCPNTE(DhcpInfo * pDhcp);
STATUS 
RequestDHCPAddr(
	PDHCP_PROTOCOL_CONTEXT	pProtocolContext,
	PTSTR					pAdapter, 
	void					*Nte, 
	unsigned				Context, 
	char					*pAddr,
	unsigned				cAddr);

BOOL ReleaseDHCPAddr(PTSTR pAdapter);
void DhcpWorkerThread();
HKEY OpenDHCPKey(DhcpInfo * pDhcp);

// autonet.c
STATUS AutoIP(DhcpInfo	*pDhcp);

#include "globals.h"


//	packet.c
void BuildDhcpPkt(DhcpInfo *pDhcp, DhcpPkt *pPkt, uchar Type, int fNew, 
					uchar *aOptionList, int *pcPkt);
STATUS SendDhcpPkt(DhcpInfo *pDhcp, DhcpPkt *pPkt, int cPkt, 
				   uchar WaitforType, int Flags);

STATUS TranslatePkt(DhcpInfo *pDhcp, DhcpPkt *pPkt, int cPkt, uchar *pType,
					unsigned int Xid);

#endif	// if 0

#endif	// dhcpp.h
