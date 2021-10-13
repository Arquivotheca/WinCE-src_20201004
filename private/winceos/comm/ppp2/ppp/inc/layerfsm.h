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


//
//	PPP Protocol Layer Finite State Machine Implementation Module
//

#pragma once

#include "cxport.h"
#include "raserror.h"

//  Misc data conversion routines for packets

#define	BYTES_TO_DWORD(dw, pB) \
	(dw) = ((pB)[0] << 24) | ((pB)[1] << 16) | ((pB)[2] <<  8) | ((pB)[3])

#define DWORD_TO_BYTES(pB, dw) \
	(pB)[0] = (BYTE)(dw >> 24); (pB)[1] = (BYTE)(dw >> 16); (pB)[2] = (BYTE)(dw >> 8); (pB)[3] = (BYTE)(dw)

#define	BYTES_TO_USHORT(us, pB) \
	(us) = ((pB)[0] <<  8) | ((pB)[1])

#define USHORT_TO_BYTES(pB, us) \
	(pB)[0] = (BYTE)((us) >> 8); (pB)[1] = (BYTE)((us) >> 0)


//
//	Value to return from BuildOptionCb to skip adding
//  the option to a configur-request.
//
#define  ERROR_PPP_SKIP_OPTION	(RASBASE+199)

//
//	Types and data structures for handling PPP option negotiation.
//

//
//	Each PPP option to be negotiated will fall into one of these four categories:
//		Unsupported - We REJect the option if received in a CR, don't send in CR.
//		Allowed - We allow the negotiation of the option, won't send in CR unless peer requests (via NAK).
//		Wanted - We want to negotiate the option, but if peer REJects we are OK to not negotiate it
//		Required - We MUST negotiate the option, otherwise negotiation fails
//
typedef enum
{
	ORL_Unsupported,
	ORL_Allowed,
	ORL_Wanted,
	ORL_Required
} OptionRequireLevel;

typedef enum
{
	ONS_Initial,
	ONS_Requested,
	ONS_Acked,
	ONS_Naked,
	ONS_Rejected
} OptionNegotiationState;

typedef DWORD (*OPTION_BUILD_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, OUT PBYTE pOptData, IN OUT PDWORD pcbOptData);
typedef DWORD (*OPTION_ACK_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, IN PBYTE pOptData, IN DWORD cbOptData);
typedef DWORD (*OPTION_NAK_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, IN PBYTE pOptData, IN DWORD cbOptData);
typedef DWORD (*OPTION_REJ_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, IN PBYTE pOptData, IN DWORD cbOptData);
typedef DWORD (*OPTION_REQUEST_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, OUT PBYTE pCode, IN OUT PBYTE *ppOptData, IN OUT PDWORD pcbOptData);
typedef void  (*OPTION_TO_STR_CB)(PVOID context, IN OUT struct OptionInfo *pInfo, IN PBYTE pOptData, IN DWORD cbOptData, IN OUT PSTR *ppStr);

typedef VOID  (*OPTION_RESET_PEER_OPT_CB)(PVOID context);

//
//	Data structure describing constant data related
//	to an option.
//
typedef struct OptionDescriptor
{
	BYTE					type;
	BYTE					fixedLength; // Set to 0-253 if the option must always have that length to be valid
#define OPTION_VARIABLE_LENGTH	255
	PSTR					szName;		// e.g. "ACCM", "IFID", "MRU"
	OPTION_BUILD_CB			BuildOptionCb;
	OPTION_ACK_CB			AckOptionCb;
	OPTION_NAK_CB			NakOptionCb;
	OPTION_REJ_CB			RejOptionCb;	// Optional reject handler
	OPTION_REQUEST_CB		RequestOptionCb;
	OPTION_TO_STR_CB		OptionToStrCb;	// Optional function to pretty print option value
} OptionDescriptor, *POptionDescriptor;

typedef struct OptionInfo
{
	struct OptionInfo		*next;
	OptionRequireLevel		orlLocal;		// whether to send in configure-request
	OptionRequireLevel		orlPeer;	// whether we allow peer to request the option
	OptionNegotiationState	onsLocal;
	BOOL					bRequested; // whether the peer requested this in the most recent config-request
	BOOL					bSuggested; // whether we have suggested to the peer the use of this option by NAKing

	POptionDescriptor		pDescriptor;// constant data describing how to handle the option

} OptionInfo, *POptionInfo;

typedef struct OptionContext
{
	PSTR						szProtocolName;		// e.g. "LCP", "IPCP", "CCP"
	OPTION_RESET_PEER_OPT_CB	CbResetPeerOptions;	// Called when we receive a CR from the peer
	PVOID						CbContext;

	POptionInfo					pOptionList;

	PBYTE						pResponse;			// Pointer to response (to a config-req) PDU being constructed 
	DWORD						cbResponse;

	//
	//	Maximum number of configure-naks packets sent without
	//	sending a configure-ack before assuming that the configuration
	//	is not converging.
	//
	DWORD				dwMaxFailure;
#define	DEFAULT_MAX_FAILURE			5

	//
	//	Count of the number of NAKs sent since the last ACK.
	//
	DWORD				dwConsecutiveFailureCount;

} OptionContext, *POptionContext;

#include <packon.h>
//
//	Options are encoded into frames
//	using the following format:
//		<Type:1 octet> <Len: 1 Octet> <Data: Len-2 Octets>
//
typedef struct OptionHeader
{
	BYTE	Type;
	BYTE	Len;
} OptionHeader, *POptionHeader;

#define	OPTION_HEADER_LENGTH	2

#include <packoff.h>

void
OptionContextInitialize(
	POptionContext				pContext,
	PSTR						szProtocolName,
	OPTION_RESET_PEER_OPT_CB	CbResetPeerOptions,
	PVOID						CbContext);

void
OptionContextCleanup(
	POptionContext pContext);

DWORD
OptionInfoAdd(
	POptionContext			pContext,
	POptionDescriptor		pDescriptor,
	OptionRequireLevel		orlLocal,
	OptionRequireLevel		orlPeer
	);

DWORD
OptionBuildConfigureRequest(
	IN		POptionContext pContext,
	IN      BYTE           id,
		OUT	PBYTE		   pFrame,
	IN	OUT	PDWORD		   pcbFrame);

DWORD
OptionProcessRxMessage(
	IN	OUT	                                            POptionContext pContext,
	__in_bcount(cbRxData)		                        PBYTE		   pRxData,
	IN		                                            DWORD		   cbRxData,
	__out_bcount_part_opt(*pcbResponse, *pcbResponse)	PBYTE		   pResponse,
	__inout_opt                                         PDWORD		   pcbResponse);

void
OptionResetNegotiation(
	IN	OUT	POptionContext pContext);

#ifdef DEBUG
void
OptionDebugAppendOptionList(
	IN		POptionContext pContext,
	IN		PSTR	pBuffer,
	IN		PBYTE	pOptions,
	IN		DWORD	cbOptions);
#endif

DWORD
OptionSetORL(
	POptionContext			pContext,
	BYTE					type,
	OptionRequireLevel		orlLocal,
	OptionRequireLevel		orlPeer
	);

//
// FSM stuff
//


typedef enum
{
	PFS_Initial,
	PFS_Starting,
	PFS_Closed,
	PFS_Stopped,
	PFS_Closing,
	PFS_Stopping,
	PFS_Request_Sent,
	PFS_Ack_Received,
	PFS_Ack_Sent,
	PFS_Opened
} PppFsmState;

typedef DWORD (*PPPFSM_THISLAYERUP_CB)(PVOID context);
typedef DWORD (*PPPFSM_THISLAYERDOWN_CB)(PVOID context);
typedef DWORD (*PPPFSM_THISLAYERSTARTED_CB)(PVOID context);
typedef DWORD (*PPPFSM_THISLAYERFINISHED_CB)(PVOID context);
typedef DWORD (*PPPFSM_SENDPACKET_CB)(PVOID context, IN USHORT ProtocolWord, IN PBYTE pData, IN DWORD cbData);
typedef DWORD (*PPPFSM_SESSIONLOCK_CB)(PVOID context);
typedef DWORD (*PPPFSM_SESSIONUNLOCK_CB)(PVOID context);

typedef void  (*PPPFSM_EXTENSION_MESSAGE_CB)(PVOID context, IN BYTE code, IN BYTE id, IN PBYTE pData, IN DWORD cbData);

//
//	The common FSM code handles standard message types 1-7.
//  Custom handlers for other types can be added via this structure.
//
typedef struct
{
	BYTE                        code;	      // message code, e.g. LCP_ECHO_REQUEST
	PSTR						szName;
    PPPFSM_EXTENSION_MESSAGE_CB MessageRxCb;  // callback handler to process received messages of the type
} PppFsmExtensionMessageDescriptor, *PPppFsmExtensionMessageDescriptor;

typedef struct
{
	PSTR							  szProtocolName;
	USHORT							  ProtocolWord;
	DWORD							  cbMaxTxPacket;  // Maximum length of a configure-request packet we will build
	PPPFSM_THISLAYERUP_CB		      ThisLayerUpCb;	// Called when negotiations complete successfully, layer is up
	PPPFSM_THISLAYERDOWN_CB			  ThisLayerDownCb;
	PPPFSM_THISLAYERSTARTED_CB		  ThisLayerStartedCb;	// Called when negotiations begin
	PPPFSM_THISLAYERFINISHED_CB		  ThisLayerFinishedCb;
	PPPFSM_SENDPACKET_CB			  SendPacketCb;
	PPPFSM_SESSIONLOCK_CB			  SessionLockCb;
	PPPFSM_SESSIONUNLOCK_CB			  SessionUnlockCb;
	PPppFsmExtensionMessageDescriptor pExtensionMessageDescriptors; // may be null if no extension messages types
} PppFsmDescriptor, *PPppFsmDescriptor;

typedef struct PppFsm
{
	PPppFsmDescriptor	pDescriptor;
	PVOID				CbContext;

	PppFsmState			state;

	CTETimer			timer;
	BOOL				bTimerRunning;

	//
	//	The restart timer is used to time transmissions
	//	of configure-request and terminate-request packets.
	//	Expiration of the restart timer causes a timeout,
	//	and potentially retransmission of the corresponding
	//	configure-request or terminate-request packet.
	//
	DWORD				dwRestartTimerMs;
#define DEFAULT_RESTART_TIMER_MS	3000	// 3 seconds

	//
	//	Number of timeouts on terminate-requests before assuming
	//	that the peer is unable to respond.
	//
	DWORD				dwMaxTerminate;
#define DEFAULT_MAX_TERMINATE		2

	//
	//	Maximum number of consecutive timeouts on configure-request
	//	packets before assuming the peer is unable to respond.
	//
	DWORD				dwMaxConfigure;
#define	DEFAULT_MAX_CONFIGURE		10

	//
	//	Time delay after receiving a terminate-request in the Opened
	//	state prior to entering the stopped state.
	//
#define STOP_DELAY_TIME_MS			100

	//
	//	Counts down from dwMaxTerminate or dwMaxConfigure.
	//	Decremented on timeout. If it reaches 0, we assume the peer
	//	is not responding.
	//
	DWORD				dwRestartCount;

	DWORD               bmActions;
#define ACTION_SEND_REQUEST	 (1<<0) // Send pRequestBuffer
#define ACTION_SEND_RESPONSE (1<<1) // Send abResponsePacket

	//
	//	Buffer in which request packets (CR, TR) are constructed.
	//
	PBYTE				pRequestBuffer;
	DWORD				cbRequestBuffer;

	BYTE				idTxCR;	// id of the last CR packet we sent
	BYTE				idTxTR; // id of the last TR packet we sent

	BYTE			    idNextCodeReject;

	//
	//	Description of how options are to be negotiated.
	//
	OptionContext		optionContext;

	//
	//  Buffer in which response packets (ACK, NAK, REJ, TA, CodeReject) are constructed.
	//
	DWORD				cbResponsePacket;
	BYTE				abResponsePacket[1500];

} PppFsm, *PPppFsm;


#define PPP_PACKET_HEADER_SIZE	4

// Utility macro to initialize a packet header

#define PPP_SET_PACKET_LEN(pPacket, len) \
	(pPacket)[2] = (BYTE)((len) >> 8); \
	(pPacket)[3] = (BYTE)((len) >> 0)

#define PPP_GET_PACKET_LEN(pPacket) \
	(((pPacket)[2] << 8) | ((pPacket)[3]))

#define PPP_SET_PACKET_HEADER(pPacket, type, id, len) \
	(pPacket)[0] = (type); \
	(pPacket)[1] = (id);   \
	PPP_SET_PACKET_LEN(pPacket, len) 

#define PPP_INIT_PACKET(pPacket, type, id) \
	PPP_SET_PACKET_HEADER(pPacket, type, id, PPP_PACKET_HEADER_SIZE)

//
//	PPP Message Codes
//
#define PPP_CONFIGURE_REQUEST	1
#define PPP_CONFIGURE_ACK		2
#define PPP_CONFIGURE_NAK		3
#define PPP_CONFIGURE_REJ		4
#define PPP_TERMINATE_REQUEST	5
#define PPP_TERMINATE_ACK		6
#define PPP_CODE_REJECT			7

PPppFsm
PppFsmNew(
	IN	PPppFsmDescriptor			pDescriptor,
	IN	OPTION_RESET_PEER_OPT_CB	CbResetPeerOptions,
	IN	PVOID						CbContext);

void
PppFsmDelete(
	PPppFsm	pFsm);

DWORD
PppFsmOptionsAdd(
	PPppFsm					pFsm,
	...);

DWORD
PppFsmOptionsSetORLs(
	PPppFsm					pFsm,
	...);

DWORD
PppFsmOpen(
	PPppFsm	pFsm);

DWORD
PppFsmClose(
	PPppFsm	pFsm);

DWORD
PppFsmRenegotiate(
	PPppFsm	pFsm);

DWORD
PppFsmLowerLayerUp(
	PPppFsm	pFsm);

DWORD
PppFsmLowerLayerDown(
	PPppFsm	pFsm);

DWORD
PppFsmProtocolRejected(
	PPppFsm	pFsm);

void
PppFsmProcessRxPacket(
	IN	OUT	PPppFsm	pFsm,
	IN		PBYTE	pPacket,
	IN		DWORD	cbPacket);
