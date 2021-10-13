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

TESTPROCAPI tst_IOCTL_DISK_READ(UINT uMsg,TPPARAM tpParam, 
								LPFUNCTION_TABLE_ENTRY lpFTE )
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
	pMyTest = new IOCTL_DISK_READ_TEST(g_hDisk,g_pKato,g_storageDeviceInfo.dwDeviceType);
	if(!pMyTest)
	{
		g_pKato->Log(LOG_FAIL,TEXT("FAILED to allocate a new IOCTL_DISK_READ_TEST class.\n"));
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

BOOL IOCTL_DISK_READ_TEST::CustomCleanup()
{
	//cleanup
	if(m_dataBuffer)
	{
		delete[] m_dataBuffer;
		m_dataBuffer = NULL;
	}
	return TRUE;
}
///This is where we can add new items to loop through, and add our own custom params
BOOL IOCTL_DISK_READ_TEST::CustomFill(void)
{
	
	BOOL bRet = Fill();
	
	PPARAMETER tParam = NULL;
	tParam = loopList.FindParameterByName(PARAM_nStartSector); 	
	if(!tParam)
	{
		bRet = FALSE;
		goto EXIT;
	}
	m_dwStartSector = GetDWORD(tParam->value, inData.nActualOutBufferSize);
	tParam = loopList.FindParameterByName(PARAM_nNumSectors); 	
	if(!tParam)
	{
		bRet = FALSE;
		goto EXIT;
	}
	m_dwNumSectors = GetDWORD(tParam->value,m_dwStartSector);
	tParam = loopList.FindParameterByName(PARAM_nNumSGBufs); 
	if(!tParam)
	{
		bRet = FALSE;
		goto EXIT;
	}
	m_dwNumSGBufs = GetDWORD(tParam->value, inData.nActualOutBufferSize);
EXIT:
	return bRet;
}

BOOL IOCTL_DISK_READ_TEST::LoadCustomParameters(void)
{
#define NUM_BUFFER_SIZES 4
#define NUM_START_SECTOR_VALUES 5
#define NUM_NUM_SECTOR_VALUES 3
#define NUM_NUM_SG_VALUES 1
#define NUM_SGBUFFER_TYPES 2
#define NUM_DATA_BUFFER_TYPES 2
	
	TCHAR* g_BufferSizes [NUM_BUFFER_SIZES] = {_T("gt_size_of_sg_req"),_T("size_of_sg_req"),_T("zero"),_T("lt_size_of_sg_req")};
	TCHAR* g_SGBufferTypes [NUM_SGBUFFER_TYPES] = {_T("NULL"),_T("ALPHA")};
	TCHAR* g_DataBufferTypes[NUM_DATA_BUFFER_TYPES] = {_T("NOT_NULL"),_T("NULL")};
	PGROUP tempGroup = new GROUP(PARAM_nInBufferSize);
	PGROUP tempGroup1 = new GROUP(PARAM_nOutBufferSize);
	if(!tempGroup || !tempGroup1)
	{
		if(tempGroup)
			delete tempGroup;
		if(tempGroup1)
			delete tempGroup1;
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate new GROUP for nInBufferSize or nOutBufferSize"));
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
	
	tempGroup = new GROUP(PARAM_DataBuffer);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate new GROUP for DataBuffer\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_DATA_BUFFER_TYPES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_DataBuffer,g_DataBufferTypes[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	
	TCHAR* g_StartSectorValues[NUM_START_SECTOR_VALUES] = {_T("random_sector"),_T("last_sector"),_T("one"),_T("invalid_sector"),_T("zero")};
	TCHAR* g_NumSectorValues[NUM_NUM_SECTOR_VALUES] = {_T("valid_sector_count"),_T("zero"),_T("past_disk_end")};
	TCHAR* g_NumSGValues[NUM_NUM_SG_VALUES] = {_T("one")};
	tempGroup = new GROUP(PARAM_nStartSector);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate new GROUP for nStartSector\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_START_SECTOR_VALUES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_nStartSector,g_StartSectorValues[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	
	tempGroup = new GROUP(PARAM_nNumSectors);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate new GROUP for nNumSectors\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_NUM_SECTOR_VALUES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_nNumSectors,g_NumSectorValues[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	
	tempGroup = new GROUP(PARAM_nNumSGBufs);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("FAILED to allocate new GROUP for nNumSGBufs\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_NUM_SG_VALUES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_nNumSGBufs,g_NumSGValues[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	
	return TRUE;
}
BOOL IOCTL_DISK_READ_TEST::RunIOCTL(void)
{
	BOOL retVal = TRUE;
	try
	{
		if(m_hDevice == NULL || INVALID_HANDLE(m_hDevice))
		{
			m_pKato->Log(LOG_DETAIL, TEXT("Invalid disk handle"));
			return FALSE;
		}
		
		
		
		TCHAR* DataBufferType = loopList.FindParameterByName(PARAM_DataBuffer)->value;
		if(_tcscmp(_T("NULL"),DataBufferType) == 0)
		{
			m_dataBuffer = NULL;
			m_dwDataBufferSize = 0;
		}
		else
		{
			DWORD temp = m_dwNumSectors * g_diskInfo.di_bytes_per_sect;
			m_dataBuffer = GetBuffer(_T("ALPHA"),_T("stack"),temp);
			if(!m_dataBuffer)
			{
				retVal = FALSE;
				goto DONE;
			}
			m_dwDataBufferSize = temp;
			
		}
		PPARAMETER tParam = loopList.FindParameterByName(PARAM_nNumSectors); 	
		
		if(!tParam)
		{
			g_pKato->Log(LOG_DETAIL, _T("WARNING: Couldn't find nNumSectors parameter"));
			
		}
		else
		{
			if(_tcscmp(_T("valid_sector_count"),tParam->value) == 0 && m_dataBuffer != NULL)
			{
				if(m_dwNumSectors * g_diskInfo.di_bytes_per_sect > m_dwDataBufferSize)
				{
					m_dwNumSectors = m_dwDataBufferSize /  g_diskInfo.di_bytes_per_sect;
				}
			}
		}
		
		
		if(inData.lpInBuffer && inData.nInBufferSize >= sizeof(SG_REQ))
		{
			((PSG_REQ)inData.lpInBuffer)->sr_start = m_dwStartSector;
			((PSG_REQ)inData.lpInBuffer)->sr_num_sec = m_dwNumSectors;
			((PSG_REQ)inData.lpInBuffer)->sr_num_sg = m_dwNumSGBufs;
			((PSG_REQ)inData.lpInBuffer)->sr_sglist[0].sb_buf = (PUCHAR)m_dataBuffer;
			((PSG_REQ)inData.lpInBuffer)->sr_sglist[0].sb_len = m_dwNumSectors * g_diskInfo.di_bytes_per_sect;
		}
		
		cpyData = inData;
		
		DWORD dwWriteControlCode = 0;
		dwWriteControlCode = DISK_IOCTL_WRITE;
		
		cpyData.LastError = GetLastError();
		
		if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, m_dwStartSector, m_dwNumSectors, m_dataBuffer))
		{
			g_pKato->Log(LOG_DETAIL, _T("WARNING: ReadWriteDisk(WRITE) failed at sector %u"), m_dwStartSector);
		}
		
		//now perform the read
		inData.RetVal = DeviceIoControl(g_hDisk, inData.dwIoControlCode, (PSG_REQ)inData.lpInBuffer,  inData.nInBufferSize , NULL, 0, (LPDWORD)inData.lpBytesReturned, NULL);
		inData.LastError = GetLastError();
	}
	catch(...)
	{
		m_pKato->Log(LOG_DETAIL, TEXT("Exception during DeviceIoControl.  Failing.\n"));
		retVal = FALSE;
		goto DONE;
	}
DONE:
	return retVal;
}

BOOL IOCTL_DISK_READ_TEST::ATAPIValidate(void)
{
	BOOL retVal = TRUE;
	TCHAR* lpInBuffer = loopList.FindParameterByName(PARAM_lpInBuffer)->value;
	TCHAR* lpOutBuffer = loopList.FindParameterByName(PARAM_lpOutBuffer)->value;
	TCHAR* BufferType = loopList.FindParameterByName(PARAM_BufferType)->value;
	TCHAR* lpBytesReturned = loopList.FindParameterByName(PARAM_IpBytesReturned)->value;
	TCHAR* nInBufferSize = loopList.FindParameterByName(PARAM_nInBufferSize)->value;
	TCHAR* nOutBufferSize = loopList.FindParameterByName(PARAM_nOutBufferSize)->value;
	TCHAR* numSectors = loopList.FindParameterByName(PARAM_nNumSectors)->value;
	TCHAR* startSector = loopList.FindParameterByName(PARAM_nStartSector)->value;
	TCHAR* dataBuffer = loopList.FindParameterByName(PARAM_DataBuffer)->value;
	
	if(_tcscmp(numSectors,_T("zero")) == 0 || _tcscmp(numSectors,_T("invalid_sector")) == 0 || _tcscmp(numSectors,_T("past_disk_end")) == 0
		|| _tcscmp(lpInBuffer,_T("NULL")) == 0 || _tcscmp(dataBuffer,_T("NULL")) == 0 || _tcscmp(nInBufferSize,_T("zero")) == 0 ||
		_tcscmp(nInBufferSize,_T("lt_size_of_sg_req")) == 0 || _tcscmp(nInBufferSize,_T("gt_size_of_sg_req")) == 0 || _tcscmp(startSector,_T("invalid_sector")) == 0)
	{
		if(((_tcscmp(numSectors,_T("past_disk_end")) == 0 || (_tcscmp(startSector,_T("invalid_sector")) == 0) && _tcscmp(numSectors,_T("zero")) != 0)) && _tcscmp(dataBuffer,_T("NULL")) != 0 && ((_tcscmp(nInBufferSize,_T("zero")) != 0) && (_tcscmp(nInBufferSize,_T("lt_size_of_sg_req")) != 0 && (_tcscmp(nInBufferSize,_T("gt_size_of_sg_req")) != 0) 
			&& (_tcscmp(lpInBuffer,_T("NULL")) != 0))))
		{
			if(dwDeviceType & STORAGE_DEVICE_TYPE_PCCARD)
			{
				if(inData.LastError != ERROR_INVALID_PARAMETER)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
					retVal = FALSE;
				}
			}
			else
			{
				if(_tcscmp(numSectors,_T("past_disk_end")) == 0 || _tcscmp(startSector,_T("invalid_sector")) == 0)
				{
					if(inData.LastError != ERROR_SECTOR_NOT_FOUND && inData.LastError != ERROR_INVALID_PARAMETER)
					{
						//fail
						m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_SECTOR_NOT_FOUND && inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
						retVal = FALSE;
					} 
				}
				else if(inData.LastError != ERROR_GEN_FAILURE && inData.LastError != ERROR_INVALID_PARAMETER)
				{
					//fail
					m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_GEN_FAILURE or ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
					retVal = FALSE;
				}
			}
		}
		else if(_tcscmp(nInBufferSize,_T("lt_size_of_sg_req")) == 0 || _tcscmp(nInBufferSize,_T("zero")) == 0)
		{
			if(inData.LastError != ERROR_GEN_FAILURE && inData.LastError != ERROR_INVALID_PARAMETER)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_GEN_FAILURE or ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
				retVal = FALSE;
			}
		}
		else if(_tcscmp(nInBufferSize,_T("gt_size_of_sg_req")) == 0)
		{
			if(inData.LastError != ERROR_INVALID_PARAMETER)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
				retVal = FALSE;
			}
		}
		else
		{
			if(inData.LastError != ERROR_INVALID_PARAMETER)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.LastError != ERROR_INVALID_PARAMETER as expected: %d\n"),inData.LastError);
				retVal = FALSE;
			}
		}
		if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
		//bugbug in atapi
		if(inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL)
			((PSG_REQ)inData.lpInBuffer)->sr_status = ((PSG_REQ)cpyData.lpInBuffer)->sr_status;
		//lpInBuffer should remain unchanged
		if((inData.lpInBuffer != NULL && cpyData.lpInBuffer != NULL) && memcmp(inData.lpInBuffer,cpyData.lpInBuffer,inData.nActualInBufferSize) != 0)
		{
			//fail
			m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
			retVal = FALSE;
		}
		
		//lpBytesReturned should always be 0
		if(g_fOpenAsStore)
		{
			if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != *((LPDWORD)cpyData.lpBytesReturned))
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned changed unexpectedly. Got %d, expected %d\n"),*((LPDWORD)inData.lpBytesReturned),*((LPDWORD)cpyData.lpBytesReturned));
				m_pKato->Log(LOG_FAIL,_T("cpyData.lpBytesReturned is %d\n"),*((LPDWORD)cpyData.lpBytesReturned));
				retVal = FALSE;
			}
		}
		else
		{
			if(inData.lpBytesReturned && *((LPDWORD)inData.lpBytesReturned) != 0)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpBytesReturned is %d, not %d\n"),*((LPDWORD)inData.lpBytesReturned),0);
				m_pKato->Log(LOG_FAIL,_T("cpyData.lpBytesReturned is %d\n"),*((LPDWORD)cpyData.lpBytesReturned));
				retVal = FALSE;
			}
		}
	}
		else
		{
			//should be valid
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
			if((inData.lpOutBuffer != NULL && cpyData.lpOutBuffer != NULL) && memcmp(inData.lpOutBuffer,cpyData.lpOutBuffer,inData.nActualOutBufferSize) != 0)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpOutBuffer changed unexpectedly\n"));
				retVal = FALSE;
			}
			DWORD BytesToValidate = m_dwNumSectors * g_diskInfo.di_bytes_per_sect;
			if((inData.lpInBuffer != NULL && m_dataBuffer != NULL && ((PSG_REQ)inData.lpInBuffer)->sr_sglist[0].sb_buf != NULL) && memcmp(((PSG_REQ)inData.lpInBuffer)->sr_sglist[0].sb_buf,m_dataBuffer,BytesToValidate) != 0)
			{
				//fail
				m_pKato->Log(LOG_FAIL,_T("inData.lpInBuffer changed unexpectedly\n"));
				retVal = FALSE;
			}
		}
		return retVal;
}
BOOL IOCTL_DISK_READ_TEST::Validate(void)
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

