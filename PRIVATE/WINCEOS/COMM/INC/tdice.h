//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/********************************************************************/
/* :ts=4 */

//** TDIVXD.H - VXD specific TDI definitions. 
//
//	This file contains VXD specific TDI definitions, primarily related
//	to the TdiDispatch Table.
//
#ifndef _TDICE_H_
#define _TDICE_H_

#include <tdikrnl.h>



typedef PTDI_IND_DISCONNECT      PDisconnectEvent;
typedef PTDI_IND_ERROR           PErrorEvent;
typedef PTDI_IND_ERROR_EX        PErrorEx;

#ifndef UNDER_CE
typedef PTDI_IND_CHAINED_RECEIVE   PChainedRcvEvent;
typedef PTDI_IND_CONNECT           PConnectEvent;
typedef PTDI_IND_RECEIVE           PRcvEvent;
typedef PTDI_IND_RECEIVE_DATAGRAM  PRcvDGEvent;
typedef PTDI_IND_RECEIVE_EXPEDITED PRcvExpEvent;
#endif

// Begin code take from WSAHELP.H
//
// Notification event definitions.  A helper DLL returns a mask of the
// events for which it wishes to be notified, and the Windows Sockets
// DLL calls the helper DLL in WSHNotify for each requested event.
//

#define WSH_NOTIFY_BIND                 0x01
#define WSH_NOTIFY_LISTEN               0x02
#define WSH_NOTIFY_CONNECT              0x04
#define WSH_NOTIFY_ACCEPT               0x08
#define WSH_NOTIFY_SHUTDOWN_RECEIVE     0x10
#define WSH_NOTIFY_SHUTDOWN_SEND        0x20
#define WSH_NOTIFY_SHUTDOWN_ALL         0x40
#define WSH_NOTIFY_CLOSE                0x80
#define WSH_NOTIFY_CONNECT_ERROR        0x100

//
// Definitions for various internal socket options.  These are used
// by the Windows Sockets DLL to communicate information to the helper
// DLL via get and set socket information calls.
//

#define SOL_INTERNAL 0xFFFE
#define SO_CONTEXT 1

//
// Address manipulation routine.
//

typedef enum _SOCKADDR_ADDRESS_INFO {
    SockaddrAddressInfoNormal,
    SockaddrAddressInfoWildcard,
    SockaddrAddressInfoBroadcast,
    SockaddrAddressInfoLoopback
} SOCKADDR_ADDRESS_INFO, *PSOCKADDR_ADDRESS_INFO;

typedef enum _SOCKADDR_ENDPOINT_INFO {
    SockaddrEndpointInfoNormal,
    SockaddrEndpointInfoWildcard,
    SockaddrEndpointInfoReserved
} SOCKADDR_ENDPOINT_INFO, *PSOCKADDR_ENDPOINT_INFO;

typedef struct _SOCKADDR_INFO {
    SOCKADDR_ADDRESS_INFO AddressInfo;
    SOCKADDR_ENDPOINT_INFO EndpointInfo;
} SOCKADDR_INFO, *PSOCKADDR_INFO;


//
// Structure and routine for determining the address family/socket
// type/protocol triples supported by an individual Windows Sockets
// Helper DLL.  The Rows field of WINSOCK_MAPPING determines the
// number of entries in the Mapping[] array; the Columns field is
// always 3 for Windows/NT product 1.
//

typedef struct _WINSOCK_MAPPING {
    DWORD Rows;
    DWORD Columns;
    struct {
        DWORD AddressFamily;
        DWORD SocketType;
        DWORD Protocol;
    } Mapping[1];
} WINSOCK_MAPPING, *PWINSOCK_MAPPING;


//
// WSRegister takes the (device) name of the helper VxD,
// the (device) name of the associated TDI transport, and
// a pointer to the helper's WSHTABLE.
//
// It returns a pointer to a location where the TDIDispatchTable
// for the helper's associated transport is stored.
//
typedef struct TDIDispatchTable *PTDIDispatch;

TDI_STATUS
WSRegister (
	IN PWSTR TransportName,
	IN PTDIDispatch DispatchTable
	);

//
// WSDeregister takes the (device) name of the helper VxD.
//

INT
WSDeregister (
	IN PWSTR HelperName
	);

// Typedef of Register Functions
typedef TDI_STATUS (*PWSRegister) (IN PWSTR TransportName,
								   IN PTDIDispatch DispatchTable);

typedef void (*PRegisterProc) (IN PWSRegister, IN CRITICAL_SECTION *AfdCS);

//	This is the format used to register, helper dll's will most
//	likely have different uses for the parameters
typedef BOOL (*PHelperProc)(DWORD Context, DWORD OpCode, 
							PBYTE pVTable, DWORD cVTable, 
							PBYTE pMTable, DWORD cMTable,
							PDWORD pUnused);

typedef void (*PReadyProc)(void);

// End code take from WSAHELP.H

struct TDIDispatchTable {
	TDI_STATUS		(*TdiOpenAddressEntry)(PTDI_REQUEST, PTRANSPORT_ADDRESS,
						uint, PVOID);
	TDI_STATUS		(*TdiCloseAddressEntry)(PTDI_REQUEST);
	TDI_STATUS		(*TdiOpenConnectionEntry)(PTDI_REQUEST, PVOID);
	TDI_STATUS		(*TdiCloseConnectionEntry)(PTDI_REQUEST);
	TDI_STATUS		(*TdiAssociateAddressEntry)(PTDI_REQUEST, HANDLE);
	TDI_STATUS		(*TdiDisAssociateAddressEntry)(PTDI_REQUEST);
	TDI_STATUS		(*TdiConnectEntry)(PTDI_REQUEST, PVOID,
						PTDI_CONNECTION_INFORMATION,
						PTDI_CONNECTION_INFORMATION);
	TDI_STATUS		(*TdiDisconnectEntry)(PTDI_REQUEST, PVOID, ushort,
						PTDI_CONNECTION_INFORMATION,
						PTDI_CONNECTION_INFORMATION);
	TDI_STATUS		(*TdiListenEntry)(PTDI_REQUEST, ushort,
						PTDI_CONNECTION_INFORMATION,
						PTDI_CONNECTION_INFORMATION);
	TDI_STATUS		(*TdiAcceptEntry)(PTDI_REQUEST, PTDI_CONNECTION_INFORMATION,
						PTDI_CONNECTION_INFORMATION);
	TDI_STATUS		(*TdiReceiveEntry)(PTDI_REQUEST, ushort *, uint *,
						PNDIS_BUFFER);
	TDI_STATUS		(*TdiSendEntry)(PTDI_REQUEST, ushort, uint, PNDIS_BUFFER);
	TDI_STATUS		(*TdiSendDatagramEntry)(PTDI_REQUEST, PTDI_CONNECTION_INFORMATION,
						uint, ULONG *, PNDIS_BUFFER);
	TDI_STATUS		(*TdiReceiveDatagramEntry)(PTDI_REQUEST,
						PTDI_CONNECTION_INFORMATION,
						PTDI_CONNECTION_INFORMATION, uint, uint *, PNDIS_BUFFER);
	TDI_STATUS		(*TdiSetEventEntry)(PVOID, int, PVOID, PVOID);
	TDI_STATUS		(*TdiQueryInformationEntry)(PTDI_REQUEST, uint,
						PNDIS_BUFFER, uint *, uint);
	TDI_STATUS		(*TdiSetInformationEntry)(PTDI_REQUEST, uint,
						PNDIS_BUFFER, uint, uint);
	TDI_STATUS		(*TdiActionEntry)(PTDI_REQUEST, uint,
						PNDIS_BUFFER, uint);
	TDI_STATUS		(*TdiQueryInformationExEntry)(PTDI_REQUEST,
						struct TDIObjectID *, PNDIS_BUFFER, uint *, void *);
	TDI_STATUS		(*TdiSetInformationExEntry)(PTDI_REQUEST,
						struct TDIObjectID *, void *, uint);

	// Versions of the helper functions
	
	//	PWSH_GET_SOCKADDR_TYPE		GetSockaddrType;
	TDI_STATUS		(*TdiGetSockaddrType)(
						IN PSOCKADDR Sockaddr,
						IN DWORD SockaddrLength,
						OUT PSOCKADDR_INFO SockaddrInfo);
										  
	//	PWSH_GET_WILDCARD_SOCKADDR	GetWildcardSockaddr;
	TDI_STATUS		(*TdiGetWildcardSockaddr) (
						IN PVOID HelperDllSocketContext,
						OUT PSOCKADDR Sockaddr,
						OUT PINT SockaddrLength);

	//	PWSH_GET_SOCKET_INFORMATION GetSocketInformation;
	TDI_STATUS		(*TdiGetSocketInformation) (
						IN PVOID HelperDllSocketContext,
						IN SOCKET SocketHandle,
						IN HANDLE TdiAddressObjectHandle,
						IN HANDLE TdiConnectionObjectHandle,
						IN INT Level,
						IN INT OptionName,
						OUT PCHAR OptionValue,
						OUT PINT OptionLength);
	
//	PWSH_GET_WINSOCK_MAPPING	GetWinsockMapping;
	DWORD			(*TdiGetWinsockMapping) (
						OUT PWINSOCK_MAPPING Mapping,
						IN DWORD MappingLength);
	
//	PWSH_NOTIFY	Notify;
	TDI_STATUS		(*TdiNotify) (
						IN PVOID HelperDllSocketContext,
						IN SOCKET SocketHandle,
						IN HANDLE TdiAddressObjectHandle,
						IN HANDLE TdiConnectionObjectHandle,
						IN DWORD NotifyEvent);
	
//	PWSH_OPEN_SOCKET OpenSocket;
	TDI_STATUS		(*TdiOpenSocket) (
						IN OUT PINT AddressFamily,
						IN OUT PINT SocketType,
						IN OUT PINT Protocol,
						OUT PWSTR TransportDeviceName,
						OUT PVOID *HelperDllSocketContext,
						OUT PDWORD NotificationEvents);
	
//	PWSH_SET_SOCKET_INFORMATION SetSocketInformation;
	TDI_STATUS		(*TdiSetSocketInformation) (
						IN PVOID HelperDllSocketContext,
						IN SOCKET SocketHandle,
						IN HANDLE TdiAddressObjectHandle,
						IN HANDLE TdiConnectionObjectHandle,
						IN INT Level,
						IN INT OptionName,
						IN PCHAR OptionValue,
						IN INT OptionLength);

//	PWSH_ENUM_PROTOCOLS 		EnumProtocols;
	TDI_STATUS		(*TdiEnumProtocols) (
						IN LPINT lpiProtocols,
						IN LPTSTR lpTransportKeyName,
						IN OUT LPVOID lpProtocolBuffer,
						IN OUT LPDWORD lpdwBufferLength);


// the following are new added to support some of the winsock2 functionality!!!

// WSHJoinLeaf (
	TDI_STATUS		(*TdiJoinLeaf) (
						IN PVOID HelperDllSocketContext,
						IN SOCKET SocketHandle,
						IN HANDLE TdiAddressObjectHandle,
						IN HANDLE TdiConnectionObjectHandle,
						IN PVOID LeafHelperDllSocketContext,
						IN SOCKET LeafSocketHandle,
						IN PSOCKADDR Sockaddr,
						IN DWORD SockaddrLength,
						IN PVOID CallerData,
						IN PVOID CalleeData,
						IN PVOID SocketQOS,
						IN PVOID GroupQOS,
						IN DWORD Flags);


//	WSHGetBroadcastSockaddr
	TDI_STATUS		(*TdiGetBroadcastSockaddr) (
						IN PVOID HelperDllSocketContext,
						OUT PSOCKADDR Sockaddr,
						OUT PINT SockaddrLength);

// WSHGetWSAProtocolInfo
	TDI_STATUS		(*TdiGetWSAProtocolInfo) (
						IN LPWSTR ProviderName,
						OUT PVOID * ProtocolInfo,
						OUT LPDWORD ProtocolInfoEntries);

//	WSHAddressToString
	TDI_STATUS		(*TdiAddressToString) (
						IN LPSOCKADDR Address,
						IN INT AddressLength,
						IN PVOID ProtocolInfo,
						OUT LPWSTR AddressString,
						IN OUT LPDWORD AddressStringLength);
	
// WSHStringToAddress
	TDI_STATUS		(*TdiStringToAddress) (
						IN LPWSTR AddressString,
						IN DWORD AddressFamily,
						IN PVOID ProtocolInfo,
						OUT LPSOCKADDR Address,
						IN OUT LPINT AddressLength);

// WSHGetProviderGuid
	TDI_STATUS		(*TdiGetProviderGuid) (
						IN LPWSTR ProviderName,
						OUT LPGUID ProviderGuid);

// WSHIoctl
	TDI_STATUS		(*TdiIoctl) (
						IN PVOID HelperDllSocketContext,
						IN SOCKET SocketHandle,
						IN HANDLE TdiAddressObjectHandle,
						IN HANDLE TdiConnectionObjectHandle,
						IN DWORD IoControlCode,
						IN LPVOID InputBuffer,
						IN DWORD InputBufferLength,
						IN LPVOID OutputBuffer,
						IN DWORD OutputBufferLength,
						OUT LPDWORD NumberOfBytesReturned,
						IN PVOID Overlapped,
						IN PVOID CompletionRoutine,
						OUT LPBOOL NeedsCompletion);

};

typedef struct TDIDispatchTable TDIDispatchTable;

typedef struct EventRcvBuffer {
	PNDIS_BUFFER	erb_buffer;
	uint			erb_size;
	CTEReqCmpltRtn	erb_rtn;			// Completion routine.
	PVOID			erb_context;		// User context.
#ifdef UNDER_CE
	DWORD			erb_access;			// this is used to store the Permissions of the thread
#endif
	ushort			*erb_flags;			// Pointer to user flags.
#ifdef UNDER_CE
	DWORD			erb_procprm;		// proc permissions for faster rcvs.- this is NEW
#endif
} EventRcvBuffer;
// the erb_access allows the another thread to write to the user space, we use this while doing pending recv's

typedef struct ConnectEventInfo {
	CTEReqCmpltRtn				cei_rtn;	// Completion routine.
	PVOID						cei_context;// User context.
	PTDI_CONNECTION_INFORMATION	cei_acceptinfo;	// Connection information for
											// the accept.
	PTDI_CONNECTION_INFORMATION	cei_conninfo;	// Connection information to be
												// returned.
} ConnectEventInfo;

typedef	TDI_STATUS	(*PConnectEvent)(PVOID EventContext, uint AddressLength,
						PTRANSPORT_ADDRESS Address, uint UserDataLength,
						PVOID UserData, uint OptionsLength, PVOID
						Options,  PVOID *AcceptingID,
						ConnectEventInfo *EventInfo);
#if 0
typedef	TDI_STATUS	(*PDisconnectEvent)(PVOID EventContext,
						PVOID ConnectionContext, LONG DisconnectDataLength,
						PVOID DisconnectData, LONG OptionsLength, PVOID
						Options, ulong Flags);

typedef	TDI_STATUS	(*PErrorEvent)(PVOID EventContext, NTSTATUS Status);
#endif

typedef TDI_STATUS (*PChainedRcvEvent)(PVOID TdiEventContext, 
	CONNECTION_CONTEXT ConnectionContext, ULONG ReceiveFlags, ULONG ReceiveLength,        // length of client data in TSDU
	ULONG StartingOffset,       // offset of start of client data in TSDU	
    NDIS_BUFFER *	Tsdu,	// IN PMDL  Tsdu,                 // TSDU data chain
    IN PVOID TsduDescriptor        // for call to TdiReturnChainedReceives
    );

typedef	TDI_STATUS	(*PRcvEvent)(PVOID EventContext, PVOID ConnectionContext,
						ulong Flags, uint Indicated, uint Available,
						uint *Taken, uchar *Data, EventRcvBuffer *Buffer);

typedef	TDI_STATUS	(*PRcvDGEvent)(PVOID EventContext, uint AddressLength,
						PTRANSPORT_ADDRESS Address, uint OptionsLength, PVOID
						Options,  uint Flags, uint Indicated, uint Available,
						uint *Taken, uchar *Data, EventRcvBuffer **Buffer);

typedef	TDI_STATUS	(*PRcvExpEvent)(PVOID EventContext, 
                        PVOID ConnectionContext, ulong Flags, uint Indicated, 
                        uint Available, uint *Taken, uchar *Data, 
                        EventRcvBuffer *Buffer);

// Endpoints file object, status code indicating error type, buffer
typedef TDI_STATUS  (*PErrorEx)(PVOID TdiEventContext, TDI_STATUS Status, 
                                PVOID Buffer);

#endif	// _TDICE_H_

