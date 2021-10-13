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

#include "main.h"

TESTPROCAPI tst_IOCTL_DISK_FORMAT_MEDIA(UINT uMsg,TPPARAM tpParam, 
LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(lpFTE);
    	if( TPM_QUERY_THREAD_COUNT == uMsg )
    	{
        	((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        	return SPR_HANDLED;
    	}
    	else if( TPM_EXECUTE != uMsg )
    	{
        	return TPR_NOT_HANDLED;
    	}
	//somewhere up here, we should check to see if this is a valid IOCTL 
	//for the driver being tested.
	
	DRIVER_TEST *pMyTest;
	pMyTest = new IOCTL_DISK_FORMAT_MEDIA_TEST(g_hDisk,g_pKato,g_storageDeviceInfo.dwDeviceType);
	if(!pMyTest)
	{
		g_pKato->Log(LOG_FAIL,_T("FAILED to allocate a new IOCTL_DISK_FORMAT_MEDIA_TEST class.\n"));
		return FALSE;
	}
	int bRet = RunIOCTLCommon(pMyTest); 
    	if(pMyTest)
    	{
		delete pMyTest;
		pMyTest = NULL;
    	}	
	return bRet;
}

///This is where we can add new items to loop through, and add our own custom params
BOOL IOCTL_DISK_FORMAT_MEDIA_TEST::CustomFill(void)
{
	BOOL bRet = Fill();
	return bRet;
}

BOOL IOCTL_DISK_FORMAT_MEDIA_TEST::LoadCustomParameters(void)
{
	#define NUM_BUFFER_SIZES 4
	TCHAR* g_BufferSizes [NUM_BUFFER_SIZES] = {_T("size_of_disk_info"),_T("zero"),_T("gt_size_of_disk_info"),_T("lt_size_of_disk_info")};
	PGROUP tempGroup = new GROUP(PARAM_nInBufferSize);
	PGROUP tempGroup1 = new GROUP(PARAM_nOutBufferSize);
	if(!tempGroup || !tempGroup1)
	{
		if(tempGroup)
			delete tempGroup;
		if(tempGroup1)
			delete tempGroup1;
		return FALSE;
	}
	for(int i = 0; i < NUM_BUFFER_SIZES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_nInBufferSize,g_BufferSizes[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
		PPARAMETER tempParam1 = new PARAMETER(PARAM_nOutBufferSize,g_BufferSizes[i]);
		tempGroup1->m_ParameterList.AddTail(tempParam1);
	}
	paramGroupList.AddTail(tempGroup);
	paramGroupList.AddTail(tempGroup1);
	
	return TRUE;
}
BOOL IOCTL_DISK_FORMAT_MEDIA_TEST::RunIOCTL(void)
{
	try
	{
		if(m_hDevice == NULL || INVALID_HANDLE(m_hDevice))
		{
			m_pKato->Log(LOG_DETAIL, TEXT("Invalid disk handle"));
			return FALSE;
		}
		cpyData.LastError = GetLastError();
		inData.RetVal = DeviceIoControl(m_hDevice,inData.dwIoControlCode,inData.lpInBuffer,inData.nInBufferSize,
		inData.lpOutBuffer,inData.nOutBufferSize,(LPDWORD)inData.lpBytesReturned,inData.lpOverLapped);
		inData.LastError = GetLastError();
	}
	catch(...)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("Exception during DeviceIoControl.  Failing.\n"));
		return FALSE;
	}
	return TRUE;
}

BOOL IOCTL_DISK_FORMAT_MEDIA_TEST::ATAPIValidate(void)
{
	BOOL retVal = TRUE;
	TCHAR* lpInBuffer = loopList.FindParameterByName(PARAM_lpInBuffer)->value;
	TCHAR* lpOutBuffer = loopList.FindParameterByName(PARAM_lpOutBuffer)->value;
	TCHAR* BufferType = loopList.FindParameterByName(PARAM_BufferType)->value;
	TCHAR* lpBytesReturned = loopList.FindParameterByName(PARAM_IpBytesReturned)->value;
	TCHAR* nInBufferSize = loopList.FindParameterByName(PARAM_nInBufferSize)->value;
	TCHAR* nOutBufferSize = loopList.FindParameterByName(PARAM_nOutBufferSize)->value;

	if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
		retVal = FALSE;
	}
	if(inData.RetVal != TRUE)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.retVal is not TRUE as expected\n"));
		retVal = FALSE;
	}
	if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
		retVal = FALSE;
	}
	if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
		retVal = FALSE;
	}
	if(inData.LastError != ERROR_SUCCESS)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
		retVal = FALSE;
	}
	return retVal;
}
BOOL IOCTL_DISK_FORMAT_MEDIA_TEST::Validate(void)
{
	if(dwDeviceType & STORAGE_DEVICE_TYPE_ATAPI || dwDeviceType & STORAGE_DEVICE_TYPE_ATA)
	{
		return ATAPIValidate();
	}
	else
	{
		return FALSE;
	}
}

