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
#include <windows.h>
#include <svsutil.hxx>

#if defined (DEBUG) || defined (_DEBUG)
#undef DEBUGCHK
#define DEBUGCHK    SVSUTIL_ASSERT
#endif

#include "bthidpriv.h"
#include "hidpkt.h"

static CRITICAL_SECTION gAllocator;
static FixedMemDescr    *pfmdAllocator;

int BthPktInitAllocator (void) {
    InitializeCriticalSection (&gAllocator);
    pfmdAllocator = svsutil_AllocFixedMemDescr (sizeof(BTHHIDPacket), 10);
    return pfmdAllocator != NULL;
}

int BthPktFreeAllocator (void) {
    if (pfmdAllocator)
        svsutil_ReleaseFixedNonEmpty (pfmdAllocator);

    pfmdAllocator = NULL;
    DeleteCriticalSection (&gAllocator);

    return TRUE;
}

void *BTHHIDPacket::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(BTHHIDPacket));

    if (! pfmdAllocator)
        return NULL;
    
    EnterCriticalSection (&gAllocator);
    void *pRes = svsutil_GetFixed (pfmdAllocator);
    LeaveCriticalSection (&gAllocator);

    return pRes;
}

void BTHHIDPacket::operator delete(void *ptr) {
    EnterCriticalSection (&gAllocator);
    if (pfmdAllocator)
        svsutil_FreeFixed (ptr, pfmdAllocator);
    LeaveCriticalSection (&gAllocator);
}

BTHHIDPacket::BTHHIDPacket() :
    m_bHeader(0),
    m_pbPayload(NULL),
    m_cbPayload(0),
    m_cbRemaining(0),
    m_iMTU(0),
    m_iReportType(BTHID_REPORT_OTHER)
{
}

BTHHIDPacket::BTHHIDPacket(BYTE* pPayload, int cbBuffer) :
    m_bHeader(0),
    m_pbPayload(pPayload),
    m_cbPayload(cbBuffer),
    m_cbRemaining(cbBuffer),
    m_iMTU(0),
    m_iReportType(BTHID_REPORT_OTHER)
{
}
/*
BTHHIDPacket::BTHHIDPacket(const BTHHIDPacket& packet)
{
    if (this != &packet)
    {
        m_bHeader = packet.m_bHeader;
        m_cbRemaining = packet.m_cbRemaining;
        m_iMTU = packet.m_iMTU;
        m_iReportType = packet.m_iReportType;
        m_cbPayload = packet.m_cbPayload;
        if (m_cbPayload > 0)
        {
            m_pbPayload = new BYTE[m_cbPayload];
            memcpy(m_pbPayload, packet.m_pbPayload, m_cbPayload);
        }
        else
        {
            m_pbPayload = NULL;
        }
    }
}
*/

void BTHHIDPacket::ReleasePayload()
{
    if (m_pbPayload != m_ucBuffer)
        delete[] m_pbPayload;

    m_cbPayload = 0;
    m_pbPayload = NULL;
}

void BTHHIDPacket::SetReportType(E_BTHID_REPORT_TYPES iReportType)
{
    m_iReportType = iReportType;
}

void BTHHIDPacket::SetHeader(BYTE bHeader)
{
    m_bHeader = bHeader;
}

void BTHHIDPacket::SetReportID(BYTE bReportID)
{
    m_bReportID = bReportID;
}

void BTHHIDPacket::SetPayload(BYTE* pbPayload, int cbPayload)
{
    m_pbPayload = pbPayload;
    m_cbPayload = cbPayload;
    m_cbRemaining = cbPayload;
}

void BTHHIDPacket::SetMTU(int iMTU)
{
    m_iMTU = iMTU;
}

void BTHHIDPacket::SetOwner(void *pOwner)
{
    m_pOwner = pOwner;
}

E_BTHID_REPORT_TYPES BTHHIDPacket::GetReportType() const
{
    return m_iReportType;
}

BYTE BTHHIDPacket::GetHeader() const
{
    return m_bHeader;
}

int  BTHHIDPacket::GetMTU() const
{
    return m_iMTU;
}

void *BTHHIDPacket::GetOwner() const
{
    return m_pOwner;
}

void BTHHIDPacket::GetPayload(BYTE** ppbPayload, int* pcbPayload) const
{
    if (!IsBadWritePtr(ppbPayload, sizeof(*ppbPayload)) && 
        !IsBadWritePtr(pcbPayload, sizeof(*pcbPayload)))
    {
        *ppbPayload = m_pbPayload;
        *pcbPayload = m_cbPayload;
    }
}

BOOL BTHHIDPacket::AddPayloadChunk(BYTE* pbPayload, int cbPayload)
{
//    DEBUGCHK(!IsBadReadPtr(pbPayload, cbPayload));

    // The payload contains the BTHID header. Store it.
    if (m_bHeader == 0) {
        m_bHeader = *pbPayload;         
    }

    ++pbPayload;
    --cbPayload;

    if (cbPayload > 0)
    {
        // Regrow internal array
        int cNew = m_cbPayload + cbPayload;
        BYTE* pNew = cNew > sizeof(m_ucBuffer) ? new BYTE[cNew] : m_ucBuffer;

        if (pNew)
        {
            // Copy old data
            if ((m_cbPayload > 0) && (pNew != m_pbPayload))
            {
                memcpy(pNew, m_pbPayload, m_cbPayload);

                if (m_pbPayload != m_ucBuffer)
                    delete[] m_pbPayload;
            }
            m_pbPayload = pNew;

            // Add the new chunk
            memcpy(m_pbPayload + m_cbPayload, pbPayload, cbPayload);
            m_cbPayload += cbPayload;
            m_cbRemaining = m_cbPayload;
        }
    }

    // If this chunk is equal to the MTU size, we need to recv more data so
    // we return true.  Otherwise, we return false to indicate that the
    // packet is complete and can be indicated up.

    return (cbPayload + 1) == m_iMTU;
}

BOOL BTHHIDPacket::GetPayloadChunk(BYTE* pbPayload, int cbPayload, int* pcbTransfered)
{
//    DEBUGCHK(!IsBadWritePtr(pcbTransfered, sizeof(*pcbTransfered)));
 
    int c = (m_iMTU - 1) < m_cbRemaining ? (m_iMTU-1) : m_cbRemaining;
    int cPayloadIndex = 0;

    if (cbPayload == 0) {
        *pcbTransfered = c + 1;
        return TRUE;
    }

    DEBUGCHK(cbPayload >= c);

    if (m_cbRemaining == m_cbPayload)
    {
        *(pbPayload +cPayloadIndex) = m_bHeader;
        cPayloadIndex++;
        //The first payload should have the report ID as the first byte.
        *(pbPayload +cPayloadIndex) = m_bReportID;
        cPayloadIndex++;        
    }
    else
    {
        BTHID_Header_Parameter headerDATC;
        headerDATC.bRawHeader = 0;
        headerDATC.datc_p.bReportType = m_iReportType;
        
        *(pbPayload +cPayloadIndex)= (BTHID_DATC << 4) | headerDATC.bRawHeader;
        cPayloadIndex++;
    }


    if (c > 0)
    {
        memcpy(pbPayload + cPayloadIndex, (m_cbPayload - m_cbRemaining) + m_pbPayload, c);
        m_cbRemaining -= c;
        cPayloadIndex += c;
    }

    // Transfered c payload bytes plus the header
    *pcbTransfered = cPayloadIndex;

    return (m_cbRemaining > 0) || (c == m_iMTU-1);
}