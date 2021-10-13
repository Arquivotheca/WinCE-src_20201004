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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    CDROM_DISCINFO cdInfo = {0};
    DWORD cbReturned = 0;    

    // query driver for cdrom disk information structure
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_DISC_INFO, NULL, 0, &cdInfo, sizeof(CDROM_DISCINFO), &cbReturned, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: IOCTL_CDROM_DISC_INFO control code failed, error %u"), GetLastError());
        return TPR_FAIL;
    }

    g_pKato->Log(LOG_DETAIL, _T("CDROM Disc Info Retreived"));
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    // log file on host machine
    HFILE hImg = HFILE_ERROR;
    
    PBYTE pSectBuffer;
    PBYTE pCompBuffer;

    DWORD cSectorsPerRead = (lpFTE->dwUserData < g_cMaxSectors) ? lpFTE->dwUserData : g_cMaxSectors;
    
    DWORD cbRead = 0;
    DWORD bufferSize = cSectorsPerRead * CD_SECTOR_SIZE;
    DWORD cSector = 0;    
    DWORD dwRetries = 0;

    ASSERT(bufferSize);

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

    // open handle to image file
    hImg = U_ropen(g_szImgFile, PPSH_READ_ACCESS);

    // unable to open the file, it does not exist
    if(HFILE_ERROR == hImg)
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
        if(HFILE_ERROR != hImg)
        {
            if(cbRead == (UINT)U_rread(hImg, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
            {
                // compare image file data with cd media data
                if(0 != memcmp(pSectBuffer + DATA_STRIPE_SIZE, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
                {
                    DebugBreak();
                    g_pKato->Log(LOG_FAIL, _T("FAIL: Sector data comparison failed at sector %u"), cSector);
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
                U_rclose(hImg);
                hImg = HFILE_ERROR;
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

    if(HFILE_ERROR != hImg)
    {
        U_rclose(hImg);
    }
    
    return cSector > 0 ? TPR_PASS : TPR_FAIL;

Fail:
    if(pSectBuffer) VirtualFree(pSectBuffer, 0, MEM_RELEASE);
    if(pCompBuffer) VirtualFree(pCompBuffer, 0, MEM_RELEASE);
    if(HFILE_ERROR != hImg) U_rclose(hImg);
    
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    // log file on host machine
    INT hImg = HFILE_ERROR;
    
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
        hImg = U_ropen(g_szImgFile, PPSH_READ_ACCESS);

        // unable to open the file, it does not exist
        if(HFILE_ERROR == hImg)
        {
            g_pKato->Log(LOG_DETAIL, _T("Unable to open data image file \"%s.\" CD data will not be verified"), g_szImgFile);
        }
        else
        {
            // read sectors from image file
            if(cbRead == (UINT)U_rread(hImg, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
            {
                // compare image file data with cd media data
                if(0 != memcmp(pSectBuffer + DATA_STRIPE_SIZE, pCompBuffer + DATA_STRIPE_SIZE, cbRead))
                {
                    DebugBreak();
                    g_pKato->Log(LOG_FAIL, _T("FAIL: Sector data comparison failed reading %u sectors"), cSectorsPerReq);
                    goto Fail;
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, _T("Image file does not contain %u sectors to compare"), cSectorsPerReq);
                goto Fail;
            }

            // close the file
            U_rclose(hImg);
            hImg = HFILE_ERROR;
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
    if(HFILE_ERROR != hImg) U_rclose(hImg);
    
    return TPR_FAIL;
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    DWORD cSector = 0;
    DWORD maxSector = g_endSector;

    // sector offset is 1/3 of a sector, rounded to nearest DWORD
    DWORD offset = (CD_SECTOR_SIZE / 3) + ((CD_SECTOR_SIZE / 3) % sizeof(DWORD));
    DWORD cbRead = 0;
    DWORD bufferSize = 2*CD_SECTOR_SIZE;
    
    // read buffers
    BYTE sgReadBuffer[2*CD_SECTOR_SIZE + 2*DATA_STRIPE_SIZE];
    BYTE readBuffer[2*CD_SECTOR_SIZE + 2*DATA_STRIPE_SIZE];

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
            DebugBreak();
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
            DebugBreak();
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
            DebugBreak();
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    // log file on host machine
    INT hImg = 0;

    BYTE buffer[CD_SECTOR_SIZE];
    DWORD cbRead = 0;
    DWORD cSector = 0;

    // open image file on host
    hImg = U_ropen(g_szImgFile, PPSH_WRITE_ACCESS);
    if(HFILE_ERROR == hImg)
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
            U_rclose(hImg);
            return TPR_FAIL;
        }

        // write sector data to image file on host
        if(cbRead != (UINT)U_rwrite(hImg, buffer, cbRead))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Failed to write entire sector to image file"));
            g_pKato->Log(LOG_DETAIL, _T("Image file is invalid"));
            U_rclose(hImg);
            return TPR_FAIL;
        }
    }

    g_pKato->Log(LOG_DETAIL, _T("Image completed. Copied sectors 0 through %u to file %s"), cSector, g_szImgFile);
    U_rclose(hImg);
    
    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestMSFAudio(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_PASS;
    
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    UCHAR nTrack = 1;
    DWORD cBytes = 0;
    CDROM_TOC cdToc = {0};
    CDROM_READ cdRead = {0};
    BYTE buffer[CD_RAW_SECTOR_SIZE]; // raw cd sector size
    BYTE buffer2[CD_RAW_SECTOR_SIZE]; // raw cd sector size

    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to query cdrom TOC using IOCTL_CDROM_READ_TOC; error %u"), GetLastError());
        return TPR_FAIL;
    }

    // make sure requested track exists on the disk
    if(nTrack >= (cdToc.LastTrack))
    {
        g_pKato->Log(LOG_DETAIL, _T("There is no track %u on the CD"), nTrack);
        return TPR_SKIP;
    }

    // make sure track address is MSF format
    // nTrack - 1 is array index into TrackData
    if(CDROM_ADDR_MSF != cdToc.TrackData[nTrack-1].Adr)
    {
        g_pKato->Log(LOG_DETAIL, _T("Track %u address is not in MSF format"), nTrack);
        g_pKato->Log(LOG_DETAIL, _T("Re-run this test with an Audio CD"));
        return TPR_SKIP;
    }

    cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
    cdRead.TransferLength = 1;
    cdRead.bRawMode = TRUE;
    cdRead.TrackMode = CDDA;
    cdRead.sgcount = 1;
   
    // try to read the beginning of each track
    for(nTrack = cdToc.FirstTrack; nTrack < cdToc.LastTrack; nTrack++)
    {
        // make sure track address is MSF format
        // nTrack - 1 is array index into TrackData
        if(CDROM_ADDR_MSF != cdToc.TrackData[nTrack-1].Adr)
        {
            g_pKato->Log(LOG_DETAIL, _T("Skipping track %u; address is not in MSF format"));
            continue;
        }
        
        // request data
        cdRead.StartAddr.Address.msf.msf_Frame = cdToc.TrackData[nTrack-1].Address[3];
        cdRead.StartAddr.Address.msf.msf_Second = cdToc.TrackData[nTrack-1].Address[2];
        cdRead.StartAddr.Address.msf.msf_Minute = cdToc.TrackData[nTrack-1].Address[1];
        cdRead.sglist[0].sb_buf = buffer;
        cdRead.sglist[0].sb_len = CD_RAW_SECTOR_SIZE;

        g_pKato->Log(LOG_DETAIL, _T("TRACK %u -- Reading first sector @ %u:%u:%u (M:S:F)"),
            nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
            cdRead.StartAddr.Address.msf.msf_Minute);

        memset(buffer, (INT)0xFF, CD_RAW_SECTOR_SIZE);
        memset(buffer2, (INT)0xFF, CD_RAW_SECTOR_SIZE);
        
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: DeviceIoControl(IOCTL_CDROM_READ_SG) failed; error %u"), GetLastError());        
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read first sector of track %u @ %u:%u:%u"),
                nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                cdRead.StartAddr.Address.msf.msf_Frame);
            retVal = TPR_FAIL;
            continue;
        }        

        // check the data
        if(!memcmp(buffer, buffer2, CD_RAW_SECTOR_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Raw read did not modify input buffer"));
        }
    }

    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
TestMSFAudioData(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    
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
        g_pKato->Log(LOG_FAIL, _T("FAIL: Global CDRom Disk handle is invalid"));
        return TPR_FAIL;
    }

    HFILE hFile = HFILE_ERROR;
    UCHAR nTrack = (UCHAR)g_cTrackNo;
    DWORD cBytes = 0;
    DWORD cSector = 0;
    DWORD trackSectors = 0;
    WCHAR szTrackName[32] = L"";

    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};
    MSF_ADDR msfTmp = {0};
    CDROM_READ cdRead = {0};
    CDROM_TOC cdToc = {0};
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};

    BYTE buffer[CD_RAW_SECTOR_SIZE] = {0}; // raw cd sector size
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to query cdrom TOC using IOCTL_CDROM_READ_TOC; error %u"), GetLastError());
        return TPR_FAIL;
    }
    
    // make sure requested track exists on the disk
    if(nTrack >= (cdToc.LastTrack))
    {
        g_pKato->Log(LOG_DETAIL, _T("There is no track %u on the CD"), nTrack);
        return TPR_SKIP;
    }

    // make sure track address is MSF format
    // nTrack - 1 is array index into TrackData
    if(CDROM_ADDR_MSF != cdToc.TrackData[nTrack-1].Adr)
    {
        g_pKato->Log(LOG_DETAIL, _T("Track %u address is not in MSF format"), nTrack);
        g_pKato->Log(LOG_DETAIL, _T("Re-run this test with an Audio CD"));
        return TPR_SKIP;
    }
    
    // start address of track
    msfAddrStart.msf_Frame = cdToc.TrackData[nTrack-1].Address[3];
    msfAddrStart.msf_Second = cdToc.TrackData[nTrack-1].Address[2];
    msfAddrStart.msf_Minute = cdToc.TrackData[nTrack-1].Address[1];
    cdTrkStart.Mode = CDROM_ADDR_MSF;
    cdTrkStart.Address.msf = msfAddrStart;
    CDROM_MSF_TO_LBA(&cdTrkStart);

    // end address of track
    msfAddrEnd.msf_Frame = cdToc.TrackData[nTrack].Address[3];
    msfAddrEnd.msf_Second = cdToc.TrackData[nTrack].Address[2];
    msfAddrEnd.msf_Minute = cdToc.TrackData[nTrack].Address[1];
    cdTrkEnd.Mode = CDROM_ADDR_MSF;
    cdTrkEnd.Address.msf = msfAddrEnd;
    CDROM_MSF_TO_LBA(&cdTrkEnd);

    // total number of sectors in the track
    trackSectors = (cdTrkEnd.Address.lba - cdTrkStart.Address.lba);

    g_pKato->Log(LOG_DETAIL, _T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
        nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);

    g_pKato->Log(LOG_DETAIL, _T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
        msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);

    g_pKato->Log(LOG_DETAIL, _T("         -- SIZE     %.2f MB"), 
        (float)trackSectors / (float)(1024 * 1024) * (float)CD_RAW_SECTOR_SIZE);

    // build read packet
    cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
    cdRead.bRawMode = TRUE;
    cdRead.TrackMode = CDDA;
    cdRead.sgcount = 1;
    cdRead.TransferLength = 1;
    cdRead.sglist[0].sb_buf = (LPBYTE)buffer;
    cdRead.sglist[0].sb_len = CD_RAW_SECTOR_SIZE;

    // open a remote file to store the track data in
    _stprintf(szTrackName, _T("cdtrack%02u.pcm"), nTrack);
    hFile = U_ropen(szTrackName, PPSH_WRITE_ACCESS);
    if(HFILE_ERROR == hFile)
    {
        g_pKato->Log(LOG_DETAIL, _T("unable to open remote file %s, skipping"), szTrackName); 
        retVal = TPR_FAIL;
        goto Error;
    }    

    // read audio data 1 sector at a time
    for(cSector = cdTrkStart.Address.lba; cSector < cdTrkStart.Address.lba + trackSectors; cSector++)
    {
        // reading sectors, so build the request in LBA and convert to MSF
        cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
        cdRead.StartAddr.Address.lba = cSector;       
        CDROM_LBA_TO_MSF(&cdRead.StartAddr, msfTmp);

        // read audio data from disk
        cdRead.sglist[0].sb_buf = buffer;
        cdRead.sglist[0].sb_len = CD_RAW_SECTOR_SIZE;
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: DeviceIoControl(IOCTL_CDROM_READ_SG) failed; error %u"), GetLastError());        
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read sector %u of track %u @ %02u:%02u:%02u"), cSector,
                nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                cdRead.StartAddr.Address.msf.msf_Minute);
            goto Error;
        }

        // write audio data to remote file
        if(CD_RAW_SECTOR_SIZE != U_rwrite(hFile, buffer, CD_RAW_SECTOR_SIZE))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to write to remote file"));
            goto Error;
        }

    }

    g_pKato->Log(LOG_DETAIL, _T("Successfully wrote audio data to file %s (Stereo, 16-bit, 44.1 KHz)"), szTrackName);
    g_pKato->Log(LOG_DETAIL, _T("To complete the test, verify that the audio file is correct"));
    
    retVal = TPR_PASS;

Error:
    if(HFILE_ERROR != hFile)
        U_rclose(hFile);

    return retVal;
}
