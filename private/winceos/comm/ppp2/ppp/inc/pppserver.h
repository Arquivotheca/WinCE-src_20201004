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
/*****************************************************************************
*
*
*/

#pragma once

//
//	PPP Server header
//

#define COUNTOF(array) sizeof(array)/sizeof(array[0])

typedef struct
{
	DWORD	IPAddr;
	DWORD	IPMask;
	DWORD	GWAddr;

	// DHCP Client HW address info
	DWORD	Index;		// "Context" passed to DHCP

	//
	//	The 16 octet Client HW address is formed as:
	//
	//	0-3:	"RAS "
	//	4-b:	<Ethernet MAC address>
	//	c-f:	<index>
	//
	BYTE	DhcpClientHWAddress[16];
#define RAS_PREPEND "RAS "

	//
	// Event used to signal that a lease request has completed,
	// successfully or not.
	//
	HANDLE  hEvent;

	BOOL    bDhcpCollision;

	DWORD   dwDhcpCollisionCount;

} PPPServerIPAddrInfo;

//
//	The PPPServerLineConfiguration_t contains parameters on what line is
//	to be managed by the PPP server and its configuration settings.
//
typedef struct PPPServerLineConfiguration
{
	LIST_ENTRY	listEntry;

	RASDEVINFO	rasDevInfo;			// DeviceType and DeviceName of the line to be managed
	PUCHAR		pDevConfig;			// Device type dependent configuration settings
	DWORD		dwDevConfigSize;	// Size in bytes of the space pointed to by pbDeviceInfo

	BOOL		bEnable;
	DWORD		bmFlags;			// PPPSRV_xxx from ras.h

	UINT		DisconnectIdleSeconds;

	//
	// IP addresses reserved for the server and client sides of the connection
	// These are preallocated whenever the line is enabled, even when no
	// connection is active.  This is because the IPCP negotiations have time
	// constraints that may not be met by the allocation procedure (e.g. DHCP)
	//
	BOOL		bUsingDhcpAddress;

	DWORD       dwIfIndex;          // Interface assigned by TCP/IP to this PPP connection

	DWORD		dwDhcpIfIndex;		// Interface through which DHCP addr obtained

	DWORD		dwProxyArpIfIndex;  // Non-zero specified the interface on which ARP proxying enabled

	PPPServerIPAddrInfo	IPAddrInfo[2];
#define SERVER_INDEX	0
#define CLIENT_INDEX	1

	// NULL if there is no session on this line (i.e. line disabled)
	pppSession_t *pSession;

	// IPV6 Network Prefix assigned to this line, if any
	BYTE        IPV6NetPrefix[16];
	DWORD       IPV6NetPrefixBitLength;

} PPPServerLineConfiguration_t;


//
// Registry names in HKLM\Comm\ppp\Server\Lines\<LineName> for configurable parameters
//
//		Enable:						DWORD	0 disables this line (default), 1 enables PPP Server management of this line
//		DeviceType:
//		DeviceInfo:					Binary	device specific configuration info
//		Flags						DWORD	bitmask of flag bits:
//		DisconnectIdleSeconds		DWORD	If non-0, disconnect if line is idle for this many seconds


typedef struct PPPServerUserCredentials
{
	RASCNTL_SERVERUSERCREDENTIALS	info;
} PPPServerUserCredentials_t;

//
//	The PPPServerConfiguration_t describes PPP server configuration settings.
//
typedef struct
{
	BOOL		bEnable;
	DWORD		bmFlags;				// PPPSRV_xxx from ras.h
	BOOL		bUseDhcpAddresses;		// Obtain addresses from DHCP server rather than static pool
	DWORD		dwStaticIpAddrStart;	// If using static IP address pool, this is the first address
	DWORD		dwStaticIpAddrCount;	// Number of static IP addresses following IpAddrStart in pool
	DWORD		bmAuthenticationMethods;// Bitmask of authentication methods to be allowed

	LIST_ENTRY	LineList;				// List of lines being managed by PPP server

	BOOL        bUseAutoIpAddresses;    // TRUE if IP addresses for clients should be generated from AutoIp pool
	DWORD	    dwAutoIpSubnet;         // Defines AutoIP address pool
	DWORD       dwAutoIpSubnetMask;

	DWORD       AuthMaxConsecutiveFail; // Max consecutive auth failures before invoking the delay
	DWORD       AuthFailHoldSeconds;    // Time to delay after failed auth attempt before allowing more connections on line

	DWORD       DhcpTimeoutMs;          // Time to wait trying to get a lease
	DWORD       DhcpMaxTimeouts;        // Max number of timeouts before giving up
	WCHAR		wszDhcpInterface[MAX_IF_NAME_LEN+1]; // Name of interface on which to obtain DHCP addresses

	// These 3 fields describe the pool of IPV6 network prefixes that are available to allocate
	// for IPV6 connections. They include the base prefix, the number of significant bits in the prefix,
	// and the number of consecutive prefixes in the pool.
	BYTE        IPV6NetPrefix[16];	
	DWORD       IPV6NetPrefixBitLength;
	DWORD       IPV6NetCount;

} PPPServerConfiguration_t;

//
// Registry names in HKLM\Comm\ppp\Server\Parms for configurable parameters
//
//		Enable:					DWORD	0 disables PPP Server (default), 1 enables PPP Server
//		UseDhcpAddresses:		DWORD	0 uses static IP address pool (default), 1 uses Dhcp server assigned addresses
//		StaticIpAddrStart:		DWORD	Starting IP address of static IP address pool
//		StaticIpAddrCount:		DWORD	Count of IP addresses in static pool to allocate
//		AuthenticationMethods:	DWORD	RASEO_ProhibitXxx bitmask (0 to allow all types)
//		

#define DEFAULT_STATIC_IP_ADDRESS_START	0xC0A8FE01		// 192.168.254.1
#define DEFAULT_STATIC_IP_ADDRESS_COUNT	254				// use addresses through 192.168.254.254
#define DEFAULT_DEVICE_TYPE				RASDT_Direct

#define RAS_PARMNAME_USE_DHCP_ADDRESSES				TEXT("UseDhcpAddresses")
#define RAS_PARMNAME_DHCP_IF_NAME				    TEXT("DhcpIfName")
#define RAS_PARMNAME_USE_AUTOIP_ADDRESSES			TEXT("UseAutoIPAddresses")
#define RAS_PARMNAME_AUTOIP_SUBNET      			TEXT("AutoIPSubnet")
#define RAS_PARMNAME_AUTOIP_SUBNET_MASK  			TEXT("AutoIPSubnetMask")
#define RAS_PARMNAME_STATIC_IP_ADDR_START			TEXT("StaticIpAddrStart")
#define RAS_PARMNAME_STATIC_IP_ADDR_COUNT			TEXT("StaticIpAddrCount")
#define RAS_PARMNAME_AUTHENTICATION_METHODS			TEXT("AuthenticationMethods")
#define RAS_PARMNAME_ENABLE							TEXT("Enable")
#define RAS_PARMNAME_FLAGS							TEXT("Flags")
#define RAS_PARMNAME_DISCONNECT_IDLE_SECONDS		TEXT("DisconnectIdleSeconds")
#define	RAS_PARMNAME_DEVICE_TYPE					TEXT("DeviceType")
#define	RAS_PARMNAME_DEVICE_INFO					TEXT("DeviceInfo")
#define	RAS_PARMNAME_DOMAIN_NAME					TEXT("DomainName")
#define	RAS_PARMNAME_PASSWORD						TEXT("Password")
#define	RAS_PARMNAME_DHCP_TIMEOUT_MS				TEXT("DHCPTimeoutMS")
#define	RAS_PARMNAME_DHCP_MAX_TIMEOUTS				TEXT("DHCPMaxTimeouts")
#define	RAS_PARMNAME_AUTH_FAIL_HOLD_SECONDS			TEXT("AuthFailHoldSeconds")
#define	RAS_PARMNAME_AUTH_FAIL_MAX			        TEXT("AuthFailMax")

#define	RAS_PARMNAME_IPV6_NET_PREFIX				TEXT("IPV6NetPrefix")
#define	RAS_PARMNAME_IPV6_NET_PREFIX_LEN			TEXT("IPV6NetPrefixBitLength")
#define	RAS_PARMNAME_IPV6_NET_COUNT					TEXT("IPV6NetCount")


extern void PPPServerInitialize();

void
PPPServerDeleteRouteToClient(
	pppSession_t        *pSession);

void
PPPServerAddRouteToClient(
	pppSession_t *pSession);

extern DWORD
PPPServerGetSessionClientIPAddress(
	pppSession_t *pSession);

extern DWORD
PPPServerGetSessionServerIPAddress(
	pppSession_t *pSession);

DWORD
PPPServerGetSessionIPMask(
	pppSession_t *pSession);

void
PPPServerGetSessionIPV6NetPrefix(
	IN  pppSession_t *pSession,
	OUT PBYTE         pPrefix,
	OUT PDWORD        pPrefixBitLength) ;

extern BOOL
PPPServerGetSessionAuthenticationRequired(
	pppSession_t *pSession);

extern DWORD PPPServerGetStatus(
	OUT	PRASCNTL_SERVERSTATUS pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut);

extern DWORD PPPServerSetEnableState(BOOL bNewEnableState);

extern DWORD PPPServerGetParameters(
	OUT	PRASCNTL_SERVERSTATUS pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut);

extern DWORD PPPServerSetParameters(
	IN	PRASCNTL_SERVERSTATUS pBufIn,
	IN	DWORD				  dwLenIn);

extern DWORD PPPServerGetIPV6NetPrefix(
	OUT	PRASCNTL_SERVER_IPV6_NET_PREFIX pBufOut,
	IN	DWORD				            dwLenOut,
	OUT	PDWORD				            pdwActualOut);

extern DWORD PPPServerSetIPV6NetPrefix(
	IN	PRASCNTL_SERVER_IPV6_NET_PREFIX pBufIn,
	IN	DWORD				            dwLenIn);

extern DWORD PPPServerLineAdd(
	IN	PRASCNTL_SERVERLINE	pBufIn,
	IN	DWORD				dwLenIn);

extern DWORD PPPServerLineIoCtl(
	IN	DWORD				  dwCode,
	IN	PRASCNTL_SERVERLINE   pBufIn,
	IN	DWORD				  dwLenIn,
	OUT	PRASCNTL_SERVERLINE   pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut);

extern DWORD PPPServerUserSetCredentials(
	IN	PRASCNTL_SERVERUSERCREDENTIALS	pBufIn,
	IN	DWORD							dwLenIn);

extern DWORD PPPServerUserDeleteCredentials(
	IN	PRASCNTL_SERVERUSERCREDENTIALS	pBufIn,
	IN	DWORD							dwLenIn);

extern BOOL PPPServerUserCredentialsGetByName(
	IN	PWCHAR	                    wszDomainAndUserName,
	OUT PPPServerUserCredentials_t *pCredentials);

extern DWORD	g_dwDNSIpAddr[2];
extern DWORD	g_dwWINSIpAddr[2];

extern DWORD PPPServerGetNameServerAddresses(PVOID unused);
