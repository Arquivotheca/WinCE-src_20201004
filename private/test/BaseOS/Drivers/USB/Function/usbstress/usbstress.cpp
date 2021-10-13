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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:
    usbstress.cpp

Abstract:
    USB Client Driver for stress testing USBDriver.

Author:
    davli
    weichen

Modified:
    smoidu

Functions:

Notes:
--*/

#define __THIS_FILE__   TEXT("usbstress.cpp")
#include <windows.h>
#include <usbdi.h>
#include "usbstress.h"
#include "usbserv.h"
#include "UsbClass.h"
#include "resource.h"
#include "pipetest.h"
#include "stateutil.h"
#include "clparse.h"

//globals
PUSB_TDEVICE_LPBKINFO g_pTstDevLpbkInfo[MAX_CLIENTS];
PBOOL g_pTstDevPipeStatus[MAX_CLIENTS];

LPUN_CLICKER_CONNECT g_clickerConnect;
LPUN_CLICKER_DISCONNECT g_clickerDisconnect;

HANDLE hClickerLib;

HINSTANCE g_hInst = NULL;
BOOL g_fTimer = FALSE;

BOOL g_fUnloadDriver = FALSE;    //unload usbstress.dll from usbd.dll if true
BOOL g_fRegUpdated = FALSE;    //registry update only needs to be done once

HANDLE g_hConnectEvent;
HANDLE g_hDisconnectEvent;


DWORD g_RandomSeed = 0;
USB_PARAM g_UsbParam;

USBDriverClass *pUsbDriver = NULL;    //USB Bus APIs

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("USB Logger"), {
                 TEXT("Errors"), TEXT("Warnings"),
                 TEXT("Functions"), TEXT("Detail"),
                 TEXT("Initialization"),
                 TEXT("USB"), TEXT("Disk I/O"), TEXT("Weird"),
                 TEXT("Events"),
                 TEXT("USB_CONTROL"), TEXT("USB_BULK"),
                 TEXT("USB_INTERRUPT"), TEXT("USB_ISOCH"),
                 TEXT("Threads"), TEXT("Undefined"),
                 TEXT("Undefined")},
    INIT_ERR | INIT_WARN | INIT_USB
};
#endif

static TCHAR *USBDriverEntryText[] = {
    {TEXT("USBDeviceAttach")},
    {TEXT("USBInstallDriver")},
    {TEXT("USBUnInstallDriver")},
    NULL
};

static TCHAR *USBDEntryPointsText[] = {
    {TEXT("OpenClientRegistryKey")},
    {TEXT("RegisterClientDriverID")},
    {TEXT("UnRegisterClientDriverID")},
    {TEXT("RegisterClientSettings")},
    {TEXT("UnRegisterClientSettings")},
    {TEXT("GetUSBDVersion")},
    NULL
};

static USBD_DRIVER_ENTRY USBDriverEntry;
static USBD_ENTRY_POINTS USBDEntryPoints;

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo = NULL;
LPSTR g_szDllCmdLine = NULL;

LPUN_CLICKER_INIT       clickInit;
LPUN_CLICKER_DEINIT   clickDeInit;


void USBMSG(DWORD verbosity ,LPWSTR szFormat,...)
{
  if( verbosity < LOG_VERBOSITY_LEVEL)
  {
     va_list va;
        if(g_pKato)
       {
            va_start(va, szFormat);
             g_pKato->LogV(verbosity, szFormat, va);
             va_end(va);
       }
  }
  else
      DEBUGMSG(1,(szFormat));

}


BOOL WINAPI DdlxDLLEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) ;
extern "C" LPCTSTR DdlxGetCmdLine( TPPARAM tpParam );

extern "C" BOOL WINAPI DllMain(HANDLE hDllHandle, ULONG dwReason, LPVOID lpreserved)
{
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DEBUGREGISTER((HINSTANCE) hDllHandle);
            pUsbDriver = new USBDriverClass();
            g_hInst = (HINSTANCE) hDllHandle;
                     g_hConnectEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
                     g_hDisconnectEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

            break;
        case DLL_PROCESS_DETACH:
            delete pUsbDriver;
            pUsbDriver = NULL;
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        default:
            break;
    }
     return DdlxDLLEntry((HINSTANCE)hDllHandle,dwReason,lpreserved) ;
}


//###############################################
//following APIs deal with test driver (usbstress.dll) registration issues
//###############################################

/*
 * @func   BOOL | USBInstallDriver | Install USB client driver.
 * @rdesc  Return TRUE if install succeeds, or FALSE if there is some error.
 * @comm   This function is called by USBD when an unrecognized device
 *         is attached to the USB and the user enters the client driver
 *         DLL name.  It should register a unique client id string with
 *         USBD, and set up any client driver settings.
 * @xref   <f USBUnInstallDriver>
 */
extern "C" BOOL
USBInstallDriver(
    LPCWSTR szDriverLibFile)  // @parm [IN] - Contains client driver DLL name
{
    DEBUGMSG(DBG_USB, (TEXT("Start USBInstallDriver %s "),szDriverLibFile));
    USBDriverClass usbDriver;
    BOOL fRet = FALSE;

    if(usbDriver.IsOk()) {
        USB_DRIVER_SETTINGS DriverSettings;
        DriverSettings.dwCount = sizeof(DriverSettings);

        // Set up our DriverSettings struct to specify how we are to
        // be loaded. The device is NetChip 2280 card


        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = USB_NO_INFO;
        DriverSettings.dwReleaseNumber = USB_NO_INFO;

        DriverSettings.dwDeviceClass = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol = USB_NO_INFO;

        DriverSettings.dwInterfaceClass = 0x0F;      //
        DriverSettings.dwInterfaceSubClass = USB_NO_INFO; // boot device
        DriverSettings.dwInterfaceProtocol = USB_NO_INFO; // mouse



        fRet=usbDriver.RegisterClientDriverID(TEXT("Generic UFN Test Loopback Driver"));
        ASSERT(fRet);
        if(fRet){
            fRet = usbDriver.RegisterClientSettings(szDriverLibFile,
                                TEXT("Generic UFN Test Loopback Driver"), NULL, &DriverSettings);
            ASSERT(fRet);
        }

    }

    DEBUGMSG(DBG_USB, (TEXT("End USBInstallDriver return value = %d "),fRet));
    ASSERT(fRet);
    return fRet;
}

/*
 * @func   BOOL | USBUnInstallDriver | Uninstall USB client driver.
 * @rdesc  Return TRUE if install succeeds, or FALSE if there is some error.
 * @comm   This function can be called by a client driver to deregister itself
 *         with USBD.
 * @xref   <f USBInstallDriver>
 */
extern "C" BOOL USBUnInstallDriver()
{
    BOOL fRet = FALSE;
    USBDriverClass usbDriver;

    if(usbDriver.IsOk()) {
        USB_DRIVER_SETTINGS DriverSettings;

        DriverSettings.dwCount = sizeof(DriverSettings);

        // This structure must be filled out the same as we registered with client driver
        DriverSettings.dwVendorId = 0x045e;
        DriverSettings.dwProductId = 0xffe0;
        DriverSettings.dwReleaseNumber = USB_NO_INFO;

        DriverSettings.dwDeviceClass = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol = USB_NO_INFO;

        DriverSettings.dwInterfaceClass = 0x0;
        DriverSettings.dwInterfaceSubClass = 0x00;    // boot device
        DriverSettings.dwInterfaceProtocol = 0x00;    // mouse

        fRet = usbDriver.UnRegisterClientSettings(TEXT("Generic UFN Test Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("Generic UFN Test Loopback Driver"));

    }

    return fRet;
}

BOOL WINAPI DeviceNotify(LPVOID lpvNotifyParameter, DWORD dwCode, LPDWORD *, LPDWORD *, LPDWORD *, LPDWORD *)
{
    BOOL fReturn = TRUE;
    switch (dwCode) {
        case USB_CLOSE_DEVICE:
            // All processing done in destructor
            if(pUsbDriver)
            {

                UsbClientDrv *pClientDrv = (UsbClientDrv *) lpvNotifyParameter;
                DWORD index = 0;
                for(int i = 0; i < (int) (pUsbDriver->GetArraySize()); i++)
                {
                    UsbClientDrv *pTempDrv = pUsbDriver->GetAt(i);
                    if(pClientDrv == pTempDrv)
                    {
                        index = i;
                        break;
                    }
                }
                fReturn = pUsbDriver->RemoveClientDrv(pClientDrv, FALSE);
                ASSERT(fReturn);

                (*pClientDrv->lpUsbFuncs->lpUnRegisterNotificationRoutine) (pClientDrv->GetUsbHandle(), DeviceNotify, pClientDrv);
                            if(!INVALID_HANDLE(g_hDisconnectEvent))
                {
                               SetEvent(g_hDisconnectEvent);

                            }
                            if(!INVALID_HANDLE(g_hConnectEvent))
                {
                               ResetEvent(g_hConnectEvent);
                   }

                if(g_pTstDevLpbkInfo[index] != NULL)
                {
                    delete g_pTstDevLpbkInfo[index];
                    g_pTstDevLpbkInfo[index] = NULL;
                }

                if(!g_UsbParam.fClicker)
                {
                    delete pClientDrv;
                                   pClientDrv = NULL;
                }


            }
            return TRUE;
    }
    return FALSE;
}

/*
 *  @func   BOOL | USBDeviceAttach | USB device attach routine.
 *  @rdesc  Return TRUE upon success, or FALSE if an error occurs.
 *  @comm   This function is called by USBD when a device is attached
 *          to the USB, and a matching registry key is found off the
 *          LoadClients registry key. The client  should determine whether
 *          the device may be controlled by this driver, and must load
 *          drivers for any uncontrolled interfaces.
 *  @xref   <f FindInterface> <f LoadGenericInterfaceDriver>
 */
extern "C" BOOL USBDeviceAttach(USB_HANDLE hDevice,    // @parm [IN] - USB device handle
                LPCUSB_FUNCS lpUsbFuncs,    // @parm [IN] - Pointer to USBDI function table
                LPCUSB_INTERFACE lpInterface,    // @parm [IN] - If client is being loaded as an interface
                //              driver, contains a pointer to the USB_INTERFACE
                //              structure that contains interface information.
                //              If client is not loaded for a specific interface,
                //              this parameter will be NULL, and the client
                //              may get interface information through <f FindInterface>
                LPCWSTR szUniqueDriverId,    // @parm [IN] - Contains client driver id string
                LPBOOL fAcceptControl,    // @parm [OUT]- Filled in with TRUE if we accept control
                //              of the device, or FALSE if USBD should continue
                //              to try to load client drivers.
                LPCUSB_DRIVER_SETTINGS lpDriverSettings,    // @parm [IN] - Contains pointer to USB_DRIVER_SETTINGS
                //              struct that indicates how we were loaded.
                DWORD dwUnused)    // @parm [IN] - Reserved for use with future versions of USBD
{
    UNREFERENCED_PARAMETER(dwUnused);
    RETAILMSG(1, (TEXT("Device Attach")));
    *fAcceptControl = FALSE;

    if(pUsbDriver == NULL || lpInterface == NULL)
        return FALSE;

    RETAILMSG(1,
         (TEXT
          ("USBStress: DeviceAttach, IF %u, #EP:%u, Class:%u, Sub:%u, Prot:%u\r\n"), lpInterface->Descriptor.bInterfaceNumber, lpInterface->Descriptor.bNumEndpoints, lpInterface->Descriptor.bInterfaceClass, lpInterface->Descriptor.bInterfaceSubClass,
          lpInterface->Descriptor.bInterfaceProtocol));

    UsbClientDrv *pLocalDriverPtr = new UsbClientDrv(hDevice, lpUsbFuncs, lpInterface,
                             szUniqueDriverId,
                             lpDriverSettings);
    ASSERT(pLocalDriverPtr);
    if(pLocalDriverPtr == NULL)
        return FALSE;

    if(!pLocalDriverPtr->Initialization() || !(pUsbDriver->AddClientDrv(pLocalDriverPtr))) {
        delete pLocalDriverPtr;
        pLocalDriverPtr = NULL;
        RETAILMSG(1, (TEXT("Device Attach failed")));
        return FALSE;
    }
    (*lpUsbFuncs->lpRegisterNotificationRoutine) (hDevice, DeviceNotify, pLocalDriverPtr);
    *fAcceptControl = TRUE;

       if(!INVALID_HANDLE(g_hConnectEvent)) {
          SetEvent(g_hConnectEvent);
       }
    RETAILMSG(1, (TEXT("Device Attach success")));


    return TRUE;
}


BOOL USBTestConnect()
{


    if(g_clickerConnect == NULL || INVALID_HANDLE(g_hConnectEvent)) {
        return FALSE;
    }

    if((*g_clickerConnect)()) {
        if(WAIT_OBJECT_0 != WaitForSingleObject(g_hConnectEvent,30*1000)) {
            g_pKato->Log(LOG_FAIL,_T("Could not reconnect within 30 seconds of clicker connect, test will fail.\n"));

          return FALSE;
        }
        g_pKato->Log(LOG_DETAIL,_T("Reconnected after clicker connect.\n"));
    }

    return TRUE;
}

BOOL USBTestDisconnect()
{

    if(g_clickerDisconnect == NULL || INVALID_HANDLE(g_hDisconnectEvent))
    {
        return FALSE;
    }

    if((*g_clickerDisconnect)()) {
        if(WAIT_OBJECT_0 != WaitForSingleObject(g_hDisconnectEvent,30*1000)) {
            g_pKato->Log(LOG_FAIL,_T("Could not disconnect within 30 seconds of clicker disconnect, test will fail.\n"));

         return FALSE;
        }
        g_pKato->Log(LOG_DETAIL,_T("Disconnected after clicker disconnect.\n"));
    }

    return TRUE;
}
//###############################################
// TUX test cases and distribution
//###############################################

TESTPROCAPI LoopTestProc(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UINT uiRand = 0;

    UNREFERENCED_PARAMETER(lpFTE);
    if(uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    USBMSG(LOG_DETAIL, _T("LoopTestProc"));

    LPCTSTR lpCommandLine = DdlxGetCmdLine( tpParam );

    if(lpCommandLine)
        ParseCommandLine(&g_UsbParam,lpCommandLine );
    else
        ParseCommandLine(&g_UsbParam,g_pShellInfo->szDllCmdLine);

    //we are waiting for the connection
    if(pUsbDriver->GetCurAvailDevs() == 0)
    {
        HWND hDiagWnd = ShowDialogBox(TEXT("Connect USB Host device with USB Function device now!"));

           //CETK users: Please ignore following statement, this is only for automated test setup
        int iCnt = 0;

        while(pUsbDriver->GetCurAvailDevs() == 0)
        {
            if(++iCnt == 10)
            {
                    DeleteDialogBox(hDiagWnd);
                    USBMSG(LOG_DETAIL,_T("*********NO device is attached, test exit without running any test cases.*******"));
                    return TPR_SKIP;
            }

            USBMSG(LOG_DETAIL,_T("Connect USB Host device with USB Function device now!"));
            Sleep(2000);
        }
        DeleteDialogBox(hDiagWnd);
    }



         if (pUsbDriver==NULL)
     {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
            return TPR_SKIP;
         }

    DWORD dwRet = TPR_PASS;
       StressTransfer * szTransfer=NULL;

    // DWORD dwStartTime = GetTickCount();
       DWORD dwRunTime = g_UsbParam.dwDuration * 60 * 1000;
       DWORD dwTimeElapsed = 0;

    DWORD dwActiveDevice = g_UsbParam.dwSelectDevice != 0 ? g_UsbParam.dwSelectDevice : 1;
    dwActiveDevice--;

    srand(g_RandomSeed);
       g_pKato->Log(LOG_DETAIL,_T("Using random seed %d\n"),g_RandomSeed);

         g_pKato->Log(LOG_DETAIL,_T("USB Stress would run for %dms\n"),dwRunTime);

       while(dwTimeElapsed < dwRunTime)
      {


            UsbClientDrv *pClientDrv = pUsbDriver->GetAt(dwActiveDevice);
            if(pClientDrv)
        {
                      DWORD dwStatus = 0;

                if(g_pTstDevLpbkInfo[dwActiveDevice] == NULL)
            {
                    g_pTstDevLpbkInfo[dwActiveDevice] = GetTestDeviceLpbkInfo(pClientDrv);
                    if(g_pTstDevLpbkInfo[dwActiveDevice] == NULL || g_pTstDevLpbkInfo[dwActiveDevice]->uNumOfPipePairs == 0)
                {    //failed or no data pipes
                        RETAILMSG(1,(_T("Can not get test device's loopback pairs' information or there's no data loopback pipe pair, test will be skipped for this device!!!")));
                                    return TPR_SKIP;
                    }
                    g_pTstDevPipeStatus[dwActiveDevice] = new BOOL[g_pTstDevLpbkInfo[dwActiveDevice]->uNumOfPipePairs];
                    memset(g_pTstDevPipeStatus[dwActiveDevice], 0x0, g_pTstDevLpbkInfo[dwActiveDevice]->uNumOfPipePairs * sizeof(BOOL));
                }

                RETAILMSG(1, (TEXT("Device retrive from %d"), dwActiveDevice));

            rand_s(&uiRand);
            if(g_UsbParam.dwClients & SERIAL_CLIENT)
                         szTransfer = new StressTransfer(Serial_Transfer,pClientDrv,dwActiveDevice,uiRand,1024);
                      else if(g_UsbParam.dwClients & RNDIS_CLIENT)
                         szTransfer = new StressTransfer(RNDIS_Transfer,pClientDrv,dwActiveDevice,uiRand,1024);
                      else if(g_UsbParam.dwClients & STORAGE_CLIENT)
                         szTransfer = new StressTransfer(Storage_Transfer,pClientDrv,dwActiveDevice,uiRand,1024);
                      else
                         szTransfer = new StressTransfer(Overload_Transfer,pClientDrv,dwActiveDevice,uiRand,1024);
                         szTransfer->ThreadStart();

                      rand_s(&uiRand);
                      if(g_UsbParam.fClicker && uiRand < (g_UsbParam.dwConnectDisconnectProbability * UINT_MAX) / 100)
            {

                  pClientDrv->Lock();

                          if(!USBTestDisconnect())
                 {
                               dwRet = TPR_FAIL;
                               goto EXIT;
                          }

                    if(szTransfer)
                    {


                                 if(!szTransfer->ThreadTerminated(INFINITE))
                                 {
                           g_pKato->Log(LOG_FAIL,_T("Transfer thread failed to exit : TIMEOUT ERROR.\n"));
                     }

                      if(szTransfer->dwLastExit == TPR_FAIL)
                         {
                                     g_pKato->Log(LOG_DETAIL,_T("Transfer failed due to disconnect.\n"));

                                 }

                     delete szTransfer;
                                    szTransfer = NULL;

                     delete pClientDrv;
                     pClientDrv = NULL;

                        }
                if(!USBTestConnect())
                {
                               dwRet = TPR_FAIL;
                               goto EXIT;
                          }

                 dwTimeElapsed+=2000;
                 continue;
                      }

            dwStatus = WaitForSingleObject(szTransfer->hEvent,1000);
            while(dwStatus)
                {
                          if(dwStatus == WAIT_TIMEOUT)
                  {
                              dwTimeElapsed += 1000;
                              if(dwTimeElapsed >= dwRunTime)
                    {
                                RETAILMSG(1,(_T("Success, closing threads and exiting.\n")));
                                dwRet = TPR_PASS;
                                goto EXIT;
                              }
                              if(dwStatus == WAIT_FAILED)
                 {
                                  RETAILMSG(1,(_T("Wait failure. GetLastError() =%d\n"),GetLastError()));
                                  dwRet = TPR_FAIL;
                                  goto EXIT;
                              }
                              if(szTransfer->dwLastExit == TPR_FAIL)
                   {
                                  dwRet = TPR_FAIL;
                                  g_pKato->Log(LOG_FAIL,_T("Transfer failed.\n"));
                                  szTransfer->ThreadStart();
                                  goto EXIT;
                              }
                              else
                  {
                                    ResetEvent(szTransfer->hEvent);
                              }

                          }
                dwStatus = WaitForSingleObject(szTransfer->hEvent,1000);
                        }
            }
            else {
                g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
                return TPR_SKIP;
            }

                   pClientDrv->Unlock();
         g_pKato->Log(LOG_DETAIL,_T("USB Stress time Elapsed %dms\n"),dwTimeElapsed);
        }
EXIT:
   if(szTransfer)
   {

     szTransfer->ThreadTerminated(INFINITE);

         if(szTransfer)
     {
              delete szTransfer;
              szTransfer = NULL;
       }

   }

    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
// TUX Function table
extern "C" {

    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        {TEXT("USB Stress Test"), 0, 0, 0, NULL},
        {TEXT("USB Stress Test"), 1, 0, 1, LoopTestProc},
        {NULL, 0, 0, 0, NULL}
    };
}

////////////////////////////////////////////////////////////////////////////////
extern "C" SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST pBT;
    LPSPS_END_TEST pET;

    switch (uMsg) {
        case SPM_LOAD_DLL:
            // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
            ((LPSPS_LOAD_DLL) spParam)->fUnicode = TRUE;
#endif                // UNICODE

            //initialize Kato
            g_pKato = (CKato *) KatoGetDefaultObject();
            if(g_pKato == NULL)
                return SPR_NOT_HANDLED;
            KatoDebug(TRUE, KATO_MAX_VERBOSITY, KATO_MAX_VERBOSITY, KATO_MAX_LEVEL);
            g_pKato->Log(LOG_COMMENT, TEXT("g_pKato Loaded"));

            //reset test device info
            memset(g_pTstDevLpbkInfo, 0, sizeof(PUSB_TDEVICE_LPBKINFO));
            //update registry if needed
            if(g_fRegUpdated == FALSE && pUsbDriver->GetCurAvailDevs() == 0) {
                RETAILMSG(1,(_T("Please stand by, installing usbstress.dll as the test driver...")));
                if(RegLoadUSBTestDll(TEXT("usbstress.dll"), FALSE) == FALSE) {
                    RETAILMSG(1,(_T("can not install usb test driver,  make sure it does exist!")));
                    return TPR_NOT_HANDLED;
                }
                g_fRegUpdated = TRUE;
            }
            Sleep(2000);
            break;

        case SPM_UNLOAD_DLL:
            for(int i = 0; i < 4; i++) {
                if(g_pTstDevLpbkInfo[i] != NULL) {
                    delete g_pTstDevLpbkInfo[i];
                    g_pTstDevLpbkInfo[i] = NULL;
                }
            }
            if(g_fUnloadDriver == TRUE)
                RegLoadUSBTestDll(TEXT("usbstress.dll"), TRUE);

                    //DeInit the Clicker
                    if( clickDeInit)
                clickDeInit();

                    //Free Clicker Library
              if(hClickerLib)
                     FreeLibrary((HMODULE)hClickerLib);

                    break;

        case SPM_SHELL_INFO:
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO) spParam;

            break;

        case SPM_REGISTER:
            ((LPSPS_REGISTER) spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else                // UNICODE
            return SPR_HANDLED;
#endif                // UNICODE

        case SPM_START_SCRIPT:
            break;

        case SPM_STOP_SCRIPT:
            break;

        case SPM_BEGIN_GROUP:
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: USBSTRESS.DLL"));
            break;

        case SPM_END_GROUP:
            g_pKato->EndLevel(TEXT("END GROUP: USBSTRESS.DLL"));
            break;

        case SPM_BEGIN_TEST:
            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST) spParam;
            g_RandomSeed = pBT->dwRandomSeed;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"), pBT->lpFTE->lpDescription, pBT->dwThreadCount, pBT->dwRandomSeed);
            break;

        case SPM_END_TEST:
            // End our logging level.
            pET = (LPSPS_END_TEST) spParam;
            g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                      pET->lpFTE->lpDescription, pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") : pET->dwResult == TPR_PASS ? TEXT("PASSED") : pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"), pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
            break;

        case SPM_EXCEPTION:
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            break;

        default:
            return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

TCHAR seps[] = TEXT(" ,\t\n");
TCHAR switches[] = TEXT("rsmotpcl?");

BOOL ParseCommandLine(LPUSB_PARAM lpUsbParam, LPCTSTR lpCommandLine)
{
    if(NULL == lpUsbParam || NULL == lpCommandLine)
        return FALSE;

    memset(lpUsbParam, 0, sizeof(USB_PARAM));

    DWORD dwSize = _tcslen(lpCommandLine) + 1 ;

    TCHAR* szLocalCommandLine = (TCHAR*)malloc(sizeof(TCHAR)*dwSize);

    TCHAR *token;
    TCHAR *pswitch;
    BOOL seenClient = FALSE;

    lpUsbParam->dwDuration = DEFAULT_TEST_TIME;

  if(!SUCCEEDED(StringCchCopy(szLocalCommandLine,dwSize,lpCommandLine)))
         return FALSE;

    TCHAR *ctex;
    token = _tcstok_s(szLocalCommandLine, seps, &ctex);
    while(token != NULL) {
        if(token[0] == TEXT('-')) {
            pswitch = _tcschr(switches, token[1]);
            if(pswitch)
            {
                switch (*pswitch)
                {
                    case _T('l'):
                    case _T('L'):
                    {
                        g_UsbParam.fClicker = FALSE;

                            token = _tcstok_s(NULL, seps, &ctex);
                        RETAILMSG(1, (_T("Clicker dll %s\n"), token));

                                          hClickerLib = LoadLibrary(token);

                                          if(hClickerLib == NULL) {
                                              RETAILMSG(1,(_T("Could not open clicker library %s, aborting.\n")));
                                              GetLastError();
                            continue;
                                          }

                                          g_clickerConnect = GetProcAddress((HMODULE)hClickerLib, CLICKER_CONNECT_FN);
                                          if(g_clickerConnect == NULL) {
                                              RETAILMSG(1,(_T("Could not find connect function %s, aborting.\n"),CLICKER_CONNECT_FN));
                                              GetLastError();

                            continue;
                                          }

                                          g_clickerDisconnect = GetProcAddress((HMODULE)hClickerLib, CLICKER_DISCONNECT_FN);
                                          if(g_clickerDisconnect == NULL) {
                                              RETAILMSG(1,(_T("Could not find disconnect function %s, aborting.\n"),CLICKER_DISCONNECT_FN));
                                              GetLastError();

                            continue;
                                          }

                                          clickInit = (LPUN_CLICKER_INIT)GetProcAddress((HMODULE)hClickerLib, CLICKER_INIT_FN);
                                          if(clickInit == NULL) {
                                              RETAILMSG(1,(_T("Could not find init function %s, aborting.\n"),CLICKER_INIT_FN));
                                              GetLastError();

                            continue;
                                          }


                                          clickDeInit = (LPUN_CLICKER_DEINIT)GetProcAddress((HMODULE)hClickerLib, CLICKER_DEINIT_FN);
                                          if(clickInit == NULL) {
                                              RETAILMSG(1,(_T("Could not find DeInit function %s, aborting.\n"),CLICKER_DEINIT_FN));
                                              GetLastError();

                            continue;
                                          }

                        token = _tcstok_s(NULL, seps, &ctex);
                        _stscanf_s(token, TEXT("%i"), &lpUsbParam->dwConnectDisconnectProbability);
                        RETAILMSG(1, (_T("Disconnect probability %d "), lpUsbParam->dwConnectDisconnectProbability));

                                          if(lpUsbParam->dwConnectDisconnectProbability <= 0 || lpUsbParam->dwConnectDisconnectProbability >= 100) {
                                              RETAILMSG(1,(_T("Invalid connect disconnect probability, must be > 0 and < 100.\n")));
                                              continue;
                                          }

                        token = _tcstok_s(NULL, seps, &ctex);

                                          if(!clickInit(token))
                        {
                                              RETAILMSG(1,(_T("Could not properly initialize clicker library with params [%s], aborting!\n"), token));
                            clickDeInit();
                                              continue;
                                          }

                               g_UsbParam.fClicker = TRUE;
                    }
                                          break;
                    case _T('r'):
                    case _T('R'):
                        seenClient = TRUE;
                        RETAILMSG(1, (_T("Setting RNDIS Client")));
                        lpUsbParam->dwClients = RNDIS_CLIENT;
                        break;
                    case _T('m'):
                    case _T('M'):
                        seenClient = TRUE;
                        RETAILMSG(1, (_T("Setting MASS STORAGE Client")));
                        lpUsbParam->dwClients = STORAGE_CLIENT;
                        break;
                    case _T('s'):
                    case _T('S'):
                        seenClient = TRUE;
                        RETAILMSG(1, (_T("Setting SERIAL Client")));
                        lpUsbParam->dwClients = SERIAL_CLIENT;
                        break;
                    case _T('o'):
                    case _T('O'):
                        seenClient = TRUE;
                        RETAILMSG(1, (_T("Setting OVERLOAD Client")));
                        lpUsbParam->dwClients = OVERLOAD_CLIENT;
                        break;
                    case _T('t'):
                    case _T('T'):
                        if(_tcslen(token) > 2)
                        {
                            _stscanf_s(token, TEXT("-t%i"), &lpUsbParam->dwDuration);
                            RETAILMSG(1, (_T("Time to Run %d "), lpUsbParam->dwDuration));
                        }
                        else
                        {
                            token = _tcstok_s(NULL, seps, &ctex);
                            _stscanf_s(token, TEXT("%i"), &lpUsbParam->dwDuration);
                            RETAILMSG(1, (_T("Time to Run %d "), lpUsbParam->dwDuration));
                        }

                                          if(lpUsbParam->dwDuration <= 0)
                        {
                                              RETAILMSG(1,(_T("Invalid duration.\n")));
                                          }
                        break;
                    case _T('?'):
                        RETAILMSG(1, (_T("Usage -")));
                        RETAILMSG(1, (_T("\tClient Types")));
                        RETAILMSG(1, (_T("\t\t-r                         RNDIS Client (Bulk In & Out / Interrupt / Control)")));
                        RETAILMSG(1, (_T("\t\t-s                        Serial Client (Bulk In & Out / Control)")));
                        RETAILMSG(1, (_T("\t\t-m                         Mass Storage  Client (Bulk In & Out / Control)")));
                        RETAILMSG(1, (_T("\t\t-v                         Video capture  Client (Isoc / Control)")));
                        RETAILMSG(1, (_T("\t\t-o (default)                       Overload client, (all pipes transfer)")));
                                          RETAILMSG(1, (_T("\t\t-l [clickerdll] [probability] [COMx:]    Use connect/disconnect functionality from [clickerdll] with disconnect probability of X/100")));
                        RETAILMSG(1, (_T("")));
                        RETAILMSG(1, (_T("\t\t-t [minutes]                Duration to run testing for.  Default runs for 5 minutes.")));
                        return FALSE;

                        break;

                    default:
                        RETAILMSG(1, (_T("Unknown command line switch \"%s\""), token));
                        break;
                }
            } else
                RETAILMSG(1, (_T("Unknown switch \"%s\""), token));
        } else
            RETAILMSG(1, (_T("Unknown parameter \"%s\""), token));
        token = _tcstok_s(NULL, seps, &ctex);
    }

    if(!seenClient)
        lpUsbParam->dwClients = OVERLOAD_CLIENT;

    return TRUE;
}


//###############################################
//following APIs are support functions for test driver setup
//###############################################

PUSB_TDEVICE_LPBKINFO
GetTestDeviceLpbkInfo(UsbClientDrv * pDriverPtr)
{
    if(pDriverPtr == NULL)
        return FALSE;
    PUSB_TDEVICE_LPBKINFO pTDLpbkInfo = NULL;
    USB_TDEVICE_REQUEST utdr;

    RETAILMSG(1,(TEXT("Get test device's information about loopback pairs")));

    utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
    utdr.bRequest = TEST_REQUEST_PAIRNUM;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(UCHAR);

    UCHAR uNumofPairs = 0;
    //issue command to device -- this time get number of pairs
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)&uNumofPairs, NULL) == FALSE){
        RETAILMSG(1,(TEXT("get number of loopback pairs on test device failed\r\n")));
        return NULL;
    }

    if(uNumofPairs == 0){
        RETAILMSG(1,(TEXT("No loopback pairs avaliable on this test device!!!\r\n")));
        return NULL;
    }

    USHORT uSize = sizeof(USB_TDEVICE_LPBKINFO) + sizeof(USB_TDEVICE_LPBKPAIR)*(uNumofPairs-1);
    pTDLpbkInfo = NULL;
    //allocate
    pTDLpbkInfo = (PUSB_TDEVICE_LPBKINFO) new BYTE[uSize];
    if(pTDLpbkInfo == NULL){

        RETAILMSG(1,(_T("Out of memory!")));
        return NULL;
    }
    else
        memset(pTDLpbkInfo, 0, uSize);

    //issue command to device -- this time get pair info
    utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
    utdr.bRequest = TEST_REQUEST_LPBKINFO;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = uSize;
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)pTDLpbkInfo, NULL) == FALSE){
        RETAILMSG(1,(TEXT("get info of loopback pairs on test device failed\r\n")));
        delete[]  (PBYTE)pTDLpbkInfo;
        return NULL;
    }

    return pTDLpbkInfo;

}

static const TCHAR gcszThisFile[] = { TEXT("USBSTRESS, USBSTRESS.CPP") };

BOOL RegLoadUSBTestDll(LPTSTR szDllName, BOOL bUnload)
{
    BOOL fRet = FALSE;
    BOOL fException;
    DWORD dwExceptionCode = 0;

    /*
     *   Make sure that all the required entry points are present in the USBD driver
     */
    LoadDllGetAddress(TEXT("USBD.DLL"), USBDEntryPointsText, (LPDLL_ENTRY_POINTS) & USBDEntryPoints);
    UnloadDll((LPDLL_ENTRY_POINTS) & USBDEntryPoints);

    /*
     *   Make sure that all the required entry points are present in the USB client driver
     */
    fRet = LoadDllGetAddress(szDllName, USBDriverEntryText, (LPDLL_ENTRY_POINTS) & USBDriverEntry);
    if(!fRet)
        return FALSE;


    if(bUnload) {
        Log(TEXT("%s: UnInstalling library \"%s\".\r\n"), gcszThisFile, szDllName);
        __try {
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpUnInstallDriver) ();
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if(fException) {
            Log(TEXT("%s: USB Driver UnInstallation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"), gcszThisFile, dwExceptionCode, GetLastError());
        }
        if(!fRet)
            Fail(TEXT("%s: UnInstalling USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName);
        else
            Log(TEXT("%s: UnInstalling USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName);
    } else {
        Log(TEXT("%s: Installing library \"%s\".\r\n"), gcszThisFile, szDllName);
        __try {
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpInstallDriver) (szDllName);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if(fException) {
            Fail(TEXT("%s: USB Driver Installation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"), gcszThisFile, dwExceptionCode, GetLastError());
        }
        if(!fRet)
            Fail(TEXT("%s: Installing USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName);
        else
            Log(TEXT("%s: Installing USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName);
    }

    UnloadDll((LPDLL_ENTRY_POINTS) & USBDriverEntry);

    return (TRUE);
}

HWND ShowDialogBox(LPCTSTR szPromptString)
{

    if(szPromptString == NULL)
        return NULL;

    //create a dialog box
    HWND hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_REMOVECARD), NULL, NULL);
    if(NULL == hDiagWnd) {
        RETAILMSG(1,(TEXT("WARNING: Can not create dialog box! ")));
    }
    //show dialog
    ShowWindow(hDiagWnd, SW_SHOW);

    //show the prompt info
    SetDlgItemText(hDiagWnd, IDC_TEXT1, szPromptString);
    UpdateWindow(hDiagWnd);

    //debug output msg for headless tests.
    RETAILMSG(1,(szPromptString));

    return hDiagWnd;

}

VOID DeleteDialogBox(HWND hDiagWnd)
{

    //destroy the dialogbox
    if(NULL != hDiagWnd)
        DestroyWindow(hDiagWnd);
    hDiagWnd = NULL;
}



//###############################################
//Vendor transfer API
//###############################################
BOOL SendVendorTransfer(UsbClientDrv * pDriverPtr, BOOL bIn, PUSB_TDEVICE_REQUEST pUtdr, LPVOID pBuffer, PDWORD pcbRetBytes)
{
    if(pDriverPtr == NULL || pUtdr == NULL)
        return FALSE;

    DWORD dwFlags = (bIn == TRUE) ? USB_IN_TRANSFER : USB_OUT_TRANSFER;
    if(pcbRetBytes != NULL) {    //we can expect short transfer here
        dwFlags |= USB_SHORT_TRANSFER_OK;
    }
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL,
                                  dwFlags,
                                  (PUSB_DEVICE_REQUEST) pUtdr,
                                  pBuffer, 0);
    if(hVTransfer == NULL) {
        RETAILMSG(1,(TEXT("issueVendorTransfer failed\r\n")));
        return FALSE;
    }

    int iCnt = 0;
    //we will wait for about 3 minutes
    while(iCnt < 1000 * 9) {
        if((pDriverPtr->lpUsbFuncs)->lpIsTransferComplete(hVTransfer) == TRUE) {
            break;
        }
        Sleep(20);
        iCnt++;
    }

    if(iCnt >= 1000 * 9) {    //time out
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
        RETAILMSG(1,(TEXT("issueVendorTransfer time out!\r\n")));
        return FALSE;
    }

    DWORD dwError = USB_NO_ERROR;
    DWORD cbdwBytes = 0;
    BOOL bRet = (pDriverPtr->lpUsbFuncs)->lpGetTransferStatus(hVTransfer,
                                  &cbdwBytes,
                                  &dwError);
    if(bRet == FALSE || dwError != USB_NO_ERROR || ((pcbRetBytes == NULL) && (cbdwBytes != pUtdr->wLength)))
    {
        RETAILMSG(1,(TEXT("IssueVendorTransfer has encountered some error!\r\n")));
        RETAILMSG(1,(TEXT("dwError = 0x%x,  cbdwBytes = %d\r\n"), dwError, cbdwBytes));
        bRet = FALSE;
    } else if(pcbRetBytes != NULL) {    //if this para is not null, we just return the actual r/w bytes.
        *pcbRetBytes = cbdwBytes;
    }

    (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);

    return bRet;
}
