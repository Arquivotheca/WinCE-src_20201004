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
#include "CameraDriverTest.h"
#include "logging.h"
#include <pnp.h>
#include <winbase.h>

CCameraDriverTest::CCameraDriverTest()
{
    m_hCamDriver = INVALID_HANDLE_VALUE;
    m_ulNumOfPinTypes = 0;
    m_ptszCameraDevices = NULL;
    m_nCameraDeviceCount = 0;
    m_nSelectedCameraDevice = -1;
    m_nNullDriverIndex = -1;
    m_bUsingNULLDriver = FALSE;

    m_lpData = NULL;
    m_pCSPropDescription = NULL;
    m_pCSPropMemberHeader = NULL;
    m_pCSPropMemberList = NULL;
    m_ulPropertySetDataType = 0;

    for(int i = 0; i < MAX_STREAMS; i++)
    {
        m_pCSMultipleItem[i] = NULL;
        m_ulPinId[i] = 0;
        memset(&(m_PinGuid[0]), 0, sizeof(GUID));
        m_tszDeviceName[i] = NULL;
        m_tszStreamName[i] = NULL;
        m_pCSDataFormat[i] = NULL;
    }
}

CCameraDriverTest::~CCameraDriverTest()
{
    Cleanup();
}

BOOL CCameraDriverTest::Cleanup()
{
    SAFEDELETE(m_lpData);

    if(m_ptszCameraDevices && m_nCameraDeviceCount > 0)
    {
        for(int i = 0; i < m_nCameraDeviceCount; i++)
            delete[] m_ptszCameraDevices[i];

        delete[] m_ptszCameraDevices;
    }

    for(int i = 0; i < MAX_STREAMS; i++)
    {
        if(NULL != m_pCSMultipleItem[i])
            delete[] m_pCSMultipleItem[i];

        if(NULL != m_tszDeviceName[i])
            delete[] m_tszDeviceName[i];

        if(NULL != m_tszStreamName[i])
            delete[] m_tszStreamName[i];

        if(NULL != m_pCSDataFormat[i])
            delete[] m_pCSDataFormat[i];

        camStream[i].Cleanup();
    }

    if(m_hCamDriver != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hCamDriver);
    }

    // now that everything's freed, reinitialize everything fresh.

    m_hCamDriver = INVALID_HANDLE_VALUE;
    m_ulNumOfPinTypes = 0;
    m_ptszCameraDevices = NULL;
    m_nCameraDeviceCount = 0;
    m_nSelectedCameraDevice = -1;
    m_nNullDriverIndex = -1;
    m_bUsingNULLDriver = FALSE;

    m_lpData = NULL;
    m_pCSPropDescription = NULL;
    m_pCSPropMemberHeader = NULL;
    m_pCSPropMemberList = NULL;
    m_ulPropertySetDataType = 0;

    for(int i = 0; i < MAX_STREAMS; i++)
    {
        m_pCSMultipleItem[i] = NULL;
        m_ulPinId[i] = 0;
        memset(&(m_PinGuid[0]), 0, sizeof(GUID));
        m_tszDeviceName[i] = NULL;
        m_tszStreamName[i] = NULL;
        m_pCSDataFormat[i] = NULL;
    }

    return TRUE;
}

VOID CCameraDriverTest::PrepareTestEnvironment (GUID guidPropertySet, ULONG ulProperty, ULONG ulPropSetDataType)
{
    m_PropertySetGuid = guidPropertySet;
    m_ulPropertyID = ulProperty;
    m_ulPropertySetDataType = ulPropSetDataType;
}

BOOL CCameraDriverTest::TestDeviceIOControl(DWORD dwIoControlCode, 
                                            LPVOID lpInBuffer, 
                                            DWORD nInBufferSize, 
                                            LPVOID lpOutBuffer,  
                                            DWORD nOutBufferSize,  
                                            LPDWORD lpBytesReturned,  
                                            LPOVERLAPPED lpOverlapped)
{
    if(!DeviceIoControl(m_hCamDriver, 
                            dwIoControlCode, 
                            lpInBuffer, 
                            nInBufferSize, 
                            lpOutBuffer,  
                            nOutBufferSize,  
                            lpBytesReturned,  
                            lpOverlapped))
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::StreamDeviceIOControl(ULONG ulPinType,
                                                    DWORD dwIoControlCode, 
                                                    LPVOID lpInBuffer, DWORD nInBufferSize, 
                                                    LPVOID lpOutBuffer,  DWORD nOutBufferSize,  
                                                    LPDWORD lpBytesReturned,  LPOVERLAPPED lpOverlapped)

{
    if(STREAM_PREVIEW > ulPinType || STREAM_STILL < ulPinType)
    {
        FAIL(TEXT("StreamDeviceIOControl : invalid pin ID."));
        return FALSE;
    }

    return camStream[ulPinType].TestStreamDeviceIOControl(dwIoControlCode, 
                                                            lpInBuffer, 
                                                            nInBufferSize, 
                                                            lpOutBuffer, 
                                                            nOutBufferSize, 
                                                            lpBytesReturned,
                                                            lpOverlapped);
}

BOOL CCameraDriverTest::FindInSteppedRanges(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers)
{
    return FALSE;
}

BOOL CCameraDriverTest::FindInRanges(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers)
{
    return FALSE;
}


BOOL CCameraDriverTest::FindInValues(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers)
{
    return FALSE;
}

BOOL CCameraDriverTest::GetPinCTypes(ULONG *pulCTypes)
{
    DWORD        dwBytesReturned = 0;
    CSPROPERTY    csProp;
    
    if(NULL != pulCTypes)
        *pulCTypes = 0;

    csProp.Set = CSPROPSETID_Pin;
    csProp.Id = CSPROPERTY_PIN_CTYPES;
    csProp.Flags = CSPROPERTY_TYPE_GET;

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csProp, 
                                        sizeof(CSPROPERTY), 
                                        &m_ulNumOfPinTypes, 
                                        sizeof(ULONG), 
                                        &dwBytesReturned,
                                        NULL))
    {
        FAIL(TEXT("GetPinCTypes : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  "));
        return FALSE;
    }

    if(0 == m_ulNumOfPinTypes)
    {
        FAIL(TEXT("GetPinCTypes : PIN_CTYPES do not match expected value"));
        return FALSE;
    }

    if(NULL != pulCTypes)
        *pulCTypes = m_ulNumOfPinTypes;

    return TRUE;
}


ULONG CCameraDriverTest::MatchPin(GUID guidPin)
{
    DWORD    dwBytesReturned = 0;
    CSP_PIN csPin;
    
    csPin.Property.Set = CSPROPSETID_Pin;
    csPin.Property.Id = CSPROPERTY_PIN_CATEGORY;
    csPin.Property.Flags = CSPROPERTY_TYPE_GET; 
    csPin.Reserved = 0;

    for (int iCount = 0; iCount < (int)m_ulNumOfPinTypes; iCount++)
    {
        GUID guidCategory;
        csPin.PinId = iCount;
        dwBytesReturned = 0;

        if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csPin, 
                                            sizeof(CSP_PIN), 
                                            &guidCategory, 
                                            sizeof (GUID),
                                            &dwBytesReturned,
                                            NULL))
        {
            FAIL(TEXT("MatchPin : TestDeviceIOControl should not have failed."));
            return FALSE;
        }

        if(guidPin == guidCategory)
            break;

    }

    return iCount;
}

BOOL CCameraDriverTest::GetDataRanges(SHORT iPinType ,PCSMULTIPLE_ITEM *ppCSMultipleItem) 
{
    CSP_PIN    csPin;
    DWORD    dwBytesReturned = 0;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("GetDataRanges : invalid pin ID."));
        return FALSE;
    }

    csPin.Property.Set = CSPROPSETID_Pin;
    csPin.Property.Id = CSPROPERTY_PIN_DATARANGES;
    csPin.Property.Flags = CSPROPERTY_TYPE_GET; 
    csPin.Reserved = 0;
    csPin.PinId = m_ulPinId[iPinType];

    if(TRUE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csPin, 
                                        sizeof(CSP_PIN), 
                                        NULL, 
                                        0,
                                        &dwBytesReturned,
                                        NULL))
    {
        FAIL(TEXT("GetDataRanges : TestDeviceIOControl should have failed."));
        return FALSE;
    }

    if(ERROR_MORE_DATA != GetLastError())
    {
        FAIL(TEXT("GetDataRanges : Incorrect Error code"));
        return FALSE;
    }

    if(0 == dwBytesReturned)
    {
        FAIL(TEXT("GetDataRanges : DataRange buffer can not be 0 length."));
        return FALSE;
    }

    if(m_pCSMultipleItem[iPinType])
        delete[] m_pCSMultipleItem[iPinType];

    m_pCSMultipleItem[iPinType] = reinterpret_cast< PCSMULTIPLE_ITEM > (new BYTE [dwBytesReturned]);

    if(NULL == m_pCSMultipleItem[iPinType])
    {
        FAIL(TEXT("GetDataRanges : OOM"));
        return FALSE;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csPin, 
                                        sizeof(CSP_PIN), 
                                        m_pCSMultipleItem[iPinType], 
                                        dwBytesReturned,
                                        &dwBytesReturned,
                                        NULL))
    {
        FAIL(TEXT("GetDataRanges : TestDeviceIOControl should not have failed while retrieving dataranges."));
        delete[] m_pCSMultipleItem[iPinType];
        m_pCSMultipleItem[iPinType] = NULL;
        return FALSE;
    }

    if(NULL == m_pCSMultipleItem[iPinType])
    {
        FAIL(TEXT("GetDataRanges  : m_pCSMultipleItem is NULL."));
        return FALSE;
    }

    if(0 == m_pCSMultipleItem[iPinType]->Count)
    {
        FAIL(TEXT("GetDataRanges  : m_pCSMultipleItem->Count should be greater than 0. No dataranges provided by the driver."));
        delete[] m_pCSMultipleItem[iPinType];
        m_pCSMultipleItem[iPinType] = NULL;
        return FALSE;
    }

    if(sizeof (CSMULTIPLE_ITEM) >= m_pCSMultipleItem[iPinType]->Size)
    {
        FAIL(TEXT("GetDataRanges  : m_pCSMultipleItem->Size should be greater than sizeof(CSMULTIPLE_ITEM). Size member does not include size for datarange structures to follow."));
        delete[] m_pCSMultipleItem[iPinType];
        m_pCSMultipleItem[iPinType] = NULL;
        return FALSE;        
    }

    if(NULL != ppCSMultipleItem)
        *ppCSMultipleItem = m_pCSMultipleItem[iPinType];

    return TRUE;
}

BOOL CCameraDriverTest::AvailablePinInstance(SHORT iPinType)
{
    CSP_PIN csPin;
    DWORD dwBytesReturned = 0;
    CSPIN_CINSTANCES csPinCInstances;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("AvailablePinInstance : invalid pin ID."));
        return FALSE;
    }

    csPin.Property.Set = CSPROPSETID_Pin;
    csPin.Property.Id = CSPROPERTY_PIN_CINSTANCES;
    csPin.Property.Flags = CSPROPERTY_TYPE_GET; 
    csPin.Reserved = 0;
    csPin.PinId = m_ulPinId[iPinType];
    
    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csPin, 
                                        sizeof(CSP_PIN), 
                                        &csPinCInstances , 
                                        sizeof(CSPIN_CINSTANCES), 
                                        &dwBytesReturned,
                                        NULL))
    {
        // the caller will determine whether or not this is a failure.
        return FALSE;
    }

    if(csPinCInstances.CurrentCount >= csPinCInstances.PossibleCount)
    {
        FAIL(TEXT("AvailablePinInstance : Current number of streams has already reached the Max allowed for this pin"));
        return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::InitializeDriver()
{
    if(!m_ptszCameraDevices)
    {
        FAIL(TEXT("InitializeDriver : invalid driver array."));
        return FALSE;
    }

    if(m_nSelectedCameraDevice < 0 || m_nSelectedCameraDevice >= m_nCameraDeviceCount)
    {
        FAIL(TEXT("InitializeDriver : invalid driver index."));
        return FALSE;
    }

    m_hCamDriver = CreateFile(m_ptszCameraDevices[m_nSelectedCameraDevice],
                                    GENERIC_READ|GENERIC_WRITE,
                                    0,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
    if(m_hCamDriver == INVALID_HANDLE_VALUE)
    {
        SKIP(TEXT("InitializeDriver : CreateFile failed"));
        return FALSE;
    }

    if(FALSE == GetPinCTypes(NULL))
    {
        SKIP(TEXT("InitializeDriver : GetPinCTypes failed"));
        CloseHandle(m_hCamDriver);
        m_hCamDriver = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    // this data structure maps from our pin identifiers (preview 0, capture 1, still 2)
    // to the OS pin identifiers.
    m_ulPinId[STREAM_PREVIEW] = MatchPin(PINNAME_VIDEO_PREVIEW);
    m_PinGuid[STREAM_PREVIEW] = PINNAME_VIDEO_PREVIEW;

    m_ulPinId[STREAM_CAPTURE] = MatchPin(PINNAME_VIDEO_CAPTURE);
    m_PinGuid[STREAM_CAPTURE] = PINNAME_VIDEO_CAPTURE;

    m_ulPinId[STREAM_STILL] = MatchPin(PINNAME_VIDEO_STILL);
    m_PinGuid[STREAM_STILL] = PINNAME_VIDEO_STILL;

    if(AvailablePinInstance(STREAM_PREVIEW))
    {
        GetDataRanges(STREAM_PREVIEW, NULL);
        m_tszDeviceName[STREAM_PREVIEW] = GetDeviceName(STREAM_PREVIEW);
        m_tszStreamName[STREAM_PREVIEW] = GetStreamName(STREAM_PREVIEW);
    }

    if(AvailablePinInstance(STREAM_CAPTURE))
    {
        GetDataRanges(STREAM_CAPTURE, NULL);
        m_tszDeviceName[STREAM_CAPTURE] = GetDeviceName(STREAM_CAPTURE);
        m_tszStreamName[STREAM_CAPTURE] = GetStreamName(STREAM_CAPTURE);
    }

    if(AvailablePinInstance(STREAM_STILL))
    {
        GetDataRanges(STREAM_STILL, NULL);
        m_tszDeviceName[STREAM_STILL] = GetDeviceName(STREAM_STILL);
        m_tszStreamName[STREAM_STILL] = GetStreamName(STREAM_STILL);
    }

    return TRUE;
}

TCHAR * CCameraDriverTest::GetStreamName(SHORT iPinType)
{
    CSP_PIN csPin;
    DWORD dwBytesReturned = 0;
    TCHAR *tszStreamName;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("GetStreamName : invalid pin ID."));
        return FALSE;
    }

    csPin.Property.Set = CSPROPSETID_Pin;
    csPin.Property.Id = CSPROPERTY_PIN_NAME;
    csPin.Property.Flags = CSPROPERTY_TYPE_GET; 
    csPin.Reserved = 0;
    csPin.PinId = m_ulPinId[iPinType];

    if(TRUE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csPin, 
                                    sizeof(CSP_PIN), 
                                    NULL , 
                                    0,
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("GetStreamName : TestDeviceIOControl should have failed."));
        return NULL;
    }

    tszStreamName = new TCHAR[dwBytesReturned];
    if(NULL == tszStreamName)
    {
        FAIL(TEXT("GetStreamName : OOM "));
        return NULL;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csPin, 
                                    sizeof(CSP_PIN), 
                                    tszStreamName, 
                                    dwBytesReturned,
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("GetStreamName : TestDeviceIOControl should not have failed."));
        delete[] tszStreamName;
        return FALSE;
    }

    return tszStreamName;
}

TCHAR * CCameraDriverTest::GetDeviceName(SHORT iPinType)
{
    CSP_PIN csPin;
    DWORD dwBytesReturned = 0;
    TCHAR *tszStreamName;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("GetDeviceName : invalid pin ID."));
        return FALSE;
    }

    csPin.Property.Set = CSPROPSETID_Pin;
    csPin.Property.Id = CSPROPERTY_PIN_DEVICENAME;
    csPin.Property.Flags = CSPROPERTY_TYPE_GET; 
    csPin.Reserved = 0;
    csPin.PinId = m_ulPinId[iPinType];

    if(TRUE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csPin, 
                                    sizeof(CSP_PIN), 
                                    NULL , 
                                    0,
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("GetDeviceName : TestDeviceIOControl should have failed."));
        return NULL;
    }

    tszStreamName = new TCHAR[dwBytesReturned];
    if(NULL == tszStreamName)
    {
        FAIL(TEXT("GetDeviceName : OOM "));
        return NULL;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csPin, 
                                    sizeof(CSP_PIN), 
                                    tszStreamName, 
                                    dwBytesReturned,
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("GetDeviceName : TestDeviceIOControl should not have failed."));
        delete[] tszStreamName;
        return FALSE;
    }

    return tszStreamName;
}

int CCameraDriverTest::GetNumSupportedFormats(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("GetNumSupportedFormats : invalid pin ID."));
        return 0;
    }

    return (int)m_pCSMultipleItem[iPinType]->Count;
}

BOOL CCameraDriverTest::GetFormatInfo(SHORT iPinType, int nVideoFormat, PCS_DATARANGE_VIDEO pcsVideoData)
{
    PCSDATARANGE pCSDataRange = NULL;
    PCS_DATARANGE_VIDEO pcsVideo = NULL;

    if(NULL == pcsVideoData)
    {
        FAIL(TEXT("GetFormatInfo : invalid CS_DATARANGE_VIDEO pointer."));
        return FALSE;
    }

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("GetFormatInfo : invalid pin ID."));
        return FALSE;
    }

    if(nVideoFormat >=  (int)m_pCSMultipleItem[iPinType]->Count)
    {
        FAIL(TEXT("GetFormatInfo  : Format selected greater than maximum format supported."));
        return FALSE;
    }

    pCSDataRange = reinterpret_cast< PCSDATARANGE > (m_pCSMultipleItem[iPinType] + 1);

    if(NULL == pCSDataRange)
    {
        FAIL(TEXT("GetFormatInfo  : pCSDataRange can not be NULL."));
        return FALSE;
    }

    // walk through the multiple item structure until we retrieve the format we want (based on position in the structure).
    for (; nVideoFormat--;)
    {
        // if we don't have a match, advance to the next one.
        if(pCSDataRange)
            pCSDataRange = reinterpret_cast<PCSDATARANGE> ((reinterpret_cast< BYTE *>(pCSDataRange) +  ((pCSDataRange->FormatSize +7) & ~7)));
    }

    if(!pCSDataRange)
        return FALSE;

    if(pCSDataRange->MajorFormat != CSDATAFORMAT_TYPE_VIDEO)
    {
        FAIL(TEXT("GetFormatInfo  : incorrect major format type."));
        return FALSE;
    }

    if(pCSDataRange->FormatSize != sizeof(CS_DATARANGE_VIDEO))
    {
        FAIL(TEXT("GetFormatInfo  : incorrect format size for the pCSDataRange."));
        return FALSE;
    }

    pcsVideo = reinterpret_cast<PCS_DATARANGE_VIDEO>(pCSDataRange);

    memcpy(pcsVideoData, pcsVideo, sizeof(CS_DATARANGE_VIDEO));

    return TRUE;
}

BOOL CCameraDriverTest::SelectVideoFormat(SHORT iPinType, int nVideoFormat)
{
    PCSMULTIPLE_ITEM pCSMultipleItem = NULL;
    PCSDATARANGE pCSDataRange = NULL;
    PCSP_PIN pCSPin = NULL;
    DWORD dwBytesReturned = 0;
    DWORD dwBufferSize = 0;
    BOOL bRet = FALSE;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("SelectVideoFormat : invalid pin ID."));
        return FALSE;
    }

    if(nVideoFormat >=  (int)m_pCSMultipleItem[iPinType]->Count)
    {
        FAIL(TEXT("SelectVideoFormat  : Format selected greater than maximum format supported."));
        goto done;
    }

    pCSDataRange = reinterpret_cast< PCSDATARANGE > (m_pCSMultipleItem[iPinType] + 1);

    if(NULL == pCSDataRange)
    {
        FAIL(TEXT("SelectVideoFormat  : pCSDataRange can not be NULL."));
        goto done;
    }

    // walk through the multiple item structure until we retrieve the format we want (based on position in the structure).
    for (; nVideoFormat--;)
    {
        // if we don't have a match, advance to the next one.
        if(pCSDataRange)
            pCSDataRange = reinterpret_cast<PCSDATARANGE> ((reinterpret_cast< BYTE *>(pCSDataRange) +  ((pCSDataRange->FormatSize +7) & ~7)));
        // the cast failed, we're past the end of the structure.
        else break;
    }

    // at this point, we can be guaranteed that pCSDataRange is at the format requested, or is NULL.
    if(NULL == pCSDataRange)
        goto done;

    dwBufferSize = sizeof(*pCSPin) + sizeof(*(m_pCSMultipleItem[iPinType])) + pCSDataRange->FormatSize;
    pCSPin    = reinterpret_cast< PCSP_PIN > (new BYTE [dwBufferSize]);
    if(NULL == pCSPin)
    {
        FAIL(TEXT("SelectVideoFormat  : OOM."));
        goto done;
    }

    pCSPin->Property.Set = CSPROPSETID_Pin;
    pCSPin->Property.Id = CSPROPERTY_PIN_DATAINTERSECTION;
    pCSPin->Property.Flags = CSPROPERTY_TYPE_GET;
    pCSPin->PinId = m_ulPinId[iPinType];
    pCSPin->Reserved = 0;

    pCSMultipleItem = reinterpret_cast< PCSMULTIPLE_ITEM > (pCSPin + 1);
    pCSMultipleItem->Count = 1;
    pCSMultipleItem->Size = sizeof(*pCSMultipleItem) + pCSDataRange->FormatSize;

    // copy over the requested format to the multiple item structure, and then modify it
    // for what we want before sending it back to the driver for configuration.
    memcpy(pCSMultipleItem + 1, pCSDataRange, pCSDataRange->FormatSize);

    // Now we are all set to query for the size of Property Value returned
    dwBytesReturned = 0;
    if(TRUE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    pCSPin, 
                                    dwBufferSize, 
                                    NULL , 
                                    0, 
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("SelectVideoFormat : TestDeviceIOControl should have failed. "));
        goto done;
    }

    if(ERROR_MORE_DATA != GetLastError())
    {
        FAIL(TEXT("SelectVideoFormat : GetLastError did not return expected value "));
        goto done;
    }

    if(0 == dwBytesReturned)
    {
        FAIL(TEXT("SelectVideoFormat : Required Size of Buffer should not be 0"));
        goto done;
    }

    // if we're called a second time for the same pin, cleanup to prevent a leak.
    if(m_pCSDataFormat[iPinType] != NULL)
        delete[] m_pCSDataFormat[iPinType];

    m_pCSDataFormat[iPinType] = reinterpret_cast< PCSDATAFORMAT > (new BYTE[dwBytesReturned]);
    if(NULL == m_pCSDataFormat[iPinType])
    {
        FAIL(TEXT("SelectVideoFormat : OOM"));
        goto done;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    pCSPin, 
                                    dwBufferSize, 
                                    m_pCSDataFormat[iPinType], 
                                    dwBytesReturned, 
                                    &dwBytesReturned,
                                    NULL))
    {
        FAIL(TEXT("SelectVideoFormat : TestDeviceIOControl  failed. "));
        goto done;
    }

    if(FALSE == camStream[iPinType].SetFormat(m_pCSDataFormat[iPinType], dwBytesReturned))
    {
        FAIL(TEXT("SelectVideoFormat : SetFormat failed. "));
        goto done;
    }

    bRet = TRUE;

done :

    if(NULL != pCSPin)
    {
        delete[] pCSPin;
        pCSPin = NULL;
    } 

    return bRet;
}

BOOL CCameraDriverTest::IsPropertySetSupported()
{
    DWORD dwBytesReturned = 0;
    CSPROPERTY csProp;

    csProp.Set = m_PropertySetGuid;
    csProp.Flags = CSPROPERTY_TYPE_SETSUPPORT;
    csProp.Id = 0;    // Ignoring Id since we are not querying about one individual property

    return TestDeviceIOControl(IOCTL_CS_PROPERTY, 
                                &csProp, 
                                sizeof(csProp), 
                                NULL, 
                                0 , 
                                &dwBytesReturned,
                                NULL);

}

BOOL CCameraDriverTest::FetchAccessFlags()
{
    DWORD dwBytesReturned = 0;
    CSPROPERTY csProp;

    csProp.Set = m_PropertySetGuid;
    csProp.Flags = CSPROPERTY_TYPE_BASICSUPPORT;
    csProp.Id = m_ulPropertyID;

    return TestDeviceIOControl(IOCTL_CS_PROPERTY, 
                                &csProp, 
                                sizeof(csProp), 
                                &m_ulAccessFlags, 
                                sizeof(m_ulAccessFlags), 
                                &dwBytesReturned,
                                NULL);
}


BOOL CCameraDriverTest::IsReadAllowed()
{
    return CSPROPERTY_TYPE_GET & m_ulAccessFlags ? TRUE : FALSE;
}

BOOL CCameraDriverTest::IsWriteAllowed()
{
    return CSPROPERTY_TYPE_SET & m_ulAccessFlags ? TRUE : FALSE;
}

BOOL CCameraDriverTest::HasDefaultValue()
{
    // if the property supports read or write, then it has a default value. 
    // the default value should not be in the access flags, it's implied by
    // the ability to read and write.
    return (CSPROPERTY_TYPE_GET | CSPROPERTY_TYPE_SET) & m_ulAccessFlags ? TRUE : FALSE;
}

BOOL CCameraDriverTest::GetCurrentPropertyValue(LPVOID lpDescriptorType,
                                                        DWORD DescriptorSize,
                                                        LPVOID lpValueBuf,
                                                        DWORD ValueBufLen)
{
    DWORD dwBytesReturned = 0;
    if(NULL == lpDescriptorType || NULL == lpValueBuf)
    {
        FAIL(TEXT("GetCurrentPropertyValue : Invalid Parameter(s) passed."));
        return FALSE;
    }

    PCSPROPERTY pCSProp = NULL;
    pCSProp = reinterpret_cast< PCSPROPERTY >(lpDescriptorType);
    pCSProp->Set = m_PropertySetGuid;
    pCSProp->Flags = CSPROPERTY_TYPE_GET;
    pCSProp->Id = m_ulPropertyID;
    if(FALSE == TestDeviceIOControl(IOCTL_CS_PROPERTY, 
                                        lpDescriptorType, 
                                        DescriptorSize, 
                                        lpValueBuf, 
                                        ValueBufLen, 
                                        &dwBytesReturned,
                                        NULL))
    {
//        Log(TEXT("CCameraDriverTest::GetCurrentPropertyValue() : GET query for the given property Failed. "));
        return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::SetPropertyValue(LPVOID lpDescriptorType,
                                                DWORD DescriptorSize,
                                                LPVOID lpValueBuf,
                                                DWORD ValueBufLen)
{
    DWORD dwBytesReturned = 0;
    if(NULL == lpDescriptorType || NULL == lpValueBuf)
    {
        FAIL(TEXT("SetPropertyValue : Invalid Parameter(s) passed."));
        return FALSE;
    }

    PCSPROPERTY pCSProp = NULL;
    pCSProp = reinterpret_cast< PCSPROPERTY >(lpDescriptorType);
    pCSProp->Set         = m_PropertySetGuid;
    pCSProp->Flags        = CSPROPERTY_TYPE_SET;
    pCSProp->Id            = m_ulPropertyID;
    if(FALSE == TestDeviceIOControl(IOCTL_CS_PROPERTY, 
                                        lpDescriptorType, 
                                        DescriptorSize, 
                                        lpValueBuf, 
                                        ValueBufLen, 
                                        &dwBytesReturned,
                                        NULL))
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::FetchDefaultValues()
{
    CSPROPERTY csProp;
    DWORD dwBytesReturned = 0;

    SAFEDELETE(m_lpData);
    
    csProp.Set         = m_PropertySetGuid;
    csProp.Id        = m_ulPropertyID;
    csProp.Flags     = CSPROPERTY_TYPE_DEFAULTVALUES;

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csProp, 
                                    sizeof(csProp), 
                                    NULL, 
                                    sizeof(CSPROPERTY_DESCRIPTION), 
                                    &dwBytesReturned,
                                    NULL))
    {
        // Expect the call to fail with an ERROR_MORE_DATA error, and set the size of the buffer required.
        if(GetLastError() != ERROR_MORE_DATA)
        {
            ERRFAIL(TEXT("FetchDefaultValues : Query to get the size of DEFAULTVALUES list for the given property Failed. "));
            return FALSE;
        }
    }

    if(0 == dwBytesReturned)
    {
        ERRFAIL(TEXT("FetchDefaultValues : DEFAULTVALUES list for this property should not be 0 length. "));
        return FALSE;
    }
    // Allocate the memory for the returned size.

    SAFEDELETE(m_lpData);
    
    m_lpData = reinterpret_cast< PCSPROPERTY_DESCRIPTION > (new BYTE[dwBytesReturned]);

    if(NULL == m_lpData)
    {
        FAIL(TEXT("FetchDefaultValues : Out of memory"));
        return FALSE;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csProp, 
                                    sizeof(csProp), 
                                    m_lpData, 
                                    dwBytesReturned, 
                                    &dwBytesReturned,
                                    NULL))
    {
        // The call can fail with an ERROR_MORE_DATA, and we'll complete the allocation properly below.
        if(GetLastError() != ERROR_MORE_DATA)
        {
            ERRFAIL(TEXT("FetchDefaultValues : DEFAULTVALUES query for the property Failed. "));        
            return FALSE;
        }
    }
   
    //Format of CSPROPERTY_DESCRIPTION
    
    //typedef struct {
    //    ULONG AccessFlags;
    //    ULONG DescriptionSize;
    //    CSIDENTIFIER PropTypeSet;
    //    ULONG MembersListCount;
    //    ULONG Reserved;
    //} CSPROPERTY_DESCRIPTION;

    

    // Check to see if the AccessFlags for PropertyDescription for DEFAULTVALUES 
    // are the same as returned earlier

    m_pCSPropDescription = reinterpret_cast< PCSPROPERTY_DESCRIPTION >(m_lpData);

    if(NULL == m_pCSPropDescription)
    {
        FAIL(TEXT("FetchDefaultValues : unable to cast m_lpData to a PCSPROPERTY_DESCRIPTION."));
        return FALSE;
    }

    // now that we have the structure, we can retrieve the size of the structure as a whole (since that data is in the structure).
    if(m_pCSPropDescription->DescriptionSize > sizeof(CSPROPERTY_DESCRIPTION))
    {
        dwBytesReturned = m_pCSPropDescription->DescriptionSize;
        SAFEDELETE(m_lpData);
        m_lpData = NULL;
        m_pCSPropDescription = NULL;

        m_lpData = reinterpret_cast< PCSPROPERTY_DESCRIPTION > (new BYTE[dwBytesReturned]);

        if(NULL == m_lpData)
        {
            FAIL(TEXT("FetchDefaultValues : Out of memory"));
            return FALSE;
        }

        if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csProp, 
                                        sizeof(csProp), 
                                        m_lpData, 
                                        dwBytesReturned, 
                                        &dwBytesReturned,
                                        NULL))
        {
            ERRFAIL(TEXT("FetchDefaultValues : DEFAULTVALUES query for the property Failed. "));        
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        m_pCSPropDescription = reinterpret_cast< PCSPROPERTY_DESCRIPTION >(m_lpData);

        if(NULL == m_pCSPropDescription)
        {
            FAIL(TEXT("FetchDefaultValues : unable to cast m_lpData to a PCSPROPERTY_DESCRIPTION."));
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

    }

    if(m_ulAccessFlags ^ m_pCSPropDescription->AccessFlags)
    {
        FAIL(TEXT("FetchDefaultValues : AccessFlags for DEFAULTVALUES of the given property do not match the property access flags"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(sizeof(CSPROPERTY_DESCRIPTION) == m_pCSPropDescription->DescriptionSize)
    {
        FAIL(TEXT("FetchDefaultValues :  CSPROPERTY_DESCRIPTION size not set correctly"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(CSPROPTYPESETID_General != m_pCSPropDescription->PropTypeSet.Set)
    {
        FAIL(TEXT("FetchDefaultValues :  requests for the given property returned incorrect PropTypeSet.Set"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }
    
    if(m_ulPropertySetDataType != m_pCSPropDescription->PropTypeSet.Id)
    {
        FAIL(TEXT("FetchDefaultValues :  requests for the given property returned incorrect PropTypeSet.Id"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(0 == m_pCSPropDescription->MembersListCount)
    {
        FAIL(TEXT("FetchDefaultValues : MembersListCount should not be 0"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    // Get to the first CSPROPERTY_MEMBERLIST element, 
    // which is right after CSPROPERTY_DESCRIPTION structure;

    PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL;

    pCSPropMemberList = reinterpret_cast<PCSPROPERTY_MEMBERSLIST> (m_pCSPropDescription + 1);

    // the counter included the start of the list, now that we're 1 in 
    LONG lCounter = m_pCSPropDescription->MembersListCount;

    if(NULL == pCSPropMemberList)
    {
        FAIL(TEXT("FetchDefaultValues : MembersList structure is NULL."));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    // Now we will be traversing this array of CSPROPERTY_MEMBERLIST


    //Format of CSPROPERYT_MEMBERLIST
    //typedef struct {
    //    CSPROPERTY_MEMBERSHEADER    MembersHeader;
    //    const VOID*                 Members;
    //} CSPROPERTY_MEMBERSLIST, *PCSPROPERTY_MEMBERSLIST;


        /*
        // here's the structure of the data

        EntryDataPointer is an array of entries.
        SecondEntry is a single value

        static CSPROPERTY_MEMBERSLIST listname [] = 
        {
           {
                //CSPROPERTY_MEMBERSHEADER
                {    
                    CSPROPERTY_MEMBER_RANGES,                //MembersFlags
                    SIZEOF_ENTRY (EntryDataPointer),            //MembersSize
                    SIZEOF_ARRAY (EntryDataPointer), //MembersCount
                    0                                                              //flags 0 or CSPROPERTY_MEMBER_FLAG_DEFAULT
                //},
                //Members
                (PVOID) EntryDataPointer,                 // MembersSize * MembersCount defines the size of this
             },
             {
                {
                    CSPROPERTY_MEMBER_VALUES,
                    SIZEOF_ENTRY (SecondEntry),
                    1,
                    CSPROPERTY_MEMBER_FLAG_DEFAULT
                },
                (PVOID) &SecondEntry,
            }    
        };
        */

    // lCounter is the number of member headers
    // verify the data structure.
    while(lCounter > 0)
    {
        PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >(pCSPropMemberList);

        // For non-Bolean Properties like Brightness, MemberSize can not be equal to 0
        if(m_ulPropertySetDataType != VT_BOOL && 
            0 == pCSPropMemberHeader->MembersSize)
        {
            FAIL(TEXT("FetchDefaultValues : MembersSize should not be 0 for non-Bolean values."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        // if the member count is 0, then this is the last one.
        if(0 == pCSPropMemberHeader->MembersCount)
        {
            FAIL(TEXT("FetchDefaultValues : MembersCount should not be 0."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        if(CSPROPERTY_MEMBER_STEPPEDRANGES != pCSPropMemberHeader->MembersFlags &&
            CSPROPERTY_MEMBER_RANGES != pCSPropMemberHeader->MembersFlags && 
            CSPROPERTY_MEMBER_VALUES != pCSPropMemberHeader->MembersFlags)
        {
            FAIL(TEXT("FetchDefaultValues : MembersFlags should be MEMBERSHEADER or MEMBER_VALUES."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        pCSPropMemberList++;

        lCounter--;
    }

    if(0 != lCounter)
    {
            FAIL(TEXT("FetchDefaultValues : Memberlists Count does not match with the data buffer entries."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::FetchBasicSupport()
{
    CSPROPERTY csProp;
    DWORD dwBytesReturned = 0;

    SAFEDELETE(m_lpData);
    
    csProp.Set         = m_PropertySetGuid;
    csProp.Id        = m_ulPropertyID;
    csProp.Flags     = CSPROPERTY_TYPE_BASICSUPPORT;

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csProp, 
                                    sizeof(csProp), 
                                    NULL, 
                                    sizeof(CSPROPERTY_DESCRIPTION), 
                                    &dwBytesReturned,
                                    NULL))
    {
        // Expect the call to fail with an ERROR_MORE_DATA error, and set the size of the buffer required.
        if(GetLastError() != ERROR_MORE_DATA)
        {
            ERRFAIL(TEXT("FetchBasicSupport : Query to get the size of DEFAULTVALUES list for the given property Failed. "));
            return FALSE;
        }
    }

    if(0 == dwBytesReturned)
    {
        ERRFAIL(TEXT("FetchBasicSupport : DEFAULTVALUES list for this property should not be 0 length. "));
        return FALSE;
    }
    // Allocate the memory for the returned size.

    SAFEDELETE(m_lpData);
    
    m_lpData = reinterpret_cast< PCSPROPERTY_DESCRIPTION > (new BYTE[dwBytesReturned]);

    if(NULL == m_lpData)
    {
        FAIL(TEXT("FetchBasicSupport : Out of memory"));
        return FALSE;
    }

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                    &csProp, 
                                    sizeof(csProp), 
                                    m_lpData, 
                                    dwBytesReturned, 
                                    &dwBytesReturned,
                                    NULL))
    {
        // The call can fail with an ERROR_MORE_DATA, and we'll complete the allocation properly below.
        if(GetLastError() != ERROR_MORE_DATA)
        {
            ERRFAIL(TEXT("FetchBasicSupport : DEFAULTVALUES query for the property Failed. "));        
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }
    }
   
    //Format of CSPROPERTY_DESCRIPTION
    
    //typedef struct {
    //    ULONG AccessFlags;
    //    ULONG DescriptionSize;
    //    CSIDENTIFIER PropTypeSet;
    //    ULONG MembersListCount;
    //    ULONG Reserved;
    //} CSPROPERTY_DESCRIPTION;

    

    // Check to see if the AccessFlags for PropertyDescription for DEFAULTVALUES 
    // are the same as returned earlier

    m_pCSPropDescription = reinterpret_cast< PCSPROPERTY_DESCRIPTION >(m_lpData);

    if(NULL == m_pCSPropDescription)
    {
        FAIL(TEXT("FetchBasicSupport : unable to cast m_lpData to a PCSPROPERTY_DESCRIPTION."));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    // now that we have the structure, we can retrieve the size of the structure as a whole (since that data is in the structure).
    if(m_pCSPropDescription->DescriptionSize > sizeof(CSPROPERTY_DESCRIPTION))
    {
        dwBytesReturned = m_pCSPropDescription->DescriptionSize;
        SAFEDELETE(m_lpData);
        m_lpData = NULL;
        m_pCSPropDescription = NULL;

        m_lpData = reinterpret_cast< PCSPROPERTY_DESCRIPTION > (new BYTE[dwBytesReturned]);

        if(NULL == m_lpData)
        {
            FAIL(TEXT("FetchBasicSupport : Out of memory"));
            return FALSE;
        }

        if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csProp, 
                                        sizeof(csProp), 
                                        m_lpData, 
                                        dwBytesReturned, 
                                        &dwBytesReturned,
                                        NULL))
        {
            ERRFAIL(TEXT("FetchBasicSupport: DEFAULTVALUES query for the property Failed. "));        
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        m_pCSPropDescription = reinterpret_cast< PCSPROPERTY_DESCRIPTION >(m_lpData);

        if(NULL == m_pCSPropDescription)
        {
            FAIL(TEXT("FetchBasicSupport : unable to cast m_lpData to a PCSPROPERTY_DESCRIPTION."));
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

    }

    if(m_ulAccessFlags ^ m_pCSPropDescription->AccessFlags)
    {
        FAIL(TEXT("FetchBasicSupport : AccessFlags for DEFAULTVALUES of the given property do not match the property access flags"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(sizeof(CSPROPERTY_DESCRIPTION) == m_pCSPropDescription->DescriptionSize)
    {
        FAIL(TEXT("FetchBasicSupport :  CSPROPERTY_DESCRIPTION size not set correctly"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(CSPROPTYPESETID_General != m_pCSPropDescription->PropTypeSet.Set)
    {
        FAIL(TEXT("FetchBasicSupport :  requests for the given property returned incorrect PropTypeSet.Set"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }
    
    if(m_ulPropertySetDataType != m_pCSPropDescription->PropTypeSet.Id)
    {
        FAIL(TEXT("FetchBasicSupport :  requests for the given property returned incorrect PropTypeSet.Id"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    if(0 == m_pCSPropDescription->MembersListCount)
    {
        FAIL(TEXT("FetchBasicSupport : MembersListCount should not be 0"));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    // Get to the first CSPROPERTY_MEMBERLIST element, 
    // which is right after CSPROPERTY_DESCRIPTION structure;

    PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL;

    pCSPropMemberList = reinterpret_cast<PCSPROPERTY_MEMBERSLIST> (m_pCSPropDescription + 1);

    // the counter included the start of the list, now that we're 1 in 
    LONG lCounter = m_pCSPropDescription->MembersListCount;

    if(NULL == pCSPropMemberList)
    {
        FAIL(TEXT("FetchBasicSupport : MembersList structure is NULL."));
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    // Now we will be traversing this array of CSPROPERTY_MEMBERLIST


    //Format of CSPROPERYT_MEMBERLIST
    //typedef struct {
    //    CSPROPERTY_MEMBERSHEADER    MembersHeader;
    //    const VOID*                 Members;
    //} CSPROPERTY_MEMBERSLIST, *PCSPROPERTY_MEMBERSLIST;


        /*
        // here's the structure of the data

        EntryDataPointer is an array of entries.
        SecondEntry is a single value

        static CSPROPERTY_MEMBERSLIST listname [] = 
        {
           {
                //CSPROPERTY_MEMBERSHEADER
                {    
                    CSPROPERTY_MEMBER_RANGES,                //MembersFlags
                    SIZEOF_ENTRY (EntryDataPointer),            //MembersSize
                    SIZEOF_ARRAY (EntryDataPointer), //MembersCount
                    0                                                              //flags 0 or CSPROPERTY_MEMBER_FLAG_DEFAULT
                //},
                //Members
                (PVOID) EntryDataPointer,                 // MembersSize * MembersCount defines the size of this
             },
             {
                {
                    CSPROPERTY_MEMBER_VALUES,
                    SIZEOF_ENTRY (SecondEntry),
                    1,
                    CSPROPERTY_MEMBER_FLAG_DEFAULT
                },
                (PVOID) &SecondEntry,
            }    
        };
        */

    // lCounter is the number of member headers
    // verify the data structure.
    while(lCounter > 0)
    {
        PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >(pCSPropMemberList);

        // For non-Bolean Properties like Brightness, MemberSize can not be equal to 0
        if(m_ulPropertySetDataType != VT_BOOL && 
            0 == pCSPropMemberHeader->MembersSize)
        {
            FAIL(TEXT("FetchBasicSupport : MembersSize should not be 0 for non-Bolean values."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        // if the member count is 0, then this is the last one.
        if(0 == pCSPropMemberHeader->MembersCount)
        {
            FAIL(TEXT("FetchBasicSupport : MembersCount should not be 0."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        if(CSPROPERTY_MEMBER_STEPPEDRANGES != pCSPropMemberHeader->MembersFlags &&
            CSPROPERTY_MEMBER_RANGES != pCSPropMemberHeader->MembersFlags && 
            CSPROPERTY_MEMBER_VALUES != pCSPropMemberHeader->MembersFlags)
        {
            FAIL(TEXT("FetchBasicSupport : MembersFlags should be MEMBERSHEADER or MEMBER_VALUES."));            
            delete[] m_lpData;
            m_lpData = NULL;
            return FALSE;
        }

        // go to the next member.
        pCSPropMemberList ++;

        lCounter--;
    }

    if(0 != lCounter)
    {
        FAIL(TEXT("FetchBasicSupport : Memberlists Count does not match with the data buffer entries."));            
        delete[] m_lpData;
        m_lpData = NULL;
        return FALSE;
    }

    return TRUE;
}

BOOL CCameraDriverTest::GetValueBuffer(PCSPROPERTY_DESCRIPTION *lpPropDesc)
{
    *lpPropDesc = reinterpret_cast< PCSPROPERTY_DESCRIPTION > (m_lpData);
    return NULL != m_lpData ? TRUE : FALSE;
}


HRESULT CCameraDriverTest::GetFirstValue(PVALUEDATA pValData)
{
    if(NULL == m_pCSPropDescription)
    {
        FAIL(TEXT("GetFirstValue : m_pCSPropDescription is NULL"));
        return E_POINTER;
    }

    if(NULL == pValData)
    {
        FAIL(TEXT("GetFirstValue : Parameter passed to this function can not be NULL"));
        return E_INVALIDARG;
    }
    
    if(sizeof(CSPROPERTY_DESCRIPTION) == m_pCSPropDescription->DescriptionSize)
    {
        FAIL(TEXT("GetFirstValue :  No Value Found"));
        return E_UNEXPECTED;    
    }

    if(0 == m_pCSPropDescription->MembersListCount)
    {
        FAIL(TEXT("GetFirstValue : MembersListCount should not be 0"));
        return E_UNEXPECTED;
    }

    // Get to the first CSPROPERTY_MEMBERLIST element, 
    // which is right after CSPROPERTY_DESCRIPTION structure;

    m_pCSPropMemberList  = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> (m_pCSPropDescription + 1);

    if(NULL == m_pCSPropMemberList)
    {
        FAIL(TEXT("GetFirstValue : MembersList structure is NULL."));
        return  E_POINTER;
    }

    m_lCurrentMemberListCounter = 0;
    
    return GetNextMemberItem(pValData) == TRUE?S_OK:E_FAIL;
}


HRESULT CCameraDriverTest::GetNextValue(PVALUEDATA pValData)
{
    if(-1 == m_lCurrentMemberListCounter || -1 == m_lCurrentMembersCounter)
    {
        FAIL(TEXT("GetNextValue : You must call GetFirstValue before this function"));
        return E_UNEXPECTED;
    }

    if(NULL == m_pCSPropDescription)
    {
        FAIL(TEXT("GetNextValue : m_pCSPropDescription can not be NULL"));
        return E_UNEXPECTED;
    }

    if(NULL == m_pCSPropMemberHeader)
    {
        FAIL(TEXT("GetNextValue : m_pCSPropMemberHeader can not be NULL"));
        return E_UNEXPECTED;
    }
    if(NULL == pValData)
    {
        FAIL(TEXT("GetNextValue : Parameter passed to this function can not be NULL"));
        return E_POINTER ;
    }


    if(m_lCurrentMembersCounter >= m_pCSPropMemberHeader->MembersCount)
    {
        m_lCurrentMembersCounter = 0;

        m_lCurrentMemberListCounter++;
        if(m_lCurrentMemberListCounter >= m_pCSPropDescription->MembersListCount)
        {
            FAIL(TEXT("GetNextValue : Value Data was not packed properly "));
            return E_UNEXPECTED;
        }

        // go to the next member by incrementing the member list pointer.
        m_pCSPropMemberList++;
    }


    return GetNextMemberItem(pValData) == TRUE?S_OK:E_FAIL;
}


BOOL CCameraDriverTest::GetNextMemberItem(PVALUEDATA pValData)
{
    m_pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >(m_pCSPropMemberList);

    // For non-Bolean Properties like Brightness, MemberSize can not be equal to 0
    if(m_ulPropertySetDataType != VT_BOOL && 
        0 == m_pCSPropMemberHeader->MembersSize)
    {
        FAIL(TEXT("GetNextMemberItem : MembersSize should not be 0 for non-Bolean values."));            
        return FALSE;
    }

    if(0 == m_pCSPropMemberHeader->MembersCount)
    {
        FAIL(TEXT("GetNextMemberItem : MembersCount should not be equal to 0 "));
        return FALSE;
    }

    LPVOID lpMembers = reinterpret_cast<LPVOID>(m_pCSPropMemberHeader + 1);
    
    switch (m_pCSPropMemberHeader->MembersFlags)
    {
        case CSPROPERTY_MEMBER_VALUES :
            pValData->Type = MEMBER_VALUES;
            pValData->lpValue = reinterpret_cast<PULONG>(lpMembers);
        break;

        case CSPROPERTY_MEMBER_STEPPEDRANGES :
            if(sizeof(LONG) == m_pCSPropMemberHeader->MembersSize)
            {
                pValData->Type = MEMBER_STEPPEDRANGES_LONG;
                pValData->lpValue = reinterpret_cast< PCSPROPERTY_STEPPING_LONG >(lpMembers);
            }
            else if(sizeof(LONGLONG) == m_pCSPropMemberHeader->MembersSize)
            {
                pValData->Type = MEMBER_STEPPEDRANGES_LONGLONG;
                pValData->lpValue = reinterpret_cast< PCSPROPERTY_STEPPING_LONGLONG >(lpMembers);
            }
            else
            {
                FAIL(TEXT("GetNextMemberItem : Incorrect Size"));
                return FALSE;
            }
            break;

        case CSPROPERTY_MEMBER_RANGES :
            if(sizeof(LONG) == m_pCSPropMemberHeader->MembersSize)
            {
                pValData->Type = MEMBER_RANGES_LONG;
                pValData->lpValue = reinterpret_cast< PCSPROPERTY_BOUNDS_LONG  >(lpMembers) ;
            }
            else if(sizeof(LONGLONG) == m_pCSPropMemberHeader->MembersSize)
            {
                pValData->Type = MEMBER_RANGES_LONGLONG;
                pValData->lpValue = reinterpret_cast< PCSPROPERTY_BOUNDS_LONGLONG  >(lpMembers);
            }
            else
            {
                FAIL(TEXT("GetNextMemberItem : Incorrect Size"));
                return FALSE;
            }

        break;
        
        default :
            FAIL(TEXT("GetNextMemberItem : Invalid MemberHeader->Flags value. "));
            return FALSE;
        break;
    }

    m_lCurrentMembersCounter++;
    return TRUE;
}
BOOL CCameraDriverTest::IsValueValid(LPVOID lpBuff)
{
    if(NULL == lpBuff)
    {
        FAIL(TEXT("IsValueValid : Parameter is NULL "));
        return FALSE;
    }

    if(NULL == m_pCSPropDescription)
    {
        FAIL(TEXT("IsValueValid : m_pCSPropDescription is NULL "));
        return FALSE;
    }

    LONG lCounter = m_pCSPropDescription->MembersListCount;

    // Get to the first CSPROPERTY_MEMBERLIST element, 
    // which is right after CSPROPERTY_DESCRIPTION structure;

    PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL;

    pCSPropMemberList = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> (m_pCSPropDescription + 1);

    if(NULL == pCSPropMemberList)
    {
        FAIL(TEXT("IsValueValid : MembersList structure is NULL."));
        return FALSE;
    }

    while(lCounter > 0)
    {
        PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >(pCSPropMemberList);

        LPVOID lpMembers = reinterpret_cast<LPVOID>(pCSPropMemberHeader + 1);
        if(NULL == lpMembers)
        {
                FAIL(TEXT("IsValueValid : Property value data is not packed properly by the driver. "));
                return FALSE;        
        }
        
        switch (pCSPropMemberHeader->MembersFlags)
        {
            case CSPROPERTY_MEMBER_VALUES :
                if(TRUE == FindInValues(lpBuff, pCSPropMemberHeader, lpMembers))
                    return TRUE;
            break;

            case CSPROPERTY_MEMBER_STEPPEDRANGES :
                if(TRUE == FindInSteppedRanges(lpBuff, pCSPropMemberHeader, lpMembers))
                    return TRUE;
            break;

            case CSPROPERTY_MEMBER_RANGES :
                if(TRUE == FindInRanges(lpBuff, pCSPropMemberHeader, lpMembers))
                    return TRUE;                
            break;
            default :
                FAIL(TEXT("IsValueValid : Invalid MemberHeader->Flags value. "));
                return FALSE;
            break;
        }

        // Jump to the next CSPROPERTY_MEMBERLIST Item
        pCSPropMemberList++;
        lCounter--;
    };
    
    return FALSE;
}

BOOL CCameraDriverTest::SetState(SHORT iPinType, CSSTATE csState)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("SetState : invalid pin ID."));
        return FALSE;
    }

    return camStream[iPinType].SetState(csState);
}

BOOL CCameraDriverTest::TriggerCaptureEvent(SHORT iPinType)
{
    DWORD dwBytesReturned = 0;
    CSPROPERTY_VIDEOCONTROL_MODE_S csProp;

    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("TriggerCaptureEvent : invalid pin ID."));
        return FALSE;
    }

    csProp.Property.Set = PROPSETID_VIDCAP_VIDEOCONTROL;
    csProp.Property.Id = CSPROPERTY_VIDEOCONTROL_MODE;
    csProp.Property.Flags = CSPROPERTY_TYPE_SET;
    csProp.StreamIndex = m_ulPinId[iPinType];
    csProp.Mode = CS_VideoControlFlag_Trigger;

    if(FALSE == TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof(CSPROPERTY_VIDEOCONTROL_MODE_S), 
                                            NULL, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL))
    {
        return FALSE;
    }

    return TRUE;
}

CSSTATE CCameraDriverTest::GetState(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
        return CSSTATE_STOP;

    return camStream[iPinType].GetState();
}

BOOL CCameraDriverTest::CreateStream(SHORT iPinType, HWND hwnd, RECT rc)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("CreateStream : invalid pin ID."));
        return FALSE;
    }

    if(FALSE == camStream[iPinType].CreateStream(m_ulPinId[iPinType], m_tszDeviceName[iPinType], iPinType))
    {
        SKIP(TEXT("CreateStream : creating the stream failed"));
        return FALSE;
    }

    if(FALSE == camStream[iPinType].CreateCameraWindow(hwnd, rc))
    {
        SKIP(TEXT("CreateStream : CreateCameraWindow failed"));
        return FALSE;
    }

    if(FALSE == camStream[iPinType].GetFrameAllocationInfoForStream())
    {
        SKIP(TEXT("CreateStream : GetFrameAllocationInfoForStream failed"));
        return FALSE;
    }

    if(FALSE == camStream[iPinType].SetBufferCount(camStream[iPinType].GetDriverBufferCount()))
    {
        SKIP(TEXT("CreateStream : setting the buffer count failed"));
        return FALSE;
    }

    return TRUE;
}

BOOL
CCameraDriverTest::SetupStream(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("SetupStream : invalid pin ID."));
        return FALSE;
    }

    // Set the State to PAUSE before creating the thread,
    // because if it's in the stop state the thread will exit.
    if(FALSE == SetState(iPinType, CSSTATE_PAUSE))
    {
        SKIP(TEXT("SetupStream : SetState failed"));
        return FALSE;
    }

//    if (iPinType == STREAM_STILL)
//    {
//        if(FALSE == camStream[iPinType].AllocateBuffers())
//        {
//            SKIP(TEXT("SetupStream : AllocateBuffers failed"));
//            return FALSE;
//        }
//        return camStream[iPinType].CreateAsyncThread();
//    }
    return TRUE; //}
}

BOOL
CCameraDriverTest::CleanupStream(SHORT iPinType)
{
    // cleanup async thread
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("CleanupStream : invalid pin ID."));
        return FALSE;
    }

    // Set the State to PAUSE before creating the thread,
    // because if it's in the stop state the thread will exit.
    if(FALSE == SetState(iPinType, CSSTATE_STOP))
    {
        SKIP(TEXT("CleanupStream : SetState failed"));
        return FALSE;
    }

//    if(iPinType == STREAM_STILL)
//    {
//        if(FALSE == camStream[iPinType].ReleaseBuffers())
//        {
//            SKIP(TEXT("SetupStream : AllocateBuffers failed"));
//            return FALSE;
//        }
//    }

    return camStream[iPinType].CleanupASyncThread();
}

BOOL
CCameraDriverTest::AllocateBuffers(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("AllocateBuffers : invalid pin ID."));
        return FALSE;
    }
    
    return camStream[iPinType].AllocateBuffers();
}

BOOL
CCameraDriverTest::ReleaseBuffers(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("ReleaseBuffers : invalid pin ID."));
        return FALSE;
    }

    return camStream[iPinType].ReleaseBuffers();
}

BOOL
CCameraDriverTest::SetArtificialDelay(SHORT iPinType, int nDelay)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
    {
        FAIL(TEXT("SetArtificialDelay : invalid pin ID."));
        return FALSE;
    }

    return camStream[iPinType].SetAritificalDelay(nDelay);
}

BOOL
CCameraDriverTest::DetermineCameraAvailability()
{
    BOOL result = TRUE;

    if(!GetAllCameraData())
    {
        FAIL(TEXT("DetermineCameraAvailability : Failed setting up the camera data."));
        result = FALSE;
        goto cleanup;
    }

    if(!SetupNULLCameraDriver())
    {
        FAIL(TEXT("DetermineCameraAvailability : Failed setting up the NULL driver information."));
        result = FALSE;
        goto cleanup;
    }

    // if the null camera device is available, select it.
    // otherwise we'll default to the first entry given.
    if(m_nNullDriverIndex >= 0)
    {
        if(!SelectCameraDevice(m_nNullDriverIndex))
        {
            FAIL(TEXT("DetermineCameraAvailability : Failed selecting the NULL driver ."));
            result = FALSE;
            goto cleanup;
        }
    }
    else result = SelectCameraDevice(0);

cleanup:
    return result;
}

BOOL
CCameraDriverTest::GetAllCameraData()
{
    // we assume failure waiting for the message queue, 
    // if we succeed then we change the result to success.
    BOOL result = FALSE;
    HANDLE hNotification = NULL, hMsgQueue = NULL;
    DWORD dwWaitResult;
    MSGQUEUEOPTIONS msgQOptions;
#ifdef DEVCLASS_CAMERA_GUID
    GUID CameraClass = DEVCLASS_CAMERA_GUID;
#else
    GUID CameraClass = {0xA32942B7, 0x920C, 0x486b, 0xB0, 0xE6, 0x92, 0xA7, 0x02, 0xA9, 0x9B, 0x35};
#endif
    int nMsgQueueTimout = 500;
    int nMaxMessageSize = (MAX_DEVCLASS_NAMELEN * sizeof(TCHAR))+ sizeof(DEVDETAIL);
    memset(&msgQOptions, 0, sizeof(MSGQUEUEOPTIONS));
    msgQOptions.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgQOptions.cbMaxMessage = nMaxMessageSize;
    msgQOptions.bReadAccess = TRUE;

    hMsgQueue = CreateMsgQueue(NULL, &msgQOptions);
    if(NULL == hMsgQueue)
    {
        FAIL(TEXT("GetAllCameraData : Failed to create the message queue for device notifications."));
        goto cleanup;
    }

    hNotification = RequestDeviceNotifications(&CameraClass, hMsgQueue, TRUE);

    if(NULL == hNotification)
    {
        FAIL(TEXT("GetAllCameraData : Failed to request device notifications."));
        goto cleanup;
    }

    DEVDETAIL *ddDeviceInformation = NULL;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;

    ddDeviceInformation = (DEVDETAIL *) new(BYTE[nMaxMessageSize]);

    do
    {

        // enumerate camera drivers in the system
        dwWaitResult = WaitForSingleObject (hMsgQueue, nMsgQueueTimout);

        if(WAIT_OBJECT_0 == dwWaitResult)
        {
            memset(ddDeviceInformation, 0x0, nMaxMessageSize);

            if(FALSE == ReadMsgQueue(hMsgQueue, ddDeviceInformation, nMaxMessageSize, &dwBytesRead, nMsgQueueTimout, &dwFlags))
            {
                FAIL(TEXT("GetAllCameraData : Message queue read failed."));
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                result = FALSE;
                goto cleanup;
            }

            if(0 == ddDeviceInformation->cbName || NULL == ddDeviceInformation->szName)
            {
                FAIL(TEXT("GetAllCameraData : Invalid device detail returned."));
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                result = FALSE;
                goto cleanup;
            }

            // realloc if necessary, otherwise do the initial allocation
            if(m_ptszCameraDevices)
            {
                TCHAR **ptszTemp = NULL;

                ptszTemp = new(TCHAR *[m_nCameraDeviceCount+1]);
                if(ptszTemp)
                {
                    memcpy(ptszTemp, m_ptszCameraDevices, sizeof(TCHAR *) * m_nCameraDeviceCount);
                    delete[] m_ptszCameraDevices;
                    m_ptszCameraDevices = ptszTemp;
                }
                else
                {
                    FAIL(TEXT("GetAllCameraData : Failed to re-allocate the array for the strings."));
                    // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                    result = FALSE;
                    goto cleanup;
                }
            }
            else
            {
                m_ptszCameraDevices = new(TCHAR *[1]);
            }

            if(!m_ptszCameraDevices)
            {
                FAIL(TEXT("GetAllCameraData : Failed to allocate the array of character strings."));
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                result = FALSE;
                goto cleanup;
            }


            // copy over the name given.
            m_ptszCameraDevices[m_nCameraDeviceCount] = new(TCHAR[ddDeviceInformation->cbName]);

            if(!m_ptszCameraDevices[m_nCameraDeviceCount])
            {
                FAIL(TEXT("GetAllCameraData : Failed to allocate character array for the device name."));
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                result = FALSE;
                goto cleanup;
            }

            // copy the string over, our maximum length is the size allocated.
            _tcsncpy(m_ptszCameraDevices[m_nCameraDeviceCount], ddDeviceInformation->szName, ddDeviceInformation->cbName/sizeof(TCHAR));

            // we successfully gathered a device name, so the call is a success (so far)
            result = TRUE;
            // increment the number of successful allocations.
            m_nCameraDeviceCount++;
        }
    // if we get anything else, then we failed.
    }while(WAIT_OBJECT_0 == dwWaitResult);

cleanup:

    if(ddDeviceInformation)
        delete[] ddDeviceInformation;

    if(!result)
    {
        FAIL(TEXT("GetAllCameraData: No camera driver information configured."));
    }

    if(hNotification && !StopDeviceNotifications(hNotification))
    {
        FAIL(TEXT("GetAllCameraData : Failed to stop device notifications."));
        result = FALSE;
    }
    hNotification = NULL;

    if(hMsgQueue && !CloseMsgQueue(hMsgQueue))
    {
        FAIL(TEXT("GetAllCameraData : Failed to close the message queue."));
        result = FALSE;
    }
    hMsgQueue = NULL;

    return result;
}

BOOL
CCameraDriverTest::SetupNULLCameraDriver()
{
    BOOL result = TRUE;
    HKEY hkeyFillLibrary = NULL;

    // if we can open the key, then set up the filler function.
    //  if we can't then we're not using the NULL driver.
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("Drivers\\BuiltIn\\NullCam"), 
                        0, // Reseved. Must == 0.
                        0, // Must == 0.
                        &hkeyFillLibrary) == ERROR_SUCCESS)
    {
        DWORD dwDataType;
        DWORD dwDataSize;
        TCHAR tszPrefix[MAX_DEVCLASS_NAMELEN];
        DWORD dwIndex;
        DWORD dwIndexSize = sizeof(DWORD);

        if(RegQueryValueEx(hkeyFillLibrary,
                            TEXT("Prefix"),
                            NULL,
                            &dwDataType,
                            (LPBYTE) tszPrefix,
                            &dwDataSize) == ERROR_SUCCESS)
        {
            if(RegQueryValueEx(hkeyFillLibrary,
                                TEXT("Index"),
                                NULL,
                                &dwDataType,
                                (LPBYTE) &dwIndex,
                                &dwIndexSize) == ERROR_SUCCESS)
            {
                // combine the prefix and the index to create the device name
                _sntprintf(tszPrefix, dwDataSize, TEXT("%s%d:"), tszPrefix, dwIndex);

                // find the device from the enumeration and set the null driver index
                m_nNullDriverIndex = FindCameraDevice(tszPrefix);
            }
        }
    }

    if(hkeyFillLibrary)
        RegCloseKey(hkeyFillLibrary);

    return result;
}


int
CCameraDriverTest::FindCameraDevice(TCHAR *tszCamDeviceName)
{
    for(int i =0; i < m_nCameraDeviceCount; i++)
    {
        if(0 == _tcscmp(m_ptszCameraDevices[i], tszCamDeviceName))
            return i;
    }
    return -1;
}

BOOL
CCameraDriverTest::GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount)
{
    if(tszCamDeviceName && nEntryCount)
    {
        *tszCamDeviceName = m_ptszCameraDevices;
        *nEntryCount = m_nCameraDeviceCount;
        return TRUE;
    }

    return FALSE;
}

BOOL
CCameraDriverTest::SelectCameraDevice(TCHAR *tszCamDeviceName)
{
    BOOL result = FALSE;
    int nCameraIndex;

    nCameraIndex = FindCameraDevice(tszCamDeviceName);

    if(nCameraIndex != -1)
        result = SelectCameraDevice(nCameraIndex);

    return result;
}

BOOL
CCameraDriverTest::SelectCameraDevice(int nCameraDevice)
{
    if(nCameraDevice >= 0 && nCameraDevice < m_nCameraDeviceCount)
    {
        m_nSelectedCameraDevice = nCameraDevice;
        m_bUsingNULLDriver = (nCameraDevice == m_nNullDriverIndex);
        return TRUE;
    }

    return FALSE;
}

int
CCameraDriverTest::GetNumberOfFramesProcessed(SHORT iPinType)
{
    if(STREAM_PREVIEW > iPinType || STREAM_STILL < iPinType)
        return CSSTATE_STOP;

    return camStream[iPinType].GetNumberOfFramesProcessed();
}
