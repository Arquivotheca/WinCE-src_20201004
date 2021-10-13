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
#include <camera.h>
#include "logging.h"
#include "ccamerastreamtest.h"

typedef class CCameraDriverTest 
{
public :
    CCameraDriverTest();
    ~CCameraDriverTest();
    BOOL Cleanup();

    BOOL DetermineCameraAvailability();
    BOOL SelectCameraDevice(TCHAR *tszCamDeviceName);
    BOOL SelectCameraDevice(int nCameraDevice);
    BOOL GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount);
    BOOL InitializeDriver();
    BOOL TestDeviceIOControl(DWORD dwIoControlCode,
                                LPVOID lpInBuffer, DWORD nInBufferSize,
                                LPVOID lpOutBuffer, DWORD nOutBufferSize,
                                LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
    BOOL StreamDeviceIOControl(ULONG ulPinType,
                                DWORD dwIoControlCode,
                                LPVOID lpInBuffer, DWORD nInBufferSize,
                                LPVOID lpOutBuffer, DWORD nOutBufferSize,
                                LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

    BOOL FetchAccessFlags();
    BOOL GetCurrentPropertyValue(    LPVOID lpDescriptorType, DWORD DescriptorSize, LPVOID lpValueBuf, DWORD ValueBufLen);
    BOOL FetchBasicSupport();
    BOOL FetchDefaultValues();
    BOOL GetValueBuffer(PCSPROPERTY_DESCRIPTION *lpPropDesc);
    BOOL IsReadAllowed();
    BOOL IsWriteAllowed();
    BOOL IsValueValid(LPVOID lpBuff);
    BOOL IsPropertySetSupported();
    BOOL HasDefaultValue();
    HRESULT GetFirstValue(PVALUEDATA pValData);
    HRESULT GetNextValue(PVALUEDATA pValData);
    BOOL GetDataRanges(SHORT iPinType, PCSMULTIPLE_ITEM *pCSMultipleItem);
    VOID PrepareTestEnvironment (GUID guidPropertySet, ULONG ulProperty, ULONG ulPropSetDataType );
    BOOL SetPropertyValue(    LPVOID lpDescriptorType, DWORD DescriptorSize, LPVOID lpValueBuf, DWORD ValueBufLen);
    BOOL SetState(SHORT iPinType, CSSTATE sState);
    BOOL TriggerCaptureEvent(SHORT iPinType);

    CSSTATE GetState(SHORT iPinType);
    int GetNumSupportedFormats(SHORT iPinType);
    BOOL GetFormatInfo(SHORT iPinType, int nVideoFormat, PCS_DATARANGE_VIDEO pcsVideoFormat);
    BOOL AllocateBuffers(SHORT iPinType);
    BOOL ReleaseBuffers(SHORT iPinType);
    BOOL CreateStream(SHORT iPinType, HWND hwnd, RECT rc);
    BOOL SetupStream(SHORT iPinType);
    BOOL CleanupStream(SHORT iPinType);
    BOOL SelectVideoFormat(SHORT iPinType, int nVideoFormat);
    BOOL AvailablePinInstance(SHORT iPinType);
    BOOL SetArtificialDelay(SHORT iPinType, int nDelay);
    int     GetNumberOfFramesProcessed(SHORT iPinType);

private :
    TCHAR * GetStreamName(SHORT iPinType);
    TCHAR * GetDeviceName(SHORT iPinType);

    ULONG MatchPin(GUID guidPin);
    BOOL GetPinCTypes(ULONG *pulCTypes);
    BOOL GetNextMemberItem(PVALUEDATA pValData);
    BOOL FindInSteppedRanges(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers);
    BOOL FindInRanges(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers);

    BOOL FindInValues(LPVOID lpVoid, PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader, LPVOID lpMembers);

    int FindCameraDevice(TCHAR *tszCamDeviceName);
    BOOL GetAllCameraData();
    BOOL SetupNULLCameraDriver();

    CAMERASTREAMTEST camStream[ MAX_STREAMS ];
    ULONG m_ulPinId [ MAX_STREAMS ];
    GUID m_PinGuid [ MAX_STREAMS ];
    PCSMULTIPLE_ITEM m_pCSMultipleItem[MAX_STREAMS]; 
    TCHAR *m_tszDeviceName[MAX_STREAMS];
    TCHAR *m_tszStreamName[MAX_STREAMS];
    PCSDATAFORMAT m_pCSDataFormat[MAX_STREAMS];

    HANDLE m_hCamDriver;
    ULONG m_ulNumOfPinTypes;

    GUID m_PropertySetGuid;
    ULONG m_ulPropertyID;
    ULONG m_ulAccessFlags;
    ULONG m_ulPropertySetDataType;
    ULONG m_lCurrentMemberListCounter;
    ULONG m_lCurrentMembersCounter;
    LPVOID m_lpData;
    PCSPROPERTY_DESCRIPTION m_pCSPropDescription;
    PCSPROPERTY_MEMBERSHEADER m_pCSPropMemberHeader;
    PCSPROPERTY_MEMBERSLIST m_pCSPropMemberList;

    TCHAR **m_ptszCameraDevices;
    int m_nCameraDeviceCount;
    int m_nSelectedCameraDevice;
    BOOL m_bUsingNULLDriver;
    int m_nNullDriverIndex;
} CAMERADRIVERTEST, *PCAMERADRIVERTEST, CAMERAPROPERTYTEST, *PCAMERAPROPERTYTEST;

