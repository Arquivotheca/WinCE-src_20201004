//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// ************************************************************
// This file contains registry related helper routines for the
// Unimodem SPI.
// **********************************************************************

#include "windows.h"
#include "types.h"
#include "memory.h"
#include "mcx.h"
#include "tspi.h"
#include "linklist.h"
#include "tspip.h"
#include "tapicomn.h"


#ifdef DEBUG
#define REG_MSGS 0
#endif

// ***************************************************************************
//
// ***************************************************************************
DWORD
MdmRegGetValue(
    PTLINEDEV ptLineDev,    // Which line device
    LPCWSTR   szKeyName,    // Which Unimodem key contains the value
    LPCWSTR   szValueName,  // What value should we look up
    DWORD     dwType,       // And what type should it be
    LPBYTE    lpData,       // Where do we return the results
    LPDWORD   lpdwSize      // How large is lpData
)
{
    DWORD dwTmpType = dwType;
    DWORD dwRetCode;
    HKEY  hKey;
    
    // ----------------------------------------------------------------------
    // ------- First, see if a device specific value exists -----------------
    // ----------------------------------------------------------------------

    // Open the device specific key if needed
    if( NULL != szKeyName )
    {
        dwRetCode = RegOpenKeyEx(
            ptLineDev->hSettingsKey,
            szKeyName,
            0,
            KEY_READ,
            &hKey);
        DEBUGMSG(REG_MSGS & ZONE_MISC,
                  (TEXT("UNIMODEM:MdmRegGetValue RegOpenKeyEx(%s) failed %d\n"),
                   szKeyName, dwRetCode));
    }
    else
    {
        // No need to open anything, use base key.
        hKey = ptLineDev->hSettingsKey;
        dwRetCode = ERROR_SUCCESS;
    }
    

    if (ERROR_SUCCESS == dwRetCode)
    {
        
        dwRetCode = RegQueryValueEx(hKey,
                                    szValueName,
                                    NULL,
                                    &dwTmpType,
                                    (PUCHAR)lpData,
                                    lpdwSize);
    
        if ( dwRetCode  != ERROR_SUCCESS)
        {
            DEBUGMSG(REG_MSGS & ZONE_MISC,
                      (TEXT("UNIMODEM:MdmRegGetValue RegQueryValueEx failed when reading %s (device specific).\r\n"), szValueName));
        }
        else
        {
            if (dwTmpType != dwType)
            {
                // Hmm, this shouldn't happen.  I guess the best thing to
                // do is to see if we can find a valid entry in the defaults
                DEBUGMSG(REG_MSGS & ZONE_MISC,
                          (TEXT("UNIMODEM:MdmRegGetValue '%s' was type x%X, not x%X.\r\n"),
                           szValueName, dwTmpType, dwType));
                dwRetCode = ERROR_NO_TOKEN;  // cause us to read default below
            }
        
        }

        // be sure to close the key if we opened it
        if( szKeyName != NULL )
        {
            RegCloseKey( hKey );
        }
        
    }
    else
    {
        // we couldn't open a device specific key, fall thru to default    
        DEBUGMSG(REG_MSGS & ZONE_MISC,
                  (TEXT("UNIMODEM:MdmRegGetValue Unable to open device specific key for %s. (ignoring)\r\n"), szKeyName));
    }
    
    
    // ----------------------------------------------------------------------
    // ------- If needed, look for the default value ------------------------
    // ----------------------------------------------------------------------
    if( ERROR_SUCCESS != dwRetCode )
    {

        // Open the default key if needed
        if( NULL != szKeyName )
        {
            dwRetCode = RegOpenKeyEx(
                TspiGlobals.hDefaultsKey,
                szKeyName,
                0,
                KEY_READ,
                &hKey);
        }
        else
        {
            // No need to open anything, use base key.
            hKey = TspiGlobals.hDefaultsKey;
            dwRetCode = ERROR_SUCCESS;
        }

        if (ERROR_SUCCESS == dwRetCode)
        {
            // Open the specified default key   
            dwTmpType = dwType;
            dwRetCode = RegQueryValueEx(hKey,
                                        szValueName,
                                        NULL,
                                        &dwTmpType,
                                        (PUCHAR)lpData,
                                        lpdwSize);
            if ( dwRetCode != ERROR_SUCCESS)
            {
                DEBUGMSG(REG_MSGS & ZONE_MISC,
                          (TEXT("UNIMODEM:MdmRegGetValue RegQueryValueEx failed when reading %s (default).\r\n"), szValueName));
            }
            else
            {
                if (dwTmpType != dwType)
                {
                    // Hmm, this shouldn't happen.  I guess the best thing to
                    // do is to see if we can find a valid entry in the defaults
                    DEBUGMSG(REG_MSGS & ZONE_MISC,
                              (TEXT("UNIMODEM:MdmRegGetValue '%s' was type x%X, not x%X.\r\n"),
                               szValueName, dwTmpType, dwType));
                    dwRetCode = ERROR_NO_TOKEN;  // return an error
                }
            }
            
            // be sure to close the key if we opened it
            if( szKeyName != NULL )
            {
                RegCloseKey( hKey );
            }
        
        }
        else
        {
            // we couldn't open a default key, return error  
            DEBUGMSG(REG_MSGS & ZONE_ERROR,
                      (TEXT("UNIMODEM:MdmRegGetValue Unable to open default key for %s rc=%d.\r\n"),
                       szKeyName, dwRetCode));
            
        }
    }

    return dwRetCode;
}   // MdmRegGetValue


#define DEVNAME_LEN 128
#define DEFAULT_EXTMODEM_ORDER -1

typedef struct _EXTMODEM_ENTRY {
    struct _EXTMODEM_ENTRY * pNext;
    DWORD                    dwOrder;
    WCHAR                    szDevName[DEVNAME_LEN];
} EXTMODEM_ENTRY, * PEXTMODEM_ENTRY;


//
// This routine walks the [HKEY_LOCAL_MACHINE\ExtModems] key and
// adds any of the external modems which it finds there.
//
VOID
EnumExternModems( )
{
    HKEY  hExtKey;    // hKey for HKEY_LOCAL_MACHINE\ExtModms
    HKEY  hExtMdm;
    DWORD RegEnum;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;
    WCHAR DevName[DEVNAME_LEN];   // Name i.e. HayesCompatible
    WCHAR DevPath[DEVNAME_LEN*2]; // Path i.e. ExtModems\HayesCompat
    WCHAR *szExtName = TEXT("ExtModems");
    LPWSTR pEnd;
    PEXTMODEM_ENTRY pExtList;
    PEXTMODEM_ENTRY pExtEntry;
    PEXTMODEM_ENTRY pCur;
    PEXTMODEM_ENTRY pPrev;

    pExtList = NULL;

    //
    // Open the external modem key if it exists.
    //
    status = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                           szExtName,
                           0,
                           KEY_READ,
                           &hExtKey);
    if (status)
    {
        // If we can't open the key, we can't enumerate any modems
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                 (TEXT("UNIMODEM:EnumExternModems RegOpenKey %s returned %d.\r\n"),
                  szExtName, status));
        return;
    }

    //
    // Build list of external modems and keep it sorted by order.
    // We keep enumerating until we eget an error from regenumkey indicating that
    // we have seen the last value.
    //
    RegEnum = 0;  // Start with the first device.
    while (TRUE)
    {
        ValLen = sizeof(DevName) / sizeof(DevName[0]);
        status = RegEnumKeyEx(
            hExtKey,
            RegEnum,
            DevName,
            &ValLen,
            NULL,
            NULL,
            NULL,
            NULL);

        if (status != ERROR_SUCCESS)
        {
            //
            // Assume all keys have been enumerated, so we are done
            //
            DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                     (TEXT("UNIMODEM:EnumExternModems RegEnumKeyEx(%d) returned %d\r\n"),
                      RegEnum, status));
            break; 
        }
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                 (TEXT("UNIMODEM:EnumExternModems RegEnum %d = %s\r\n"), RegEnum, DevName));

        pExtEntry = TSPIAlloc(sizeof(EXTMODEM_ENTRY));
        if (NULL == pExtEntry) {
            DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                     (TEXT("UNIMODEM:EnumExternModems No memory for %s\r\n"), DevName));
            break; 
        }

        wcscpy(pExtEntry->szDevName, DevName);
        pExtEntry->dwOrder = DEFAULT_EXTMODEM_ORDER;    // Assume end of list if no order specified

        //
        // Read order value if it exists
        //
        status = RegOpenKeyEx( hExtKey,
                               DevName,
                               0,
                               KEY_READ,
                               &hExtMdm);
        if (ERROR_SUCCESS == status) {
            ValLen = sizeof(pExtEntry->dwOrder);
            RegQueryValueEx(hExtMdm,
                            L"Order",
                            NULL,
                            &ValType,
                            (LPBYTE)&pExtEntry->dwOrder,
                            &ValLen
                            );
            RegCloseKey(hExtMdm);
        }

        //
        // Insert in sorted order
        //
        if (pExtList == NULL) {
            pExtList = pExtEntry;               // head of empty list
            pExtEntry->pNext = NULL;
        } else {
            pCur = pPrev = pExtList;

            while (pCur) {
                if (pExtEntry->dwOrder < pCur->dwOrder) {
                    pExtEntry->pNext = pCur;
                    if (pCur == pPrev) {
                        pExtList = pExtEntry;   // insert at head
                        break;
                    }
                    pPrev->pNext = pExtEntry;   // insert in middle
                    break;
                }
                pPrev = pCur;
                pCur = pCur->pNext;
            }

            if (NULL == pCur) {
                pExtEntry->pNext = NULL;        // insert at end
                pPrev->pNext = pExtEntry;
            }
        }

#ifdef DEBUG
        pExtEntry = pExtList;
        while (pExtEntry) {
            DEBUGMSG(1, (L"UNIMODEM: %s: order = 0x%x\n", pExtEntry->szDevName, pExtEntry->dwOrder));
            pExtEntry = pExtEntry->pNext;
        }
#endif
        
        RegEnum++;
    }   // while (more device keys to try)

    RegCloseKey(hExtKey);

    if (pExtList) {
        // OK, if we just create a Registry path for this device then
        // we can use CreateLineDev to do everything else for us.
        wcscpy( DevPath, szExtName);
        wcscat( DevPath, TEXT("\\"));        
        pEnd = DevPath + wcslen(DevPath);
    }

    //
    // Walk sorted list of external modems and create them in order.
    //
    while (pExtList) {
        pExtEntry = pExtList;
        pExtList = pExtEntry->pNext;

        wcscpy(pEnd, pExtEntry->szDevName);
        createLineDev(NULL,     // LPCWSTR hActiveKey, NULL = no associated active device
                      DevPath,  // LPCWSTR lpszDevPath
                      NULL);    // LPCWSTR lpszDeviceName, NULL = read it for me
        TSPIFree(pExtEntry);
    }
}   // EnumExternModems



