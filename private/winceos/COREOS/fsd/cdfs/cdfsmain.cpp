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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsmain.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"


#define ARRAYSIZE(a)            (sizeof(a)/sizeof(a[0]))
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#define REGSTR_PATH_UDFS        L"System\\StorageManager\\UDFS"

PUDFSDRIVERLIST g_pHeadFSD = NULL;          
CRITICAL_SECTION g_csMain;
HANDLE g_hHeap;
const SIZE_T g_uHeapInitSize = 20;    // Initial size of the heap in KB.
SIZE_T g_uHeapMaxSize = 0;    // Maximum size of the heap in KB. The default is unbounded.
BOOL g_bDestroyOnUnmount = FALSE;

//
// The following RIFF information is pulled from the desktop.
//

//  Audio Philes begin with this header block to identify the data as a
//  PCM waveform.  AudioPhileHeader is coded as if it has no data included
//  in the waveform.  Data must be added in 2352-byte multiples.
//
//  Fields marked 'ADJUST' need to be adjusted based on the size of the
//  data: Add (nSectors*2352) to the DWORDs at offsets 1*4 and 10*4.
//
//  File Size of TRACK??.WAV = nSectors*2352 + sizeof(AudioPhileHeader)
//  RIFF('WAVE' fmt(1, 2, 44100, 176400, 16, 4) data( <CD Audio Raw Data> )
//
//  The number of sectors in a CD-XA CD-DA file is (DataLen/2048).
//  CDFS will expose these files to applications as if they were just
//  'WAVE' files, adjusting the file size so that the RIFF file is valid.
//

LONG CdXAAudioPhileHeader[] = {
    0x46464952,                         // Chunk ID = 'RIFF'
    -8,                                 // Chunk Size = (file size - 8) ADJUST1
    0x45564157,                         // 'WAVE'
    0x20746d66,                         // 'fmt '
    16,                                 // Chunk Size (of 'fmt ' subchunk) = 16
    0x00020001,                         // WORD Format Tag WORD nChannels
    44100,                              // DWORD nSamplesPerSecond
    2352 * 75,                          // DWORD nAvgBytesPerSec
    0x00100004,                         // WORD nBlockAlign WORD nBitsPerSample
    0x61746164,                         // 'data'
    -44                                 // <CD Audio Raw Data>          ADJUST2
};

//
//  XA Files begin with this RIFF header block to identify the data as
//  raw CD-XA sectors.  Data must be added in 2352-byte multiples.
//
//  This header is added to all CD-XA files which are marked as having
//  mode2form2 sectors.
//
//  Fields marked 'ADJUST' need to be adjusted based on the size of the
//  data: Add file size to the marked DWORDS.
//
//  File Size of TRACK??.WAV = nSectors*2352 + sizeof(XAFileHeader)
//
//  RIFF('CDXA' FMT(Owner, Attr, 'X', 'A', FileNum, 0) data ( <CDXA Raw Data> )
//

LONG CdXAFileHeader[] = {
    0x46464952,                         // Chunk ID = 'RIFF'
    -8,                                 // Chunk Size = (file size - 8) ADJUST
    0x41584443,                         // 'CDXA'
    0x20746d66,                         // 'fmt '
    16,                                 // Chunk Size (of CDXA chunk) = 16
    0,                                  // DWORD Owner ID
    0x41580000,                         // WORD Attributes
                                        // BYTE Signature byte 1 'X'
                                        // BYTE Signature byte 2 'A'
    0,                                  // BYTE File Number
    0,                                  // BYTE Reserved[7]
    0x61746164,                         // 'data'
    -44                                 // <CD-XA Raw Sectors>          ADJUST
};

//+-------------------------------------------------------------------------
//
//  Function:   InitGlobals
//
//  Synopsis:
//
//  Arguments:  None
//
//  Returns: void
//
//
//  Notes:
//
//--------------------------------------------------------------------------

void InitGlobals(void)
{
    DWORD dwError;
    DWORD dwSize;
    DWORD dwType;
    DWORD dwData;
    HKEY hKey;

    dwError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGSTR_PATH_UDFS, 0, 0, &hKey);

    if(dwError == ERROR_SUCCESS)
    {
        dwSize = sizeof(DWORD);
        dwError = RegQueryValueEx(hKey, L"HeapMaxSize", NULL, &dwType, (LPBYTE)&dwData, &dwSize);

        if(dwError == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            // Reserve at least as many pages as we initially commit.
            g_uHeapMaxSize = dwData == 0 ? 0 : max(dwData, g_uHeapInitSize);
        }

        dwSize = sizeof(DWORD);
        dwError = RegQueryValueEx(hKey, L"DestroyHeapOnUnmount", NULL, &dwType, (LPBYTE)&dwData, &dwSize);

        if(dwError == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            g_bDestroyOnUnmount = dwData == 1 ? TRUE : FALSE;
        }

        RegCloseKey(hKey);
    }

    return;
}


//+-------------------------------------------------------------------------
//
//  Function:   DllMain
//
//  Synopsis:
//
//  Arguments:  [DllInstance] --
//              [Reason]      --
//              [Reserved]    --
//
//  Returns:
//
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL WINAPI DllMain(HANDLE DllInstance, DWORD Reason, LPVOID Reserved)
{
    switch(Reason) {
        case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls( (HMODULE)DllInstance);
            DEBUGREGISTER((HINSTANCE) DllInstance);

#if DEBUG
            if (ZONE_INITBREAK) {
                DebugBreak();
            }
#endif
            
            InitGlobals();

            if(!g_bDestroyOnUnmount) {
                g_hHeap = HeapCreate(0, 1024 * g_uHeapInitSize, 1024 * g_uHeapMaxSize);
            }

            InitializeCriticalSection(&g_csMain);

            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_ATTACH\n")));
            return TRUE;

        case DLL_PROCESS_DETACH:
            DeleteCriticalSection(&g_csMain);

            if(!g_bDestroyOnUnmount) {
                HeapDestroy( g_hHeap);
            }

            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_DETACH\n")));
            return TRUE;

        default:
            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d ignored\n"), Reason));
            return TRUE;
    }

    DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d failed\n"), Reason));
    
    return FALSE;
}


//---------------------------------------------------------------------------
//
//  Function:   FSD_MountDisk
//
//  Synopsis:   Initialization service called by FSDMGR.DLL
//
//  Arguments:  
//      hdsk == FSDMGR disk handle, or NULL to deinit all
//      frozen volumes on *any* disk that no longer has any open
//      files or dirty buffers.
//
//  Returns:    BOOL   
//
//  Notes:
//
//---------------------------------------------------------------------------

BOOL FSD_MountDisk(HDSK hDsk)
{
    PUDFSDRIVER pUDFS;
    BOOL fRet;

    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!MountDisk hDsk=%08X\r\n"), hDsk));
    //
    //  Create the volume class
    //

    pUDFS = new CReadOnlyFileSystemDriver();

    if (pUDFS == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    
    fRet = pUDFS->Initialize(hDsk);

    if (fRet) {
        EnterCriticalSection( &g_csMain);
        UDFSDRIVERLIST *pUDFSEntry = new UDFSDRIVERLIST;
        if (pUDFSEntry) {
            pUDFSEntry->hDsk = hDsk;
            pUDFSEntry->pUDFS = pUDFS;
            pUDFSEntry->pUDFSNext  = g_pHeadFSD;
            g_pHeadFSD = pUDFSEntry;
            fRet = pUDFS->Mount();
        } else {
            DEBUGCHK(0);
        }    
        LeaveCriticalSection( &g_csMain);
    } 

    if (!fRet) {
        delete pUDFS;
    }
    return fRet;
}


//---------------------------------------------------------------------------
//
//  Function:   FSD_UnmountDisk
//
//  Synopsis:   Deinitialization service called by FSDMGR.DLL
//
//  Arguments:  
//      hdsk == FSDMGR disk handle, or NULL to deinit all
//      frozen volumes on *any* disk that no longer has any open
//      files or dirty buffers.
//
//  Returns:    BOOL   
//
//  Notes:
//
//---------------------------------------------------------------------------

BOOL FSD_UnmountDisk(HDSK hDsk)
{
    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!UnmountDisk hDsk=%08X\r\n"), hDsk));
    EnterCriticalSection(&g_csMain);
    PUDFSDRIVERLIST pUDFSEntry = g_pHeadFSD;
    PUDFSDRIVERLIST pUDFSPrev = NULL;
    while( pUDFSEntry) {
        if (pUDFSEntry->hDsk == hDsk) {
            if (pUDFSPrev) {
                pUDFSPrev->pUDFSNext = pUDFSEntry->pUDFSNext;
            } else {
                g_pHeadFSD = pUDFSEntry->pUDFSNext;
            }
            break;
        }
        pUDFSPrev = pUDFSEntry;
        pUDFSEntry = pUDFSEntry->pUDFSNext;
    }
    LeaveCriticalSection(&g_csMain);
    if (pUDFSEntry) {
        if(pUDFSEntry->pUDFS) {
            //pUDFSEntry->pUDFS->DeregisterVolume(pUDFSEntry->pUDFS);
            pUDFSEntry->pUDFS->Unmount();
            delete pUDFSEntry->pUDFS;
            pUDFSEntry->pUDFS = NULL;
        }
        delete pUDFSEntry;
        return TRUE;
    }

    return FALSE;
}

//---------------------------------------------------------------------------
//
//  Function:   ROFS_RecognizeVolume
//
//  Synopsis:   Called by the file manager to see if the file system should
//              mount this volume.
//
//  Arguments:  
//      hDisk == FSDMGR disk handle
//      pGuid == The file system to check - in case there is one DLL to check
//               for multiple file systems.  This is not used by this file
//               system.
//
//  Returns:    BOOL   
//
//  Notes:
//       For ISO 9660 we will check the VRS.
//       For CD Audio we will check the session type and track information.
//       For Video CD we will check the track information.
//
//---------------------------------------------------------------------------

BOOL ROFS_RecognizeVolume( HANDLE hDisk, 
                           const GUID* pGuid, 
                           const BYTE* pBootSector, 
                           DWORD dwSectorSize )
{
    BOOL fResult = FALSE;
    
    //
    // First we will check for a CD Audio disc.
    //
    PCD_PARTITION_INFO pPartInfo = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBytesReturned = 0;

    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * sizeof(TRACK_INFORMATION2);
    
    pPartInfo = (PCD_PARTITION_INFO)new BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    if( !FSDMGR_DiskIoControl( (DWORD)hDisk,
                               IOCTL_CDROM_GET_PARTITION_INFO,
                               NULL,
                               0,
                               pPartInfo,
                               dwBufferSize,
                               &dwBytesReturned,
                               NULL ) )
    {
        fResult = FALSE;
        goto exit_recognizevolume;
    }

    if( pPartInfo->fAudio )
    {
        fResult = TRUE;
        goto exit_recognizevolume;
    }

    //
    // Not CD Audio - try CDFS (ISO9660/JOLIET)
    //
    DIRECTORY_ENTRY DirEntry;
    BOOL fUnicode = FALSE;
    BOOL fCDXA = FALSE;
    
    if( CCDFSFileSystem::Recognize( (DWORD)hDisk,
                                    &DirEntry, 
                                    &fUnicode,
                                    &fCDXA,
                                    NULL, 
                                    0 ) )
    {
        fResult = TRUE;
        goto exit_recognizevolume;
    }

exit_recognizevolume:
    if( !fResult && pPartInfo )
    {
        delete [] pPartInfo;
    }
    
    return fResult;
}

