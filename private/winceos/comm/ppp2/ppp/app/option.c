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
//	PPP Option Negotiation Implementation
//


//  Include Files

#include "windows.h"
#include "types.h"
#include "layerfsm.h"
#include "debug.h"
#include "raserror.h"

#define MALLOC(size)	LocalAlloc(LPTR, (size))
#define FREE(ptr)		LocalFree(ptr)

void
OptionContextInitialize(
	POptionContext				pContext,
	PSTR						szProtocolName,
	OPTION_RESET_PEER_OPT_CB	CbResetPeerOptions,
	PVOID						CbContext)
//
//	Initialize an option negotiation context structure.
//
{
	pContext->szProtocolName = szProtocolName;
	pContext->pOptionList = NULL;
	pContext->CbResetPeerOptions = CbResetPeerOptions;
	pContext->CbContext = CbContext;
	pContext->dwMaxFailure = DEFAULT_MAX_FAILURE;	
	pContext->dwConsecutiveFailureCount = 0;	
}

void
OptionContextCleanup(
	POptionContext pContext)
//
//	Release all the resources allocated for an option negotiation context
//
{
	POptionInfo pInfo,
				pNext;

	if (pContext)
	{
		for (pInfo = pContext->pOptionList; pInfo; pInfo = pNext)
		{
			pNext = pInfo->next;
			FREE(pInfo);
		}
	}
}

DWORD
OptionInfoAdd(
	POptionContext			pContext,
	POptionDescriptor		pDescriptor,
	OptionRequireLevel		orlLocal,
	OptionRequireLevel		orlPeer
	)
//
//	Add an option to the list of options known by the context.
//
{
	POptionInfo pInfo, *ppInfo;
	DWORD		dwResult = NO_ERROR;

	pInfo = (POptionInfo)MALLOC(sizeof(OptionInfo));
	if (pInfo == NULL)
	{
		dwResult = ERROR_OUTOFMEMORY;
	}
	else
	{
		pInfo->pDescriptor = pDescriptor;
		pInfo->orlLocal = orlLocal;
		pInfo->orlPeer = orlPeer;
		pInfo->onsLocal = ONS_Initial;

		// Add the new node to the end of the Option List

		for (ppInfo = &pContext->pOptionList; *ppInfo; ppInfo = &(*ppInfo)->next)
			;

		pInfo->next = NULL;
		*ppInfo = pInfo;
	}

	return dwResult;
}

POptionInfo
OptionInfoFind(
	POptionContext			pContext,
	BYTE					type)
//
//	Find the option info for the specified type.
//
//	Return NULL if type not found.
//
{
	POptionInfo pInfo;

	for (pInfo = pContext->pOptionList;
		 pInfo;
		 pInfo = pInfo->next)
	{
		if (pInfo->pDescriptor->type == type)
			break;
	}

	return pInfo;
}

DWORD
OptionSetORL(
	POptionContext			pContext,
	BYTE					type,
	OptionRequireLevel		orlLocal,
	OptionRequireLevel		orlPeer
	)
//
//	Set the ORLs for the specified option
//
{
	POptionInfo pInfo;
	DWORD		dwResult = NO_ERROR;

	pInfo = OptionInfoFind(pContext, type);
	if (pInfo)
	{
		pInfo->orlLocal = orlLocal;
		pInfo->orlPeer = orlPeer;
	}
	else
	{
		dwResult = ERROR_INVALID_PARAMETER;
	}

	return dwResult;
}

DWORD
OptionResponseAdd(
	__inout_bcount(cbResponse)         PBYTE	pResponse,
	IN		                           DWORD	cbResponse,
	IN		                           BYTE	    code,					// ACK, NAK, REJ
	IN		                           BYTE	    optionType,				// protocol specific option type identifier
	__in_bcount_opt(cbOptData)		   PBYTE	pOptData,
	IN		                           DWORD	cbOptData)
//
//	Add an option to a response packet being built.
//
//	If the response packet is already a NAK or REJ, then don't add
//	any ACKs to it.
//	If the  response packet is already a REJ, the only add more REJs
//	to it.
//
{
	BYTE	codeCurrent = pResponse[0];
	DWORD	dwResult = NO_ERROR;
	DWORD  offset;
	DWORD	lenOpt;

    PREFAST_ASSERT(cbResponse > PPP_PACKET_HEADER_SIZE);
	PREFAST_ASSERT(cbOptData < 254);

	if (code < codeCurrent)
	{
		// Don't add the option to the response, since
		// we are already doing a REJ or a NAK of other options.
	}
	else
	{
		if (code > codeCurrent)
		{
			// Reset the packet to the new code
			pResponse[0] = code;
			pResponse[2] = 0;
			pResponse[3] = 4;
		}

		// Add the option to the list of options in the response

		offset = (pResponse[2] << 8) | pResponse[3];

		lenOpt = 2 + cbOptData;

		PREFAST_SUPPRESS(12013, "offset + lenOpt cannot overflow, the numbers are small");
		if (offset + lenOpt < cbResponse)
		{
			pResponse[offset] = optionType;
			pResponse[offset + 1] = (BYTE)lenOpt;
			memcpy(pResponse + offset + 2, pOptData, cbOptData);

			offset += lenOpt;
			pResponse[2] = (BYTE)(offset >> 8);
			pResponse[3] = (BYTE)offset;
		}
		else
		{
			dwResult = ERROR_INSUFFICIENT_BUFFER;
		}
	}

	return dwResult;
}

DWORD
OptionsWalk(
	IN		PBYTE		   pOptions,
	IN		DWORD		   cbOptions,
	IN	OUT	POptionContext pContext,	OPTIONAL
	IN		DWORD		  (*pOptionCb)(POptionContext pContext, POptionHeader pOption) OPTIONAL)
//
//	Walk through the options contained within a frame, calling the
//	specified function on each option.
//	If the options are invalid in any way, return ERROR_PPP_INVALID_PACKET.
//
{
    POptionHeader  pOption;
	DWORD		   dwResult = NO_ERROR;
	BOOL		   frameOptionSizesOk = TRUE;	

	for (pOption = (POptionHeader)pOptions;
	     cbOptions;
		 pOption = (POptionHeader)((PBYTE)pOption + pOption->Len))
	{
		if ((cbOptions < 2)
		||  (pOption->Len < 2)
		||  (pOption->Len > cbOptions ))
		{
			// Invalid frame, either:
			//  1. Insufficient size for the option header (Type + Len)
			//  2. option length is too small (less than option header size)
			//  3. option length is too large (greater than remaining bytes in frame)
			dwResult = ERROR_PPP_INVALID_PACKET;
			break;
		}

		if (pOptionCb)
		{
			dwResult = pOptionCb(pContext, pOption);
			if (dwResult != NO_ERROR)
				break;
		}
		cbOptions -= pOption->Len;
	}

	return dwResult;
}

#ifdef DEBUG
void
OptionDebugAppendOptionList(
	IN		POptionContext pContext,
	IN		PSTR	pBuffer,
	IN		PBYTE	pOptions,
	IN		DWORD	cbOptions)
//
//	Append to pBuffer an option list in ASCII readable text.
//
{
    POptionHeader  pOption;
	BOOL		   frameOptionSizesOk = TRUE;
	POptionInfo	   pInfo;

	pBuffer += strlen(pBuffer);
	for (pOption = (POptionHeader)pOptions;
	     cbOptions;
		 pOption = (POptionHeader)((PBYTE)pOption + pOption->Len))
	{
		if ((cbOptions < 2)
		||  (pOption->Len < 2)
		||  (pOption->Len > cbOptions ))
		{
			pBuffer += sprintf(pBuffer, "ERROR - BAD OPTION Len=%u cbOptions=%u ", pOption->Len, cbOptions);
			break;
		}

		pInfo = OptionInfoFind(pContext, pOption->Type);
		if (pInfo)
		{
			pBuffer += sprintf(pBuffer, " %hs", pInfo->pDescriptor->szName);
		}
		else
		{
			pBuffer += sprintf(pBuffer, " [%u]", pOption->Type);
		}
		if (pOption->Len > OPTION_HEADER_LENGTH)
		{
			BYTE  cbOptData = pOption->Len - OPTION_HEADER_LENGTH;
			PBYTE pOptData = (PBYTE)pOption + OPTION_HEADER_LENGTH;

			*(pBuffer++) = '=';
			if (pInfo && pInfo->pDescriptor->OptionToStrCb)
			{
				pInfo->pDescriptor->OptionToStrCb(pContext->CbContext, pInfo, pOptData, cbOptData, &pBuffer);
			}
			else
			{
				// Just do hex dump of option data
				while (cbOptData--)
				{
					pBuffer += sprintf(pBuffer, "%02X", *pOptData++);
				}
			}
		}

		cbOptions -= pOption->Len;
	}
}
#endif


DWORD
OptionRequest(
	IN	OUT	POptionContext pContext,
	IN		POptionHeader  pOption)
//
//	The peer is requesting a value for the option via a configure-request.
//
{
	DWORD		dwResult = NO_ERROR;
	POptionInfo pInfo;
	BYTE		type = pOption->Type;
	BYTE		len  = pOption->Len;
	BYTE		code;
	PBYTE		pOptData = (PBYTE)pOption + OPTION_HEADER_LENGTH;
	DWORD		cbOptData = len - OPTION_HEADER_LENGTH;

	//
	//	Any length errors should have been caught already.
	//
	ASSERT(len >= OPTION_HEADER_LENGTH);

	pInfo = OptionInfoFind(pContext, type);

	if (pInfo == NULL || pInfo->orlPeer == ORL_Unsupported)
	{
		//
		//	We don't support this option, inform the peer via a reject.
		//
		code = PPP_CONFIGURE_REJ;
	}
	else if (pInfo->pDescriptor->fixedLength != OPTION_VARIABLE_LENGTH
	&&       pInfo->pDescriptor->fixedLength != cbOptData)
	{
		//
		//	Option length is invalid. Pretty hopeless situation if the peer is unable to
		//	set the option length to the value we expect.
		//
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Protocol %hs Option %hs length is %u, expected %u\n"),
			pContext->szProtocolName, pInfo->pDescriptor->szName, len, pInfo->pDescriptor->fixedLength));
		code = PPP_CONFIGURE_REJ;
	}
	else
	{
		pInfo->bRequested = TRUE;

		pInfo->pDescriptor->RequestOptionCb(pContext->CbContext, pInfo, &code, &pOptData, &cbOptData);

		if (code == PPP_CONFIGURE_NAK
		&&  pContext->dwConsecutiveFailureCount >= pContext->dwMaxFailure)
		{
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Protocol %hs Failing to converge, REJecting Option %hs instead of NAKing\n"),
				pContext->szProtocolName, pInfo->pDescriptor->szName));
			code = PPP_CONFIGURE_REJ;
			pOptData = (PBYTE)pOption + OPTION_HEADER_LENGTH;
			cbOptData = len - OPTION_HEADER_LENGTH;
		}
	}

	//
	//	Update the response
	//
	dwResult = OptionResponseAdd(pContext->pResponse, pContext->cbResponse, code, type, pOptData, cbOptData);

	return dwResult;
}

DWORD
OptionAcked(
	IN	OUT	POptionContext pContext,
	IN		POptionHeader  pOption)
//
//	The value for the option has been accepted from the negotiations,
//	and we can now make it go into effect.
//
{
	DWORD       dwResult = NO_ERROR;
	POptionInfo pInfo;
	BYTE		type = pOption->Type;
	BYTE		len  = pOption->Len;

	//
	//	Any length errors should have been caught already.
	//
	ASSERT(len >= OPTION_HEADER_LENGTH);

	pInfo = OptionInfoFind(pContext, type);
	//
	//	All the options in an ACK message should be ones that we sent
	//	in a config-request.  And we should only be sending options
	//	that we support in a config-request.  Hence info should always
	//	be found for an option here.
	//
	ASSERT(pInfo && pInfo->onsLocal == ONS_Requested);

	if (pInfo)
	{
		ASSERT(pInfo->pDescriptor->fixedLength == OPTION_VARIABLE_LENGTH || pInfo->pDescriptor->fixedLength == len - OPTION_HEADER_LENGTH);
		pInfo->onsLocal = ONS_Acked;

		if (pInfo->pDescriptor->AckOptionCb)
			dwResult = pInfo->pDescriptor->AckOptionCb(pContext->CbContext, pInfo, (PBYTE)pOption + OPTION_HEADER_LENGTH, len - OPTION_HEADER_LENGTH);
	}

	return dwResult;
}

DWORD
OptionNak(
	IN	OUT	POptionContext pContext,
	IN		POptionHeader  pOption)
//
//	The peer is suggesting a value for the option.
//
{
	DWORD       dwResult = NO_ERROR;
	POptionInfo pInfo;
	BYTE		type = pOption->Type;
	BYTE		len  = pOption->Len;
	PBYTE		pOptData = (PBYTE)pOption + OPTION_HEADER_LENGTH;
	DWORD		cbOptData = len - OPTION_HEADER_LENGTH;

	//
	//	Any length errors should have been caught already.
	//
	ASSERT(len >= OPTION_HEADER_LENGTH);

	pInfo = OptionInfoFind(pContext, type);
	//
	//	A NAK can to either suggest an alternative value for an option
	//	that was present in the configure request, or can be requesting
	//	the negotiation of an option that was not present in the configure-request.
	//	In the latter case, we may not support that option at all, so
	//  pInfo will be NULL and we just ignore the peer on that option.
	//

	if (pInfo && pInfo->orlLocal != ORL_Unsupported)
	{
		if (pInfo->pDescriptor->fixedLength != OPTION_VARIABLE_LENGTH
		&&  pInfo->pDescriptor->fixedLength != cbOptData)
		{
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Protocol %hs Option %hs length is %u, expected %u\n"),
				pContext->szProtocolName, pInfo->pDescriptor->szName, cbOptData, pInfo->pDescriptor->fixedLength));
		}
		else
		{
			//
			//	If the handler wants to allow negotiation of an "allowed" option,
			//	it should leave the pInfo->onsLocal to ONS_Naked.  We will then add it
			//  to the next configure-request.  In the unusual case where the handler
			//  does not want to negotiate the value it can set pInfo->onsLocal = ONS_Initial.
			//
			pInfo->onsLocal = ONS_Naked;

			if (pInfo->pDescriptor->NakOptionCb)
				pInfo->pDescriptor->NakOptionCb(pContext->CbContext, pInfo, pOptData, cbOptData);
		}
	}

	return dwResult;
}

DWORD
OptionRej(
	IN	OUT	POptionContext pContext,
	IN		POptionHeader  pOption)
//
//	The peer does not want to use an option we requested.
//
{
	DWORD       dwResult = NO_ERROR;
	POptionInfo pInfo;
	BYTE		type = pOption->Type;
	BYTE		len  = pOption->Len;

	//
	//	Any length errors should have been caught already.
	//
	ASSERT(len >= 2);

	pInfo = OptionInfoFind(pContext, type);

	if (pInfo)
	{
		//
		//	Most of the time all that needs to be done is to set the status
		//  of the option to Rejected so that it does not get added to the
		//  next configure-request. However, an optional reject handler can
		//  be provided which can override this default behaviour.
		//
		//  If a required option is rejected, then we will fail out of the
		//  negotiations when we try to construct our next configure request.
		//
		pInfo->onsLocal = ONS_Rejected;
		DEBUGMSG(ZONE_TRACE && pInfo->orlLocal == ORL_Required, (L"PPP: Required option %hs REJected\n", pInfo->pDescriptor->szName));
		if (pInfo->pDescriptor->RejOptionCb)
			pInfo->pDescriptor->RejOptionCb(pContext->CbContext, pInfo, (PBYTE)pOption + OPTION_HEADER_LENGTH, len - OPTION_HEADER_LENGTH);
	}
	else
	{
		//
		//	A REJ should only be sent for an option that we requested,
		//	so if pInfo is NULL it means the peer is misbehaving.
		//

		DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - RX REJ for unknown option Type %u\n"), type));
	}

	return dwResult;
}

BOOL
OptionValidateSizes(
	IN		PBYTE		   pOptions,
	IN		DWORD		   cbOptions)
//
//	Check that the sizes of a list of PPP options are legal.
//
//	Return TRUE if they are ok, FALSE if not.
//
{
	return OptionsWalk(pOptions, cbOptions, NULL, NULL) == NO_ERROR;
}

DWORD
OptionBuildConfigureRequest(
	IN		POptionContext pContext,
	IN      BYTE           id,
		OUT	PBYTE		   pFrame,
	IN	OUT	PDWORD		   pcbFrame)
//
//	Build a configure-request to be sent to the peer.
//
{
	POptionInfo pOptionList;
	PBYTE		pOptions;
	USHORT		cbOptions;
	DWORD		cbOptData,
				cbSpaceRemaining;
	DWORD		dwResult = NO_ERROR;

	cbSpaceRemaining = *pcbFrame;
	*pcbFrame = 0;

	if (cbSpaceRemaining < PPP_PACKET_HEADER_SIZE)
	{
		// Insufficient space to even put the basic frame header
		dwResult = ERROR_INSUFFICIENT_BUFFER;
	}
	else
	{
		cbSpaceRemaining -= PPP_PACKET_HEADER_SIZE;
		pOptions = &pFrame[PPP_PACKET_HEADER_SIZE];
		cbOptions = 0;

		for (pOptionList = pContext->pOptionList;
			 pOptionList;
			 pOptionList = pOptionList->next)
		{
			if (pOptionList->orlLocal == ORL_Required && pOptionList->onsLocal == ONS_Rejected)
			{
				// A required option has been rejected by the peer.
				// Negotiation has failed.
				dwResult = ERROR_CAN_NOT_COMPLETE;
				break;
			}

			//
			//	Add an option to the configure-request if:
			//		1. It is required
			//		2. It is wanted and has not been rejected
			//		3. It is allowed and the peer has Naked requesting it
			//
			if ((pOptionList->orlLocal == ORL_Required)
			||  (pOptionList->orlLocal == ORL_Wanted && pOptionList->onsLocal != ONS_Rejected)
			||  (pOptionList->orlLocal == ORL_Allowed && pOptionList->onsLocal == ONS_Naked))
			{
				BYTE	cbMinSpaceNeeded = OPTION_HEADER_LENGTH;

				//
				//	Check for sufficient buffer space to add the option to the packet
				//
				if (pOptionList->pDescriptor->fixedLength != OPTION_VARIABLE_LENGTH)
				{
					cbMinSpaceNeeded += pOptionList->pDescriptor->fixedLength;
				}
				if (cbSpaceRemaining < cbMinSpaceNeeded)
				{
					// Insufficient space remains in packet to add another option
					dwResult = ERROR_INSUFFICIENT_BUFFER;
					break;
				}

				//
				//	Get the option data
				//
				if (pOptionList->pDescriptor->BuildOptionCb == NULL)
				{
					ASSERT(pOptionList->pDescriptor->fixedLength == 0);

					// Boolean option, no data, no need to call BuildOptionCb
					cbOptData = 0;
					dwResult = NO_ERROR;
				}
				else
				{
					cbOptData = cbSpaceRemaining - OPTION_HEADER_LENGTH; // Tell the callback the max space they have
					dwResult = pOptionList->pDescriptor->BuildOptionCb(pContext->CbContext, pOptionList, pOptions + OPTION_HEADER_LENGTH, &cbOptData);
				}
				
				//
				//	Add the option to the packet
				//
				if (dwResult == NO_ERROR)
				{
					// If the option length is fixed, the callback MUST set the size to be equal to the fixed length
					ASSERT(pOptionList->pDescriptor->fixedLength == OPTION_VARIABLE_LENGTH || cbOptData == pOptionList->pDescriptor->fixedLength);

					// Maximum length for option data is 253, since length+2 must fit in a BYTE.
					ASSERT(cbOptData < 254);

					// Callback handler must not use more space than was available.
					ASSERT(cbOptData < cbSpaceRemaining - OPTION_HEADER_LENGTH);

					// Fill in the Type and Len for the option
					pOptions[0] = pOptionList->pDescriptor->type;
					pOptions[1] = (BYTE)(cbOptData + OPTION_HEADER_LENGTH);
					pOptions += cbOptData + OPTION_HEADER_LENGTH;
					cbOptions += (BYTE)(cbOptData + OPTION_HEADER_LENGTH);
					cbSpaceRemaining -= cbOptData + OPTION_HEADER_LENGTH;

					pOptionList->onsLocal = ONS_Requested;
				}
				else if (dwResult == ERROR_PPP_SKIP_OPTION)
				{
					// Just skip adding this option to the config-request
					dwResult = NO_ERROR;
				}
				else
				{
					break;
				}
			}
		}

		if (dwResult == NO_ERROR)
		{
			cbOptions += PPP_PACKET_HEADER_SIZE;
			PPP_SET_PACKET_HEADER(pFrame, PPP_CONFIGURE_REQUEST, id, cbOptions);

			*pcbFrame = cbOptions;
		}
	}

	return dwResult;
}

void
OptionResetNegotiation(
	IN	OUT	POptionContext pContext)
//
//	Reset the state of the option negotiation to the initial state.
//	That is, set it to the state prior to any configure-requests
//	being seen.
//
{
	POptionInfo pInfo;

	pContext->dwConsecutiveFailureCount = 0;	

	//
	// Reset the state of all the options we are looking for
	// so that we can track which ones we see and which we
	// do not.
	//

	for (pInfo = pContext->pOptionList;
		 pInfo;
		 pInfo = pInfo->next)
	{
		pInfo->onsLocal = ONS_Initial;
		pInfo->bRequested = FALSE;
		pInfo->bSuggested = FALSE;
	}
}

DWORD
OptionProcessPeerConfigRequest(
	IN	OUT	                                        POptionContext pContext,
	__in_bcount(cbOptions)		                    PBYTE		   pOptions,
	IN		                                        DWORD		   cbOptions,
	__out_bcount_part(*pcbResponse, *pcbResponse)	PBYTE		   pResponse,
	IN	OUT	                                        PDWORD		   pcbResponse,
	IN		                                        BYTE		   id)
//
//	Processs the options in a received configure-request message.
//
{
	POptionInfo pInfo;
	DWORD		dwResult = NO_ERROR;
	PBYTE   	pOptData;
	DWORD    	cbOptData;
	BYTE        code;

	//
	//	Reset all Rx options to default values
	//
	pContext->CbResetPeerOptions(pContext->CbContext);

    PREFAST_ASSERT(*pcbResponse >= PPP_PACKET_HEADER_SIZE);

	// Initialize the response to be an ACK with 0 options
	PPP_INIT_PACKET(pResponse, PPP_CONFIGURE_ACK, id);

	//
	// Reset the state of all the options we are looking for
	// so that we can track which ones we see and which we
	// do not.
	//

	for (pInfo = pContext->pOptionList;
		 pInfo;
		 pInfo = pInfo->next)
	{
		pInfo->bRequested = FALSE;
	}

	// Process each option, building the ACK/NAK/REJ response

	pContext->pResponse = pResponse;
	pContext->cbResponse = *pcbResponse;
	OptionsWalk(pOptions, cbOptions, pContext, OptionRequest);

	// Scan the list for any required/wanted options that were not present in the CR
	// NAK these (except only NAK for a wanted option once per negotiation)

	if (pResponse[0] != PPP_CONFIGURE_REJ)
	{
		for (pInfo = pContext->pOptionList;
			 pInfo;
			 pInfo = pInfo->next)
		{
			if (!pInfo->bRequested
			&&  ( (pInfo->orlPeer == ORL_Required)
			 ||  ((pInfo->orlPeer == ORL_Wanted) && !pInfo->bSuggested)))
			{
				pInfo->bSuggested = TRUE; // Set the suggested flag so we don't do this again

				if (pContext->dwConsecutiveFailureCount >= pContext->dwMaxFailure)
				{
					// Too many NAKs in a row, we are not converging on a configuration.
					dwResult = ERROR_PPP_NOT_CONVERGING;
					break;
				}

				pOptData = NULL;
				cbOptData = 0;
				dwResult = pInfo->pDescriptor->RequestOptionCb(pContext->CbContext, pInfo, &code, &pOptData, &cbOptData);
				if (dwResult == NO_ERROR)
				{
					ASSERT(pInfo->pDescriptor->fixedLength == OPTION_VARIABLE_LENGTH || cbOptData == pInfo->pDescriptor->fixedLength);
					dwResult = OptionResponseAdd(pContext->pResponse, pContext->cbResponse, PPP_CONFIGURE_NAK, pInfo->pDescriptor->type, pOptData, cbOptData);
				}
				if (dwResult != NO_ERROR)
				{
					break;
				}
			}
		}
	}

	if (dwResult == NO_ERROR)
	{
		if (pResponse[0] == PPP_CONFIGURE_ACK)
			pContext->dwConsecutiveFailureCount = 0;
		else if (pResponse[0] == PPP_CONFIGURE_NAK)
			pContext->dwConsecutiveFailureCount++;
	}

	*pcbResponse = (pResponse[2] << 8) | pResponse[3]; 

	return dwResult;
}

DWORD
OptionProcessRxMessage(
	IN	OUT	                                            POptionContext pContext,
	__in_bcount(cbRxData)                               PBYTE		   pRxData,
	IN		                                            DWORD		   cbRxData,
	__out_bcount_part_opt(*pcbResponse, *pcbResponse)	PBYTE		   pResponse,
	__inout_opt                                         PDWORD		   pcbResponse)
//
//	Process an option negotiation message for our protocol.
//	There are four kinds of option negotiation messages:
//		Configure-Request
//		Configure-Ack
//		Configure-Nak
//		Configure-Rej
//
//  If a Response (ACK/NAK/REJ) message is being processed (rather than a request), then
//  pResponse will be NULL because responses are not generated for responses.
//
//  For a Request message:
//     1. On entry, *pcbResponse contains the size of the space pointed to by pResponse
//	      available to build a response message.
//	   2. On exit, *pcbResponse will be non-zero if a response message (stored in pResponse)
//	      is to be sent.
{
	DWORD		cbResponse,
				cbMaxResponse,
				cbOptions;
	PBYTE		pOptions;
	BYTE		code, id;
	USHORT		length;
	DWORD		dwResult = NO_ERROR;

	cbMaxResponse = PPP_PACKET_HEADER_SIZE;
	if (pcbResponse)
	{
		cbMaxResponse = *pcbResponse;
		*pcbResponse = 0;
	}
	cbResponse = 0;

	do
	{
		//
		// Building any kind of response will require a minimal
		// 4 byte header
		//
		if (cbMaxResponse < PPP_PACKET_HEADER_SIZE)
		{
			dwResult = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

		//
		//	pData format:
		//		<code:1 octet> <id:1 octet> <length:2 octets> <options:length-4 octets>
		//
		if (cbRxData < PPP_PACKET_HEADER_SIZE)
		{
			// Too short to contain the minimal header
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Packet length %u less than minimal message header (4)\n"), cbRxData));
			dwResult = ERROR_INVALID_MESSAGE;
			break;
		}
		code = pRxData[0];
		id = pRxData[1];
		length = PPP_GET_PACKET_LEN(pRxData);

		ASSERT(PPP_CONFIGURE_REQUEST <= code && code <= PPP_CONFIGURE_REJ);

		if (length < PPP_PACKET_HEADER_SIZE || length > cbRxData)
		{
			// 
			// Specified length is shorter than a valid header or
			// greater than the count of received bytes - ignore the frame
			//
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Message type %u length %u invalid (rx packetlen=%u\n"), code, length, cbRxData));
			dwResult = ERROR_INVALID_MESSAGE;
			break;
		}

		pOptions = pRxData + PPP_PACKET_HEADER_SIZE;
		cbOptions = length - PPP_PACKET_HEADER_SIZE;

		//
		//	Verify the option list is syntactically correct
		//
		if (!OptionValidateSizes(pOptions, cbOptions))
		{
			// Option sizes are invalid, ignore the frame
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Frame received with invalid option sizes\n")));
			dwResult = ERROR_INVALID_MESSAGE;
			break;
		}

		switch (code)
		{
		case PPP_CONFIGURE_REQUEST:
			cbResponse = cbMaxResponse;
			dwResult = OptionProcessPeerConfigRequest(pContext, pOptions, cbOptions, pResponse, &cbResponse, id);
			if (dwResult == NO_ERROR)
				*pcbResponse = cbResponse;
			break;

		case PPP_CONFIGURE_ACK:
			// Commit the options
			dwResult = OptionsWalk(pOptions, cbOptions, pContext, OptionAcked);
			break;

		case PPP_CONFIGURE_NAK:
			OptionsWalk(pOptions, cbOptions, pContext, OptionNak);
			break;

		case PPP_CONFIGURE_REJ:
			// Set the state of each rejected option
			OptionsWalk(pOptions, cbOptions, pContext, OptionRej);
			break;
		}
	} while (FALSE); // end do

	return dwResult;
}

