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
	dhcpp.h

  DESCRIPTION:
	private dhcp.h


*/

#ifndef _DHCPP_H_
#define _DHCPP_H_

//
//  System include files.
//

#include <windows.h>
#include <types.h>

#include <dhcp.h>
#include <protocol.h>

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

#include <types.h>
#include <winsock.h>
#include <memory.h>
#include "ptype.h"

typedef unsigned char UCHAR, *PUCHAR;
typedef unsigned short USHORT, ushort;
typedef unsigned int uint;
typedef void *PVOID;

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
#include <cenet.h>

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
#define DHCPCFG_DHCP_ENABLED_FL		0x0001  // EnableDHCP
#define DHCPCFG_USER_OPTIONS_FL		0x0002  // DhcpOptions key
#define DHCPCFG_OPTION_CHANGE_FL	0x0004
#define DHCPCFG_SAVE_OPTIONS_FL		0x0008
#define DHCPCFG_NO_MAC_COMPARE_FL	0x0010  // DhcpNoMacCompare
#define DHCPCFG_SEND_OPTIONS_FL     0x0020  // DhcpSendOptions key
#define DHCPCFG_AUTO_IP_ENABLED_FL  0x0040  // AutoCfg
#define DHCPCFG_FAST_AUTO_IP_FL     0x0080  // DhcpEnableImmediateAutoIP
#define DHCPCFG_CONSTANT_RATE_FL    0x0100  // DhcpConstantRate
#define DHCPCFG_DIRECT_RENEWAL_FL   0x0200  // DhcpDirectRenewal


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
#define DHCPSTATE_MEDIA_DISC_PENDING 0x0100
#define DHCPSTATE_MEDIA_DISC        0x0200
#define DHCPSTATE_WLAN_NOT_AUTH     0x0400
#define DHCPSTATE_AFD_INTF_UP_FL    0x0800
#define DHCPSTATE_WIRELESS_ADHOC    0x1000
#define DHCPSTATE_DELETE            0x2000

#define ETH_ADDR_LEN 6

//
// IP Autoconfiguration defaults
//
#define DHCP_IPAUTOCONFIGURATION_DEFAULT_SUBNET  0x0000FEA9     //"169.254.0.0"
#define DHCP_IPAUTOCONFIGURATION_DEFAULT_MASK    0x0000FFFF     //"255.255.0.0"
#define DHPC_IPAUTOCONFIG_DEFAULT_INTERVAL       300            // 5 minutes in seconds
#define DHCP_TIMEOUT_AUTOCFG                     1000           // 1 second

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
    uint        AutoTimeout;    // Timeout for fallback to AutoIP
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
#define DHCP_EVENT_PROCESS_T1       7
#define DHCP_EVENT_PROCESS_T2       8
#define DHCP_EVENT_PROCESS_EXPIRE   9

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

STATUS 
RenewDHCPAddr(
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



#endif	// dhcpp.h
