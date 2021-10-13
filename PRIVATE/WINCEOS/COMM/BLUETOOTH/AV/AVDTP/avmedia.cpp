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
////////////////////////////////////////////////////////////////////////////////
// avmedia.cpp
//
// This file implements the AVDTMedia class.  This class parses AVDTP media
// packets and implements flow control for media packets.
//
////////////////////////////////////////////////////////////////////////////////


#include "avdtpriv.h"

#include "cenet.h"

// Default low/high thresholds.  By design high=low*2.
// The larger these values, the larger the latency.
#define AVDTP_MEDIA_LOW_THRESHOLD_MS    40
#define AVDTP_MEDIA_HIGH_THRESHOLD_MS   80

#define AVDTP_MEDIA_THRESHOLD_MAX       10000 

#define AVDTP_FLOW_CONTROL_DEBUG    1

#pragma pack(push, 1)
typedef struct _RTPHdr_t {
    BYTE firstByte;
    BYTE payloadType;
    WORD seq;
    DWORD time;
    DWORD ssrc;
} RTPHdr_t;
#pragma pack(pop)

// We are linked into btd.  Cheat a little and call right into these functions.
int BthGetBasebandConnections(int c, BASEBAND_CONNECTION_DATA* p, int *pc);
int BthGetQueuedHCIPacketCount(BT_ADDR* bta, int* pPacketCount);

AVDTMedia::AVDTMedia(void)
{
    m_dwLowThresholdMs = AVDTP_MEDIA_LOW_THRESHOLD_MS;
    m_dwHighThresholdMs = AVDTP_MEDIA_HIGH_THRESHOLD_MS;
}

int AVDTMedia::Init(DWORD dwLowThreshold, DWORD dwHighThreshold)
{
    if (dwLowThreshold > AVDTP_MEDIA_THRESHOLD_MAX) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::Init : Low Threshold is too large %d\n", dwLowThreshold));
        dwLowThreshold = 0;
    } 
    if (dwHighThreshold > AVDTP_MEDIA_THRESHOLD_MAX) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::Init : High Threshold is too large %d\n", dwHighThreshold));
        dwHighThreshold = 0;
    }
    
    if (dwLowThreshold) {
        m_dwLowThresholdMs = dwLowThreshold;
    }

    if (dwHighThreshold) {
        m_dwHighThresholdMs = dwHighThreshold;
    }

    if (m_dwLowThresholdMs >= m_dwHighThresholdMs) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::Init : Low is greater than high threshold low:%d high:%d\n", dwLowThreshold, dwHighThreshold));
        m_dwLowThresholdMs = AVDTP_MEDIA_LOW_THRESHOLD_MS;
        m_dwHighThresholdMs = AVDTP_MEDIA_HIGH_THRESHOLD_MS;
    }
    
    return ERROR_SUCCESS;    
}

void AVDTMedia::Deinit(void)
{    
}

/*------------------------------------------------------------------------------
    AVDTMedia::PauseStream

    This method will sleep for the specified amount of time.
    
    Parameters:
        hStream:        Handle to the stream object
        hSession:       Handle to the media session
        dwMs:           Milliseconds to sleep for
------------------------------------------------------------------------------*/
DWORD AVDTMedia::PauseStream(HANDLE hStream, HANDLE hSession, DWORD dwMs)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    avdtp_addref();
    avdtp_unlock();
    Sleep(dwMs);
    avdtp_lock();
    avdtp_delref();

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::PauseStream : VerifyStream failed with ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    if (! VerifySession(pStream->hSessMedia)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::PauseStream : VerifySession failed with ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    return ERROR_SUCCESS;    
}

/*------------------------------------------------------------------------------
    AVDTMedia::WriteRequest
    
    This method writes an AVDTP media packet specified by the following
    parameters.  On top of writing the packets down to the L2CAP layer, this
    function makes sure the packets are written at the bit rate specified in
    the stream.
    
    Parameters:
        pStream:        Stream object
        pSession:       Session object
        pBuffer:        Buffer to write
        dwTime:         TimeInfo param from spec
        bPayloadType:   Payload type from spec
        usMarker:       Marker bit from spec
------------------------------------------------------------------------------*/
int AVDTMedia::WriteRequest(
            Stream* pStream,
            Session* pSession,
            BD_BUFFER* pBuffer,
            DWORD dwTime,
            BYTE bPayloadType,
            USHORT usMarker)
{
    int iRes = ERROR_SUCCESS;
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    DWORD cbTotalMedia = BufferTotal(pBuffer) - pStream->cbCodecHeader;
    
    if (pStream->dwBitsPerSec) {
        // Do flow control

        DWORD Tcurr = dwTime;

        if (! pStream->dwLowThresholdBytes) {
            // Calculate the number of bytes for low/high thresholds based 
            // on bit rate and low/high threshold latency
            pStream->dwLowThresholdBytes = (pStream->dwBitsPerSec * m_dwLowThresholdMs) / 8000;
            pStream->dwHighThresholdBytes = (pStream->dwBitsPerSec * m_dwHighThresholdMs) / 8000;
            
            pStream->dwNextSlotStartTime = Tcurr + (m_dwHighThresholdMs - m_dwLowThresholdMs);
            pStream->dwPendingBytes = 0;

            IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"AVDTMedia::WriteRequest : Rate=%d Low=%d High=%d cbTotalMedia=%d\n", 
                pStream->dwBitsPerSec, pStream->dwLowThresholdBytes, pStream->dwHighThresholdBytes, cbTotalMedia));

            RETAILMSG(AVDTP_FLOW_CONTROL_DEBUG, (L"[AVDTP] Rate=%d Low=%d High=%d cbTotalMedia=%d\n", 
                pStream->dwBitsPerSec, pStream->dwLowThresholdBytes, pStream->dwHighThresholdBytes, cbTotalMedia));
        }

        if ((int)(Tcurr - pStream->dwNextSlotStartTime) > (int)m_dwHighThresholdMs) {
            // We are way out of sync.  Reset everything and schedule next sync
            // to be difference of high/low thresholds.  But in that time send 
            // data amount that corresponds to the high threshold.

            RETAILMSG(AVDTP_FLOW_CONTROL_DEBUG, (L"[AVDTP] +oos -- t:%d pend:%d\n", (int)(Tcurr - pStream->dwNextSlotStartTime), pStream->dwPendingBytes));
            
            pStream->dwNextSlotStartTime = Tcurr + (m_dwHighThresholdMs - m_dwLowThresholdMs);
            pStream->dwPendingBytes = 0;
        } else if (pStream->dwPendingBytes >= pStream->dwHighThresholdBytes) {
            // We have sent enough data.  Now check how long we need to sleep to
            // get back to the low threshold.
            int msToNextSlot = (int) (pStream->dwNextSlotStartTime - Tcurr);
            if (msToNextSlot > 0) {
                //DEBUGMSG(1, (L"[AVDTP] +slp -- %d\n", msToNextSlot));
                iRes = PauseStream(pStream->hContext, pStream->hSessMedia, msToNextSlot);
                if (ERROR_SUCCESS != iRes) {
                    return iRes;
                }
            }

            // We are now at the low threshold.  Schedule next sync for the low
            // threshold and account for the overlapping pending bytes.
            pStream->dwNextSlotStartTime += (m_dwHighThresholdMs - m_dwLowThresholdMs);
            pStream->dwPendingBytes = pStream->dwPendingBytes - (pStream->dwHighThresholdBytes - pStream->dwLowThresholdBytes);
        }

        pStream->dwPendingBytes += cbTotalMedia;
    }

    if (pStream->fHciBuffOverflow || (pStream->usSeqNum & 0x0f) == 0x0f) {
        // Check to make sure data is not buffering up in HCI.  If it is then we are
        // entering an area of interference or going out of range.  The only thing
        // we can do is try to block the upper layer for a small amount of time and
        // hope the user inteference passes.  If we do not do this the UX is worse
        // since the user will hear the music breaks and then speed up quickly until
        // it catches up.

        int cPacketsPending = 0;
        BT_ADDR bta = SET_NAP_SAP(pSession->ba.NAP, pSession->ba.SAP);
        
        if (ERROR_SUCCESS == BthGetQueuedHCIPacketCount(&bta, &cPacketsPending)) {
            if (cPacketsPending >= pStream->dwHciBuffThreshold) {
                RETAILMSG(AVDTP_FLOW_CONTROL_DEBUG, (L"[AVDTP] +hci_buff -- pend:%d th:%d slp:%d\n", 
                    cPacketsPending, 
                    pStream->dwHciBuffThreshold, 
                    pStream->dwHciBuffThresholdSleep));

                iRes = PauseStream(pStream->hContext, pStream->hSessMedia, pStream->dwHciBuffThresholdSleep);
                if (ERROR_SUCCESS != iRes) {
                    return iRes;
                }
                
                pStream->fHciBuffOverflow = TRUE;
            } else {
                pStream->fHciBuffOverflow = FALSE;
            }
        }
    }

    // Send the packet

    pBuffer->cStart -= sizeof(RTPHdr_t);
    RTPHdr_t* pHeader = (RTPHdr_t*) (pBuffer->pBuffer + pBuffer->cStart);

    pHeader->firstByte = 0x80; // RTP v2, padding, no extensions, CSRC count=0
    pHeader->payloadType = bPayloadType | (usMarker ? 0x80 : 0x00);
    pHeader->seq = htons(pStream->usSeqNum);
    pHeader->time = htonl(dwTime);
    pHeader->ssrc = htonl(0x00000001); // SSRC=1

    pStream->usSeqNum++;

    iRes = L2CAPSend(NULL, pSession->cid, pBuffer, NULL, FALSE, FALSE);
    
    return iRes;
}

/*------------------------------------------------------------------------------
    AVDTMedia::ProcessPacket
    
    This method is called when an incoming AVDTP packet arrives on a media
    channel.
    
    Parameters:
        pSession:   Session object
        pBuffer:    Buffer from the incoming packet
------------------------------------------------------------------------------*/
int AVDTMedia::ProcessPacket(
            Session* pSession,
            BD_BUFFER* pBuffer)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_INSUFFICIENT_BUFFER;

    RTPHdr_t header;

    if (! BufferGetChunk(pBuffer, sizeof(header), (unsigned char*)&header)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::ProcessPacket : Buffer is too small\n"));
        goto exit;
    }

    BYTE bNumCSRC = header.firstByte & 0x0f;
    BYTE bPayloadType = header.payloadType & 0x7f;
    USHORT usMarker = (header.payloadType & 0x80) ? 0x01 : 0x00;
    DWORD dwTimeInfo = ntohl(header.time);
    USHORT usReliability = 0;

    DWORD cbCSRC = bNumCSRC * 4;

    if (cbCSRC > BufferTotal(pBuffer)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTMedia::ProcessPacket : Invalid CSRC count (%d)\n", bNumCSRC));
        goto exit;
    }

    // Skip CSRC headers
    pBuffer->cStart += cbCSRC;

    iRes = StreamDataUp(pSession, pBuffer, dwTimeInfo, bPayloadType, usMarker, usReliability);

exit:
    if ((ERROR_SUCCESS != iRes) && pBuffer->pFree) {
        pBuffer->pFree(pBuffer);
    }
    
    return iRes;
}

            
