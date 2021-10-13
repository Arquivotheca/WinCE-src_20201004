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
#if ! defined (__hidpkt_H__)
#define __hidpkt_H__        1

#define BTHHID_TYPICAL_PAYLOAD    64

class BTHHIDPacket
{
public:
    BTHHIDPacket();
    BTHHIDPacket(BYTE* pPayload, int cbBuffer);

    void ReleasePayload();

    void SetReportType(E_BTHID_REPORT_TYPES iReportType);
    void SetHeader(BYTE bHeader);
    void SetReportID(BYTE bReportID);	
    void SetPayload(BYTE* pbPayload, int cbPayload);     // no copying
    void SetMTU(int iMTU);
    void SetOwner(void *lpOwner);

    E_BTHID_REPORT_TYPES GetReportType() const;
    BYTE GetHeader() const;
    int  GetMTU() const;
    void *GetOwner() const;
    void GetPayload(BYTE** ppbPayload, int* pcbPayload) const; // no copying

    // Copies to and from pbPayload
    BOOL AddPayloadChunk(BYTE* pbPayload, int cbPayload);
    BOOL GetPayloadChunk(BYTE* pbPayload, int cbPayload, int* pcbTransfered);

    void *operator new (size_t iSize);
    void operator delete(void *ptr);

private:
    BTHHIDPacket(const BTHHIDPacket& packet); // shouldn't need that

    BYTE  m_bHeader;
    BYTE m_bReportID;
    BYTE* m_pbPayload;
    int   m_cbPayload;
    int   m_cbRemaining;
    int   m_iMTU;

    void  *m_pOwner;
    E_BTHID_REPORT_TYPES m_iReportType;

    unsigned char m_ucBuffer[BTHHID_TYPICAL_PAYLOAD];
};

int BthPktInitAllocator (void);
int BthPktFreeAllocator (void);

#endif
