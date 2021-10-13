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
#define MARK_TEST           0
#define NUM_ITER 3
#define SIZE_OF_SECTOR 512
#define MAX_FILE_SIZE 1073741824 //1GB
HANDLE g_hDisk = INVALID_HANDLE_VALUE;

TCHAR g_szPath[MAX_PATH] = _T("");
DWORD             g_dwStartFileSize = 1;
DWORD             g_dwEndFileSize = 65536;
DWORD             g_dwStartTransferSize = 1;
DWORD             g_dwEndTransferSize = 64;
DWORD             g_dwStartClusterSize = 1;
DWORD             g_dwEndClusterSize = 64;
DWORD             g_dwStartDiskUsage = 0;
DWORD             g_dwEndDiskUsage = 100;
DWORD             g_dwStartFATCacheSize = 0;
DWORD             g_dwEndFATCacheSize = 0;
DWORD             g_dwStartDATACacheSize = 0;
DWORD             g_dwEndDATACacheSize = 0;

BOOL              g_fWriteThrough = false;
BOOL              g_fFATCache = false;
BOOL              g_fDATACache = false;
BOOL              g_fSetFilePointerEOF = false;
BOOL              g_fTFAT = false;
BOOL              g_fUseCache = false;
CFSInfo           *g_pFsInfo = NULL;

static const LPWSTR g_szRegKey = L"SYSTEM\\StorageManager\\FATFS";

void FlushDiskCache(void);

void FlushDiskCache(void)
{
	g_pFsInfo->Dismount();
	g_pFsInfo->Mount();
}
// --------------------------------------------------------------------
BOOL
GetDiskInfo(
			HANDLE hDisk,
			LPWSTR pszDisk,
			DISK_INFO *pDiskInfo)
			// --------------------------------------------------------------------
{
	ASSERT(pDiskInfo);

	DWORD cbReturned;

	// query driver for disk information structure

	// first, try IOCTL_DISK_GETINFO call
	if(!DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, pDiskInfo, sizeof(DISK_INFO), &cbReturned, NULL))        
	{
		g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to IOCTL_DISK_GETINFO"), pszDisk);

		// the disk may only support the legacy DISK_IOCTL_GETINFO call
		if(!DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, pDiskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL))
		{
			g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to DISK_IOCTL_GETINFO"), pszDisk);
			return FALSE;
		}

		// this disk will work, but it only supports old DISK_IOCTLs so set the global flag
		g_pKato->Log(LOG_DETAIL, _T("test will use old DISK_IOCTL_* control codes"));
	}
	return TRUE;
}

// --------------------------------------------------------------------
HANDLE 
OpenDevice(
		   LPCTSTR pszDiskName)
		   // --------------------------------------------------------------------
{
	return CreateFile(pszDiskName, GENERIC_READ, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

// --------------------------------------------------------------------
BOOL 
MakeJunkBuffer(
			   PBYTE pBuffer,
			   DWORD cBytes)
			   // --------------------------------------------------------------------
{
	if(!MapCallerPtr(pBuffer, cBytes))
	{
		return FALSE;
	}
	for(UINT i = 0; i < cBytes; i++)
	{
		pBuffer[i] = (BYTE)rand();
	}
	return TRUE;
}

typedef enum _CacheType
{
	FATCACHE=0,
	DATACACHE=1,
	CACHEONOFF=2,
	FLAGS=3
}CacheType;


BOOL SetFatRegistry(IN DWORD cacheSize,CacheType type )
//    
//  Set the current FATFS disk cache size in registry
//-----------------------------------------------    
{
    DWORD dwError = 0;
    BOOL fRet = TRUE;
    HKEY hKeyFATFS = NULL;

    cacheSize = NearestPowerOfTwo(cacheSize);


 // open the fatfs registry key
    dwError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_szRegKey, 0, KEY_ALL_ACCESS, &hKeyFATFS);
	
    if(ERROR_SUCCESS == dwError)
    {
		dwError = RegDeleteKey(hKeyFATFS,TEXT("CacheSize"));
		if(type == FATCACHE)
	        dwError = RegSetValueEx(hKeyFATFS, TEXT("FatCacheSize"), 0, REG_DWORD, (UCHAR*)&cacheSize, sizeof(DWORD));
		else if(type == DATACACHE)
			dwError = RegSetValueEx(hKeyFATFS, TEXT("DataCacheSize"), 0, REG_DWORD, (UCHAR*)&cacheSize, sizeof(DWORD));
		else if(type == CACHEONOFF)
			dwError = RegSetValueEx(hKeyFATFS, TEXT("EnableCache"), 0, REG_DWORD, (UCHAR*)&g_fUseCache, sizeof(DWORD));
	  else if(type == FLAGS)
			dwError = RegSetValueEx(hKeyFATFS, TEXT("Flags"), 0, REG_DWORD, (UCHAR*)&cacheSize, sizeof(DWORD));
	
        if(ERROR_SUCCESS == dwError)
        {
			if(type == FATCACHE)
				NKDbgPrintfW(L"FATCACHE: FATFS cache size set to %u", cacheSize);
			else if(type == DATACACHE)
				NKDbgPrintfW(L"DATACACHE: DATA cache size set to %u", cacheSize);
			else if(type == CACHEONOFF)
				NKDbgPrintfW(L"Cache is %d", g_fUseCache);
			else if(type == FLAGS)
				NKDbgPrintfW(L"FAT Flags are set to %d", cacheSize);
            fRet = TRUE;
        }
        else
        {
			if(type == FATCACHE)
				NKDbgPrintfW(L"FATCACHE: failed to set FATFS cache size in registry");
      else if(type == DATACACHE)
					NKDbgPrintfW(L"DATACACHE: failed to set DATA cache size in registry");
			else if(type == CACHEONOFF)
					NKDbgPrintfW(L"Failed to turn cache on or off");
			else if(type == FLAGS)
					NKDbgPrintfW(L"Failed to set FAT flags");
			fRet = FALSE;
            
        }
		goto Exit;
    }
    else
    {
        NKDbgPrintfW(L"FATCACHE: failed to open FATFS registry key");
        fRet = FALSE;
    }

Exit:
	RegCloseKey(hKeyFATFS);
    return fRet;
}

void FillDisk(int percentage,int FileSectorsNeeded)
{
     PARTINFO PI = {0};
	 PI.cbSize = sizeof(PARTINFO);
	 g_pFsInfo->PartInfo(&PI);
	
	 
	 double fact = (double)percentage / 100.0;
	 DWORD NumSectorsToFill = (DWORD)(fact * PI.snNumSectors);
	   
	 //figure out how many sectors are on the disk, and the size of the sectors
	 g_pKato->Log(LOG_DETAIL,TEXT("There are %d sectors on the disk.\n"),PI.snNumSectors);
	// g_pKato->Log(LOG_DETAIL,TEXT("%d sectors are free.\n"),PI.snFreeSectors);

	 g_pKato->Log(LOG_DETAIL,TEXT("I will fill %d percent of them.\n"),percentage);
	 g_pKato->Log(LOG_DETAIL,TEXT("This is about %d sectors.\n"),NumSectorsToFill);
		
	 //leave enough room for the largest file
	 if(PI.snNumSectors - NumSectorsToFill < FileSectorsNeeded)
	 {
		 //leave 10 more sectors than needed
		NumSectorsToFill = (DWORD)PI.snNumSectors - FileSectorsNeeded - 10;
	 }
	 DWORD FillFileSize = NumSectorsToFill * SIZE_OF_SECTOR;
	 TCHAR FileName[MAX_PATH];
	 
	 DWORD NumFiles = FillFileSize / MAX_FILE_SIZE;
	 TCHAR num[10];
	 DWORD BytesTransferred = 0;
	 for(unsigned int i = 0; i < NumFiles; i++)
	 {
		 g_pKato->Log(LOG_DETAIL,TEXT("Writing File %d\n"),i+1);
		 _tcscpy(num,TEXT(""));
		_stprintf(num,TEXT("%d\0"),i);
		_tcscpy(FileName,g_szPath);

		_tcscat(FileName,TEXT("\\filldisk.fil"));
		_tcscat(FileName,num);
		_tcscat(FileName,TEXT("\0"));
		DeleteFile(FileName);
		HANDLE hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,0);
		SetFilePointer(hFile,MAX_FILE_SIZE,NULL,FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		BytesTransferred += MAX_FILE_SIZE;
		//g_pKato->Log(LOG_DETAIL,TEXT("There are %d free sectors on the disk.\n"),PI.snFreeSectors);
		g_pKato->Log(LOG_DETAIL,TEXT("Total Bytes Transferred: %x\n"),BytesTransferred);
	 }

	 if(BytesTransferred < FillFileSize)
	 {
		//we have 1 more file to write
		g_pKato->Log(LOG_DETAIL,TEXT("Writing File %d\n"),i+1);
		
		int FillSize = FillFileSize - BytesTransferred;
		_tcscpy(num,TEXT(""));
		_stprintf(num,TEXT("%d\0"),i);
		_tcscpy(FileName,g_szPath);
		_tcscat(FileName,TEXT("\\filldisk.fil"));
		_tcscat(FileName,num);
		_tcscat(FileName,TEXT("\0"));
		DeleteFile(FileName);
		HANDLE hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | (g_fWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),0);
		SetFilePointer(hFile,FillSize,NULL,FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		BytesTransferred += FillSize;
		g_pKato->Log(LOG_DETAIL,TEXT("Total Bytes Transferred: %x\n"),BytesTransferred);
	 }
	// g_pKato->Log(LOG_DETAIL,TEXT("There are %d free sectors on the disk.\n"),PI.snFreeSectors);
		
	 ASSERT(BytesTransferred == FillFileSize);
	   	
}
TESTPROCAPI 
TestReadWritePerf(
				  UINT uMsg, 
				  TPPARAM tpParam, 
				  LPFUNCTION_TABLE_ENTRY lpFTE)
				  // --------------------------------------------------------------------
{
	if( TPM_QUERY_THREAD_COUNT == uMsg )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if( TPM_EXECUTE != uMsg )
	{
		return TPR_NOT_HANDLED;
	}    
#define FILE_NAME TEXT("\\Hard Disk")

	g_pFsInfo = new CFSInfo(g_szPath);
	
	

	TCHAR FileName[MAX_PATH];
	_tcscpy(FileName,g_szPath);
	_tcscat(FileName,TEXT("\\perf.fil\0"));
	//perf marker strings
	TCHAR perfMarkFileSize[MAX_PATH];
	TCHAR perfMarkTransferSize[MAX_PATH];
	TCHAR perfMarkClusterSize[MAX_PATH];
	TCHAR perfMarkDiskUsage[MAX_PATH];
	TCHAR perfMarkMisc[MAX_PATH];
	TCHAR perfMarkFATCacheSize[MAX_PATH];
	TCHAR perfMarkDATACacheSize[MAX_PATH];
	//TCHAR perfMarkCalculations[MAX_PATH * 2];
	TCHAR MarkerString[MAX_PATH * 8];
  
//	_tcscpy(perfMarkCalculations,TEXT(",MB/s(MAX)=%File(b)%/%MAX%*1000/1024/1024"));
//	_tcscat(perfMarkCalculations,TEXT(",MB/s(AVG)=%File(b)%/%AVG%*1000/1024/1024"));
//	_tcscat(perfMarkCalculations,TEXT(",MB/s(MIN)=%File(b)%/%MIN%*1000/1024/1024"));

	//first, set up cache
	if(g_fUseCache)
	{
		//turn on cache
		_tcscpy(perfMarkMisc,TEXT(",Cache=On"));
	}
	else
	{
		//turn off cache
		_tcscpy(perfMarkMisc,TEXT(",Cache=Off"));
		//default values for cache sizes
		g_dwStartFATCacheSize = 0;
		g_dwEndFATCacheSize = 0;
		g_dwStartDATACacheSize = 0;
		g_dwEndDATACacheSize = 0;
	}
	
	if(g_fSetFilePointerEOF)
		_tcscat(perfMarkMisc,TEXT(",PreAllocFile=True"));
	else
		_tcscat(perfMarkMisc,TEXT(",PreAllocFile=False"));
	
	if(g_fTFAT)
    _tcscat(perfMarkMisc,TEXT(",FatType=TFAT"));
  else
    _tcscat(perfMarkMisc,TEXT(",FatType=FAT"));
    
  if(g_fWriteThrough)
    _tcscat(perfMarkMisc,TEXT(",ForceWriteThrough=True"));
  else
    _tcscat(perfMarkMisc,TEXT(",ForceWriteThrough=False"));

	TCHAR itoaVal[10];

	for(unsigned int FileSize = g_dwStartFileSize; FileSize <= g_dwEndFileSize; FileSize = FileSize << 1)
	{
		if(FileSize == 0)
			break;
		unsigned int ActualFileSize = FileSize * SIZE_OF_SECTOR;
		_tcscpy(itoaVal,TEXT(""));
		_itot(ActualFileSize,itoaVal,10);
		_tcscpy(perfMarkFileSize,TEXT(",File(b)="));
		_tcscat(perfMarkFileSize,itoaVal);
		for(unsigned int FatCacheSize = g_dwStartFATCacheSize; FatCacheSize <= g_dwEndFATCacheSize; FatCacheSize = FatCacheSize << 1)
		{
		  unsigned int ActualFatCacheSize = FatCacheSize * SIZE_OF_SECTOR;
		  _tcscpy(itoaVal,TEXT(""));
			_itot(ActualFatCacheSize,itoaVal,10);
			_tcscpy(perfMarkFATCacheSize,TEXT(",FATCache(b)="));
			_tcscat(perfMarkFATCacheSize,itoaVal);
			for(unsigned int DataCacheSize = g_dwStartDATACacheSize; DataCacheSize <= g_dwEndDATACacheSize; DataCacheSize = DataCacheSize << 1)
			{
			   unsigned int ActualDataCacheSize = DataCacheSize * SIZE_OF_SECTOR;
			   _tcscpy(itoaVal,TEXT(""));
				_itot(ActualDataCacheSize,itoaVal,10);
				_tcscpy(perfMarkDATACacheSize,TEXT(",DATACache(b)="));
				_tcscat(perfMarkDATACacheSize,itoaVal);
					//turn on the cache, and set the proper FATCache and DATACache registry settings
					//unload
					g_pFsInfo->Dismount();
					int Flags = 0x44;
					//CHANGE REGISTRY SETTINGS HERE
					SetFatRegistry(FatCacheSize,FATCACHE);
					SetFatRegistry(DataCacheSize,DATACACHE);
					SetFatRegistry(Flags,FLAGS);
					SetFatRegistry(0,CACHEONOFF);
					//reload
					g_pFsInfo->Mount();

				if(DataCacheSize == 0)
					DataCacheSize++;
				if(FatCacheSize == 0)
					FatCacheSize++;
				for(unsigned int TransferSize = g_dwStartTransferSize; TransferSize <= g_dwEndTransferSize; TransferSize = TransferSize << 1)
				{
					if(TransferSize == 0)
						break;
					unsigned int ActualTransferSize = TransferSize * SIZE_OF_SECTOR;
					_tcscpy(itoaVal,TEXT(""));
					_itot(ActualTransferSize,itoaVal,10);
					_tcscpy(perfMarkTransferSize,TEXT(",Transfer(b)="));
					_tcscat(perfMarkTransferSize,itoaVal);

					for(unsigned int ClusterSize = g_dwStartClusterSize; ClusterSize <= g_dwEndClusterSize; ClusterSize = ClusterSize << 1)
					{
						unsigned int ActualClusterSize = ClusterSize * SIZE_OF_SECTOR;
						_tcscpy(itoaVal,TEXT(""));
						_itot(ActualClusterSize,itoaVal,10);
						_tcscpy(perfMarkClusterSize,TEXT(",Cluster(b)="));
						_tcscat(perfMarkClusterSize,itoaVal);
						//format the disk
						FORMAT_OPTIONS fo;
						fo.dwClusSize = ActualClusterSize;
						fo.dwRootEntries = 0;
						fo.dwFatVersion = 16;
						fo.dwNumFats = 1;
						fo.dwFlags = g_fTFAT ? FATUTIL_FORMAT_TFAT : 0;
						
						
						for(unsigned int DiskUsage = g_dwStartDiskUsage; DiskUsage <= g_dwEndDiskUsage; DiskUsage += 33)
						{
						  _tcscpy(itoaVal,TEXT(""));
							_itot(DiskUsage,itoaVal,10);
							_tcscpy(perfMarkDiskUsage,TEXT(",Disk(perc)="));
							_tcscat(perfMarkDiskUsage,itoaVal);
							
							g_pFsInfo->Format(&fo);
							FillDisk(DiskUsage,FileSize);

							
							//Create a buffer of ActualTransferSize length
							LPBYTE pBuffer = NULL;  //buffer with data to copy
							DWORD BufSize = ActualTransferSize + 1;
							pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, BufSize);
							for(unsigned int j = 0; j < BufSize -1;j++)
							{
								pBuffer[j] = rand() % 256;
							}
							 pBuffer[ActualTransferSize] = '\0';
							if(NULL == pBuffer) {
								g_pKato->Log(LOG_DETAIL,TEXT("Perf_RegisterMark() failed"));
							}

						
							_tcscpy(MarkerString,TEXT("Test=Write"));
							_tcscat(MarkerString,perfMarkMisc);
							_tcscat(MarkerString,perfMarkFileSize);
							_tcscat(MarkerString,perfMarkFATCacheSize);
							_tcscat(MarkerString,perfMarkDATACacheSize);
							_tcscat(MarkerString,perfMarkTransferSize);
							_tcscat(MarkerString,perfMarkClusterSize);
							_tcscat(MarkerString,perfMarkDiskUsage);
					for(int iter = 0; iter < NUM_ITER; iter++)
						{
							DWORD cbWritten = 0;
							//now write the file
							DWORD cbTransferred = 0;
							//delete the file
							DeleteFile(FileName);
							//create the file
							HANDLE hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | (g_fWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),0);
							

  							if(!Perf_RegisterMark(MARK_TEST, MarkerString))
  							{
  								g_pKato->Log(LOG_DETAIL,TEXT("Perf_RegisterMark() failed"));
  								// goto Error;
  							}
  							Perf_MarkBegin(MARK_TEST);
  							if(g_fSetFilePointerEOF)
							  {
								  SetFilePointer(hFile,ActualFileSize,NULL,FILE_BEGIN);
								  SetEndOfFile(hFile);
								  SetFilePointer(hFile,0,NULL,FILE_BEGIN);
							  }
  							while(cbTransferred < ActualFileSize) {
  								if(!WriteFile(hFile, pBuffer, ActualTransferSize, &cbWritten, NULL)) {
									g_pKato->Log(LOG_DETAIL,TEXT("Failed to write %d bytes. Error Code: %d\n"),ActualTransferSize,GetLastError());
  									break;
  								}
  								//g_pKato->Log(LOG_DETAIL,TEXT("FileSize: %d\tTransferSize: %d\tWrote: %d\tTotalTransfer: %d\n"),ActualFileSize,ActualTransferSize,cbWritten,cbTransferred);
  								cbTransferred += cbWritten;
  								//g_pKato->Log(LOG_DETAIL,TEXT("Total Transferred =  %d bytes"),cbTransferred);
  							}//end while
  							Perf_MarkEnd(MARK_TEST);
  							CloseHandle(hFile);
						}
						
							//should flush the cache
							FlushDiskCache();
						for(int iter = 0; iter < NUM_ITER; iter++)
						{
							HANDLE hFile = CreateFile(FileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | (g_fWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0), NULL);
						
							//move to beginning of file
							SetFilePointer(hFile,0,0, FILE_BEGIN); 

							_tcscpy(MarkerString,TEXT("Test=Read"));
							_tcscat(MarkerString,perfMarkMisc);
							_tcscat(MarkerString,perfMarkFileSize);
							_tcscat(MarkerString,perfMarkFATCacheSize);
							_tcscat(MarkerString,perfMarkDATACacheSize);
							_tcscat(MarkerString,perfMarkTransferSize);
							_tcscat(MarkerString,perfMarkClusterSize);
							_tcscat(MarkerString,perfMarkDiskUsage);
						//	_tcscat(MarkerString,perfMarkCalculations);

							//now read it back
							DWORD cbRead = 0;
							DWORD cbTransferred = 0;
							if(!Perf_RegisterMark(MARK_TEST, MarkerString))
					  {
						  //LOG(TEXT("Perf_RegisterMark() failed"));
						  //goto Error;
					  }
            
  					  Perf_MarkBegin(MARK_TEST);
  					  while(cbTransferred < ActualFileSize) 
  					  {
  						  if(!ReadFile(hFile, pBuffer, ActualTransferSize, &cbRead, NULL)) 
  						  {
  							  g_pKato->Log(LOG_DETAIL,TEXT("Failed to read %d bytes.  Error code %d\n"),ActualTransferSize,GetLastError());
  							  break;
  						  }
  						  //g_pKato->Log(LOG_DETAIL,TEXT("Read %d bytes"),cbRead);
  						  cbTransferred += cbRead;
						  //g_pKato->Log(LOG_DETAIL,TEXT("Total Transferred =  %d bytes"),cbTransferred);
  					  }//end read
  					  Perf_MarkEnd(MARK_TEST);
  					  CloseHandle(hFile);
  				}

					  if(pBuffer)
						  LocalFree(pBuffer);
					  pBuffer = NULL; 
						}
					}
				}
			}
		}
	}

	if(g_pFsInfo)
		delete g_pFsInfo;
	return TPR_PASS;
	
error:
  return TPR_FAIL;
} 


VOID
Calibrate(void)
{
	for(int dwCount = 0; dwCount < CALIBRATION_COUNT; dwCount++)
	{
		Perf_MarkBegin(MARK_CALIBRATE);
		Perf_MarkEnd(MARK_CALIBRATE);
	}
}