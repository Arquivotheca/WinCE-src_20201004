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

#include "sddetect.h"

LPTSTR sdHostPrefixTable[] =
{
    TEXT("SHC"),
    TEXT("SDH"),
    TEXT("HSC"),
    TEXT("NSD")
};
DWORD g_deviceindex[10];

BOOL DriverLoading(LPCTSTR pszRegPath, LPCTSTR pszPrefix)
{
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

    HANDLE hSearchDevice = NULL;    //handle to device if found
    HANDLE hHandle = NULL;
    BOOL rVal = false; //return value

    // require to load device
    hHandle = ActivateDeviceEx(pszRegPath, NULL , 0, NULL);
    if (hHandle == INVALID_HANDLE_VALUE)
        return false;

    hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, pszPrefix, &di);

    // device is already loaded
      HANDLE hParentDevice = di.hParentDevice;
      DEVMGR_DEVICE_INFORMATION parent_di;
      memset(&parent_di, 0, sizeof(parent_di));
      parent_di.dwSize = sizeof(parent_di);
      if(GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di))
      {
         //open a handle to the parent device
          LPCTSTR pszParentBusName = parent_di.szBusName;

          ce::auto_hfile hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
          if (!hfParent.valid()) {
              NKDbgPrintfW(_T("CreateFile('%s') failed %d\r\n"), pszParentBusName, GetLastError());
              rVal = true;
          }
          else {
              LPCTSTR pszBusName = di.szBusName;
              rVal = DeviceIoControl(hfParent, IOCTL_BUS_ACTIVATE_CHILD,
                                                  (PVOID) pszBusName, (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), NULL, 0, NULL, NULL);
              if(!rVal) {
                  DEBUGMSG(TRUE, (_T("%s on IOCTL_BUS_ACTIVATE_CHILD failed %d\r\n"), pszBusName, GetLastError()));
              }
          }
     }

    FindClose(hSearchDevice);
    return rVal;
}

BOOL LoadingDriver(DEVMGR_DEVICE_INFORMATION& di)
{
      BOOL rVal = FALSE; //return value

      HANDLE hParentDevice = di.hParentDevice;
      DEVMGR_DEVICE_INFORMATION parent_di;
      memset(&parent_di, 0, sizeof(parent_di));
      parent_di.dwSize = sizeof(parent_di);
      if(GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di))
      {
            //open a handle to the parent device
            LPCTSTR pszParentBusName = parent_di.szBusName;

            ce::auto_hfile hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (!hfParent.valid()) {
                NKDbgPrintfW(_T("CreateFile('%s') failed %d\r\n"), pszParentBusName, GetLastError());
                rVal = TRUE;
            }
            else
            {
                bool fUnload = false;

                LPCTSTR pszBusName = di.szBusName;
                DEBUGMSG(TRUE,(_T("*********Loading driver : %s*********"),di.szLegacyName));
                rVal = DeviceIoControl(hfParent, fUnload ? IOCTL_BUS_DEACTIVATE_CHILD : IOCTL_BUS_ACTIVATE_CHILD,
                      (PVOID) pszBusName, (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), NULL, 0, NULL, NULL);
                if(!rVal) {
                    DEBUGMSG(TRUE, (_T("%s on '%s' failed %d\r\n"),
                    fUnload ? L"IOCTL_BUS_DEACTIVATE_CHILD" : L"IOCTL_BUS_ACTIVATE_CHILD",
                    pszBusName, GetLastError()));
                }
            }
      }
      return rVal;
}

BOOL UnloadingDriver(LPCTSTR pszSearchPrefix, DEVMGR_DEVICE_INFORMATION& di)
{
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

    HANDLE hSearchDevice = NULL;    //handle to device if found
    BOOL rVal = false; //return value

    hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, pszSearchPrefix, &di);

    if(hSearchDevice == NULL) {
        rVal = true;
    }
    else {
        HANDLE hParentDevice = di.hParentDevice;
        DEVMGR_DEVICE_INFORMATION parent_di;
        memset(&parent_di, 0, sizeof(parent_di));
        parent_di.dwSize = sizeof(parent_di);
        if(!GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di))  {
            rVal = true;
        }
       else {
           //open a handle to the parent device
            LPCTSTR pszParentBusName = parent_di.szBusName;

            ce::auto_hfile hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (!hfParent.valid()) {
                DEBUGMSG(TRUE, (_T("CreateFile('%s') failed %d\r\n"), pszParentBusName, GetLastError()));
                rVal = true;
            }
            else {
                LPCTSTR pszBusName = di.szBusName;
                DEBUGMSG(TRUE, (_T("*********Unloading driver : %s*********"),di.szLegacyName));
                rVal = DeviceIoControl(hfParent, IOCTL_BUS_DEACTIVATE_CHILD,
                                                    (PVOID) pszBusName, (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), NULL, 0, NULL, NULL);
                if(!rVal) {
                    DEBUGMSG(TRUE, (_T("%s on IOCTL_BUS_DEACTIVATE_CHILD failed %d\r\n"), pszBusName, GetLastError()));
                }
            }
       }
    }
    FindClose(hSearchDevice);
    return rVal;
}

BOOL UnloadSDBus(DEVMGR_DEVICE_INFORMATION& di)
{
    BOOL bSuccess = FALSE;

    // then load the SD Bus Driver
      DEVMGR_DEVICE_INFORMATION diSDHC;

    // look for the # of host in the system
    DWORD dwNumOfHost = NumOfSDHostController();

    // host controller in the Active key is starting at SDC1, SDC2 ...
    for (DWORD i = 0; i < dwNumOfHost; i++)
        bSuccess &= UnloadSDHostController(diSDHC, g_deviceindex[i]);

    TCHAR szSearchString[10];
    StringCchPrintf(szSearchString, 10, TEXT("%s*"), SDDRV_PREFIX);

    bSuccess = UnloadingDriver(szSearchString, di);

    return bSuccess;
}

DWORD NumOfSDHostController()
{
    DWORD dwNum = 0;

    DEVMGR_DEVICE_INFORMATION tmpDi;
    HKEY hKey;
    DWORD dwSize=sizeof(DWORD);
    DWORD dwType=REG_DWORD;
    HANDLE hSearchDevice = INVALID_HANDLE_VALUE;

    TCHAR szSearchString[10];

    for (int c = 0; c <  _countof(sdHostPrefixTable); c++ )
    {
        memset(&tmpDi, 0, sizeof(tmpDi));
        tmpDi.dwSize = sizeof(tmpDi);

        StringCchPrintf(szSearchString, 10, TEXT("%s*"), sdHostPrefixTable[c]);

        // Search for the device with the Mobile Host Prefix
        hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, szSearchString, &tmpDi);

        if(hSearchDevice != INVALID_HANDLE_VALUE) {
            if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,tmpDi.szDeviceKey,0,0,&hKey) == ERROR_SUCCESS)
            {
                if(RegQueryValueEx(hKey, TEXT("Index"),NULL,&dwType,(LPBYTE)&g_deviceindex[dwNum], &dwSize)!=ERROR_SUCCESS)
                {
                    //Index provided by device manager
                    g_deviceindex[dwNum] = 1;
                }

                RegCloseKey(hKey);
                dwNum++;
            }
            while (FindNextDevice(hSearchDevice, &tmpDi))
            {
                if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,tmpDi.szDeviceKey,0,0,&hKey) == ERROR_SUCCESS)
                {
                    if(RegQueryValueEx(hKey, TEXT("Index"),NULL,&dwType,(LPBYTE)&g_deviceindex[dwNum], &dwSize)!=ERROR_SUCCESS)
                    {
                        g_deviceindex[dwNum] = g_deviceindex[dwNum - 1] + 1;
                    }
                    RegCloseKey(hKey);
                    dwNum++;
                }
            }
        }

        FindClose(hSearchDevice);
        hSearchDevice = INVALID_HANDLE_VALUE;
    }
    return dwNum;
}

BOOL UnloadSDHostController(DEVMGR_DEVICE_INFORMATION& di, DWORD dwHostIndex)
{
    DEVMGR_DEVICE_INFORMATION tmpDi;
    HANDLE hSearchDevice = INVALID_HANDLE_VALUE;

    TCHAR szSearchString[10];

    for (int c = 0; c <  _countof(sdHostPrefixTable); c++ )
    {
        memset(&tmpDi, 0, sizeof(tmpDi));
        tmpDi.dwSize = sizeof(tmpDi);

        StringCchPrintf(szSearchString, 10, TEXT("%s%d*"), sdHostPrefixTable[c], dwHostIndex);

        // Search for the device with the Mobile Host Prefix
        hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, szSearchString, &tmpDi);

        if(hSearchDevice != INVALID_HANDLE_VALUE) {
            FindClose(hSearchDevice);
            return UnloadingDriver(szSearchString, di);
        }
    }
    return FALSE;
}


BOOL isSDMediaPresent(SDCARD_DEVICE_TYPE devType, BOOL bHighCap, DWORD& dwHostIndex, DWORD& dwSlotIndex)
{
    HANDLE hBusDriver;

    hBusDriver = NULL;
    DWORD  numberOfSlots = 0;  // number of slots
    DWORD  length;             // IOCTL return length
    PBUS_DRIVER_SLOT_INFO_EX   pSlotArray = NULL;  // slot array
    BOOL bFoundMatchDevice = FALSE;

    hBusDriver = CreateFile(SDCARD_BUS_DRIVER_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,  0,  NULL, OPEN_EXISTING, 0, NULL);
    if (hBusDriver == INVALID_HANDLE_VALUE)    {
        DEBUGMSG(TRUE,  (_T("Failed to open handle to SD Bus Driver (%u) \n"), GetLastError()));
        return FALSE;
    }
    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_SLOT_COUNT, NULL, 0, &numberOfSlots, sizeof(DWORD), &length, NULL))  {
            DEBUGMSG(TRUE, (_T("IOCTL Failed (%u) \n"), GetLastError()));
            goto END;
    }
    if (0 == numberOfSlots)   {   //this should never happen. If there are no slots, there should be no bus driver anymore
        DEBUGMSG(TRUE, (_T("No host controllers \n")));
        goto END;

    }
    DEBUGMSG(TRUE, (_T("# of Slot found: %d \n"), numberOfSlots));

    // allocate the slot array
    pSlotArray = (PBUS_DRIVER_SLOT_INFO_EX)LocalAlloc(LPTR, numberOfSlots * (sizeof(BUS_DRIVER_SLOT_INFO_EX)));
    if (NULL == pSlotArray)   {
        DEBUGMSG(TRUE, (_T("Failed to allocate slot array \n")));
        goto END;
    }

    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_SLOT_INFO_EX, NULL, 0, pSlotArray,
                         numberOfSlots * (sizeof(BUS_DRIVER_SLOT_INFO_EX)), &length, NULL)) {
        DEBUGMSG(TRUE, ( _T("IOCTL Failed (%u)"), GetLastError()));
        goto END;
    }

    for (DWORD  i = 0; i < numberOfSlots; i++)    {
        // a card is present
        if (pSlotArray[i].CardPresent)     {
              // found device type
              if (pSlotArray[i].DeviceType == devType) {
                    // if it is SD Memory / MMC - check for the high capacity bit
                    switch (pSlotArray[i].DeviceType) {
                        case Device_SD_Memory:
                        case Device_MMC:
                            bFoundMatchDevice = ((pSlotArray[i].CardInterfaceEx.InterfaceModeEx.bit.sdHighCapacity > 0) == bHighCap);
                            break;
                        case Device_SD_IO:
                            bFoundMatchDevice = TRUE;
                            break;
                    }
            }
            if (bFoundMatchDevice) {
                dwHostIndex = pSlotArray[i].HostIndex;
                dwSlotIndex = pSlotArray[i].SlotIndex;
                break;
            }
        }
    }

END:
    if (hBusDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(hBusDriver);
    }

    if (NULL != pSlotArray)     {
        LocalFree(pSlotArray);
    }
    return bFoundMatchDevice;

}

VOID displayCardInterface(SD_CARD_INTERFACE_EX *pCardInterface)
{
    if( pCardInterface )
    {
        DEBUGMSG(TRUE, (_T("     Clock Rate:  %d"), pCardInterface->ClockRate));
        DEBUGMSG(TRUE, (_T("     SD 4 bit mode:  %s"), pCardInterface->InterfaceModeEx.bit.sd4Bit ? _T("Yes") : _T("No")));
        DEBUGMSG(TRUE, (_T("     MMC 8 bit mode:  %s"), pCardInterface->InterfaceModeEx.bit.mmc8Bit ? _T("Yes") : _T("No")));
        DEBUGMSG(TRUE, (_T("     SD high Speed:  %s"), pCardInterface->InterfaceModeEx.bit.sdHighSpeed ? _T("Yes") : _T("No")));
        DEBUGMSG(TRUE, (_T("     SD Write Protected:  %s"), pCardInterface->InterfaceModeEx.bit.sdWriteProtected ? _T("Yes") : _T("No")));
        DEBUGMSG(TRUE, (_T("     SD High Capacity:  %s"), pCardInterface->InterfaceModeEx.bit.sdHighCapacity ? _T("Yes") : _T("No")));
    }
}

VOID displaySlotInfo(BUS_DRIVER_SLOT_INFO_EX *pSlotInfo)
{
    if( !pSlotInfo )
        return;

    DEBUGMSG(TRUE, ( _T("Slot %d; Host %d"), pSlotInfo->HostIndex, pSlotInfo->SlotIndex));
    DEBUGMSG(TRUE, (_T("------------------------")));

    if (pSlotInfo->CardPresent) {
        DEBUGMSG(TRUE, (_T("     Description:  %s"), pSlotInfo->Description));
        switch (pSlotInfo->DeviceType)
        {
            case Device_Unknown:
                DEBUGMSG(TRUE, (_T("     Device Type = Device_Unknown")));
                break;
            case Device_MMC:
                DEBUGMSG(TRUE, (_T("     Device Type = Device_MMC")));
                break;
            case Device_SD_Memory:
                DEBUGMSG(TRUE, (_T("     Device Type = Device_SD_Memory")));
                break;
            case Device_SD_IO :
                DEBUGMSG(TRUE, (_T("     Device Type = Device_SD_IO ")));
                break;
            case Device_SD_Combo:
                DEBUGMSG(TRUE, (_T("     Device Type = Device_SD_Combo")));
                break;
        }

        displayCardInterface(&(pSlotInfo->CardInterfaceEx));
    }
}

VOID displayAllSlot() {
    HANDLE hBusDriver;

    hBusDriver = NULL;
    DWORD  numberOfSlots = 0;  // number of slots
    DWORD dwVersion = 0;

    DWORD  length;             // IOCTL return length
    PBUS_DRIVER_SLOT_INFO_EX   pSlotArray = NULL;  // slot array

    hBusDriver = CreateFile(SDCARD_BUS_DRIVER_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,  0,  NULL, OPEN_EXISTING, 0, NULL);
    if (hBusDriver == INVALID_HANDLE_VALUE)    {
        DEBUGMSG(TRUE,  (_T("Failed to open handle to SD Bus Driver (%u) \n"), GetLastError()));
        return;
    }
// Get Bus version
    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_BUS_VERSION, NULL, 0, &dwVersion, sizeof(DWORD), &length, NULL))  {
           DEBUGMSG(TRUE, (_T("IOCTL Failed (%u) \n"), GetLastError()));
           goto END;
    }
    else {
               DEBUGMSG(TRUE, (_T("SD Bus Version:  0x%x \n"), dwVersion));
    }

    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_VERSION, NULL, 0, &dwVersion, sizeof(DWORD), &length, NULL))  {
           DEBUGMSG(TRUE, (_T("IOCTL Failed (%u) \n"), GetLastError()));
           goto END;
    }
    else {
               DEBUGMSG(TRUE, (_T("OS Version:  0x%x \n"), dwVersion));
    }

// Get # of slot
    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_SLOT_COUNT, NULL, 0, &numberOfSlots, sizeof(DWORD), &length, NULL))  {
            DEBUGMSG(TRUE, (_T("IOCTL Failed (%u) \n"), GetLastError()));
            goto END;
    }
    if (0 == numberOfSlots)   {   //this should never happen. If there are no slots, there should be no bus driver anymore
        DEBUGMSG(TRUE, (_T("No host controllers \n")));
        goto END;

    }
    DEBUGMSG(TRUE, (_T("# of Slot found: %d \n"), numberOfSlots));

    // allocate the slot array
    pSlotArray = (PBUS_DRIVER_SLOT_INFO_EX)LocalAlloc(LPTR, numberOfSlots * (sizeof(BUS_DRIVER_SLOT_INFO_EX)));
    if (NULL == pSlotArray)   {
        DEBUGMSG(TRUE, (_T("Failed to allocate slot array \n")));
        goto END;
    }

// Get Slot Info
    if (!DeviceIoControl(hBusDriver, IOCTL_SD_BUS_DRIVER_GET_SLOT_INFO_EX, NULL, 0, pSlotArray,
                         numberOfSlots * (sizeof(BUS_DRIVER_SLOT_INFO_EX)), &length, NULL)) {
        DEBUGMSG(TRUE, ( _T("IOCTL Failed (%u)"), GetLastError()));
        goto END;
    }

    for (DWORD  i = 0; i < numberOfSlots; i++)    {
        displaySlotInfo(&(pSlotArray[i]));
    }

END:
    if (hBusDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(hBusDriver);
    }

    if (NULL != pSlotArray)     {
        LocalFree(pSlotArray);
    }
}

BOOL hasSDMemory() {
    DWORD dwHostIndex = 0;
    DWORD dwSlotIndex = 0;
    return isSDMediaPresent(Device_SD_Memory, FALSE /*highCap*/, dwHostIndex, dwSlotIndex);
}

BOOL hasSDHCMemory() {
    DWORD dwHostIndex = 0;
    DWORD dwSlotIndex = 0;
    return isSDMediaPresent(Device_SD_Memory, TRUE /*highCap*/, dwHostIndex, dwSlotIndex);
}

BOOL hasMMC() {
    DWORD dwHostIndex = 0;
    DWORD dwSlotIndex = 0;
    return isSDMediaPresent(Device_MMC, FALSE /*highCap*/, dwHostIndex, dwSlotIndex);
}

BOOL hasMMCHC() {
    DWORD dwHostIndex = 0;
    DWORD dwSlotIndex = 0;
    return isSDMediaPresent(Device_MMC, TRUE /*highCap*/, dwHostIndex, dwSlotIndex);
}

BOOL hasSDIO() {
    DWORD dwHostIndex = 0;
    DWORD dwSlotIndex = 0;
    return isSDMediaPresent(Device_SD_IO, TRUE /*highCap - Ignore for SDIO*/, dwHostIndex, dwSlotIndex);
}

BOOL getFirstDeviceSlotInfo(SDCARD_DEVICE_TYPE devType, BOOL bHighCap, DWORD& dwHostIndex, DWORD& dwSlotIndex)
{
    return isSDMediaPresent(devType, bHighCap, dwHostIndex, dwSlotIndex);
}
