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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "windows.h"
#include "winbase.h"
#include "main.h"
#include "globals.h"
#include "utility.h"
#include "perf.h"

#include <BTSPerfLog.h>

#define DEF_SECTOR_SIZE     512
#define DATA_STRIPE         0x2BC0FFEE
#define DATA_STRIPE_SIZE    sizeof(DWORD)

// data stripe functions, put 4 byte data stripe at start and end of buffer

static DWORD g_startSector = 0;
static TCHAR g_Serial[100] = {'\0'};
static TCHAR g_MFG[100] = {'\0'};
static ULONGLONG g_DiskSize = 0;
static TCHAR *g_Format = L"Bps AVG=%%Bytes%%/%%AVG%%,Bps MIN=%%Bytes%%/%%MAX%%,Bps MAX=%%Bytes%%/%%MIN%%";


// BTS Perflog variables
LPCWSTR g_metricsNames[] = {
    {L"Read Throughput (bytes/s)"},
    {L"Write Throughput (bytes/s)"},
    {L"Absolute Zero"},
    {L"Speed of Light"},
    NULL
};

const LPWSTR SESSION_NAMESPACE  = L"MDPG\\Storage\\Disktest";
const LPWSTR SCENARIO_NAMESPACE = L"Storage\\Disk Performance Test";
DiskTest_Perf *g_btsPerfLog = NULL;

static enum ENUM_TESTS
{
    DISKTEST_SECTOR_RW_PERF = 0,
    DISKTEST_SG_RW_PERF
};

// {0F23DDBB-881E-4ebf-BFA6-BFC28577F25E}
static GUID g_PERFGUID[] = {
    { 0x0f23ddbb, 0x881e, 0x4ebf, { 0xbf, 0xa6, 0xbf, 0xc2, 0x85, 0x77, 0xf2, 0x5e }},
    { 0x8b0b4778, 0x8100, 0x4d10, { 0xad, 0x13, 0x39, 0x52, 0xf8, 0xb0, 0xbc, 0x6f }}
};


bool DiskTest_Perf::PerfWriteLog(HANDLE h, PVOID pv)
{
    return BTSPerfLog::PerfWriteLog(h, pv);
}


bool DiskTest_Perf::PerfWriteLog()
{
    LARGE_INTEGER freq;
    float SecsToRead, SecsToWrite;
    DWORD dwReadKBps, dwWrittenKBps;
    bool fRet1, fRet2, fRet3, fRet4;
    enum {
        READ, 
        WRITE, 
        MIN, 
        MAX
    };

    lptlsData p = GetTlsData();
    if (p == NULL)
    {
        return false;
    }


    // Add profile information to the log file.
    AddAuxData(L"Profile", g_szProfile);


    // Calculate the statistics to log.
    QueryPerformanceFrequency(&freq);

    SecsToRead = static_cast<float>(p->read_duration.QuadPart)/static_cast<float>(freq.QuadPart);
    dwReadKBps = static_cast<DWORD> (static_cast<float>(p->bytesRead/1024) / SecsToRead);

    SecsToWrite = static_cast<float>(p->write_duration.QuadPart)/static_cast<float>(freq.QuadPart);
    dwWrittenKBps = static_cast<DWORD> (static_cast<float>(p->bytesWritten/1024) / SecsToWrite);

    fRet1 = PerfWriteLog(usrDefinedHnd(g_metricsNames[READ]), 
                                            reinterpret_cast<PVOID>(dwReadKBps));
    fRet2 = PerfWriteLog(usrDefinedHnd(g_metricsNames[WRITE]), 
                                            reinterpret_cast<PVOID>(dwWrittenKBps));
    fRet3 = PerfWriteLog(usrDefinedHnd(g_metricsNames[MIN]), 
                                            reinterpret_cast<PVOID>(0));
    fRet4 = PerfWriteLog(usrDefinedHnd(g_metricsNames[MAX]), 
                                            reinterpret_cast<PVOID>(299792458));

    return fRet1 && fRet2 && fRet3 && fRet4;
}

bool DiskTest_Perf::PerfFinalize()
{
    bool fRet = PerfWriteLog();
    if (fRet)
    {
        return BTSPerfLog::PerfFinalize();
    }
    return false;
}

// skip first few sectors that marked as reserved/readonly by FLASH driver
DWORD GetFirstWritableSector()
{
    DWORD bufferSize = 0;
    PBYTE pWriteBuffer = NULL;
    DWORD firstWritableSector = (DWORD)-1;
    LONG start = 0, end = g_diskInfo.di_total_sectors - 1;
    LONG cSector = 0; 
    
    if (g_diskInfo.di_bytes_per_sect > 0)
    {
        bufferSize = g_diskInfo.di_bytes_per_sect;
    }
    else
    {
        goto Fail;
    }
    
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pWriteBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers (bufferSize = %d)"), bufferSize);
        goto Fail;
    }

    while (start <= end) 
    {
        cSector = (start + end) / 2;
        
        if(ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, 1, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("Success: ReadWriteDisk(WRITE) at sector %u"), cSector);
            firstWritableSector = cSector;
            end = cSector - 1;
        }
        else
        {
            g_pKato->Log(LOG_FAIL, _T("Fail: ReadWriteDisk(WRITE) at sector %u"), cSector);        
            start = cSector + 1;
        }
    }
    
Fail:    
    if(pWriteBuffer) 
        VirtualFree(pWriteBuffer, 0, MEM_RELEASE);

    return firstWritableSector;
}


void ShowDiskInfo()
{
    BYTE  storeInfo[256];
    
    //first some info about the disk
    PSTORAGE_IDENTIFICATION pStoreInfo = (PSTORAGE_IDENTIFICATION) storeInfo;
    DWORD dwBytesRet = 0;
    if (!DeviceIoControl(g_hDisk, IOCTL_DISK_GET_STORAGEID, NULL, 0, pStoreInfo, 100, &dwBytesRet, NULL)) 
    { 
        DWORD err = GetLastError();
        g_pKato->Log(LOG_DETAIL, _T("IOCTL_DISK_GET_STORAGEID failed with error [%u]\n."), err);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("IOCTL_DISK_GET_STORAGEID succeeded\n."));

        if (dwBytesRet)
        {
            int i = 0;
            const unsigned char *SerialNo=(((BYTE *)pStoreInfo)+pStoreInfo->dwSerialNumOffset);
            while (i < (int)(dwBytesRet-pStoreInfo->dwSerialNumOffset) && SerialNo[i] !=0)
            {
                g_Serial[i] = (TCHAR)SerialNo[i];
                i++;
            }
            g_Serial[i] = 0;
            if (i == 1)
            {
                HRESULT hr = StringCchCopy(g_Serial, countof(g_Serial), _T("Unknown"));
                if (FAILED(hr))
                {
                    g_pKato->Log(LOG_DETAIL, _T("StringCchCopy failed in ShowDiskInfo()"));
                }
            }
            i = 0;
            const unsigned char *MfgID=(((BYTE *)pStoreInfo)+pStoreInfo->dwManufactureIDOffset);
            int diff = (int) (pStoreInfo->dwSerialNumOffset - pStoreInfo->dwManufactureIDOffset);
            if( diff > 0 )
            {
                while (i < diff && MfgID[i] != 0 && (i < 100))
                {
                    g_MFG[i] = (TCHAR)MfgID[i];
                    i++;
                }
            }
            g_MFG[i] = 0;
        }
        // Logging
        ULONGLONG ullKBs = g_diskInfo.di_total_sectors / 1000 * g_diskInfo.di_bytes_per_sect;
        g_DiskSize = GetDiskSize(ullKBs);
        g_pKato->Log(LOG_DETAIL, _T("Serial #           : %s.\n"), g_Serial);
        g_pKato->Log(LOG_DETAIL, _T("Manufacturer: #    : %s.\n"), GetManufacturer(_ttoi(g_MFG)));
        g_pKato->Log(LOG_DETAIL, _T("Disk Size:         : %d MB.\n"), g_DiskSize);
    }
}


void SetStartSector()
{
    static BOOL bCalled = FALSE;

    if (!bCalled) {
        g_startSector = GetFirstWritableSector();
        if (g_startSector == -1) {
            g_startSector = 0;
        }
        bCalled = TRUE;
    }
}


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
    pdw = (PDWORD)(buffer + ((LONG)size - DATA_STRIPE_SIZE));
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

    pdw = (PDWORD)(buffer + ((LONG)size - DATA_STRIPE_SIZE));
    if(*pdw != DATA_STRIPE)
    {
        return FALSE;
    }

    return TRUE;
}


TESTPROCAPI 
TestReadWriteSeq(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT ret = TPR_SKIP;
    int max_iter = g_dwPerfIterations;

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
        Zorchage(g_pKato); 
        goto Fail;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("FAIL: Global mass storage device handle is invalid"));
        goto Fail;
    }

    ShowDiskInfo();

    SetStartSector();

    if (g_logMethod == LOG_BTS)
    {
        g_btsPerfLog = new DiskTest_Perf(
                            SESSION_NAMESPACE,
                            SCENARIO_NAMESPACE,
                            lpFTE->lpDescription,
                            &g_PERFGUID[DISKTEST_SECTOR_RW_PERF],
                            g_pShellInfo->hInstance);
        ASSERT(g_btsPerfLog);
        if (g_btsPerfLog == NULL)
        {
            goto Fail;
        }

        if ( !g_btsPerfLog->PerfInitialize() )
        {
            goto Fail;
        }
    }

    for(int iter = 0; iter < max_iter; iter++)
    {
        for (DWORD n = 0; n < g_sectorListCounts; n++) 
        {
            ret = TestReadWriteSeq_Common(g_sectorList[n], FALSE);
            if (ret != TPR_PASS) {
                goto Fail;
            }
        }
    }

    if (g_logMethod == LOG_BTS)
    {
        if ( !g_btsPerfLog->PerfFinalize() )
        {
            goto Fail;
        }
    }

    ret = TPR_PASS;
Fail:

    if (g_logMethod == LOG_BTS)
    {
        delete g_btsPerfLog;
        g_btsPerfLog = NULL;
    }
    return ret;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadWriteSeq_Common(DWORD sectors, BOOL bAllSectors)
// --------------------------------------------------------------------
{  
    DWORD retVal = TPR_FAIL;
    DWORD dwStartSector;
    DWORD cSectorsPerReq = sectors;
    DWORD cSector = 0;
    DWORD cSectorsChecked =0;
    TCHAR perfMark1[1024];
    TCHAR perfMark2[1024];
    UINT  rwBytes = 0;

    if(cSectorsPerReq > g_dwMaxSectorsPerOp) 
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: %u sectors is larger than maximum allowed (%u)"),
            cSectorsPerReq, g_dwMaxSectorsPerOp);
        return TPR_SKIP;
    }
    
    // format disk (may be required to free sectors on flash)
    FormatMedia(g_hDisk);

    // Get the page size on the current system
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD dwPageSize = sysInfo.dwPageSize;
    ASSERT(dwPageSize > DATA_STRIPE_SIZE);

    PBYTE pStartOfReadBuffer = NULL;
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;

    DWORD bufferSize = 0;
    if ( (cSectorsPerReq > 0) && (g_diskInfo.di_bytes_per_sect > 0))
    {
        bufferSize = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;
    }
    ASSERT(bufferSize);

    DWORD readBufferSize = bufferSize + 2*DATA_STRIPE_SIZE + dwPageSize;
    if (readBufferSize < bufferSize) {
        g_pKato->Log(LOG_FAIL, _T("Integer overflow in TestReadWriteSeq_Common()"));
        goto Fail;
    }
    
    // All the reads and writes must be from a buffer that is byte-aligned to
    // the boundry required by DMA settings (typically 4 or 16 bytes) 
    // VirtualAlloc will give page-aligned addresses, but we must make sure
    // that the read offset of DATA_STRIPE_SIZE is properly aligned
    pStartOfReadBuffer = (PBYTE)VirtualAlloc(NULL, readBufferSize, MEM_COMMIT, PAGE_READWRITE);
    // Align the working read buffer with a page boundry
    pReadBuffer = pStartOfReadBuffer + dwPageSize - DATA_STRIPE_SIZE;
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pWriteBuffer || NULL == pReadBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers (bufferSize = %d)"), bufferSize);
        goto Fail;
    }

    AddSentinel(pReadBuffer, bufferSize + 2*DATA_STRIPE_SIZE);
    // put junk in the buffer
    MakeJunkBuffer(pWriteBuffer, bufferSize);

    // register perf markers
    rwBytes = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;
    HRESULT hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=Write"), 
        rwBytes, g_Format);
    if (FAILED(hr)) {
        goto Fail;
    }    

    hr = StringCchPrintf(perfMark2, countof(perfMark2), _T("Bytes=%u,%s,Test=Read"), 
        rwBytes, g_Format);
    if (FAILED(hr)) {
        goto Fail;
    }        

    // fail in case of DWORD underflow
    if ( g_diskInfo.di_total_sectors < cSectorsPerReq )
    {
        goto Fail;
    }

    // perform writes accross the disk
    for(dwStartSector = 1; dwStartSector < g_diskInfo.di_total_sectors - cSectorsPerReq && dwStartSector > 0; 
        bAllSectors ? dwStartSector+= cSectorsPerReq : dwStartSector <<= 1)
    {    
        cSector = dwStartSector - 1;
        if (cSector < g_startSector) 
        {
            DWORD newSector = cSector + g_startSector;
            if (newSector < g_diskInfo.di_total_sectors - cSectorsPerReq && newSector > cSector) 
            {
                cSector = newSector;
            }
        }

        //if this is the last run, then force to R/W the last sectors on the disk
        if( (cSector + g_diskInfo.di_total_sectors / (READ_LOCATIONS)) >= (g_diskInfo.di_total_sectors - cSectorsPerReq))
        {
            cSector = g_diskInfo.di_total_sectors - cSectorsPerReq;
        }
       
        //Write first
        g_pKato->Log(LOG_DETAIL, _T("writing + reading + comparing %u sectors of size %u bytes @ sector %u"), cSectorsPerReq, g_diskInfo.di_bytes_per_sect,cSector);
          
        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_WRITE, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }   
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(WRITE_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_WRITE);
        }

        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, cSectorsPerReq, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) at sector %u"), cSector);
            goto Fail;
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(WRITE_DURATION, rwBytes);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_WRITE);
        }

        if(!bAllSectors)
        {
            if (g_logMethod != LOG_BTS)
            {
                if(!g_pPerfLog->PerfRegister(PERF_READ, perfMark2))
                {
                    g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                    goto Fail;
                }
            }
            
            if (g_logMethod == LOG_BTS)
            {
                g_btsPerfLog->PerfBegin(READ_DURATION);
            }
            else
            {
                g_pPerfLog->PerfBegin(PERF_READ);
            }

            if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_READ, &g_diskInfo, cSector, cSectorsPerReq, pReadBuffer + DATA_STRIPE_SIZE))
            {
                g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(READ) at sector %u"), cSector);
                goto Fail;
            }

            if (g_logMethod == LOG_BTS)
            {
                g_btsPerfLog->PerfEnd(READ_DURATION, rwBytes);
            }
            else
            {
                g_pPerfLog->PerfEnd(PERF_READ);
            }

            //now verify
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
        }
        cSectorsChecked++;
    }  

    g_pKato->Log(LOG_DETAIL, _T("Wrote, Read and Verified %u sectors"), cSectorsChecked * cSectorsPerReq);

    retVal = cSectorsChecked > 0 ? TPR_PASS : TPR_SKIP;
Fail:

    if(pStartOfReadBuffer)  
        VirtualFree(pStartOfReadBuffer, 0, MEM_RELEASE);

    if(pWriteBuffer) 
        VirtualFree(pWriteBuffer, 0, MEM_RELEASE);

    return retVal;
} 


TESTPROCAPI 
TestReadWriteMulti(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT ret = TPR_SKIP;
    int max_iter =g_dwPerfIterations;

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
        Zorchage(g_pKato); 
        goto Fail;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("FAIL: Global mass storage device handle is invalid"));
        goto Fail;
    }    

    ShowDiskInfo();

    SetStartSector();

    if (g_logMethod == LOG_BTS)
    {
        g_btsPerfLog = new DiskTest_Perf(
                            SESSION_NAMESPACE,
                            SCENARIO_NAMESPACE,
                            lpFTE->lpDescription,
                            &g_PERFGUID[DISKTEST_SG_RW_PERF],
                            g_pShellInfo->hInstance);
        ASSERT(g_btsPerfLog);
        if (g_btsPerfLog == NULL)
        {
            goto Fail;
        }

        if ( !g_btsPerfLog->PerfInitialize() )
        {
            goto Fail;
        }
    }

    for(int iter = 0; iter < max_iter; iter++)
    {
        for (DWORD n = 0; n < g_sectorListCounts; n++) 
        {
            ret = TestReadWriteMulti_Common(g_sectorList[n]);
            if (ret != TPR_PASS)
            {
                goto Fail;
            }
        }
    }

    if (g_logMethod == LOG_BTS)
    {
        if ( !g_btsPerfLog->PerfFinalize() )
        {
            goto Fail;
        }
    }

    ret = TPR_PASS;
Fail:    
    if (g_logMethod == LOG_BTS)
    {
        delete g_btsPerfLog;
        g_btsPerfLog = NULL;
    }
    return ret;
}


// --------------------------------------------------------------------
TESTPROCAPI 
TestReadWriteMulti_Common(DWORD sectors)
// --------------------------------------------------------------------
{ 
    DWORD retVal = TPR_FAIL;
    DWORD dwStartSector;    
    DWORD cSector = 0;
    //perf markers
    TCHAR perfMark1[1024];
    UINT temp = 0;
    UINT rwBytes = 0;
    
    // sector offset is 1/3 of a sector, DWORD aligned
    DWORD offset =  (g_diskInfo.di_bytes_per_sect / 3) + ( sizeof(DWORD) - ((g_diskInfo.di_bytes_per_sect / 3) % sizeof(DWORD)) );
    
    // Get the page size on the current system
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD dwPageSize = sysInfo.dwPageSize;
    ASSERT(dwPageSize > DATA_STRIPE_SIZE);

    PBYTE pStartOfReadBuffer = NULL;
    PBYTE pReadBuffer = NULL;
    PBYTE pWriteBuffer = NULL;

    DWORD cSectorsPerReq = sectors;

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

    DWORD bufferSize = 0;
    if ( (cSectorsPerReq > 0) && (g_diskInfo.di_bytes_per_sect > 0))
    {
        bufferSize = g_diskInfo.di_bytes_per_sect * cSectorsPerReq;
    }
    ASSERT(bufferSize);

    DWORD readBufferSize = bufferSize + 2*DATA_STRIPE_SIZE + dwPageSize;
    if (readBufferSize < bufferSize) {
        g_pKato->Log(LOG_FAIL, _T("Integer overflow in TestReadWriteMulti_Common()"));
        goto Fail;
    }
    
    //ReadBuffer need to be expanded for those stripe bytes.
    pStartOfReadBuffer = (PBYTE)VirtualAlloc(NULL, readBufferSize, MEM_COMMIT, PAGE_READWRITE);
    pReadBuffer = pStartOfReadBuffer + dwPageSize - DATA_STRIPE_SIZE;
    pWriteBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pWriteBuffer || NULL == pReadBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers (bufferSize = %d)"), bufferSize);
        goto Fail;
    }

    g_pKato->Log(LOG_DETAIL, _T("Writing, reading with multiple SG buffers, comparing sectors"));    

    // fail in case of DWORD underflow
    if ( g_diskInfo.di_total_sectors < cSectorsPerReq )
    {
        goto Fail;
    }

    // perform writes accross the disk
    for(dwStartSector = 1; 
        dwStartSector < g_diskInfo.di_total_sectors - cSectorsPerReq 
        && dwStartSector > 0; 
        dwStartSector <<= 1)
    {
        cSector = dwStartSector - 1;
        if (cSector < g_startSector) {
            DWORD newSector = cSector + g_startSector;
            // double check the result to make sure no integer overflow occurs
            if (newSector < g_diskInfo.di_total_sectors - cSectorsPerReq - 1 && newSector > cSector)
            {
                cSector = newSector;
            }
            else
            {
                continue;
            }
        }
        
        MakeJunkBuffer(pWriteBuffer, bufferSize);

        g_pKato->Log(LOG_DETAIL, _T("writing + reading w/multiple SG buffers + comparing %u sectors @ sector %u"), cSectorsPerReq, cSector);
        
        rwBytes = cSectorsPerReq * g_diskInfo.di_bytes_per_sect;
        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(WRITE_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_WRITE);
        }

        // write the junk buffer
        if(!ReadWriteDisk(g_hDisk, IOCTL_DISK_WRITE, &g_diskInfo, cSector, cSectorsPerReq, pWriteBuffer))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadWriteDisk(WRITE) failed"));
            goto Fail;
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(WRITE_DURATION, rwBytes);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_WRITE);
        }

        // Start 6 types of SG reads

        //
        // 1) start 1/3 into first sector, end 2/3 into first sector
        //   
        temp = g_diskInfo.di_bytes_per_sect - 2*offset;
        HRESULT hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read1"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }
        
        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_SG_READ_1, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }
        }

        AddSentinel(pReadBuffer, g_diskInfo.di_bytes_per_sect - 2*offset + 2*DATA_STRIPE_SIZE);

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_1);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset,
                   pReadBuffer + DATA_STRIPE_SIZE,
                   g_diskInfo.di_bytes_per_sect - 2*offset))
            {
                g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() failed at sector %u"), cSector);
                goto Fail;
            }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_1);
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
        temp = bufferSize - offset;
        hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read2"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }    

        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_SG_READ_2, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }   
        }

        AddSentinel(pReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE);

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_2);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_2);
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
        temp = bufferSize - offset;
        hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read3"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }    

        if (g_logMethod != LOG_BTS)
        {
             if(!g_pPerfLog->PerfRegister(PERF_SG_READ_3, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }    
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_3);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, 0, 
                       pReadBuffer + DATA_STRIPE_SIZE, 
                       bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }
        
        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_3);
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
        temp = bufferSize - 2*offset;
        hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read4"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }

        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_SG_READ_4, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }    
        }

        AddSentinel(pReadBuffer, bufferSize - 2*offset + 2*DATA_STRIPE_SIZE);

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_4);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, offset, 
                   pReadBuffer + DATA_STRIPE_SIZE, 
                   bufferSize - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_4);
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
            

        //
        // 5) start at the second byte into first sector, end at the first byte (included) into final sector
        //
        temp = bufferSize - g_diskInfo.di_bytes_per_sect;
        hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read5"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }

        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_SG_READ_5, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }    
        }

        AddSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE);

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_5);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, 1,
                   pReadBuffer + DATA_STRIPE_SIZE,
                   bufferSize - g_diskInfo.di_bytes_per_sect))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg() failed at sector %u"), cSector);
            goto Fail;
        }


        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_5);
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
        // 6) start at the last byte of the first sector, end at the last second (included) byte of final sector
        //
        temp = bufferSize - g_diskInfo.di_bytes_per_sect;
        hr = StringCchPrintf(perfMark1, countof(perfMark1), _T("Bytes=%u,%s,Test=SG Read6"), 
            temp, g_Format);
        if (FAILED(hr)) {
            goto Fail;
        }

        if (g_logMethod != LOG_BTS)
        {
            if(!g_pPerfLog->PerfRegister(PERF_SG_READ_6, perfMark1))
            {
                g_pKato->Log(LOG_FAIL,_T("Perf_RegisterMark() failed"));
                goto Fail;
            }  
        }

        AddSentinel(pReadBuffer, bufferSize - g_diskInfo.di_bytes_per_sect + 2*DATA_STRIPE_SIZE);

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfBegin(READ_DURATION);
        }
        else
        {
            g_pPerfLog->PerfBegin(PERF_SG_READ_6);
        }

        if(!ReadDiskSg(g_hDisk, &g_diskInfo, cSector, g_diskInfo.di_bytes_per_sect - 1, 
                           pReadBuffer + DATA_STRIPE_SIZE, 
                           bufferSize - g_diskInfo.di_bytes_per_sect))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSg failed"));
            goto Fail;
        }

        if (g_logMethod == LOG_BTS)
        {
            g_btsPerfLog->PerfEnd(READ_DURATION, temp);
        }
        else
        {
            g_pPerfLog->PerfEnd(PERF_SG_READ_6);
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


    retVal = TPR_PASS;

Fail:
    if(pStartOfReadBuffer) 
        VirtualFree(pStartOfReadBuffer, 0, MEM_RELEASE);
    
    if(pWriteBuffer) 
        VirtualFree(pWriteBuffer, 0, MEM_RELEASE);  

    return retVal;
}

