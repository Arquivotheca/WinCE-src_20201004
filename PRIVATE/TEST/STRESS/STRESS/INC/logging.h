//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __LOGGING_H__
#define __LOGGING_H__



#define STRESS_LOGGING_ZONE_PATH	_T("Software\\Microsoft\\CEStress\\Logging Zones")


// Logging Level: All zones active, no zones active, or custom zones (see below)

#define SLOG_ALL					0xFFFFFFFF
#define SLOG_NONE					0x0
#define SLOG_CUSTOM					0x1



// Logging Verbosity


#define SLOG_ALWAYS					0
#define SLOG_ABORT					SLOG_ALWAYS
#define SLOG_FAIL					1
#define SLOG_WARN1					2
#define SLOG_WARN2					3
#define SLOG_COMMENT				4
#define SLOG_VERBOSE				5



///////////////////////////////////////////////////
// Custom Logging Zones
//


// Logging Zones: GWES


#define SLOG_WINMGR					0x0001
#define SLOG_MSGQUE					0x0002
#define SLOG_DLGMGR					0x0004
#define SLOG_TIMERS					0x0008
#define SLOG_RESOURCE				0x0010
#define SLOG_CONTROLS				0x0020
#define SLOG_INPUT					0x0040
#define SLOG_SIP					0x0080
#define SLOG_OOM					0x0100
#define SLOG_NOTIFICATIONS			0x0200

// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




//  Logging Zones: GDI


#define SLOG_FONT					0x0001
#define SLOG_BITMAP					0x0002
#define SLOG_RGN					0x0004
#define SLOG_TEXT					0x0008
#define SLOG_CLIP					0x0010
#define SLOG_DRAW					0x0020

// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Shell 


#define SLOG_NOTIF					0x0001
#define SLOG_COMMDLG				0x0002
#define SLOG_COMMCTRL				0x0004
#define SLOG_SHAPI					0x0008

// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: COM 


#define SLOG_AUTOMATION				0x0001
#define SLOG_MARSHALL				0x0002
#define SLOG_THREAD					0x0004
#define SLOG_MINICOM				0x0008

// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: IE 


#define SLOG_DEFAULT					0xFFFF
#define SLOG_XML						0x0001

// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000



// Logging Zones: Net


#define SLOG_WINSOCK				0x0001
#define SLOG_RAS					0x0002
#define SLOG_TAPI					0x0004
#define SLOG_BLUETOOTH				0x0008
#define SLOG_LDAP					0x0010
#define SLOG_UPNP					0x0020
#define SLOG_ICMP					0x0040
#define SLOG_MSMQ					0x0080
#define SLOG_OBEX					0x0100
#define SLOG_REDIR					0x0200
#define SLOG_SNMP					0x0400
#define SLOG_IPHLP					0x0800
#define SLOG_RDP				    0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000



// Logging Zones: Kernel


#define SLOG_DEFAULT						0xFFFF
#define SLOG_THREADS						0x0001
// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000



//  Logging Zones: Apps


#define SLOG_DEFAULT						0xFFFF
// #define SLOG_						0x0001
// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Servers


#define SLOG_DEFAULT				0xFFFF

#define SLOG_HTTP					0x0001
#define SLOG_FTP					0x0002
#define SLOG_TELNET					0x0004
#define SLOG_SOAP					0x0008

// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Drivers


#define SLOG_DEFAULT						0xFFFF
// #define SLOG_						0x0001
// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Filesys


#define SLOG_DEFAULT					0xFFFF
#define SLOG_SQLCE						0x0001
#define SLOG_REGISTRY					0x0002
#define SLOG_CEDB						0x0004

// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Intl


#define SLOG_DEFAULT						0xFFFF
// #define SLOG_						0x0001
// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000




// Logging Zones: Security


#define SLOG_DEFAULT						0xFFFF
#define SLOG_CAPI							0x0001

// #define SLOG_						0x0002
// #define SLOG_						0x0004
// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000



// Logging Zones: DX 


#define SLOG_DEFAULT					0xFFFF

#define SLOG_DSOUND						0x0001
#define SLOG_DMUSIC						0x0002
#define SLOG_DDRAW						0x0004
#define SLOG_WINDOWSMEDIA				0x0008
#define SLOG_DVD						0x0010
#define SLOG_D3D						0x0020
#define SLOG_AUDIO						0x0040
#define SLOG_DSHOW						0x0080

// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000



// Logging Zones: FRAMEWORKS 


#define SLOG_DEFAULT					0xFFFF

#define SLOG_DOTNET						0x0001
#define SLOG_MFC						0x0002
#define SLOG_ATL						0x0004

// #define SLOG_						0x0008
// #define SLOG_						0x0010
// #define SLOG_						0x0020
// #define SLOG_						0x0040
// #define SLOG_						0x0080
// #define SLOG_						0x0100
// #define SLOG_						0x0200
// #define SLOG_						0x0400
// #define SLOG_						0x0800
// #define SLOG_						0x1000
// #define SLOG_						0x2000
// #define SLOG_						0x4000
// #define SLOG_						0x8000






// Logging Spaces


#define SLOG_SPACE_GWES				0x0001
#define SLOG_SPACE_SHELL			0x0002
#define SLOG_SPACE_GDI				0x0004
#define SLOG_SPACE_IE				0x0008
#define SLOG_SPACE_APPS				0x0010
#define SLOG_SPACE_NET				0x0020
#define SLOG_SPACE_SERVERS			0x0040
#define SLOG_SPACE_COM				0x0080
#define SLOG_SPACE_KERNEL			0x0100
#define SLOG_SPACE_DRIVERS			0x0200
#define SLOG_SPACE_FILESYS			0x0400
#define SLOG_SPACE_SECURITY			0x0800
#define SLOG_SPACE_INTL				0x1000
#define SLOG_SPACE_DX				0x2000
#define SLOG_SPACE_FRAMEWORKS		0x4000
// #define SLOG_SPACE_					0x8000



#endif // __LOGGING_H__
