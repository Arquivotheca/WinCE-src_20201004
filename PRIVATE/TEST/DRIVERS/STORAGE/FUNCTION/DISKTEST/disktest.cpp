//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

		

// --------------------------------------------------------------------
TESTPROCAPI 
TestGetDiskName(
    UINT uMsg, 
    TPPARAM tpParam, 
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
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    TCHAR szDiskName[MAX_PATH] = _T("");
    DWORD cbReturned = 0;

    if(!DeviceIoControl(g_hDisk, g_fOldIoctls ? DISK_IOCTL_GETNAME : IOCTL_DISK_GETNAME, NULL, 0, szDiskName, sizeof(szDiskName), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: DeviceIoControl(GETNAME) failed; error %u"), GetLastError());
        g_pKato->Log(LOG_DETAIL, _T("the disk does not support %s"), 
            g_fOldIoctls ? _T("DISK_IOCTL_GETNAME") : _T("IOCTL_DISK_GETNAME"));
        return TPR_SKIP;
    }

    if(!szDiskName[0])
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: DeviceIoControl(GETNAME) returned empty name"));
        return TPR_SKIP;
    }

    g_pKato->Log(LOG_DETAIL, _T("Disk is named: %s"), szDiskName);

    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestFormatMedia(
    UINT uMsg, 
    TPPARAM tpParam, 
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
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    if(!FormatMedia(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: FormatMedia() failed; error %u"), GetLastError());
        return TPR_FAIL;
    }    
       
    return TPR_PASS;
}


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
    
    DWORD dwStartSector;
    DWORD cSectorsPerReq = lpFTE->dwUserData;
    DWORD cSector = 0;
    DWORD cSectorsChecked =0;

    if(cSectorsPerReq > g_dwMaxSectorsPerOp) 
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: %u sectors is larger than maximum allowed (%u)"),
            cSectorsPerReq, g_dwMaxSectorsPerOp);
        return TPR_SKIP;
    }
    
    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);
    
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;

    DWORD bufferSize = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;

    ASSERT(bufferSize);
    
    pReadBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize + 2*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pWriteBuffer || NULL == pReadBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
        goto Fail;
    }
    
    AddSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE);

    // perform writes accross the disk
    for(dwStartSector = 1; 
        dwStartSector < g_diskInfo.di_total_sectors - cSectorsPerReq 
        && dwStartSector > 0; 
        dwStartSector <<= 1)
    {    
        cSector = dwStartSector - 1;
        //if this is the last run, then force to R/W the last sectors on the disk
        if( (cSector + g_diskInfo.di_total_sectors / (READ_LOCATIONS)) >= (g_diskInfo.di_total_sectors - cSectorsPerReq))
        {
            cSector = g_diskInfo.di_total_sectors - cSectorsPerReq;
        }
        
        // put junk in the buffer
        MakeJunkBuffer(pWriteBuffer, bufferSize);

        g_pKato->Log(LOG_DETAIL, _T("reading + writing + comparing %u sectors @ sector %u..."), cSectorsPerReq, cSector);

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, cSectorsPerReq, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) at sector %u"), cSector);
            goto Fail;
        }

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, cSector, cSectorsPerReq, pReadBuffer + DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) at sector %u"), cSector);
            goto Fail;
        }

        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer, bufferSize))
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
    VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    VirtualFree(pWriteBuffer, 0, MEM_RELEASE);
    
    g_pKato->Log(LOG_DETAIL, _T("Wrote, Read and Verified %u sectors"), cSectorsChecked * cSectorsPerReq);

    return cSectorsChecked > 0 ? TPR_PASS : TPR_FAIL;
Fail:
    if(pReadBuffer)  VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);

    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadWriteSize(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cSectorsPerReq = 0;
    DWORD cSector = lpFTE->dwUserData;
    DWORD bufferSize = 0;

    // read/write buffers, allocated at each pass
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;    

    for(cSectorsPerReq = 1; 
        (cSectorsPerReq < (MAX_BUFFER_SIZE / g_diskInfo.di_bytes_per_sect))
        && (cSectorsPerReq <= g_diskInfo.di_total_sectors)
        && (cSectorsPerReq <= g_dwMaxSectorsPerOp); cSectorsPerReq <<= 1)
    {
        g_pKato->Log(LOG_DETAIL, _T("writing + reading + comparing %u sectors @ sector %u"), cSectorsPerReq, cSector);
        bufferSize = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;
        ASSERT(bufferSize);

        pReadBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize + 2*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);
        pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);        
        
        if(NULL == pWriteBuffer || NULL == pReadBuffer)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
            goto Fail;
        }

        // mark the read buffer boundaries
        AddSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE);
        
        // put junk in the buffer
        MakeJunkBuffer(pWriteBuffer, bufferSize);

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, cSectorsPerReq, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) requesting %u sectors"), cSectorsPerReq);
            goto Fail;
        }

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, cSector, cSectorsPerReq, pReadBuffer + DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) requesting %u sector(s)"), cSectorsPerReq);
            goto Fail;
        }

        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer, bufferSize))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: buffer miscompare; read %u sector(s)"), cSectorsPerReq);
            goto Fail;
        }

        if(!CheckSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) overwrote the buffer"));
            goto Fail;
        }
        
        VirtualFree(pReadBuffer, 0, MEM_RELEASE);
        VirtualFree(pWriteBuffer, 0, MEM_RELEASE);
    }  
    
    return TPR_PASS;
Fail:
    if(pReadBuffer)  VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);

    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadWriteMulti(
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

    DWORD dwStartSector;    
    DWORD cSector = 0;
    
    // sector offset is 1/3 of a sector, DWORD aligned
    DWORD offset = (g_diskInfo.di_bytes_per_sect / 3) +((g_diskInfo.di_bytes_per_sect / 3) % sizeof(DWORD));
    
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;

    DWORD cSectorsPerReq = lpFTE->dwUserData;

    if(cSectorsPerReq > g_dwMaxSectorsPerOp) 
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: %u sectors is larger than maximum allowed (%u)"),
            cSectorsPerReq, g_dwMaxSectorsPerOp);
        return TPR_SKIP;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);
    
    // We need at least 2 sectors 
    ASSERT(cSectorsPerReq >= 2);
    
    DWORD bufferSize = g_diskInfo.di_bytes_per_sect * cSectorsPerReq;

    //ReadBuffer need to be expanded for those stripe bytes.
    pReadBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize+2*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pWriteBuffer || NULL == pReadBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Writing, reading with multiple SG buffers, comparing sectors"));    
/*
    // Check every READ_LOCATIONS number of sectors, with additional random offset    
    for(cSector = 0; 
        cSector < g_diskInfo.di_total_sectors - cSectorsPerReq; // don't run off end of disk 
        cSector += (g_diskInfo.di_total_sectors / (READ_LOCATIONS) + (rand() % 10)))
*/
    // perform writes accross the disk
    for(dwStartSector = 1; 
        dwStartSector < g_diskInfo.di_total_sectors - cSectorsPerReq 
        && dwStartSector > 0; 
        dwStartSector <<= 1)
    {
        cSector = dwStartSector - 1;
        
        MakeJunkBuffer(pWriteBuffer, bufferSize);

        g_pKato->Log(LOG_DETAIL, _T("writing + reading w/multiple SG buffers + comparing %u sectors @ sector %u"), cSectorsPerReq, cSector);
        
        // write the junk buffer
        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, cSectorsPerReq, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) failed"));
            goto Fail;
        }

        // Start 4 types of SG reads

        //
        // 1) start 1/3 into first sector, end 2/3 into first sector
        //
        AddSentinel(pReadBuffer, g_diskInfo.di_bytes_per_sect - 2*offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset,
                       pReadBuffer + DATA_STRIPE_SIZE,
                       g_diskInfo.di_bytes_per_sect - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() failed at sector %u"), cSector);
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer + offset, g_diskInfo.di_bytes_per_sect - 2*offset))
        {            
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, g_diskInfo.di_bytes_per_sect - 2*offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 2) start 1/3 into first sector, end at end of final sector
        //
        AddSentinel(pReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer + offset, bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 3) start at beginning of first sector, end 2/3 through final sector        
        //
        // keep data stripe from prev. because buffer is same size
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, 0, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer, bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 4) start 1/3 into first sector, end 2/3 through final sector
        //
        AddSentinel(pReadBuffer, bufferSize - 2*offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer + offset, bufferSize - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, bufferSize - 2*offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }
        
        // Start 4 types of boundary SG reads

        //
        // 1) start at the second byte into first sector, end at the first byte (included) into final sector
        //
        AddSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, 1,
                       pReadBuffer + DATA_STRIPE_SIZE,
                       bufferSize - g_diskInfo.di_bytes_per_sect))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() failed at sector %u"), cSector);
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer + 1, bufferSize - g_diskInfo.di_bytes_per_sect))
        {            
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 2) start at the last byte of the first sector, end at the last second (included) byte of final sector
        //
        AddSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, g_diskInfo.di_bytes_per_sect - 1, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - g_diskInfo.di_bytes_per_sect))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }
        // verify the read
        if(memcmp(pReadBuffer + DATA_STRIPE_SIZE, pWriteBuffer + g_diskInfo.di_bytes_per_sect - 1, bufferSize - g_diskInfo.di_bytes_per_sect))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

    }

    if(pReadBuffer) VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);  

    return TPR_PASS;

Fail:
    if(pReadBuffer) VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);  

    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestSGWrite(
    UINT uMsg, 
    TPPARAM tpParam, 
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
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD dwRetVal = TPR_FAIL;
    BYTE pbSGBuffer[sizeof(SG_REQ) + sizeof(SG_BUF) * (MAX_SG_BUF)] = {0};
    PSG_REQ pSGReq = (PSG_REQ)pbSGBuffer;

    BYTE *pReadBuffer = NULL;
    DWORD cbMaxBytesPerSG = g_dwMaxSectorsPerOp * g_diskInfo.di_bytes_per_sect;
    DWORD dwStartSector, dwNumSectors, dwSG, dwNumSG, cbWrite, cbOffset;

    HANDLE hHeap = HeapCreate(0, cbMaxBytesPerSG*MAX_SG_BUF, 0);
    if(NULL == hHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapCreate failed; error %u"), GetLastError());
        goto done;         
    }

    // start by allocating all the write SG buffers up front
    for(dwSG = 0; dwSG < MAX_SG_BUF; dwSG++)
    {
        pSGReq->sr_sglist[dwSG].sb_buf = (BYTE*)HeapAlloc(hHeap, 0, cbMaxBytesPerSG);
        if(NULL == pSGReq->sr_sglist[dwSG].sb_buf)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc failed to allocate %u bytes; error %u"), 
                cbMaxBytesPerSG, GetLastError());
            goto done;            
        }
    }

    // allocate a read buffer
    pReadBuffer = (BYTE*)HeapAlloc(hHeap, 0, cbMaxBytesPerSG);
    if(NULL == pReadBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc failed to allocate %u bytes; error %u"), 
            cbMaxBytesPerSG, GetLastError());
        goto done;            
    }

    // perform writes accross the disk
    for(dwStartSector = 1; dwStartSector < g_diskInfo.di_total_sectors && dwStartSector > 0; dwStartSector <<= 1)
    {
        pSGReq->sr_start = dwStartSector - 1;

        // iterate through number of sectors to read
        for(dwNumSectors = 1; dwNumSectors <= g_dwMaxSectorsPerOp; dwNumSectors <<= 1)
        {        
            pSGReq->sr_num_sec = dwNumSectors;

            g_pKato->Log(LOG_DETAIL, _T("writing w/multiple SG buffers + verifying, %u sectors @ sector %u"),
                pSGReq->sr_num_sec, pSGReq->sr_start);

            if(pSGReq->sr_start + pSGReq->sr_num_sec >= g_diskInfo.di_total_sectors)
            {
                // terminate the loop because this request won't fit
                break;
            }
                
            // iterate through sg buffer count
            for(dwNumSG = 2; dwNumSG <= MAX_SG_BUF; dwNumSG++)
            {
                pSGReq->sr_num_sg = dwNumSG;
                
                // total bytes to write this operation
                cbWrite = dwNumSectors * g_diskInfo.di_bytes_per_sect;

                // initialize the SG request buffer sizes
                for(dwSG = 0; dwSG < pSGReq->sr_num_sg-1; dwSG++)
                {
                    // pick a random length for the buffer
                    pSGReq->sr_sglist[dwSG].sb_len = 1 + (Random() % (cbWrite - (pSGReq->sr_num_sg - dwSG)));
                    cbWrite -= pSGReq->sr_sglist[dwSG].sb_len;
                    VERIFY(MakeJunkBuffer(pSGReq->sr_sglist[dwSG].sb_buf, pSGReq->sr_sglist[dwSG].sb_len));
                }

                pSGReq->sr_sglist[dwSG].sb_len = cbWrite;
                VERIFY(MakeJunkBuffer(pSGReq->sr_sglist[dwSG].sb_buf, pSGReq->sr_sglist[dwSG].sb_len));

                // perform the multiple SG write
                if(!ReadWriteDiskMulti(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, pSGReq))
                {
                    g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing %u sectors @ sector %u w/%u SG buffers! SG list:"),
                        pSGReq->sr_num_sec, pSGReq->sr_start, pSGReq->sr_num_sg);
                    for(dwSG = 0; dwSG < pSGReq->sr_num_sg; dwSG++)
                        g_pKato->Log(LOG_DETAIL, _T("    SG_REQ.sr_sglist[%u].sb_len=%u"), dwSG+1, pSGReq->sr_sglist[dwSG].sb_len);
                    
                    goto done;
                }

                ASSERT(pSGReq->sr_num_sec == dwNumSectors);

                // read back data into a single SG buffer
                if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, pSGReq->sr_start, dwNumSectors, pReadBuffer))
                {
                    g_pKato->Log(LOG_DETAIL, _T("ERROR: failed reading %u sectors @ sector %u w/1 SG buffer!"),
                        pSGReq->sr_num_sec, pSGReq->sr_start);
                    goto done;
                }

                // verify the data
                cbOffset = 0;
                for(dwSG = 0; dwSG < pSGReq->sr_num_sg-1; dwSG++)
                {
                    if(0 != memcmp(pReadBuffer + cbOffset, pSGReq->sr_sglist[dwSG].sb_buf, pSGReq->sr_sglist[dwSG].sb_len))
                    {
                        g_pKato->Log(LOG_FAIL, _T("ERROR: bad data in SG buffer #%u when writing %u sectors @ sector %u w/%u SG buffers! SG list:"),
                            dwSG+1, pSGReq->sr_num_sec, pSGReq->sr_start, pSGReq->sr_num_sg);
                        for(dwSG = 0; dwSG < pSGReq->sr_num_sg; dwSG++)
                            g_pKato->Log(LOG_DETAIL, _T("    SG_REQ.sr_sglist[%u].sb_len=%u"), dwSG+1, pSGReq->sr_sglist[dwSG].sb_len);

                        goto done;
                    }
                    cbOffset += pSGReq->sr_sglist[dwSG].sb_len;
                }
            }
        }
    }
    dwRetVal = TPR_PASS;
        
done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    return dwRetVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestSGRead(
    UINT uMsg, 
    TPPARAM tpParam, 
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
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD dwRetVal = TPR_FAIL;
    BYTE pbSGBuffer[sizeof(SG_REQ) + sizeof(SG_BUF) * (MAX_SG_BUF)] = {0};
    PSG_REQ pSGReq = (PSG_REQ)pbSGBuffer;

    BYTE *pWriteBuffer = NULL;
    DWORD cbMaxBytesPerSG = g_dwMaxSectorsPerOp * g_diskInfo.di_bytes_per_sect;
    DWORD dwStartSector, dwNumSectors, dwSG, dwNumSG, cbWrite, cbOffset;

    HANDLE hHeap = HeapCreate(0, cbMaxBytesPerSG*MAX_SG_BUF, 0);
    if(NULL == hHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapCreate failed; error %u"), GetLastError());
        goto done;         
    }

    // start by allocating all the read SG buffers up front
    for(dwSG = 0; dwSG < MAX_SG_BUF; dwSG++)
    {
        pSGReq->sr_sglist[dwSG].sb_buf = (BYTE*)HeapAlloc(hHeap, 0, cbMaxBytesPerSG);
        if(NULL == pSGReq->sr_sglist[dwSG].sb_buf)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc failed to allocate %u bytes; error %u"), 
                cbMaxBytesPerSG, GetLastError());
            goto done;            
        }
    }

    // allocate a write buffer
    pWriteBuffer = (BYTE*)HeapAlloc(hHeap, 0, cbMaxBytesPerSG);
    if(NULL == pWriteBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc failed to allocate %u bytes; error %u"), 
            cbMaxBytesPerSG, GetLastError());
        goto done;            
    }

    // perform read accross the disk
    for(dwStartSector = 1; dwStartSector < g_diskInfo.di_total_sectors && dwStartSector > 0; dwStartSector <<= 1)
    {
        pSGReq->sr_start = dwStartSector - 1;

        // iterate through number of sectors to read
        for(dwNumSectors = 1; dwNumSectors <= g_dwMaxSectorsPerOp; dwNumSectors <<= 1)
        {        
            pSGReq->sr_num_sec = dwNumSectors;

            g_pKato->Log(LOG_DETAIL, _T("writing + verifying w/multiple SG buffers, %u sectors @ sector %u"),
                pSGReq->sr_num_sec, pSGReq->sr_start);

            if(pSGReq->sr_start + pSGReq->sr_num_sec >= g_diskInfo.di_total_sectors)
            {
                // terminate the loop because this request won't fit
                break;
            }
                
            // iterate through sg buffer count
            for(dwNumSG = 2; dwNumSG <= MAX_SG_BUF; dwNumSG++)
            {
                // write out data from a single SG buffer
                VERIFY(MakeJunkBuffer(pWriteBuffer, cbMaxBytesPerSG));
                if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, pSGReq->sr_start, dwNumSectors, pWriteBuffer))
                {
                    g_pKato->Log(LOG_DETAIL, _T("ERROR: failed writing %u sectors @ sector %u w/1 SG buffer!"),
                        pSGReq->sr_num_sec, pSGReq->sr_start);
                    goto done;
                }

                pSGReq->sr_num_sg = dwNumSG;
                
                // total bytes to read this operation
                cbWrite = dwNumSectors * g_diskInfo.di_bytes_per_sect;

                // initialize the SG request buffer sizes
                for(dwSG = 0; dwSG < pSGReq->sr_num_sg-1; dwSG++)
                {
                    // pick a random length for the buffer
                    pSGReq->sr_sglist[dwSG].sb_len = 1 + (Random() % (cbWrite - (pSGReq->sr_num_sg - dwSG)));
                    cbWrite -= pSGReq->sr_sglist[dwSG].sb_len;
                }

                pSGReq->sr_sglist[dwSG].sb_len = cbWrite;

                // perform the multiple SG read
                if(!ReadWriteDiskMulti(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, pSGReq))
                {
                    g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading %u sectors @ sector %u w/%u SG buffers! SG list:"),
                        pSGReq->sr_num_sec, pSGReq->sr_start, pSGReq->sr_num_sg);
                    for(dwSG = 0; dwSG < pSGReq->sr_num_sg; dwSG++)
                        g_pKato->Log(LOG_DETAIL, _T("    SG_REQ.sr_sglist[%u].sb_len=%u"), dwSG+1, pSGReq->sr_sglist[dwSG].sb_len);
                    
                    goto done;
                }

                ASSERT(pSGReq->sr_num_sec == dwNumSectors);

                // verify the data
                cbOffset = 0;
                for(dwSG = 0; dwSG < pSGReq->sr_num_sg-1; dwSG++)
                {
                    if(0 != memcmp(pWriteBuffer + cbOffset, pSGReq->sr_sglist[dwSG].sb_buf, pSGReq->sr_sglist[dwSG].sb_len))
                    {
                        g_pKato->Log(LOG_FAIL, _T("ERROR: bad data in SG buffer #%u when reading %u sectors @ sector %u w/%u SG buffers! SG list:"),
                            dwSG+1, pSGReq->sr_num_sec, pSGReq->sr_start, pSGReq->sr_num_sg);
                        for(dwSG = 0; dwSG < pSGReq->sr_num_sg; dwSG++)
                            g_pKato->Log(LOG_DETAIL, _T("    SG_REQ.sr_sglist[%u].sb_len=%u"), dwSG+1, pSGReq->sr_sglist[dwSG].sb_len);

                        goto done;
                    }
                    cbOffset += pSGReq->sr_sglist[dwSG].sb_len;
                }
            }
        }
    }
    
    dwRetVal = TPR_PASS;
        
done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    return dwRetVal;
}



// --------------------------------------------------------------------
TESTPROCAPI 
TestReadPastEnd(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cBufferSize = g_diskInfo.di_bytes_per_sect * 3;
    PBYTE pBuffer = (PBYTE)VirtualAlloc(NULL, cBufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(!pBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc failed to create buffer"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Attempting to read past end of disk..."));
    
    // read last sector so buffer extends 1 sector past end of disk
    if(ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, g_diskInfo.di_total_sectors - 1, 2, pBuffer))
    {
        // warn on success
        g_pKato->Log(LOG_DETAIL, _T("WARNING: successfully read past end of disk"));
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("Failed to read past end of disk as expected."));
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_PASS;
    
Fail:
    if(pBuffer) VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadOffDisk(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cBufferSize = g_diskInfo.di_bytes_per_sect * 3;
    PBYTE pBuffer = (PBYTE)VirtualAlloc(NULL, cBufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(!pBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc failed to create buffer"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Attempting to read past end of disk..."));

    // read 100 sectors past end of disk
    if(ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, g_diskInfo.di_total_sectors + 100, 2, pBuffer))
    {
        // warn on success
        g_pKato->Log(LOG_DETAIL, _T("WARNING: successfully read past end of disk"));
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("Failed to read past end of disk as expected."));
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_PASS;
    
Fail:
    if(pBuffer) VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_FAIL;
}


// --------------------------------------------------------------------
TESTPROCAPI 
TestWritePastEnd(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cBufferSize = g_diskInfo.di_bytes_per_sect * 3;
    PBYTE pBuffer = (PBYTE)VirtualAlloc(NULL, cBufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(!pBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc failed to crete buffer"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Attempting to write past end of disk..."));
    
    // write last sector so buffer extends 1 sector past end of disk
    if(ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, g_diskInfo.di_total_sectors - 1, 2, pBuffer))
    {
        // warn on success
        g_pKato->Log(LOG_DETAIL, _T("WARNING: successfully wrote past end of disk"));
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("Failed to write past end of disk as expected."));
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_PASS;
    
Fail:
    if(pBuffer) VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestWriteOffDisk(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cBufferSize = g_diskInfo.di_bytes_per_sect * 3;
    PBYTE pBuffer = (PBYTE)VirtualAlloc(NULL, cBufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(!pBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc failed to crete buffer"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Attempting to write past end of disk..."));

    // write 100 sectors past end of disk
    if(ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, g_diskInfo.di_total_sectors + 100, 2, pBuffer))
    {
        // warn on success
        g_pKato->Log(LOG_DETAIL, _T("WARNING: successfully wrote past end of disk"));
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("Failed to write past end of disk as expected."));
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_PASS;
    
Fail:
    if(pBuffer) VirtualFree(pBuffer, 0, MEM_RELEASE);
    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestSGBoundsCheck(
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

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cSector = lpFTE->dwUserData;
    DWORD cBytes = 0;
    DWORD retVal = TPR_PASS;
    UINT splitSize = 0;

    PBYTE pPreBuffer = NULL;
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;
    
    BYTE reqData[sizeof(SG_REQ) + sizeof(SG_BUF)] = {0};
    PSG_REQ pSgReq = (PSG_REQ)reqData;   

    pPreBuffer = (PBYTE)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);
    pReadBuffer = (PBYTE)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);

    if(!pPreBuffer || !pReadBuffer || !pWriteBuffer)
    {
        g_pKato->Log(LOG_FAIL, L"Out of memory");
        retVal = TPR_FAIL;
        goto Fail;
    }       

    // perform all read/write at the same location
    pSgReq->sr_start = cSector;

    // try all pre buffer sizes (1 through sectorsize-1)
    g_pKato->Log(LOG_DETAIL, L"writing, then reading back %u sectors @ sector %u using 2 SG buffers of every possible size...", pSgReq->sr_num_sec, pSgReq->sr_start);
    for(splitSize = 1; splitSize < g_diskInfo.di_bytes_per_sect; splitSize++)
    {
        // set up sg request for writing, 1 sg buffer        
        pSgReq->sr_num_sg = 1;
        pSgReq->sr_num_sec = 1;
        pSgReq->sr_sglist[0].sb_buf = pWriteBuffer;
        pSgReq->sr_sglist[0].sb_len = g_diskInfo.di_bytes_per_sect;
        
        VERIFY(MakeJunkBuffer(pWriteBuffer, g_diskInfo.di_bytes_per_sect));

        // write data
        if(!DeviceIoControl(g_hDisk, DISK_IOCTL_WRITE, pSgReq, sizeof(SG_REQ), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL DeviceIoControl(DISK_IOCTL_WRITE) 1 SG buffer failed; error %u\r\n", GetLastError());        
            retVal = TPR_FAIL;
            goto Fail;
        }
        
        // set up sg request for reading, 2 sg buffers
        pSgReq->sr_num_sg = 2;

        // pre-read buffer, to hold the data before the read buffer starts
        pSgReq->sr_sglist[0].sb_buf = pPreBuffer;
        pSgReq->sr_sglist[0].sb_len = splitSize;

        // read buffer (starts one byte into the sector)
        pSgReq->sr_sglist[1].sb_buf = pReadBuffer;
        pSgReq->sr_sglist[1].sb_len = g_diskInfo.di_bytes_per_sect - splitSize;

        g_pKato->Log(LOG_DETAIL, L"reading %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
            pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
        if(!DeviceIoControl(g_hDisk, DISK_IOCTL_READ, pSgReq, (sizeof(SG_REQ) + sizeof(SG_BUF)), NULL, 0, &cBytes, NULL))
        {
           g_pKato->Log(LOG_FAIL, L"ERROR: failed reading %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                        pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
            retVal = TPR_FAIL;
            goto Fail;
        }

        if(memcmp(pWriteBuffer, pPreBuffer, splitSize))
        {
            g_pKato->Log(LOG_FAIL, L"ERROR: bad data in SG buffer 1 after writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
            retVal = TPR_FAIL;
            continue;
        }

        if(memcmp(pWriteBuffer + splitSize, pReadBuffer, g_diskInfo.di_bytes_per_sect - splitSize))
        {
            g_pKato->Log(LOG_FAIL, L"ERROR: bad data in SG buffer 2 after writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
            retVal = TPR_FAIL;
            continue;
        }
    }

    // try all pre buffer sizes (1 through sectorsize-1)
    g_pKato->Log(LOG_DETAIL, L"writing %u sectors @ sector %u using 2 SG buffers of every possible size, then reading back...", pSgReq->sr_num_sec, pSgReq->sr_start);
    for(splitSize = 1; splitSize < g_diskInfo.di_bytes_per_sect; splitSize++)
    {        
        // set up sg request for writing, 2 sg buffers
        pSgReq->sr_num_sg = 2;

        // pre-write buffer, to hold the data before the write buffer starts
        pSgReq->sr_sglist[0].sb_buf = pPreBuffer;
        pSgReq->sr_sglist[0].sb_len = splitSize;

        // write buffer (starts one byte into the sector)
        pSgReq->sr_sglist[1].sb_buf = pWriteBuffer;
        pSgReq->sr_sglist[1].sb_len = g_diskInfo.di_bytes_per_sect - splitSize;

        VERIFY(MakeJunkBuffer(pPreBuffer, g_diskInfo.di_bytes_per_sect));
        VERIFY(MakeJunkBuffer(pWriteBuffer, g_diskInfo.di_bytes_per_sect));

        g_pKato->Log(LOG_DETAIL, L"writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
            pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);        
        if(!DeviceIoControl(g_hDisk, DISK_IOCTL_WRITE, pSgReq, (sizeof(SG_REQ) + sizeof(SG_BUF)), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, L"ERROR: failed writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                        pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
            retVal = TPR_FAIL;
            goto Fail;
        }

        // set up sg request for reading, 1 sg buffer
        pSgReq->sr_start = 0;
        pSgReq->sr_num_sg = 1;
        pSgReq->sr_num_sec = 1;
        pSgReq->sr_sglist[0].sb_buf = pReadBuffer;
        pSgReq->sr_sglist[0].sb_len = g_diskInfo.di_bytes_per_sect;

        // read data
        if(!DeviceIoControl(g_hDisk, DISK_IOCTL_READ, pSgReq, sizeof(SG_REQ), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL DeviceIoControl(DISK_IOCTL_READ) 1 SG buffer failed; error %u\r\n", GetLastError());        
            retVal = TPR_FAIL;
            goto Fail;
        }

        if(memcmp(pReadBuffer, pPreBuffer, splitSize))
        {
            g_pKato->Log(LOG_FAIL, L"ERROR: bad data in SG buffer 1 after writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                        pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);

            retVal = TPR_FAIL;
            continue;
        }

        if(memcmp(pReadBuffer + splitSize, pWriteBuffer, g_diskInfo.di_bytes_per_sect - splitSize))
        {
            g_pKato->Log(LOG_FAIL, L"ERROR: bad data in SG buffer 2 after writing %u sectors @ sector %u using 2 SG buffers (SG_REQ.sr_sglist[0].sb_len=%u, SG_REQ.sr_sglist[1].sb_len=%u)", 
                pSgReq->sr_num_sec, pSgReq->sr_start, pSgReq->sr_sglist[0].sb_len, pSgReq->sr_sglist[1].sb_len);
            retVal = TPR_FAIL;
            continue;
        }
    }
    
    retVal = TPR_PASS;
    
Fail:
    if(pPreBuffer) VirtualFree(pPreBuffer, 0, MEM_RELEASE);
    if(pReadBuffer) VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);
    
    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// TestInvalidDeleteSectors
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI TestInvalidDeleteSectors(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(); 
		return TPR_SKIP; 
	}

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk)) {
        g_pKato->Log(LOG_SKIP, _T("FAIL: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    DWORD retVal = TPR_PASS;
    PBYTE pBuffer = NULL;
    PBYTE pVerifyBuffer = NULL;
    
    g_pKato->Log(LOG_DETAIL, L"trying IOCTL_DISK_DELETE_SECTORS...");
    if(!DeleteSectors(g_hDisk, 0, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, L"formatting the media to free all sectors...");
    if(!FormatMedia(g_hDisk)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: FormatMedia()"));
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, L"attempting to delete sector zero...");
    if(!DeleteSectors(g_hDisk, 0, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, L"attempting to delete all sectors...");
    if(!DeleteSectors(g_hDisk, 0, g_diskInfo.di_total_sectors)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: DeleteSectors(); error = %u"), GetLastError());
        retVal = TPR_FAIL;
    }
    
    g_pKato->Log(LOG_DETAIL, L"attempting to delete last sector + 1...");
    if(DeleteSectors(g_hDisk, g_diskInfo.di_total_sectors, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: DeleteSectors() succeeded past end of disk"));
    }

    g_pKato->Log(LOG_DETAIL, L"attempting to delete last sector + 2...");
    if(DeleteSectors(g_hDisk, g_diskInfo.di_total_sectors + 1, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: DeleteSectors() succeeded past end of disk"));
    }

    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_diskInfo.di_bytes_per_sect);
    pVerifyBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_diskInfo.di_bytes_per_sect);
    if(NULL == pBuffer || NULL == pVerifyBuffer) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: LocalAlloc(); error = %u"), GetLastError());
        retVal = TPR_FAIL;
        goto Fail;
    }
    // set both buffers to the same random data for later verification
    MakeJunkBuffer(pBuffer, g_diskInfo.di_bytes_per_sect);
    memcpy(pVerifyBuffer, pBuffer, g_diskInfo.di_bytes_per_sect);

    // write known data to the last sector on the disk
    g_pKato->Log(LOG_DETAIL, L"writing to last sector...");
    if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, g_diskInfo.di_total_sectors-1, 1, pBuffer)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk() ; error = %u"), GetLastError());
        retVal = TPR_FAIL;
    }

    // attempt to free two sectors: one that is valid and one that is not valid
    g_pKato->Log(LOG_DETAIL, L"attempting to delete 2 sectors starting on final sector %u...", g_diskInfo.di_total_sectors-1);
    if(DeleteSectors(g_hDisk, g_diskInfo.di_total_sectors-1, 2)) {
        g_pKato->Log(LOG_FAIL, _T("WARNING: DeleteSectors() failed with request ending beyong media"));
    } else {
        // since the DeleteSectors call failed, we expect that the final sector was not deleted
        // so read it back to verify the data is still correct
        memset(pBuffer, 0, g_diskInfo.di_bytes_per_sect);
        g_pKato->Log(LOG_DETAIL, L"reading back last sector...");
        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, g_diskInfo.di_total_sectors-1, 1, pBuffer)) {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk() ; error = %u"), GetLastError());
            retVal = TPR_FAIL;
        }

        // verify data
        if(0 != memcmp(pBuffer, pVerifyBuffer, g_diskInfo.di_bytes_per_sect)) {
            g_pKato->Log(LOG_FAIL, _T("FAIL: DeleteSectors failed even though it deleted the last sector"), GetLastError());
            retVal = TPR_FAIL;
        }
    }

    g_pKato->Log(LOG_DETAIL, L"writing to sector zero...");
    if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, 0, 1, pBuffer)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk() ; error = %u"), GetLastError());
        retVal = TPR_FAIL;
    }
    g_pKato->Log(LOG_DETAIL, L"attempting to delete sector zero...");
    if(!DeleteSectors(g_hDisk, 0, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
    }
    g_pKato->Log(LOG_DETAIL, L"attempting to delete sector zero again...");
    if(!DeleteSectors(g_hDisk, 0, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
    }
    
    g_pKato->Log(LOG_DETAIL, L"writing to last sector...");
    if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, g_diskInfo.di_total_sectors-1, 1, pBuffer)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk() ; error = %u"), GetLastError());
        retVal = TPR_FAIL;
    }

    g_pKato->Log(LOG_DETAIL, L"attempting to delete the last sector...");
    if(!DeleteSectors(g_hDisk, g_diskInfo.di_total_sectors-1, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
    }
    g_pKato->Log(LOG_DETAIL, L"attempting to delete the last sector...");
    if(!DeleteSectors(g_hDisk, g_diskInfo.di_total_sectors-1, 1)) {
        g_pKato->Log(LOG_DETAIL, _T("DeleteSectors() failed; this disk may not support IOCTL_DISK_DELETE_SECTORS; error = %u"), GetLastError());
        retVal = TPR_SKIP;
    }
        
Fail:
    if(pBuffer) {
        LocalFree(pBuffer);
    }
    return retVal;
}

////////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------------
BOOL
AlignmentTest(
    DWORD dwIoctl, 
    DWORD sector, 
    DWORD cSectors,
    PBYTE pBuffer,
    DWORD cbBuffer)
// --------------------------------------------------------------------
{
    BOOL fRet = TRUE;
    PBYTE pAlignedBuffer;

    // buffer must be dword-aligned
    ASSERT(0 == (((DWORD)pBuffer) & (sizeof(DWORD)-1)));
    if(((DWORD)pBuffer) & (sizeof(DWORD)-1))
    {
        g_pKato->Log(LOG_DETAIL, TEXT("AlignmentTest input buffer is not dword-aligned: 0x%08x"),
            pBuffer);
        return FALSE;
    }
    
    // buffer must be large enough to hold the requested sectors + an additional quad-word
    ASSERT(cbBuffer >= g_diskInfo.di_bytes_per_sect*cSectors + sizeof(DWORD));
    if(cbBuffer < g_diskInfo.di_bytes_per_sect*cSectors + sizeof(DWORD))
    {
        g_pKato->Log(LOG_DETAIL, TEXT("AlignmentTest input buffer too small (%u bytes, %u bytes required)"),
            cbBuffer, g_diskInfo.di_bytes_per_sect*cSectors + sizeof(DWORD));
        return FALSE;
    }

    if(0 == (((DWORD)pBuffer) & (UserKInfo[KINX_PAGESIZE]-1)))
    {
        pAlignedBuffer = pBuffer;
        __try
        {   
            g_pKato->Log(LOG_DETAIL, TEXT("testing page-aligned buffer (buffer=0x%08x).."), pAlignedBuffer);
            if(!ReadWriteDisk(g_hDisk, dwIoctl, &g_diskInfo, sector, cSectors, pAlignedBuffer))
            {
                g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using page-aligned buffer failed; error %u"), GetLastError());
                fRet = FALSE;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using page-aligned buffer caused an exception"));
            fRet = FALSE;
        }
    }

    pAlignedBuffer = pBuffer + sizeof(DWORD);
    __try
    {   
        g_pKato->Log(LOG_DETAIL, TEXT("testing dword-aligned buffer (buffer=0x%08x).."), pAlignedBuffer);
        if(!ReadWriteDisk(g_hDisk, dwIoctl, &g_diskInfo, sector, cSectors, pAlignedBuffer))
        {
            g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using dword-aligned buffer failed; error %u"), GetLastError());
            fRet = FALSE;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using dword-aligned buffer caused an exception"));
        fRet = FALSE;
    }

    pAlignedBuffer = pBuffer + sizeof(WORD);
    __try
    {   
        g_pKato->Log(LOG_DETAIL, TEXT("testing word-aligned buffer (buffer=0x%08x).."), pAlignedBuffer);
        if(!ReadWriteDisk(g_hDisk, dwIoctl, &g_diskInfo, sector, cSectors, pAlignedBuffer))
        {
            g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using word-aligned buffer failed; error %u"), GetLastError());
            fRet = FALSE;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using word-aligned buffer caused an exception"));
        fRet = FALSE;
    }

    pAlignedBuffer = pBuffer + sizeof(BYTE);
    __try
    {   
        g_pKato->Log(LOG_DETAIL, TEXT("testing byte-aligned buffer (buffer=0x%08x).."), pAlignedBuffer);
        if(!ReadWriteDisk(g_hDisk, dwIoctl, &g_diskInfo, sector, cSectors, pAlignedBuffer))
        {
            g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using byte-aligned buffer failed; error %u"), GetLastError());
            fRet = FALSE;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("ERROR: i/o using byte-aligned buffer caused an exception"));
        fRet = FALSE;
    }

    return fRet;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestWriteBufferTypes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    ASSERT(lpFTE);
    
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    FormatMedia(g_hDisk);

    BYTE pbStack[DEF_SECTOR_SIZE+sizeof(DWORD)];
    static BYTE pbDataSection[DEF_SECTOR_SIZE+sizeof(DWORD)];
    BYTE *pbVirtual = NULL;
    BYTE *pbHeap = NULL;

    if(g_diskInfo.di_bytes_per_sect > DEF_SECTOR_SIZE)
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: non-standard sector size: %u bytes"), 
            g_diskInfo.di_bytes_per_sect);
        return TPR_SKIP;
    }

    BYTE reqBuffer[sizeof(SG_REQ) + sizeof(SG_BUF) * 3] = {0};
    PSG_REQ pSGReq = (PSG_REQ)reqBuffer;

    // first, test the ability to write data from the stack allocated buffer
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using stack buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbStack, sizeof(pbStack)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with stack buffer"));
        goto Fail;
    }

    // next, test the ability to wride data from the static buffer (from the global data section)
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using global buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbDataSection, sizeof(pbDataSection)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with global buffer"));
        goto Fail;
    }

    // next, test the ability to write data from a heap allocated buffer
    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, g_diskInfo.di_bytes_per_sect+sizeof(DWORD));
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using heap buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbHeap, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with heap buffer"));
        goto Fail;
    }    
    VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    pbHeap = NULL;

    // next, test the ability to write data from a read-only virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_READONLY);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READONLY) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using valloc read-only buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with valloc read-only buffer"));
        goto Fail;
    }        
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    // next, test the ability to write data from a read/execute virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_EXECUTE_READ);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READ) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using valloc read/execute buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with valloc read/execute buffer"));
        goto Fail;
    }
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    // next, test the ability to write data from a r/w virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READWRITE) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using valloc read/write buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with valloc read/write buffer"));
        goto Fail;
    }  
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    // next, test the ability to write data from a r/w/x virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READWRITE) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("writing %u sectors @ sector %u using valloc read/write/execute buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_WRITE, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed writing with valloc read/write/execute buffer"));
        goto Fail;
    }
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, g_diskInfo.di_bytes_per_sect+sizeof(DWORD));
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }
    
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect, MEM_COMMIT, PAGE_READONLY);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READONLY) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }

    // now, do a multi-sg read with an assortment of buffer types
    pSGReq->sr_start = 0;
    pSGReq->sr_num_sec = 0;
    pSGReq->sr_num_sg = 0;

    // sg 0 = stack buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbStack;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;
    
    // sg 1 = global buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbDataSection;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;

    // sg 2 = valloc buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbVirtual;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;


    // sg 3 = byte-aligned heap buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbHeap+sizeof(BYTE);
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;

    // one sector per buffer
    pSGReq->sr_num_sec = pSGReq->sr_num_sg;

    __try
    {
        DWORD dwReceived;
        g_pKato->Log(LOG_DETAIL, _T("writing 1 sector from each of 4 sg buffers (stack, global, valloc (read-only), and byte-aligned heap) at sector %u"), 0);
        if(!DeviceIoControl(g_hDisk, g_fOldIoctls ? DISK_IOCTL_WRITE : IOCTL_DISK_WRITE, pSGReq, sizeof(reqBuffer), NULL, 0, &dwReceived, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: write with multi-sg buffers failed; error %u"), GetLastError());
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: write with multi-sg buffers caused an exception"));
        goto Fail;
    }
    
    VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    pbHeap = NULL;

    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));
    pbVirtual = NULL;

    return TPR_PASS;
    
Fail:
    if(pbHeap) 
    {
        VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    }
    if(pbVirtual)
    {
        VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));
    }
    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadBufferTypes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    ASSERT(lpFTE);
    
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global mass storage device handle is invalid"));
        return TPR_SKIP;
    }

    BYTE pbStack[DEF_SECTOR_SIZE+sizeof(DWORD)];
    static BYTE pbDataSection[DEF_SECTOR_SIZE+sizeof(DWORD)];
    BYTE *pbVirtual = NULL;
    BYTE *pbHeap = NULL;

    if(g_diskInfo.di_bytes_per_sect > DEF_SECTOR_SIZE)
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: non-standard sector size: %u bytes"), 
            g_diskInfo.di_bytes_per_sect);
        return TPR_SKIP;
    }

    BYTE reqBuffer[sizeof(SG_REQ) + sizeof(SG_BUF) * 3] = {0};
    PSG_REQ pSGReq = (PSG_REQ)reqBuffer;

    // first, test the ability to write data from the stack allocated buffer
    g_pKato->Log(LOG_DETAIL, _T("reading %u sectors @ sector %u using stack buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_READ, 0, 1, pbStack, sizeof(pbStack)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading with stack buffer"));
        goto Fail;
    }

    // next, test the ability to wride data from the static buffer (from the global data section)
    g_pKato->Log(LOG_DETAIL, _T("reading %u sectors @ sector %u using global buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_READ, 0, 1, pbDataSection, sizeof(pbDataSection)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading with global buffer"));
        goto Fail;
    }

    // next, test the ability to write data from a heap allocated buffer
    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, g_diskInfo.di_bytes_per_sect+sizeof(DWORD));
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("reading %u sectors @ sector %u using heap buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_READ, 0, 1, pbHeap, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading with heap buffer"));
        goto Fail;
    }    
    VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    pbHeap = NULL;

    // next, test the ability to write data from a r/w virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READWRITE) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("reading %u sectors @ sector %u using valloc read/write buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_READ, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading with valloc read/write buffer"));
        goto Fail;
    }  
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    // next, test the ability to write data from a r/w/x virtual buffer
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect+sizeof(DWORD), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READWRITE) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }
    g_pKato->Log(LOG_DETAIL, _T("reading %u sectors @ sector %u using valloc read/write/execute buffer..."), 1, 0);        
    if(!AlignmentTest(IOCTL_DISK_READ, 0, 1, pbVirtual, g_diskInfo.di_bytes_per_sect+sizeof(DWORD)))
    {
        g_pKato->Log(LOG_FAIL, _T("ERROR: failed reading with valloc read/write/execute buffer"));
        goto Fail;
    }
    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, g_diskInfo.di_bytes_per_sect+sizeof(DWORD));
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }
    
    pbVirtual = (BYTE*)VirtualAlloc(NULL, g_diskInfo.di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READWRITE) failed; error = %u"), 
            GetLastError());
        goto Fail;
    }

    // now, do a multi-sg read with an assortment of buffer types
    pSGReq->sr_start = 0;
    pSGReq->sr_num_sec = 0;
    pSGReq->sr_num_sg = 0;

    // sg 0 = stack buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbStack;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;
    
    // sg 1 = global buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbDataSection;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;

    // sg 2 = valloc buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbVirtual;
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;


    // sg 3 = byte-aligned heap buffer
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_buf = pbHeap+sizeof(BYTE);
    pSGReq->sr_sglist[pSGReq->sr_num_sg].sb_len = g_diskInfo.di_bytes_per_sect;
    pSGReq->sr_num_sg++;

    // one sector per buffer
    pSGReq->sr_num_sec = pSGReq->sr_num_sg;

    __try
    {
        DWORD dwReceived;
        g_pKato->Log(LOG_DETAIL, _T("reading 1 sector into each of 4 sg buffers (stack, global, valloc, and byte-aligned heap) at sector %u"), 0);
        if(!DeviceIoControl(g_hDisk, g_fOldIoctls ? DISK_IOCTL_READ : IOCTL_DISK_READ, pSGReq, sizeof(reqBuffer), NULL, 0, &dwReceived, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read with multi-sg buffers failed; error %u"), GetLastError());
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read with multi-sg buffers caused an exception"));
        goto Fail;
    }
    
    VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    pbHeap = NULL;

    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));
    pbVirtual = NULL;

    return TPR_PASS;
    
Fail:
    if(pbHeap) 
    {
        VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    }
    if(pbVirtual)
    {
        VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));
    }
    return TPR_FAIL;
}
