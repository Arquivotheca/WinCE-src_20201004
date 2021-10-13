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
*   @doc
*   @module auth.h | Authentication Header File
*
*   Date: 7-25-95
*
*   @comm
*/


#pragma once

#include "layerfsm.h"
#include "crypt.h"
#include "md5.h"
#include "eap.h"
#include "pppserver.h"

// CHAP Authentication Protocol
//
// Algorithms

#define     PPP_CHAP_MD5              (0x05)          // MD5
#define     PPP_CHAP_MSEXT            (0x80)          // Microsoft extensions
#define     PPP_CHAP_MSEXT_V2          (0x81)          // MS CHAP V2

// CHAP Option Code Fields

#define     AUTH_CHAP_CHALLENGE             (0x01)
#define     AUTH_CHAP_RESPONSE              (0x02)
#define     AUTH_CHAP_SUCCESS               (0x03)
#define     AUTH_CHAP_FAILURE               (0x04)
#define     AUTH_CHAP_CHANGE_PASSWORD_V1    (0x05)
#define     AUTH_CHAP_CHANGE_PASSWORD_V2    (0x06)
#define     AUTH_CHAP_CHANGE_PASSWORD_MSV2  (0x07)

// MD5 constants
#define MD5_CHALLENGE_LENGTH      16
#define MD5_RESPONSE_LENGTH       16

// MS CHAP V1 constants
#define MSCHAP_CHALLENGE_LENGTH   8
#define MSCHAP_RESPONSE_LENGTH    (LM_RESPONSE_LENGTH + NT_RESPONSE_LENGTH + 1)

// MS CHAP V2 constants
#define MSCHAPV2_CHALLENGE_LENGTH 16
#define MSCHAPV2_RESPONSE_LENGTH  (LM_RESPONSE_LENGTH + NT_RESPONSE_LENGTH + 1)
#define CHAP_MSV2_RESPONSE_PEER_CHALLENGE_LENGTH 16
#define CHAP_MSV2_RESPONSE_RESERVED_LENGTH       8

// Common constants
// Our MS CHAP v1 uses an 8 octet challenge, MD5 and MSv2 CHAP use 16 octet challenge.
// The challenge from a peer may be larger. We will allow challenge lengths up to 256.
#define MAX_CHALLENGE_LENGTH        256
#define MAX_SYSTEM_NAME_LEN            64

// MD5 challenge response length is 16, MSv1 and MSv2 are 49
#define MAX_RESPONSE_VALUE_LEN      (LM_RESPONSE_LENGTH + NT_RESPONSE_LENGTH + 1)
#define MAX_RESPONSE_NAME_LEN       (DNLEN + 1 + UNLEN)

// Max length of Message field of CHAP success/fail packet
#define CHAP_MAX_MESSAGE_LENGTH        128

//
// Format of an MSCHAP V2 Change Password packet (see RFC 2759 section 7)
//
typedef struct
{
    BYTE         Code;
    BYTE         Id;
    BYTE         Length[2];
    BYTE         EncryptedPassword[516];
    BYTE         EncryptedHash[16];
    BYTE         PeerChallenge[16];
    BYTE         Reserved[8];
    BYTE         NTResponse[24];
    BYTE         Flags[2];
} MSCHAP_V2_CHANGE_PASSWORD_PACKET, *PMSCHAP_V2_CHANGE_PASSWORD_PACKET;

////////////////////////////////////////////////////////////////////////////////
//
// PAP Authentication Protocol
//
// Option Code Fields

#define        AUTH_PAP_REQUEST        (1)
#define        AUTH_PAP_ACK            (2)
#define        AUTH_PAP_NAK            (3)

////////////////////////////////////////////////////////////////////////////////


//
// CHAP Algorithm information
//
typedef struct
{
    PSTR  szName;
    BYTE  ChallengeLength;
    BYTE  ResponseLength;
    BYTE  Version; // For V=n string of response
    void  (*ComputeResponse)(
        struct auth_context *,
        OUT PBYTE *ppResponse,
        OUT PBYTE pcbResponse);
    DWORD (*ProcessResponse)(
        struct auth_context *, 
        IN  PBYTE pResponse, 
        IN  DWORD cbResponse, 
        IN  PPPServerUserCredentials_t *pCredentials,
        OUT PCHAR pMessage,
        OUT PDWORD pcbMessage);
    DWORD (*ProcessSuccess)(struct auth_context *, IN PBYTE pMessage, IN DWORD cbMessage);
    DWORD (*ProcessFailure)(struct auth_context *, IN PBYTE pMessage, IN DWORD cbMessage);
} PppChapAlgorithmDescriptor, *PPppChapAlgorithmDescriptor;

////////////////////////////////////////////////////////////////////////////////

//  Authentication Context

typedef struct  auth_context
{
    pppSession_t      *session;                   // session context

    // Layer Mgmt

    PppFsmState                                 state;

    // Unlike most state machines, the auth FSM can remain Up
    // even when not in the Opened state. This is because it
    // is possible to reauthenticate to confirm a client
    // identity on a periodic basis after an initial successful
    // authentication. While reauthenticating, we do not want
    // to tear down the higher network layers.
    BOOL                                        bAuthLayerUp;

    //
    // Retransmit timer used to trigger resends to the peer
    // when no packets received from peer during authentication.
    //
    CTETimer        Timer;
    BOOL            bTimerRunning;
    BYTE            retryCounter;

    // Active Protocol

    ushort          ActiveProtocol; 
    BYTE            ActiveAlgorithm;            // if applicable
    PPppChapAlgorithmDescriptor pActiveAlgDescriptor;


    //
    // Domain/username of user authenticated this session
    //
    WCHAR            wszDomainAndUserName[DNLEN + 1 + UNLEN + 1];

    //
    // Space to build chap response packet which is resent on
    // timeouts.
    //
    BYTE            ChapResponsePacket[sizeof(MSCHAP_V2_CHANGE_PASSWORD_PACKET)];
    DWORD           cbChapResponsePacket;

    // Chap Authentication Control

    USHORT          chapRespId;                 // chap response id
    BOOL            IgnoreRetryMsg;                // ignore chap retry msgs
    DWORD            dwFailedResponses;          // count of failed client responses

    // Chap Challenge and Response storage for MD5, MSV1, MSV2

    BYTE            Challenge[ MAX_CHALLENGE_LENGTH ];    // Challenge received FROM our peer
    BYTE            ChallengeLen;                        // MD5 is N bytes, MSV1 is 8, V2 is 16
    BYTE            Response [ MAX_RESPONSE_VALUE_LEN ];
    BYTE            ResponseLen;

    // CHAP V2 Password change support
    BOOL            bPasswordExpired;
    BOOL            bSavePassword;
    WCHAR           NewPassword[ PWLEN + 1 ];

    // Place to build/store CHAP success/fail packet
    BYTE            SuccFailPacket[4/*PPP_PACKET_HEADER_SIZE*/ + CHAP_MAX_MESSAGE_LENGTH];
    DWORD           cbSuccFailPacket;

    // Chap - MD5

    MD5_CTX            md5ctx;                        // md5 context

    // PAP Authentication Control

    BYTE            pap_Identifier;
    USHORT          pap_AckId;                   // Id of ACKed CR packet
    PBYTE            pPapRequest;
    DWORD           cbPapRequest;

    // Failure Packet Data

    DWORD            ErrorCode;                    
    BOOL            Retry;
    DWORD            Version;
    BOOL            NewChallenge;

    NDIS_WORK_ITEM  AuthCloseWorkItem;
    NDIS_EVENT      AuthCloseEvent;

    // EAP Authentication
    PEAP_SESSION    pEapSession;
}
authCntxt_t, AuthContext, *PAuthContext;

// Function Prototypes
//
// Instance Management

DWORD   pppAuth_InstanceCreate( void *SessionContext, void **ReturnedContext );
void    pppAuth_InstanceDelete( void  *context );

// Layer Management

void     pppAuth_Reset( authCntxt_t *c_p );
PBYTE    pppAuth_GetChallenge( void *context);

void    authSucceeded(IN    OUT    PAuthContext pContext);
void    authFailed(IN    OUT    PAuthContext pContext, DWORD ErrorCode);
void    authDone(IN    OUT    PAuthContext pContext, DWORD ErrorCode);
void    authTimerStart(IN    OUT    PAuthContext pContext);
void    authTimerStop( IN    OUT    PAuthContext pContext);

// Data & Control Processing

void
authSendPacket(
    IN    OUT    PAuthContext pContext,
    IN      PBYTE        pPacket,
    IN      DWORD        cbPacket);

void    AuthProcessRxPacket( void *context, pppMsg_t *msg_p );

// CHAP Authentication

void        chapProcessRxPacket( authCntxt_t *c_p, PBYTE pPacket, DWORD cbPacket);
PppFsmState    chapStart( authCntxt_t *c_p );
void        chapTimeout(authCntxt_t *c_p);
void        chapSendResponse(PAuthContext c_p);

int
chapGetEncodedPassword(
    IN    pppSession_t *session,
    OUT    BYTE         *pBuffer,
    IN    UINT          cbBuffer);

// PAP Authentication 

PppFsmState    papStart( authCntxt_t *c_p );
void        papStop ( authCntxt_t *c_p );
void        papInput( authCntxt_t *c_p, PBYTE pPacket, DWORD cbPacket);
void        papTimeout(authCntxt_t *c_p);

// EAP Authentication
extern    HMODULE     g_hEapDll;
DWORD    rasEapGetDllFunctionHooks();

void        eapInput( authCntxt_t *c_p, PBYTE pData, DWORD len );
void        eapNewResponse( authCntxt_t *c_p );
PppFsmState    eapStart( authCntxt_t *c_p );
void        eapStop( authCntxt_t *c_p );
extern void (*g_pEapSessionRxImpliedSuccessPacket)();

DWORD
rasGetEapUserData (
    IN        HANDLE    hToken,           // access token for user 
    IN        LPCTSTR pszPhonebook,     // path to phone book to use 
    IN        LPCTSTR pszEntry,         // name of entry in phone book 
    OUT        PBYTE   pbEapData,        // retrieved data for the user 
    IN    OUT    PDWORD  pdwSizeofEapData);// size of retrieved data 

DWORD
rasGetEapConnectionData (
    IN        LPCTSTR pszPhonebook,     // path to phone book to use 
    IN        LPCTSTR pszEntry,         // name of entry in phone book 
    OUT        PBYTE   pbEapData,        // retrieved data for the user 
    IN    OUT    PDWORD  pdwSizeofEapData);// size of retrieved data 

DWORD
rasSetEapUserData(
    IN    HANDLE    hToken,           // access token for user
    IN    LPCTSTR pszPhonebook,     // path to phone book to use
    IN    LPCTSTR pszEntry,         // name of entry in phone book
    IN    PBYTE    pbEapData,        // data to store for the user
    IN    DWORD    dwSizeofEapData); // size of data

DWORD
rasSetEapConnectionData(
    IN    LPCTSTR pszPhonebook,    // path to phone book to use
    IN    LPCTSTR pszEntry,        // name of entry in phone book
    IN    PBYTE    pbEapData,       // data to store for the connection
    IN    DWORD    dwSizeofEapData);// size of data

//  Function Prototypes for amb.c

void
LmPasswordHash(
    IN    PCHAR    pPassword,            // 0-to-14-oem-char password
    OUT    PBYTE    pPasswordHash);        // 16 byte resultant hash

void 
LmChallengeResponse(
    IN    BYTE *Challenge,            // 8-octet  challenge
    IN    CHAR *Password,                // 0-to-14-oem-char password
    OUT    BYTE *Response );            // 24-octet response

void
MD4Hash(
    IN    PBYTE    pData,                // data to be irreversibly hashed
    IN    DWORD    cbData,                // count of bytes in pData
    OUT    PBYTE    pDataHash);            // 16-octet resultant hash

void
NtPasswordHash(
    IN    PWCHAR    pPassword,            // 0-to-256-unicode-char password
    OUT    PBYTE    pPasswordHash);        // 16-octet resultant hash

void
HashNtPasswordHash(
    IN    PBYTE    pPasswordHash,        // 16-octet hashed password
    OUT    PBYTE    pPasswordHashHash);    // 16-octet resultant hash

void
NtChallengeResponse(
    IN    PBYTE    Challenge,            // 8-octet challenge
    IN    PWCHAR    Password,            // 0-to-256 unicode char password
    OUT    PBYTE    Response );            // 24-octet response

void
ChallengeHash(
    IN    PBYTE    PeerChallenge,                // 16-octet 
    IN    PBYTE    AuthenticatorChallenge,        // 16-octet 
    IN    PCHAR    UserName,                    // 0-to-256 char user name
    OUT    PBYTE    Challenge );                // 8-octet 

void
GenerateNTResponse(
    IN    PBYTE    AuthenticatorChallenge,        // 16-octet 
    IN    PBYTE    PeerChallenge,                // 16-octet 
    IN    PCHAR    UserName,                    // 0-to-256 char user name
    IN    PWCHAR    Password,                    // 0-to-256 unicode char password
    OUT    PBYTE    Response );                    // 24-octet response

void
NewPasswordEncryptedWithOldNtPasswordHash(
    IN    PWCHAR    NewPassword,       // 0-to-256-unicode-char password
    IN    PWCHAR    OldPassword,       // 0-to-256-unicode-char password
    OUT    PVOID   EncryptedPwBlock); // 512 octet encrypted pw followed by 4 octet length

void
OldNtPasswordHashEncryptedWithNewNtPasswordHash(
    IN    PWCHAR    NewPassword,       // 0-to-256-unicode-char password
    IN    PWCHAR    OldPassword,       // 0-to-256-unicode-char password
    OUT    PBYTE   EncryptedPasswordHash);         // 16-octet

#ifdef DEBUG
char *
ToHexStr(
    PBYTE    pBytes,
    DWORD    cBytes);
#endif

void    authNewState(PAuthContext pContext, PppFsmState  newState);

DWORD   AuthOpen(PAuthContext pContext);
DWORD   AuthClose(PAuthContext pContext);
void    AuthLowerLayerUp(PAuthContext context );
void    AuthLowerLayerDown(PAuthContext context );
void    AuthRxImpliedSuccessPacket(PAuthContext context);

void    authBuildDomainAndUserNameString(IN  pppSession_t *pSession, OUT PWSTR pwszName);
