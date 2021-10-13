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
#include <windows.h>

#include <bt_sdp.h>
#include <bthapi.h>

#include "hidsdp.h"

BTHHIDSdpParser::BTHHIDSdpParser() :
    m_ppRecords(NULL),
    m_cRecords(0),
    m_fInfoLoaded(FALSE)
{
}

BTHHIDSdpParser::~BTHHIDSdpParser()
{
    unsigned int i = 0;
    if (m_ppRecords)
    {
        for (i = 0; i < m_cRecords; i++) 
            m_ppRecords[i]->Release();
        CoTaskMemFree(m_ppRecords);
    }
}

int BTHHIDSdpParser::Start (const unsigned char* pSdpBuffer, int cBuffer)
{
    int iErr = ERROR_SUCCESS;

    CoInitializeEx(NULL, COINIT_MULTITHREADED); 

    HRESULT hrServiceSearch = ServiceAndAttributeSearch((UCHAR*) pSdpBuffer, cBuffer, &m_ppRecords, &m_cRecords);
    if (SUCCEEDED(hrServiceSearch) && m_cRecords == 1)
        m_fInfoLoaded = TRUE;
    else
        iErr = ERROR_INVALID_PARAMETER;

    return iErr;
}

int BTHHIDSdpParser::End (void)
{
    this->BTHHIDSdpParser::~BTHHIDSdpParser();
    this->BTHHIDSdpParser::BTHHIDSdpParser();

    CoUninitialize();

    return 0;
}

// Accessors
int BTHHIDSdpParser::GetHIDReconnectInitiate(BOOL* pfReconnectInitiate)
{
    int iErr = ERROR_INVALID_PARAMETER;

    if (m_fInfoLoaded && pfReconnectInitiate)
    {
        ISdpRecord* pRecord = m_ppRecords[0];
        NodeData    nodeAttrib;
        HRESULT     hr = pRecord->GetAttribute(SDP_ATTRIB_HID_RECONNECT_INITIATE, &nodeAttrib);

        if (SUCCEEDED(hr))
        {
            *pfReconnectInitiate = nodeAttrib.u.booleanVal;
            iErr = ERROR_SUCCESS;
        }
    }

    return iErr;
}

int BTHHIDSdpParser::GetHIDNormallyConnectable(BOOL* pfNormallyConnectable)
{
    int iErr = ERROR_INVALID_PARAMETER;

    if (m_fInfoLoaded && pfNormallyConnectable)
    {
        ISdpRecord* pRecord = m_ppRecords[0];
        NodeData    nodeAttrib;
        HRESULT     hr = pRecord->GetAttribute(SDP_ATTRIB_HID_NORMALLY_CONNECTABLE, &nodeAttrib);

        if (SUCCEEDED(hr))
        {
            *pfNormallyConnectable = nodeAttrib.u.booleanVal;
            iErr = ERROR_SUCCESS;
        }
    }

    return iErr;
}

int BTHHIDSdpParser::GetHIDVirtualCable(BOOL* pfVirtualCable)
{
    int iErr = ERROR_INVALID_PARAMETER;

    if (m_fInfoLoaded && pfVirtualCable)
    {
        ISdpRecord* pRecord = m_ppRecords[0];
        NodeData    nodeAttrib;
        HRESULT     hr = pRecord->GetAttribute(SDP_ATTRIB_HID_VIRTUAL_CABLE, &nodeAttrib);

        if (SUCCEEDED(hr))
        {
            *pfVirtualCable = nodeAttrib.u.booleanVal;
            iErr = ERROR_SUCCESS;
        }
    }

    return iErr;

}

int BTHHIDSdpParser::GetHIDDeviceSubclass(unsigned char* pucDeviceSubclass)
{
    int iErr = ERROR_INVALID_PARAMETER;

    if (m_fInfoLoaded && pucDeviceSubclass)
    {
        ISdpRecord* pRecord = m_ppRecords[0];
        NodeData    nodeAttrib;
        HRESULT     hr = pRecord->GetAttribute(SDP_ATTRIB_HID_DEVICE_SUBCLASS, &nodeAttrib);

        if (SUCCEEDED(hr))
        {
            *pucDeviceSubclass = nodeAttrib.u.uint8;
            iErr = ERROR_SUCCESS;
        }
    }

    return iErr;
}
int BTHHIDSdpParser::GetHIDReportDescriptor(LPBLOB pbReportDescriptor)
{
    int                 iErr            = E_FAIL;
    unsigned long       cElements       = 0;
    ISdpRecord*         pRecord         = NULL;
    ISdpNodeContainer*  pContainer      = NULL;
    NodeData            nodeData, nodeClassDescriptorData;
    HRESULT             hr;

    nodeClassDescriptorData.u.str.length = 0;
    nodeClassDescriptorData.u.str.val = NULL;

    if (!m_fInfoLoaded || !pbReportDescriptor)
    {
        iErr = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    pRecord = m_ppRecords[0];
    hr = pRecord->GetAttribute(SDP_ATTRIB_HID_DESCRIPTOR_LIST, &nodeData);
    if (FAILED(hr))
    {
        iErr = HRESULT_CODE(hr);
        goto Error;
    }

    // Validate structure
    pContainer = nodeData.u.container;
    pContainer->GetNodeCount(&cElements);
    if (cElements != 1)
    {
        iErr = ERROR_BAD_FORMAT;
        goto Error;
    }

    // Move down to only child
    pContainer->GetNode(0, &nodeData);

    // Validate structure. In future versions of HID, 
    // we might get more than 2 children here. At least 2 are garanteed.
    pContainer = nodeData.u.container;
    pContainer->GetNodeCount(&cElements); // on 2nd call, this throws an exception.
    if (cElements < 2)
    {
        iErr = ERROR_BAD_FORMAT;
        goto Error;
    }

    // Move down to the first child, the ClassDescriptorType
    pContainer->GetNodeStringData(1, &nodeClassDescriptorData);

    // We now point to our report descriptor. Fill in the blob.
    if (pbReportDescriptor->cbSize == 0)
        pbReportDescriptor->cbSize = nodeClassDescriptorData.u.str.length;
    else
    {
        if (pbReportDescriptor->pBlobData)
        {
            pbReportDescriptor->cbSize = nodeClassDescriptorData.u.str.length;
            memcpy(pbReportDescriptor->pBlobData, nodeClassDescriptorData.u.str.val, pbReportDescriptor->cbSize);
        }
        else
        {
            iErr = ERROR_INVALID_PARAMETER;
            goto Error;
        }
    }

    iErr = ERROR_SUCCESS;

Error:

    if (nodeClassDescriptorData.u.str.val)
        CoTaskMemFree(nodeClassDescriptorData.u.str.val);

    return iErr;
}

// 
// Takes a raw stream ServiceAttribute response from the server and converts 
// it into an array of ISdpRecord elements to facilitate manipulation.
// 
HRESULT
BTHHIDSdpParser::ServiceAndAttributeSearch(
                          UCHAR *szResponse,             // in - response returned from SDP ServiceAttribute query
                          DWORD cbResponse,            // in - length of response
                          ISdpRecord ***pppSdpRecords, // out - array of pSdpRecords
                          ULONG *pNumRecords           // out - number of elements in pSdpRecords
                          )
{
    HRESULT hres = E_FAIL;
    
    *pppSdpRecords = NULL;
    *pNumRecords = 0;
    
    ISdpStream *pIStream = NULL;
    
    hres = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
        __uuidof(ISdpStream),(LPVOID *) &pIStream);
    
    if (FAILED(hres))
        return hres;  
    
    hres = pIStream->Validate(szResponse,cbResponse,NULL);
    
    if (SUCCEEDED(hres)) {
        hres = pIStream->VerifySequenceOf(szResponse,cbResponse,
            SDP_TYPE_SEQUENCE,NULL,pNumRecords);
        
        if (SUCCEEDED(hres) && *pNumRecords > 0) {
            *pppSdpRecords = (ISdpRecord **) CoTaskMemAlloc(sizeof(ISdpRecord*) * (*pNumRecords));
            
            if (pppSdpRecords != NULL) {
                hres = pIStream->RetrieveRecords(szResponse,cbResponse,*pppSdpRecords,pNumRecords);
                
                if (!SUCCEEDED(hres)) {
                    CoTaskMemFree(*pppSdpRecords);
                    *pppSdpRecords = NULL;
                    *pNumRecords = 0;
                }
            }
            else {
                hres = E_OUTOFMEMORY;
            }
        }
    }
    
    if (pIStream != NULL) {
        pIStream->Release();
        pIStream = NULL;
    }
    
    return hres;
}
