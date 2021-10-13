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
*	RasReg.h
*
*	Private header file for the Ras Registry Functions
*
*	Date:	11/23/95
*
*	Modification History:
*		Date		Who			What
*	
*
*
*/

#ifndef _RASREG_H_
#define _RASREG_H_

// ----------------------------------------------------------------
//
//	Defines
//
// ----------------------------------------------------------------

#define	RASBOOK_KEY				TEXT("Comm\\RasBook")
#define RASBOOK_ENTRY_VALUE		TEXT("Entry")
#define RASBOOK_DEVCFG_VALUE	TEXT("DevCfg")

// ----------------------------------------------------------------
//
//	Structures
//
// ----------------------------------------------------------------

// This structure is what is actually stored in the registry.
// Since Pegasus doesn't use most of the fields in the normal RASENTRY
// struct it seemed a shame to stuff 1600 bytes into the registry
// for no apparent reason.

typedef	struct tagRaspEntry	
{
	DWORD		dwfOptions;
	DWORD		dwCountryID;
	DWORD		dwCountryCode;
	TCHAR		szAreaCode[ RAS_MaxAreaCode + 1 ];
	TCHAR		szLocalPhoneNumber[ RAS_MaxPhoneNumber + 1 ];
	RASIPADDR	ipaddr;
	RASIPADDR	ipaddrDns;
	RASIPADDR	ipaddrDnsAlt;
	RASIPADDR	ipaddrWins;
	RASIPADDR	ipaddrWinsAlt;
	DWORD		dwFrameSize;
	DWORD		dwFramingProtocol;
	TCHAR		szDeviceType[ RAS_MaxDeviceType + 1 ];
	TCHAR		szDeviceName[ RAS_MaxDeviceName + 1 ];
	TCHAR		szScript [ MAX_PATH ];

	DWORD		dwCustomAuthKey; // EAP extension type to use		
}
RASPENTRY, *LPRASPENTRY;

//
//	This is the structure used in CE 3.0 and earlier.
//
typedef	struct tagRaspEntry_V3
{
	DWORD		dwfOptions;
	DWORD		dwCountryID;
	DWORD		dwCountryCode;
	TCHAR		szAreaCode[ 10 /*RAS_MaxAreaCode*/ + 1 ];
	TCHAR		szLocalPhoneNumber[ 128 /*RAS_MaxPhoneNumber*/ + 1 ];
	RASIPADDR	ipaddr;
	RASIPADDR	ipaddrDns;
	RASIPADDR	ipaddrDnsAlt;
	RASIPADDR	ipaddrWins;
	RASIPADDR	ipaddrWinsAlt;
	DWORD		dwFrameSize;
	DWORD		dwFramingProtocol;
	TCHAR		szDeviceType[ 16 /*RAS_MaxDeviceType*/ + 1 ];
	TCHAR		szDeviceName[ 32 /*RAS_MaxDeviceName*/ + 1 ];
	TCHAR		szScript [ MAX_PATH ];
}
RASPENTRY_V3, *LPRASPENTRY_V3;

// ----------------------------------------------------------------
//
//	Internal Function Declarations
//
// ----------------------------------------------------------------
DWORD RaspCheckEntryName(LPCTSTR lpszEntry);
VOID  RaspInitRasEntry(LPRASENTRY pRasEntry);
VOID  RaspConvPrivPublic(LPRASPENTRY pRaspEntry, LPRASENTRY pRasEntry);
VOID  RaspConvPublicPriv(LPRASENTRY pRasEntry, LPRASPENTRY pRaspEntry);

extern DWORD
OpenRasEntryKey(
    IN	LPCTSTR	szPhonebook,
	IN	LPCTSTR  szEntry,
    OUT	PHKEY	phKey);

// ----------------------------------------------------------------
// Debug macro's for NT compilation
// ----------------------------------------------------------------
#ifndef UNDER_CE
#ifdef DEBUG
extern int  DebugFlag;
#   define DEBUGZONE( b )    (DebugFlag&(0x00000001<<b))

#   define ZONE_INIT        DEBUGZONE(0)        // 0x00001
#   define ZONE_MAC         DEBUGZONE(1)        // 0x00002
#   define ZONE_LCP         DEBUGZONE(2)        // 0x00004
#   define ZONE_AUTH        DEBUGZONE(3)        // 0x00008
#   define ZONE_NCP         DEBUGZONE(4)        // 0x00010
#   define ZONE_IPCP        DEBUGZONE(5)        // 0x00020
#   define ZONE_RAS         DEBUGZONE(6)        // 0x00040
#   define ZONE_PPP         DEBUGZONE(7)        // 0x00080
#   define ZONE_TIMING      DEBUGZONE(8)        // 0x00100
#   define ZONE_TRACE       DEBUGZONE(9)        // 0x00200
#   define ZONE_PACKET      DEBUGZONE(10)       // 0x00400
#   define ZONE_EVENT       DEBUGZONE(11)       // 0x00800
#   define ZONE_ALLOC       DEBUGZONE(12)       // 0x01000
#   define ZONE_FUNCTION    DEBUGZONE(13)       // 0x02000
#   define ZONE_WARN        DEBUGZONE(14)       // 0x04000
#   define ZONE_ERROR       DEBUGZONE(15)       // 0x08000
#   define ZONE_SERIAL      DEBUGZONE(16)       // 0x10000
		 
#else // DEBUG
#endif // DEBUG
#endif // UNDER_CE


#endif // _RASREG_H_
