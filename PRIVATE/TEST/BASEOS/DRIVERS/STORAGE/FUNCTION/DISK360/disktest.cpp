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
#include "utility.h"

#define DEF_SECTOR_SIZE     512
#define DATA_STRIPE         0x2BC0FFEE
#define DATA_STRIPE_SIZE    sizeof(DWORD)

// data stripe functions, put 4 byte data stripe at start and end of buffer

// --------------------------------------------------------------------
VOID
AddSentinel(
    PBYTE buffer,
    DWORD size)
// --------------------------------------------------------------------
{
    ASSERT(size >= 2*DATA_STRIPE_SIZE);
    
    PDWORD pdw = (PDWORD)buffer;
    *pdw = DATA_STRIPE;
    pdw = (PDWORD)(buffer + (size - DATA_STRIPE_SIZE));
    *pdw = DATA_STRIPE;
}

VOID AddSectorNumber(PBYTE buffer, DWORD dwSectorNum)
{
    ASSERT(buffer != NULL);
    PDWORD pdw = (PDWORD)buffer;
    *pdw = dwSectorNum;
}
// --------------------------------------------------------------------
BOOL
CheckSentinel(
    PBYTE buffer,
    DWORD size)
// --------------------------------------------------------------------
{
    ASSERT(size >= 2*DATA_STRIPE_SIZE);

    PDWORD pdw = (PDWORD)buffer;
    if(*pdw != DATA_STRIPE)
    {
        return FALSE;
    }

    pdw = (PDWORD)(buffer + (size - DATA_STRIPE_SIZE));
    if(*pdw != DATA_STRIPE)
    {
        return FALSE;
    }

    return TRUE;
}


typedef enum _BUFFER_TYPE
{
  ALPHA_BUFFER=1,
  NUM_BUFFER=2,
  RAND_BUFFER=3    
}BUFFER_TYPE;

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadWriteSeq(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
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
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("FAIL: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    if(g_dwSectorSpacing <= 0)
    {
      g_pKato->Log(LOG_DETAIL, _T("DISK360: Forcing g_dwSectorSpacing to %u, because an invalid value was used."),g_diskInfo.di_total_sectors);
      g_dwSectorSpacing = g_diskInfo.di_total_sectors;    
    }
  
    DWORD dwStartSector;
    DWORD cSectorsPerReq = g_dwNumSectorsPerOp;
 
    DWORD cSector = 0;
    DWORD cSectorsChecked =0;

    // float temp = ((float)g_diskInfo.di_total_sectors) * (1/(float)g_dwSectorSpacing);
   //  printf("temp = %f, %d\n",temp,g_dwSectorSpacing);
     
   //  goto Fail;
     
    DWORD dwSectorSpacing = (float)g_diskInfo.di_total_sectors * (1/(float)g_dwSectorSpacing);  //perform test every g_dwNumSectorsPerOp sectors
    if(dwSectorSpacing == 0)
      dwSectorSpacing = 1;
    g_pKato->Log(LOG_DETAIL, _T("We will write and read every %u sectors"), dwSectorSpacing);
        
    //make sure we don't overwrite the next start sector
    if(cSectorsPerReq > dwSectorSpacing)
    {
       g_pKato->Log(LOG_DETAIL, _T("Reducing numsectors because it is larger than SectorSpacing"));
       cSectorsPerReq = dwSectorSpacing;
    }
    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);
    
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;
    PBYTE pTempWriteBuffer = NULL;
    
    DWORD bufferSize = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;

    ASSERT(bufferSize);
    
    pReadBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize  + 4*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
    pTempWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pWriteBuffer || NULL == pReadBuffer || pTempWriteBuffer == NULL)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
        goto Fail;
    }
  
  
    // put junk in the buffer
    MakeJunkBuffer(pWriteBuffer, bufferSize);
    //add a sentinal to read buffer
    AddSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE);
       
   // AddSentinel(pReadBuffer, bufferSize + 4*DATA_STRIPE_SIZE);
   // AddSentinel(pWriteBuffer, bufferSize + 2*DATA_STRIPE_SIZE); //add sentinal to what we are writing

    // perform writes accross the disk
    for(dwStartSector = 0; 
        dwStartSector < g_diskInfo.di_total_sectors; 
        dwStartSector += dwSectorSpacing)
    {    
        if(dwStartSector + cSectorsPerReq > g_diskInfo.di_total_sectors)
        {
            //we will exceed the end of the disk
            cSectorsPerReq =  g_diskInfo.di_total_sectors - dwStartSector;
            if(cSectorsPerReq == 0)
              break;
        }
        //copy to temp write buffer
        memcpy(pTempWriteBuffer,pWriteBuffer,bufferSize);
        //add sector number to buffer
        AddSectorNumber(pTempWriteBuffer,dwStartSector);
        
        g_pKato->Log(LOG_DETAIL, _T("writing %u sectors starting at sector %u..."), cSectorsPerReq, dwStartSector);
        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, dwStartSector, cSectorsPerReq, pTempWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) at sector %u"), cSector);
            goto Fail;
        }
        cSectorsChecked++;
    }  
    g_pKato->Log(LOG_DETAIL, _T("Wrote %u sectors"), cSectorsChecked * cSectorsPerReq);
    cSectorsChecked = 0;
    cSectorsPerReq = g_dwNumSectorsPerOp;
    //make sure we don't overwrite the next start sector
    if(cSectorsPerReq > dwSectorSpacing)
    {
       g_pKato->Log(LOG_DETAIL, _T("Reducing numsectors because it is larger than SectorSpacing"));
       cSectorsPerReq = dwSectorSpacing;
    }
  
    //now verify
    for(dwStartSector = 0; 
        dwStartSector < g_diskInfo.di_total_sectors; 
        dwStartSector += dwSectorSpacing)
    {    
        if(dwStartSector + cSectorsPerReq > g_diskInfo.di_total_sectors)
        {
            //we will exceed the end of the disk
            cSectorsPerReq =  g_diskInfo.di_total_sectors - dwStartSector;
            if(cSectorsPerReq == 0)
              break;
        }
        //copy to temp write buffer
        memcpy(pTempWriteBuffer,pWriteBuffer,bufferSize);
        //add sector number to buffer
        AddSectorNumber(pTempWriteBuffer,dwStartSector);
        
        g_pKato->Log(LOG_DETAIL, _T("reading and verifying %u sectors starting at sector %u..."), cSectorsPerReq, dwStartSector);

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, dwStartSector, cSectorsPerReq, pReadBuffer + DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) at sector %u"), cSector);
            goto Fail;
        }

        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pTempWriteBuffer, bufferSize))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: buffer miscompare; start sector %u, read %u sectors"), cSector, cSectorsPerReq);
            goto Fail;
        }

        if(!CheckSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) overwrote the buffer at sector %u"), cSector);
            goto Fail;
        }
        cSectorsChecked++;
    }  

     g_pKato->Log(LOG_DETAIL, _T("Wrote, Read and Verified %u sectors"), cSectorsChecked * cSectorsPerReq);

    VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    VirtualFree(pWriteBuffer, 0, MEM_RELEASE);
    VirtualFree(pTempWriteBuffer, 0, MEM_RELEASE);
   
    return cSectorsChecked > 0 ? TPR_PASS : TPR_FAIL;
Fail:
    if(pReadBuffer)  VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);

    return TPR_FAIL;
}


