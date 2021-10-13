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
//  File:       udfsinit.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udfs.h"
#include <diskio.h>

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::CReadOnlyFileSystemDriver
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

extern PUDFSDRIVER g_pHeadFSD;

DWORD TestUnitThread(LPVOID lParam)
{
    PUDFSDRIVER pVol = (PUDFSDRIVER)lParam;

    pVol->MediaCheckInside(pVol);

    return 0;
}



//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::MediaChanged
//
//  Synopsis:   This method is called by Atapi Device Driver upon Media change 
//              detection (Load/Unload).
//
//  Arguments:  [pVol] --
//              [bUnitReady] - TRUE if Unit Ready; FALSE otherwise.
//
//  Returns:
//
//  Notes:   This method eliminates TestUnitReadyTest thread. 
//
//--------------------------------------------------------------------------


void CReadOnlyFileSystemDriver::MediaChanged(CReadOnlyFileSystemDriver* pVol, BOOL bUnitReady )
{       
    EnterCriticalSection(&pVol->m_csListAccess);
    //
    //  If Unit not ready and the volume is registered: 
    //   - Clear Cash and Deregister Volume.
    //
    if (!(bUnitReady) && (pVol->m_pFileSystem))
    {
        DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS:MediaChanged: DeregisterVolume pVol = %x\r\n"),pVol));

            //  Mark All Disk Handles as dirty!!!

        if (InterlockedTestExchange(
                &(pVol->m_State),
                StateClean,
                StateDirty) == StateClean)
        {   
            //  Clean deletes File Object and set m_pFileSystem to NULL
            pVol->Clean();

        }
        pVol->DeregisterVolume(pVol);
    }

    // Mount Volume if Unit is ready again.
    
    if ((pVol->m_RootDirectoryPointer.cbSize == 0) && (bUnitReady))
    {
        //  Try to mount new file.
        //  Media is not ready if Mount failed.
            DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS:MediaChanged: RegisterVolume  pVol = %x\r\n"),pVol));

        if (pVol->Mount())
            SetLastError(NO_ERROR);
    }

    LeaveCriticalSection(&pVol->m_csListAccess);
    
}
 
//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::MediaCheckInside
//
//  Synopsis:
//
//  Arguments:  (none)
//
//  Returns:
//
//--------------------------------------------------------------------------

void CReadOnlyFileSystemDriver::MediaCheckInside(CReadOnlyFileSystemDriver* pVol)
{
    CDROM_TESTUNITREADY     tur;
    DWORD                   wAvail;
    BOOL                    fRet;
   // DWORD                   dwErr;
    BOOL                    bRemounted = FALSE;

    for (;;) {
        
        DEBUGMSG(ZONE_MEDIA,(TEXT("CDFS: MediaCheckInside = %x\r\n"),pVol));
      

        fRet = UDFSDeviceIoControl( IOCTL_CDROM_TEST_UNIT_READY, NULL, 0, (PVOID)&tur, sizeof(tur), &wAvail, NULL);

        if (!fRet || !tur.bUnitReady) {
            
            DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS:MediaCheckInside: Cleaning Volume!!pVol = %x\r\n"),pVol));
   
            if (bRemounted) {
                bRemounted = FALSE;
                RegisterVolume( pVol, FALSE);
            }
            //  Clean deletes File Object and set m_pFileSystem to NULL

            if (InterlockedTestExchange( &(m_State), StateClean, StateDirty) == StateClean) {
                //
                //  Mark All Disk Handles as dirty!!!
                //

                EnterCriticalSection(&pVol->m_csListAccess);
                Clean();
                LeaveCriticalSection(&pVol->m_csListAccess);
            }

            EnterCriticalSection(&pVol->m_csListAccess);
            Unmount();
            LeaveCriticalSection(&pVol->m_csListAccess);
        } else {
            if (fRet && tur.bUnitReady && !bRemounted) {
                bRemounted = TRUE;
                
                // Check to see if we have already been mounted
                if ((pVol->m_RootDirectoryPointer.cbSize == 0)) {
                    EnterCriticalSection(&pVol->m_csListAccess);
                    Mount();
                    LeaveCriticalSection(&pVol->m_csListAccess);
                }
            }
        }    

        DWORD dwTimeOut = WaitForSingleObject( m_hWakeUpEvent, 5000);
        if (dwTimeOut != WAIT_TIMEOUT)  // We where signaled to exit !!!
            break;
    }
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::Initialize
//
//  Synopsis:
//
//  Arguments:  [hDsk] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

#define MAX_DEVICE_NAME  32

BOOL CReadOnlyFileSystemDriver::Initialize(HDSK hDsk)
{
    BOOL            fSuccess = TRUE;
    BOOL            fRegistered;
//    HANDLE          hThread;
    DWORD           wAvail; 

    // First open the driver

    m_hDsk = hDsk;
    
    if (fSuccess = UDFSDeviceIoControl(DISK_IOCTL_GETNAME, NULL, 0, (LPVOID)m_szAFSName, MAX_DEVICE_NAME * sizeof(WCHAR), &wAvail, NULL)) { 
        if (!FSDMGR_GetRegistryValue(m_hDsk, L"MountLabel", (LPDWORD)&m_fRegisterLabel)) {
            m_fRegisterLabel= 0;
        }   
        m_hVolume = ::FSDMGR_RegisterVolume(m_hDsk, m_szAFSName, (PVOLUME)this);

        if (!m_hVolume) {
            goto error;
        }    

        fRegistered = TRUE;
        
        //
        //  Register MediaChange Callback Function by Atapi Driver.
        //

#if 0
        CALLBACKINFO    cbi; 
        
        cbi.hProc = (HANDLE) GetCurrentProcess();
        cbi.pfn = (FARPROC)CReadOnlyFileSystemDriver::MediaChanged;
        cbi.pvArg0 = (void *)this;
        
        fSuccess = UDFSDeviceIoControl(
                    IOCTL_CDROM_TEST_UNIT_READY,
                    NULL,
                    0,
                    (LPVOID)NULL,
                    0,
                    &wAvail,
                    NULL);
#else
        DWORD dwThreadId;
        DWORD dwThreadPri;
        m_hWakeUpEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
        m_hTestUnitThread = ::CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)TestUnitThread, this, 0, &dwThreadId);
        fSuccess = (m_hTestUnitThread != NULL) && (m_hWakeUpEvent != NULL);
        if (fSuccess && FSDMGR_GetRegistryValue(m_hDsk, L"TestUnitThreadPrio256", &dwThreadPri)) {
            ::CeSetThreadPriority (m_hTestUnitThread, dwThreadPri);
        }   

#endif                  
        
    } else {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!cdfsInit: RegisterAFSName failed (%d)\n"), GetLastError()));
        fRegistered = FALSE;
    }
error:
    if( !fSuccess) {
        if( fRegistered) {
            ::FSDMGR_DeregisterVolume(m_hVolume);
        }

       // delete m_pCache;
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT(" Initialize Done\r\n")));

    return fSuccess;
}

