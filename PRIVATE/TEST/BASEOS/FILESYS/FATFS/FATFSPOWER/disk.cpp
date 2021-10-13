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
#include "PowerTest.h"
#define MARK_TEST           0
#define NUM_ITER 3
#define SIZE_OF_SECTOR 512
#define MAX_FILE_SIZE 1073741824 //1GB
HANDLE g_hDisk = INVALID_HANDLE_VALUE;

extern TCHAR g_szPath[MAX_PATH];
extern DWORD g_dwFileSize;
extern DWORD g_dwTransferSize;
extern BOOL  g_bWriteThrough;
extern BOOL  g_bCache;
extern DWORD g_dwTestTime;
extern TEST_TYPE g_Test_Type;
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

    if(type != FLAGS)
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
			dwError = RegSetValueEx(hKeyFATFS, TEXT("EnableCache"), 0, REG_DWORD, (UCHAR*)&g_bCache, sizeof(DWORD));
	  else if(type == FLAGS)
			dwError = RegSetValueEx(hKeyFATFS, TEXT("Flags"), 0, REG_DWORD, (UCHAR*)&cacheSize, sizeof(DWORD));
	
        if(ERROR_SUCCESS == dwError)
        {
			if(type == FATCACHE)
				NKDbgPrintfW(L"FATCACHE: FATFS cache size set to %u", cacheSize);
			else if(type == DATACACHE)
				NKDbgPrintfW(L"DATACACHE: DATA cache size set to %u", cacheSize);
			else if(type == CACHEONOFF)
				NKDbgPrintfW(L"Cache is %d", g_bCache);
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




BOOL TestReadWritePower(void)
				  // --------------------------------------------------------------------
{
	 
  BOOL bRet = TRUE;
  g_pFsInfo = new CFSInfo(g_szPath);
  HANDLE hFile = NULL;
  TCHAR FileName[MAX_PATH];
  	_tcscpy(FileName,g_szPath);
	_tcscat(FileName,TEXT("\\perf.fil\0"));
	
	//configure the registry
  g_pFsInfo->Dismount();
	int Flags = 0x44;
					//CHANGE REGISTRY SETTINGS HERE
					SetFatRegistry(0,FATCACHE);
					SetFatRegistry(0,DATACACHE);
					SetFatRegistry(Flags,FLAGS);
					//this will use the value of g_bUseCache
					SetFatRegistry(0,CACHEONOFF);
	//reload
	g_pFsInfo->Mount();
	
	//Create a buffer of g_dwTransferSize length
  LPBYTE pBuffer = NULL;  //buffer with data to copy
	DWORD BufSize = g_dwTransferSize + 1;
	pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, BufSize);
	for(unsigned int j = 0; j < BufSize -1;j++)
	{
	   pBuffer[j] = rand() % 256;
	}
	pBuffer[g_dwTransferSize] = '\0';
	if(NULL == pBuffer) 
	{
	   Debug(TEXT("Failed to allocate pBuffer\n"));
	   bRet = FALSE;
	   goto END;
	}
						
						
	//if we're only reading, we need to create a file one time
	if(g_Test_Type == READ)
	{
	   DeleteFile(FileName);
	   hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | (g_bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),0);
	   SetFilePointer(hFile,g_dwFileSize,NULL,FILE_BEGIN);
		 SetEndOfFile(hFile);
		 SetFilePointer(hFile,0,NULL,FILE_BEGIN);
		 CloseHandle(hFile);
	} 
	//store the current time
	DWORD StartTime = timeGetTime();
 while(timeGetTime() - StartTime < g_dwTestTime * 1000)
  {
        switch(g_Test_Type)
        {
          case READ:
          {
            //first Open a file
            hFile = CreateFile(FileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | (g_bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0), NULL);
						//move to beginning of file
					  SetFilePointer(hFile,0,0, FILE_BEGIN);
					  DWORD cbTransferred = 0;
					  DWORD cbRead = 0;	
					  //now transfer the data
					  while(cbTransferred < g_dwFileSize) 
  					{
  						  if(!ReadFile(hFile, pBuffer, g_dwTransferSize, &cbRead, NULL)) 
  						  {
  							  Debug(TEXT("Failed to read %d bytes.  Error code %d\n"),g_dwTransferSize,GetLastError());
  							  bRet = FALSE;
  							  goto END;
  							  break;
  						  }
  						  cbTransferred += cbRead;
						 }
						 CloseHandle(hFile);
             break;
          }
          case WRITE:
          {
              DWORD cbTransferred = 0;
              DWORD cbWritten = 0;
              //delete the file
							DeleteFile(FileName);
							//create the file
							HANDLE hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | (g_bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),0);
							while(cbTransferred < g_dwFileSize) {
  								if(!WriteFile(hFile, pBuffer, g_dwTransferSize, &cbWritten, NULL)) {
									   Debug(TEXT("Failed to write %d bytes. Error Code: %d\n"),g_dwTransferSize,GetLastError());
  									 bRet = FALSE;
  									 goto END;
  									 break;
  								}
  								cbTransferred += cbWritten;
  							}
  						CloseHandle(hFile);
              
              //now read the file
              hFile = CreateFile(FileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | (g_bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0), NULL);
						//move to beginning of file
					  SetFilePointer(hFile,0,0, FILE_BEGIN);
					  cbTransferred = 0;
					  DWORD cbRead = 0;	
					  //now transfer the data
					  while(cbTransferred < g_dwFileSize) 
  					{
  						  if(!ReadFile(hFile, pBuffer, g_dwTransferSize, &cbRead, NULL)) 
  						  {
  							  Debug(TEXT("Failed to read %d bytes.  Error code %d\n"),g_dwTransferSize,GetLastError());
  							  bRet = FALSE;
  							  goto END;
  							  break;
  						  }
  						  cbTransferred += cbRead;
						 }
						 CloseHandle(hFile);
             break;
          }
          case READWRITE:
          {
              DWORD cbTransferred = 0;
              DWORD cbWritten = 0;
              //delete the file
							DeleteFile(FileName);
							//create the file
							HANDLE hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | (g_bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),0);
							while(cbTransferred < g_dwFileSize) {
  								if(!WriteFile(hFile, pBuffer, g_dwTransferSize, &cbWritten, NULL)) {
									   Debug(TEXT("Failed to write %d bytes. Error Code: %d\n"),g_dwTransferSize,GetLastError());
  									 bRet = FALSE;
  									 goto END;
  									 break;
  								}
  								cbTransferred += cbWritten;
  							}
  						CloseHandle(hFile);
            break;
          }
          default:
            break;    
        }
  }
END:
  CloseHandle(hFile);
   if(pBuffer)
			LocalFree(pBuffer);
	pBuffer = NULL; 
    
	if(g_pFsInfo)
		delete g_pFsInfo;
	
	return bRet;
} 


