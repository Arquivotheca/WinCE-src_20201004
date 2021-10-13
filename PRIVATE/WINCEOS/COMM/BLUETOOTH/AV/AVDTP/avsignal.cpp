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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// Implementation of AVDTP Signaling Component

#include <windows.h>
#include "avdtpriv.h"

// AVDTP packet types
#define AVDTP_PACKET_SINGLE     0
#define AVDTP_PACKET_START      1
#define AVDTP_PACKET_CONTINUE   2
#define AVDTP_PACKET_END        3

// AVDTP message types
#define AVDTP_MESSAGE_COMMAND   0
#define AVDTP_MESSAGE_RESERVED  1
#define AVDTP_MESSAGE_RESP_ACC  2
#define AVDTP_MESSAGE_RESP_REJ  3

#define AVDTP_SIGNAL_HEADER_SINGLE_SIZE     2
#define AVDTP_SIGNAL_HEADER_START_SIZE      3
#define AVDTP_SIGNAL_HEADER_CONTINUE_SIZE   1
#define AVDTP_SIGNAL_HEADER_END_SIZE        1

#define FIXED_DISCOVER_EP_ARRAY_SIZE    5

#define AVDTP_DISCOVER_EP_SIZE  2

int packet_size_table[4] = 
                {AVDTP_SIGNAL_HEADER_SINGLE_SIZE,
                 AVDTP_SIGNAL_HEADER_START_SIZE,
                 AVDTP_SIGNAL_HEADER_CONTINUE_SIZE,
                 AVDTP_SIGNAL_HEADER_END_SIZE};

AVDTSignal::AVDTSignal(void)
{
}

int AVDTSignal::Init(void)
{
    return ERROR_SUCCESS;
}

void AVDTSignal::Deinit(void)
{
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetNosp
    
    This private method gets the "number of signaling packets" based on the 
    payload size and MTU.  This implementation assumes payload > mtu.
    
    Parameters:
        cbPayload:  Payload size
        mtu:        Maximum transmission unit
    
    Returns (BYTE): "Number of signaling packets"
------------------------------------------------------------------------------*/
BYTE AVDTSignal::GetNosp(
            DWORD cbPayload, 
            DWORD mtu)
{
    SVSUTIL_ASSERT(cbPayload > mtu);
    
    DWORD cbRest = cbPayload - (mtu - AVDTP_SIGNAL_HEADER_START_SIZE);

    DWORD nosp = (1 +                                                       // Start packet
        (cbRest / (mtu - AVDTP_SIGNAL_HEADER_CONTINUE_SIZE)) +              // Middle packets
        ((cbRest % (mtu - AVDTP_SIGNAL_HEADER_CONTINUE_SIZE)) ? 1 : 0));    // Remainder in last packet

    SVSUTIL_ASSERT(nosp < 256);

    return (BYTE) nosp;
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetHeaderSize
    
    This private method gets the size of the signaling header based on the
    packet type.
    
    Parameters:
        bPacketType:    Type of signaling packet (START/END/CONTINUE/SINGLE)
    
    Returns (DWORD):    The size of the header in bytes
------------------------------------------------------------------------------*/
DWORD AVDTSignal::GetHeaderSize(
            BYTE bPacketType)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    SVSUTIL_ASSERT(bPacketType >= 0 && bPacketType <= 3);

    return packet_size_table[bPacketType];
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetSignalBuffer
    
    This private method gets a buffer of the specified size and adds the
    common AVDTP signaling headers to the front of the buffer.  This function
    will determine what packet type to use based on the requested payload size
    and the MTU for the specified session.
    
    Parameters:
        pSession:       Session object
        fContinue:      Is this the first packet or a continue?
        cbPayload:      Size of the payload data in bytes
        bSignalId:      Signal Id used in signaling header
        bMsgType:       Message type (command/ACC Resp/REJ Resp)
        pbPacketType:   Out param specifying packet type used
    
    Returns (BD_BUFFER):    The signaling buffer
------------------------------------------------------------------------------*/
BD_BUFFER* AVDTSignal::GetSignalBuffer(
            Session* pSession,
            BOOL fContinue,
            DWORD cbPayload, 
            BYTE bSignalId, 
            BYTE bMsgType,
            PBYTE pbPacketType)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bNosp = 0;

    // Determine the buffer size with header.  If this packet ends up not fitting in the
    // MTU the buffer size will just be set to the MTU.
    DWORD cbBuffer = 
        cbPayload + 
        (fContinue ? AVDTP_SIGNAL_HEADER_CONTINUE_SIZE : AVDTP_SIGNAL_HEADER_SINGLE_SIZE);

    if (pSession->usOutMTU < cbBuffer) {
        // The buffer does not fit in the MTU
        cbBuffer = pSession->usOutMTU;
        *pbPacketType = fContinue ? AVDTP_PACKET_CONTINUE : AVDTP_PACKET_START;
    } else {
        // The buffer fits in the MTU        
        *pbPacketType = fContinue ? AVDTP_PACKET_END : AVDTP_PACKET_SINGLE;
    }

    // For the START packet, calculate NOSP
    if (*pbPacketType == AVDTP_PACKET_START) {
        bNosp = GetNosp(cbPayload, pSession->usOutMTU);
    }

    // Allocate the buffer and create the header
    BD_BUFFER* pBuffer = GetAVDTPBuffer(cbBuffer);
    if (pBuffer) {
        PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart];
        pHeader[0] = bMsgType | (*pbPacketType << 2) | (pSession->bTransaction << 4);

        if (*pbPacketType == AVDTP_PACKET_SINGLE) {
            pHeader[1] = bSignalId;
        } else if (*pbPacketType == AVDTP_PACKET_START) {
            pHeader[1] = bNosp;
            pHeader[2] = bSignalId;
        }

        // Increment the transaction number
        if ((*pbPacketType == AVDTP_PACKET_SINGLE) || (*pbPacketType == AVDTP_PACKET_END)) {
            pSession->bTransaction = (pSession->bTransaction + 1) % 16;
        }
    }

    return pBuffer;
}

/*------------------------------------------------------------------------------
    AVDTSignal::CopyFragment
    
    When receiving signaling data, the data may arrive in fragments.  This
    private method will copy the fragments to a local buffer for processing
    later on.
    
    Parameters:
        pSession:   Session object
        pBuffer:    Incoming packet fragment to copy
------------------------------------------------------------------------------*/
int AVDTSignal::CopyFragment(
                Session* pSession, 
                BD_BUFFER* pBuffer)
{
    if (! pSession->pRecvBuffer) {
        // We will allocate this dynamically since it is a large
        // buffer and unnecessary overhead in each session since
        // fragmentation will rarely be required.
        pSession->pRecvBuffer = new BYTE[RECV_BUFFER_SIZE];
        if (! pSession->pRecvBuffer) {
            return FALSE;
        }

        SVSUTIL_ASSERT(pSession->cbRecvBuffer == 0);
    }

    int cbBuffer = BufferTotal(pBuffer);

    if ((cbBuffer > RECV_BUFFER_SIZE) || // check this for INT overflow
        ((cbBuffer + pSession->cbRecvBuffer) > RECV_BUFFER_SIZE)) {
        // Our implementation makes the assumption that nobody will ever
        // need to send more than RECV_BUFFER_SIZE bytes of data in a
        // fragmented signaling packet.
        SVSUTIL_ASSERT(0);
        return FALSE;
    }

    PREFAST_SUPPRESS (12008, "False positive, we check for INT overflow of cbBuffer + pSession->cbRecvBuffer");
    if (! BufferGetChunk(pBuffer, cbBuffer, pSession->pRecvBuffer + pSession->cbRecvBuffer)) {
        // This should never fail since we just checked the size above.
        SVSUTIL_ASSERT(0);
        return FALSE;
    }

    SVSUTIL_ASSERT(pSession->cRemainingFrags > 0);

    pSession->cbRecvBuffer += cbBuffer;
    pSession->cRemainingFrags--;

    return TRUE;    
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetCapabilityBufferSize
    
    This private method gets the required size of a buffer from a capability
    structure.
    
    Parameters:
        pCapabilityIE:  Array of capabilities 
        cCapabilityIE:  Number of capability array elements   
    
    Returns (DWORD): The size of the buffer
------------------------------------------------------------------------------*/
DWORD AVDTSignal::GetCapabilityBufferSize(
            PAVDTP_CAPABILITY_IE pCapabilityIE, 
            DWORD cCapabilityIEs)
{
    DWORD cbPayload = 0;
    
    for (DWORD i = 0; i < cCapabilityIEs; i++) {
        cbPayload += 2 + pCapabilityIE[i].bServiceCategoryLen;
    }
    
    return cbPayload;
}

/*------------------------------------------------------------------------------
    AVDTSignal::SerializeCapabilityBuffer
    
    This private method copies an array of capability structs to a buffer for
    transmitting to the peer device.
    
    Parameters:
        pCapabilityIE:      Array of capability structs
        cCapabilityIEs:     Number of array elements
        pbConfig:           The buffer which will store the capabilities
        cbPayload:          The size of the buffer in bytes
------------------------------------------------------------------------------*/
int AVDTSignal::SerializeCapabilityBuffer(
            PAVDTP_CAPABILITY_IE pCapabilityIE, 
            DWORD cCapabilityIEs, 
            PBYTE pbConfig, 
            DWORD cbPayload)
{
    DWORD cbConfig = 0;

    for (DWORD i = 0; i < cCapabilityIEs; i++) {
        pbConfig[cbConfig] = pCapabilityIE[i].bServiceCategory;
        pbConfig[cbConfig+1] = pCapabilityIE[i].bServiceCategoryLen;

        if (pCapabilityIE[i].bServiceCategory == AVDTP_CAT_MEDIA_CODEC) {
            pbConfig[cbConfig+2] = pCapabilityIE[i].MediaCodec.bMediaType << 4;
            pbConfig[cbConfig+3] = pCapabilityIE[i].MediaCodec.bMediaCodecType;

            memcpy(&pbConfig[cbConfig+4], pCapabilityIE[i].MediaCodec.CodecIEs, pCapabilityIE[i].bServiceCategoryLen - 2);
        }

        cbConfig += 2 + pCapabilityIE[i].bServiceCategoryLen;
    }

    SVSUTIL_ASSERT(cbConfig == cbPayload);

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    AVDTSignal::SerializeDiscoverBuffer
    
    This private method copies an array of endpoint info structs to a buffer
    for trasmitting to the peer device.
    
    Parameters:
        pSEIDInfo:  Array of SEID info structs
        cSEIDInfo:  Number of array elements
        pbDisc:     The buffer which will store the endpoint info
        cbPayload:  Size of the buffer in bytes
------------------------------------------------------------------------------*/
int AVDTSignal::SerializeDiscoverBuffer(
            PAVDTP_ENDPOINT_INFO pSEIDInfo, 
            DWORD cSEIDInfo, 
            PBYTE pbDisc, 
            DWORD cbPayload)
{
    DWORD cbEndpoint = 0;

    for (DWORD i = 0; i < cSEIDInfo; i++) {
        pbDisc[cbEndpoint] = (pSEIDInfo[i].bSEID << 2) | (pSEIDInfo[i].fInUse << 1);
        pbDisc[cbEndpoint + 1] = (pSEIDInfo[i].fMediaType << 4) | (pSEIDInfo[i].bTSEP << 3);

        cbEndpoint += 2;
    }

    SVSUTIL_ASSERT(cbEndpoint == cbPayload);

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    AVDTSignal::RecvServiceCapability
    
    This private method takes a buffer from the peer device as input and
    creates a capability array of structs based on the buffer.  This method
    is called by GetCapabilities and SetConfiguration since they share a lot
    of common logic.
    
    Parameters:
        pBuffer:                The incoming buffer
        pCapability:            Array of capabilities
        pcCapabilityIE:         The number of array elements
        fAllowAllCategories:    Are other categories invalid?
        pbFailedCategory:       Out pointer to failed service category
------------------------------------------------------------------------------*/
int AVDTSignal::RecvServiceCapability(
            BD_BUFFER* pBuffer,
            PAVDTP_CAPABILITY_IE pCapability,
            PDWORD pcCapabilityIE,
            BOOL fAllowAllCategories,
            PBYTE pbFailedCategory)
{
    BYTE bRet = AVDTP_ERROR_BAD_LENGTH;
    
    *pcCapabilityIE = 0;
    *pbFailedCategory = 0;
    
    // While we have data in the buffer
    while (BufferTotal(pBuffer) && *pcCapabilityIE < 2) {
        BYTE bCategory;
        BYTE bLen;

        // Get category and length
        if (! BufferGetByte(pBuffer, (unsigned char*)&bCategory)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Buffer is too small\n"));
            goto exit;
        }
        if (! BufferGetByte(pBuffer, (unsigned char*)&bLen)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Buffer is too small\n"));
            goto exit;
        }

        // Check for supported categories
        if ((bCategory == AVDTP_CAT_MEDIA_TRANSPORT) || 
            (bCategory == AVDTP_CAT_MEDIA_CODEC)) {

            SVSUTIL_ASSERT(*pcCapabilityIE < 2);

            memset(&pCapability[*pcCapabilityIE], 0, sizeof(pCapability[0]));
            
            pCapability[*pcCapabilityIE].dwSize = sizeof(pCapability[0]);
            pCapability[*pcCapabilityIE].bServiceCategory = bCategory;
            pCapability[*pcCapabilityIE].bServiceCategoryLen = bLen;

            if (bCategory == AVDTP_CAT_MEDIA_TRANSPORT) {
                if (bLen != 0) {                    
                    IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Media Transport LOSC should be 0.\n"));
                    bRet = AVDTP_ERROR_BAD_MEDIA_TRANSPORT_FORMAT;
                    *pbFailedCategory = AVDTP_CAT_MEDIA_TRANSPORT;
                    goto exit;
                }
            }
            else if (bCategory == AVDTP_CAT_MEDIA_CODEC) {
                BYTE bMediaType;
                BYTE bCodecType;
                
                if (! BufferGetByte(pBuffer, (unsigned char*)&bMediaType)) {
                    IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Buffer is too small\n"));
                    *pbFailedCategory = AVDTP_CAT_MEDIA_CODEC;
                    goto exit;
                }

                if (! BufferGetByte(pBuffer, (unsigned char*)&bCodecType)) {
                    IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Buffer is too small\n"));
                    *pbFailedCategory = AVDTP_CAT_MEDIA_CODEC;
                    goto exit;
                }
                
                pCapability[*pcCapabilityIE].MediaCodec.bMediaType = (bMediaType >> 4);
                pCapability[*pcCapabilityIE].MediaCodec.bMediaCodecType = bCodecType;

                DWORD cbCodecSpecific = bLen - 2;
                if ((cbCodecSpecific > 0) && 
                    (cbCodecSpecific <= sizeof(pCapability[*pcCapabilityIE].MediaCodec.CodecIEs))) {
                    if (! BufferGetChunk(pBuffer, cbCodecSpecific, (unsigned char*)pCapability[*pcCapabilityIE].MediaCodec.CodecIEs)) {
                        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::RecvServiceCapability : Buffer is too small\n"));
                        *pbFailedCategory = AVDTP_CAT_MEDIA_CODEC;
                        goto exit;
                    }
                }
            }

            (*pcCapabilityIE)++;
        } else {
            if (! fAllowAllCategories) {
                bRet = AVDTP_ERROR_BAD_SERV_CATEGORY;
                *pbFailedCategory = bCategory;
                goto exit;
            }
            
            // Advance pointer to next IE
            pBuffer->cStart += bLen;
        }
    }

    bRet = AVDTP_ERROR_SUCCESS;

exit:
    return bRet;
}


/*------------------------------------------------------------------------------
    AVDTSignal::DiscoverRequest
    
    This method is called sends a discover request to the specified peer.
    
    Parameters:
        pSession:   Session object
        lpvContext: Call context for async processing
        pba:        Address of peer
------------------------------------------------------------------------------*/
int AVDTSignal::DiscoverRequest(
            Session* pSession, 
            LPVOID lpvContext, 
            BD_ADDR* pba)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 0;    
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_DISCOVER,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    
    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::DiscoverResponse
    
    This method sends a discover response to the specified peer.  If bError is
    non-zero this is an error and pSEIDInfo should be null.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pSEIDInfo:      Endpoint info for local device
        cSEIDInfo:      Number of endpoints
        bError:         Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::DiscoverResponse(
            Session* pSession, 
            BYTE bTransaction, 
            PAVDTP_ENDPOINT_INFO pSEIDInfo, 
            DWORD cSEIDInfo, 
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BD_BUFFER* pBuffer = NULL;

    BYTE buffDisc[FIXED_BUFFER_SIZE];
    PBYTE pbDiscStart = buffDisc;

    if (bError) {
        // Response is a reject

        BYTE bPacketType = 0xff;
        DWORD cbPayload = 1;
        pBuffer = GetSignalBuffer(
                    pSession,
                    FALSE,
                    cbPayload, 
                    AVDTP_SIGNAL_DISCOVER, 
                    AVDTP_MESSAGE_RESP_REJ, 
                    &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bError;

        iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
        if (iRes != ERROR_SUCCESS) {
            goto exit;
        }        
    } else {
        DWORD cbPayload = cSEIDInfo * 2;

        if (cbPayload > FIXED_BUFFER_SIZE) {
            pbDiscStart = new BYTE[cbPayload];
            if (! pbDiscStart) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }
        }
        
        iRes = SerializeDiscoverBuffer(pSEIDInfo, cSEIDInfo, pbDiscStart, cbPayload);
        if (ERROR_SUCCESS != iRes) {
            goto exit;
        }

        PBYTE pbDisc = pbDiscStart;
        BOOL fContinue = FALSE;

        while (cbPayload > 0) {
            BYTE bPacketType = 0xff;
            
            // Call this helper to create the signal header
            pBuffer = GetSignalBuffer(
                pSession,
                fContinue,
                cbPayload, 
                AVDTP_SIGNAL_DISCOVER, 
                AVDTP_MESSAGE_RESP_ACC, 
                &bPacketType);

            if (! pBuffer) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }

            DWORD cbHeader = GetHeaderSize(bPacketType);

            // Move pointer to the start of the payload
            DWORD cbTotal = BufferTotal(pBuffer) - cbHeader;
            PBYTE pPayload = &pBuffer->pBuffer[pBuffer->cStart + cbHeader];

            memcpy(pPayload, pbDisc, cbTotal);

            iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
            if (iRes != ERROR_SUCCESS) {
                goto exit;
            }

            // Keep track of how much payload data is left to send
            cbPayload -= cbTotal;
            pbDisc += cbTotal;
            fContinue = TRUE;

#ifdef DEBUG
            if ((bPacketType == AVDTP_PACKET_SINGLE) || (bPacketType == AVDTP_PACKET_END)) {
                SVSUTIL_ASSERT(cbPayload == 0);
            }
#endif // DEBUG            
        }
    }

    SVSUTIL_ASSERT(iRes == ERROR_SUCCESS);
 
exit:
    if (pbDiscStart && (pbDiscStart != buffDisc)) {
        delete[] pbDiscStart;
    }
    
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetCapabilitiesRequest
    
    This method sends a GetCapabilities request to the specified peer.  
    
    Parameters:
        pSession:   Session object
        lpvContext: Call context for async processing
        bAcpSEID:   The SEID of the ACP
------------------------------------------------------------------------------*/
int AVDTSignal::GetCapabilitiesRequest(
            Session* pSession, 
            LPVOID lpvContext, 
            BYTE bAcpSEID)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 1;
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_GET_CAPABILITIES, 
        AVDTP_MESSAGE_COMMAND, 
        &bPacketType);

    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];
    pHeader[0] = bAcpSEID << 2;

    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;    
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetCapabilitiesResponse
    
    This method sends a GetCapabilities response to the specified peer.  If
    bError is non-zero an error will be sent.
    
    Parameters:
        pSession:           Session object 
        bTransaction:       Signaling transaction id
        pCapabilityIE:      Local peer capabilities 
        cCapablilityIEs:    Number capabilities
        bError:             Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::GetCapabilitiesResponse(
            Session* pSession, 
            BYTE bTransaction, 
            PAVDTP_CAPABILITY_IE pCapabilityIE, 
            DWORD cCapabilityIEs, 
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    BYTE buffCapability[FIXED_BUFFER_SIZE];
    PBYTE pbCapabilityStart = buffCapability;

    if (bError) {
        // Response is a reject

        DWORD cbPayload = 1;
        pBuffer = GetSignalBuffer(
                    pSession,
                    FALSE,
                    cbPayload, 
                    AVDTP_SIGNAL_GET_CAPABILITIES, 
                    AVDTP_MESSAGE_RESP_REJ, 
                    &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bError;

        iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
        if (iRes != ERROR_SUCCESS) {
            goto exit;
        } 
    } else {
        DWORD cbPayload = GetCapabilityBufferSize(pCapabilityIE, cCapabilityIEs);
        SVSUTIL_ASSERT(cbPayload);

        if (cbPayload > FIXED_BUFFER_SIZE) {
            pbCapabilityStart = new BYTE[cbPayload];
            if (! pbCapabilityStart) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }
        }
        
        iRes = SerializeCapabilityBuffer(pCapabilityIE, cCapabilityIEs, pbCapabilityStart, cbPayload);
        if (ERROR_SUCCESS != iRes) {
            goto exit;
        }

        PBYTE pbCapability = pbCapabilityStart;
        BOOL fContinue = FALSE;

        while (cbPayload > 0) {        
            pBuffer = GetSignalBuffer(
                        pSession,
                        fContinue, 
                        cbPayload, 
                        AVDTP_SIGNAL_GET_CAPABILITIES, 
                        AVDTP_MESSAGE_RESP_ACC, 
                        &bPacketType);

            if (! pBuffer) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }            

            DWORD cbHeader = GetHeaderSize(bPacketType);

            // Move pointer to the start of the payload
            DWORD cbTotal = BufferTotal(pBuffer) - cbHeader;
            PBYTE pPayload = &pBuffer->pBuffer[pBuffer->cStart + cbHeader];

            memcpy(pPayload, pbCapability, cbTotal);

            iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
            if (iRes != ERROR_SUCCESS) {
                goto exit;
            }

            // Keep track of how much payload data is left to send
            cbPayload -= cbTotal;
            pbCapability += cbTotal;
            fContinue = TRUE;

#ifdef DEBUG
            if ((bPacketType == AVDTP_PACKET_SINGLE) || (bPacketType == AVDTP_PACKET_END)) {
                SVSUTIL_ASSERT(cbPayload == 0);
            }
#endif // DEBUG
        }
    }

    SVSUTIL_ASSERT(iRes == ERROR_SUCCESS);

exit:
    if (pbCapabilityStart && (pbCapabilityStart != buffCapability)) {
        delete[] pbCapabilityStart;
    }
    
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::SetConfigurationRequest
    
    This method sends a SetConfiguration command to the specified peer.
    
    Parameters:
        pSession:       Sesssion object
        lpvContext:     Call context to async processing
        bAcpSEID:       SEID of ACP
        bIntSEID:       SEID of INT
        pCapabilityIE:  Configuration(s) to set
        cCapabilityIEs: Number of configurations
------------------------------------------------------------------------------*/
int AVDTSignal::SetConfigurationRequest(
            Session* pSession, 
            LPVOID lpvContext, 
            BYTE bAcpSEID,
            BYTE bIntSEID,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = GetCapabilityBufferSize(pCapabilityIE, cCapabilityIEs);
    SVSUTIL_ASSERT(cbPayload);

    BYTE buffConfig[FIXED_BUFFER_SIZE];
    PBYTE pbConfigStart = buffConfig;

    if (cbPayload > FIXED_BUFFER_SIZE) {
        pbConfigStart = new BYTE[cbPayload];
        if (! pbConfigStart) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }
    }

    iRes = SerializeCapabilityBuffer(pCapabilityIE, cCapabilityIEs, pbConfigStart, cbPayload);
    if (ERROR_SUCCESS != iRes) {
        goto exit;
    }

    PBYTE pbConfig = pbConfigStart;
    BD_BUFFER* pBuffer = NULL;
    BOOL fContinue = FALSE;
    
    while (cbPayload > 0) {
        BYTE bPacketType = 0xff;
        
        // Call this helper to create the signal header
        pBuffer = GetSignalBuffer(
            pSession,
            fContinue,
            cbPayload+2, 
            AVDTP_SIGNAL_SET_CONFIGURATION, 
            AVDTP_MESSAGE_COMMAND, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        DWORD cbHeader = GetHeaderSize(bPacketType);

        // Move pointer to the start of the payload
        DWORD cbTotal = BufferTotal(pBuffer) - cbHeader;
        PBYTE pPayload = &pBuffer->pBuffer[pBuffer->cStart + cbHeader];
        if (fContinue == FALSE) {
            // SEIDs are at the front of the first command
            pPayload[0] = bAcpSEID << 2;
            pPayload[1] = bIntSEID << 2;
            pPayload += 2;
            cbTotal -= 2;
        }

        memcpy(pPayload, pbConfig, cbTotal);

        iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
        if (iRes != ERROR_SUCCESS) {
            goto exit;
        }

        // Keep track of how much payload data is left to send
        cbPayload -= cbTotal;
        pbConfig += cbTotal;
        fContinue = TRUE;

#ifdef DEBUG
        if ((bPacketType == AVDTP_PACKET_SINGLE) || (bPacketType == AVDTP_PACKET_END)) {
            SVSUTIL_ASSERT(cbPayload == 0);
        }
#endif // DEBUG
    }
        
exit:
    if (pbConfigStart && (pbConfigStart != buffConfig)) {
        delete[] pbConfigStart;
    }
    
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;  
}

/*------------------------------------------------------------------------------
    AVDTSignal::SetConfigurationResponse
    
    This method sends a SetConfiguration response to the specified peer.  If
    bError is non-zero an error will be sent.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bServiceCategory:   Service category which caused error
        bError:             Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::SetConfigurationResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bServiceCategory,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;
    
    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        // Response is a reject

        pBuffer = GetSignalBuffer(
                    pSession,
                    FALSE,
                    2, 
                    AVDTP_SIGNAL_SET_CONFIGURATION, 
                    AVDTP_MESSAGE_RESP_REJ, 
                    &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bServiceCategory;
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE + 1] = bError;
    } else {
        // Call this helper to create the signal header
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_SET_CONFIGURATION, 
            AVDTP_MESSAGE_RESP_ACC, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetConfigurationRequest
    
    This method sends a GetConfiguration command to the specified peer.
    
    Parameters:
        pStream:    Stream object
        pSession:   Session object
        lpvContext: Call context for async processing
------------------------------------------------------------------------------*/
int AVDTSignal::GetConfigurationRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 1;
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_GET_CONFIGURATION, 
        AVDTP_MESSAGE_COMMAND, 
        &bPacketType);

    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];
    pHeader[0] = pStream->bRemoteSeid << 2;

    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes; 
}

/*------------------------------------------------------------------------------
    AVDTSignal::GetConfigurationResponse
    
    This method sends a GetConfiguration response to the specified peer.  If
    bError is non-zero an error will be sent.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pConfigIE:      Configuration(s) to send
        cConfig:        Number of configurations
        bError:         Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::GetConfigurationResponse(
            Session* pSession, 
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pConfigIE,
            DWORD cConfig,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    BYTE buffConfig[FIXED_BUFFER_SIZE];
    PBYTE pbConfigStart = buffConfig;

    if (bError) {
        // Response is a reject

        DWORD cbPayload = 1;
        pBuffer = GetSignalBuffer(
                    pSession,
                    FALSE,
                    cbPayload, 
                    AVDTP_SIGNAL_GET_CONFIGURATION, 
                    AVDTP_MESSAGE_RESP_REJ, 
                    &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bError;

        iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
        if (iRes != ERROR_SUCCESS) {
            goto exit;
        } 
    } else {
        DWORD cbPayload = GetCapabilityBufferSize(pConfigIE, cConfig);
        SVSUTIL_ASSERT(cbPayload);

        if (cbPayload > FIXED_BUFFER_SIZE) {
            pbConfigStart = new BYTE[cbPayload];
            if (! pbConfigStart) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }
        }
        
        iRes = SerializeCapabilityBuffer(pConfigIE, cConfig, pbConfigStart, cbPayload);
        if (ERROR_SUCCESS != iRes) {
            goto exit;
        }

        PBYTE pbConfig = pbConfigStart;
        BOOL fContinue = FALSE;

        while (cbPayload > 0) {        
            pBuffer = GetSignalBuffer(
                        pSession,
                        fContinue, 
                        cbPayload, 
                        AVDTP_SIGNAL_GET_CONFIGURATION, 
                        AVDTP_MESSAGE_RESP_ACC, 
                        &bPacketType);

            if (! pBuffer) {
                iRes = ERROR_OUTOFMEMORY;
                goto exit;
            }            

            DWORD cbHeader = GetHeaderSize(bPacketType);

            // Move pointer to the start of the payload
            DWORD cbTotal = BufferTotal(pBuffer) - cbHeader;
            PBYTE pPayload = &pBuffer->pBuffer[pBuffer->cStart + cbHeader];

            memcpy(pPayload, pbConfig, cbTotal);

            iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
            if (iRes != ERROR_SUCCESS) {
                goto exit;
            }

            // Keep track of how much payload data is left to send
            cbPayload -= cbTotal;
            pbConfig += cbTotal;
            fContinue = TRUE;

#ifdef DEBUG
            if ((bPacketType == AVDTP_PACKET_SINGLE) || (bPacketType == AVDTP_PACKET_END)) {
                SVSUTIL_ASSERT(cbPayload == 0);
            }
#endif // DEBUG
        }
    }

    SVSUTIL_ASSERT(iRes == ERROR_SUCCESS);

exit:
    if (pbConfigStart && (pbConfigStart != buffConfig)) {
        delete[] pbConfigStart;
    }
    
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OpenStreamRequest
    
    This method sends an Open request to the specified peer.
    
    Parameters:
        pStream:    Stream object
        pSession:   Session object
        lpvContext: Call context for async processing
------------------------------------------------------------------------------*/
int AVDTSignal::OpenStreamRequest(
            Stream* pStream, 
            Session* pSession,
            LPVOID lpvContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 1;    
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_OPEN,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];
    pHeader[0] = pStream->bRemoteSeid << 2;
    
    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OpenStreamResponse
    
    This method sends an Open response to the specified peer.  If bError is
    non-zero an error will be sent.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bError:         Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::OpenStreamResponse(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            1, 
            AVDTP_SIGNAL_OPEN, 
            AVDTP_MESSAGE_RESP_REJ, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bError;
    } else {
        // Call this helper to create the signal header    
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_OPEN,         
            AVDTP_MESSAGE_RESP_ACC,
            &bPacketType);
        
        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    // Send the signaling frame    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::CloseStreamRequest
    
    This method sends a Close request to the specified peer.
    
    Parameters:
        pStream:    Stream object
        pSession:   Session object
        lpvContext: Call context for async processing
------------------------------------------------------------------------------*/
int AVDTSignal::CloseStreamRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 1;    
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_CLOSE,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];
    pHeader[0] = pStream->bRemoteSeid << 2;
    
    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::CloseStreamResponse
    
    This method sends a Close response to the specified peer.  If bError is
    non-zero an error will be sent.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bError:         Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::CloseStreamResponse(
            Session* pSession,             
            BYTE bTransaction,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            1, 
            AVDTP_SIGNAL_CLOSE, 
            AVDTP_MESSAGE_RESP_REJ, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bError;
    } else {
        // Call this helper to create the signal header    
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_CLOSE,         
            AVDTP_MESSAGE_RESP_ACC,
            &bPacketType);
        
        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    // Send the signaling frame    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::StartStreamRequest
    
    This method sends a Start request to the specified peer.  A start request
    can specify multiple streams to be started in a single request.
    
    Parameters:
        pSession:       Session object
        lpvContext:     Call context for async processing
        ppStreams:      Pointer to an array of streams
        cStreamHandles: Number of streams
------------------------------------------------------------------------------*/
int AVDTSignal::StartStreamRequest(
            Session* pSession,
            LPVOID lpvContext,             
            Stream** ppStreams,
            DWORD cStreamHandles)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = cStreamHandles;
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_START,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // It is safe to assume this today since our API only supports one
    // stream handle at a time.
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];

    for (int i = 0; i < cStreamHandles; i++) {
        pHeader[i] = ppStreams[i]->bRemoteSeid << 2;
    }

    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, FALSE);

exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::StartStreamResponse
    
    This method sends a Start response to the specified peer.  If bError is
    non-zero an error is sent.
    
    Parameters:
        pSession: 
        bTransaction: 
        pFirstFailedStream: 
        bError: 
------------------------------------------------------------------------------*/
int AVDTSignal::StartStreamResponse(
            Session* pSession,
            BYTE bTransaction,
            Stream* pFirstFailedStream,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            2, 
            AVDTP_SIGNAL_START, 
            AVDTP_MESSAGE_RESP_REJ, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        BYTE bAcpSEID = pFirstFailedStream ? pFirstFailedStream->bLocalSeid << 2 : 0;

        // Fill out the error fields in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bAcpSEID;
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE + 1] = bError;        
    } else {
        // Call this helper to create the signal header    
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_START,         
            AVDTP_MESSAGE_RESP_ACC,
            &bPacketType);
        
        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    // Send the signaling frame    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, bError ? TRUE : FALSE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

int AVDTSignal::SuspendRequest(
            Session* pSession,
            LPVOID lpvContext,             
            Stream** ppStreams,
            DWORD cStreamHandles)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = cStreamHandles;
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_SUSPEND,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // It is safe to assume this today since our API only supports one
    // stream handle at a time.
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];

    for (int i = 0; i < cStreamHandles; i++) {
        pHeader[i] = ppStreams[i]->bRemoteSeid << 2;
    }

    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);

exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

int AVDTSignal::SuspendResponse(
            Session* pSession,
            BYTE bTransaction,
            Stream* pFirstFailedStream,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            2, 
            AVDTP_SIGNAL_SUSPEND, 
            AVDTP_MESSAGE_RESP_REJ, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        BYTE bAcpSEID = pFirstFailedStream ? pFirstFailedStream->bLocalSeid << 2 : 0;

        // Fill out the error fields in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bAcpSEID;
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE + 1] = bError;        
    } else {
        // Call this helper to create the signal header    
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_SUSPEND,         
            AVDTP_MESSAGE_RESP_ACC,
            &bPacketType);
        
        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    // Send the signaling frame    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::ReconfigureRequest
    
    This method sends a Reconfigure request to the specified peer.
    
    Parameters:
        pStream:        Stream object
        pSession:       Session object
        lpvContext:     Call context for async processing
        pCapabilityIE:  Capabilitie(s) to reconfigure
        cCapabilityIEs: Number of capabilities
------------------------------------------------------------------------------*/
int AVDTSignal::ReconfigureRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvContext, 
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = GetCapabilityBufferSize(pCapabilityIE, cCapabilityIEs);
    SVSUTIL_ASSERT(cbPayload);

    BYTE buffConfig[FIXED_BUFFER_SIZE];
    PBYTE pbConfigStart = buffConfig;

    if (cbPayload > FIXED_BUFFER_SIZE) {
        pbConfigStart = new BYTE[cbPayload];
        if (! pbConfigStart) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }
    }

    iRes = SerializeCapabilityBuffer(pCapabilityIE, cCapabilityIEs, pbConfigStart, cbPayload);
    if (ERROR_SUCCESS != iRes) {
        goto exit;
    }

    PBYTE pbConfig = pbConfigStart;
    BD_BUFFER* pBuffer = NULL;
    BOOL fContinue = FALSE;
    
    while (cbPayload > 0) {
        BYTE bPacketType = 0xff;
        
        // Call this helper to create the signal header
        pBuffer = GetSignalBuffer(
            pSession,
            fContinue,
            cbPayload+1, 
            AVDTP_SIGNAL_RECONFIGURE, 
            AVDTP_MESSAGE_COMMAND, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        DWORD cbHeader = GetHeaderSize(bPacketType);

        // Move pointer to the start of the payload
        DWORD cbTotal = BufferTotal(pBuffer) - cbHeader;
        PBYTE pPayload = &pBuffer->pBuffer[pBuffer->cStart + cbHeader];
        if (fContinue == FALSE) {
            // SEID is at the front of the first command
            pPayload[0] = pStream->bRemoteSeid << 2;
            pPayload += 1;
            cbTotal -= 1;
        }

        memcpy(pPayload, pbConfig, cbTotal);

        iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, TRUE, TRUE);
        if (iRes != ERROR_SUCCESS) {
            goto exit;
        }

        // Keep track of how much payload data is left to send
        cbPayload -= cbTotal;
        pbConfig += cbTotal;
        fContinue = TRUE;

#ifdef DEBUG
        if ((bPacketType == AVDTP_PACKET_SINGLE) || (bPacketType == AVDTP_PACKET_END)) {
            SVSUTIL_ASSERT(cbPayload == 0);
        }
#endif // DEBUG
    }
        
exit:
    if (pbConfigStart && (pbConfigStart != buffConfig)) {
        delete[] pbConfigStart;
    }
    
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::ReconfigureResponse
    
    This method sends a Reconfigure response to the specified peer.  If bError
    is non-zero an error is sent.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction
        bServiceCategory:   Service category which caused the error
        bError:             Error to send
------------------------------------------------------------------------------*/
int AVDTSignal::ReconfigureResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bServiceCategory,            
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;
    
    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    if (bError) {
        // Response is a reject

        pBuffer = GetSignalBuffer(
                    pSession,
                    FALSE,
                    2, 
                    AVDTP_SIGNAL_RECONFIGURE, 
                    AVDTP_MESSAGE_RESP_REJ, 
                    &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

        // Fill out the error field in the reject response
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = bServiceCategory;
        pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE + 1] = bError;
    } else {
        // Call this helper to create the signal header
        pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_RECONFIGURE, 
            AVDTP_MESSAGE_RESP_ACC, 
            &bPacketType);

        if (! pBuffer) {
            iRes = ERROR_OUTOFMEMORY;
            goto exit;
        }

        // This frame is too small to be anything but SINGLE
        SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
    }
    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }
    
    return iRes;
}

int AVDTSignal::SecurityControlRequest(
            Session* pSession, 
            LPVOID lpvCallContext, 
            PBYTE pSecurityControlData,
            USHORT cbSecurityControlData)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    return iRes;
}

int AVDTSignal::SecurityControlResponse(
            Session* pSession, 
            BYTE bTransaction,
            PBYTE pSecurityControlData,
            USHORT cbSecurityControlData,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::AbortRequest
    
    This method sends an Abort command to the specified peer.
    
    Parameters:
        pStream:    Stream object
        pSession:   Session object
        lpvContext: Call context for async processing
------------------------------------------------------------------------------*/
int AVDTSignal::AbortRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    DWORD cbPayload = 1;    
    BYTE bPacketType = 0xff;

    // Call this helper to create the signal header    
    BD_BUFFER* pBuffer = GetSignalBuffer(
        pSession,
        FALSE,
        cbPayload, 
        AVDTP_SIGNAL_ABORT,         
        AVDTP_MESSAGE_COMMAND,
        &bPacketType);
    
    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

    PBYTE pHeader = &pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE];
    pHeader[0] = pStream->bRemoteSeid << 2;
    
    // Send the signaling frame    
    iRes = L2CAPSend(lpvContext, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::AbortResponse
    
    This method sends an Abort response to the specified peer.  If bError is
    non-zero an error is sent.  In this case, the Abort packet does not send
    an error but can mark the packet as a REJECT.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bError:         Non-zero indicates this should be a reject
------------------------------------------------------------------------------*/
int AVDTSignal::AbortResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // Need to set the transaction when acting as ACP
    pSession->bTransaction = bTransaction;

    BYTE bPacketType = 0xff;
    BD_BUFFER* pBuffer = NULL;

    pBuffer = GetSignalBuffer(
            pSession,
            FALSE,
            0, 
            AVDTP_SIGNAL_ABORT, 
            (bError ? AVDTP_MESSAGE_RESP_REJ : AVDTP_MESSAGE_RESP_ACC), 
            &bPacketType);

    if (! pBuffer) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // This frame is too small to be anything but SINGLE
    SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);
   
    // Send the signaling frame    
    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, pSession, FALSE, TRUE);
    
exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer) {
        // Send failed, so make sure we don't leak the buffer
        BufferFree(pBuffer);
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::ProcessPacket
    
    This method is called when an incoming packet is received on the specified
    session.  This method will parse the packet and figure out what API to call
    to handle the command.
    
    Parameters:
        pSession:   Session object
        pBuffer:    Buffer that we received
------------------------------------------------------------------------------*/
int AVDTSignal::ProcessPacket(
            Session* pSession, 
            BD_BUFFER* pBuffer)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    unsigned char SignalHeader;
    if (! BufferGetByte(pBuffer, &SignalHeader)) {
        iRes = ERROR_INVALID_DATA;
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
        goto exit;
    }

    // Get the first part of the signaling header
    BYTE bTransaction = SignalHeader >> 4;
    BYTE bPacketType = (SignalHeader >> 2) & 0x3;
    BYTE bMsgType = SignalHeader & 0x03;

    BD_BUFFER buffFrags;
    BD_BUFFER* pB = pBuffer;

    switch (bPacketType) {
    case AVDTP_PACKET_START:
    {
        //
        // We will always copy START packet to a temp buffer stored in the session object.
        //
        
        if (pSession->cRemainingFrags > 0) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Received start packet but still expecting %d fragments\n", pSession->cRemainingFrags));
            goto exit;
        }
        
        unsigned char nosp;
        if (! BufferGetByte(pB, &nosp)) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
            goto exit;
        }

        pSession->cRemainingFrags = nosp;
        
        if (! CopyFragment(pSession, pB)) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too large, can't copy it\n"));
            goto exit;
        }

        return ERROR_SUCCESS;
    }
    case AVDTP_PACKET_CONTINUE:
    {
        //
        // We will always copy START packet to a temp buffer stored in the session object.
        //
        
        if (pSession->cRemainingFrags == 0) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Received too many continue packets\n", pSession->cRemainingFrags));
            goto exit;
        }

        if (! CopyFragment(pSession, pB)) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too large, can't copy it\n"));
            goto exit;
        }
        
        return ERROR_SUCCESS;
    }
    case AVDTP_PACKET_END:
    {
        //
        // Got an END packet.  Copy the data to the temp buffer, then create a BD_BUFFER that
        // wraps this packet.  Then go ahead and handle this packet.
        //
        
        if (pSession->cRemainingFrags != 1) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Received end packet but still expecting %d fragments\n", pSession->cRemainingFrags));
            goto exit;
        }

        if (! CopyFragment(pSession, pB)) {
            iRes = ERROR_INVALID_DATA;
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too large, can't copy it\n"));
            goto exit;
        }

        buffFrags.cSize = RECV_BUFFER_SIZE;
        buffFrags.cEnd = pSession->cbRecvBuffer;
        buffFrags.cStart = 0;
        buffFrags.fMustCopy = TRUE;
        buffFrags.pFree = NULL;
        buffFrags.pBuffer = pSession->pRecvBuffer;

        pB = &buffFrags;

        // Reset necessary fragmentation info in session
        pSession->cbRecvBuffer = 0;
        pSession->pRecvBuffer = NULL;
        SVSUTIL_ASSERT(pSession->cRemainingFrags == 0);

        break;
    }
    case AVDTP_PACKET_SINGLE:
    {
        //
        // Got a SINGLE packet.
        //
                
        break;
    }
    
    default:
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Incorrect packet type\n"));
        iRes = ERROR_INVALID_DATA;
        goto exit;
    }

    // Get the signal id
    BYTE bSignalId = 0;
    if (! BufferGetByte(pB, &bSignalId)) {
        iRes = ERROR_INVALID_DATA;
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
        goto exit;
    }        
    bSignalId = (bSignalId & 0x3f);

    // Handle the packet based on signal id
    switch (bSignalId) {
    case AVDTP_SIGNAL_DISCOVER:
    {
        iRes = OnDiscover(pSession, pB, bMsgType, bTransaction);        
        break;
    }
    case AVDTP_SIGNAL_GET_CAPABILITIES:
    {
        iRes = OnGetCapabilities(pSession, pB, bMsgType, bTransaction);
        break;        
    }
    case AVDTP_SIGNAL_SET_CONFIGURATION:
    {
        iRes = OnSetConfiguration(pSession, pB, bMsgType, bTransaction);
        break;        
    }
    case AVDTP_SIGNAL_GET_CONFIGURATION:
    {
        iRes = OnGetConfiguration(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_RECONFIGURE:
    {
        iRes = OnReconfigure(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_OPEN:
    {
        iRes = OnOpenStream(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_START:
    {
        iRes = OnStartStream(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_CLOSE:
    {
        iRes = OnCloseStream(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_SUSPEND:
    {
        iRes = OnSuspendStream(pSession, pB, bMsgType, bTransaction);
        break;
    }
    case AVDTP_SIGNAL_ABORT:
    {
        iRes = OnAbort(pSession, pB, bMsgType, bTransaction);
        break;
    }    
    case AVDTP_SIGNAL_SECURITY_CONTROL:
    {
        iRes = OnSecurityControl(pSession, pB, bMsgType, bTransaction);
        break;
    }
    
    default:
        // Invalid Signal Id, is the packet corrupted?
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Invalid signaling Id\n"));
        iRes = OnUnknownSignalId(pSession, pB, bMsgType, bTransaction);
    }

exit:
    if (pBuffer->pFree) {
        pBuffer->pFree(pBuffer);
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnDiscover
    
    This method is called when a Discover command or response is received. 
    This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnDiscover(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnDiscover: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;

    BYTE bSendError = 0x00;

    AVDTP_ENDPOINT_INFO AcpEPInfo[FIXED_DISCOVER_EP_ARRAY_SIZE];
    PAVDTP_ENDPOINT_INFO pAcpEPInfo = AcpEPInfo;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bError = 0;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
            goto exit;
        }
        iRes = DiscoverResponseUp(pSession, bTransaction, NULL, 0, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        DWORD cbRemaining = BufferTotal(pBuffer);

        // This response must have a list of ACP SEID info (2 bytes each)
        if ((cbRemaining % AVDTP_DISCOVER_EP_SIZE) != 0) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Discover response is malformed.\n"));
            goto exit;
        }

        DWORD cIterations = cbRemaining / AVDTP_DISCOVER_EP_SIZE;

        // If we have several endpoints in the discover response
        // we need to allocate memory for storage.
        if (cIterations > FIXED_DISCOVER_EP_ARRAY_SIZE) {                
            pAcpEPInfo = new AVDTP_ENDPOINT_INFO[cIterations];
            if (! pAcpEPInfo) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Out of memory.\n"));
                goto exit;
            }
        }

        for (DWORD i = 0; i < cIterations; i++) {
            BYTE DiscRespBuff[AVDTP_DISCOVER_EP_SIZE];
            if (! BufferGetChunk(pBuffer, AVDTP_DISCOVER_EP_SIZE, (unsigned char*)DiscRespBuff)) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
                goto exit;
            }

            memset(&pAcpEPInfo[i], 0, sizeof(AVDTP_ENDPOINT_INFO));

            pAcpEPInfo[i].dwSize = sizeof(AVDTP_ENDPOINT_INFO);
            pAcpEPInfo[i].bSEID = DiscRespBuff[0] >> 2;
            pAcpEPInfo[i].fInUse = (DiscRespBuff[0] >> 1) & 0x01;
            pAcpEPInfo[i].fMediaType = DiscRespBuff[1] >> 4;
            pAcpEPInfo[i].bTSEP = (DiscRespBuff[1] >> 3) & 0x01;
        }

        bSendError = DiscoverResponseUp(pSession, bTransaction, pAcpEPInfo, cIterations, 0);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        iRes = DiscoverRequestUp(pSession, bTransaction);
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }
    
exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR; // unknown error, return failure
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = DiscoverResponse(pSession, bTransaction, NULL, 0, bSendError);    
    }
    
    if (pAcpEPInfo != AcpEPInfo) {
        delete[] pAcpEPInfo;
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnGetCapabilities
    
    This method is called when a GetCapabilities command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnGetCapabilities(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnGetCapabilities: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bError = 0;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetCapabilities : Buffer is too small\n"));
            goto exit;
        }
        iRes = GetCapabilitiesResponseUp(pSession, bTransaction, NULL, 0, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        AVDTP_CAPABILITY_IE Capability[2]; // we only support media codec & media transport today
        DWORD cCapabilityIE = 0;
        BYTE bFailedCategory = 0;

        BYTE bError = RecvServiceCapability(pBuffer, Capability, &cCapabilityIE, TRUE, &bFailedCategory);
        if (AVDTP_ERROR_SUCCESS != bError) {
            iRes = ERROR_INVALID_DATA;
            goto exit;
        }

        iRes = GetCapabilitiesResponseUp(pSession, bTransaction, Capability, cCapabilityIE, 0);   
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetCapabilities : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }

        bAcpSEID = bAcpSEID >> 2;
        
        bSendError = GetCapabilitiesRequestUp(pSession, bTransaction, bAcpSEID);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR; // unknown error, return failure
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = GetCapabilitiesResponse(pSession, bTransaction, NULL, 0, bSendError);    
    } 
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnSetConfiguration
    
    This method is called when a SetConfiguration command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnSetConfiguration(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnSetConfiguration: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;
    BYTE bFailedCategory = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bServiceCategory = 0;
        BYTE bError = 0;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bServiceCategory) ||
            ! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::ProcessPacket : Buffer is too small\n"));
            goto exit;
        }

        iRes = SetConfigurationResponseUp(pSession, bTransaction, bServiceCategory, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = SetConfigurationResponseUp(pSession, bTransaction, 0, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID, bIntSEID;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetCapabilities : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }
        if (! BufferGetByte(pBuffer, (unsigned char*)&bIntSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetCapabilities : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }

        bAcpSEID = bAcpSEID >> 2;
        bIntSEID = bIntSEID >> 2;

        AVDTP_CAPABILITY_IE Config[2]; // we only support media codec & media transport today
        DWORD cConfig = 0;        

        bSendError = RecvServiceCapability(pBuffer, Config, &cConfig, FALSE, &bFailedCategory);
        if (AVDTP_ERROR_SUCCESS != bSendError) {
            goto exit;
        }

        bSendError = SetConfigurationRequestUp(pSession, bTransaction, bAcpSEID, bIntSEID, Config, cConfig, &bFailedCategory);  
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = SetConfigurationResponse(pSession, bTransaction, bFailedCategory, bSendError);    
    } 
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnGetConfiguration
    
    This method is called when a GetConfiguration command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnGetConfiguration(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnGetConfiguration: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bError = 0;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetConfiguration : Buffer is too small\n"));
            goto exit;
        }
        iRes = GetConfigurationResponseUp(pSession, bTransaction, NULL, 0, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        AVDTP_CAPABILITY_IE Config[2]; // we only support media codec & media transport today
        DWORD cConfig = 0;
        BYTE bFailedCategory = 0;

        iRes = RecvServiceCapability(pBuffer, Config, &cConfig, TRUE, &bFailedCategory);
        if (ERROR_SUCCESS != iRes) {
            goto exit;
        }

        iRes = GetConfigurationResponseUp(pSession, bTransaction, Config, cConfig, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnGetConfiguration : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }

        bAcpSEID = bAcpSEID >> 2;
        
        bSendError = GetConfigurationRequestUp(pSession, bTransaction, bAcpSEID);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = GetConfigurationResponse(pSession, bTransaction, NULL, 0, bSendError);    
    } 
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnOpenStream
    
    This method is called when an Open command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnOpenStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnOpenStream: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
        
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bError = 0;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnOpenStream : Buffer is too small\n"));
            goto exit;
        }
        iRes = OpenResponseUp(pSession, bTransaction, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = OpenResponseUp(pSession, bTransaction, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnOpenStream : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }
        bAcpSEID = bAcpSEID >> 2;        
        bSendError = OpenRequestUp(pSession, bTransaction, bAcpSEID);
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = OpenStreamResponse(pSession, bTransaction, bSendError);    
    } 
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnCloseStream
    
    This method is called when a Close command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnCloseStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OnCloseStream: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bError = 0;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnOpenStream : Buffer is too small\n"));
            goto exit;
        }
        iRes = CloseResponseUp(pSession, bTransaction, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = CloseResponseUp(pSession, bTransaction, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnOpenStream : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }
        bAcpSEID = bAcpSEID >> 2;        
        bSendError = CloseRequestUp(pSession, bTransaction, bAcpSEID);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = CloseStreamResponse(pSession, bTransaction, bSendError);    
    }
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnStartStream
    
    This method is called when a Start command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnStartStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AVDTSignal::OnStartStream: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    BYTE SeidBuffer[FIXED_BUFFER_SIZE];
    PBYTE pSeidList = SeidBuffer;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bFailedSEID;
        BYTE bError = 0;         
        if (! BufferGetByte(pBuffer, (unsigned char*)&bFailedSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Buffer is too small\n"));
            goto exit;
        }
        bFailedSEID = bFailedSEID >> 2;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Buffer is too small\n"));
            goto exit;
        }
        iRes = StartResponseUp(pSession, bTransaction, bFailedSEID, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = StartResponseUp(pSession, bTransaction, 0, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        DWORD cbRemaining = BufferTotal(pBuffer);

        if (cbRemaining > FIXED_BUFFER_SIZE) {
            pSeidList = new BYTE[cbRemaining];
            if (! pSeidList) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Out of memory.\n"));                
                goto exit;
            }
        }

        for (int i = 0; i < cbRemaining; i++) {
            if (! BufferGetByte(pBuffer, (unsigned char*)&pSeidList[i])) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Buffer is too small\n"));
                bSendError = AVDTP_ERROR_BAD_LENGTH;
                goto exit;
            }
            pSeidList[i] = pSeidList[i] >> 2;
        }

        bSendError = StartRequestUp(pSession, bTransaction, pSeidList, cbRemaining);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = StartStreamResponse(pSession, bTransaction, 0, bSendError);    
    }
    
    if (pSeidList != SeidBuffer) {
        delete[] pSeidList;
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnSuspendStream
    
    This method is called when a Suspend command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnSuspendStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AVDTSignal::OnSuspendStream: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;

    BYTE SeidBuffer[FIXED_BUFFER_SIZE];
    PBYTE pSeidList = SeidBuffer;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bFailedSEID;
        BYTE bError = 0;         
        if (! BufferGetByte(pBuffer, (unsigned char*)&bFailedSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnSuspendStream : Buffer is too small\n"));
            goto exit;
        }
        bFailedSEID = bFailedSEID >> 2;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Buffer is too small\n"));
            goto exit;
        }
        iRes = SuspendResponseUp(pSession, bTransaction, bFailedSEID, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = SuspendResponseUp(pSession, bTransaction, 0, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        DWORD cbRemaining = BufferTotal(pBuffer);

        if (cbRemaining > FIXED_BUFFER_SIZE) {
            pSeidList = new BYTE[cbRemaining];
            if (! pSeidList) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Out of memory.\n"));                
                goto exit;
            }
        }

        for (int i = 0; i < cbRemaining; i++) {
            if (! BufferGetByte(pBuffer, (unsigned char*)&pSeidList[i])) {
                IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnStartStream : Buffer is too small\n"));
                bSendError = AVDTP_ERROR_BAD_LENGTH;
                goto exit;
            }
            pSeidList[i] = pSeidList[i] >> 2;
        }

        bSendError = SuspendRequestUp(pSession, bTransaction, pSeidList, cbRemaining);
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = SuspendResponse(pSession, bTransaction, 0, bSendError);    
    }
    
    if (pSeidList != SeidBuffer) {
        delete[] pSeidList;
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnReconfigure
    
    This method is called when a Reconfigure command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnReconfigure(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AVDTSignal::OnReconfigure: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;
    BYTE bSendError = 0x00;
    BYTE bFailedCategory = 0x00;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        BYTE bServiceCategory = 0;
        BYTE bError = 0;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bServiceCategory) ||
            ! BufferGetByte(pBuffer, (unsigned char*)&bError)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnReconfigure : Buffer is too small\n"));
            goto exit;
        }

        iRes = ReconfigureResponseUp(pSession, bTransaction, bServiceCategory, bError);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = ReconfigureResponseUp(pSession, bTransaction, 0, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnReconfigure : Buffer is too small\n"));
            bSendError = AVDTP_ERROR_BAD_LENGTH;
            goto exit;
        }

        bAcpSEID = bAcpSEID >> 2;

        AVDTP_CAPABILITY_IE Config[2]; // we only support media codec & media transport today
        DWORD cConfig = 0;

        bSendError = RecvServiceCapability(pBuffer, Config, &cConfig, FALSE, &bFailedCategory);
        if (AVDTP_ERROR_SUCCESS != bSendError) {
            goto exit;
        }

        bSendError = ReconfigureRequestUp(pSession, bTransaction, bAcpSEID, Config, cConfig, &bFailedCategory);  
        
        iRes = ERROR_SUCCESS;
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        bSendError = AVDTP_ERROR_BAD_HEADER_FORMAT;
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    if (bSendError == AVDTP_ERROR_UNDEFINED) {
        iRes = ERROR_INTERNAL_ERROR;
    } else if (bSendError) {
        // We are handling the error so it is ok to overwrite iRes
        iRes = ReconfigureResponse(pSession, bTransaction, bFailedCategory, bSendError);    
    } 
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnSecurityControl
    
    This method is called when a SecurityControl command or response is
    received.  This command is not supported right now so we are responding
    to this command with ERROR_NOT_SUPPORTED.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnSecurityControl(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    int iRes = ERROR_SUCCESS;

    // We don't support this.  Ignore response messages.
    if (bMsgType == AVDTP_MESSAGE_COMMAND) {
        BYTE bPacketType = 0xff;
        BD_BUFFER* pResponse = GetSignalBuffer(
                pSession,
                FALSE,
                1, 
                AVDTP_SIGNAL_SECURITY_CONTROL, 
                AVDTP_MESSAGE_RESP_REJ,
                &bPacketType);
        if (! pResponse) {
            iRes = ERROR_OUTOFMEMORY;
        } else {
            // This frame is too small to be anything but SINGLE
            SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

            // Fill out the error field in the reject response
            pBuffer->pBuffer[pBuffer->cStart + AVDTP_SIGNAL_HEADER_SINGLE_SIZE] = AVDTP_ERROR_NOT_SUPPORTED;

            iRes = L2CAPSend(NULL, pSession->cid, pResponse, pSession, FALSE, TRUE);
        }
    }

    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnAbort
    
    This method is called when an Abort command or response is
    received.  This method will call the proper handler.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnAbort(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AVDTSignal::OnAbort: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));
    
    int iRes = ERROR_INSUFFICIENT_BUFFER;

    switch (bMsgType) {
    case AVDTP_MESSAGE_RESP_REJ:
    {
        iRes = AbortResponseUp(pSession, bTransaction, 1);
        break;
    }
    case AVDTP_MESSAGE_RESP_ACC:
    {
        iRes = AbortResponseUp(pSession, bTransaction, 0);
        break;
    }
    case AVDTP_MESSAGE_COMMAND:
    {
        BYTE bAcpSEID;
        if (! BufferGetByte(pBuffer, (unsigned char*)&bAcpSEID)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTSignal::OnAbort : Buffer is too small\n"));
            goto exit;
        }
        bAcpSEID = bAcpSEID >> 2;
        BYTE bRet = AbortRequestUp(pSession, bTransaction, bAcpSEID);
        if ((bRet == ERROR_SUCCESS) || (bRet == AVDTP_ERROR_BAD_SEID)) {
            // On success we return success.  On bad seid we do the same             
            // since per spec we ignore the request.
            iRes = ERROR_SUCCESS; 
        } else {
            iRes = ERROR_INTERNAL_ERROR;
        }
        break;
    }
    case AVDTP_MESSAGE_RESERVED:
    {
        // Ignore bad abort request
        iRes = ERROR_SUCCESS;
        break;
    }

    default:
        SVSUTIL_ASSERT(0);
        iRes = ERROR_INTERNAL_ERROR;
    }

exit:
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTSignal::OnUnknownSignalId
    
    This method is called when an unknown signaling id is received.  This 
    method will respond with a general reject if this is a command message.
    
    Parameters:
        pSession:       Session object
        pBuffer:        Recv buffer
        bMsgType:       Message type
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
int AVDTSignal::OnUnknownSignalId(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AVDTSignal::OnUnknownSignalId: ba:%04x%08x transaction:%d type:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bMsgType));

    int iRes = ERROR_SUCCESS;

    // Send a general reject when we get an unknown command.  Just ignore responses.
    if (bMsgType == AVDTP_MESSAGE_COMMAND) {
        
        // Need to set the transaction when acting as ACP
        pSession->bTransaction = bTransaction;
        
        BYTE bPacketType = 0xff;
        BD_BUFFER* pResponse = GetSignalBuffer(
                pSession,
                FALSE,
                0, 
                0x00, // RFA 
                AVDTP_MESSAGE_RESP_REJ, 
                &bPacketType);
        if (! pResponse) {
            iRes = ERROR_OUTOFMEMORY;
        } else {
            // This frame is too small to be anything but SINGLE
            SVSUTIL_ASSERT(bPacketType == AVDTP_PACKET_SINGLE);

            iRes = L2CAPSend(NULL, pSession->cid, pResponse, pSession, FALSE, TRUE);
        }
    }

    return iRes;
}

