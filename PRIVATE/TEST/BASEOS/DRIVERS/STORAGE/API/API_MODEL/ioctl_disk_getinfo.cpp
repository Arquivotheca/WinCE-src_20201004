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

TESTPROCAPI tst_IOCTL_DISK_GETINFO(UINT uMsg,TPPARAM tpParam, 
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
	int bRet = FALSE;
	DISK_INFO originalInfo = {0};
	GetDiskInfo(g_hDisk,_T(""),&originalInfo);
	//set some bogus DISK_INFO information
	g_diskInfo.di_total_sectors = DI_TOTAL_SECTORS;
	g_diskInfo.di_bytes_per_sect = DI_BYTES_PER_SECT;
	g_diskInfo.di_cylinders = DI_CYLINDERS;
	g_diskInfo.di_heads = DI_HEADS;
	g_diskInfo.di_sectors = DI_SECTORS;
	g_diskInfo.di_flags = DI_FLAGS;
	SetDiskInfo(g_hDisk,NULL,&g_diskInfo);
	
	//somewhere up here, we should check to see if this is a valid IOCTL 
	//for the driver being tested.
	
	DRIVER_TEST *pMyTest;
	pMyTest = new IOCTL_DISK_GETINFO_TEST(g_hDisk,g_pKato,g_storageDeviceInfo.dwDeviceType);
	if(!pMyTest)
	{
		g_pKato->Log(LOG_FAIL,_T("FAILED to allocate a new IOCTL_DISK_GETINFO_TEST class.\n"));
		bRet = FALSE;
		goto EXIT;
	}
	bRet = RunIOCTLCommon(pMyTest); 
    if(pMyTest)
    {
		delete pMyTest;
		pMyTest = NULL;
    }	
EXIT:
	//restore disk info
	SetDiskInfo(g_hDisk,NULL,&originalInfo);
	GetDiskInfo(g_hDisk,_T(""),&g_diskInfo);
	return bRet;
}

///This is where we can add new items to loop through, and add our own custom params
BOOL IOCTL_DISK_GETINFO_TEST::CustomFill(void)
{
	BOOL bRet = Fill();
	return bRet;
}

BOOL IOCTL_DISK_GETINFO_TEST::LoadCustomParameters(void)
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
BOOL IOCTL_DISK_GETINFO_TEST::RunIOCTL(void)
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

BOOL IOCTL_DISK_GETINFO_TEST::ATAPIValidate(void)
{
	BOOL retVal = TRUE;
	TCHAR* lpInBuffer = loopList.FindParameterByName(PARAM_lpInBuffer)->value;
	TCHAR* lpOutBuffer = loopList.FindParameterByName(PARAM_lpOutBuffer)->value;
	TCHAR* BufferType = loopList.FindParameterByName(PARAM_BufferType)->value;
	TCHAR* lpBytesReturned = loopList.FindParameterByName(PARAM_IpBytesReturned)->value;
	TCHAR* nInBufferSize = loopList.FindParameterByName(PARAM_nInBufferSize)->value;
	TCHAR* nOutBufferSize = loopList.FindParameterByName(PARAM_nOutBufferSize)->value;

	if((_tcscmp(lpInBuffer,_T("NULL")) != 0 && _tcscmp(nInBufferSize,_T("size_of_disk_info")) != 0))
	{
		if(inData.LastError != ERROR_INVALID_PARAMETER)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
			retVal = FALSE;
		}
		if((inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL) && (memcmp(inData.lpInBuffer,cpyData.lpInBuffer,inData.nActualInBufferSize) != 0))
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
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
		if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
			retVal = FALSE;
		}
	}
	else if((_tcscmp(lpOutBuffer,_T("NULL")) != 0 && _tcscmp(nOutBufferSize,_T("size_of_disk_info")) == 0))
	{
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
			if(inData.LastError != cpyData.LastError)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError changed unexpectedly\n"));
				retVal = FALSE;
			}
		}
		if(inData.lpOutBuffer)
		{
			PDISK_INFO curInfo = ((PDISK_INFO)inData.lpOutBuffer);
				if(curInfo->di_total_sectors !=DI_TOTAL_SECTORS ||
			   curInfo->di_bytes_per_sect != DI_BYTES_PER_SECT ||
			   curInfo->di_cylinders != DI_CYLINDERS ||
			   curInfo->di_heads != DI_HEADS ||
			   curInfo->di_sectors !=DI_SECTORS ||
			   curInfo->di_flags != DI_FLAGS)
			{
				m_pKato->Log(LOG_FAIL,_T("lpOutBuffer does not contain valid DISK_INFO\n"));
				retVal = FALSE;
			}
		}
		if((inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL) && memcmp(inData.lpInBuffer,cpyData.lpInBuffer,inData.nActualInBufferSize) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
		if(inData.RetVal != TRUE)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.retVal is not TRUE as expected\n"));
			retVal = FALSE;
		}
		if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != sizeof(DISK_INFO))
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),sizeof(DISK_INFO));
			retVal = FALSE;
		}
	}
	else if((_tcscmp(lpInBuffer,_T("NULL")) != 0) && _tcscmp(nInBufferSize,_T("size_of_disk_info")) == 0 
		&& (_tcscmp(lpOutBuffer,_T("NULL")) == 0))
	{
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
			if(inData.LastError != cpyData.LastError)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError changed unexpectedly\n"));
				retVal = FALSE;
			}
		}
		if(inData.lpInBuffer)
		{
			PDISK_INFO curInfo = ((PDISK_INFO)inData.lpInBuffer);
			if(curInfo->di_total_sectors !=DI_TOTAL_SECTORS ||
			   curInfo->di_bytes_per_sect != DI_BYTES_PER_SECT ||
			   curInfo->di_cylinders != DI_CYLINDERS ||
			   curInfo->di_heads != DI_HEADS ||
			   curInfo->di_sectors !=DI_SECTORS ||
			   curInfo->di_flags != DI_FLAGS)
			{
				m_pKato->Log(LOG_FAIL,_T("lpOutBuffer does not contain valid DISK_INFO\n"));
				retVal = FALSE;
			}
		}
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
		if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != sizeof(DISK_INFO))
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),sizeof(DISK_INFO));
			retVal = FALSE;
		}
	}
	else if((_tcscmp(lpInBuffer,_T("NULL")) == 0 || _tcscmp(nInBufferSize,_T("size_of_disk_info")) != 0) ||
		(_tcscmp(lpOutBuffer,_T("NULL")) == 0 || _tcscmp(nOutBufferSize,_T("size_of_disk_info")) != 0))
	{
		if(inData.LastError != ERROR_INVALID_PARAMETER)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
			retVal = FALSE;
		}
		if((inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL) && (memcmp(inData.lpInBuffer,cpyData.lpInBuffer,inData.nActualInBufferSize) != 0))
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
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
		if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
			retVal = FALSE;
		}
	}
	else
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("Unexpected case in %s\n"),__FUNCTION__);
		retVal = FALSE;
	}
	return retVal;
}

BOOL IOCTL_DISK_GETINFO_TEST::CDRomValidate(void)
{ 
	BOOL retVal = TRUE;
	if(inData.LastError != ERROR_BAD_COMMAND || inData.RetVal != 0 || (inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0))
	{
		retVal = false;
	}
	return retVal;
}

BOOL IOCTL_DISK_GETINFO_TEST::Validate(void)
{
	if(dwDeviceType & STORAGE_DEVICE_TYPE_REMOVABLE_MEDIA 
	&& dwDeviceType & STORAGE_DEVICE_TYPE_ATAPI && dwDeviceType & STORAGE_DEVICE_TYPE_PCIIDE)
	{
		return CDRomValidate();
	}
	else if(dwDeviceType & STORAGE_DEVICE_TYPE_ATAPI || dwDeviceType & STORAGE_DEVICE_TYPE_ATA)
	{
		return ATAPIValidate();
	}
	else
	{
		return FALSE;
	}
}

