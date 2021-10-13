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
//  File:       udfsmnt.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"
#include "storemgr.h"


//+-------------------------------------------------------------------------
//
//  Member:     CCDFSFileSystem::SaveRootName
//
//  Synopsis:   Save Root/Volume name into Driver object used
//              by Volume mounting
//           
//
//  Arguments:  [pCache]                --
//              [pRootDirectoryPointer] --
//              [ppNewFS]               --
//
//  Returns:    BOOL
//
//  Notes:
//
//--------------------------------------------------------------------------
void CReadOnlyFileSystemDriver::SaveRootName( const char* pName, BOOL UniCode )
{
    int     i;

    if (UniCode )   {   // UNICODE string only 15 chars
        // TODO: ?? BIG ENDIAN ??
        for( i = 0; i < 32 ; i +=2 ) {
            ((PUCHAR)m_RootName)[i+1]   = pName[i];
            ((PUCHAR)m_RootName)[i] = pName[i+1];
        }
        i = 16;// WCHAR count
    } else { // ANSI
            // others ????? treated as ANSI
            for( i = 0; i< 32 ; i ++ ){
                m_RootName[i] = pName[i];
            }
            i = 32;//WCHAR count
    }
    // clear spaces in tail
    while( i-- ){
        if( m_RootName[i] == L' ' )
            m_RootName[i] = 0;
        else
            break;
    }
}



BOOL CReadOnlyFileSystemDriver::RegisterVolume(PUDFSDRIVER pVol, BOOL bMountLabel)
{
    DEBUGMSG(ZONE_INIT,(TEXT("UDFS: RegisterVolume enetered pVol = %x\r\n"),pVol)); 
    if (pVol->m_fRegisterLabel) {
        if (bMountLabel) {
            pVol->m_hVolume = ::FSDMGR_RegisterVolume(pVol->m_hDsk, &pVol->m_RootName[0], (PVOLUME)pVol);
        } else {
            pVol->m_hVolume = ::FSDMGR_RegisterVolume(pVol->m_hDsk, pVol->m_szAFSName, (PVOLUME)pVol);
        }   
    }   
    return (pVol->m_hVolume != NULL);
}
 
//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::Mount
//
//  Synopsis:
//
//  Arguments:  (none)
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::Mount()
{
    DWORD           dwAvail;
    CDROM_DISCINFO  cdDiscInfo;

  //  DEBUGCHK(m_RootDirectoryPointer.cbSize == 0);

    // We only want to call this function once
    if(m_hMounting) {
        WaitForSingleObject(m_hMounting, 5000);

        if(m_RootDirectoryPointer.cbSize) {
            // Already mounted
            ReleaseMutex(m_hMounting);
            return TRUE;
        }
    }

    memset(&cdDiscInfo, 0, sizeof(cdDiscInfo));

    if(g_bDestroyOnUnmount) {
        m_hHeap = HeapCreate(0, 1024 * g_uHeapInitSize, 1024 * g_uHeapMaxSize);
    }
    else {
        m_hHeap = g_hHeap;
    }

    if (m_hHeap && UDFSDeviceIoControl( IOCTL_CDROM_DISC_INFO, NULL, 0, &cdDiscInfo, sizeof(cdDiscInfo), &dwAvail, NULL)) {
        if (m_bFileSystemType = CFileSystem::DetectCreateAndInit( this, &m_RootDirectoryPointer, &m_pFileSystem)) {
            if (RegisterVolume(this)) {
                FSDMGR_GetVolumeName( m_hVolume, m_szMountName, MAX_PATH);
                if (m_bFileSystemType) {
                    m_bMounted = TRUE;
                    switch(m_bFileSystemType) {
                        case FILE_SYSTEM_TYPE_CDFS:
                            memcpy(&m_MountGuid, &CDFS_MOUNT_GUID, sizeof(GUID));
                            break;
                        case FILE_SYSTEM_TYPE_CDDA:
                            memcpy(&m_MountGuid, &CDDA_MOUNT_GUID, sizeof(GUID));
                            break;
                    }
                    FSDMGR_AdvertiseInterface( &m_MountGuid, m_szMountName, TRUE);
                }    
                m_State = StateClean;          

                if(m_hMounting) {
                    ReleaseMutex(m_hMounting);
                }
                return (TRUE);
            }
        }
    }

    if(m_hMounting) {
        ReleaseMutex(m_hMounting);
    }

    return FALSE;

}

BOOL CReadOnlyFileSystemDriver::Unmount()
{
    if (m_bMounted) {
        FSDMGR_AdvertiseInterface( &m_MountGuid, m_szMountName, FALSE);
        
        ZeroMemory( m_szMountName, sizeof(m_szMountName) );
        ZeroMemory( &m_MountGuid, sizeof(GUID) );

        if(m_pFileSystem) {
            delete m_pFileSystem;
            m_pFileSystem = NULL;
        }

        if(g_bDestroyOnUnmount && m_hHeap) {
            HeapDestroy( m_hHeap);
            m_hHeap = NULL;
            m_State = StateClean;
            m_pFileHandleList = NULL;
            m_cFindHandleListSize = 0;
            m_ppFindHandles = NULL;
        }

        m_bMounted = FALSE;
    }    
    return(TRUE);
}


//+-------------------------------------------------------------------------
//
//  Member:     CFileSystem::DetectCreateAndInit
//
//  Synopsis:   Tries to create the various file systems
//
//  Arguments:  [pCache]                --
//              [pRootDirectoryPointer] --
//              [ppNewFS]               --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BYTE CFileSystem::DetectCreateAndInit(PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem **ppNewFS)
{
    BYTE bReturn = 0;
    BOOL fRet;
    PCD_PARTITION_INFO pPartInfo = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBytesReturned = 0;

    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * sizeof(TRACK_INFORMATION2);
    
    pPartInfo = (PCD_PARTITION_INFO)new BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_detectcreateandinit;
    }

    if( !pDrv->UDFSDeviceIoControl( IOCTL_CDROM_GET_PARTITION_INFO,
                                    NULL,
                                    0,
                                    pPartInfo,
                                    dwBufferSize,
                                    &dwBytesReturned,
                                    NULL ) )
    {
        goto exit_detectcreateandinit;
    }
    
    if (fRet = CCDDAFileSystem::DetectCreateAndInit(pDrv, pRootDirectoryPointer, ppNewFS, pPartInfo)) { // First try to detect if it is CD Audio 
        pPartInfo = NULL;
        bReturn = FILE_SYSTEM_TYPE_CDDA;
    } else {    
        if (fRet = CCDFSFileSystem::DetectCreateAndInit(pDrv, pRootDirectoryPointer, ppNewFS, pPartInfo)) { // Third see if it is a CD-ROM
            pPartInfo = NULL;
            bReturn = FILE_SYSTEM_TYPE_CDFS;
        } 
    }

exit_detectcreateandinit:
    if( pPartInfo && !bReturn )
    {
        delete [] pPartInfo;
    }
    
    return bReturn;
}



