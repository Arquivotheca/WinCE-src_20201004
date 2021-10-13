//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#include "udfs.h"
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
void CReadOnlyFileSystemDriver::SaveRootName( PCHAR pName,BOOL UniCode )
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
 
void CReadOnlyFileSystemDriver::DeregisterVolume(PUDFSDRIVER pVol)
{
    TCHAR szName[MAX_PATH];
    DEBUGMSG(ZONE_INIT,(TEXT("UDFS: DeregisterVolume enetered pVol = %x\r\n"),pVol)); 

    if (m_hTestUnitThread && m_hWakeUpEvent) {
        // TODO: Need something better here
        SetEvent( m_hWakeUpEvent);
        WaitForSingleObject( m_hTestUnitThread, 15000);
    }    
    if (pVol->m_hVolume) {
        FSDMGR_GetVolumeName( pVol->m_hVolume, szName, MAX_PATH);
        switch( pVol->m_bFileSystemType) {
            case FILE_SYSTEM_TYPE_UDFS: 
                FSDMGR_AdvertiseInterface( &UDFS_MOUNT_GUID,  szName, FALSE);
                break;
            case FILE_SYSTEM_TYPE_CDFS:
                FSDMGR_AdvertiseInterface( &CDFS_MOUNT_GUID,  szName, FALSE);
                break;
            case FILE_SYSTEM_TYPE_CDDA:
                FSDMGR_AdvertiseInterface( &CDDA_MOUNT_GUID,  szName, FALSE);
                break;
        }        
    }    
    //
    // Delete Previously mounted File System
    //
    if (m_pFileSystem)
    {       
        delete m_pFileSystem;
        m_pFileSystem = NULL;
    }
    //
    //  Unmount Volume : Deregister Volume and Volume Name.
    //
    ::FSDMGR_DeregisterVolume(pVol->m_hVolume);
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

    if (m_hHeap && UDFSDeviceIoControl( IOCTL_CDROM_DISC_INFO, &cdDiscInfo, sizeof(cdDiscInfo), &cdDiscInfo, sizeof(cdDiscInfo), &dwAvail, NULL)) {
        if (m_bFileSystemType = CFileSystem::DetectCreateAndInit( this, &m_RootDirectoryPointer, &m_pFileSystem)) {
            if (RegisterVolume(this)) {
                FSDMGR_GetVolumeName( m_hVolume, m_szMountName, MAX_PATH);
                if (m_bFileSystemType) {
                    m_bMounted = TRUE;
                    switch(m_bFileSystemType) {
                        case FILE_SYSTEM_TYPE_UDFS: 
                            memcpy(&m_MountGuid, &UDFS_MOUNT_GUID, sizeof(GUID));
                            break;
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
        wcscpy( m_szMountName, L"");
        memset( &m_MountGuid, 0, sizeof(GUID));

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
    BOOL            fRet;
    if (fRet = CCDDAFileSystem::DetectCreateAndInit(pDrv, pRootDirectoryPointer, ppNewFS)) { // First try to detect if it is CD Audio 
        return FILE_SYSTEM_TYPE_CDDA;
    } else {    
        if (fRet = CUDFSFileSystem::DetectCreateAndInit(pDrv, pRootDirectoryPointer, ppNewFS)) { // Second try to see if it is a DVD
            return FILE_SYSTEM_TYPE_UDFS;
        } else {
            if (fRet = CCDFSFileSystem::DetectCreateAndInit(pDrv, pRootDirectoryPointer, ppNewFS)) { // Third see if it is a CD-ROM
                    return FILE_SYSTEM_TYPE_CDFS;
            } 
        } 
    }    
    return 0;
}



