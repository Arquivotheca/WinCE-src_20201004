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
TESTPROCAPI tst_IOCTL_DISK_GET_STORAGEID(UINT uMsg,TPPARAM tpParam, 
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
	pMyTest = new IOCTL_DISK_GET_STORAGEID_TEST(g_hDisk,g_pKato,g_storageDeviceInfo.dwDeviceType);

	int bRet = RunIOCTLCommon(pMyTest); 
    	if(pMyTest)
    	{
		delete pMyTest;
		pMyTest = NULL;
    	}	
	return bRet;
}

///This is where we can add new items to loop through, and add our own custom params
BOOL IOCTL_DISK_GET_STORAGEID_TEST::CustomFill(void)
{	
	BOOL bRet = Fill();
		STORAGE_IDENTIFICATION si = {0};
		DWORD BytesReturned = 0;
		DeviceIoControl(g_hDisk,IOCTL_DISK_GET_STORAGEID,NULL,0,
		&si,sizeof(STORAGE_IDENTIFICATION),&BytesReturned,NULL);

	m_ExpectedBytes = si.dwSize;
	return bRet;
}

BOOL IOCTL_DISK_GET_STORAGEID_TEST::LoadCustomParameters(void)
{
	#define NUM_BUFFER_SIZES 6
	TCHAR* g_BufferSizes [NUM_BUFFER_SIZES] = {_T("size_of_STORAGE_IDENTIFICATION"),_T("zero"),_T("lt_size_of_STORAGE_IDENTIFICATION"),_T("size_of_EXPECTED_STORAGE_IDENTIFICATION"),_T("gt_size_of_EXPECTED_STORAGE_IDENTIFICATION"),_T("lt_size_of_EXPECTED_STORAGE_IDENTIFICATION"),};
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
BOOL IOCTL_DISK_GET_STORAGEID_TEST::RunIOCTL(void)
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
		PSTORAGE_IDENTIFICATION pSI = (PSTORAGE_IDENTIFICATION)inData.lpOutBuffer;
		inData.LastError = GetLastError();
	}
	catch(...)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("Exception during DeviceIoControl.  Failing.\n"));
		return FALSE;
	}
	return TRUE;
}

BOOL IOCTL_DISK_GET_STORAGEID_TEST::ATAPIValidate(void)
{
	BOOL retVal = TRUE;
	TCHAR* lpInBuffer = loopList.FindParameterByName(PARAM_lpInBuffer)->value;
	TCHAR* lpOutBuffer = loopList.FindParameterByName(PARAM_lpOutBuffer)->value;
	TCHAR* BufferType = loopList.FindParameterByName(PARAM_BufferType)->value;
	TCHAR* lpBytesReturned = loopList.FindParameterByName(PARAM_IpBytesReturned)->value;
	TCHAR* nInBufferSize = loopList.FindParameterByName(PARAM_nInBufferSize)->value;
	TCHAR* nOutBufferSize = loopList.FindParameterByName(PARAM_nOutBufferSize)->value;

	if(_tcscmp(lpOutBuffer,_T("NULL")) != 0 && _tcscmp(nOutBufferSize,_T("zero")) != 0 && _tcscmp(nOutBufferSize,_T("lt_size_of_STORAGE_IDENTIFICATION")))
	{
		if(_tcscmp(nOutBufferSize,_T("size_of_EXPECTED_STORAGE_IDENTIFICATION")) == 0 || _tcscmp(nOutBufferSize,_T("gt_size_of_EXPECTED_STORAGE_IDENTIFICATION")) == 0)
		{
			
			//valid
			/*if(inData.lpOutBuffer && _tcscmp((TCHAR*)inData.lpOutBuffer,expectedString) != 0)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer equals %s, instead of Hard Disk\n"),(TCHAR*)inData.lpOutBuffer);
				retVal = FALSE;
			}*/
			if(inData.RetVal != TRUE)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.retVal is not TRUE as expected\n"));
				retVal = FALSE;
			}
			if(inData.lpBytesReturned  && *((LPDWORD)inData.lpBytesReturned) != m_ExpectedBytes)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),sizeof(STORAGE_IDENTIFICATION));
				retVal = FALSE;
			}
			//bugbug
			if(g_fOpenAsStore)
			{
				if(inData.LastError != 0)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.LastError is not zero, as expected.  It should be zero because we opened the device through the Storage Manager\n"));
					retVal = FALSE;
				}
			}
			else
			{
				if(inData.LastError != ERROR_SUCCESS)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.LastError changed unexpectedly. Expecting %d, got %d\n"),ERROR_SUCCESS,inData.LastError);
					retVal = FALSE;
				}
			}
		}
		else
		{
			//invalid
			if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
				retVal = FALSE;
			}
			if(inData.RetVal != FALSE)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.retVal is not FALSE as expected\n"));
				retVal = FALSE;
			}
			if(_tcscmp(_T("size_of_STORAGE_IDENTIFICATION"),nOutBufferSize) == 0)
			{
				if(inData.lpBytesReturned  && *((LPDWORD)inData.lpBytesReturned) != sizeof(STORAGE_IDENTIFICATION))
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),sizeof(STORAGE_IDENTIFICATION));
					retVal = FALSE;
				}
			}
			else if(_tcscmp(_T("zero"),nOutBufferSize) == 0)
			{
				if(inData.lpBytesReturned  && *((LPDWORD)inData.lpBytesReturned) != 0)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
					retVal = FALSE;
				}
			}
			else
			{ //returned some bytes, but not enough
				if(inData.lpBytesReturned  && *((LPDWORD)inData.lpBytesReturned) != inData.nOutBufferSize)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),inData.nOutBufferSize);
					retVal = FALSE;
				}
			}
			if(inData.LastError != ERROR_INSUFFICIENT_BUFFER)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INSUFFICIENT_BUFFER as expected: %d\n"),inData.LastError);
				retVal = FALSE;
			}
		}
	}
	else
	{
		//invalid
		if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
		if(inData.RetVal != FALSE)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.retVal is not FALSE as expected\n"));
			retVal = FALSE;
		}
		if(inData.lpBytesReturned  && *((LPDWORD)inData.lpBytesReturned) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
			retVal = FALSE;
		}
		if(inData.LastError != ERROR_INVALID_PARAMETER)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
			retVal = FALSE;
		}
	}
	return retVal;
}
BOOL IOCTL_DISK_GET_STORAGEID_TEST::Validate(void)
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

