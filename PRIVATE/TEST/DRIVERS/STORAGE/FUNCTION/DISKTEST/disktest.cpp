//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "main.h"

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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cSectorsPerReq = lpFTE->dwUserData;
    DWORD cSector = 0;
    DWORD cSectorsChecked =0;
    
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

    g_pKato->Log(LOG_DETAIL, _T("Writing, reading, comparing sectors, %u sector(s) at a time"), cSectorsPerReq);
    // Check every READ_LOCATIONS number of sectors, with additional random offset
    for(cSector = 0; 
        cSector < g_diskInfo.di_total_sectors - cSectorsPerReq; // don't run off end of disk 
        cSector += (g_diskInfo.di_total_sectors / (READ_LOCATIONS) + (rand() % 10)))
    {    
        //if this is the last run, then force to R/W the last sectors on the disk
        if( (cSector + g_diskInfo.di_total_sectors / (READ_LOCATIONS)) >= (g_diskInfo.di_total_sectors - cSectorsPerReq))
        {
            cSector = g_diskInfo.di_total_sectors - cSectorsPerReq;
        }
        
        // put junk in the buffer
        MakeJunkBuffer(pWriteBuffer, bufferSize);

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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cSectorsPerReq = 0;
    DWORD cSector = 0;
    DWORD bufferSize = 0;

    // read/write buffers, allocated at each pass
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;    

    for(cSectorsPerReq = 1; 
        (cSectorsPerReq < (MAX_BUFFER_SIZE / g_diskInfo.di_bytes_per_sect))
        && (cSectorsPerReq <= g_diskInfo.di_total_sectors); cSectorsPerReq <<= 1)
    {
        g_pKato->Log(LOG_DETAIL, _T("Writing, reading, comparing sectors, %u sectors at a time"), cSectorsPerReq);
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
    
    g_pKato->Log(LOG_DETAIL, _T("Success"));

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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    DWORD cSector = 0;
    
    // sector offset is 1/3 of a sector, DWORD aligned
    DWORD offset = (g_diskInfo.di_bytes_per_sect / 3) +((g_diskInfo.di_bytes_per_sect / 3) % sizeof(DWORD));
    
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;

    DWORD cSectorsPerReq = lpFTE->dwUserData;
    //We need at least 2 sectors 
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
    // Check every READ_LOCATIONS number of sectors, with additional random offset
    for(cSector = 0; 
        cSector < g_diskInfo.di_total_sectors - cSectorsPerReq; // don't run off end of disk 
        cSector += (g_diskInfo.di_total_sectors / (READ_LOCATIONS) + (rand() % 10)))
    {
        MakeJunkBuffer(pWriteBuffer, bufferSize);
        
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

    g_pKato->Log(LOG_DETAIL, _T("Success"));

    return TPR_PASS;

Fail:
    if(pReadBuffer) VirtualFree(pReadBuffer, 0, MEM_RELEASE);
    if(pWriteBuffer) VirtualFree(pWriteBuffer, 0, MEM_RELEASE);  

    return TPR_FAIL;
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Read past end of disk"));
        goto Fail;
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    g_pKato->Log(LOG_DETAIL, _T("Failed to read past end of disk... Success."));
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Read past end of disk"));
        goto Fail;
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    g_pKato->Log(LOG_DETAIL, _T("Failed to read past end of disk... Success."));
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Read past end of disk"));
        goto Fail;
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    g_pKato->Log(LOG_DETAIL, _T("Failed to write past end of disk... Success."));
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Read past end of disk"));
        goto Fail;
    }
    
    VirtualFree(pBuffer, 0, MEM_RELEASE);
    g_pKato->Log(LOG_DETAIL, _T("Failed to write past end of disk... Success."));
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
    }

    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

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
    
    // write known data to disk 
    // (each byte is set equal to it's byte position in the sector % 0xFF)
    for(cBytes = 0; cBytes < g_diskInfo.di_bytes_per_sect; cBytes++)
        pWriteBuffer[cBytes] = (BYTE)cBytes;

    // set up sg request for writing, 1 sg buffer
    pSgReq->sr_start = 0;
    pSgReq->sr_num_sg = 1;
    pSgReq->sr_num_sec = 1;
    pSgReq->sr_sglist[0].sb_buf = pWriteBuffer;
    pSgReq->sr_sglist[0].sb_len = g_diskInfo.di_bytes_per_sect;    

    // write data
    if(!DeviceIoControl(g_hDisk, DISK_IOCTL_WRITE, pSgReq, sizeof(SG_REQ), NULL, 0, &cBytes, NULL))
    {
        g_pKato->Log(LOG_FAIL, L"FAIL DeviceIoControl(DISK_IOCTL_WRITE) failed; error %u\r\n", GetLastError());        
        retVal = TPR_FAIL;
        goto Fail;
    }

    // try all pre buffer sizes (1 through sectorsize-1)
    for(splitSize = 1; splitSize < g_diskInfo.di_bytes_per_sect; splitSize++)
    {
        // set up sg request for reading, 2 sg buffers
        pSgReq->sr_num_sg = 2;

        // pre-read buffer, to hold the data before the read buffer starts
        pSgReq->sr_sglist[0].sb_buf = pPreBuffer;
        pSgReq->sr_sglist[0].sb_len = splitSize;

        // read buffer (starts one byte into the sector)
        pSgReq->sr_sglist[1].sb_buf = pReadBuffer;
        pSgReq->sr_sglist[1].sb_len = g_diskInfo.di_bytes_per_sect - splitSize;

        if(!DeviceIoControl(g_hDisk, DISK_IOCTL_READ, pSgReq, (sizeof(SG_REQ) + sizeof(SG_BUF)), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL DeviceIoControl(DISK_IOCTL_READ) failed; error %u\r\n", GetLastError());
            retVal = TPR_FAIL;
            goto Fail;
        }

        // make sure the data read at offset 1 is equal to the data written
        if(memcmp(pWriteBuffer + splitSize, pReadBuffer, g_diskInfo.di_bytes_per_sect - splitSize))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL Data miscompare error\r\n");
            retVal = TPR_FAIL;
            continue;
        }
        g_pKato->Log(LOG_DETAIL, L"PASS");
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk)) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global Hard Disk handle is invalid"));
        return TPR_FAIL;
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

