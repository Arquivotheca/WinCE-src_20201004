//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************
* 
*
*   @doc
*   @module ppp.h | PPP Header File
*
*   Date: 6-23-95
*
*   @comm
*/

#ifndef PPP_H
#define PPP_H

#include    "ndis.h"
#include	"ndiswan.h"
#include    "rasxport.h"
#include    "rasreg.h"
#include    "compress.h"
#include    "tapi.h"
#include    "eap.h"
#include    "debug.h"
#include    <cenet.h>

#define LITTLE_ENDIAN                                   // always little endian

extern	DWORD	g_dwTotalLineCount;

// SLIP Defaults

#define SLIP_DEFAULT_MTU                1006

// PPP Defaults

#define PPP_DEFAULT_MTU                 1500

// For VPNs, make the default MTU smaller to reduce chance of IP fragmentation
#define PPP_DEFAULT_VPN_MTU             (PPP_DEFAULT_MTU - 100)

// PPP Parameter Limits

#define PPP_MAX_TERMINATE               (2)                 // 2 attempts
#define PPP_MAX_CONFIGURE               (10)                // 10 attempts
#define PPP_MAX_FAILURE                 (5)                 // 5 attempts
#define PPP_RESTART_TIMER               (3)                 // 3 seconds
#define ARP_802_ADDR_LENGTH             (6)                 
#define	PPP_AUTH_DEFAULT_MAX_TRIES		10
#define	PPP_AUTH_MAX_RESPONSE_FAILURES	3                   // maximum number of bad chap responses before giving up

// PPP Frame Address and Control

#define PPP_ALL_STATIONS                (0xFF)
#define PPP_UI                          (0x03)


//
//	Structure used to manage asynchronous request queueing and completion.
//
typedef struct completion_handler_info
{
	struct completion_handler_info	*pNext;
	void							(*pCompleteCallback)(PVOID);
	PVOID							pCallbackData;
} CompleteHanderInfo_t;

////////////////////////////////////////////////////////////////////////////////

// PPP Internal Message Packet Format

typedef struct  ppp_message
{
    USHORT          ProtocolType;               // decoded message protocol
    
	//
	// data pointer changes as the packet is decoded.
	// they start out pointing at the very beginning of the packet (address/control bytes)
	// and are advanced to point to the information field by PPPDecodeProtocol
	//
	DWORD           len;                        // total message length
    PBYTE           data;                       // pntr to message data

	// pPPPFrame points to the protocol field, it is used if a protocol reject needs to be sent
	PBYTE           pPPPFrame;
	DWORD           cbPPPFrame;

	// cbMACPacket contains the length of the PPP packet that was indicated from the MAC layer,
	// except for the address/control and initial protocol fields.
	// e.g. if the packet looks like FF 03 00 21 45 ... (50 more bytes), then cbMACPacket = 51.
	DWORD           cbMACPacket;
}
pppMsg_t;

// PPP / IP Interface Context

typedef struct _PPP_CONTEXT 
{
    struct _PPP_CONTEXT *Next;
    void                *IPContext;
    PVOID				IPV6Context;
    struct  pppSession  *Session;                       // PPP Session access
    uint                index;                          // Indexfrom IP.
    uint                ifinst;                         // Interface instance
	BOOL				fOpen;							// Are we open?
    uchar               addr[ ARP_802_ADDR_LENGTH ];
    WCHAR               AdapterName[ RAS_MaxDeviceName + 1 ]; // The adapter name to register with TCPIP
} 
PPP_CONTEXT;


// Lock Status

typedef struct  LockStatus
{
    BYTE    Locked;
    BYTE    WaitCnt;
}
pppLockStatus_t;

//  PPP Session Mode
//
//  The following Mode enumerates the possible modes of
//  operation. Currently the PPP code supports PPP and
//  SLIP. CSLIP is currently not supported.

typedef enum    ppp_session_mode
{
    PPPMODE_PPP,
    PPPMODE_SLIP,
    PPPMODE_CSLIP
}
pppMode_t;

#ifdef DEBUG
// Unique identifiers for each reference
#define REF_SESSIONNEW				0
#define REF_GETUSERTHREAD			1
#define REF_SENDIPVX				2
#define REF_CONNECTIONTHREAD		3
#define REF_TIMER					4
#define REF_EAPGETIDENTITY			5
#define REF_EAPSAVECONNECTIONDATA	6
#define REF_EAPSAVEUSERDATA			7
#define REF_EAPSENDPACKET			8
#define REF_EAPAUTHENTICATE	        9
#define REF_ISSUEREQUEST	        10
#define REF_WANRCV	                11
#define REF_WANRCVCOMPLETE	        12
#define REF_LINE_NEWCALL	        13
#define REF_LINE_CALLSTATE	        14
#define REF_LINE_REPLY		        15
#define REF_WAN_LINE_DOWN	        16
#define REF_RASDIAL					17
#define REF_RASHANGUP	            18
#define REF_GETCONNECTSTATUS	    19
#define REF_GETPROJECTIONINFO	    20
#define REF_GETRASSTATS	            21
#define REF_SERVERLINEOPEN	        22
#define REF_DHCP					23
#define REF_RAS_RENAME              24
#define REF_SEND_COMPLETE           25

#define REF_MAX_ID	32

#define PPPADDREF(session, id) pppAddRefTracked((session), (id))
#define PPPDELREF(session, id) pppDelRefTracked((session), (id))
#define PPPADDREFMAC(pMac, id) pppAddRefForMacContextTracked((pMac), (id))

#else // DEBUG

#define PPPADDREF(session, id) pppAddRef(session)
#define PPPDELREF(session, id) pppDelRef(session)
#define PPPADDREFMAC(pMac, id) pppAddRefForMacContext(pMac)

#endif

//#define PPP_LOG_EVENTS 1
#ifdef PPP_LOG_EVENTS

//
// Keep track of the last few events on the session for debugging purposes.
//
typedef struct pppEventLog
{
	DWORD TickCount;
	DWORD Event;
} pppEventLog_t;

#endif


// PPP Session Context

typedef struct  pppSession
{
    PPP_CONTEXT         *context;               // Context pointer
    LONG                RefCnt;                 // Context reference cntr
#ifdef DEBUG
	LONG				TrackedRefCnt[REF_MAX_ID];
#endif
    HANDLE              hHeap;                  // Heap handle
    int                 MemUsage;               // Amount of memory alloced.
    pppMode_t           Mode;                   // session mode


    RASCONNSTATE        RasConnState;           // RAS state
    DWORD               RasError;               // RAS error, if any
    CRITICAL_SECTION    RasCritSec;             // RAS critical section
    CRITICAL_SECTION    SesCritSec;             // session's critical section
    BOOL                HangUpReq;              // Hangup requested
    BYTE                Locked;                 // lock active
    BYTE                LockWaitCnt;            // Critical section wait count
    HANDLE              StateEvent;             // connection's state event
    HANDLE              UserEvent;              // user prompt event
    HANDLE              startHandle;            // connection thread handle
	
	LPBYTE				lpbDevConfig;
	DWORD				dwDevConfigSize;

	BOOL				bIsServer;				// Are we client or server?
	BOOL				bInitialRefCntDeleted;	// Has the initial session Ref been deleted

    // Passed in Parameters

    RASDIALPARAMS       rasDialParams;          // RAS Dial Parameters
    RASPENTRY           rasEntry;               // RAS Entry (internal version)
    DWORD               notifierType;           // RAS notifier type
    LPVOID              (*notifier)();          // RAS notifier (callback)

    // Layer Context pointers

    void                *macCntxt;              // mac context
    void                *lcpCntxt;              // lcp context
    void                *authCntxt;             // authentication context
    void                *ncpCntxt;              // ncp context

	// PPP configurable protocol parameters
	// These are common to LCP, CCP, IPCP, AUTH
	DWORD				dwMaxCRRetries;				// Number of times to resend a CR when not getting an ACK before giving up
	DWORD				dwMaxTRRetries;				// Number of times to resend a TR when not getting an ACK before giving up
	DWORD				dwReqTimeoutMs;				// Time to wait before resending a CR or TR when no ACK received
	DWORD				dwMaxCRFails;				// Max number of RCR_M events before giving up (fail to converge)
	DWORD				dwAckDupCR;					// Send ack for a CR with same Identifier as previously acked CR if non-0

	DWORD               dwMinimumMRU;               // Smallest LCP MRU we will ACK from the peer
#define LCP_DEFAULT_MINIMUM_MRU     64

	DWORD				bmAuthTypesAllowed;			// Bitmask of the authentication protocols that can be used
#define	AUTH_MASK_PAP				(1<<0)
#define AUTH_MASK_CHAP_MD5			(1<<1)
#define AUTH_MASK_CHAP_MS			(1<<2)
#define AUTH_MASK_CHAP_MSV2			(1<<3)
#define AUTH_MASK_EAP				(1<<4)
#define AUTH_MASK_ALL_SUPPORTED		0x1F

#define	AUTH_BITNUM_EAP				4

	DWORD				dwAuthMaxTries;				// Max tries to initiate authentication
	DWORD				dwAuthMaxFailures;			// Max bad auth replies before disconnecting

	//
	//	MPPE encryption keys saved from an EAP extension
	//
	BYTE				MPPESendKey[MAX_MPPE_KEY_LENGTH];
	DWORD				cbMPPESendKey;
	BYTE				MPPERecvKey[MAX_MPPE_KEY_LENGTH];
	DWORD				cbMPPERecvKey;


	DWORD				bmCryptTypes;				// Bitmask of encryption types that PPP will allow/negotiate
	DWORD				dwForceVpnEncryption;		// Force VPN connections to use data encryption if non-0
	DWORD				dwForceVpnDefaultRoute;		// Force VPN connections to be the default route
	DWORD				dwAlwaysAddSubnetRoute;		// Always add a subnet route, even if a default route is added
	DWORD				dwAlwaysAddDefaultRoute;	// Always add a default route, even when RASEO_RemoteDefaultGateway is not set
	DWORD				bDisableMsChapLmResponse;	// Clear the LmResponse field of an MSCHAP response packet
	DWORD				bDisableMsChapNtResponse;	// Clear the NtResponse field of an MSCHAP response packet

	DWORD				dwAllowAutoSuspendWhileSessionActive;
	DWORD				dwAlwaysSuggestIpAddr;
	DWORD				dwAlwaysRequestDNSandWINS;

	DWORD				dwStartTickCount;			// Tick Count when connection established
	NDIS_WAN_GET_STATS_INFO	Stats;					// Link stats.

	CompleteHanderInfo_t	*pPendingSessionCloseCompleteList;

	//
	// DHCP Inform/Ack packet related data to obtain domain name
	//
	DWORD               dwMaxDhcpInformTries;
	DWORD               dwDhcpInformTries;
	DWORD               dwDhcpTimeoutMs;
	BOOL                bSentDhcpInformPacket;
	CTETimer			dhcpTimer;
	BOOL				bDhcpTimerRunning;

	CHAR                szDomain[MAX_PATH];

	DWORD               dwConsecutiveClientAuthFails;
	HANDLE				hOpenThread;

	// Flag used to detect whether peer is still sending us anything, to detect
	// loss of connectivity over media types that can't otherwise detect it (e.g. PPPoE).
	BOOL                bRxData;
	BOOL                bDisconnectDueToUnresponsivePeer;
	BOOL                bUseRasErrorFromAuthFailure;

	//
	// Table of protocols registered for this session
	//

	PPROTOCOL_CONTEXT    pRegisteredProtocolTable;
	DWORD                cRegisteredProtocolTable;
	DWORD                numRegisteredProtocols;

	//
	// The function to handle RX packets received by the MAC layer
	//
	void                 (*RxPacketHandler)(struct pppSession *, pppMsg_t *);

	HWND                 hPasswordDialog;

#ifdef PPP_LOG_EVENTS
#define PPP_EVENT_LOG_SIZE 8
	pppEventLog_t eventLog[PPP_EVENT_LOG_SIZE];
	DWORD         eventLogIndex;
#endif
} pppSession_t;

#define NEED_ENCRYPTION(pRasEntry) \
    (((pRasEntry)->dwfOptions & (RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption)) \
                             == (RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption))							  

// Registry names in HKLM\Comm\ppp\Parms for configurable parameters

#define RAS_VALUENAME_MAXTERMINATE              TEXT("MaxTerminate")
#define RAS_VALUENAME_MAXCONFIGURE              TEXT("MaxConfigure")
#define RAS_VALUENAME_MAXFAILURE                TEXT("MaxFailure")
#define RAS_VALUENAME_RESTARTTIMER              TEXT("RestartTimer")	// Specified in seconds in registry
#define RAS_VALUENAME_ACKDUPCR                  TEXT("AckDupCR")
#define RAS_VALUENAME_CRYPTTYPES                TEXT("CryptTypesSupported")
#define RAS_VALUENAME_FORCEVPNCRYPT             TEXT("ForceVpnEncryption")
#define RAS_VALUENAME_FORCEVPNDEFAULTROUTE      TEXT("ForceVpnDefaultRoute")
#define RAS_VALUENAME_ALWAYSADDSUBNETROUTE      TEXT("AlwaysAddSubnetRoute")
#define RAS_VALUENAME_ALWAYSADDDEFAULTROUTE     TEXT("AlwaysAddDefaultRoute")
#define RAS_VALUENAME_DISABLEMSCHAPLMRESPONSE   TEXT("DisableMsChapLMResponse")
#define RAS_VALUENAME_DISABLEMSCHAPNTRESPONSE   TEXT("DisableMsChapNTPassword")
#define RAS_VALUENAME_ALLOWSUSPEND              TEXT("AllowSuspend")		// 0 (default) prevents auto suspend while session active
#define RAS_VALUENAME_ALWAYSSUGGESTIPADDR       TEXT("AlwaysSuggestIpAddr")	// 1 causes client to always suggest an IP address to the server.
																			// even when a static IP addr has been set
#define RAS_VALUENAME_ALWAYSREQUESTDNS          TEXT("AlwaysRequestDNSandWINS")	// 1 causes client to always request DNS/WINS addresses from the
																			    // server even when static DNS/WINS addr are set
#define RAS_VALUENAME_AUTHMAXTRIES              TEXT("AuthMaxTries")
#define RAS_VALUENAME_AUTHMAXFAILURES           TEXT("AuthMaxFailures")

#define RAS_VALUENAME_DHCPMAXTRIES              TEXT("DHCPMaxTries")
#define RAS_VALUENAME_DHCPTIMEOUTMS             TEXT("DHCPTimeoutMs")

#define RAS_VALUENAME_MINIMUM_MRU               TEXT("MinimumMRU")			// Lowest acceptable MRU value we will ACK from the peer

#define RAS_VALUENAME_IPV6_DISABLE              TEXT("IPV6Disable")			// Do not negotiate IPV6 if set to 1
#define RAS_VALUENAME_IPV6_IFID                 TEXT("IPV6IFID")			// Predefined IPV6 Interface ID to use
#define RAS_VALUENAME_IPV6_RANDOM_IFID          TEXT("IPV6RandomIFID")		// Randomly generated IPV6 Interface ID to use
#define RAS_VALUENAME_IPV6_FLAGS                TEXT("IPV6Flags")

#define RAS_VALUENAME_LCP_IDLE_DISCONNECT_MS       TEXT("LcpIdleDisconnectMs")
#define RAS_VALUENAME_LCP_ECHO_REQUEST_INTERVAL_MS TEXT("LcpEchoRequestIntervalMs")
#define RAS_VALUENAME_LCP_IDLE_DEVICE_TYPES        TEXT("LcpIdleDeviceTypes")

// PPP Start Message

typedef struct  pppStart
{
    PPP_CONTEXT     *context;                   // session's context handle
    void            *session;                   // returned session handle
    DWORD           error;                      // returned error code
    DWORD           notifierType;               // From RAS
    LPVOID          notifier;                   // From RAS
    RASDIALPARAMS   *rasDialParams;
    RASENTRY        *rasEntry;
	LPBYTE			lpbDevConfig;
	DWORD			dwDevConfigSize;
	BOOL			bIsServer;
}
pppStart_t;

#ifndef UNDER_CE
// Host/Network Byte Ordering Macros

#ifdef BIG_ENDIAN

#define ntohl(l)    (l)
#define htonl(l)    (l)
#define ntohs(s)    (s)
#define htons(s)    (s)

#endif 

#ifdef LITTLE_ENDIAN

#define ntohl(l)    _byteswap_ulong((ULONG)(l))
#define htonl(l)    _byteswap_ulong((ULONG)(l))
#define ntohs(s)    _byteswap_ushort((USHORT)(s))
#define htons(s)    _byteswap_ushort((USHORT)(s))

#endif
#endif  // UNDER_CE

// Function Prototypes and Macros

void
pppLock( pppSession_t * );

void
pppUnLock( pppSession_t * );

#define rasLock( session )   EnterCriticalSection( &((session)->RasCritSec) )

#define rasUnLock( session ) LeaveCriticalSection( &((session)->RasCritSec) )

BOOL
pppAddRef( pppSession_t *s_p );

BOOL 
pppDelRef( pppSession_t *s_p );

pppSession_t *
pppAddRefForMacContext( void *macCntxt );

void
PPPDeleteInterface(
	PPP_CONTEXT			*pContext,
	pppSession_t        *s_p);

PPP_CONTEXT *
pppContextNew(void);

VOID
pppComputeAuthTypesAllowed(pppSession_t    *s_p);

DWORD  
pppSessionNew( pppStart_t *start_p );

DWORD  
pppSessionDelete( pppSession_t *s_p );

DWORD  
pppSessionRun( pppSession_t *s_p );

DWORD
pppSessionStop(
	pppSession_t *s_p,
	void		(*pSessionCloseCompleteCallback)(PVOID),
	PVOID		pCallbackData);

void
PppHandleLineConnectedEvent(
	pppSession_t *s_p);

DWORD        
pppConnectionHangUp( pppSession_t *s_p );

DWORD
pppGetWins( pppSession_t *, RASPPPADDR * );

DWORD
pppGetDns( pppSession_t *, RASPPPADDR * );

void        
pppChangeOfState( pppSession_t *s_p, RASCONNSTATE Event, DWORD Info );

void
pppSendNdisBufferChainViaSLIP(
	PVOID         session,
    PNDIS_BUFFER  pNdisBuffer);

DWORD
pppSendData(
	IN	void  *session,
	IN	USHORT ProtocolWord,
	IN	PBYTE  pData,
	IN	DWORD  cData);

void
pppRcvData( void *session, pppMsg_t *msg_p );

void *
pppAllocateMemory(IN	UINT	nBytes);

void 
pppFreeMemory(IN	void	*pMem, IN	UINT	 nBytes);

void *
pppAlloc( pppSession_t *s_p, DWORD size, TCHAR *type );

BOOL
pppFree( pppSession_t *s_p, LPVOID address, TCHAR *type );

DWORD        
pppSetRasDialParams( pppSession_t *s_p, RASDIALPARAMS *rasDial_p );

void
pppStartTimer( pppSession_t     *s_p, 
               CTETimer         *tmr_p, 
               unsigned long    delay, 
               CTEEventRtn      func,  
               void             *arg );

DWORD
pppStopTimer( pppSession_t *s_p, CTETimer *tmr_p );

DWORD
PPPSessionRegisterProtocol(
    IN OUT pppSession_t    *pSession,
	IN USHORT               ProtocolType,
	IN PPROTOCOL_DESCRIPTOR pDescriptor,
	IN PVOID                Context);

// Packet Routines

DWORD
PPPDecodeProtocol( pppMsg_t *msg_p );

void
PPPRxProtocolReject(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg);

void
PPPRxPacketHandler(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg);

void
SLIPRxPacketHandler(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg);

void
PPPSessionUpdateRxPacketStats(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg);

BOOL
CopyNdisPacketToWanPacket(
	IN		PNDIS_PACKET		pNdisPacket,
	IN	OUT	PNDIS_WAN_PACKET	pWanPacket);

BYTE *
StrToUpper( BYTE *string );

BYTE
HexCharValue( CHAR ch );

void 
pppExecuteCompleteCallbacks(
	CompleteHanderInfo_t **ppCompletionList);

DWORD
pppInsertCompleteCallbackRequest(
	IN	OUT	CompleteHanderInfo_t **ppCompletionList,
	IN		void				(*pCompleteCallback)(PVOID),
	IN		PVOID				pCallbackData);

void FAR PASCAL 
RegisterIPClass( HINSTANCE hInst );

#define	REG_ALLOC_MEMORY		(1<<0)

extern DWORD ReadRegistryValues(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName,	OPTIONAL
	...);

extern DWORD WriteRegistryValues(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName,	OPTIONAL
	...);

extern LONG DeleteRegistryKeyAndContents(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName);

typedef BOOL (*pfnPostMessageW) (
    HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam);
extern pfnPostMessageW g_pfnPostMessageW;

extern ULONG  IPGetNetMask(ULONG Addr);

extern  PPP_CONTEXT     *pppContextList;
extern  CRITICAL_SECTION v_ListCS;
extern  HANDLE           v_hInstDLL;

extern  BOOL             g_bPPPInitialized;

extern void PppDhcpStart(pppSession_t *);
extern void PppDhcpStop(IN pppSession_t *pSession);
extern BOOL PppDhcpRxPacket(
	IN pppSession_t *pSession,
	IN PBYTE          pBuffer,
	IN DWORD          cbBuffer);

#ifdef DEBUG

// Debug Macros

extern  TCHAR               *DbgStrLayCodes[];
#define LAY_STR(x)           DbgStrLayCodes[x]

BOOL
pppAddRefTracked(
	IN OUT pppSession_t *s_p,
	IN     DWORD        idRef);

BOOL
pppDelRefTracked(
	IN OUT pppSession_t *s_p,
	IN     DWORD        idRef);

pppSession_t *
pppAddRefForMacContextTracked(
	IN     void *pMac,
	IN     DWORD idRef);

#endif // DEBUG

#endif // PPP_H
