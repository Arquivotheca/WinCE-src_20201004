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
////////////////////////////////////////////////////////////////////////////////
//
//  CDDATEST TUX DLL
//
//  Module: cddatest.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

// wave file format header for PCM data
typedef struct __WAV_HEADER
{
    DWORD ChunkID;
    DWORD ChunkSize;
    DWORD Format;

    DWORD SubChunk1ID;
    DWORD SubChunk1Size;
    WORD  AudioFormat;
    WORD  NumChannels;
    DWORD SampleRate;
    DWORD ByteRate;
    WORD  BlockAlign;
    WORD  BitsPerSample;
    
    DWORD SubChunk2ID;
    DWORD SubChunk2Size;

} WAV_HEADER, *PWAV_HEADER;
    
// default 64KB heap
static const DWORD DEF_HEAP_SIZE = 65536;

// --------------------------------------------------------------------
DWORD 
CompareRawData(
    BYTE *pbBuffer1, 
    BYTE *pbBuffer2, 
    DWORD cbBuffer)
// --------------------------------------------------------------------
{
    DWORD dwErrors = 0;
    DWORD *pdwBuffer1 = (DWORD*)pbBuffer1;
    DWORD *pdwBuffer2 = (DWORD*)pbBuffer2;
    
    for(DWORD i = 0; i < cbBuffer/4; i++)
    {
        if(pdwBuffer1[i] != pdwBuffer2[i]) 
        {
            // how many bits are different in the DWORDs
            DWORD dwDiff = pdwBuffer1[i] & ~pdwBuffer2[i];
            while(dwDiff)
            {
                dwErrors += dwDiff & 1;
                dwDiff >>= 1;
            }
        }
    }
    return dwErrors;
}

// standard CD track info for .wav file header
static const DWORD  CD_SAMPLE_RATE_HZ       = 44100;
static const DWORD  CD_SAMPLE_BIT_WIDTH     = 16;
static const DWORD  CD_AUDIO_CHANNELS       = 2;

static const DWORD  CD_BITS_PER_SECOND      = CD_AUDIO_CHANNELS * CD_SAMPLE_RATE_HZ * CD_SAMPLE_BIT_WIDTH;
static const DWORD  CD_BYTES_PER_SECOND     = CD_BITS_PER_SECOND / 8;

// --------------------------------------------------------------------
VOID
DumpCDToc(
    CDROM_TOC *pcdToc)    
// --------------------------------------------------------------------
{
    DWORD trackSectors = 0;
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};
    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};

    for(DWORD nTrack = pcdToc->FirstTrack; nTrack <= pcdToc->LastTrack; nTrack++)
    {
        // start address of track
        msfAddrStart.msf_Frame = pcdToc->TrackData[nTrack-1].Address[3];
        msfAddrStart.msf_Second = pcdToc->TrackData[nTrack-1].Address[2];
        msfAddrStart.msf_Minute = pcdToc->TrackData[nTrack-1].Address[1];
        cdTrkStart.Mode = CDROM_ADDR_MSF;
        cdTrkStart.Address.msf = msfAddrStart;
        CDROM_MSF_TO_LBA(&cdTrkStart);
        
        // end address of track
        msfAddrEnd.msf_Frame = pcdToc->TrackData[nTrack].Address[3];
        msfAddrEnd.msf_Second = pcdToc->TrackData[nTrack].Address[2];
        msfAddrEnd.msf_Minute = pcdToc->TrackData[nTrack].Address[1];
        cdTrkEnd.Mode = CDROM_ADDR_MSF;
        cdTrkEnd.Address.msf = msfAddrEnd;
        CDROM_MSF_TO_LBA(&cdTrkEnd);

        // total number of sectors in the track
        trackSectors = (cdTrkEnd.Address.lba - cdTrkStart.Address.lba);
        
        LOG(_T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);
        
        LOG(_T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);
        
        LOG(_T("         -- SIZE     %.2f MB"), 
            (float)trackSectors / (float)(0x100000) * (float)CDROM_RAW_SECTOR_SIZE);
    }
}

// --------------------------------------------------------------------
VOID 
GenerateWaveFileHeader(
    DWORD cbTrackSize,
    PWAV_HEADER pHeader )    
// --------------------------------------------------------------------
{
    // build .wav file header

    // subchunk2 contains actual audio data
    memcpy(&pHeader->SubChunk2ID, "data", sizeof(DWORD));
    pHeader->SubChunk2Size = cbTrackSize;

    // subchunk1 contains audio format description
    memcpy(&pHeader->SubChunk1ID, "fmt ", sizeof(DWORD));
    pHeader->SubChunk1Size = CD_SAMPLE_BIT_WIDTH;
    pHeader->AudioFormat = 1; // type 1 indicates PCM
    pHeader->NumChannels = CD_AUDIO_CHANNELS;
    pHeader->SampleRate = CD_SAMPLE_RATE_HZ;
    pHeader->BitsPerSample = CD_SAMPLE_BIT_WIDTH;
    pHeader->ByteRate = pHeader->SampleRate * pHeader->NumChannels * (pHeader->BitsPerSample / 8);
    pHeader->BlockAlign = pHeader->NumChannels * (pHeader->BitsPerSample / 8);

    // complete the header
    memcpy(&pHeader->ChunkID, "RIFF", sizeof(DWORD));
    pHeader->ChunkSize = 36 + pHeader->SubChunk2Size;
    memcpy(&pHeader->Format, "WAVE", sizeof(DWORD));
}

// --------------------------------------------------------------------
HANDLE OpenDevice(LPCTSTR pszDiskName)
// --------------------------------------------------------------------
{
    WCHAR szDevice[MAX_PATH];
    StringCchCopyEx(szDevice, MAX_PATH, pszDiskName, NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    
    // open the device as either a store or a stream device
    if(g_fOpenAsStore)
    {
        return OpenStore(szDevice);
    }
    else if(g_fOpenAsVolume)
    {
        StringCchCatEx(szDevice, MAX_PATH, L"\\VOL:", NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    
    return CreateFile(szDevice, GENERIC_READ, FILE_SHARE_READ, 
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}


// --------------------------------------------------------------------
HANDLE OpenCDDAHandle()
// --------------------------------------------------------------------
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = NULL;
    
    STORAGEDEVICEINFO sdi = {0};

    CDROM_TOC cdToc = {0};
    CDROM_DISCINFO diskInfo = {0};
    DWORD cbReturned = 0;
    
    // if a disk name was specified, try to open it
    if(g_szDiskName[0])
    {
        hDisk = OpenDevice(g_szDiskName);
        if(INVALID_HANDLE_VALUE == hDisk)
        {
            g_pKato->Log(LOG_DETAIL, _T("unable to open disk \"%s\"; error %u"), g_szDiskName, GetLastError());
        }
        return hDisk;
    }

    // enumerate storage devices using the specified method
    //
    if(g_fOpenAsVolume)
    {   
        // open the storage device as a volume; e.g. CreateFile on \CDROM Drive\VOL:
        // this method requires the storage device to be mounted
        
        // detect any mounted CDDA devices
        hDetect = DEV_DetectFirstDevice(&CDDA_MOUNT_GUID, szDisk, MAX_PATH);
    }
    else if(g_fOpenAsStore)
    {
        // open the storage device as a store using OpenStore
        
        // look for stores advertized by storage manager
        hDetect = DEV_DetectFirstDevice(&STORE_MOUNT_GUID, szDisk, MAX_PATH);
    }
    else // open as a device
    {
        // open the storage device 
        // look for block devices advertized by device manager
        hDetect = DEV_DetectFirstDevice(&BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
    }
    if(hDetect)
    {
        do
        {            
            // open a handle to the enumerated device
            g_pKato->Log(LOG_DETAIL, _T("opening disk \"%s\"..."), szDisk);
            hDisk = OpenDevice(szDisk); 
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                g_pKato->Log(LOG_DETAIL, _T("unable to open disk \"%s\"; error %u"), szDisk, GetLastError());
                continue;
            }

            // support filtering devices by user-specified profile
            if(g_szProfile[0])
            {
                if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
                {
                    g_pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), szDisk, GetLastError());
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
                else
                {
                    // check for a profile match
                    if(0 != wcsicmp(g_szProfile, sdi.szProfile))
                    {
                        g_pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"%s\""), szDisk, sdi.szProfile, g_szProfile);
                        VERIFY(CloseHandle(hDisk));
                        hDisk = INVALID_HANDLE_VALUE;
                        continue;
                    }
                }
            }
            
            // query driver for cdrom disk information structure
            if(!DeviceIoControl(hDisk, IOCTL_CDROM_DISC_INFO, NULL, 0, &diskInfo, sizeof(CDROM_DISCINFO), &cbReturned, NULL))
            {
                // if the call fails, this is not a cdrom device
                g_pKato->Log(LOG_DETAIL, _T("\"%s\" is not a cdrom device"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }

            // query driver for cdrom TOC structure
            // verify that there is AT LEAST 1 track, and that the track is an audio track
            if(!DeviceIoControl(hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cbReturned, NULL) ||
                cdToc.LastTrack == 0 || cdToc.TrackData[0].Control & 0x4)

            {
                // if the call fails, this is not a cdrom device
                g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not contain a CDDA (audio) disk"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }

            g_pKato->Log(LOG_DETAIL, _T("found CDROM device with CDDA disk: \"%s\""), szDisk);
            DumpCDToc(&cdToc);
            break;
        } while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));

        DEV_DetectClose(hDetect);
    }

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        g_pKato->Log(LOG_DETAIL, _T("found no audio CD-ROM disks!"));
        g_pKato->Log(LOG_DETAIL, _T("please insert a CDDA CD into your CD-ROM drive and re-run the test"));
    }
    
    return hDisk;
}


// --------------------------------------------------------------------
TESTPROCAPI 
Tst_AudioCDBVT(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_PASS;
    
    if(TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if(TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    } 
    
    UCHAR nTrack = 1;
    DWORD cBytes = 0;
    CDROM_ADDR cdAddr = {0};
    CDROM_TOC cdToc = {0};
    CDROM_READ cdRead = {0};
    RAW_READ_INFO cdRawRead = {0};
    HANDLE hHeap = NULL;
    BYTE *pBuffer1 = NULL, *pBuffer2 = NULL;
    
    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }

    // create heap for local buffers
    hHeap = HeapCreate(0, DEF_HEAP_SIZE, 0);
    if(NULL == hHeap)
    {
        ERRFAIL("HeapCreate()");
        goto done;
    }

    pBuffer1 = (BYTE*)HeapAlloc(hHeap, 0, CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer1)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }

    pBuffer2 = (BYTE*)HeapAlloc(hHeap, 0, CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer2)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    // make sure requested track exists on the disk
    if( (nTrack > cdToc.LastTrack) || (nTrack < cdToc.FirstTrack) )
    {
        g_pKato->Log(LOG_SKIP, _T("There is no track %u on the CD"), nTrack);
        goto done;
    }

    LOG(_T("reading one sector two times from each data track and verifying data..."));

    // static sg read info
    cdRead.TransferLength = 1;
    cdRead.sgcount = 1;
    cdRead.bRawMode = TRUE;
    cdRead.TrackMode = CDDA;

    // static raw read info     
    cdRawRead.SectorCount = 1;
    cdRawRead.TrackMode = CDDA;
   
    // try to read the beginning of each track
    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            g_pKato->Log(LOG_DETAIL, _T("Track %u is not an audio track"), nTrack);
            continue;
        }

        cdAddr.Mode = CDROM_ADDR_MSF;
        cdAddr.Address.msf.msf_Frame = cdToc.TrackData[nTrack-1].Address[3];
        cdAddr.Address.msf.msf_Second = cdToc.TrackData[nTrack-1].Address[2];
        cdAddr.Address.msf.msf_Minute = cdToc.TrackData[nTrack-1].Address[1];
        
        // request data
        memcpy(&cdRead.StartAddr, &cdAddr, sizeof(CDROM_ADDR));
        cdRead.sglist[0].sb_buf = pBuffer1;
        cdRead.sglist[0].sb_len = CDROM_RAW_SECTOR_SIZE;

        g_pKato->Log(LOG_DETAIL, _T("TRACK %u -- Reading first sector @ %u:%u:%u (M:S:F)"),
            nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
            cdRead.StartAddr.Address.msf.msf_Frame);
        
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_SG)");       
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read first sector of track %u @ %u:%u:%u"),
                nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                cdRead.StartAddr.Address.msf.msf_Frame);            
            LOG(_T("NOTE: THIS MAY NOT BE A BUG! check CD for possible scratches/damage"));
            continue;
        }

        CDROM_MSF_TO_LBA(&cdAddr);
        cdRawRead.DiskOffset.LowPart = cdAddr.Address.lba;

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_RAW_READ, &cdRawRead, sizeof(RAW_READ_INFO), pBuffer2, CDROM_RAW_SECTOR_SIZE, &cBytes, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_RAW_READ)");
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read 1 raw sector at LBA %u"), cdRawRead.DiskOffset.LowPart);
            LOG(_T("NOTE: THIS MAY NOT BE A BUG! check CD for possible scratches/damage"));            
            continue;
        }
    }

done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_AudioCDDataChk(
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

    UCHAR nTrack = 0;
    DWORD dwErrors = 0;
    DWORD sectorsPerRead = lpFTE->dwUserData;
    DWORD sectorsToRead = 0;
    DWORD cBytes = 0;
    DWORD cSector = 0;
    DWORD trackSectors = 0;
    DWORD cbWritten = 0;
    DWORD dwPercentComplete = 0;

    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};
    MSF_ADDR msfTmp = {0};

    CDROM_READ cdRead = {0};
    RAW_READ_INFO cdRawRead = {0};

    CDROM_TOC cdToc = {0};
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};

    HANDLE hHeap = NULL;
    BYTE *pBuffer1 = NULL;
    BYTE *pBuffer2 = NULL;

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }

    // create heap for local buffers
    hHeap = HeapCreate(0, DEF_HEAP_SIZE, 0);
    if(NULL == hHeap)
    {
        ERRFAIL("HeapCreate()");
        goto done;
    }

    pBuffer1 = (BYTE*)HeapAlloc(hHeap, 0, sectorsPerRead*CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer1)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }

    pBuffer2 = (BYTE*)HeapAlloc(hHeap, 0, sectorsPerRead*CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer2)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    LOG(_T("reading each data track twice using %u raw sectors per read and verifying data..."), sectorsPerRead);

    // read each track
    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            g_pKato->Log(LOG_DETAIL, _T("Track %u is not an audio track"), nTrack);
            continue;
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

        LOG(_T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);

        LOG(_T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);

        LOG(_T("         -- SIZE     %.2f MB"), 
            (float)trackSectors / (float)(0x100000) * (float)CDROM_RAW_SECTOR_SIZE);

        // build sg read packet
        cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
        cdRead.bRawMode = TRUE;
        cdRead.TrackMode = CDDA;
        cdRead.sgcount = 1;        

        // build raw read packet         
        cdRawRead.TrackMode = CDDA;

        dwPercentComplete = 0xFFFFFFFF;
        for(cSector = cdTrkStart.Address.lba; cSector < cdTrkStart.Address.lba + trackSectors; cSector += sectorsPerRead)
        {
            if(dwPercentComplete != (100*(cSector-cdTrkStart.Address.lba))/(trackSectors)) 
            {
                dwPercentComplete = (100*(cSector-cdTrkStart.Address.lba))/(trackSectors);
                LOG(_T("Track %u verified %u%%..."), nTrack, dwPercentComplete);
            }

            sectorsToRead = sectorsPerRead;
            if(cSector + sectorsPerRead > cdTrkStart.Address.lba + trackSectors)
            {
                sectorsToRead = cdTrkStart.Address.lba + trackSectors - cSector;
            }
            
            // reading sectors, so build the request in LBA and convert to MSF
            cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
            cdRead.StartAddr.Address.lba = cSector;       
            CDROM_LBA_TO_MSF(&cdRead.StartAddr, msfTmp);

            // read raw audio data from disk using IOCTL_CDROM_READ_SG
            cdRead.TransferLength = sectorsToRead;
            cdRead.sglist[0].sb_buf = pBuffer1;
            cdRead.sglist[0].sb_len = sectorsPerRead*CDROM_RAW_SECTOR_SIZE;
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_SG)");
                g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read %u sectors at sector %u of track %u @ %02u:%02u:%02u"), 
                    sectorsToRead, cSector, nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                    cdRead.StartAddr.Address.msf.msf_Frame);
                LOG(_T("NOTE: THIS MAY NOT BE A BUG! check CD for possible scratches/damage"));
                goto done;
            }

            // read raw audio data from disk using IOCTL_CDROM_RAW_READ            
            cdRawRead.SectorCount = sectorsToRead;
            cdRawRead.DiskOffset.LowPart = cSector;
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_RAW_READ, &cdRawRead, sizeof(RAW_READ_INFO), pBuffer2, sectorsToRead*CDROM_RAW_SECTOR_SIZE, &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_RAW_READ)");
                g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read %u raw sector at LBA %u"), sectorsToRead, cdRawRead.DiskOffset.LowPart);
                LOG(_T("NOTE: THIS MAY NOT BE A BUG! check CD for possible scratches/damage"));                
                continue;
            }

            if(cBytes != sectorsToRead*CDROM_RAW_SECTOR_SIZE)
            {
                // do we care?
            }

            // compare both read requests
            dwErrors = CompareRawData(pBuffer1, pBuffer2, sectorsPerRead*CDROM_RAW_SECTOR_SIZE);
            if(dwErrors)
            {
                LOG(_T("WARNING: data contained %u bit errors out of %u bits (%u%%)"), 
                    dwErrors, sectorsPerRead*CDROM_RAW_SECTOR_SIZE*8, 100*dwErrors/(sectorsPerRead*CDROM_RAW_SECTOR_SIZE*8));
            }      
        }
    }
    
done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));

    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_AudioCDDataThroughput(
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

    UCHAR nTrack = 0;
    DWORD dwErrors = 0;
    DWORD sectorsPerRead = lpFTE->dwUserData;
    DWORD sectorsToRead = 0;
    DWORD cBytes = 0;
    DWORD cSector = 0;
    DWORD trackSectors = 0;
    DWORD cbWritten = 0;
    DWORD dwPercentComplete = 0;
    DWORD tStart = 0;
    DWORD tTotal = 0;
    DWORD cbTotal = 0;

    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};
    MSF_ADDR msfTmp = {0};

    CDROM_READ cdRead = {0};
    RAW_READ_INFO cdRawRead = {0};

    CDROM_TOC cdToc = {0};
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};

    HANDLE hHeap = NULL;
    BYTE *pBuffer1 = NULL;
    BYTE *pBuffer2 = NULL;

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }

    // create heap for local buffers
    hHeap = HeapCreate(0, DEF_HEAP_SIZE, 0);
    if(NULL == hHeap)
    {
        ERRFAIL("HeapCreate()");
        goto done;
    }

    pBuffer1 = (BYTE*)HeapAlloc(hHeap, 0, sectorsPerRead*CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer1)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    LOG(_T("reading each data track using %u raw sectors per read to measure throughput..."), sectorsPerRead);

    // read each track
    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            g_pKato->Log(LOG_DETAIL, _T("Track %u is not an audio track"), nTrack);
            continue;
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

        LOG(_T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);

        LOG(_T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);

        LOG(_T("         -- SIZE     %.2f MB"), 
            (float)trackSectors / (float)(0x100000) * (float)CDROM_RAW_SECTOR_SIZE);

        // build sg read packet
        cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
        cdRead.bRawMode = TRUE;
        cdRead.TrackMode = CDDA;
        cdRead.sgcount = 1;        

        dwPercentComplete = 0xFFFFFFFF;
        for(cSector = cdTrkStart.Address.lba; cSector < cdTrkStart.Address.lba + trackSectors; cSector += sectorsPerRead)
        {
            if(dwPercentComplete != (100*(cSector-cdTrkStart.Address.lba))/(trackSectors)) 
            {
                dwPercentComplete = (100*(cSector-cdTrkStart.Address.lba))/(trackSectors);
                if(tStart) 
                {                
                    LOG(_T("Track %u read %u%%, avg. throughput = %u bytes/s..."), nTrack, dwPercentComplete, cbTotal/tTotal*1000);
                }
            }

            sectorsToRead = sectorsPerRead;
            if(cSector + sectorsPerRead > cdTrkStart.Address.lba + trackSectors)
            {
                sectorsToRead = cdTrkStart.Address.lba + trackSectors - cSector;
            }
            
            // reading sectors, so build the request in LBA and convert to MSF
            cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
            cdRead.StartAddr.Address.lba = cSector;       
            CDROM_LBA_TO_MSF(&cdRead.StartAddr, msfTmp);

            // read raw audio data from disk using IOCTL_CDROM_READ_SG
            cdRead.TransferLength = sectorsToRead;
            cdRead.sglist[0].sb_buf = pBuffer1;
            cdRead.sglist[0].sb_len = sectorsPerRead*CDROM_RAW_SECTOR_SIZE;
            tStart = GetTickCount();
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_SG)");
                g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read %u sectors at sector %u of track %u @ %02u:%02u:%02u"), 
                    sectorsToRead, cSector, nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                    cdRead.StartAddr.Address.msf.msf_Frame);
                LOG(_T("NOTE: THIS MAY NOT BE A BUG! check CD for possible scratches/damage"));
                goto done;
            }
            tStart = GetTickCount() - tStart;

            if(sectorsToRead == sectorsPerRead)
            {
                // we don't perform the check against the required throughput if we read
                // a smaller chunk of data-- it is possible that we only read 1 or 2 sectors
                // near the end of the track, and this will give us a low throughput number
                // and isn't necessarily a driver bug
                if(sectorsToRead * CDROM_RAW_SECTOR_SIZE / tStart * 1000 < CD_BYTES_PER_SECOND)
                {
                    LOG(_T("WARNING: last read throughput %u bytes/s is less than the required %u bytes/s (2 channels, 16-bit @ 44.1 kHZ)"),
                        sectorsToRead * CDROM_RAW_SECTOR_SIZE / tStart * 1000 , CD_BYTES_PER_SECOND);
                }
            }

            // track total times
            tTotal += tStart;
            cbTotal += (sectorsToRead * CDROM_RAW_SECTOR_SIZE);
        }
    }
    
done:
    if(tTotal && cbTotal)
    {
        LOG(_T("the average data read throughput was %u bytes/s while performing %u sector reads"), 
            cbTotal/tTotal*1000, sectorsPerRead);
        LOG(_T("cd quality sound (2 channel, 16-bit @ 44.1 kHZ) requires %u bytes/s"), CD_BYTES_PER_SECOND);

        if(cbTotal/tTotal*1000 < CD_BYTES_PER_SECOND)
        {
            FAIL("average throughput is unable to sustain the 44.1 kHZ sample rate");
        }
    }
    if(hHeap) VERIFY(HeapDestroy(hHeap));

    return GetTestResult();
}


// --------------------------------------------------------------------
TESTPROCAPI 
Tst_TrackToWavFile(
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

    HANDLE hFile = INVALID_HANDLE_VALUE;
    UCHAR nTrack = (UCHAR)g_cTrackNo;
    DWORD sectorsPerRead = lpFTE->dwUserData;
    DWORD sectorsToRead = 0;
    DWORD cBytes = 0;
    DWORD cSector = 0;
    DWORD trackSectors = 0;
    DWORD cbWritten = 0;
    DWORD dwPercentComplete = 0;
    WCHAR szTrackName[MAX_PATH] = L"";

    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};
    MSF_ADDR msfTmp = {0};
    CDROM_READ cdRead = {0};
    CDROM_TOC cdToc = {0};
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};

    HANDLE hHeap = NULL;
    BYTE *pBuffer1 = NULL;
    
    WAV_HEADER header = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // create heap for local buffers
    hHeap = HeapCreate(0, DEF_HEAP_SIZE, 0);
    if(NULL == hHeap)
    {
        ERRFAIL("HeapCreate()");
        goto done;
    }

    pBuffer1 = (BYTE*)HeapAlloc(hHeap, 0, sectorsPerRead*CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer1)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }
    
    // make sure requested track exists on the disk
    if(nTrack > cdToc.LastTrack || nTrack < cdToc.FirstTrack)
    {
        g_pKato->Log(LOG_SKIP, _T("Track %u does not exist (LastTrack=%u)"), nTrack, cdToc.LastTrack);
        goto done;
    }

    // make sure it is an audio track
    if(cdToc.TrackData[nTrack-1].Control & 0x4)
    {
        g_pKato->Log(LOG_SKIP, _T("Track %u is not an audio track"), nTrack);
        g_pKato->Log(LOG_SKIP, _T("Re-run this test with an Audio CD"));
        goto done;
    }

    LOG(_T("reading track %u using %u raw sectors per read and writing to output .wav file \"%s\"..."), 
        nTrack, sectorsPerRead, szTrackName);
    
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

    LOG(_T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
        nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);

    LOG(_T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
        msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);

    LOG(_T("         -- SIZE     %.2f MB"), 
        (float)trackSectors / (float)(0x100000) * (float)CDROM_RAW_SECTOR_SIZE);

    GenerateWaveFileHeader(trackSectors * CDROM_RAW_SECTOR_SIZE, &header);

    // open a remote file to store the track data in
    StringCchPrintfEx(szTrackName, MAX_PATH, NULL, NULL, STRSAFE_NULL_ON_FAILURE, _T("%s\\cdtrack%02u.wav"), g_szOutputDir, nTrack);
    LOG(_T("PCM data from track %u will be saved to file \"%s\""), nTrack, szTrackName);
    hFile = CreateFile(szTrackName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hFile)
    {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // write the header to the .wav file
    LOG(_T("writing .WAV file header to file \"%s\"..."), szTrackName);
    if(!WriteFile(hFile, (BYTE*)&header, sizeof(WAV_HEADER), &cbWritten, NULL) ||
        cbWritten != sizeof(WAV_HEADER))
    {
        ERRFAIL("WriteFile()");
        goto done;
    }

    // build read packet
    cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
    cdRead.bRawMode = TRUE;
    cdRead.TrackMode = CDDA;
    cdRead.sgcount = 1;
    cdRead.sglist[0].sb_buf = pBuffer1;
    cdRead.sglist[0].sb_len = CDROM_RAW_SECTOR_SIZE*sectorsPerRead;

    // read audio data 1 sector at a time
    dwPercentComplete = 0xFFFFFFFF;
    for(cSector = cdTrkStart.Address.lba; cSector < cdTrkStart.Address.lba + trackSectors; cSector += sectorsPerRead)
    {
        if(dwPercentComplete != (100*(cSector-cdTrkStart.Address.lba))/(trackSectors)) 
        {
            dwPercentComplete = (100*(cSector-cdTrkStart.Address.lba))/(trackSectors);
            LOG(_T("Track %u copied %u%%..."), nTrack, dwPercentComplete);
        }
        
        sectorsToRead = sectorsPerRead;
        if(cSector + sectorsPerRead > cdTrkStart.Address.lba + trackSectors)
        {
            sectorsToRead = cdTrkStart.Address.lba + trackSectors - cSector;
        }
        
        // reading sectors, so build the request in LBA and convert to MSF
        cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
        cdRead.StartAddr.Address.lba = cSector;       
        CDROM_LBA_TO_MSF(&cdRead.StartAddr, msfTmp);

        // read audio data from disk
        cdRead.TransferLength = sectorsToRead;
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_SG)");
            g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read %u sectors at sector %u of track %u @ %02u:%02u:%02u"), 
                sectorsToRead, cSector, nTrack, cdRead.StartAddr.Address.msf.msf_Minute, 
                cdRead.StartAddr.Address.msf.msf_Second, cdRead.StartAddr.Address.msf.msf_Frame);
            goto done;
        }

        // write audio data to remote file
        if(!WriteFile(hFile, pBuffer1, sectorsToRead*CDROM_RAW_SECTOR_SIZE, &cbWritten, NULL) ||
            cbWritten != sectorsToRead*CDROM_RAW_SECTOR_SIZE)
        {
            ERRFAIL("WriteFile()");
            if(ERROR_DISK_FULL == GetLastError())
            {
                LOG(_T("The output location, \"%s\", may be full. Try freeing some space and re-running the test"),
                    g_szOutputDir);
            }
            goto done;
        }
    }

    LOG(_T("Successfully wrote audio data to file %s (Stereo, 16-bit, 44.1 KHz)"), szTrackName);
    LOG(_T("To complete the test, verify that the audio file is correct"));
    
done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    if(INVALID_HANDLE_VALUE != hFile)
        VERIFY(CloseHandle(hFile));
    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_TrackSummary(
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

    HANDLE hFile = INVALID_HANDLE_VALUE;
    UCHAR nTrack = 0;
    DWORD cBytes = 0;
    DWORD cSector = 0;
    DWORD trackSectors = 0;
    DWORD cbWritten = 0;
    DWORD sectorsPerTrack = lpFTE->dwUserData;
    DWORD sectorsWritten = 0;
    WCHAR szTrackName[MAX_PATH] = L"";

    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfTmp = {0};
    CDROM_READ cdRead = {0};
    CDROM_TOC cdToc = {0};
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};

    HANDLE hHeap = NULL;
    BYTE *pBuffer1 = NULL;
    
    WAV_HEADER header = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }

    // create heap for local buffers
    hHeap = HeapCreate(0, DEF_HEAP_SIZE, 0);
    if(NULL == hHeap)
    {
        ERRFAIL("HeapCreate()");
        goto done;
    }

    pBuffer1 = (BYTE*)HeapAlloc(hHeap, 0, CDROM_RAW_SECTOR_SIZE);
    if(NULL == pBuffer1)
    {
        ERRFAIL("HeapAlloc()");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    // open a remote file to store the track data in
    StringCchPrintfEx(szTrackName, MAX_PATH, NULL, NULL, STRSAFE_NULL_ON_FAILURE, _T("%s\\summary.wav"), g_szOutputDir);
    LOG(_T("PCM data from track %u will be saved to file \"%s\""), nTrack, szTrackName);
    hFile = CreateFile(szTrackName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hFile)
    {
        ERRFAIL("CreateFile()");
        goto done;
    }

    LOG(_T("reading %u sectors of each data track and writing to output .wav file \"%s\"..."), 
        sectorsPerTrack, szTrackName);

    // seek past the .wav header-- it will be added once the file is written
    VERIFY(sizeof(WAV_HEADER) == SetFilePointer(hFile, sizeof(WAV_HEADER), NULL, FILE_BEGIN));

    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            g_pKato->Log(LOG_DETAIL, _T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        msfAddrStart.msf_Frame = cdToc.TrackData[nTrack-1].Address[3];
        msfAddrStart.msf_Second = cdToc.TrackData[nTrack-1].Address[2];
        msfAddrStart.msf_Minute = cdToc.TrackData[nTrack-1].Address[1];
        cdTrkStart.Mode = CDROM_ADDR_MSF;
        cdTrkStart.Address.msf = msfAddrStart;
        CDROM_MSF_TO_LBA(&cdTrkStart);

        cdTrkEnd.Mode = CDROM_ADDR_LBA;
        cdTrkEnd.Address.lba = cdTrkStart.Address.lba + sectorsPerTrack;

        // total number of sectors in the track
        trackSectors = (cdTrkEnd.Address.lba - cdTrkStart.Address.lba);

        LOG(_T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);

        LOG(_T("copying %u sectors to .wav file \"%s\""), sectorsPerTrack, szTrackName);

        // build read packet
        cdRead.StartAddr.Mode = CDROM_ADDR_MSF;
        cdRead.bRawMode = TRUE;
        cdRead.TrackMode = CDDA;
        cdRead.sgcount = 1;
        cdRead.TransferLength = 1;

        // read audio data 1 sector at a time
        for(cSector = cdTrkStart.Address.lba; cSector < cdTrkStart.Address.lba + trackSectors; cSector++)
        {
            // reading sectors, so build the request in LBA and convert to MSF
            cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
            cdRead.StartAddr.Address.lba = cSector;       
            CDROM_LBA_TO_MSF(&cdRead.StartAddr, msfTmp);

            // read audio data from disk
            cdRead.sglist[0].sb_buf = pBuffer1;
            cdRead.sglist[0].sb_len = CDROM_RAW_SECTOR_SIZE;
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_SG)");
                g_pKato->Log(LOG_FAIL, _T("FAIL: Unable to read sector %u of track %u @ %02u:%02u:%02u"), cSector,
                    nTrack, cdRead.StartAddr.Address.msf.msf_Minute, cdRead.StartAddr.Address.msf.msf_Second,
                    cdRead.StartAddr.Address.msf.msf_Frame);
                goto done;
            }

            // write audio data to remote file
            if(!WriteFile(hFile, pBuffer1, CDROM_RAW_SECTOR_SIZE, &cbWritten, NULL) ||
                cbWritten != CDROM_RAW_SECTOR_SIZE)
            {
                ERRFAIL("WriteFile()");
                if(ERROR_DISK_FULL == GetLastError())
                {
                    LOG(_T("The output disk, \"%s\", may be full. Try freeing some space and re-running the test"),
                        g_szOutputDir);
                }
                goto done;
            }            
        }
        sectorsWritten += sectorsPerTrack;
    }

    // write the header to the .wav file
    GenerateWaveFileHeader(sectorsWritten * CDROM_RAW_SECTOR_SIZE, &header);
    VERIFY(0 == SetFilePointer(hFile, 0, NULL, FILE_BEGIN));
    LOG(_T("writing .WAV file header to \"%s\"..."), szTrackName);
    if(!WriteFile(hFile, (BYTE*)&header, sizeof(WAV_HEADER), &cbWritten, NULL) ||
        cbWritten != sizeof(WAV_HEADER))
    {
        ERRFAIL("WriteFile()");
        goto done;
    }

    LOG(_T("Successfully wrote audio data to file %s (Stereo, 16-bit, 44.1 KHz)"), szTrackName);
    LOG(_T("To complete the test, verify that the audio file is correct"));
    
done:
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    if(INVALID_HANDLE_VALUE != hFile)
        VERIFY(CloseHandle(hFile));
    return GetTestResult();
}

VOID
DumpPosition(
    SUB_Q_CURRENT_POSITION *pCurrentPosition)
{
    ASSERT(pCurrentPosition);

    DWORD dwAddr;
    CDROM_ADDR addrRelative, addrAbsolute;
    MSF_ADDR msfTmp;
    LPCWSTR pszAudioStatus = NULL;

    switch(pCurrentPosition->Header.AudioStatus)
    {
        case AUDIO_STATUS_IN_PROGRESS:
            pszAudioStatus = L"AUDIO_STATUS_IN_PROGRESS";
            break;
            
        case AUDIO_STATUS_PAUSED:
            pszAudioStatus = L"AUDIO_STATUS_PAUSED";
            break;

        case AUDIO_STATUS_NO_STATUS:
            pszAudioStatus = L"AUDIO_STATUS_NO_STATUS";
            break;

        case AUDIO_STATUS_PLAY_COMPLETE:
            pszAudioStatus = L"AUDIO_STATUS_PLAY_COMPLETE";
            break;
        
        case AUDIO_STATUS_NOT_SUPPORTED:
            pszAudioStatus = L"AUDIO_STATUS_NOT_SUPPORTED";
            break;

        case AUDIO_STATUS_PLAY_ERROR:
            pszAudioStatus = L"AUDIO_STATUS_PLAY_ERROR";
            break;

        default:
            pszAudioStatus = L"!!!ERROR!!!";
            break;

    }
        
    LOG(_T("SUB_Q_CURRENT_POSITION.Header.AudioStatus   = %s"), pszAudioStatus);
#ifdef DEBUG
    LOG(_T("SUB_Q_CURRENT_POSITION.FormatCode           = 0x%02X"), pCurrentPosition->FormatCode);
    LOG(_T("SUB_Q_CURRENT_POSITION.Control              = 0x%01X"), pCurrentPosition->Control);
    LOG(_T("SUB_Q_CURRENT_POSITION.ADR                  = 0x%01X"), pCurrentPosition->ADR);
    LOG(_T("SUB_Q_CURRENT_POSITION.IndexNumber          = %u"), pCurrentPosition->IndexNumber);
#endif // DEBUG
    LOG(_T("SUB_Q_CURRENT_POSITION.TrackNumber          = %u"), pCurrentPosition->TrackNumber);

    addrAbsolute.Mode = CDROM_ADDR_LBA;
    memcpy(&dwAddr, pCurrentPosition->AbsoluteAddress, sizeof(DWORD));
    addrAbsolute.Address.lba = BEDW_TO_DW(dwAddr);
    
    addrRelative.Mode = CDROM_ADDR_LBA;
    memcpy(&dwAddr, pCurrentPosition->TrackRelativeAddress, sizeof(DWORD));
    addrRelative.Address.lba = BEDW_TO_DW(dwAddr);   

    // convert to MSF
    CDROM_LBA_TO_MSF(&addrAbsolute, msfTmp);    
    CDROM_LBA_TO_MSF(&addrRelative, msfTmp);
    
    LOG(_T("SUB_Q_CURRENT_POSITION.AbsoluteAddress      = %u:%u:%u"), 
        addrAbsolute.Address.msf.msf_Minute, 
        addrAbsolute.Address.msf.msf_Second,
        addrAbsolute.Address.msf.msf_Frame);

    
    LOG(_T("SUB_Q_CURRENT_POSITION.TrackRelativeAddress = %u:%u:%u"), 
        addrRelative.Address.msf.msf_Minute, 
        addrRelative.Address.msf.msf_Second,
        addrRelative.Address.msf.msf_Frame);
}

    

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PlayTracksInOrder(
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

    DWORD nTrack, cBytes, tStart, tSleep = lpFTE->dwUserData;
    DWORD dwAddr1, dwAddr2;
    CDROM_PLAY_AUDIO_MSF cdpamsf = {0};    
    CDROM_TOC cdToc = {0};

    CDROM_SUB_Q_DATA_FORMAT sqdf = {0};
    SUB_Q_CHANNEL_DATA sqcdNew = {0};
    SUB_Q_CHANNEL_DATA sqcdLast = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    // play each track
    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            LOG(_T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        cdpamsf.StartingF = cdToc.TrackData[nTrack-1].Address[3];
        cdpamsf.StartingS = cdToc.TrackData[nTrack-1].Address[2];
        cdpamsf.StartingM = cdToc.TrackData[nTrack-1].Address[1];

        // end address of track
        cdpamsf.EndingF = cdToc.TrackData[nTrack].Address[3];
        cdpamsf.EndingS = cdToc.TrackData[nTrack].Address[2];
        cdpamsf.EndingM = cdToc.TrackData[nTrack].Address[1];

        LOG(_T("TRACK %2u -- PLAYING %02u:%02u:%02u to  %02u:%02u:%02u"), nTrack, 
            cdpamsf.StartingM, cdpamsf.StartingS, cdpamsf.StartingF,
            cdpamsf.EndingM, cdpamsf.EndingS, cdpamsf.EndingF);

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_PLAY_AUDIO_MSF, &cdpamsf, sizeof(cdpamsf), NULL, 0, NULL, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_PLAY_AUDIO_MSF)");
            goto done;
        }

        memset(&sqcdLast, 0, sizeof(SUB_Q_CHANNEL_DATA));
        
        tStart = GetTickCount();
        for(;;)
        {
            LOG(_T("allowing track to play for %u ms..."), tSleep / 10);
            Sleep(tSleep / 10);

            sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
            
            LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
                &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
                goto done;
            }

            DumpPosition(&sqcdNew.CurrentPosition);

            if(AUDIO_STATUS_IN_PROGRESS != sqcdNew.CurrentPosition.Header.AudioStatus)
            {
                if(AUDIO_STATUS_PLAY_COMPLETE == sqcdNew.CurrentPosition.Header.AudioStatus)
                {
                    LOG(_T("Track finished playing before timeout"));
                    break;
                }

                FAIL("CurrentPosition.Header.AudioStatus != AUDIO_STATUS_IN_PROGRESS");
            }

            // make sure the absolute address is progressing
            memcpy(&dwAddr1, sqcdNew.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            memcpy(&dwAddr2, sqcdLast.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            if(BEDW_TO_DW(dwAddr1) <= BEDW_TO_DW(dwAddr2))
            {
                FAIL("CurrentPosition.AbsoluteAddress went back in time");
            }

            // make sure we are playing the right track
            if(sqcdNew.CurrentPosition.TrackNumber != nTrack)
            {
                FAIL("CurrentPosition.TrackNumber is incorrect");
            }

            // save a copy of the data for comparison next time
            memcpy(&sqcdLast, &sqcdNew, sizeof(SUB_Q_CHANNEL_DATA));

            if(GetTickCount() - tStart > tSleep)
            {
                break;
            }
        }

        // every other track, stop before playing
        if(0 == (nTrack % 2))
        {
            LOG(_T("stopping playback (IOCTL_CDROM_STOP_AUDIO)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, NULL, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_STOP_AUDIO)");
            }
            else
            {
                if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
                    &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
                {
                    ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
                }
                else
                {
                    DumpPosition(&sqcdNew.CurrentPosition);
                }
            }
        }

    }
    
done:
    DeviceIoControl(g_hDisk, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, NULL, NULL);
    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PlayTracksReverseOrder(
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

    DWORD nTrack, cBytes, tStart, tSleep = lpFTE->dwUserData;
    DWORD dwAddr1, dwAddr2;
    CDROM_PLAY_AUDIO_MSF cdpamsf = {0};    
    CDROM_TOC cdToc = {0};

    CDROM_SUB_Q_DATA_FORMAT sqdf = {0};
    SUB_Q_CHANNEL_DATA sqcdNew = {0};
    SUB_Q_CHANNEL_DATA sqcdLast = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    // play each track
    for(nTrack = cdToc.LastTrack; nTrack >= cdToc.FirstTrack && nTrack <= cdToc.LastTrack; nTrack--)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            LOG(_T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        cdpamsf.StartingF = cdToc.TrackData[nTrack-1].Address[3];
        cdpamsf.StartingS = cdToc.TrackData[nTrack-1].Address[2];
        cdpamsf.StartingM = cdToc.TrackData[nTrack-1].Address[1];

        // end address of track
        cdpamsf.EndingF = cdToc.TrackData[nTrack].Address[3];
        cdpamsf.EndingS = cdToc.TrackData[nTrack].Address[2];
        cdpamsf.EndingM = cdToc.TrackData[nTrack].Address[1];

        LOG(_T("TRACK %2u -- PLAYING %02u:%02u:%02u to  %02u:%02u:%02u"), nTrack, 
            cdpamsf.StartingM, cdpamsf.StartingS, cdpamsf.StartingF,
            cdpamsf.EndingM, cdpamsf.EndingS, cdpamsf.EndingF);

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_PLAY_AUDIO_MSF, &cdpamsf, sizeof(cdpamsf), NULL, 0, NULL, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_PLAY_AUDIO_MSF)");
            goto done;
        }

        memset(&sqcdLast, 0, sizeof(SUB_Q_CHANNEL_DATA));
        
        tStart = GetTickCount();
        for(;;)
        {
            LOG(_T("allowing track to play for %u ms..."), tSleep / 10);
            Sleep(tSleep / 10);

            sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
            
            LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
                &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
                goto done;
            }

            DumpPosition(&sqcdNew.CurrentPosition);

            if(AUDIO_STATUS_IN_PROGRESS != sqcdNew.CurrentPosition.Header.AudioStatus)
            {
                if(AUDIO_STATUS_PLAY_COMPLETE == sqcdNew.CurrentPosition.Header.AudioStatus)
                {
                    LOG(_T("Track finished playing before timeout"));
                    break;
                }

                FAIL("CurrentPosition.Header.AudioStatus != AUDIO_STATUS_IN_PROGRESS");
            }

            // make sure the absolute address is progressing
            memcpy(&dwAddr1, sqcdNew.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            memcpy(&dwAddr2, sqcdLast.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            if(BEDW_TO_DW(dwAddr1) <= BEDW_TO_DW(dwAddr2))
            {
                FAIL("CurrentPosition.AbsoluteAddress went back in time");
            }

            // make sure we are playing the right track
            if(sqcdNew.CurrentPosition.TrackNumber != nTrack)
            {
                FAIL("CurrentPosition.TrackNumber is incorrect");
            }

            // save a copy of the data for comparison next time
            memcpy(&sqcdLast, &sqcdNew, sizeof(SUB_Q_CHANNEL_DATA));

            if(GetTickCount() - tStart > tSleep)
            {
                break;
            }
        }

        // every other track, stop before playing
        if(0 == (nTrack % 2))
        {
            LOG(_T("stopping playback (IOCTL_CDROM_STOP_AUDIO)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, NULL, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_STOP_AUDIO)");
            }
        }

    }
    
done:
    DeviceIoControl(g_hDisk, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, NULL, NULL);
    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PlayAndPauseTracksInOrder(
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

    DWORD nTrack, cBytes, tStart, tSleep = lpFTE->dwUserData;
    DWORD dwAddr1, dwAddr2;
    CDROM_PLAY_AUDIO_MSF cdpamsf = {0};    
    CDROM_TOC cdToc = {0};

    CDROM_SUB_Q_DATA_FORMAT sqdf = {0};
    SUB_Q_CHANNEL_DATA sqcdNew = {0};
    SUB_Q_CHANNEL_DATA sqcdLast = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    // play each track
    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            LOG(_T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        cdpamsf.StartingF = cdToc.TrackData[nTrack-1].Address[3];
        cdpamsf.StartingS = cdToc.TrackData[nTrack-1].Address[2];
        cdpamsf.StartingM = cdToc.TrackData[nTrack-1].Address[1];

        // end address of track
        cdpamsf.EndingF = cdToc.TrackData[nTrack].Address[3];
        cdpamsf.EndingS = cdToc.TrackData[nTrack].Address[2];
        cdpamsf.EndingM = cdToc.TrackData[nTrack].Address[1];

        LOG(_T("TRACK %2u -- PLAYING %02u:%02u:%02u to  %02u:%02u:%02u"), nTrack, 
            cdpamsf.StartingM, cdpamsf.StartingS, cdpamsf.StartingF,
            cdpamsf.EndingM, cdpamsf.EndingS, cdpamsf.EndingF);

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_PLAY_AUDIO_MSF, &cdpamsf, sizeof(cdpamsf), NULL, 0, NULL, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_PLAY_AUDIO_MSF)");
            goto done;
        }

        memset(&sqcdLast, 0, sizeof(SUB_Q_CHANNEL_DATA));
        
        tStart = GetTickCount();
        for(;;)
        {
            LOG(_T("allowing track to play for %u ms..."), tSleep / 10);
            Sleep(tSleep / 10);

            sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
            
            LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
                &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
                goto done;
            }

            DumpPosition(&sqcdNew.CurrentPosition);

            if(AUDIO_STATUS_IN_PROGRESS != sqcdNew.CurrentPosition.Header.AudioStatus)
            {
                if(AUDIO_STATUS_PLAY_COMPLETE == sqcdNew.CurrentPosition.Header.AudioStatus)
                {
                    LOG(_T("Track finished playing before timeout"));
                    break;
                }

                FAIL("CurrentPosition.Header.AudioStatus != AUDIO_STATUS_IN_PROGRESS");
            }            

            LOG(_T("pausing audio (IOCTL_CDROM_PAUSE_AUDIO)..."), tSleep / 10);
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, NULL, 0, NULL, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_PAUSE_AUDIO)");
                continue;
            }
            
            LOG(_T("allowing track to remain paused for %u ms..."), tSleep / 10);
            Sleep(tSleep / 10);

            sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
            
            LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
                &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
                goto done;
            }

            DumpPosition(&sqcdNew.CurrentPosition);

            if(AUDIO_STATUS_PAUSED != sqcdNew.CurrentPosition.Header.AudioStatus)
            {
                FAIL("CurrentPosition.Header.AudioStatus != AUDIO_STATUS_PAUSED");
            }

            // make sure the absolute address is progressing
            memcpy(&dwAddr1, sqcdNew.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            memcpy(&dwAddr2, sqcdLast.CurrentPosition.AbsoluteAddress, sizeof(DWORD));
            if(BEDW_TO_DW(dwAddr1) <= BEDW_TO_DW(dwAddr2))
            {
                FAIL("CurrentPosition.AbsoluteAddress went back in time");
            }

            // make sure we are playing the right track
            if(sqcdNew.CurrentPosition.TrackNumber != nTrack)
            {
                FAIL("CurrentPosition.TrackNumber is incorrect");
            }

            // save a copy of the data for comparison next time
            memcpy(&sqcdLast, &sqcdNew, sizeof(SUB_Q_CHANNEL_DATA));

            LOG(_T("resuming audio (IOCTL_CDROM_RESUME_AUDIO)..."), tSleep / 10);
            if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_RESUME_AUDIO, NULL, 0, NULL, 0, NULL, NULL))
            {
                ERRFAIL("DeviceIoControl(IOCTL_CDROM_RESUME_AUDIO)");
                continue;
            }

            if(GetTickCount() - tStart > tSleep)
            {
                break;
            }
        }

        

    }
    
done:
    DeviceIoControl(g_hDisk, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, NULL, NULL);
    return GetTestResult();
}



// --------------------------------------------------------------------
TESTPROCAPI 
Tst_SeekTrackInOrder(
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

    DWORD nTrack, cBytes, tSleep = lpFTE->dwUserData;
    CDROM_SEEK_AUDIO_MSF cdsamsf = {0};
    CDROM_TOC cdToc = {0};

    CDROM_SUB_Q_DATA_FORMAT sqdf = {0};
    SUB_Q_CHANNEL_DATA sqcdNew = {0};
    SUB_Q_CHANNEL_DATA sqcdLast = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    for(nTrack = cdToc.FirstTrack; nTrack <= cdToc.LastTrack; nTrack++)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            LOG(_T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        cdsamsf.F = cdToc.TrackData[nTrack-1].Address[3];
        cdsamsf.S = cdToc.TrackData[nTrack-1].Address[2];
        cdsamsf.M = cdToc.TrackData[nTrack-1].Address[1];
       

        LOG(_T("TRACK %2u -- SEEKING %02u:%02u:%02u"), nTrack, 
            cdsamsf.M, cdsamsf.S, cdsamsf.F);

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_SEEK_AUDIO_MSF, &cdsamsf, sizeof(cdsamsf), NULL, 0, NULL, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_SEEK_AUDIO_MSF)");
            goto done;
        }

        sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
        
        LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
            &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
            goto done;
        }
        
        DumpPosition(&sqcdNew.CurrentPosition);        
    }
    
done:
    return GetTestResult();
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_SeekTrackReverseOrder(
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

    DWORD nTrack, cBytes, tSleep = lpFTE->dwUserData;
    CDROM_SEEK_AUDIO_MSF cdsamsf = {0};
    CDROM_TOC cdToc = {0};

    CDROM_SUB_Q_DATA_FORMAT sqdf = {0};
    SUB_Q_CHANNEL_DATA sqcdNew = {0};
    SUB_Q_CHANNEL_DATA sqcdLast = {0};

    // validate the global disk handle
    if(INVALID_HANDLE_VALUE == g_hDisk)
    {
        SKIP("Global CDRom Disk handle is invalid");
        goto done;
    }
    
    // query a CD table of contents
    if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cBytes, NULL))
    {
        ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_TOC)");
        goto done;
    }

    for(nTrack = cdToc.LastTrack; nTrack >= cdToc.FirstTrack && nTrack <= cdToc.LastTrack; nTrack--)
    {
        // make sure it is an audio track
        if(cdToc.TrackData[nTrack-1].Control & 0x4)
        {
            LOG(_T("Track %u is not an audio track"), nTrack);
            continue;
        }

        // start address of track
        cdsamsf.F = cdToc.TrackData[nTrack-1].Address[3];
        cdsamsf.S = cdToc.TrackData[nTrack-1].Address[2];
        cdsamsf.M = cdToc.TrackData[nTrack-1].Address[1];
       

        LOG(_T("TRACK %2u -- SEEKING %02u:%02u:%02u"), nTrack, 
            cdsamsf.M, cdsamsf.S, cdsamsf.F);

        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_SEEK_AUDIO_MSF, &cdsamsf, sizeof(cdsamsf), NULL, 0, NULL, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_SEEK_AUDIO_MSF)");
            goto done;
        }

        sqdf.Format = IOCTL_CDROM_CURRENT_POSITION;
        
        LOG(_T("checking current position (IOCTL_CDROM_READ_Q_CHANNEL::IOCTL_CDROM_CURRENT_POSITION)..."));
        if(!DeviceIoControl(g_hDisk, IOCTL_CDROM_READ_Q_CHANNEL, &sqdf, sizeof(CDROM_SUB_Q_DATA_FORMAT), 
            &sqcdNew, sizeof(SUB_Q_CURRENT_POSITION), &cBytes, NULL))
        {
            ERRFAIL("DeviceIoControl(IOCTL_CDROM_READ_Q_CHANNEL, IOCTL_CDROM_CURRENT_POSITION)");
            goto done;
        }
        
        DumpPosition(&sqcdNew.CurrentPosition);
    }
    
done:
    return GetTestResult();
}




