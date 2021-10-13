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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:    FALMAIN.CPP

Abstract:       FLASH Abstraction Layer (FAL) for Windows CE 

Notes:          The following stream interface driver is used to support FLASH
                memory devices.  Specifically, this purpose of this driver is to
                interface file systems (i.e. FAT) with specific FLASH chips in a
                device-independent manner.  Here is a diagram illustrating the 
                overall architecture:

                                ---------------
                                | File System |
                                ---------------
                                       |             uses logical sector addresses
                                ---------------
                                |     FAL     |
                                ---------------
                                       |             uses physical sector addresses
                                ---------------
                                |     FMD     |
                                ---------------
                                       |             uses program/erase algorithms
                               ------------------
                               | FLASH Hardware |
                               ------------------                               

                Each of the components is defined as follows:

                FAL (FLASH Abstraction Layer) - abstracts underlying FLASH hardware to file system(s)
                FMD (FLASH Media Driver)      - interfaces directly with underlying FLASH media

                Using this design, OEMs are free to implement custom FMDs for their respective FLASH chips.
                Practically, the simple FMD interface means that it is *very easy* for OEMs to port an example
                FMD to their respective hardware.
                
                The primary responsibilities of the FAL are as follows:

                    * Abstract the physical FLASH media from the file system.

                    * Translate logical sector addresses to physical sector addresses.

                    * "Wear-level" the physcial media.  FLASH memory has a limited write/erase lifetime and
                      it is important to spread the write/erase cycles across the entire media in order to
                      extend the lifetime of the memory device.

                Note that the FAL is NOT a file system; rather, it is a translation layer that file systems
                use in order to interface with FLASH media in a device-independent manner.


Environment:    This module (FAL.LIB) needs to be linked with a FLASH Media Driver (FMD.LIB) in
                order to create the full FLASH driver (FLASHDRV.DLL).

-----------------------------------------------------------------------------*/
#include <windows.h>
#include <windev.h>
#include "storemgr.h"

#include "fal.h"

DBGPARAM dpCurSettings = {
    TEXT("MSFLASH"), {
        TEXT("Init"),
        TEXT("Error"),
        TEXT("Compactor"),
        TEXT("Write"),
        TEXT("Read"),
        TEXT("Function"),
        TEXT("6"),
        TEXT("7"),
        TEXT("8"),
        TEXT("9"),
        TEXT("10"),
        TEXT("11"),
        TEXT("12"),
        TEXT("CeLog Errors"),
        TEXT("CeLog Compaction"),
        TEXT("CeLog Verbose")},
    0x2          // Errors are only on by default
}; 

//------------------------------ GLOBALS -------------------------------------------
CRITICAL_SECTION   g_csMain;                 // Only allow one request at a time.
CRITICAL_SECTION   g_csFlashDevice;          // Used to make driver re-entrant
static LONG        g_lReferenceCount = 0;        

static PDEVICE     g_pDevice = NULL;         // Handle to the device
PFlashInfoEx       g_pFlashMediaInfo = NULL; // FLASH chip-specific device information

HKEY               g_hDeviceKey = NULL;
HINSTANCE          g_hCoreDll = NULL;

TCHAR              g_szProfile[PROFILENAMESIZE];


DWORD              g_dwCompactionPrio256 = THREAD_PRIORITY_IDLE + 248;
DWORD              g_dwCompactionCritPrio256 = THREAD_PRIORITY_TIME_CRITICAL + 248;

DWORD              g_dwAvailableSectors        = 0;            // Total # of available sectors on media

CEDEVICE_POWER_STATE g_CurrentPowerState = D0;
FMDInterface FMD;
Fal** g_FalObjects;
//----------------------------------------------------------------------------------


BOOL GetDeviceInfo(PSTORAGEDEVICEINFO psdi) 
{
    StringCchCopy (psdi->szProfile, PROFILENAMESIZE, g_szProfile);
    psdi->dwDeviceClass = STORAGE_DEVICE_CLASS_BLOCK;
    psdi->dwDeviceType = STORAGE_DEVICE_TYPE_FLASH;
    psdi->dwDeviceFlags = STORAGE_DEVICE_FLAG_READWRITE | STORAGE_DEVICE_FLAG_TRANSACTED;

    if (g_pFlashMediaInfo->flashType == NOR)
        psdi->dwDeviceFlags |= STORAGE_DEVICE_FLAG_XIP;
    
    return TRUE;
}
            

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                          DLL ENTRY
-------------------------------------------------------------------*/
STDAPI_(BOOL) WINAPI
DllEntry(HINSTANCE DllInstance, DWORD Reason, LPVOID Reserved)
{
    switch(Reason) 
    {
    case DLL_PROCESS_ATTACH:
        RETAILREGISTERZONES (DllInstance);  
        DEBUGMSG(ZONE_INIT,(TEXT("FLASHDRV.DLL:DLL_PROCESS_ATTACH \r\n")));
        DisableThreadLibraryCalls((HMODULE)DllInstance);
        break;

    case DLL_PROCESS_DETACH:
        DEBUGMSG(ZONE_INIT,(TEXT("FLASHDRV.DLL:DLL_PROCESS_DETACH \r\n")));
        break;
    }
    return TRUE;
} 



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Init()

Description:    Initializes the FAL, queries the FMD for the FLASH
                memory device properties, etc.

Returns:        Context data for the FLASH device.
-------------------------------------------------------------------*/
DWORD DSK_Init(DWORD dwContext) 
{
    PDEVICE pDevice = g_pDevice;
    DWORD dwNumReserved = 0;
    DWORD dwUpdateReadOnly = 0;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Init()\r\n")));

    //----- 1. Check for re-entrant call.  If we have already initialized, then return -----
    //         the open handle to the FLASH device.
    if(InterlockedIncrement(&g_lReferenceCount) > 1)
    {
      return (DWORD)g_pDevice;
    }

    //----- 2. First time initializing.  Create the handle for the FLASH device -----
    if((pDevice = (PDEVICE)LocalAlloc(LPTR, sizeof(DEVICE))) != NULL)
    {
        memset(pDevice, 0, sizeof(pDevice) );
        pDevice->dwID = 'GOOD';         // "Anything" validates pdev, 'GOOD' is a fine choice
    }else
    {
        ReportError((TEXT("FLASHDRV.DLL:LocalAlloc() failed - unable to allocate PDEVICE handle for FLASH device.\r\n")));  
        goto INIT_ERROR;
    }

    GetFMDInterface(pDevice);

    //----- 3. Initialize the FLASH Media Driver (FMD) -----
    if((pDevice->hFMD=FMD.pInit((LPTSTR)dwContext,NULL,NULL))==NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("FLASHDRV.DLL:Unable to initialize FLASH Media Driver (FMD).\r\n")));
        goto INIT_ERROR;
    }

    if (FMD.pGetInfoEx)
    {
        if(!FMD.pGetInfoEx(NULL, &dwNumReserved) || (dwNumReserved == 0))
        {
            ReportError((TEXT("FLASHDRV.DLL:Unable to query FMD for FLASH device properties.\r\n")));
            goto INIT_ERROR;
        }

        g_pFlashMediaInfo = (PFlashInfoEx)LocalAlloc (LPTR, sizeof(FlashInfoEx) + (dwNumReserved - 1) * sizeof(FlashRegion));
        if (!g_pFlashMediaInfo)
        {
            goto INIT_ERROR;
        }
        
        if(!FMD.pGetInfoEx(g_pFlashMediaInfo, &dwNumReserved))
        {
            ReportError((TEXT("FLASHDRV.DLL:Unable to query FMD for FLASH device properties.\r\n")));
            goto INIT_ERROR;
        }
    }
    else
    {
        FlashInfo oldFlashInfo;

        // Use the old version of GetInfo
        if(!FMD.pGetInfo(&oldFlashInfo))
        {
            ReportError((TEXT("FLASHDRV.DLL:Unable to query FMD for FLASH device properties.\r\n")));
            goto INIT_ERROR;
        }

        g_pFlashMediaInfo = (PFlashInfoEx)LocalAlloc (LPTR, sizeof(FlashInfoEx));
        if (!g_pFlashMediaInfo)
        {
            goto INIT_ERROR;
        }

        // Fill in the FlashInfoEx version with the info from FlashInfo.
        g_pFlashMediaInfo->flashType = oldFlashInfo.flashType;
        g_pFlashMediaInfo->dwNumBlocks = oldFlashInfo.dwNumBlocks;
        g_pFlashMediaInfo->dwDataBytesPerSector = oldFlashInfo.wDataBytesPerSector;
        g_pFlashMediaInfo->dwNumRegions = 1;

        g_pFlashMediaInfo->region[0].regionType = FILESYS;
        g_pFlashMediaInfo->region[0].dwStartPhysBlock = 0;
        g_pFlashMediaInfo->region[0].dwNumPhysBlocks = oldFlashInfo.dwNumBlocks;
        g_pFlashMediaInfo->region[0].dwNumLogicalBlocks = FIELD_NOT_IN_USE;        
        g_pFlashMediaInfo->region[0].dwSectorsPerBlock = oldFlashInfo.wSectorsPerBlock;
        g_pFlashMediaInfo->region[0].dwBytesPerBlock = oldFlashInfo.dwBytesPerBlock;
        g_pFlashMediaInfo->region[0].dwCompactBlocks = DEFAULT_FLASH_BLOCKS_TO_RESERVE;
        
    }
    

    //----- 6. Initialize the FLASH device's critical section -----
    InitializeCriticalSection(&g_csMain);
    InitializeCriticalSection(&g_csFlashDevice);

    g_hCoreDll = LoadLibrary (TEXT("coredll.dll"));
    if (g_hCoreDll) 
    {
        pOpenDeviceKey fnOpenDeviceKey = (pOpenDeviceKey)GetProcAddress(g_hCoreDll, TEXT("OpenDeviceKey"));
        if (fnOpenDeviceKey)
            g_hDeviceKey = fnOpenDeviceKey ((PCTSTR)dwContext);
    }

    // Get profile name
    StringCchCopy (g_szProfile, PROFILENAMESIZE, L"FlashDisk");
    if (g_hDeviceKey)
    {
        DWORD dwLen = 0;
        pRegQueryValueEx fnRegQueryValueEx = (pRegQueryValueEx)GetProcAddress(g_hCoreDll, TEXT("RegQueryValueExW"));

        if (fnRegQueryValueEx)
        {
            if ((fnRegQueryValueEx (g_hDeviceKey, TEXT("Profile"), NULL, NULL, NULL, &dwLen) == ERROR_SUCCESS) &&
                 (dwLen <= (sizeof(TCHAR) * PROFILENAMESIZE)))
            {
                fnRegQueryValueEx (g_hDeviceKey, TEXT("Profile"), NULL, NULL, (LPBYTE)g_szProfile, &dwLen);        
            }
            
            dwLen = sizeof(DWORD);

            if (fnRegQueryValueEx(g_hDeviceKey, L"CompactionPrio256", 0, NULL, (LPBYTE)&g_dwCompactionPrio256, &dwLen) != ERROR_SUCCESS) 
                g_dwCompactionPrio256 = THREAD_PRIORITY_IDLE + 248;

            if (fnRegQueryValueEx(g_hDeviceKey, L"CompactionCritPrio256", 0, NULL, (LPBYTE)&g_dwCompactionCritPrio256, &dwLen) != ERROR_SUCCESS) 
                g_dwCompactionCritPrio256 = THREAD_PRIORITY_TIME_CRITICAL + 248;

            if (fnRegQueryValueEx(g_hDeviceKey, L"UpdateReadOnly", 0, NULL, (LPBYTE)&dwUpdateReadOnly, &dwLen) != ERROR_SUCCESS) 
                dwUpdateReadOnly = 0;

        }
    }
        
    //----- 7. Initialize the FAL -----
    g_FalObjects = new Fal*[g_pFlashMediaInfo->dwNumRegions];
    if (!g_FalObjects) {
        ReportError((TEXT("FLASHDRV.DLL: Out of memory!\r\n")));
        goto INIT_ERROR;
    }
    memset (g_FalObjects, 0, sizeof(Fal*) * g_pFlashMediaInfo->dwNumRegions);
    
    DWORD dwStartLogSector = 0;
    DWORD dwStartPhysSector = 0;

    if (g_pFlashMediaInfo->region[0].dwNumPhysBlocks) {
        dwStartPhysSector = g_pFlashMediaInfo->region[0].dwStartPhysBlock * g_pFlashMediaInfo->region[0].dwSectorsPerBlock;
    }
    
    for (DWORD iRegion = 0; iRegion < g_pFlashMediaInfo->dwNumRegions; iRegion++)
    {
        PFlashRegion pRegion = &g_pFlashMediaInfo->region[iRegion];
        
        // calculate the number of logical and physical blocks
        if (pRegion->dwNumLogicalBlocks == FIELD_NOT_IN_USE) 
        {
            // Determine logical range from the given physical range
            if (!CalculateLogicalRange(pRegion))
            {
                goto INIT_ERROR;
            }
        }
        else 
        {
            if (iRegion == 0)
            {
                pRegion->dwStartPhysBlock = 0;
            }
            else
            {
                pRegion->dwStartPhysBlock = g_pFlashMediaInfo->region[iRegion-1].dwStartPhysBlock + 
                    g_pFlashMediaInfo->region[iRegion-1].dwNumPhysBlocks;
            }
                    
            if (pRegion->dwNumLogicalBlocks == END_OF_FLASH) 
            {
                // This region goes to the end of flash.  Determine how many logical
                // sectors are available
                pRegion->dwNumPhysBlocks = g_pFlashMediaInfo->dwNumBlocks - pRegion->dwStartPhysBlock;
                if (!CalculateLogicalRange(pRegion))
                {
                    goto INIT_ERROR;
                }
            }
            else 
            {
                // Determine phsyical range from the given logical number of blocks
                if (!CalculatePhysRange(pRegion))
                {
                    goto INIT_ERROR;
                }
            }
        }
        
        if (pRegion->regionType == XIP) {
            g_FalObjects[iRegion] = new XipFal(dwStartLogSector, dwStartPhysSector, !dwUpdateReadOnly);
        }
        else {
            // If this is a read-only filesys region and we are not updating it, then the
            // FAL object should be created as read-only
            BOOL fReadOnly = FALSE;
            if ((pRegion->regionType == READONLY_FILESYS) && !dwUpdateReadOnly) {
                fReadOnly = TRUE;
            }
            g_FalObjects[iRegion] = new FileSysFal(dwStartLogSector, dwStartPhysSector, fReadOnly);
        }
        
        if(!g_FalObjects[iRegion] || !g_FalObjects[iRegion]->StartupFAL(pRegion)) {
            ReportError((TEXT("FLASHDRV.DLL:Unable to initialize FAL!\r\n")));
            goto INIT_ERROR;
        }

        dwStartPhysSector += g_FalObjects[iRegion]->GetNumPhysSectors();

        dwStartLogSector += g_FalObjects[iRegion]->GetNumLogSectors();
        g_dwAvailableSectors += g_FalObjects[iRegion]->GetNumLogSectors();
        
    }
    

    //----- 9. Cache the handle to the device and then return it to DEVICE.EXE -----
    g_pDevice = pDevice;
    return (DWORD)pDevice;  

INIT_ERROR:
    DSK_Deinit((DWORD)pDevice);
    return (DWORD)NULL;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Deinit()

Description:    Unloads the console device

Returns:        Boolean indicating success; DEVICE.EXE does not
                check the return code.
-------------------------------------------------------------------*/
BOOL DSK_Deinit(DWORD dwContext)
{
    PDEVICE pDevice = (PDEVICE) dwContext;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Deinit()\r\n")));

    //----- 1. If this not the last user of the FLASH device, then return -----
    if(InterlockedDecrement(&g_lReferenceCount) > 0)
    {
        return TRUE;
    }

    //----- 2. Close the FLASH device -----
    DSK_Close(dwContext);                               
    

    //----- 3. De-initialize the FAL -----
    if (g_pFlashMediaInfo && g_FalObjects) {
        for (DWORD iRegion = 0; iRegion < g_pFlashMediaInfo->dwNumRegions; iRegion++) {        
            if(g_FalObjects[iRegion]) {
                g_FalObjects[iRegion]->ShutdownFAL();
                delete g_FalObjects[iRegion];
            }
        }
    }
    delete[] g_FalObjects;

    if (g_pFlashMediaInfo) 
    {
        LocalFree (g_pFlashMediaInfo);
    }
    
    //----- 4. De-initialize the FLASH Media Driver (FMD) -----
    if(!FMD.pDeInit(pDevice->hFMD))
    {
        ReportError((TEXT("FLASHDRV.DLL:Unable to de-initialize FLASH Media Driver (FMD).\r\n")));
    }

    //----- 5. Unhook any shimmed FMD functions -----
    if (pDevice->hFMDHook) 
    {
        FMDHOOK_UnhookInterface(pDevice->hFMDHook, &FMD);
    }

    //----- 6. Free the FLASH device handle -----
    if(pDevice)                             
    {
        pDevice->dwID = 'DEAD';
        LocalFree(pDevice); 
    }

    //----- 7. Delete the Logical --> Physical Mapper's critical section -----
    DeleteCriticalSection(&g_csFlashDevice);
    DeleteCriticalSection(&g_csMain);

    if (g_hCoreDll) {
        pRegCloseKey fnRegCloseKey = (pRegCloseKey)GetProcAddress(g_hCoreDll, TEXT("RegCloseKey"));
        if (fnRegCloseKey)
            fnRegCloseKey (g_hDeviceKey);
        FreeLibrary (g_hCoreDll);
    }

    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Open()

Description:    Clears the console screen and restores defaults

Returns:        Context data for the console device 
-------------------------------------------------------------------*/
DWORD
DSK_Open(
    DWORD dwData,       // pdev
    DWORD dwAccess,
    DWORD dwShareMode)
{
    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Open()\r\n")));

    return dwData;  
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Close()

Description:    Closes the console

Returns:        Boolean indicating success
-------------------------------------------------------------------*/
BOOL DSK_Close(DWORD Handle)
{
    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Close()\r\n")));

    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Read()

Description:    Not Used.  The file system performs read commands by
                issuing the appopriate IOCTL.  See DSK_IoControl()
                for details.

Returns:        FALSE
-------------------------------------------------------------------*/
DWORD 
DSK_Read(
    DWORD Handle, 
    LPVOID pBuffer,                                             // Output buffer
    DWORD dwNumBytes)                                           // Buffer size in bytes
{
    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Read()\r\n")));
    return FALSE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Write()

Description:    Not Used.  The file system performs write commands by
                issuing the appopriate IOCTL.  See DSK_IoControl()
                for details.
                
Returns:        FALSE
-------------------------------------------------------------------*/
DWORD DSK_Write(
    DWORD Handle, 
    LPCVOID pBuffer, 
    DWORD dwInBytes)
{ 
    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Write()\r\n")));
    return FALSE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_Seek()

Description:    Seeks are not supported.

Returns:        FALSE
-------------------------------------------------------------------*/
DWORD 
DSK_Seek(
    DWORD Handle, 
    long lDistance, 
    DWORD dwMoveMethod
    )
{ 
    DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:Seek()\r\n")));
    return FALSE;
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_IOControl()

Description:    I/O Control function.  File systems use this function to:
                    * initialize the media      (DISK_IOCTL_INITIALIZED)
                    * read from the media       (DISK_IOCTL_READ)
                    * write to the media        (DISK_IOCTL_WRITE)
                    * get info about media      (DISK_IOCTL_GETINFO)
                    * set info for media        (DISK_IOCTL_SETINFO)
                    * get name of the media     (DISK_IOCTL_GETNAME)
                    * format the media          (DISK_IOCTL_FORMAT_MEDIA)
                    * deletes specified sectors (IOCTL_DISK_DELETE_SECTORS)

                Note that in all cases, accessing the actual media is
                performed by the FLASH Media Driver (FMD).

                Additionally, OEMs are free to implement their own user-defined
                IOCTLs for allowing applications to call directly into the FMD.
                
Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL
DSK_IOControl(
    DWORD Handle,
    DWORD dwIoControlCode,
    PBYTE pInBuf,
    DWORD nInBufSize,
    PBYTE pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned)
{
    BOOL fRet = FALSE;

    EnterCriticalSection(&g_csMain);
    EnterCriticalSection(&g_csFlashDevice);

    __try 
    {

    //----- 1. Check the function parameters to make sure we can handle the request... -----

    switch (dwIoControlCode) 
    {
        case DISK_IOCTL_INITIALIZED:
            if(pInBuf == NULL)
            {
                goto PARAMETER_ERROR;
            }
            break;
        case DISK_IOCTL_READ:case IOCTL_DISK_READ:
        case DISK_IOCTL_WRITE:case IOCTL_DISK_WRITE:
            if(pInBuf == NULL || nInBufSize < sizeof(SG_REQ)) 
            {
                goto PARAMETER_ERROR;
            }
            break;

        case IOCTL_DISK_GETINFO:
            if(pOutBuf == NULL || nOutBufSize != sizeof(DISK_INFO)) 
            {
                goto PARAMETER_ERROR;
            }
            break;

        case DISK_IOCTL_GETINFO:
        case DISK_IOCTL_SETINFO:case IOCTL_DISK_SETINFO:
            if(pInBuf == NULL || nInBufSize != sizeof(DISK_INFO)) 
            {
                goto PARAMETER_ERROR;
            }
            break;

        case DISK_IOCTL_GETNAME:case IOCTL_DISK_GETNAME:
            if(pOutBuf == NULL || nOutBufSize == 0)
            {
                goto PARAMETER_ERROR;
            }
            break;
            
        case DISK_IOCTL_FORMAT_MEDIA: case IOCTL_DISK_FORMAT_MEDIA:
            break;
            
        case IOCTL_DISK_DELETE_SECTORS:
        case IOCTL_DISK_SET_SECURE_WIPE_FLAG:
            if(pInBuf == NULL || nInBufSize != sizeof(DELETE_SECTOR_INFO))
            {
                goto PARAMETER_ERROR;
            }
            break;

        case IOCTL_DISK_SECURE_WIPE:
            if(pInBuf == NULL || nInBufSize != sizeof(DELETE_SECTOR_INFO))
            {
                goto PARAMETER_ERROR;
            }
            break;

        case IOCTL_DISK_DEVICE_INFO:
            if(pInBuf == NULL || nInBufSize != sizeof(STORAGEDEVICEINFO))
            {
                goto PARAMETER_ERROR;
            }
            break;
        case IOCTL_DISK_GET_SECTOR_ADDR:
            if(pInBuf == NULL || pOutBuf == NULL || nInBufSize != nOutBufSize)
            {
                goto PARAMETER_ERROR;
            }
            break;        
        case IOCTL_POWER_CAPABILITIES:
            if(pOutBuf == NULL || nOutBufSize < sizeof(POWER_CAPABILITIES) || pBytesReturned == NULL) 
            {
                goto PARAMETER_ERROR;
            }
            break;
            
        case IOCTL_POWER_SET:
            if(pOutBuf == NULL || nOutBufSize < sizeof(CEDEVICE_POWER_STATE) || pBytesReturned == NULL) 
            {
                goto PARAMETER_ERROR;
            }
            break;
            
        case IOCTL_POWER_GET:
            if(pOutBuf == NULL || nOutBufSize < sizeof(CEDEVICE_POWER_STATE) || pBytesReturned == NULL) 
            {
                goto PARAMETER_ERROR;
            }
            break;
            
        default:
            // Pass any other IOCTL through to the FMD.
            fRet = FMD.pOEMIoControl(dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
            goto IO_EXIT; 
        }


        //----- 2. Now handle the IOCTL request... -----    
        switch (dwIoControlCode) 
        {
        case DISK_IOCTL_INITIALIZED:
            break;


        case DISK_IOCTL_READ:case IOCTL_DISK_READ:
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(DISK_IOCTL_READ)\r\n")));      

            //----- Service this scatter/gather READ request -----
            if((fRet = ReadFromMedia((PSG_REQ)pInBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ReadFromMedia() failed.\r\n")));        
            }   
            break;


        case DISK_IOCTL_WRITE:case IOCTL_DISK_WRITE:
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(DISK_IOCTL_WRITE)\r\n")));     
            
            //----- Service this scatter/gather WRITE request -----
            if((fRet = WriteToMedia((PSG_REQ)pInBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() failed.\r\n")));     
            }
            break;


        case DISK_IOCTL_GETINFO:case  IOCTL_DISK_GETINFO:
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(DISK_IOCTL_GETINFO)\r\n")));       

            //----- Retrieve the storage information for this FLASH device -----
            if((fRet = GetDiskInfo((PDISK_INFO)(dwIoControlCode!=IOCTL_DISK_GETINFO?pInBuf:pOutBuf))) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - Unable to get disk information from FMD!\r\n")));
                goto IO_EXIT;
            }
            break;


        case DISK_IOCTL_SETINFO: case IOCTL_DISK_SETINFO:
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(DISK_IOCTL_SETINFO)\r\n")));       

            //----- The FLASH media is a fixed size; hence, nothing needs to be done -----
            break;


        case DISK_IOCTL_GETNAME:case IOCTL_DISK_GETNAME:
            SetLastError(ERROR_NOT_SUPPORTED);
            break;


        case DISK_IOCTL_FORMAT_MEDIA:case IOCTL_DISK_FORMAT_MEDIA:
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(DISK_IOCTL_FORMAT_MEDIA)\r\n")));      

            //----- Format the media on the FLASH device -----
            if((fRet = FormatMedia()) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - Unable to format FLASH media!\r\n")));
                goto IO_EXIT;
            }
            break;
        case IOCTL_DISK_DEVICE_INFO:
        {
            fRet = GetDeviceInfo((PSTORAGEDEVICEINFO)pInBuf);
            if (pBytesReturned)
                *pBytesReturned = sizeof(STORAGEDEVICEINFO);
            break;    
        }    

        case IOCTL_DISK_DELETE_SECTORS:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_DISK_DELETE_SECTORS)\r\n")));
            //----- Delete the requested sectors -----
            if((fRet = DeleteSectors((PDELETE_SECTOR_INFO)pInBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - DeleteSectors() failed.\r\n")));
                goto IO_EXIT;
            }
            break;

        case IOCTL_DISK_SECURE_WIPE:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_DISK_SECURE_WIPE)\r\n")));
            //----- Secure wipe the flash region  -----
            if((fRet = SecureWipe((PDELETE_SECTOR_INFO)pInBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - SecureWipe() failed.\r\n")));
                goto IO_EXIT;
            }
            break;

        case IOCTL_DISK_SET_SECURE_WIPE_FLAG:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_DISK_SET_SECURE_WIPE_FLAG)\r\n")));
            //----- Set the secure wipe flag for the flash region  -----
            if((fRet = SetSecureWipeFlag((PDELETE_SECTOR_INFO)pInBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - SetSecureWipeFlag() failed.\r\n")));
                goto IO_EXIT;
            }
            break;

            
        case IOCTL_DISK_GET_SECTOR_ADDR:
        {
            DEBUGMSG(ZONE_FUNCTION,(TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_DISK_GET_SECTOR_ADDR)\r\n")));      

            //----- Service this scatter/gather READ request -----
            if((fRet = GetPhysSectorAddr((PSECTOR_ADDR)pInBuf, (PSECTOR_ADDR)pOutBuf, nInBufSize / sizeof(SECTOR_ADDR))) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ReadFromMedia() failed.\r\n")));        
            }   
            break;  
        }
        case IOCTL_POWER_CAPABILITIES:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_POWER_CAPABILITIES)\r\n")));
            if((fRet = GetPowerCapabilities((PPOWER_CAPABILITIES)pOutBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - GetPowerCapabilities() failed.\r\n")));
                goto IO_EXIT;
            }
            *pBytesReturned = sizeof(POWER_CAPABILITIES);
            break;
            
        case IOCTL_POWER_SET:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_POWER_SET)\r\n")));
            if((fRet = SetPowerState((PCEDEVICE_POWER_STATE)pOutBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - SetPowerState() failed.\r\n")));
                goto IO_EXIT;
            }
            *pBytesReturned = sizeof(CEDEVICE_POWER_STATE);
            break;
            
        case IOCTL_POWER_GET:
            DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:DSK_IOControl(IOCTL_POWER_GET)\r\n")));
            if((fRet = GetPowerState((PCEDEVICE_POWER_STATE)pOutBuf)) == FALSE)
            {
                ReportError((TEXT("FLASHDRV.DLL:ERROR - GetPowerState() failed.\r\n")));
                goto IO_EXIT;
            }
            *pBytesReturned = sizeof(CEDEVICE_POWER_STATE);
            break;
        
    }
    }        
    __except (EXCEPTION_EXECUTE_HANDLER)  
    {      
        SetLastError(ERROR_INVALID_PARAMETER);
    }
           
IO_EXIT:       
    LeaveCriticalSection(&g_csFlashDevice);
    LeaveCriticalSection(&g_csMain);
    return fRet;

PARAMETER_ERROR:
    SetLastError(ERROR_INVALID_PARAMETER);
    LeaveCriticalSection(&g_csFlashDevice);
    LeaveCriticalSection(&g_csMain);
    return FALSE;
    
}                                                                             


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_PowerUp()

Description:    Performs the necessary powerup procedures.
                
Returns:        N/A
-------------------------------------------------------------------*/
VOID DSK_PowerUp(VOID)
{
    // Invoke the FMD to perform any necessary powerup procedures...
    FMD.pPowerUp();
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DSK_PowerDown()

Description:    Performs the necessary powerdown procedures.
                
Returns:        N/A
-------------------------------------------------------------------*/
VOID DSK_PowerDown(VOID)
{
    // Invoke the FMD to perform any necessary powerdown procedures...
    FMD.pPowerDown();
}


//------------------------------- Helper Functions ----------------------------------------


VOID GetFMDInterface(PDEVICE pDevice)
{
    FMD.cbSize = sizeof(FMDInterface);

    // query FMD intrface from the FMD    
    FMD.pOEMIoControl = FMD_OEMIoControl;
    if (!FMD_OEMIoControl (IOCTL_FMD_GET_INTERFACE, NULL, 0, (PBYTE)&FMD, sizeof(FMDInterface), NULL))
    {
        // FMD does not support IOCTL_FMD_GET_INTERFACE, so build the FMDInterface 
        // structure using the legacy FMD functions
        FMD.pInit = FMD_Init;
        FMD.pDeInit = FMD_Deinit;
        FMD.pGetInfo = FMD_GetInfo;        
        FMD.pGetBlockStatus = FMD_GetBlockStatus;     
        FMD.pSetBlockStatus = FMD_SetBlockStatus;
        FMD.pReadSector = FMD_ReadSector;
        FMD.pWriteSector = FMD_WriteSector;
        FMD.pEraseBlock = FMD_EraseBlock;
        FMD.pPowerUp = FMD_PowerUp;
        FMD.pPowerDown = FMD_PowerDown;
    }

    // query hook library in case any FMD functions need to be shimmed
    pDevice->hFMDHook = FMDHOOK_HookInterface(&FMD);
}

Fal* GetFALObject (DWORD dwStartSector, DWORD dwNumSectors)
{
    for (DWORD iRegion = 0; iRegion < g_pFlashMediaInfo->dwNumRegions; iRegion++) {
        if (g_FalObjects[iRegion]->IsInRange (dwStartSector, dwNumSectors)) {
            return g_FalObjects[iRegion];
        }
    }
    return NULL;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       GetDiskInfo()

Description:    Retrieves disk information for the underlying FLASH media.

Notes:          The storage information for the FLASH media is retrieved by
                FMD_GetInfo() at driver initialization.

Returns:        Boolean indicating success.
-------------------------------------------------------------------------------*/
BOOL GetDiskInfo(PDISK_INFO pDiskInfo)
{
    pDiskInfo->di_total_sectors  = g_dwAvailableSectors;
    pDiskInfo->di_bytes_per_sect = g_pFlashMediaInfo->dwDataBytesPerSector;

    // Emulate a cylinder as a flash block.
    pDiskInfo->di_cylinders      = g_pFlashMediaInfo->dwNumBlocks;
    pDiskInfo->di_heads          = 1;
    pDiskInfo->di_sectors        = g_pFlashMediaInfo->region[0].dwSectorsPerBlock;
    pDiskInfo->di_flags          = DISK_INFO_FLAG_PAGEABLE;

    return TRUE;  
}


BOOL CheckSg (PSG_REQ pSG_req, BOOL fRead, LPBOOL pfCombineSg, LPDWORD pdwTotalSize)
{
    DWORD dwSGBuffNum = 0;  
    DWORD dwTotalSize = 0;

    *pfCombineSg = FALSE;
    *pdwTotalSize = 0;

    //----- 1. Parse the scatter/gather request to determine if it can be fulfilled -----
    pSG_req->sr_status = ERROR_IO_PENDING;

    //----- For debugging purposes, print out the READ request parameters... -----
    if (fRead ? ZONE_READ_OPS : ZONE_WRITE_OPS) {
        DEBUGMSG(ZONE_READ_OPS, (TEXT("FLASHDRV.DLL:%s() request.\r\n"), fRead ? TEXT("Read") : TEXT("Write")));                        
        DEBUGMSG(ZONE_READ_OPS, (TEXT("             Start Sector = %x\r\n"), pSG_req->sr_start));   
        DEBUGMSG(ZONE_READ_OPS, (TEXT("             # of Sectors = %d\r\n"), pSG_req->sr_num_sec)); 
        DEBUGMSG(ZONE_READ_OPS, (TEXT("             # of SG bufs = %d\r\n"), pSG_req->sr_num_sg));  
        DEBUGMSG(ZONE_READ_OPS, (TEXT("                 BUFF LEN = %d\r\n"), pSG_req->sr_sglist[0].sb_len));
    }
    
    if((pSG_req->sr_num_sg <= 0) || (pSG_req->sr_num_sg > MAX_SG_BUF))      // Is the # of scatter/gather buffers legitimate?
    {
        ReportError((TEXT("FLASHDRV.DLL:%s() - invalid parameter: pSG_req->sr_num_sg=%d\r\n"), fRead ? TEXT("Read") : TEXT("Write"), pSG_req->sr_num_sg));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    if(pSG_req->sr_num_sec <= 0)                                            // Is the request for 0 sectors?
    {
        ReportError((TEXT("FLASHDRV.DLL:%s() - request for 0 sectors\r\n"), fRead ? TEXT("Read") : TEXT("Write")));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    if((pSG_req->sr_start + pSG_req->sr_num_sec) > g_dwAvailableSectors)    // Does this request "run" off the end of the media?
    {
        ReportError((TEXT("FLASHDRV.DLL:%s() - request runs off end of media\r\n"), fRead ? TEXT("Read") : TEXT("Write")));
        pSG_req->sr_status = ERROR_SECTOR_NOT_FOUND;
        return FALSE;
    }


    for(dwSGBuffNum = 0; dwSGBuffNum < pSG_req->sr_num_sg; dwSGBuffNum++)          // Check each scatter/gather buffer...
    {
        if((pSG_req->sr_sglist[dwSGBuffNum].sb_len <= 0) || (pSG_req->sr_sglist[dwSGBuffNum].sb_buf == NULL)) 
        {
            ReportError((TEXT("FLASHDRV.DLL:%s() - scatter/gather buffer %d isn't valid\r\n"), fRead ? TEXT("Read") : TEXT("Write"), dwSGBuffNum));
            pSG_req->sr_status = ERROR_INVALID_PARAMETER;
            return FALSE;
        }
        if (((pSG_req->sr_sglist[dwSGBuffNum].sb_len % g_pFlashMediaInfo->dwDataBytesPerSector) != 0))
        {
            *pfCombineSg = TRUE;
        }

        if (dwTotalSize + pSG_req->sr_sglist[dwSGBuffNum].sb_len < dwTotalSize)
        {
            // Integer overflow
            ReportError((TEXT("FLASHDRV.DLL - Integer overflow when calculating total buffer size.\r\n")));
            pSG_req->sr_status = ERROR_INVALID_PARAMETER;
            return FALSE;
        }
        
        dwTotalSize += pSG_req->sr_sglist[dwSGBuffNum].sb_len;
    }

    if (!dwTotalSize || (dwTotalSize % g_pFlashMediaInfo->dwDataBytesPerSector)) {
        ReportError((TEXT("FLASHDRV.DLL:%s() - Sum of buffer lengths (%d) is not a sector multiple.\r\n"), fRead ? TEXT("Read") : TEXT("Write"), dwTotalSize));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    if (dwTotalSize / g_pFlashMediaInfo->dwDataBytesPerSector != pSG_req->sr_num_sec)
    {
        ReportError((TEXT("FLASHDRV.DLL:%s() - Sum of buffer lengths (%d) is not equal to number of sectors.\r\n"), fRead ? TEXT("Read") : TEXT("Write"), dwTotalSize));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
        
    *pdwTotalSize = dwTotalSize;
    return TRUE;
}


BOOL GetPhysSectorAddr (PSECTOR_ADDR pLogicalSectors, PSECTOR_ADDR pPhysAddrs, DWORD dwNumSectors)
{
    SECTOR_ADDR physicalSectorAddr;
    DWORD i;
    
    Fal* pFAL = GetFALObject (pLogicalSectors[0], 1);
    if (!pFAL) {
        ReportError((TEXT("FLASHDRV.DLL:GetPhysSectorAddress() - GetFALObject failed\r\n")));
        return FALSE;
    }

    for (i = 0; i < dwNumSectors; i++) 
    {
        if (pFAL->m_pMap->GetPhysicalSectorAddr(pLogicalSectors[i], &physicalSectorAddr) && (physicalSectorAddr != UNMAPPED_LOGICAL_SECTOR) &&
            FMD.pGetPhysSectorAddr)
        {
            FMD.pGetPhysSectorAddr(physicalSectorAddr, &pPhysAddrs[i]);
        }
        else
        {
            pPhysAddrs[i] = UNMAPPED_LOGICAL_SECTOR;
        }
    }    
    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       InternalReadFromMedia()

Description:    Performs the specified scatter/gather READ request from the media.

Notes:          After parsing the parameters, the actual READ request is handled
                by the FLASH Media Driver (FMD).

Returns:        Boolean indicating success.
-------------------------------------------------------------------------------*/
BOOL ReadFromMedia(PSG_REQ pSG_req)
{
    SG_REQ       sgNew;
    PUCHAR       pBuff         = NULL;
    DWORD        dwSGBuffNum   = 0;
    DWORD        dwTotalSize   = 0;
    BOOL         fRet          = FALSE;
    BOOL         fCombineSg    = FALSE;
    Fal*         pFAL          = NULL;

    if (!CheckSg (pSG_req, TRUE, &fCombineSg, &dwTotalSize)) {
        goto READ_ERROR;
    }

    pFAL = GetFALObject (pSG_req->sr_start, pSG_req->sr_num_sec);
    if (!pFAL) {
        ReportError((TEXT("FLASHDRV.DLL:ReadFromMedia() - GetFALObject failed\r\n")));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        goto READ_ERROR;
    }

    if (fCombineSg) {
        
        // Combine multiple SG buffers into one if any buffer contains a
        // partial sector

        sgNew.sr_num_sec = dwTotalSize / g_pFlashMediaInfo->dwDataBytesPerSector;

        sgNew.sr_sglist[0].sb_len = dwTotalSize;
        sgNew.sr_sglist[0].sb_buf = (BYTE *)LocalAlloc( LPTR, sgNew.sr_sglist[0].sb_len);
        
        if (!sgNew.sr_sglist[0].sb_buf) {
            pSG_req->sr_status = GetLastError();
            goto READ_ERROR;
        }
        
        sgNew.sr_num_sg = 1;
        sgNew.sr_start = pSG_req->sr_start;
        
        if ((fRet = pFAL->ReadFromMedia(&sgNew, FALSE)) == TRUE) {

            // Copy contents read in sgNew back to original sg buffers
            
            pBuff = sgNew.sr_sglist[0].sb_buf;
            for(dwSGBuffNum=0; dwSGBuffNum<pSG_req->sr_num_sg; dwSGBuffNum++)          
            {
                LPBYTE pBuff2 = NULL;

                // Open the sg buffer; performs caller access check.
                if (FAILED (CeOpenCallerBuffer (
                        (PVOID*)&pBuff2,
                        pSG_req->sr_sglist[dwSGBuffNum].sb_buf,
                        pSG_req->sr_sglist[dwSGBuffNum].sb_len,
                        ARG_O_PTR,
                        FALSE ))) 
                {
                    ReportError((TEXT("FLASHDRV.DLL:ReadFromMedia() - CeOpenCallerBuffer() failed, couldn't obtain pointer to buffer.\r\n"), 0));
                    pSG_req->sr_status = ERROR_INSUFFICIENT_BUFFER;         // Assume CeOpenCallerBuffer() failed because buffer was invalid
                }
                else
                {
                    memcpy( pBuff2, pBuff,  pSG_req->sr_sglist[dwSGBuffNum].sb_len);
                }    
                pBuff += pSG_req->sr_sglist[dwSGBuffNum].sb_len;

                // Close the sg buffer.
                VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                    pBuff2,
                    pSG_req->sr_sglist[dwSGBuffNum].sb_buf,
                    pSG_req->sr_sglist[dwSGBuffNum].sb_len,
                    ARG_O_PTR )));                
            }
        } else {
            pSG_req->sr_status = sgNew.sr_status;
        }    
        LocalFree( sgNew.sr_sglist[0].sb_buf);

    } else {
        fRet = pFAL->ReadFromMedia(pSG_req, TRUE);
    }    
READ_ERROR:
    if (fRet) {
        pSG_req->sr_status = ERROR_SUCCESS;
    } else {
        SetLastError( pSG_req->sr_status);
    }    
    return fRet;
}    

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       WriteToMedia()

Description:    Performs the specified scatter/gather WRITE request to the media.

Notes:          After parsing the parameters, the actual WRITE request is handled
                by the FLASH Media Driver (FMD).

Returns:        Boolean indicating success.
-------------------------------------------------------------------------------*/
BOOL WriteToMedia(PSG_REQ pSG_req)
{
    SG_REQ       sgNew;
    PUCHAR       pBuff         = NULL;
    DWORD        dwSGBuffNum   = 0;
    DWORD        dwTotalSize   = 0;
    BOOL         fRet          = FALSE;
    BOOL         fCombineSg    = FALSE;
    Fal*         pFAL          = NULL;

    if (!CheckSg (pSG_req, FALSE, &fCombineSg, &dwTotalSize)) {
        goto WRITE_ERROR;
    }

    pFAL = GetFALObject (pSG_req->sr_start, pSG_req->sr_num_sec);
    if (!pFAL) {
        ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - GetFALObject failed\r\n")));
        pSG_req->sr_status = ERROR_INVALID_PARAMETER;
        goto WRITE_ERROR;
    }

    if (fCombineSg) {

        // Combine multiple SG buffers into one if any buffer contains a
        // partial sector
        
        sgNew.sr_num_sec = dwTotalSize / g_pFlashMediaInfo->dwDataBytesPerSector;

        sgNew.sr_sglist[0].sb_len = dwTotalSize;
        sgNew.sr_sglist[0].sb_buf = (BYTE *)LocalAlloc( LPTR, sgNew.sr_sglist[0].sb_len);
            
        if (!sgNew.sr_sglist[0].sb_buf) {
            pSG_req->sr_status = GetLastError();
            goto WRITE_ERROR;
        }
        
        sgNew.sr_num_sg = 1;
        sgNew.sr_start = pSG_req->sr_start;

        // Copy contents to write from original sg buffers to new sg buffer

        pBuff = sgNew.sr_sglist[0].sb_buf;
        for(dwSGBuffNum=0; dwSGBuffNum < pSG_req->sr_num_sg; dwSGBuffNum++)          
        {
            LPBYTE pBuff2 = NULL;

            // Open the sg buffer; performs caller access check.
            if (FAILED (CeOpenCallerBuffer (
                    (PVOID*)&pBuff2,
                    pSG_req->sr_sglist[dwSGBuffNum].sb_buf,
                    pSG_req->sr_sglist[dwSGBuffNum].sb_len,
                    ARG_I_PTR,
                    FALSE ))) 
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - CeOpenCallerBuffer() failed, couldn't obtain pointer to buffer.\r\n"), 0));
                pSG_req->sr_status = ERROR_INSUFFICIENT_BUFFER;         // Assume CeOpenCallerBuffer() failed because buffer was invalid
            }
            else
            {
                memcpy( pBuff, pBuff2,  pSG_req->sr_sglist[dwSGBuffNum].sb_len);
            }    
            pBuff += pSG_req->sr_sglist[dwSGBuffNum].sb_len;

            // Close the sg buffer.
            VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                pBuff2,
                pSG_req->sr_sglist[dwSGBuffNum].sb_buf,
                pSG_req->sr_sglist[dwSGBuffNum].sb_len,
                ARG_I_PTR )));                            
        }
        
        fRet = pFAL->WriteToMedia(&sgNew, FALSE);
        if (!fRet) {
            pSG_req->sr_status = sgNew.sr_status;
        }
        
        LocalFree( sgNew.sr_sglist[0].sb_buf);

    } else {
        fRet = pFAL->WriteToMedia(pSG_req, TRUE);
    }   
    
WRITE_ERROR:
    if (fRet) {
        pSG_req->sr_status = ERROR_SUCCESS;
    } else {
        SetLastError( pSG_req->sr_status);
    }    
    return fRet;
}


BOOL DeleteSectors(PDELETE_SECTOR_INFO pDeleteSectorInfo)
{    
    Fal* pFal = GetFALObject (pDeleteSectorInfo->startsector, pDeleteSectorInfo->numsectors);
    if (!pFal) {
        ReportError((TEXT("FLASHDRV.DLL:DeleteSectors() - GetFALObject failed\r\n")));
        return FALSE;
    }
       
    return pFal->DeleteSectors (pDeleteSectorInfo->startsector, pDeleteSectorInfo->numsectors);
}

BOOL SecureWipe(PDELETE_SECTOR_INFO pDeleteSectorInfo)
{    
    BOOL fRet = TRUE;
    
    Fal* pFal = GetFALObject (pDeleteSectorInfo->startsector, pDeleteSectorInfo->numsectors);
    if (!pFal) {
        ReportError((TEXT("FLASHDRV.DLL:SecureWipe() - GetFALObject failed\r\n")));
        return FALSE;
    }
       
    fRet = pFal->SecureWipe();

    pFal->ShutdownFAL();
    pFal->StartupFAL(pFal->GetFlashRegion());

    return fRet;
}

BOOL SetSecureWipeFlag(PDELETE_SECTOR_INFO pDeleteSectorInfo)
{    
    Fal* pFal = GetFALObject (pDeleteSectorInfo->startsector, pDeleteSectorInfo->numsectors);
    if (!pFal) {
        ReportError((TEXT("FLASHDRV.DLL:SetSecureWipeFlag() - GetFALObject failed\r\n")));
        return FALSE;
    }
       
    return (pFal->SetSecureWipeFlag(INVALID_BLOCK_ID) != INVALID_BLOCK_ID);
}
BOOL FormatMedia(VOID)
{
    BOOL fRet = TRUE;
    
    for (DWORD iRegion = 0; iRegion < g_pFlashMediaInfo->dwNumRegions; iRegion++) {
        
        if (g_pFlashMediaInfo->region[iRegion].regionType == FILESYS) {
            fRet = g_FalObjects[iRegion]->FormatRegion();
            if (!fRet) {
                break;
            } else {
                g_FalObjects[iRegion]->ShutdownFAL();
                fRet = g_FalObjects[iRegion]->StartupFAL(&g_pFlashMediaInfo->region[iRegion]);
            }
            
        }
    }
    return fRet;
}

BOOL GetPowerCapabilities(PPOWER_CAPABILITIES ppc)
{
    memset (ppc, 0, sizeof(POWER_CAPABILITIES));
    ppc->DeviceDx = DX_MASK(D0) | DX_MASK(D4);
    return TRUE;    
}

BOOL SetPowerState (PCEDEVICE_POWER_STATE pNewPowerState)
{
    if (g_CurrentPowerState <= D2 && *pNewPowerState >= D3)
    {
        // This is going from power on to power off, so wait for
        // the compaction threads to finish by entering
        // the device critical section
        EnterCriticalSection (&g_csFlashDevice);
    }    
    else if (g_CurrentPowerState >= D3 && *pNewPowerState <= D2)
    {
        // This is going from power off to power on, so leave
        // the critical section
        LeaveCriticalSection (&g_csFlashDevice);
    }
    g_CurrentPowerState = *pNewPowerState;

    return TRUE;
}

BOOL GetPowerState (PCEDEVICE_POWER_STATE pPowerState)
{
    *pPowerState = g_CurrentPowerState;
    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ComputeLog2()

Description:    Computes log2 of the specified number.  If the specified number
                is NOT a power of 2, an error is returned.

Returns:        log2 of the specified number; otherwise NOT_A_POWER_OF_2 indicating error.
------------------------------------------------------------------------------*/
UCHAR ComputeLog2(DWORD dwNum)
{
    UCHAR log2 = 0;

    //----- 1. SPECIAL CASE: 2 raised to any exponent never equals 0 ----
    if(dwNum == 0)
    {
        return NOT_A_POWER_OF_2;
    }

    //----- 2. Keep dividing by 2 until the LSB is 1 -----
    while(!(dwNum & 0x000000001))
    {
        dwNum >>= 1;
        log2++;
    }

    //----- 3. If (dwNum>>1) != 0, dwNum wasn't a power of 2 -----
    if(dwNum>>1)
    {
        return NOT_A_POWER_OF_2;
    }

    return log2;
}

BOOL CalculateLogicalRange(PFlashRegion pRegion)
{
    DWORD dwBlockID;
    DWORD dwNumLogicalBlocks = 0;
    
    for (dwBlockID = pRegion->dwStartPhysBlock; dwBlockID < pRegion->dwStartPhysBlock + pRegion->dwNumPhysBlocks; dwBlockID++)
    {
        DWORD dwStatus = FMD.pGetBlockStatus (dwBlockID);

        if (!(dwStatus & (BLOCK_STATUS_RESERVED | BLOCK_STATUS_BAD)))
        {
            dwNumLogicalBlocks++;
        }
    }

    if (dwNumLogicalBlocks <= pRegion->dwCompactBlocks)
    {
        ReportError((TEXT("FLASHDRV.DLL:CalculateLogicalRange() - Invalid number of logical blocks %d\r\n"), dwNumLogicalBlocks));
        return FALSE;
    }

    // Account for compaction blocks
    dwNumLogicalBlocks -= pRegion->dwCompactBlocks;
    pRegion->dwNumLogicalBlocks = dwNumLogicalBlocks;
    return TRUE;
}

BOOL CalculatePhysRange(PFlashRegion pRegion)
{
    DWORD dwBlockID = pRegion->dwStartPhysBlock;
    DWORD dwNumLogicalBlocks = 0;

    // Need to skip BAD or RESERVED blocks in compaction blocks
    DWORD dwNeededLogicalBlocks = pRegion->dwNumLogicalBlocks + pRegion->dwCompactBlocks; 
    
    while (dwNumLogicalBlocks < dwNeededLogicalBlocks)
    {
        DWORD dwStatus = FMD.pGetBlockStatus (dwBlockID);

        if (!(dwStatus & (BLOCK_STATUS_RESERVED | BLOCK_STATUS_BAD)))
        {
            dwNumLogicalBlocks++;
        }
        
        dwBlockID++;
    }    

    if (dwNumLogicalBlocks == 0)
    {
        ReportError((TEXT("FLASHDRV.DLL:CalculatePhysRange() - Invalid number of logical blocks %d\r\n"), dwNumLogicalBlocks));
        return FALSE;
    }

    pRegion->dwNumPhysBlocks = dwBlockID - pRegion->dwStartPhysBlock;
    return TRUE;
}

