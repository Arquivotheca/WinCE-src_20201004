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

#include "ParameterFuncs.h"
#include "main.h"

TCHAR* DRIVER_TEST::GetBuffer(const TCHAR* value,const TCHAR* BufferType,DWORD & dwBufferSize)
{
	dwBufferSize = 0;
	if(value == NULL)
		return NULL;
	TCHAR* tempBuffer = NULL;
	if(_tcscmp(value,TEXT("NULL")) == 0)
	{
		return NULL;
	}
	else if(_tcscmp(value,TEXT("NOT_NULL")) == 0)
	{
		dwBufferSize = rand() % MAX_BUF  + MIN_BUF;
		dwBufferSize = dwBufferSize + dwBufferSize % sizeof(DWORD);
		tempBuffer = new TCHAR[dwBufferSize];
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		MakeJunkBuffer(tempBuffer,dwBufferSize);
		return tempBuffer;
	}
	else if(_tcscmp(value,TEXT("size_of_buffer")) == 0)
	{
		DWORD* dwordBuf = new DWORD;
		//(DWORD*)tempBuffer = new DWORD;
		if(!dwordBuf)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		*dwordBuf = dwBufferSize;
		return (TCHAR*)dwordBuf;
	}
	else if(_tcscmp(value,TEXT("DISK_INFO_VALID")) == 0)
	{
		tempBuffer = (TCHAR*)new DISK_INFO;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		dwBufferSize = sizeof(DISK_INFO);
	}
	else if(_tcscmp(value,TEXT("PBOOL_TRUE")) == 0)
	{
		tempBuffer = (TCHAR*)new BOOL;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		*tempBuffer = TRUE;
		dwBufferSize = sizeof(BOOL);
	}
	else if(_tcscmp(value,TEXT("PBOOL_FALSE")) == 0)
	{
		tempBuffer = (TCHAR*)new BOOL;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		*tempBuffer = FALSE;
		dwBufferSize = sizeof(BOOL);
	}
	else if(_tcscmp(value,TEXT("FMD_INFO_VALID")) == 0)
	{
		/*tempBuffer = (TCHAR*)new FMDInfo;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		dwBufferSize = sizeof(FMDInfo);*/
	}
	else if(_tcscmp(value,TEXT("DEVICE_INFO_STATUS_VALID")) == 0)
	{
		tempBuffer = (TCHAR*)new STORAGEDEVICEINFO;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		((PSTORAGEDEVICEINFO)tempBuffer)->cbSize = sizeof(STORAGEDEVICEINFO);
		dwBufferSize = sizeof(STORAGEDEVICEINFO);
	}
	else if(_tcscmp(value,TEXT("DELETE_SECTOR_INFO_VALID")) == 0)
	{
		tempBuffer = (TCHAR*)new DELETE_SECTOR_INFO;
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		dwBufferSize = sizeof(DELETE_SECTOR_INFO);
	}
	
	else if(_tcscmp(value,TEXT("VALID_NAME_BUFFER")) == 0)
	{
		tempBuffer = (TCHAR*)new TCHAR[MAX_PATH];
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Unable to allocate VALID_NAME_BUFFER"));
		}
		else
			dwBufferSize = MAX_PATH;
		
	}
	else if(_tcscmp(value,TEXT("JUNK")) == 0)
	{
		dwBufferSize = rand() % MAX_BUF + MIN_BUF;
		tempBuffer = new TCHAR[dwBufferSize + 1];
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		MakeJunkBuffer(tempBuffer,dwBufferSize);
	}
	else if(_tcscmp(value,TEXT("ALPHA")) == 0)
	{
		dwBufferSize = MAX_SECTORS * SECTOR_SIZE; 
		tempBuffer = new TCHAR[dwBufferSize + 1];
		if(!tempBuffer)
		{
			g_pKato->Log(LOG_FAIL,_T("Failed to allocate ALPHA buffer of size %d\n"),dwBufferSize);
			return NULL;
		}
		MakeAlphaBuffer(tempBuffer,dwBufferSize);
	}
	else if(_tcscmp(value,TEXT("ZERO_BUFFER")) == 0)
	{
		dwBufferSize = MAX_SECTORS * SECTOR_SIZE; 
		tempBuffer = new TCHAR[dwBufferSize + 1];
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		MakeZeroBuffer(tempBuffer,dwBufferSize);
	}
	else if(_tcscmp(value,TEXT("ONE_BUFFER")) == 0)
	{
		dwBufferSize = MAX_SECTORS * SECTOR_SIZE; 
		tempBuffer = new TCHAR[dwBufferSize + 1];
		if(!tempBuffer)
		{
			//this->Logger->LogError(TEXT("Failed to allocate tempBuffer\n"));
			return NULL;
		}
		MakeOneBuffer(tempBuffer,dwBufferSize);
	}
	else
	{
		g_pKato->Log(LOG_DETAIL,_T("WARNING:  Unknown parameter %s passed to GetBuffer.  Returning NULL.\n"),value);
		return NULL;
	}
	return tempBuffer;
}

DWORD DRIVER_TEST::GetDWORD(const TCHAR* value,DWORD dwBufferSize)
{
	if(value == NULL)
		return NULL;

	TCHAR* tempBuffer = NULL;
	if(_tcscmp(value,TEXT("zero")) == 0)
	{
		return 0;
	}
	else if(_tcscmp(value,TEXT("not_zero")) == 0)
	{
		//assumes dwBufferSize is not zero.  Need to fix.
		if(dwBufferSize == 0)
		{
			DWORD temp;
			temp = rand()  % MAX_BUF;
			if(temp == 0)
				temp = 1;
			return temp;
		}
		else
			return dwBufferSize;
	}
	else if(_tcscmp(value,TEXT("AllSectors")) == 0)
	{
		return g_diskInfo.di_total_sectors;
	}
	else if(_tcscmp(value,TEXT("TwoThirdsSectors")) == 0)
	{
		return (g_diskInfo.di_total_sectors / 3) * 2;
	}
	else if(_tcscmp(value,TEXT("OneThirdSectors")) == 0)
	{
		return (g_diskInfo.di_total_sectors / 3);
	}
	else if(_tcscmp(value,TEXT("1")) == 0)
	{
		return 1;
	}
	else if(_tcscmp(value,TEXT("2")) == 0)
	{
		return 2;
	}
	else if(_tcscmp(value,TEXT("4")) == 0)
	{
		return 4;
	}
	else if(_tcscmp(value,TEXT("8")) == 0)
	{
		return 8;
	}
	else if(_tcscmp(value,TEXT("16")) == 0)
	{
		return 16;
	}
	else if(_tcscmp(value,_T("size_of_ATAPI_DISK_NAME")) == 0)
	{
		if(dwDeviceType & STORAGE_DEVICE_TYPE_PCCARD)
		{
			return (_tcslen(ATAPI_CF_DISK_NAME) + 1) * sizeof(TCHAR);
		}
		else
		{
			return (_tcslen(ATAPI_HD_DISK_NAME) + 1) * sizeof(TCHAR);
		}
	}
	else if(_tcscmp(value,_T("gt_size_of_ATAPI_DISK_NAME")) == 0)
	{
		if(dwDeviceType & STORAGE_DEVICE_TYPE_PCCARD)
		{
			return rand() + 1 + ((_tcslen(ATAPI_CF_DISK_NAME) + 1) * sizeof(TCHAR));
		}
		else
		{
			return rand() + 1 + ((_tcslen(ATAPI_HD_DISK_NAME) + 1) * sizeof(TCHAR));
		}
	}
	else if(_tcscmp(value,_T("lt_size_of_ATAPI_DISK_NAME")) == 0)
	{
		DWORD temp;
		if(dwDeviceType & STORAGE_DEVICE_TYPE_PCCARD)
		{
			temp = rand()  % ((_tcslen(ATAPI_CF_DISK_NAME) + 1) * sizeof(TCHAR));
		}
		else
		{
			temp = rand()  % ((_tcslen(ATAPI_HD_DISK_NAME) + 1) * sizeof(TCHAR));
		}
		if(temp == 0)
			temp = 1;
		return temp;	
	}
	else if(_tcscmp(value,_T("size_of_STORAGE_IDENTIFICATION")) == 0)
	{
		return sizeof(STORAGE_IDENTIFICATION);
	}
	else if(_tcscmp(value,_T("lt_size_of_STORAGE_IDENTIFICATION")) == 0)
	{
		DWORD temp = rand()  % sizeof(STORAGE_IDENTIFICATION);
		if(temp == 0)
			temp = 1;
		return temp;	
	}
	else if(_tcscmp(value,_T("size_of_EXPECTED_STORAGE_IDENTIFICATION")) == 0)
	{
		STORAGE_IDENTIFICATION si = {0};
		DWORD BytesReturned = 0;
		DeviceIoControl(g_hDisk,IOCTL_DISK_GET_STORAGEID,NULL,0,
		&si,sizeof(STORAGE_IDENTIFICATION),&BytesReturned,NULL);
		return si.dwSize;
	}
	else if(_tcscmp(value,_T("gt_size_of_EXPECTED_STORAGE_IDENTIFICATION")) == 0)
	{
		STORAGE_IDENTIFICATION si = {0};
		DWORD BytesReturned = 0;
		DeviceIoControl(g_hDisk,IOCTL_DISK_GET_STORAGEID,NULL,0,
		&si,sizeof(STORAGE_IDENTIFICATION),&BytesReturned,NULL);
		return rand() + 1 + si.dwSize;
	}
	else if(_tcscmp(value,_T("lt_size_of_EXPECTED_STORAGE_IDENTIFICATION")) == 0)
	{
		STORAGE_IDENTIFICATION si = {0};
		DWORD BytesReturned = 0;
		DeviceIoControl(g_hDisk,IOCTL_DISK_GET_STORAGEID,NULL,0,
		&si,sizeof(STORAGE_IDENTIFICATION),&BytesReturned,NULL);
		DWORD temp = rand()  % si.dwSize;
		if(temp <= sizeof(STORAGE_IDENTIFICATION))
		{
			temp = sizeof(STORAGE_IDENTIFICATION) + 1;
		}
		return temp;
	}
	else if(_tcscmp(value,TEXT("size_of_bool")) == 0)
	{
		return sizeof(BOOL);
	}
	else if(_tcscmp(value,TEXT("lt_size_of_bool")) == 0)
	{
		return rand() % sizeof(BOOL);
	}
	else if(_tcscmp(value,TEXT("gt_size_of_bool")) == 0)
	{
		return rand() + 1 + sizeof(BOOL);
	}
	else if(_tcscmp(value,TEXT("size_of_buffer")) == 0)
	{
		return dwBufferSize;
	}
	else if(_tcscmp(value,TEXT("lt_size_of_dword")) == 0)
	{
		return rand() % sizeof(DWORD);
	}
	else if(_tcscmp(value,TEXT("gt_size_of_dword")) == 0)
	{
		if(dwBufferSize == 0)
			return (rand() + sizeof(DWORD)) % MAX_BUF;
		else if(dwBufferSize < sizeof(DWORD))
		{
			return sizeof(DWORD);
		}
		else
			return dwBufferSize;
	}
	/*else if(_tcscmp(value,TEXT("gt_size_of_FMDInfo")) == 0)
	{
			if(dwBufferSize > sizeof(FMDInfo))
				return dwBufferSize;
			return (rand() % MAX_BUF) + 1 + sizeof(FMDInfo); //return a value greater than sizeof(FMDInfo)
	}							
	else if(_tcscmp(value,TEXT("lt_size_of_FMDInfo")) == 0)
	{
			if(dwBufferSize < sizeof(FMDInfo))
				return dwBufferSize;
			return (rand() % (sizeof(FMDInfo) -1)); //return a value greater than sizeof(FMDInfo)
	}
	else if(_tcscmp(value,TEXT("size_of_FMDInfo")) == 0)
	{
			return sizeof(FMDInfo); //return a value greater than sizeof(FMDInfo)
	}*/
	else if(_tcscmp(value,TEXT("lt_size_of_buffer")) == 0)
	{
		if(dwBufferSize <= 0)
			return 0;
		else
		{
			DWORD temp = rand() % (dwBufferSize - 1) + 1;
			if(temp == 0)
				temp = 1;
			return temp;//return a value less than sizeof(buffer)
		}
	}
	else if(_tcscmp(value,TEXT("gt_size_of_buffer")) == 0)
	{
		return (rand() % MAX_BUF) + dwBufferSize; //return a value greater than sizeof(buffer)
	}
	else if(_tcscmp(value,TEXT("ERROR_NOT_SUPPORTED")) == 0)
	{
		return ERROR_NOT_SUPPORTED;
	}
	else if(_tcscmp(value,TEXT("ERROR_INVALID_PARAMETER")) == 0)
	{
		return ERROR_INVALID_PARAMETER;
	}
	else if(_tcscmp(value,TEXT("ERROR_SUCCESS")) == 0)
	{
		return ERROR_SUCCESS;
	}
	else if(_tcscmp(value,TEXT("ERROR_SECTOR_NOT_FOUND")) == 0)
	{
		return ERROR_SECTOR_NOT_FOUND;
	}
	else if(_tcscmp(value,TEXT("ERROR_DISK_FULL")) == 0)
	{
		return ERROR_DISK_FULL;
	}

	/*m_Dword_WstringMapTable[ERROR_SUCCESS] = TEXT("ERROR_SUCCESS");
	m_Wstring_DwordMapTable[L"ERROR_SECTOR_NOT_FOUND"] = ERROR_SECTOR_NOT_FOUND;
	m_Dword_WstringMapTable[ERROR_SECTOR_NOT_FOUND] = TEXT("ERROR_SECTOR_NOT_FOUND");
	m_Wstring_DwordMapTable[L"ERROR_GEN_FAILURE"] = ERROR_GEN_FAILURE;
	m_Dword_WstringMapTable[ERROR_GEN_FAILURE] = TEXT("ERROR_GEN_FAILURE");
	m_Wstring_DwordMapTable[L"ERROR_WRITE_PROTECT"] = ERROR_WRITE_PROTECT;
	m_Dword_WstringMapTable[ERROR_WRITE_PROTECT] = TEXT("ERROR_WRITE_PROTECT");*/
	else if(_tcscmp(value,TEXT("ERROR_GEN_FAILURE")) == 0)
	{
		return ERROR_GEN_FAILURE;
	}
	else if(_tcscmp(value,TEXT("ERROR_WRITE_PROTECT")) == 0)
	{
		return ERROR_WRITE_PROTECT;
	}
	else if(_tcscmp(value,TEXT("UNCHANGED")) == 0)
	{
		return -1;
	}
	else if(_tcscmp(value,TEXT("one")) == 0)
	{
		return 1;
	}
	else if(_tcscmp(value,TEXT("two")) == 0)
	{
		return 2;
	}
	else if(_tcscmp(value,TEXT("last_sector")) == 0)
	{
		return (g_diskInfo.di_total_sectors - 1);
	}
	else if(_tcscmp(value,TEXT("invalid_sector")) == 0)
	{
		return (g_diskInfo.di_total_sectors - 1) + rand() % (MAXDWORD - g_diskInfo.di_total_sectors + 1);
	}
	else if(_tcscmp(value,TEXT("random_sector")) == 0)
	{
		if(g_diskInfo.di_total_sectors == 1)
			return 0;
		return rand() % (g_diskInfo.di_total_sectors - 1);
	}
	else if(_tcscmp(value,TEXT("valid_sector_count")) == 0)
	{
		DWORD startSec = dwBufferSize;

		//if the start sector is invalid, return anything, so the test will continue
		if(startSec > g_diskInfo.di_total_sectors)
		{
			return ((rand() % (g_diskInfo.di_total_sectors - 1)) % MAX_SECTORS) + 1;
		}
		DWORD numSectors = g_diskInfo.di_total_sectors - startSec;
		if(numSectors <= 0)
			numSectors = 1;
		DWORD ret = ((rand() % numSectors) % MAX_SECTORS) + 1; //we need at least one sector to be valid
		return ret;
	}
	else if(_tcscmp(value,TEXT("zero_sectors")) == 0)
	{
		return 0;
	}
	else if(_tcscmp(value,TEXT("past_disk_end")) == 0)
	{
		DWORD startSec = dwBufferSize;
		//if the start sector is invalid, return anything, so the test will continue
		if(startSec > g_diskInfo.di_total_sectors)
		{
				return rand() % g_diskInfo.di_total_sectors == 0;
		}

		DWORD numSectors = g_diskInfo.di_total_sectors - startSec;
		DWORD ret = numSectors + 1 + rand() % 10;  // up to 10 sectors past end of disk
		return ret;
	}
	if(_tcscmp(value,TEXT("one_third_sector")) == 0)
	{
		return (g_diskInfo.di_bytes_per_sect / 3) +((g_diskInfo.di_bytes_per_sect / 3) % sizeof(DWORD));
    }
	else if(_tcscmp(value,TEXT("two_thirds_sector")) == 0)
	{
		return 2 * ((g_diskInfo.di_bytes_per_sect / 3 ) +((g_diskInfo.di_bytes_per_sect / 3) % sizeof(DWORD)));
    }
	else if(_tcscmp(value,TEXT("one_half_sector")) == 0)
	{
		return (g_diskInfo.di_bytes_per_sect / 2) +((g_diskInfo.di_bytes_per_sect / 2) % sizeof(DWORD));
	}
	else if(_tcscmp(value,TEXT("zero")) == 0)
	{
		return 0;
	}
	else if(_tcscmp(value,TEXT("sector_end_minus_one")) == 0)
	{
		return  g_diskInfo.di_total_sectors -1;
	}
	else if(_tcscmp(value,TEXT("gt_size_of_delete_sector_info")) == 0)
	{
			if(dwBufferSize > sizeof(DELETE_SECTOR_INFO))
				return dwBufferSize;
			return (rand() % MAX_BUF) + 1 + sizeof(DELETE_SECTOR_INFO); //return a value greater than sizeof(FMDInfo)
	}							
	else if(_tcscmp(value,TEXT("lt_size_of_delete_sector_info")) == 0)
	{
			if(dwBufferSize < sizeof(DELETE_SECTOR_INFO))
				return dwBufferSize;
			return (rand() % (sizeof(DELETE_SECTOR_INFO) -1)); //return a value less than sizeof(FMDInfo)
	}
	else if(_tcscmp(value,TEXT("size_of_delete_sector_info")) == 0)
	{
			return sizeof(DELETE_SECTOR_INFO); //return a value greater than sizeof(FMDInfo)
	}
	else if(_tcscmp(value,TEXT("size_of_disk_info")) == 0)
	{
			return sizeof(DISK_INFO); //return sizeof(DISK_INFO)
	}
	else if(_tcscmp(value,TEXT("lt_size_of_disk_info")) == 0)
	{
			if(dwBufferSize < sizeof(DISK_INFO))
				return dwBufferSize;
			DWORD temp = (rand() % (sizeof(DISK_INFO) -1));
			if(temp == 0)
				temp = 1;
			return temp;//return a value less than sizeof(DISK_INFO)
	}
	else if(_tcscmp(value,TEXT("gt_size_of_disk_info")) == 0)
	{
		if(dwBufferSize > sizeof(DISK_INFO))
				return dwBufferSize;
			return (rand() % MAX_BUF) + 1 + sizeof(DISK_INFO); //return a value greater than sizeof(DISK_INFO)
	}
	else if(_tcscmp(value,TEXT("size_of_storage_device_info")) == 0)
	{
			return sizeof(STORAGEDEVICEINFO); //return sizeof(STORAGEDEVICEINFO)
	}
	else if(_tcscmp(value,TEXT("lt_size_of_storage_device_info")) == 0)
	{
			if(dwBufferSize < sizeof(DISK_INFO))
				return dwBufferSize;
			DWORD temp = (rand() % (sizeof(STORAGEDEVICEINFO) -1));
			if(temp == 0)
				temp = 1;
			return temp;  //return a value less than sizeof(STORAGEDEVICEINFO)
	}
	else if(_tcscmp(value,TEXT("gt_size_of_storage_device_info")) == 0)
	{
		if(dwBufferSize > sizeof(STORAGEDEVICEINFO))
				return dwBufferSize;
			return (rand() % MAX_BUF) + 1 + sizeof(STORAGEDEVICEINFO); //return a value greater than sizeof(STORAGEDEVICEINFO)
	}
	else if(_tcscmp(value,TEXT("size_of_sg_req")) == 0)
	{
			return sizeof(SG_REQ); //return sizeof(DISK_INFO)
	}
	else if(_tcscmp(value,TEXT("lt_size_of_sg_req")) == 0)
	{
			if(dwBufferSize < sizeof(SG_REQ))
				return dwBufferSize;
			return (rand() % (sizeof(SG_REQ) -1)); //return a value less than sizeof(DISK_INFO)
	}
	else if(_tcscmp(value,TEXT("gt_size_of_sg_req")) == 0)
	{
		if(dwBufferSize > sizeof(SG_REQ))
				return dwBufferSize;
			return (rand() % MAX_BUF) + 1 + sizeof(SG_REQ); //return a value greater than sizeof(DISK_INFO)
	}
	else
	{
		return -1;
	}
}

// --------------------------------------------------------------------
BOOL 
MakeJunkBuffer(
    TCHAR* pBuffer,
    DWORD cBytes)
// --------------------------------------------------------------------
{
    if(pBuffer == NULL)
    {
        return FALSE;
    }
    
    for(UINT i = 0; i < cBytes; i++)
    {
        pBuffer[i] = (BYTE)rand();
    }
    pBuffer[cBytes-1] = '\0';
    return TRUE;
}

// --------------------------------------------------------------------
BOOL 
MakeAlphaBuffer(
    TCHAR* pBuffer,
    DWORD cBytes)
// --------------------------------------------------------------------
{
    if(pBuffer == NULL)
    {
        return FALSE;
    }
	 //between 33 and 122
    for(UINT i = 0; i < cBytes; i++)
    {
		UINT val = i % 122;//between 0 and 122
		if(val < 33)
			val += 33;
        pBuffer[i] = (TCHAR)val; 
		
    }
    pBuffer[cBytes -1] = '\0';
    return TRUE;
}
// --------------------------------------------------------------------
BOOL 
MakeZeroBuffer(
    TCHAR* pBuffer,
    DWORD cBytes)
// --------------------------------------------------------------------
{
    if(pBuffer == NULL)
    {
        return FALSE;
    }
    
    for(UINT i = 0; i < cBytes; i++)
    {
        pBuffer[i] = 0;
    }
    pBuffer[cBytes - 1] = '\0';
    return TRUE;
}
// --------------------------------------------------------------------
BOOL 
MakeOneBuffer(
    TCHAR* pBuffer,
    DWORD cBytes)
// --------------------------------------------------------------------
{
    if(pBuffer == NULL)
    {
        return FALSE;
    }
    
    for(UINT i = 0; i < cBytes; i++)
    {
        pBuffer[i] = 1;
    }
    pBuffer[cBytes - 1] = '\0';
    return TRUE;
}
