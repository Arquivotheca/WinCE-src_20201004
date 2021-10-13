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
/*++


  Module Name:    ssltypes.h

Abstract:       SSL/Winsock internal data declarations.

Contents:
struct SPBuffer
struct SSLSOCK_CONTEXT
SSL_STATE_* state declarations
SSL_OUTCALL_TABLE - Outcall table used within secure.

--*/

#ifndef _SSLTYPES_H_
#define _SSLTYPES_H_

extern PSecurityFunctionTableW v_IPackage;
extern HINSTANCE               v_hPackage;

//
// Buffer for storing plain text and cipher text for recv's and sends.
//

typedef struct _SPBuffer
{
	DWORD  cbBuffer;
	DWORD  cbData;
	LPVOID pvBuffer;
} SPBuffer, *PSPBuffer;

//
// SSL states.
//

#define SSL_STATE_UNCONNECTED       0L
#define SSL_STATE_HANDSHAKING       1L
#define SSL_STATE_CONNECTED         2L
#define SSL_STATE_SHUTTINGDOWN      3L
#define SSL_STATE_CLOSING           4L
#define SSL_STATE_ERROR             0xff

// Calls used by outcall table are either here
// or defined in WS2. 
typedef LPVOID (WSPAPI * LPSSLMAPPTR) ( LPVOID lpIn);
typedef void (WSPAPI * LPSSLLOCK) ( LPTSTR lpTag);
typedef void (WSPAPI * LPSSLUNLOCK) ();


typedef struct _SSL_OUTCALL_TABLE {
	LPWSPCONNECT             lpWSPConnect;
	LPWSPRECV                lpWSPRecv;
	LPWSPSELECT              lpWSPSelect;
	LPWSPSEND                lpWSPSend;
	LPWSPADDRESSTOSTRING     lpWSPAddressToString;
	LPSSLLOCK                lpLock; // Lock critical section.
	LPSSLUNLOCK              lpUnLock; // Unlock criticial section
} SSL_OUTCALL_TABLE, FAR * LPSSL_OUTCALL_TABLE;

void setGlobalOutCallTable(LPSSL_OUTCALL_TABLE x);
extern LPSSL_OUTCALL_TABLE g_lpOutCallTable; // Shouldn't have this at this scope.
//
// SSL Context.
//

typedef struct _SSLSOCK_CONTEXT
{
	//
	// Mode of socket.
	//
	//
	// Reference count;
	//

	DWORD cRef;

	//
	// Current socket (SSL) state. SSL_STATE_*.
	//

	DWORD dwState;

	//
	// Winsock Error code. Valid when dwState = SSL_STATE_ERROR.
	//

	DWORD dwErrorCode;

	//
	// This is used to ensure that only one function executes at a time.
	// Use InterlockedExchange to test access with TRUE and FALSE.
	//

	DWORD fInProgress;

	//
	// Base socket stuff.
	//


	//
	// WinSock socket handle. Used to free the socket handle when this
	// object is deleted.
	//

	SOCKET WsSocket;

	//
	// Application data.
	//

	//
	// Server Name.
	//

	PWSTR pszServerName;
	DWORD cchServerName;
       // 
       // remote address
       //
       PSOCKADDR serverAddr;
       DWORD dwServerAddrLen;
	//
	// Flags set via SO_SSL_SET_FLAGS WSAIoctl.
	//

	DWORD dwFlags;
	//
	// Currently enabled protoocols (SO_SSL_SET_PROTOCOLS). Zero means all protocols.
	//
	DWORD rgbitsProtocols;
	//
	// Ceritificate chain used for authenticating self to the other party.
	//

	DWORD  dwMyCertChainLen;
	LPBLOB pMyCertChain;

	//
	// Validate certificate hook.
	//

	SSLVALIDATECERTFUNC lpfnValidateCertHookFunc;
	LPVOID              lpvValidateCertHookArg;

	//
	// Authentication request hook.
	//

	SSLAUTHREQUESTFUNC lpfnAuthRequestHookFunc;
	LPVOID             lpvAuthRequestHookArg;

	//
	// SChannel Data.
	//

	//
	// Handles to SSPI credential and context.
	//

	BOOL       fMyCredsValid;
	CredHandle hMyCreds;
	CtxtHandle hContext;

	//
	// Don't read handshake messages when this is TRUE.
	//

	BOOL fReadHandshakeProcessed;

	//
	// Buffers for recv.
	//

	SPBuffer         RecvBufferedPlaintext;
	SPBuffer         RecvBufferedCiphertext;
	CRITICAL_SECTION csRecvBuffer;

	//
	// Buffer for send.
	//

	SPBuffer         SendBufferedCiphertext;
	CRITICAL_SECTION csSendBuffer;
	//LPWSPPROC_TABLE m_proctable;
	LPSSL_OUTCALL_TABLE m_outcall;

} SSLSOCK_CONTEXT, *LPSSLSOCK_CONTEXT;

//
// Random constants. BUGBUG - attempt to map these onto existing schannel
// constants!
//

// Maximum length of any certificate chain.
#define MAX_CERT_CHAIN_LENGTH   16

// Maximum length of a plaintext block
#define MAX_PLAINTEXT_LEN       0x8000

// Maximum length of encryption and protocol overhead
#define MAX_OVERHEAD_LEN        0x40

// Maximum length of a ciphertext block
#define MAX_CIPHERTEXT_LEN      (MAX_PLAINTEXT_LEN + MAX_OVERHEAD_LEN)

// Size of plaintext block to send.
#define SEND_PLAINTEXT_LEN      0x1000

// List of base provider events to listen for.
#define SSL_BASE_EVENTS (FD_READ | FD_WRITE | FD_ACCEPT | FD_CONNECT | FD_CLOSE)


// For fInProgress. 0 is not in progress.
#define SSL_CALL_IN_PROGRESS 0x1

//
// Security prototypes.
//

//
// Connect using secure sockets.
//

DWORD
SecureConnect(
		IN OUT LPSSLSOCK_CONTEXT           pSslCtxt,
		IN     const struct sockaddr FAR * name,
		IN     INT                         namelen
		);

//
// Set/retrieve security options and settings.
//

DWORD
SslIoctl(
		IN  LPSSLSOCK_CONTEXT pSslCtxt,
		IN  DWORD   dwIoControlCode,
		IN  LPVOID  lpvInBuffer,
		IN  DWORD   cbInBuffer,
		OUT LPVOID  lpvOutBuffer,
		IN  DWORD   cbOutBuffer,
		OUT LPDWORD lpcbBytesReturned
		);

//
// Send SSL ciphertext function.
//

DWORD
SendCiphertext(
		IN  LPSSLSOCK_CONTEXT pSslCtxt,
		IN  PSPBuffer         pPlaintext,
		IN  DWORD             flags,
		OUT LPDWORD           lpcbSent
		);

//
// Recv SSL ciphertext function.
//

DWORD
RecvPlaintext(
		IN     LPSSLSOCK_CONTEXT pSslCtxt,
		IN OUT PSPBuffer         pOutput,
		IN     DWORD             flags,
		OUT LPDWORD           lpcbRecvd
		);

//
// Set the socket to use security.
//

DWORD
SetSecureOpt(
		SOCKET      s,
		LPSOCK_INFO sock,
		int         level,
		int         optname,
		LPDWORD     optval,
		int         optlen
		);

//
// Select...
//

BOOL
IsSecureSelectRequired(
		fd_set FAR * readfds
		);

DWORD
SecureSelect(
		int nfds,
		fd_set FAR * readfds,
		fd_set FAR * writefds,
		fd_set FAR * exceptfds,
		IN     LPTIMEVAL   lpTimeout,
		LPINT lpErrno
		);
//
// Accept...
//

DWORD
SecureAccept(
		LPSSLSOCK_CONTEXT   pSslCtxt,
		struct sockaddr FAR *addr,
		INT FAR             *addrlen,
		PSOCKHAND            pSockHand
		);

//
// SSL Context helper functions.
//

DWORD
RefSslSockContext(
		IN LPSSLSOCK_CONTEXT pCtxt
		);

DWORD
DerefSslSockContext(
		IN LPSSLSOCK_CONTEXT pCtxt
		);

LPSSLSOCK_CONTEXT
GetSslSockContext(
		IN SOCKET s
		);

LPSSLSOCK_CONTEXT
GetNewSslSockContext(
		IN SOCKET s
		);

VOID
FreeSslSockContext(
		IN LPSSLSOCK_CONTEXT pCtxt
		);

//
// From recv.c.
//

DWORD
DecryptCiphertext(
		IN OUT LPSSLSOCK_CONTEXT pSslCtxt
		);

DWORD
ReadCiphertextFromAFD(
		IN  LPSSLSOCK_CONTEXT pSslCtxt,
		OUT PSPBuffer         pCiphertext,
		IN  DWORD             flags,
		OUT LPDWORD           lpcbCiphertext
		);

//
// Buffer allocation/free helper functions.
//

DWORD
AllocSPBuffer(
		IN OUT PSPBuffer pBuffer,
		IN     DWORD     cb
		);

VOID
FreeSPBuffer(
		IN OUT PSPBuffer pBuffer
		);

//
// Declarations for clihand.c.
//

DWORD
PerformClientHandshakeEx(
		IN LPSSLSOCK_CONTEXT pSslCtxt,
		IN BOOL              fNewConnection
		);

WINSOCK_STATUS
PerformClientHandshake(
		LPSSLSOCK_CONTEXT pSslCtxt
		);

WINSOCK_STATUS
ClientNegotiateLoop(
		LPSSLSOCK_CONTEXT pSslCtxt,
		BOOL fDoInitialRead
		);

BOOL
SerializeCertChain(
		LPBLOB pCertChain,
		DWORD  dwChainLen,
		PBYTE  pbCertChain,
		PDWORD pcbCertChain
		);

// WINSOCK_STATUS
// CreateSchannelCredentials(
//     LPSSLSOCK_CONTEXT pSslCtxt,
//     LPSSLCREDLIST     pCredList
//     );

WINSOCK_STATUS
DestroySchannelCredentials(
		LPSSLSOCK_CONTEXT pSslCtxt
		);

WINSOCK_STATUS
ReadHandshakeMsg(
		LPSSLSOCK_CONTEXT pSslCtxt,
		PSPBuffer   pBuffer
		);

WINSOCK_STATUS
WriteHandshakeMsg(
		LPSSLSOCK_CONTEXT pSslCtxt,
		PSPBuffer pBuffer
		);

WINSOCK_STATUS
ValidateCertificate(
		LPSSLSOCK_CONTEXT pSockContext
		);

//
// Declarations from srvhand.c.
//

WINSOCK_STATUS
ServerNegotiateLoop(
		LPSSLSOCK_CONTEXT pSockContext,
		BOOL              fDoInitialRead,
		BOOL              fNewContext);

//
// Abstract the send and recv buffer locks.
//

__inline
VOID
LockRecvBuffer(
		IN LPSSLSOCK_CONTEXT pSslCtxt
		)
{
	EnterCriticalSection(&pSslCtxt->csRecvBuffer);
}

__inline
VOID
UnlockRecvBuffer(
		IN LPSSLSOCK_CONTEXT pSslCtxt
		)
{
	LeaveCriticalSection(&pSslCtxt->csRecvBuffer);
}

__inline
VOID
LockSendBuffer(
		IN LPSSLSOCK_CONTEXT pSslCtxt
		)
{
	EnterCriticalSection(&pSslCtxt->csSendBuffer);
}

__inline
VOID
UnlockSendBuffer(
		IN LPSSLSOCK_CONTEXT pSslCtxt
		)
{
	LeaveCriticalSection(&pSslCtxt->csSendBuffer);
}


#endif // _SSLTYPES_H_
