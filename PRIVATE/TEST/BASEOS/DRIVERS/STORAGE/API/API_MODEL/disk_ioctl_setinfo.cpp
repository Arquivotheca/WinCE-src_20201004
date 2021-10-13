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

TESTPROCAPI tst_DISK_IOCTL_SETINFO(UINT uMsg,TPPARAM tpParam, 
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
	pMyTest = new DISK_IOCTL_SETINFO_TEST(g_hDisk,g_pKato,g_storageDeviceInfo.dwDeviceType);
	if(!pMyTest)
	{
		g_pKato->Log(LOG_FAIL,_T("FAILED to allocate a new DISK_IOCTL_SETINFO_TEST class.\n"));
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
BOOL DISK_IOCTL_SETINFO_TEST::CustomFill(void)
{
	BOOL bRet = Fill();
	return bRet;
}

BOOL DISK_IOCTL_SETINFO_TEST::LoadCustomParameters(void)
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
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate buffer for nInBufferSize or nOutBufferSize"));
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
BOOL DISK_IOCTL_SETINFO_TEST::RunIOCTL(void)
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

BOOL DISK_IOCTL_SETINFO_TEST::ATAPIValidate(void)
{
	BOOL retVal = TRUE;
	TCHAR* lpInBuffer = loopList.FindParameterByName(PARAM_lpInBuffer)->value;
	TCHAR* lpOutBuffer = loopList.FindParameterByName(PARAM_lpOutBuffer)->value;
	TCHAR* BufferType = loopList.FindParameterByName(PARAM_BufferType)->value;
	TCHAR* lpBytesReturned = loopList.FindParameterByName(PARAM_IpBytesReturned)->value;
	TCHAR* nInBufferSize = loopList.FindParameterByName(PARAM_nInBufferSize)->value;
	TCHAR* nOutBufferSize = loopList.FindParameterByName(PARAM_nOutBufferSize)->value;
	//lpOutBuffer should remain unchanged
	if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
		retVal = FALSE;
	}
	//lpInBuffer should remain unchanged
	if((inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL) && memcmp(inData.lpInBuffer,cpyData.lpInBuffer,inData.nActualInBufferSize) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
		retVal = FALSE;
	}
	//lpBytesReturned should always be 0
	if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0)
	{
		//fail
		m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
		retVal = FALSE;
	}
	
	if((_tcscmp(lpInBuffer,_T("NULL")) != 0 && _tcscmp(nInBufferSize,_T("size_of_disk_info")) == 0))
	{
		PDISK_INFO curInfo = ((PDISK_INFO)inData.lpInBuffer);
		LPVOID temp = lpInBuffer;
		DISK_INFO tempInfo = {0};
		//nothing to do for CDROM, since we can't verify that it actually worked with GetDiskInfo
		if(dwDeviceType & STORAGE_DEVICE_TYPE_REMOVABLE_MEDIA 
		&& dwDeviceType & STORAGE_DEVICE_TYPE_ATAPI && dwDeviceType & STORAGE_DEVICE_TYPE_PCIIDE)
		{
			retVal = TRUE;
		}
		else
		{

			if(!GetDiskInfo(g_hDisk,NULL,&tempInfo))
			{
					//fail
					m_pKato->Log(LOG_FAIL,_T("GetDiskInfo failed, so we can't verify if SetDiskInfo worked.\n"));
					retVal = FALSE;
			}
			else
			{
				((PDISK_INFO)inData.lpInBuffer)->di_flags |= DISK_INFO_FLAG_PAGEABLE;
				((PDISK_INFO)inData.lpInBuffer)->di_flags &= ~DISK_INFO_FLAG_UNFORMATTED;
				if(memcmp(inData.lpInBuffer,&tempInfo,inData.nInBufferSize) != 0)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("GetDiskInfo returned a different value than what we just tried to set\n with SetDiskInfo.  This could indicate a problem in either\nGetDiskInfo or SetDiskInfo\n"));
					retVal = FALSE;
				}
			}
		}
		//the valid case
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
		if(inData.RetVal != TRUE)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.retVal is not TRUE as expected\n"));
			retVal = FALSE;
		}
	}
	else
	{
		//the invalid case
		if(inData.LastError != ERROR_INVALID_PARAMETER)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
			retVal = FALSE;
		}
		if(inData.RetVal != FALSE)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.retVal is not FALSE as expected\n"));
			retVal = FALSE;
		}
	}
	return retVal;
}
BOOL DISK_IOCTL_SETINFO_TEST::Validate(void)
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

