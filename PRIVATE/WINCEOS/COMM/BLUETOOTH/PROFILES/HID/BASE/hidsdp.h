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
#if ! defined (__bthhidsdp_H__)
#define __bthhidsdp_H__        1

struct ISdpRecord;

class BTHHIDSdpParser
{
public:
    BTHHIDSdpParser();
    ~BTHHIDSdpParser();

    int Start (const unsigned char* pSdpBuffer, int cBuffer);
    int End (void);

    // Accessors
    int GetHIDReconnectInitiate(BOOL* pfReconnectInitiate);
    int GetHIDNormallyConnectable(BOOL* pfNormallyConnectable);
    int GetHIDVirtualCable(BOOL* pfVirtualCable);
    int GetHIDDeviceSubclass(unsigned char* pucDeviceSubclass);
    int GetHIDReportDescriptor(LPBLOB pbReportDescriptor);

private:
    HRESULT ServiceAndAttributeSearch(
                          UCHAR *szResponse,             // in - response returned from SDP ServiceAttribute query
                          DWORD cbResponse,              // in - length of response
                          ISdpRecord ***pppSdpRecords,   // out - array of pSdpRecords
                          ULONG *pNumRecords             // out - number of elements in pSdpRecords
                          );



    unsigned long   m_cRecords;
    ISdpRecord**    m_ppRecords;
    BOOL            m_fInfoLoaded;
};

#endif

