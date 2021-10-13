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
*   @module ccp.h | CCP NCP Header File
*
*   Date: 1-9-96
*
*   @comm
*/


#ifndef CCP_H
#define CCP_H

#include "layerfsm.h"

#include "ndis.h"
#include "ndiswan.h"
#include "ipexport.h"
#include "rc4.h"
#include "sha.h" // SHA Library

// CCP Message Codes

#define	CCP_RESET_REQ			14
#define CCP_RESET_ACK			15

// CPP Option Enumeration

#define CCP_OPT_MSC_CCP			18

// Definitions for the Supported Bits field of CCP option 18 (CCP_OPT_MSC_CCP)

#define	MCCP_COMPRESSION			0x00000001
#define MSTYPE_ENCRYPTION_40        0x00000010	// obsolete
#define MSTYPE_ENCRYPTION_40F       0x00000020
#define MSTYPE_ENCRYPTION_128       0x00000040
#define MSTYPE_ENCRYPTION_56        0x00000080
#define MSTYPE_ENCRYPTION_40V       0x00000100
#define MSTYPE_HISTORYLESS          0x01000000	// stateless mode

////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

// Option Values

typedef struct ccp_option_values
{
	//
	//	The Supported Bits being negotiated
	//
	DWORD	SupportedBits;
} 
ccpOptValue_t;

//
// Encryption key sizes
//
#ifndef MAX_SESSIONKEY_SIZE
#define MAX_SESSIONKEY_SIZE     8
#endif

#ifndef MAX_USERSESSIONKEY_SIZE
#define MAX_USERSESSIONKEY_SIZE 16
#endif

#ifndef MAX_CHALLENGE_SIZE
#define MAX_CHALLENGE_SIZE      8
#endif

#define MAX_NT_RESPONSE         24

#define MAX_EAPKEY_SIZE         256

//
// This information is used to describe the encryption that is being
// done on the bundle.  At some point this should be moved into
// wanpub.h and ndiswan.h.
//
typedef struct _ENCRYPTION_INFO{
	UCHAR			StartKey[16];			// Start key
	UCHAR			SessionKey[16];			// Session key used for encrypting
	ULONG			SessionKeyLength;		// Session key length
	PVOID			Context;				// Key encryption context
	RC4_KEYSTRUCT	rc4KS;
	DWORD			bmEncryptionType;		// either MSTYPE_ENCRYPTION_40F or MSTYPE_ENCRYPTION_128
	DWORD			Flags;
#define					CRYPTO_IS_SERVER	(1<<0)
#define					CRYPTO_IS_SEND		(1<<1)	// Set if info for encrypting for tx
													// Clear if info for decrypting rx data
} ENCRYPTION_INFO, *PENCRYPTION_INFO;


//  CCP Context

typedef struct  ccp_context
{
    pppSession_t  	*session;                   // session context

	PPppFsm			pFsm;

    // Negotiated option values

    ccpOptValue_t   local;                      // local values (affect how we receive)
    ccpOptValue_t   peer;                      	// peer values (affect how we transmit)

	// Microsoft Compression Context and 
	// Translation Buffers
	//
	// Note these are allocated dynamically 
	// only when the option is negotiated.

	BOOL			txFlush;				// tx flush next packet
	USHORT			txCoherency;			// tx coherency count
	SendContext		mppcSndCntxt;			// send context

	USHORT			rxCoherency;			// rx coherency count
	USHORT			LastRC4Reset;			// Encryption key reset
	RecvContext		mppcRcvCntxt;			// receive context
	BOOL            bRxOutOfSync;           // Are we currently out of sync.
	DWORD           lastDecryptionKeyIndex; // Value 0..15 indicating index of
	                                        // last decryption key used

	// Encryption support

	DWORD			bmEncryptionTypesSupported;	// bitmask containing locally supported encryption types

	PENCRYPTION_INFO ptxEncryptionInfo;
	PENCRYPTION_INFO prxEncryptionInfo;

	// Scratch buffer used to build response option data
	BYTE			abResponseBits[4];

	// ID to use in next reset request packet

	BYTE			idResetRequest;

	// Buffer to copy decompressed packets to so that NAT
	// doesn't wind up writing over packet data in history
	// buffer.
	PBYTE           ScratchRxBuf;
	DWORD           cbScratchRxBuf;

} CCPContext, *PCCPContext;

// Function Prototypes

// Instance Management

DWORD	pppCcp_InstanceCreate( void *SessionContext, void **ReturnContext );
void    pppCcp_InstanceDelete( void  *context );

// Layer Management

void	pppCcp_LowerLayerUp(void *context);
void    pppCcp_LowerLayerDown(IN	PVOID	context);

// Message Processing

void    CcpProcessRxPacket( void *context, pppMsg_t *msg_p );
void    CpktProcessRxPacket( void *context, pppMsg_t *msg_p );
BOOL	pppCcp_Compress( pppSession_t *, PNDIS_WAN_PACKET *ppPacket, USHORT *pwProtocol );
DWORD   CcpOpen(IN	PVOID	context );
DWORD   CcpClose(IN	PVOID	context );
DWORD   CcpRenegotiate(IN	PVOID	context );
void    pppCcp_Rejected (void *context);

DWORD
ccpOptionInit(
	PCCPContext pContext);

void
ccpOptionValueReset(
	PCCPContext pContext);

void
ccpResetPeerOptionValuesCb(
	IN		PVOID  context);

void
ccpRxResetRequest(
	IN	PVOID	context,
	IN	BYTE	code,
	IN  BYTE    id,
	IN	PBYTE	pData,
	IN	DWORD	cbData);

void
ccpRxResetAck(
	IN	PVOID	context,
	IN	BYTE	code,
	IN  BYTE    id,
	IN	PBYTE	pData,
	IN	DWORD	cbData);

// Encryption

USHORT  EncryptTxData(
	IN		PCCPContext         c_p,
	IN	OUT PBYTE				pbData,
	IN		DWORD				cbData);

void DecryptRxData(
	IN		PCCPContext         c_p,
	IN	OUT PBYTE				pbData,
	IN		DWORD				cbData);

void
EncryptionInfoDelete(
	IN	PCCPContext         c_p,
	IN	PENCRYPTION_INFO	pEncryptionInfo);

PENCRYPTION_INFO
EncryptionInfoNew(
	IN	PCCPContext         c_p,
	IN	DWORD				bmEncryptionType,
	IN	DWORD				Flags);

void 
EncryptionInfoChangeKey(
	IN OUT PENCRYPTION_INFO	pEncryptInfo);

void
EncryptionInfoReinitializeKey(
	IN OUT PENCRYPTION_INFO	pEncryptInfo);


#endif
