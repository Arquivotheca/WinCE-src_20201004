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
TestUnitReady(
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    CDROM_TESTUNITREADY turData = {0};
    DWORD cbReturned = 0;    

    // IOCTL_CDROM_TEST_UNIT_READY control code should succeed if there is media in the drive 
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_TEST_UNIT_READY, NULL, 0, &turData, sizeof(turData), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_TEST_UNIT_READY control code failed, error %u"), GetLastError());
        g_pKato->Log(LOG_DETAIL, _T("Make sure media is present in the cdrom drive"));
        return TPR_FAIL;
    }

    // check returned data
    if(turData.bUnitReady)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROM Device is Ready"));
    }

    // the call succeeded, but the ready flag was not set
    else
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: CDROM_TESTUNITREADY.bUnitRead == FALSE"));
        g_pKato->Log(LOG_DETAIL, _T("CDROM Device is Not Ready"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestIssueInquiry(
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    INQUIRY_DATA inqData = {0};
    DWORD cbReturned = 0;    

    // IOCTL_CDROM_ISSUE_INQIRY control code should succeed if there is media in the drive 
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_ISSUE_INQUIRY, NULL, 0, &inqData, sizeof(inqData), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_ISSUE_INQUIRY control code failed, error %u"), GetLastError());
        return TPR_FAIL;
    }

    g_pKato->Log(LOG_DETAIL, _T("inqDevType         0x%02x"), inqData.inqDevType);
    g_pKato->Log(LOG_DETAIL, _T("inqRMB             0x%02x"), inqData.inqRMB);
    g_pKato->Log(LOG_DETAIL, _T("inqVersion         0x%02x"), inqData.inqVersion);    
    g_pKato->Log(LOG_DETAIL, _T("inqAtapiVersion    0x%02x"), inqData.inqAtapiVersion);
    g_pKato->Log(LOG_DETAIL, _T("inqLength          0x%02x"), inqData.inqLength);
    g_pKato->Log(LOG_DETAIL, _T("inqVendor          0x%02x 0x%02x 0x02x 0x02x 0x02x 0x02x 0x02x 0x02x"), 
        inqData.inqVendor[0], inqData.inqVendor[1], inqData.inqVendor[2], inqData.inqVendor[3],
        inqData.inqVendor[4], inqData.inqVendor[5], inqData.inqVendor[6], inqData.inqVendor[7]);
   g_pKato->Log(LOG_DETAIL, _T("inqProdID          0x%02x 0x%02x 0x02x 0x02x 0x02x 0x02x 0x02x 0x02x")
        _T(" 0x%02x 0x%02x 0x02x 0x02x 0x02x 0x02x 0x02x 0x02x"), 
        inqData.inqProdID[0], inqData.inqProdID[1], inqData.inqProdID[2], inqData.inqProdID[3],
        inqData.inqProdID[4], inqData.inqProdID[5], inqData.inqProdID[6], inqData.inqProdID[7],
        inqData.inqProdID[8], inqData.inqProdID[9], inqData.inqProdID[10], inqData.inqProdID[11],
        inqData.inqProdID[12], inqData.inqProdID[13], inqData.inqProdID[14], inqData.inqProdID[15]);
    
    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestEjectMedia(
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

    CDROM_TESTUNITREADY turData = {0};
    DWORD cbReturned = 0;  
    DWORD dwRetries = 0;
    
    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    g_pKato->Log(LOG_DETAIL, _T("Ejecting media"));

    // IOCTL_CDROM_EJECT_MEDIA control code should succeed if the disk is not ejected
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_EJECT_MEDIA, NULL, 0, NULL, 0, NULL, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_EJECT_MEDIA control code failed, error %u"), GetLastError());
        return TPR_FAIL;
    }

    // IOCTL_CDROM_TEST_UNIT_READY control code should either fail, or turData.bUnitRead should be FALSE
    // if the Eject operation succeded
    if(DeviceIoControl(g_hDisk, IOCTL_CDROM_TEST_UNIT_READY, NULL, 0, &turData, sizeof(turData), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_DETAIL, _T("IOCTL_CDROM_TEST_UNIT_READY succeeded, CDROM_TESTUNITREADY.bUnitRead = %s"),
            turData.bUnitReady ? _T("TRUE") : _T("FALSE"));
        if(turData.bUnitReady)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_TEST_UNIT_READY control code failed, error %u"), GetLastError());
            g_pKato->Log(LOG_DETAIL, _T("Make sure media is present in the cdrom drive"));
            return TPR_FAIL;
        }
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("IOCTL_CDROM_TEST_UNIT_READY failed appropriately with no media in drive"));
    }
    
    // IOCTL_CDROM_LOAD_MEDIA control code should succeed if the disk ejected
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_LOAD_MEDIA, NULL, 0, NULL, 0, NULL, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_LOAD_MEDIA control code failed, error %u"), GetLastError());
        g_pKato->Log(LOG_DETAIL, _T("You may need to manually close the CD drawer in order to continue testing"));
        return TPR_FAIL;
    }

    // it is possible the media is not immediately ready after loading, so retry a few times
    // before indicating failure
    dwRetries = 0;
    while(dwRetries < g_cMaxReadRetries)
    {
        // wait 1 second before trying to access the disk
        Sleep(1000);
        
        // IOCTL_CDROM_TEST_UNIT_READY control code should succeed if there is media in the drive 
        if(DeviceIoControl(g_hDisk, IOCTL_CDROM_TEST_UNIT_READY, NULL, 0, &turData, sizeof(turData), &cbReturned, NULL))
        {
            break;
        }
        g_pKato->Log(LOG_DETAIL, _T("IOCTL_CDROM_TEST_UNIT_READY failed; retrying..."));
        dwRetries++;
    }

    if(dwRetries == g_cMaxReadRetries) {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_TEST_UNIT_READY control code failed, error %u"), GetLastError());
        g_pKato->Log(LOG_DETAIL, _T("Make sure media is present in the cdrom drive"));
        return TPR_FAIL;
    }

    // check returned data
    if(turData.bUnitReady)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROM Device is Ready"));
    }

    // the call succeeded, but the ready flag was not set
    else
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: CDROM_TESTUNITREADY.bUnitRead == FALSE"));
        g_pKato->Log(LOG_DETAIL, _T("CDROM Device is Not Ready"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}



// --------------------------------------------------------------------
TESTPROCAPI 
TestCDRomInfo(
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    CDROM_DISCINFO cdInfo = {0};
    DWORD cbReturned = 0;    

    // query driver for cdrom disk information structure
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_DISC_INFO, NULL, 0, &cdInfo, sizeof(CDROM_DISCINFO), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_DISC_INFO control code failed, error %u"), GetLastError());
        return TPR_FAIL;
    }

    g_pKato->Log(LOG_DETAIL, _T("CDROM Disc Info Retrieved"));
    // dump the info
    g_pKato->Log(LOG_DETAIL, _T("FirstTrack: %04x"), cdInfo.FirstTrack);
    g_pKato->Log(LOG_DETAIL, _T("LastTrack: %04x"), cdInfo.LastTrack);
    g_pKato->Log(LOG_DETAIL, _T("FirstSession: %02x"), cdInfo.FirstSession);
    g_pKato->Log(LOG_DETAIL, _T("LastSession: %02x"), cdInfo.LastSession);
    g_pKato->Log(LOG_DETAIL, _T("ReqSession: %02x"), cdInfo.ReqSession);
    g_pKato->Log(LOG_DETAIL, _T("RetSession: %02x"), cdInfo.RetSession);
    g_pKato->Log(LOG_DETAIL, _T("LogicStartAddr: %04x"), cdInfo.LogicStartAddr);

    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestRead(
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    // handle to the image file
    HANDLE hImg = INVALID_HANDLE_VALUE;
    
    PBYTE pSectBuffer = NULL;
    PBYTE pCompBuffer = NULL;

    DWORD cSectorsPerRead = (lpFTE->dwUserData < g_cMaxSectors) ? lpFTE->dwUserData : g_cMaxSectors;
    
    DWORD cbRead = 0;
    DWORD bufferSize = cSectorsPerRead * CD_SECTOR_SIZE;
    DWORD cSector = 0;    
    DWORD dwRetries = 0;

    ASSERT(bufferSize);

    // calculate buffer size and check for overflow
    DWORD bufferTempSize = bufferSize + 2*DATA_STRIPE_SIZE;
    if (bufferTempSize < bufferSize)
    {
        goto Fail;
    }
    // allocate buffers
    pSectBuffer = (PBYTE)VirtualAlloc(NULL, bufferTempSize, MEM_COMMIT, PAGE_READWRITE);
    pCompBuffer = (PBYTE)VirtualAlloc(NULL, bufferTempSize, MEM_COMMIT, PAGE_READWRITE);

    if(NULL == pSectBuffer || NULL == pCompBuffer)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
        goto Fail;
    }

    // mark the read buffer boundaries to check for overruns
    AddSentinel(pSectBuffer, bufferSize + 2*DATA_STRIPE_SIZE);
    AddSentinel(pCompBuffer, bufferSize + 2*DATA_STRIPE_SIZE);

    // open handle to image file
    hImg = CreateFile(g_szImgFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // unable to open the file, it does not exist
    if(INVALID_HANDLE_VALUE == hImg)
    {
        g_pKato->Log(LOG_DETAIL, _T("Unable to open data image file \"%s.\" CD data will not be verified"), g_szImgFile);
    }

    g_pKato->Log(LOG_DETAIL, _T("Reading CDROM media sectors 0 through %u, %u sectors at a time..."), g_endSector, cSectorsPerRead);

    for(cSector = 0; cSector < g_endSector; cSector += cSectorsPerRead)
    {
        // it is possible a read will fail when there is more data, so retry up to specified number of tries
        dwRetries = 0;
        while(dwRetries < g_cMaxReadRetries)
        {
            if(ReadDiskSectors(g_hDisk, cSector, pSectBuffer + DATA_STRIPE_SIZE, bufferSize, &cbRead))
            {
                // successfully read sectors
                break;
            }
            dwRetries++;
            g_pKato->Log(LOG_DETAIL, _T("Read failed, retrying %u..."), dwRetries);
        }
        if(dwRetries >= g_cMaxReadRetries)
        {
            g_pKato->Log(LOG_DETAIL, _T("Failed after %u retries. Finished reading from disk"), dwRetries);
            break;
        }
            
        if(cbRead != bufferSize)
        {
            g_pKato->Log(LOG_FAIL, _T("Did not read requested number of bytes: %u != %u"), bufferSize, cbRead);
            goto Fail;
        }

        // read next sector from image file
        if(INVALID_HANDLE_VALUE != hImg)
        {
            if(ReadFile(hImg, pCompBuffer + DATA_STRIPE_SIZE, bufferSize, &cbRead, NULL))
            {
                // compare image file data with cd media data
                if(0 != memcmp(pSectBuffer + DATA_STRIPE_SIZE, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
                {
                    g_pKato->Log(LOG_FAIL, _T("FAIL: the data in the image file \"%s\" does not match the data read from cd at sector %u"), g_szImgFile, cSector);
                    goto Fail;
                }
            }
            
            else
            {
                // if nothing was read, the image file must be empty so the test cannot pass
                if(0 == cSector)
                {           
                    g_pKato->Log(LOG_FAIL, _T("FAIL: image file is empty"));
                    goto Fail;
                }
                // the image might be smaller than the actual media -- this warning is normal for the last few sectors
                g_pKato->Log(LOG_DETAIL, _T("WARNING: image does not contain enough data; skipping check for sectors after %u"), cSector);
                VERIFY(CloseHandle(hImg));
                hImg = INVALID_HANDLE_VALUE;
            }
        }

        // verify that the read operation did not overwrite the buffer
        if(!CheckSentinel(pSectBuffer, bufferSize + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Data read wrote out of bounds"));
            goto Fail;
        }                 
    }   

    g_pKato->Log(LOG_DETAIL, _T("Read %u total sectors from media"), cSector);

    VirtualFree(pSectBuffer, 0, MEM_RELEASE);
    VirtualFree(pCompBuffer, 0, MEM_RELEASE);

    if(INVALID_HANDLE_VALUE != hImg) VERIFY(CloseHandle(hImg));
    
    return cSector > 0 ? TPR_PASS : TPR_FAIL;

Fail:
    if(pSectBuffer) VirtualFree(pSectBuffer, 0, MEM_RELEASE);
    if(pCompBuffer) VirtualFree(pCompBuffer, 0, MEM_RELEASE);
    if(INVALID_HANDLE_VALUE != hImg) VERIFY(CloseHandle(hImg));
    
    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestReadSize(
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    HANDLE hImg = INVALID_HANDLE_VALUE;
    
    PBYTE pSectBuffer;
    PBYTE pCompBuffer;
    
    DWORD cbRead = 0;
    DWORD cSectorsPerReq = 1;
    DWORD bufferSize = 0;

    g_pKato->Log(LOG_DETAIL, _T("Reading CDROM media starting 1 sector per read, ending %u sectors per read"), g_cMaxSectors);

    for(cSectorsPerReq = 1; cSectorsPerReq <= g_cMaxSectors; cSectorsPerReq <<= 1)
    {
        g_pKato->Log(LOG_DETAIL, _T("Reading %u sectors..."), cSectorsPerReq);
        bufferSize = cSectorsPerReq * CD_SECTOR_SIZE;
        
        // allocate buffers
        pSectBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize + 2*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);
        pCompBuffer = (PBYTE)VirtualAlloc(NULL, bufferSize + 2*DATA_STRIPE_SIZE, MEM_COMMIT, PAGE_READWRITE);

        if(NULL == pSectBuffer || NULL == pCompBuffer)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to allocate memory for buffers"));
            goto Fail;
        }    

        // mark the read buffer boundaries to check for overruns
        AddSentinel(pSectBuffer, bufferSize + 2*DATA_STRIPE_SIZE);
        AddSentinel(pCompBuffer, bufferSize + 2*DATA_STRIPE_SIZE);        
            
        if(!ReadDiskSectors(g_hDisk, 0, pSectBuffer + DATA_STRIPE_SIZE, bufferSize, &cbRead))
        {
            g_pKato->Log(LOG_DETAIL, _T("Finished reading from disk"));
            break;
        }

        if(cbRead != bufferSize)
        {
            g_pKato->Log(LOG_FAIL, _T("Did not read requested number of bytes: %u != %u"), bufferSize, cbRead);
            goto Fail;
        }

        
        // open handle to image file
        hImg = CreateFile(g_szImgFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        // unable to open the file, it does not exist
        if(INVALID_HANDLE_VALUE == hImg)        
        {
            g_pKato->Log(LOG_DETAIL, _T("Unable to open data image file \"%s.\" CD data will not be verified"), g_szImgFile);
        }
        else
        {

            if(ReadFile(hImg, pCompBuffer + DATA_STRIPE_SIZE, bufferSize, &cbRead, NULL))
            {
                // compare image file data with cd media data
                if(0 != memcmp(pSectBuffer + DATA_STRIPE_SIZE, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
                {
                    g_pKato->Log(LOG_FAIL, _T("FAIL: Sector data comparison failed reading %u sectors"), cSectorsPerReq);
                    g_pKato->Log(LOG_FAIL, _T("FAIL: the data in the image file \"%s\" does not match the data read from cd"), g_szImgFile);
                    goto Fail;
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, _T("Image file does not contain %u sectors to compare"), cSectorsPerReq);
                goto Fail;
            }

            // close the file
            VERIFY(CloseHandle(hImg));
            hImg = INVALID_HANDLE_VALUE;
        }
        
        // verify that the read operation did not overwrite the buffer
        if(!CheckSentinel(pSectBuffer, bufferSize + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Data read wrote out of bounds"));
            goto Fail;
        }       
        VirtualFree(pSectBuffer, 0, MEM_RELEASE);
        VirtualFree(pCompBuffer, 0, MEM_RELEASE);
    }   

    g_pKato->Log(LOG_DETAIL, _T("Read max of %u sectors at once from media"), cSectorsPerReq);

    // was the requested maximum reached?
    return cSectorsPerReq > g_cMaxSectors ? TPR_PASS : TPR_FAIL;

Fail:
    if(pSectBuffer) VirtualFree(pSectBuffer, 0, MEM_RELEASE);
    if(pCompBuffer) VirtualFree(pCompBuffer, 0, MEM_RELEASE);
    if(INVALID_HANDLE_VALUE != hImg) VERIFY(CloseHandle(hImg));
    
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    BYTE pbStack[CD_SECTOR_SIZE];
    static BYTE pbDataSection[CD_SECTOR_SIZE];
    BYTE *pbVirtual = NULL;
    BYTE *pbHeap = NULL;
    DWORD cbRead;

    BYTE reqBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 3] = {0};
    PCDROM_READ pCdReq = (PCDROM_READ)reqBuffer;

    // first, test the ability to read data into the stack allocated buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into stack allocated buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbStack, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into STACK ALLOCATED buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into stack allocated buffer caused an exception"));
        goto Fail;
    }

    // next, test the ability to read data into the static buffer (from the global data section)
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into static data buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbDataSection, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into static data buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into stack allocated buffer caused an exception"));
        goto Fail;
    }

    // allocate a heap buffer for the next read
    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, CD_SECTOR_SIZE );
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }

    // next, test the ability to read data into a heap allocated buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into HeapAlloc() buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbHeap, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into HeapAlloc() buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into HeapAlloc() buffer caused an exception"));
        goto Fail;
    }
    
    VERIFY(HeapFree(GetProcessHeap(), 0, pbHeap));
    pbHeap = NULL;

    pbVirtual = (BYTE*)VirtualAlloc(NULL, CD_SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_READWRITE) failed; error = %u"), GetLastError());
        goto Fail;
    }

    // next, test the ability to read data into a r/w virtual buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into VirtualAlloc() read/write buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbVirtual, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into VirtualAlloc() read/write buffer failed"));
            goto Fail;
        }    
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into VirtualAlloc() read/write caused an exception"));
        goto Fail;
    }


    VERIFY(VirtualFree(pbVirtual, 0, MEM_RELEASE));

    pbVirtual = (BYTE*)VirtualAlloc(NULL, CD_SECTOR_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(NULL == pbVirtual)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READWRITE) failed; error = %u"), GetLastError());
        goto Fail;
    }

    // next, test the ability to read data into a r/w/x virtual buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into VirtualAlloc() read/write/execute buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbVirtual, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into VirtualAlloc() read/write/execute buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into VirtualAlloc() read/write/execute caused an exception"));
        goto Fail;
    }

    pbHeap = (BYTE*)HeapAlloc(GetProcessHeap(), 0, CD_SECTOR_SIZE+sizeof(DWORD));
    if(NULL == pbHeap)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: HeapAlloc() failed; error = %u"), GetLastError());
        goto Fail;
    }

    // next, test the ability to read data into an unaligned r/w buffers

    // read into BYTE-aligned buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into BYTE-aligned buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbHeap+1, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into BYTE-aligned buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into unaligned buffer caused an exception"));
        goto Fail;
    }

    // read into WORD-aligned buffer
    __try
    {
        g_pKato->Log(LOG_DETAIL, _T("reading %u bytes into WORD-aligned buffer at sector %u"), CD_SECTOR_SIZE, 0);
        if(!ReadDiskSectors(g_hDisk, 0, pbHeap+2, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into WORD-aligned buffer failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into unaligned buffer caused an exception"));
        goto Fail;
    }

    // now, do a multi-sg read with an assortment of buffer types

    pCdReq->bRawMode = FALSE;
    pCdReq->sgcount = 0;
    pCdReq->StartAddr.Mode = CDROM_ADDR_LBA;
    pCdReq->StartAddr.Address.lba = 0;

    // sg 0 = stack buffer
    pCdReq->sglist[pCdReq->sgcount].sb_buf = pbStack;
    pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE;
    pCdReq->sgcount++;

    // sg 1 = global buffer
    pCdReq->sglist[pCdReq->sgcount].sb_buf = pbDataSection;
    pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE;
    pCdReq->sgcount++;

    // sg 2 = valloc buffer
    pCdReq->sglist[pCdReq->sgcount].sb_buf = pbVirtual;
    pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE;
    pCdReq->sgcount++;

    // sg 3 = byte-aligned heap buffer
    pCdReq->sglist[pCdReq->sgcount].sb_buf = pbHeap + 1;
    pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE;
    pCdReq->sgcount++;

    // one sector per buffer
    pCdReq->TransferLength = pCdReq->sgcount;

    __try
    {
        DWORD dwReceived;
        g_pKato->Log(LOG_DETAIL, _T("reading 1 sector into each of 4 sg buffers (stack, global, valloc, and byte-aligned heap) at sector %u"), 0);
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, pCdReq, sizeof(reqBuffer), NULL, 0, &dwReceived, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: read into multi-sg buffers failed"));
            goto Fail;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: read into multi-sg buffers caused an exception"));
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
TestReadTOC(
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    CDROM_TOC cdtoc = {0};
    DWORD cbReturned = 0;    
    CDROM_ADDR cdaddr, cdaddrnext,cdaddrmsf;
    MSF_ADDR msftemp;

    // IOCTL_CDROM_READ_TOC control code should succeed if there is media in the drive 
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdtoc, sizeof(cdtoc), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_READ_TOC control code failed, error %u"), GetLastError());
        g_pKato->Log(LOG_DETAIL, _T("Make sure media is present in the cdrom drive"));
        return TPR_FAIL;
    }

    // check returned data -- warn on failure, but don't fail the test
    g_pKato->Log(LOG_DETAIL, _T("Got the CDROM TOC Information Length=%ld\r\n"), ((WORD)cdtoc.Length[0] << 8) | (WORD)cdtoc.Length[1]);
    g_pKato->Log(LOG_DETAIL, _T("First track is %ld Last track is %ld\r\n"), cdtoc.FirstTrack, cdtoc.LastTrack);

    if(0 == cdtoc.FirstTrack && 0 == cdtoc.LastTrack)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: there are no tracks listed in the CD TOC"));
    }

    else if(1 != cdtoc.FirstTrack) 
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: the first track on listed in the CD TOC is not track 1"));
    }

    if(cdtoc.LastTrack < cdtoc.FirstTrack)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: the last track on the CD is before the first track"));
    }

    for (DWORD i=0; i < cdtoc.LastTrack; i++)
    {
        cdaddr.Mode = CDROM_ADDR_MSF;
        cdaddr.Address.msf.msf_Frame = cdtoc.TrackData[i].Address[3];
        cdaddr.Address.msf.msf_Second = cdtoc.TrackData[i].Address[2];
        cdaddr.Address.msf.msf_Minute = cdtoc.TrackData[i].Address[1];
        cdaddr.Mode = CDROM_ADDR_MSF;
        cdaddrnext.Address.msf.msf_Frame = cdtoc.TrackData[i+1].Address[3];
        cdaddrnext.Address.msf.msf_Second = cdtoc.TrackData[i+1].Address[2];
        cdaddrnext.Address.msf.msf_Minute = cdtoc.TrackData[i+1].Address[1];
        CDROM_MSF_TO_LBA(&cdaddr);
        CDROM_MSF_TO_LBA(&cdaddrnext);
        cdaddrmsf.Address.lba = cdaddrnext.Address.lba-cdaddr.Address.lba;
        CDROM_LBA_TO_MSF(&cdaddrmsf, msftemp);
        g_pKato->Log(LOG_DETAIL, _T("Track[%2u] at \tMSF(%02u:%02u:%02u)\tMSFLength(%02u:%02u:%02u)\tLBA(%7d)\tLBA_Length(%7d)\r\n"),
                    cdtoc.TrackData[i].TrackNumber, 
                    cdtoc.TrackData[i].Address[1],
                    cdtoc.TrackData[i].Address[2],
                    cdtoc.TrackData[i].Address[3],
                    cdaddrmsf.Address.msf.msf_Minute,
                    cdaddrmsf.Address.msf.msf_Second,
                    cdaddrmsf.Address.msf.msf_Frame,
                    cdaddr.Address.lba,
                    cdaddrnext.Address.lba-cdaddr.Address.lba);
    }

    return TPR_PASS;
}


// --------------------------------------------------------------------
TESTPROCAPI 
TestMultiSgRead(
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
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    DWORD cSector = 0;
    DWORD maxSector = g_endSector;

    // sector offset is 1/3 of a sector, rounded to nearest DWORD
    DWORD offset = (CD_SECTOR_SIZE / 3) + ((CD_SECTOR_SIZE / 3) % sizeof(DWORD));
    DWORD cbRead = 0;
    DWORD bufferSize = 2*CD_SECTOR_SIZE;
    
    // read buffers
    static BYTE sgReadBuffer[2*CD_SECTOR_SIZE + 2*DATA_STRIPE_SIZE];
    static BYTE readBuffer[2*CD_SECTOR_SIZE + 2*DATA_STRIPE_SIZE];

    AddSentinel(readBuffer, bufferSize + 2*DATA_STRIPE_SIZE);

    // perform the test accross the entire disk until failure
    g_pKato->Log(LOG_DETAIL, _T("Reading and verifying all sectors using multiple SG buffers..."));
    for(cSector = 0; cSector < maxSector; cSector++)
    {
        // perform a normal read to compare SG buffer reads to
        if(!ReadDiskSectors(g_hDisk, cSector, readBuffer + DATA_STRIPE_SIZE, 2*CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_DETAIL, _T("Finished reading from disk"));
            break;
        }
        // verify initial read did not go out of bounds
        if(!CheckSentinel(readBuffer, bufferSize + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskSectors() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        // Start 4 types of SG reads

        //
        // 1) start 1/3 into first sector, end 2/3 into first sector
        //
        AddSentinel(sgReadBuffer, CD_SECTOR_SIZE - 2*offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskMulti(g_hDisk, cSector, offset,
                          sgReadBuffer + DATA_STRIPE_SIZE,
                          CD_SECTOR_SIZE - 2*offset, &cbRead))
        {
            // multi read could fail even if standard read did not fail 
            // once we're past the end of the data on the disk
            g_pKato->Log(LOG_DETAIL, _T("Finished reading from disk"));
            break;
        }
        // verify the read
        if(memcmp(sgReadBuffer + DATA_STRIPE_SIZE, readBuffer + offset + DATA_STRIPE_SIZE, CD_SECTOR_SIZE - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            goto Fail;
        }
        if(!CheckSentinel(sgReadBuffer, CD_SECTOR_SIZE - 2*offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 2) start 1/3 into first sector, end at end of final sector
        //
        AddSentinel(sgReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskMulti(g_hDisk, cSector, offset, 
                          sgReadBuffer + DATA_STRIPE_SIZE, 
                          bufferSize - offset, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() failed at sector %u"), cSector);
            goto Fail;
        }
        // verify the read
        if(memcmp(sgReadBuffer + DATA_STRIPE_SIZE, readBuffer + offset + DATA_STRIPE_SIZE, bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            g_pKato->Log(LOG_FAIL, _T("FAIL: The data read with 1 sg buffer does not match the data read with multiple sg buffers"));
            goto Fail;
        }
        if(!CheckSentinel(sgReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }

        //
        // 3) start at beginning of first sector, end 2/3 through final sector        
        //          
        // keep data stripe from prev. because buffer is same size
        if(!ReadDiskMulti(g_hDisk, cSector, 0, 
                          sgReadBuffer + DATA_STRIPE_SIZE, 
                          bufferSize - offset, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() failed at sector %u"), cSector);
            goto Fail;
        }
        // verify the read
        if(memcmp(sgReadBuffer + DATA_STRIPE_SIZE, readBuffer + DATA_STRIPE_SIZE, bufferSize - offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            g_pKato->Log(LOG_FAIL, _T("FAIL: The data read with 1 sg buffer does not match the data read with multiple sg buffers"));
            goto Fail;
        }
        if(!CheckSentinel(sgReadBuffer, bufferSize - offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }     

        //
        // 4) start 1/3 into first sector, end 2/3 through final sector
        //
        AddSentinel(sgReadBuffer, bufferSize - 2*offset + 2*DATA_STRIPE_SIZE);
        if(!ReadDiskMulti(g_hDisk, cSector, offset, 
                          sgReadBuffer + DATA_STRIPE_SIZE, 
                          bufferSize - 2*offset, &cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() failed at sector %u"), cSector);
            goto Fail;
        }
        // verify the read
        if(memcmp(sgReadBuffer + DATA_STRIPE_SIZE, readBuffer + offset + DATA_STRIPE_SIZE, bufferSize - 2*offset))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Buffer miscompare at sector %u"), cSector);
            g_pKato->Log(LOG_FAIL, _T("FAIL: The data read with 1 sg buffer does not match the data read with multiple sg buffers"));
            goto Fail;
        }
        if(!CheckSentinel(sgReadBuffer, bufferSize - 2*offset + 2*DATA_STRIPE_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: ReadDiskMulti() wrote out of bounds at sector %u"), cSector);
            goto Fail;
        }        
    }
    
    g_pKato->Log(LOG_DETAIL, _T("Read and Verified %u sectors"), cSector);

    // if no sectors were read, fail
    return cSector > 0 ? TPR_PASS: TPR_FAIL;
Fail:

    return TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
UtilMakeCDImage(
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

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        g_pKato->Log(LOG_SKIP, _T("SKIP: Global CDRom Disk handle is invalid"));
        return TPR_SKIP;
    }

    // log file on host machine
    HANDLE hImg = INVALID_HANDLE_VALUE;

    BYTE buffer[CD_SECTOR_SIZE];
    DWORD cbRead = 0;
    DWORD cSector = 0;


    // open handle to image file
    hImg = CreateFile(g_szImgFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    // unable to open the file, it does not exist
    if(INVALID_HANDLE_VALUE == hImg)        
    {
        g_pKato->Log(LOG_DETAIL, _T("Unable to create data image file"));
        return TPR_SKIP;
    }

    g_pKato->Log(LOG_DETAIL, _T("Creating image of CDROM media sectors 0 through %u..."), g_endSector);

    // read each sector and log to file
    for(cSector = 0; cSector < g_endSector; cSector++)
    {
        if(!ReadDiskSectors(g_hDisk, cSector, buffer, CD_SECTOR_SIZE, &cbRead))
        {
            g_pKato->Log(LOG_DETAIL, _T("Finished reading media"));
            break;
        }
        
        // returned incorrect number of bytes read
        if(cbRead != CD_SECTOR_SIZE)
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Sector %08x : Failed to read entire sector!"), cSector);            
            VERIFY(CloseHandle(hImg));
            return TPR_FAIL;
        }

        // write sector data to file
        if(!WriteFile(hImg, buffer, CD_SECTOR_SIZE, &cbRead, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Failed to write entire sector to image file"));
            g_pKato->Log(LOG_DETAIL, _T("Image file is invalid"));
            VERIFY(CloseHandle(hImg));
            return TPR_FAIL;
        }
    }

    g_pKato->Log(LOG_DETAIL, _T("Image completed. Copied sectors 0 through %u to file %s"), cSector, g_szImgFile);
    VERIFY(CloseHandle(hImg));
    
    return TPR_PASS;
}
