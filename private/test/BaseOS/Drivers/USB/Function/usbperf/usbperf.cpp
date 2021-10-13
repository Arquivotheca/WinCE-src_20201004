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
/*++
Module Name:
    usbperf.cpp

Abstract:
    USB Client Driver for testing USBDriver.

Author:
    davli

Modified:
    weichen
    
Functions:

Notes:
--*/

#define __THIS_FILE__   TEXT("usbperf.cpp")
#include <windows.h>
#include <usbdi.h>
#include "loadfn.h"
#include "usbperf.h"
#include "UsbClass.h"
#include "resource.h"
#include "perftst.h"

//
// CONFIGURATION SELECTION - stored in <g_bIsochCfg> 
//
#define INVALID_CONFIGURATION       0
#define DEFAULT_CONFIGURATION       1
#define ALTERNATIVE_CONFIGURATION   2

#define RECONFIG_TIMEOUT (DWORD)(30000)                                 // 30 seconds
#define RECONFIG_RESET   (DWORD)(5000)                                  // initial delay is 5 sec
#define RECONFIG_DELAY   (DWORD)(100)                                   // granularity 0.1 sec
#define RECONFIG_RETRIES (DWORD)((12000-RECONFIG_RESET)/RECONFIG_DELAY) // for a total of 12 sec

//globals
PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[MAX_USB_CLIENT_DRIVER] = {NULL,};
UCHAR g_bIsochCfg[MAX_USB_CLIENT_DRIVER] = {INVALID_CONFIGURATION,}; // flip configuration on Reflector side for best isochronous performance

HINSTANCE g_hInst = NULL;

BOOL g_fUnloadDriver = FALSE;    // unload usbperf.dll from usbd.dll if true
BOOL g_fRegUpdated = FALSE;      // registry update only needs to be done once, unless unloaded

USBDriverClass *g_pUsbDriver=NULL; //USB Bus APIs

static  TCHAR   *USBDriverEntryText[] = {
    { TEXT("USBDeviceAttach") },
    { TEXT("USBInstallDriver") },
    { TEXT("USBUnInstallDriver") },
    NULL
    };

static  TCHAR   *USBDEntryPointsText[] = {
    { TEXT("OpenClientRegistryKey") },
    { TEXT("RegisterClientDriverID") },
    { TEXT("UnRegisterClientDriverID") },
    { TEXT("RegisterClientSettings") },
    { TEXT("UnRegisterClientSettings") },
    { TEXT("GetUSBDVersion") },
    NULL
    };

static  USBD_DRIVER_ENTRY   USBDriverEntry;
static  USBD_ENTRY_POINTS   USBDEntryPoints;

// PerfScenario entry points.
#define PERFSCENARIO_DLL L"PerfScenario.dll"
PFN_PERFSCEN_OPENSESSION  g_lpfnPerfScenarioOpenSession  = NULL;
PFN_PERFSCEN_CLOSESESSION g_lpfnPerfScenarioCloseSession = NULL;
PFN_PERFSCEN_ADDAUXDATA   g_lpfnPerfScenarioAddAuxData   = NULL;
PFN_PERFSCEN_FLUSHMETRICS g_lpfnPerfScenarioFlushMetrics = NULL;

// Keep track of how we're doing with PerfScenario.
PERFSCENARIO_STATUS g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;

extern WCHAR g_szPerfOutputFilename[MAX_PATH];

CKato            *g_pKato=NULL;
SPS_SHELL_INFO   *g_pShellInfo;

//Currently, only one Reflector is used at a time, so only one event is needed.
HANDLE  g_hEvtDeviceRemoved = NULL;



#pragma message ("\r\n *** PROFILER & CeLOG functionality is enabled! ***\r\n")

UINT    g_uiProfilingInterval = 0;
#define USB_DEFAULT_PROFILER_INTERVAL 50

BOOL    g_fCeLog = FALSE;
HANDLE  g_hFlushEvent = NULL;
#define CELZONE_UNINTERESTING  ~( 0                     \
                                | CELZONE_HEAP          \
                                | CELZONE_GWES          \
                                | CELZONE_LOADER        \
                                | CELZONE_MEMTRACKING   \
                                | CELZONE_BOOT_TIME     \
                                | CELZONE_GDI           \
                                | CELZONE_LOWMEM        \
                                | CELZONE_KCALL         \
                                | CELZONE_DEBUG         \
                                )
#define CELZONE_INTERESTING     ( 0                 \
                                | CELZONE_PROFILER  \
                                | CELZONE_MISC      \
                                )

// ----------------------------------------------------------------------------
//
// Debugger
//
// ----------------------------------------------------------------------------
#ifdef DEBUG

//
// These defines must match the ZONE_* defines
//
#define INIT_ERR                 1
#define INIT_WARN                2
#define INIT_FUNC                4
#define INIT_DETAIL              8
#define INIT_INI                16
#define INIT_USB                32
#define INIT_IO                 64
#define INIT_WEIRD             128
#define INIT_EVENTS          0x100
#define INIT_USB_CONTROL     0x200
#define INIT_USB_BULK        0x400
#define INIT_USB_INTERRUPT   0x800
#define INIT_USB_USB_ISOCH  0x1000
#define INIT_THREAD         0x2000

DBGPARAM dpCurSettings = {
    TEXT("USB Logger"), {
    TEXT("Errors"),   TEXT("Warnings"), TEXT("Functions"),TEXT("Detail"),
    TEXT("Initialization"),
    TEXT("USB"),      TEXT("Disk I/O"), TEXT("Weird"),TEXT("Events"),
    TEXT("USB_CONTROL"),TEXT("USB_BULK"),TEXT("USB_INTERRUPT"),TEXT("USB_ISOCH"),
    TEXT("Threads"),TEXT("Undefined"),TEXT("Undefined") },
    INIT_ERR|INIT_WARN |INIT_USB
};
#endif  // DEBUG


extern "C" BOOL WINAPI DllMain(HANDLE hDllHandle, ULONG dwReason, LPVOID lpreserved)
{
    UNREFERENCED_PARAMETER (lpreserved);

    switch (dwReason)    {
      case DLL_PROCESS_ATTACH:
        DEBUGREGISTER((HINSTANCE)hDllHandle);
        g_pUsbDriver= new USBDriverClass();
             g_hInst = (HINSTANCE)hDllHandle;
        break;
      case DLL_PROCESS_DETACH:
        delete g_pUsbDriver;
        g_pUsbDriver=NULL;
        break;
      default:
        break;
    }
    return TRUE;
}


//###############################################
//following APIs deal with test driver (usbperf.dll) registration issues
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
        DriverSettings.dwReleaseNumber     = USB_NO_INFO;

        DriverSettings.dwDeviceClass       = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass    = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol    = USB_NO_INFO;

        DriverSettings.dwInterfaceClass    = 0x0F;      //
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
extern "C" BOOL
USBUnInstallDriver()
{
    BOOL fRet = FALSE;
    USBDriverClass usbDriver;

    if (usbDriver.IsOk())  {
        USB_DRIVER_SETTINGS DriverSettings;

        DriverSettings.dwCount = sizeof(DriverSettings);

        // This structure must be filled out the same as we registered with
        //NetChip2280
        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = 0xffe0;
        DriverSettings.dwReleaseNumber     = USB_NO_INFO;

        DriverSettings.dwDeviceClass       = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass    = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol    = USB_NO_INFO;

        DriverSettings.dwInterfaceClass    = 0x0;
        DriverSettings.dwInterfaceSubClass = 0x00; // boot device
        DriverSettings.dwInterfaceProtocol = 0x00; // mouse

        fRet =usbDriver.UnRegisterClientSettings(TEXT("Generic UFN Test Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("Generic UFN Test Loopback Driver"));

    }

    return fRet;
}

//This gets called when the old instance is removed and a new one comes back.
BOOL WINAPI DeviceNotify(LPVOID lpvNotifyParameter,DWORD dwCode,LPDWORD * ,LPDWORD * ,LPDWORD * ,LPDWORD * )
{
    BOOL fReturn = TRUE;
    switch(dwCode)
    {
        case USB_CLOSE_DEVICE:
            g_pKato->Log(LOG_DETAIL, TEXT("USBPerf: Close Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            // All processing done in destructor
            if (g_pUsbDriver) {
                UsbClientDrv * pClientDrv=(UsbClientDrv *)lpvNotifyParameter;
                if (g_pUsbDriver->RemoveClientDrv(pClientDrv,FALSE)) {
                    if (g_hEvtDeviceRemoved) {
                        SetEvent(g_hEvtDeviceRemoved);
                    }
                }
                (*pClientDrv->lpUsbFuncs->lpUnRegisterNotificationRoutine)(
                    pClientDrv->GetUsbHandle(),
                    DeviceNotify,
                    pClientDrv);
                delete pClientDrv;
            }
            break;
        case USB_SUSPENDED_DEVICE:
            g_pKato->Log(LOG_DETAIL, TEXT("USBPerf: Suspend Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            break;
        case USB_RESUMED_DEVICE:
            g_pKato->Log(LOG_DETAIL, TEXT("USBPerf: Resume Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            break;
        default:
            g_pKato->Log(LOG_DETAIL, TEXT("USBPerf: Unknown Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            fReturn = FALSE;
            break;
    }

    return fReturn;
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
extern "C" BOOL
USBDeviceAttach(
    USB_HANDLE hDevice,           // @parm [IN] - USB device handle
    LPCUSB_FUNCS lpUsbFuncs,      // @parm [IN] - Pointer to USBDI function table
    LPCUSB_INTERFACE lpInterface, // @parm [IN] - If client is being loaded as an interface
                  //              driver, contains a pointer to the USB_INTERFACE
                  //              structure that contains interface information.
                  //              If client is not loaded for a specific interface,
                  //              this parameter will be NULL, and the client
                  //              may get interface information through <f FindInterface>

    LPCWSTR szUniqueDriverId,     // @parm [IN] - Contains client driver id string
    LPBOOL fAcceptControl,        // @parm [OUT]- Filled in with TRUE if we accept control
                  //              of the device, or FALSE if USBD should continue
                  //              to try to load client drivers.
    LPCUSB_DRIVER_SETTINGS lpDriverSettings,// @parm [IN] - Contains pointer to USB_DRIVER_SETTINGS
                        //              struct that indicates how we were loaded.

    DWORD dwUnused)               // @parm [IN] - Reserved for use with future versions of USBD
{
    UNREFERENCED_PARAMETER(dwUnused);
    DEBUGMSG(DBG_USB, (TEXT("USBPerf: Device Attach")));
    *fAcceptControl = FALSE;

    if(g_pUsbDriver == NULL || lpInterface == NULL)
        return FALSE;

    DEBUGMSG(DBG_USB, (TEXT("USBPerf: DeviceAttach, IF %u, #EP:%u, Class:%u, Sub:%u, Prot:%u\r\n"),
            lpInterface->Descriptor.bInterfaceNumber,lpInterface->Descriptor.bNumEndpoints,
            lpInterface->Descriptor.bInterfaceClass,lpInterface->Descriptor.bInterfaceSubClass,
            lpInterface->Descriptor.bInterfaceProtocol));

    UsbClientDrv *pLocalDriverPtr=new UsbClientDrv(hDevice,lpUsbFuncs,lpInterface,szUniqueDriverId,lpDriverSettings);
    ASSERT(pLocalDriverPtr);
    if(pLocalDriverPtr == NULL)
        return FALSE;

    if (!pLocalDriverPtr->Initialization() || !(g_pUsbDriver->AddClientDrv(pLocalDriverPtr)))   {
        delete pLocalDriverPtr;
        pLocalDriverPtr=NULL;
             DEBUGMSG(DBG_USB, (TEXT("USBPerf: Device Attach failed")));
        return FALSE;
    }
    (*lpUsbFuncs->lpRegisterNotificationRoutine)(hDevice,DeviceNotify, pLocalDriverPtr);
    *fAcceptControl = TRUE;
    DEBUGMSG(DBG_USB, (TEXT("USBPerf: Device Attach success, pLocalDrv=%x"),pLocalDriverPtr));
    return TRUE;
}


//###############################################
// TUX test cases and distribution
//###############################################

// Here we use some internal information regarding the loopback configurations.
// All loopback configurations have at least one BULK endpoint.
// For high speed, we use "lufldrv -p" to employ the high-speed performance reflector.
// For full speed, we use "lufldrv -f" to employ the full-speed reflector.
// With the above reflector configurations:
//    - All full-speed BULK endpoints have a packetsize of 0x40.
//    - All high-speed BULK endpoints have a packetsize greater than 0x40.
BOOL IsHighSpeed(UsbClientDrv* pClientDrv)
{
    if (pClientDrv == NULL)
    {
        return FALSE;
    }

    // Find the first BULK endpoint.
    DWORD i = 0;
    const DWORD NUM_ENDPOINTS = pClientDrv->GetNumOfEndPoints();
    while ((i < NUM_ENDPOINTS) && 
            ((pClientDrv->GetDescriptorPoint(i)->Descriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) != USB_ENDPOINT_TYPE_BULK))
    {
        i++;
    }

    // See if the packet size is greater than 0x40.
    if (i == NUM_ENDPOINTS)
    {
        return FALSE;
    }
    else
    {
        return (pClientDrv->GetDescriptorPoint(i)->Descriptor.wMaxPacketSize > 0x40);
    }
}


TESTPROCAPI LoopTestProc(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER (tpParam);

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DEBUGMSG(DBG_FUNC, (TEXT("LoopTestProc")));

    if (g_pUsbDriver==NULL) 
    {
        g_pKato->Log(LOG_FAIL, TEXT("No USB Function Device Driver loaded, failing!"));
        return TPR_FAIL;
    }
    if(g_pUsbDriver->GetCurAvailDevs() == 0)  //Driver loaded, but no devices are attached.  Skip to prevent a search through the entire array.
    {
        g_pKato->Log(LOG_FAIL, TEXT("No REFLECTOR detected, failing!"));
        return TPR_FAIL;
    }

    DWORD dwRet = TPR_PASS;
    DWORD dwDriverNumber = 0;
    USB_PARAM m_UsbParam;
    BOOL fHighSpeed = TRUE;

    ParseCommandLine(&m_UsbParam,g_pShellInfo->szDllCmdLine);
    DWORD dwActiveDevice = (m_UsbParam.dwSelectDevice!=0)?m_UsbParam.dwSelectDevice:1;
    dwActiveDevice--;

    //make phsyical mem ready
    PPhysMemNode PMemN = new PhysMemNode[MY_PHYS_MEM_SIZE];

    for (int i=0;i<(int)(g_pUsbDriver->GetArraySize());i++)
    {
        UsbClientDrv* pClientDrv = g_pUsbDriver->GetAt(i);

        // For tests which require RECONFIG (or RESET) of Reflector,
        // few retries will help to avoid premature failure before 
        // Reflector comes back up & runing again.
        DWORD dwRetry = 30;
        while (pClientDrv==NULL && dwRetry!=0) {
            pClientDrv = g_pUsbDriver->GetAt(i);
            Sleep(100);
            dwRetry--;
        }

        if (pClientDrv && (dwActiveDevice==0) && i<MAX_USB_CLIENT_DRIVER)  //g_pTstDevLpbkInfo[i] is used.
        {
            pClientDrv->Lock();
            if(g_pTstDevLpbkInfo[i] == NULL)
            {
                if(lpFTE->dwUniqueID < SPECIAL_CASE)
                {
                    g_pTstDevLpbkInfo[i] = GetTestDeviceLpbkInfo(pClientDrv);
                    if(g_pTstDevLpbkInfo[i] == NULL || g_pTstDevLpbkInfo[i]->uNumOfPipePairs == 0) //failed or no data pipes
                    {
                        NKDbgPrintfW(_T("Can not get test device's loopback pairs' information or there's no data loopback pipe pair, test will be skipped for this device!!!"));
                        pClientDrv->Unlock();
                        continue;
                    }
                }
            } // if(g_pTstDevLpbkInfo[i] == NULL){

            DEBUGMSG(DBG_INI, (TEXT("Device retrieve from %d"),i));
            dwDriverNumber++;

            fHighSpeed = IsHighSpeed(pClientDrv);

            // standard USB performance cases
            if (lpFTE->dwUniqueID < BULK_BLOCKING_CASE)
            {
                BOOL fPhysMem         = (lpFTE->dwUniqueID / PHYSMEM_CASE == PHYS_MEM_HOST);
                LONG iTiming          = (lpFTE->dwUniqueID % 10);
                LONG iXferPriority    = (lpFTE->dwUniqueID % 100) / 10;
                LONG iQParam          = (lpFTE->dwUniqueID % 1000) / 100;
                LONG iXferDirection   = (iXferPriority & 1);
                UCHAR uEndpointType;

                // decode endpoint type
                // at this time, 'iTiming' contains both timing type & endpoint type
                // this switch extracts the endpoint type only, timing follows right after
                switch(iTiming)
                {
                case 1+RAW_TIMING:
                case 1:
                    uEndpointType = USB_ENDPOINT_TYPE_BULK;
                    break;

                case 2+RAW_TIMING:
                case 2:
                    uEndpointType = USB_ENDPOINT_TYPE_INTERRUPT;
                    break;

                case 3+RAW_TIMING:
                case 3:
                    uEndpointType = USB_ENDPOINT_TYPE_ISOCHRONOUS;
                    break;

                default:
                    // unrecognized case
                    pClientDrv->Unlock();
                    delete [] PMemN;
                    return TPR_NOT_HANDLED;

                }
                // now, set the timing alone
                iTiming = (iTiming<4)?HOST_TIMING:RAW_TIMING;

#ifdef EXTRA_TRACES
                // spit out all params 
                NKDbgPrintfW(_T("Std test #%i '%s'; EndPt:%i Timing:%i Prio:%i Phys:%i Dir:%i\n"),
                    lpFTE->dwUniqueID,lpFTE->lpDescription,uEndpointType,
                    iTiming,iXferPriority,fPhysMem,iXferDirection);
                Sleep(10); // to get this trace sent over to host before proceeding
#endif

                //
                // Selecting new cofiguration is necessary: ALT=2, DEF=1
                //
                UCHAR uConfigToUse = (uEndpointType == USB_ENDPOINT_TYPE_ISOCHRONOUS) ? ALTERNATIVE_CONFIGURATION : DEFAULT_CONFIGURATION;
                if (g_bIsochCfg[i]==uConfigToUse)
                    goto RunNormalPerfTests;

                g_bIsochCfg[i] = uConfigToUse;
                uConfigToUse = (ALTERNATIVE_CONFIGURATION==uConfigToUse) ? TRUE : FALSE;

                NKDbgPrintfW(_T("Reflector 're-configure to %s' request about to be send...\r\n"),
                                uConfigToUse?_T("alternative"):_T("default"));

                DWORD dwConfigTime = GetTickCount();
                SetTestDeviceConfiguration(&pClientDrv, uConfigToUse, fHighSpeed, i);

#ifdef REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION
                Sleep(1000);
#else // ~ REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION
                if (pClientDrv == NULL)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("Cannot obtain Driver handle for re-configured Reflector in case %u '%s', failing!\n"),lpFTE->dwUniqueID,lpFTE->lpDescription);
                    dwRet = TPR_FAIL;
                    break;
                }
#endif // REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION

                pClientDrv->ResetDefaultPipe(); // just in case
                Sleep(50);
                g_pTstDevLpbkInfo[i] = GetTestDeviceLpbkInfo(pClientDrv); // new endpoint geometry
                Sleep(20);

                if(g_pTstDevLpbkInfo[i] == NULL || g_pTstDevLpbkInfo[i]->uNumOfPipePairs == 0) //failed or no data pipes
                {
                    g_pKato->Log(LOG_FAIL, TEXT("Cannot obtain pipe pairs for re-configured Reflector in case %u '%s', failing!\n"),lpFTE->dwUniqueID,lpFTE->lpDescription);
                    dwRet = TPR_FAIL;
                    break;
                }

                NKDbgPrintfW(_T("Reflector 're-configure to %s' request complete in %u msec...\r\n\r\n"),
                                uConfigToUse? _T("alternative"):_T("default"),GetTickCount()-dwConfigTime);

RunNormalPerfTests:
                dwRet = NormalPerfTests(pClientDrv,
                                        uEndpointType,
                                        i,                              // port number
                                        fPhysMem,                       // 0 or PHYS_MEM_HOST
                                        !iXferDirection,                // flip the BOOL value
                                        (iXferPriority<<16)|iTiming,    // < 0000 0000 0000 pppD 0000 0000 0000 tttt >  combo
                                        (iQParam<<16)|BLOCK_NONE,       // < 0000 0000 0000 qqqQ 0000 0000 0000 bbbb >  combo
                                        fHighSpeed,
                                        lpFTE->lpDescription);
            }
            else // special bulk test cases
            {
                if(lpFTE->dwUniqueID > 15000 && lpFTE->dwUniqueID < 17000 && (lpFTE->dwUniqueID % 10) == 1) 
                {
                    NKDbgPrintfW(_T("%d host timing. %d\n"), lpFTE->dwUniqueID, i);
                    dwRet = NormalPerfTests(pClientDrv, USB_ENDPOINT_TYPE_BULK, i, 
                                            (lpFTE->dwUniqueID / 100) % 10, 
                                            ((lpFTE->dwUniqueID / 10) % 10)^1, 
                                            HOST_TIMING,
                                            (lpFTE->dwUniqueID / 1000) % 10, 
                                            fHighSpeed,
                                            lpFTE->lpDescription);
                    break;
                }
                else if(lpFTE->dwUniqueID > 25000 && lpFTE->dwUniqueID < 27000 && (lpFTE->dwUniqueID % 10) == 1) 
                {
                    NKDbgPrintfW(_T("device timing.\n"));
                    dwRet = NormalPerfTests(pClientDrv, USB_ENDPOINT_TYPE_BULK, i, 
                                            (lpFTE->dwUniqueID / 100) % 10, 
                                            ((lpFTE->dwUniqueID / 10) % 10)^1, 
                                            DEV_TIMING,
                                            (lpFTE->dwUniqueID / 1000) % 10, 
                                            fHighSpeed,
                                            lpFTE->lpDescription);
                    break;
                }
                else if(lpFTE->dwUniqueID > 35000 && lpFTE->dwUniqueID < 37000 && (lpFTE->dwUniqueID % 10) == 1) 
                {
                    NKDbgPrintfW(_T("sync timing.\n"));
                    dwRet = NormalPerfTests(pClientDrv, USB_ENDPOINT_TYPE_BULK, i, 
                                            (lpFTE->dwUniqueID / 100) % 10, 
                                            ((lpFTE->dwUniqueID / 10) % 10)^1, 
                                            SYNC_TIMING,
                                            (lpFTE->dwUniqueID / 1000) % 10, 
                                            fHighSpeed,
                                            lpFTE->lpDescription);
                    break;
                }
                else 
                {
                    g_pKato->Log(LOG_FAIL, TEXT("Cannot handle case %u '%s'\n"),lpFTE->dwUniqueID,lpFTE->lpDescription);
                    dwRet = TPR_NOT_HANDLED;
                }
            }

            pClientDrv->Unlock();
            break;

        } // if (pClientDrv && (dwActiveDevice==0))
        else if (pClientDrv) 
        {
            dwActiveDevice--;
        }
    } //for (int i=0;i<(int)(g_pUsbDriver->GetArraySize());i++) {

    delete [] PMemN;
    if (dwDriverNumber==0)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
        dwRet = TPR_SKIP;
    }

    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//
// The function table is split in two main sections: normal tests and specialized BULK tests.
//
// Specialized BULK performance tests communicate w/ function device 
// synchronize execution and receive performance information from it.
//
// Normal tests measure performance only on the host side, for output or input.
//
// Following performance scenarios are exercised:
//
// 1. All-buffers-at-once transfers (aka "Normal Performance Test")
//    This type of call submits all buffers to the host driver in a tight loop,
//    providing callback pointer to a completion routine, and setting USB_NO_WAIT flag;
//    then waits for certain period of time for all transfers to complete.
//
// 2. Keep-queue-full transfers (aka "Full Queue Performance Test")
//    This type of transfers is typical for streaming applications,
//    when new transfer is issued as previous one completes (form of double-buffering.)
//    Callback pointer is provided and USB_NO_WAIT flag is set.
//    First few buffers are submitted "at-once", then transfer mode changes -
//    next buffers are submitted to the queue from within completion callback.
//      (These tests have "FullQ" as part of the name.)
//
// (*) Simple synchronous IssueTransfer() calls
//    This is the simplest method, application submits a buffer for transfer and 
//    USB Host Driver does not return before the transaction is complete.
//    No callback pointer is provided or USB_NO_WAIT flag is set.
//      (These tests have "Sync call" as part of the name.)
//      (NOTE: there is no watchdog mechanism provided for this test type.)
//
//  NOTE: For proper comparison, each (1) and (2) group is complemented
//        with same tests of type (*), using exactly the same amount of data.
//
//
// 3. High-volume Bulk & Interrupt tests using physical memory
//      (aka "Full Queue Physical Memory Performance Test")
//    These tests differ from Group (2) by the volume of data rtansfered,
//    and by the type of memory allocation - not virtual but physical memory.
//    Exactly 0x78000 bytes of data are transfered for each test case.
//    Again, each pair of tests is complimented with another pair of type (*)
//    so that performance comparison between streaming application scenario
//    and simple "one-by-one" transfer w/o callback can be done.
//
//
// 4. Isochronous micro-frame frequency (aka "Isochronous Microframe Performance Test")
//    Per USB documentation, error level in isochronous transfers can be reduced
//    if higher micro-frame frequency is applied, instead of "dumping" full block.
//    A streaming application which needs (or which allows user to control)
//    high quality, may be modifying this factor.
//    Two sub-groups - "all-buffers-at-once" and "Full Queue" are employed here.
//    As before, tests cases of type (*) transfering same amount of data 
//    are complementing each test case of the two types mentioned above.
//    Microframe divisors of 1, 2, 4 and 8 are applied, thus quadrupling
//    the number of performance scenarios for this group.
//
//
// Each of the test groups above is quadripled by the priority of the calling thread.
// This factor is not expected to produce noticeable (if any) performance impact,
// but in all potential race conditions the thread priority plays important role.
//
// Ten different buffer sizes are employed for each test case - from 64 to 65536 bytes.
// Special size 1536 is related to network operations.
//
//

//    bit coding for TestID
//  ========================
//
//      BULK    INT     ISOC
//      xxx1    xxx2    xxx3    USB_NO_WAIT is set (async return from Driver)
//      xxx5    xxx6    xxx7    call will return only when complete (sync return)
//
//      CALL DIRECTION (bit 0 in the tens)
//      xx0x (even tens) = OUT
//      xx1x (odd tens)  = IN
//
//      PRIORITY LEVEL OF THE CALLING THREAD (bits 2:1 in the tens)
//      xx0x, xx1x = normal priority level
//      xx2x, xx3x = high priority level
//      xx4x, xx5x = low priority level
//      xx6x, xx7x = critical priority level
//
//      FULL QUEUE OF TRANSFERS (bit 0 in the hundreds)
//      x0xx (even hundreds) = issue all transfers at once and wait completion, do not "refill"
//      x1xx (odd hundreds)  = issue few initial transfers, "refill" on completion notification
//
//      ISOCHRONOUS MICROFRAMING (bits 2:1 in the hundreds) - not used in BULK and INT
//      x2xx, x3xx = double microframe rate
//      x4xx, x5xx = quadruple microframe rate
//      x6xx, x7xx = fill all 8 microframes
//      
//
//      Group 8xxx (PHYSMEM_CASE) is for physical memory tests, it is always performed on full queue
//      (that is, all tests transfer equal amount of data for which performance is measured.)
//      For consistency, all full queue cases have bit 0 in the hundreds set to ONE.
//
//

extern "C"{

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    //--------------------------------------------------------------------------------------------------------
    { TEXT("Normal Performance Test"),                                              0, 0, 0,     NULL},

    { TEXT("Bulk Normal Perf Test"),                                                1, 0, 1001,  LoopTestProc } ,
    { TEXT("Interrupt Normal Perf Test"),                                           1, 0, 1002,  LoopTestProc } ,
    { TEXT("Isochronous Normal Perf Test"),                                         1, 0, 1003,  LoopTestProc } ,
    { TEXT("Bulk Normal Perf Test (Sync call, OUT)"),                               1, 0, 1005,  LoopTestProc } ,
    { TEXT("Interrupt Normal Perf Test (Sync call, OUT)"),                          1, 0, 1006,  LoopTestProc } ,
    { TEXT("Isochronous Normal Perf Test (Sync call, OUT)"),                        1, 0, 1007,  LoopTestProc } ,

    { TEXT("Bulk Normal Perf Test (IN)"),                                           1, 0, 1011,  LoopTestProc } ,
    { TEXT("Interrupt Normal Perf Test (IN)"),                                      1, 0, 1012,  LoopTestProc } ,
    { TEXT("Isochronous Normal Perf Test (IN)"),                                    1, 0, 1013,  LoopTestProc } ,
    { TEXT("Bulk Normal Perf Test (Sync call, IN)"),                                1, 0, 1015,  LoopTestProc } ,
    { TEXT("Interrupt Normal Perf Test (Sync call, IN)"),                           1, 0, 1016,  LoopTestProc } ,
    { TEXT("Isochronous Normal Perf Test (Sync call, IN)"),                         1, 0, 1017,  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri Perf Test"),                                                1, 0, 1021,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri Perf Test"),                                           1, 0, 1022,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri Perf Test"),                                         1, 0, 1023,  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri Perf Test (Sync call, OUT)"),                               1, 0, 1025,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test (Sync call, OUT)"),                          1, 0, 1026,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri Perf Test (Sync call, OUT)"),                        1, 0, 1027,  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri Perf Test (IN)"),                                           1, 0, 1031,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri Perf Test (IN)"),                                      1, 0, 1032,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri Perf Test (IN)"),                                    1, 0, 1033,  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri Perf Test (Sync call, IN)"),                                1, 0, 1035,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test (Sync call, IN)"),                           1, 0, 1036,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri Perf Test (Sync call, IN)"),                         1, 0, 1037,  LoopTestProc } ,

#ifdef TEST_ALL_PRIORITIES
    { TEXT("Bulk Lo-Pri Perf Test"),                                                1, 0, 1041,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test"),                                           1, 0, 1042,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri Perf Test"),                                         1, 0, 1043,  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri Perf Test (Sync call, OUT)"),                               1, 0, 1045,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test (Sync call, OUT)"),                          1, 0, 1046,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri Perf Test (Sync call, OUT)"),                        1, 0, 1047,  LoopTestProc } ,

    { TEXT("Bulk Lo-Pri Perf Test (IN)"),                                           1, 0, 1051,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test (IN)"),                                      1, 0, 1052,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri Perf Test (IN)"),                                    1, 0, 1053,  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri Perf Test (Sync call, IN)"),                                1, 0, 1055,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri Perf Test (Sync call, IN)"),                           1, 0, 1056,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri Perf Test (Sync call, IN)"),                         1, 0, 1057,  LoopTestProc } ,

    { TEXT("Bulk Time-Critical Perf Test"),                                         1, 0, 1061,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical Perf Test"),                                    1, 0, 1062,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical Perf Test"),                                  1, 0, 1063,  LoopTestProc } ,
    { TEXT("Bulk Time-Critical Perf Test (Sync call, OUT)"),                        1, 0, 1065,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical Perf Test (Sync call, OUT)"),                   1, 0, 1066,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical Perf Test (Sync call, OUT)"),                 1, 0, 1067,  LoopTestProc } ,

    { TEXT("Bulk Time-Critical Perf Test (IN)"),                                    1, 0, 1071,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical Perf Test (IN)"),                               1, 0, 1072,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical Perf Test (IN)"),                             1, 0, 1073,  LoopTestProc } ,
    { TEXT("Bulk Time-Critical Perf Test (Sync call, IN)"),                         1, 0, 1075,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical Perf Test (Sync call, IN)"),                    1, 0, 1076,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical Perf Test (Sync call, IN)"),                  1, 0, 1077,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    //--------------------------------------------------------------------------------------------------------
    { TEXT("Full Queue Performance Test"),                                          0, 0, 0,     NULL},

    { TEXT("Bulk Normal FullQ Perf Test"),                                          1, 0, 1101,  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ Perf Test"),                                     1, 0, 1102,  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ Perf Test"),                                   1, 0, 1103,  LoopTestProc } ,
    { TEXT("Bulk Normal FullQ Perf Test (Sync call, OUT)"),                         1, 0, 1105,  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ Perf Test (Sync call, OUT)"),                    1, 0, 1106,  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ Perf Test (Sync call, OUT)"),                  1, 0, 1107,  LoopTestProc } ,

    { TEXT("Bulk Normal FullQ Perf Test (IN)"),                                     1, 0, 1111,  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ Perf Test (IN)"),                                1, 0, 1112,  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ Perf Test (IN)"),                              1, 0, 1113,  LoopTestProc } ,
    { TEXT("Bulk Normal FullQ Perf Test (Sync call, IN)"),                          1, 0, 1115,  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ Perf Test (Sync call, IN)"),                     1, 0, 1116,  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ Perf Test (Sync call, IN)"),                   1, 0, 1117,  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri FullQ Perf Test"),                                          1, 0, 1121,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ Perf Test"),                                     1, 0, 1122,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ Perf Test"),                                   1, 0, 1123,  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri FullQ Perf Test (Sync call, OUT)"),                         1, 0, 1125,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ Perf Test (Sync call, OUT)"),                    1, 0, 1126,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ Perf Test (Sync call, OUT)"),                  1, 0, 1127,  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri FullQ Perf Test (IN)"),                                     1, 0, 1131,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ Perf Test (IN)"),                                1, 0, 1132,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ Perf Test (IN)"),                              1, 0, 1133,  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri FullQ Perf Test (Sync call, IN)"),                          1, 0, 1135,  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ Perf Test (Sync call, IN)"),                     1, 0, 1136,  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ Perf Test (Sync call, IN)"),                   1, 0, 1137,  LoopTestProc } ,

#ifdef TEST_ALL_PRIORITIES
    { TEXT("Bulk Lo-Pri FullQ Perf Test"),                                          1, 0, 1141,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ Perf Test"),                                     1, 0, 1142,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ Perf Test"),                                   1, 0, 1143,  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri FullQ Perf Test (Sync call, OUT)"),                         1, 0, 1145,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ Perf Test (Sync call, OUT)"),                    1, 0, 1146,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ Perf Test (Sync call, OUT)"),                  1, 0, 1147,  LoopTestProc } ,

    { TEXT("Bulk Lo-Pri FullQ Perf Test (IN)"),                                     1, 0, 1151,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ Perf Test (IN)"),                                1, 0, 1152,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ Perf Test (IN)"),                              1, 0, 1153,  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri FullQ Perf Test (Sync call, IN)"),                          1, 0, 1155,  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ Perf Test (Sync call, IN)"),                     1, 0, 1156,  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ Perf Test (Sync call, IN)"),                   1, 0, 1157,  LoopTestProc } ,

    { TEXT("Bulk Time-Critical FullQ Perf Test"),                                   1, 0, 1161,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ Perf Test"),                              1, 0, 1162,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ Perf Test"),                            1, 0, 1163,  LoopTestProc } ,
    { TEXT("Bulk Time-Critical FullQ Perf Test (Sync call, OUT)"),                  1, 0, 1165,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ Perf Test (Sync call, OUT)"),             1, 0, 1166,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ Perf Test (Sync call, OUT)"),           1, 0, 1167,  LoopTestProc } ,

    { TEXT("Bulk Time-Critical FullQ Perf Test (IN)"),                              1, 0, 1171,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ Perf Test (IN)"),                         1, 0, 1172,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ Perf Test (IN)"),                       1, 0, 1173,  LoopTestProc } ,
    { TEXT("Bulk Time-Critical FullQ Perf Test (Sync call, IN)"),                   1, 0, 1175,  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ Perf Test (Sync call, IN)"),              1, 0, 1176,  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ Perf Test (Sync call, IN)"),            1, 0, 1177,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    //--------------------------------------------------------------------------------------------------------
    { TEXT("Isochronous Microframe Performance Test"),                              0, 0, 0,     NULL},

    { TEXT("Isoch x2 microframe Normal Perf Test"),                              1, 0, 1203,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, 1207,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal Perf Test (IN)"),                         1, 0, 1213,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal Perf Test (Sync call, IN)"),              1, 0, 1217,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri Perf Test"),                              1, 0, 1223,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, 1227,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri Perf Test (IN)"),                         1, 0, 1233,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, 1237,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x2 microframe Lo-Pri Perf Test"),                              1, 0, 1243,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, 1247,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri Perf Test (IN)"),                         1, 0, 1253,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, 1257,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical Perf Test"),                       1, 0, 1263,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, 1267,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical Perf Test (IN)"),                  1, 0, 1273,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, 1277,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch x2 microframe Normal FullQ Perf Test"),                        1, 0, 1303,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, 1307,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal FullQ Perf Test (IN)"),                   1, 0, 1313,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, 1317,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri FullQ Perf Test"),                        1, 0, 1323,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1327,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, 1333,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1337,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x2 microframe Lo-Pri FullQ Perf Test"),                        1, 0, 1343,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1347,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, 1353,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1357,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical FullQ Perf Test"),                 1, 0, 1363,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, 1367,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, 1373,  LoopTestProc } ,
    { TEXT("Isoch x2 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, 1377,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch x4 microframe Normal Perf Test"),                              1, 0, 1403,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, 1407,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal Perf Test (IN)"),                         1, 0, 1413,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal Perf Test (Sync call, IN)"),              1, 0, 1417,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri Perf Test"),                              1, 0, 1423,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, 1427,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri Perf Test (IN)"),                         1, 0, 1433,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, 1437,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x4 microframe Lo-Pri Perf Test"),                              1, 0, 1443,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, 1447,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri Perf Test (IN)"),                         1, 0, 1453,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, 1457,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical Perf Test"),                       1, 0, 1463,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, 1467,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical Perf Test (IN)"),                  1, 0, 1473,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, 1477,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch x4 microframe Normal FullQ Perf Test"),                        1, 0, 1503,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, 1507,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal FullQ Perf Test (IN)"),                   1, 0, 1513,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, 1517,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri FullQ Perf Test"),                        1, 0, 1523,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1527,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, 1533,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1537,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x4 microframe Lo-Pri FullQ Perf Test"),                        1, 0, 1543,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1547,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, 1553,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1557,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical FullQ Perf Test"),                 1, 0, 1563,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, 1567,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, 1573,  LoopTestProc } ,
    { TEXT("Isoch x4 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, 1577,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch x8 microframe Normal Perf Test"),                              1, 0, 1603,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, 1607,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal Perf Test (IN)"),                         1, 0, 1613,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal Perf Test (Sync call, IN)"),              1, 0, 1617,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri Perf Test"),                              1, 0, 1623,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, 1627,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri Perf Test (IN)"),                         1, 0, 1633,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, 1637,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x8 microframe Lo-Pri Perf Test"),                              1, 0, 1643,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, 1647,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri Perf Test (IN)"),                         1, 0, 1653,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, 1657,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical Perf Test"),                       1, 0, 1663,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, 1667,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical Perf Test (IN)"),                  1, 0, 1673,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, 1677,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch x8 microframe Normal FullQ Perf Test"),                        1, 0, 1703,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, 1707,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal FullQ Perf Test (IN)"),                   1, 0, 1713,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, 1717,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri FullQ Perf Test"),                        1, 0, 1723,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1727,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, 1733,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1737,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch x8 microframe Lo-Pri FullQ Perf Test"),                        1, 0, 1743,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, 1747,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, 1753,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, 1757,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical FullQ Perf Test"),                 1, 0, 1763,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, 1767,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, 1773,  LoopTestProc } ,
    { TEXT("Isoch x8 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, 1777,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    //--------------------------------------------------------------------------------------------------------
    { TEXT("Physical Memory Full Queue Performance Test"),                          0, 0, 0,     NULL},

    { TEXT("Bulk Normal FullQ PhysMem Perf Test"),                                  1, 0, (PHYSMEM_CASE+101),  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ PhysMem Perf Test"),                             1, 0, (PHYSMEM_CASE+102),  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ PhysMem Perf Test"),                           1, 0, (PHYSMEM_CASE+103),  LoopTestProc } ,
    { TEXT("Bulk Normal FullQ PhysMem Perf Test (Sync call, OUT)"),                 1, 0, (PHYSMEM_CASE+105),  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ PhysMem Perf Test (Sync call, OUT)"),            1, 0, (PHYSMEM_CASE+106),  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ PhysMem Perf Test (Sync call, OUT)"),          1, 0, (PHYSMEM_CASE+107),  LoopTestProc } ,

    { TEXT("Bulk Normal FullQ PhysMem Perf Test (IN)"),                             1, 0, (PHYSMEM_CASE+111),  LoopTestProc } ,
    { TEXT("Interrupt Normal PhysMem FullQ Perf Test (IN)"),                        1, 0, (PHYSMEM_CASE+112),  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ PhysMem Perf Test (IN)"),                      1, 0, (PHYSMEM_CASE+113),  LoopTestProc } ,
    { TEXT("Bulk Normal FullQ PhysMem Perf Test (Sync call, IN)"),                  1, 0, (PHYSMEM_CASE+115),  LoopTestProc } ,
    { TEXT("Interrupt Normal FullQ PhysMem Perf Test (Sync call, IN)"),             1, 0, (PHYSMEM_CASE+116),  LoopTestProc } ,
    { TEXT("Isochronous Normal FullQ PhysMem Perf Test (Sync call, IN)"),           1, 0, (PHYSMEM_CASE+117),  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri FullQ PhysMem Perf Test"),                                  1, 0, (PHYSMEM_CASE+121),  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ PhysMem Perf Test"),                             1, 0, (PHYSMEM_CASE+122),  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ PhysMem Perf Test"),                           1, 0, (PHYSMEM_CASE+123),  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),                 1, 0, (PHYSMEM_CASE+125),  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),            1, 0, (PHYSMEM_CASE+126),  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),          1, 0, (PHYSMEM_CASE+127),  LoopTestProc } ,

    { TEXT("Bulk Hi-Pri FullQ PhysMem Perf Test (IN)"),                             1, 0, (PHYSMEM_CASE+131),  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ PhysMem Perf Test (IN)"),                        1, 0, (PHYSMEM_CASE+132),  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ PhysMem Perf Test (IN)"),                      1, 0, (PHYSMEM_CASE+133),  LoopTestProc } ,
    { TEXT("Bulk Hi-Pri FullQ PhysMem Perf Test (Sync call, IN)"),                  1, 0, (PHYSMEM_CASE+135),  LoopTestProc } ,
    { TEXT("Interrupt Hi-Pri FullQ PhysMem Perf Test (Sync call, IN)"),             1, 0, (PHYSMEM_CASE+136),  LoopTestProc } ,
    { TEXT("Isochronous Hi-Pri FullQ PhysMem Perf Test (Sync call, IN)"),           1, 0, (PHYSMEM_CASE+137),  LoopTestProc } ,

#ifdef TEST_ALL_PRIORITIES
    { TEXT("Bulk Lo-Pri FullQ PhysMem Perf Test"),                                  1, 0, (PHYSMEM_CASE+141),  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ PhysMem Perf Test"),                             1, 0, (PHYSMEM_CASE+142),  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ PhysMem Perf Test"),                           1, 0, (PHYSMEM_CASE+143),  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),                 1, 0, (PHYSMEM_CASE+145),  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),            1, 0, (PHYSMEM_CASE+146),  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ PhysMem Perf Test (Sync call, OUT)"),          1, 0, (PHYSMEM_CASE+147),  LoopTestProc } ,

    { TEXT("Bulk Lo-Pri FullQ PhysMem Perf Test (IN)"),                             1, 0, (PHYSMEM_CASE+151),  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ PhysMem Perf Test (IN)"),                        1, 0, (PHYSMEM_CASE+152),  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ PhysMem Perf Test (IN)"),                      1, 0, (PHYSMEM_CASE+153),  LoopTestProc } ,
    { TEXT("Bulk Lo-Pri FullQ PhysMem Perf Test (Sync call, IN)"),                  1, 0, (PHYSMEM_CASE+155),  LoopTestProc } ,
    { TEXT("Interrupt Lo-Pri FullQ PhysMem Perf Test (Sync call, IN)"),             1, 0, (PHYSMEM_CASE+156),  LoopTestProc } ,
    { TEXT("Isochronous Lo-Pri FullQ PhysMem Perf Test (Sync call, IN)"),           1, 0, (PHYSMEM_CASE+157),  LoopTestProc } ,

    { TEXT("Bulk Time-Critical FullQ PhysMem Perf Test"),                           1, 0, (PHYSMEM_CASE+161),  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ PhysMem Perf Test"),                      1, 0, (PHYSMEM_CASE+162),  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ PhysMem Perf Test"),                    1, 0, (PHYSMEM_CASE+163),  LoopTestProc } ,
    { TEXT("Bulk Time-Critical FullQ PhysMem Perf Test (Sync call, OUT)"),          1, 0, (PHYSMEM_CASE+165),  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ PhysMem Perf Test (Sync call, OUT)"),     1, 0, (PHYSMEM_CASE+166),  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ PhysMem Perf Test (Sync call, OUT)"),   1, 0, (PHYSMEM_CASE+167),  LoopTestProc } ,

    { TEXT("Bulk Time-Critical FullQ PhysMem Perf Test (IN)"),                      1, 0, (PHYSMEM_CASE+171),  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ PhysMem Perf Test (IN)"),                 1, 0, (PHYSMEM_CASE+172),  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ PhysMem Perf Test (IN)"),               1, 0, (PHYSMEM_CASE+173),  LoopTestProc } ,
    { TEXT("Bulk Time-Critical FullQ PhysMem Perf Test (Sync call, IN)"),           1, 0, (PHYSMEM_CASE+175),  LoopTestProc } ,
    { TEXT("Interrupt Time-Critical FullQ PhysMem Perf Test (Sync call, IN)"),      1, 0, (PHYSMEM_CASE+176),  LoopTestProc } ,
    { TEXT("Isochronous Time-Critical FullQ PhysMem Perf Test (Sync call, IN)"),    1, 0, (PHYSMEM_CASE+177),  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

        //--------------------------------------------------------------------------------------------------------

    { TEXT("Physical Memory Isochronous Microframe Performance Test"),                              0, 0, 0,     NULL},
    
    { TEXT("Isoch PhysMem x2 microframe Normal Perf Test"),                              1, 0, PHYSMEM_CASE+203,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+207,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+213,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+217,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+223,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+227,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+233,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+237,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+243,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+247,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+253,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+257,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical Perf Test"),                       1, 0, PHYSMEM_CASE+263,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, PHYSMEM_CASE+267,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical Perf Test (IN)"),                  1, 0, PHYSMEM_CASE+273,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, PHYSMEM_CASE+277,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch PhysMem x2 microframe Normal FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+303,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+307,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+313,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+317,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+323,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+327,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+333,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+337,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+343,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+347,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+353,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+357,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical FullQ Perf Test"),                 1, 0, PHYSMEM_CASE+363,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, PHYSMEM_CASE+367,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, PHYSMEM_CASE+373,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x2 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, PHYSMEM_CASE+377,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch PhysMem x4 microframe Normal Perf Test"),                              1, 0, PHYSMEM_CASE+403,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+407,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+413,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+417,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+423,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+427,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+433,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+437,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+443,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+447,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+453,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+457,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical Perf Test"),                       1, 0, PHYSMEM_CASE+463,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, PHYSMEM_CASE+467,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical Perf Test (IN)"),                  1, 0, PHYSMEM_CASE+473,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, PHYSMEM_CASE+477,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch PhysMem x4 microframe Normal FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+503,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+507,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+513,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+517,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+523,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+527,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+533,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+537,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+543,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+547,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+553,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+557,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical FullQ Perf Test"),                 1, 0, PHYSMEM_CASE+563,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, PHYSMEM_CASE+567,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, PHYSMEM_CASE+573,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x4 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, PHYSMEM_CASE+577,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch PhysMem x8 microframe Normal Perf Test"),                              1, 0, PHYSMEM_CASE+603,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+607,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+613,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+617,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+623,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+627,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+633,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+637,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri Perf Test"),                              1, 0, PHYSMEM_CASE+643,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri Perf Test (Sync call, OUT)"),             1, 0, PHYSMEM_CASE+647,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri Perf Test (IN)"),                         1, 0, PHYSMEM_CASE+653,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri Perf Test (Sync call, IN)"),              1, 0, PHYSMEM_CASE+657,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical Perf Test"),                       1, 0, PHYSMEM_CASE+663,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical Perf Test (Sync call, OUT)"),      1, 0, PHYSMEM_CASE+667,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical Perf Test (IN)"),                  1, 0, PHYSMEM_CASE+673,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical Perf Test (Sync call, IN)"),       1, 0, PHYSMEM_CASE+677,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    { TEXT("Isoch PhysMem x8 microframe Normal FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+703,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+707,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+713,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Normal FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+717,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+723,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+727,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+733,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Hi-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+737,  LoopTestProc } ,
#ifdef TEST_ALL_PRIORITIES
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri FullQ Perf Test"),                        1, 0, PHYSMEM_CASE+743,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri FullQ Perf Test (Sync call, OUT)"),       1, 0, PHYSMEM_CASE+747,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri FullQ Perf Test (IN)"),                   1, 0, PHYSMEM_CASE+753,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Lo-Pri FullQ Perf Test (Sync call, IN)"),        1, 0, PHYSMEM_CASE+757,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical FullQ Perf Test"),                 1, 0, PHYSMEM_CASE+763,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical FullQ Perf Test (Sync call, OUT)"),1, 0, PHYSMEM_CASE+767,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical FullQ Perf Test (IN)"),            1, 0, PHYSMEM_CASE+773,  LoopTestProc } ,
    { TEXT("Isoch PhysMem x8 microframe Time-Critical FullQ Perf Test (Sync call, IN)"), 1, 0, PHYSMEM_CASE+777,  LoopTestProc } ,
#endif // TEST_ALL_PRIORITIES

    //--------------------------------------------------------------------------------------------------------
    { TEXT("Special Bulk Performance Test"),                                        0, 0, 0,     NULL},

    { TEXT("Bulk Host Blocking Test / Host Timing"),                                1, 0, 15001, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Host Timing (IN)"),                           1, 0, 15011, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing"),                            1, 0, 16001, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing (IN)"),                       1, 0, 16011, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing"),                            1, 0, 25001, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing (IN)"),                       1, 0, 25011, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing"),                        1, 0, 26001, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing (IN)"),                   1, 0, 26011, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing"),                                1, 0, 35001, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing (IN)"),                           1, 0, 35011, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing"),                            1, 0, 36001, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing (IN)"),                       1, 0, 36011, LoopTestProc},

    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Host"),                1, 0, 15101, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Host (IN)"),           1, 0, 15111, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Host"),            1, 0, 16101, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Host (IN)"),       1, 0, 16111, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Host"),            1, 0, 25101, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Host (IN)"),       1, 0, 25111, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Host"),        1, 0, 26101, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Host (IN)"),   1, 0, 26111, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Host"),                1, 0, 35101, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Host (IN)"),           1, 0, 35111, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Host"),            1, 0, 36101, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Host (IN)"),       1, 0, 36111, LoopTestProc},

    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Func"),                1, 0, 15201, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Func (IN)"),           1, 0, 15211, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Func"),            1, 0, 16201, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Func (IN)"),       1, 0, 16211, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Func"),            1, 0, 25201, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Func (IN)"),       1, 0, 25211, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Func"),        1, 0, 26201, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Func (IN)"),   1, 0, 26211, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Func"),                1, 0, 35201, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Func (IN)"),           1, 0, 35211, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Func"),            1, 0, 36201, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Func (IN)"),       1, 0, 36211, LoopTestProc},

    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Both"),                1, 0, 15301, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Host Timing / Phys Mem Both (IN)"),           1, 0, 15311, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Both"),            1, 0, 16301, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Host Timing / Phys Mem Both (IN)"),       1, 0, 16311, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Both"),            1, 0, 25301, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Function Timing / Phys Mem Both (IN)"),       1, 0, 25311, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Both"),        1, 0, 26301, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Function Timing / Phys Mem Both (IN)"),   1, 0, 26311, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Both"),                1, 0, 35301, LoopTestProc},
    { TEXT("Bulk Host Blocking Test / Sync Timing / Phys Mem Both (IN)"),           1, 0, 35311, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Both"),            1, 0, 36301, LoopTestProc},
    { TEXT("Bulk Function Blocking Test / Sync Timing / Phys Mem Both (IN)"),       1, 0, 36311, LoopTestProc},
    { NULL, 0, 0, 0, NULL }
};

} // extern "C"


////////////////////////////////////////////////////////////////////////////////
extern "C"
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    static HINSTANCE s_hLibPerfScenario = 0;

    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
        case SPM_LOAD_DLL:
        // Flag for UNICODE
#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE

            //initialize Kato
            g_pKato =(CKato *)KatoGetDefaultObject();
            if(g_pKato == NULL)
            {
                return SPR_NOT_HANDLED;
            }
            KatoDebug(TRUE, KATO_MAX_VERBOSITY, KATO_MAX_VERBOSITY, KATO_MAX_LEVEL);
            g_pKato->Log(LOG_COMMENT,TEXT("g_pKato Loaded"));

            //reset test device info
            memset(g_pTstDevLpbkInfo, 0, sizeof(PUSB_TDEVICE_LPBKINFO)*MAX_USB_CLIENT_DRIVER);
            //update registry if needed
            if(g_fRegUpdated == FALSE)
            {
                NKDbgPrintfW(_T("Please stand by, installing usbperf.dll as the test driver..."));
                if(RegLoadUSBTestDll(TEXT("usbperf.dll"), FALSE) == FALSE)
                {
                    NKDbgPrintfW(_T("can not install usb test driver,  make sure it does exist!"));
                    return TPR_NOT_HANDLED;
                }
                g_fRegUpdated = TRUE;
            }

            if (g_hEvtDeviceRemoved == NULL) {
                g_hEvtDeviceRemoved = CreateEvent(NULL,TRUE,FALSE,TEXT("UsbTestDeviceRemove"));
                if (g_hEvtDeviceRemoved == NULL) {
                    g_pKato->Log(LOG_COMMENT,TEXT("WARNING!!! USBPERF cannot create removal notification event, some tests will be skipped."));
                }
            }

            //we are waiting for the connection
            if(g_pUsbDriver->GetCurAvailDevs () == 0)
            {
                HWND hDiagWnd = ShowDialogBox(TEXT("Connect USB Host device with USB Function device now!"));

                //CETK users: Please ignore following statement, this is only for automated test setup
                SetRegReady(1, 3);
                int iCnt = 0;
                while(g_pUsbDriver->GetCurAvailDevs () == 0)
                {
                    if(++iCnt == 10)
                    {
                        DeleteDialogBox(hDiagWnd);
                        NKDbgPrintfW(_T("!!!NO device is attatched, test exit without running any test cases.!!!"));
                        return TPR_NOT_HANDLED;
                    }

                    NKDbgPrintfW(_T("Connect USB Host device with USB Function device now (%i)"),iCnt);
                    Sleep(2000);
                }
                DeleteDialogBox(hDiagWnd);
            }

            // Load PerfScenario
            s_hLibPerfScenario = LoadLibrary(PERFSCENARIO_DLL);
            if (s_hLibPerfScenario)
            {
                g_lpfnPerfScenarioOpenSession  = reinterpret_cast<PFN_PERFSCEN_OPENSESSION>  (GetProcAddress(s_hLibPerfScenario, L"PerfScenarioOpenSession"));
                g_lpfnPerfScenarioCloseSession = reinterpret_cast<PFN_PERFSCEN_CLOSESESSION> (GetProcAddress(s_hLibPerfScenario, L"PerfScenarioCloseSession"));
                g_lpfnPerfScenarioAddAuxData   = reinterpret_cast<PFN_PERFSCEN_ADDAUXDATA>   (GetProcAddress(s_hLibPerfScenario, L"PerfScenarioAddAuxData"));
                g_lpfnPerfScenarioFlushMetrics = reinterpret_cast<PFN_PERFSCEN_FLUSHMETRICS> (GetProcAddress(s_hLibPerfScenario, L"PerfScenarioFlushMetrics"));

                if ((NULL == g_lpfnPerfScenarioOpenSession)  ||
                    (NULL == g_lpfnPerfScenarioCloseSession) ||
                    (NULL == g_lpfnPerfScenarioAddAuxData)   ||
                    (NULL == g_lpfnPerfScenarioFlushMetrics))
                {
                    g_PerfScenarioStatus = PERFSCEN_ABORT;
                    g_pKato->Log(LOG_FAIL, L"GetProcAddress(PERFSCENARIO_DLL) FAILED.");
                }
                else
                {
                    g_PerfScenarioStatus = PERFSCEN_INITIALIZED;
                }
            }
            else
            {
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                g_pKato->Log(LOG_FAIL, L"LoadLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x", GetLastError());
            }

            Sleep(2000);
            break;

        case SPM_UNLOAD_DLL:
            if (0 != g_uiProfilingInterval)
            {
                ProfileStop();
            }
            if (g_fCeLog)
            {
                if (NULL != g_hFlushEvent)
                {
                    SetEvent(g_hFlushEvent);
                    Sleep(300);
                }
                CeLogMsg(TEXT("<<<USBPerf: finished."));
                if (NULL != g_hFlushEvent)
                {
                    SetEvent(g_hFlushEvent);
                    Sleep(10);
                    CloseHandle(g_hFlushEvent);
                    g_hFlushEvent = NULL;
                }
            }
            for(int i = 0; i < MAX_USB_CLIENT_DRIVER; i++)
            {
                if(g_pTstDevLpbkInfo[i] != NULL)
                {
                    delete g_pTstDevLpbkInfo[i];
                }
            }
            DeleteRegEntry();
            if(g_fUnloadDriver == TRUE)
                if(RegLoadUSBTestDll(TEXT("usbperf.dll"), TRUE))
                    g_fRegUpdated = FALSE;

            if (g_hEvtDeviceRemoved != NULL)
            {
                CloseHandle(g_hEvtDeviceRemoved);
                g_hEvtDeviceRemoved = NULL;
            }

            if (s_hLibPerfScenario) {
                BOOL bRet = FreeLibrary(s_hLibPerfScenario);
                if (!bRet)
                {
                    g_pKato->Log(LOG_FAIL, L"FreeLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x", GetLastError());
                }
                g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;

                
                if (0 != wcscmp(g_szPerfOutputFilename, PERF_OUTFILE))
                {
                    bRet = CopyFile(g_szPerfOutputFilename, PERF_OUTFILE, FALSE);
                    if (!bRet)
                    {
                        g_pKato->Log(LOG_FAIL, L"CopyFile(%s) FAILED.  GLE=%#x", PERF_OUTFILE, GetLastError());
                    }
                }
            }
            break;

        case SPM_SHELL_INFO:
            
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
            if (NULL != g_pShellInfo)
            {
                if (NULL != g_pShellInfo->szDllCmdLine)
                {
                    const TCHAR* pUSBPerfParam;
                    NKDbgPrintfW(TEXT("Command line: \"%s\"\r\n"),g_pShellInfo->szDllCmdLine);

                    //
                    // Check if Profiling is requested and set interval, when one is specified
                    //
                    pUSBPerfParam = _tcspbrk(g_pShellInfo->szDllCmdLine,_T("Pp"));
                    if (pUSBPerfParam)
                    {
                        pUSBPerfParam++;
                        g_uiProfilingInterval = _tstoi(pUSBPerfParam);
                        if (g_uiProfilingInterval < 5 || g_uiProfilingInterval > 5000)
                            g_uiProfilingInterval = USB_DEFAULT_PROFILER_INTERVAL;
                        ProfileStart(50000, PROFILE_BUFFER); // use large interval to avoid immediate hits
                        ProfileStart(g_uiProfilingInterval,(PROFILE_BUFFER|PROFILE_PAUSE));
                    }

                    //
                    // Check if CeLog usage is requested
                    //
                    pUSBPerfParam = _tcspbrk(g_pShellInfo->szDllCmdLine,_T("Ll"));
                    if (pUSBPerfParam)
                    {

                        DWORD dwZoneUser, dwZoneCE, dwZoneProcess, dwZoneAvail;

                        
                        g_fCeLog = CeLogGetZones(&dwZoneUser,&dwZoneCE,&dwZoneProcess,&dwZoneAvail);
                        if (g_fCeLog)
                        {
                            NKDbgPrintfW(TEXT("USBPerf.DLL -- CeLog zones: User=%08x, CE=%08x, Proc=%08x, Avail=%08x"),
                                                dwZoneUser,dwZoneCE,dwZoneProcess,dwZoneAvail);
                            dwZoneCE &= CELZONE_UNINTERESTING;
                            dwZoneCE |= CELZONE_INTERESTING;
                            NKDbgPrintfW(TEXT("     ZoneCE: set to %08x"),dwZoneCE);
                            CeLogSetZones(dwZoneUser,dwZoneCE,dwZoneProcess);

                            g_hFlushEvent = OpenEvent(EVENT_MODIFY_STATE,FALSE,CELOG_FILLEVENT_NAME);
                            if (NULL != g_hFlushEvent)
                            {
                                PulseEvent(g_hFlushEvent);
                                Sleep(300);
                            }

                            if (0 != g_uiProfilingInterval)
                            {
                                CeLogMsg(TEXT(">>>USBPerf: profiling at %u usec interval."),g_uiProfilingInterval);
                            }
                            else
                            {
                                CeLogMsg(TEXT(">>>USBPerf: started."));
                            }

                            CeLogReSync();
                        }
                    }
            
                } // szDllCmdLine != NULL
            } // spParam != NULL
            break;

        case SPM_REGISTER:
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
            return SPR_HANDLED;
#endif // UNICODE

        case SPM_START_SCRIPT:
            break;

        case SPM_STOP_SCRIPT:
            break;

        case SPM_BEGIN_GROUP:
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: USBPerf.DLL"));
            break;

        case SPM_END_GROUP:
            g_pKato->EndLevel(TEXT("END GROUP: USBPerf.DLL"));
            break;

        case SPM_BEGIN_TEST:
            if (g_fCeLog)
            {
                if (NULL != g_hFlushEvent)
                {
                    PulseEvent(g_hFlushEvent);
                    Sleep(300);
                }
                CeLogReSync();
            }
            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription,
                                pBT->dwThreadCount,
                                pBT->dwRandomSeed);
            break;

        case SPM_END_TEST:
            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                              pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                              pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
            if (g_fCeLog)
            {
                if (NULL != g_hFlushEvent)
                {
                    PulseEvent(g_hFlushEvent);
                    Sleep(1000);
                }
            }
            break;

        case SPM_EXCEPTION:
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            break;

        default:
            return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}


//user can specify which test device to connect to for the test
//this test dll allows user to host hook up more than one test device, and running tests
// through these test devices simultaneously
BOOL ParseCommandLine(LPUSB_PARAM lpUsbParam, LPCTSTR lpCommandLine)
{
    ASSERT(lpUsbParam);
    memset(lpUsbParam,0,sizeof(USB_PARAM));
    DWORD dwNum;

    while (lpCommandLine && *lpCommandLine)
    {
        switch (*lpCommandLine)
        {
            case _T('d'):case _T('D'):
                dwNum = 0;
                lpCommandLine++;
                while (*lpCommandLine>=_T('0') && *lpCommandLine<= _T('9'))
                {
                    dwNum = dwNum*10 + *lpCommandLine - _T('0');
                    lpCommandLine++;
                }
                lpUsbParam->dwSelectDevice = dwNum;
                DEBUGMSG(DBG_INI, (_T("Select Device %d "),lpUsbParam->dwSelectDevice));
                break;

            default:
                lpCommandLine++;
                break;
        }
    }
    return TRUE;
}

//###############################################
//following APIs reconfig test device side
//###############################################


//
// Helper Function to perform after any Re-Configuration.
//
// On success, a new unlocked instance of Client Driver had been created.
// On failure, either the old instance remains, and it is unlocked,
// or no Client Driver Instance is available altogether.
//
// It is responsibility of the caller to:
//  - refresh its pointer of Client Driver Instance, if one exists; and
//  - when the instance exists, to lock its critical section.
//
BOOL WaitForReflectorReset(UsbClientDrv* pDriverPtr, int iUsbDriverIndex)
{
    BOOL bRet = FALSE;
#ifdef REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION
    if (pDriverPtr) 
    {
        bRet = TRUE;
        Sleep(1000);
    }
#else // ~ REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION
    if(pDriverPtr != NULL)
    {
        DWORD dwConfigRetries = 0;
        DWORD dwTicks = GetTickCount();

        // wait until DeviceNotify routine is called by USBD for device removal
        DWORD dwRet = WaitForSingleObject(g_hEvtDeviceRemoved,RECONFIG_TIMEOUT);
                
        if (WAIT_OBJECT_0 == dwRet) {
            // check if the old instance is nullified
            if (g_pUsbDriver->GetAt(iUsbDriverIndex) != NULL) 
            {
                g_pKato->Log(LOG_DETAIL, TEXT("Old driver removal from array at %u not detected, expected in full-speed test case."), iUsbDriverIndex);
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, TEXT("Old driver removal from array detected!"));
            }
            ResetEvent(g_hEvtDeviceRemoved);

            Sleep(RECONFIG_RESET);
            //Wait for new instance to be added to array.
            while (dwConfigRetries <= RECONFIG_RETRIES)
            {
                //
                // After Reflector resets, we need to re-fetch driver instance -
                // from HOST prospective, this is new functional device freshly attached.
                //
                pDriverPtr = g_pUsbDriver->GetAt(iUsbDriverIndex);
                if (pDriverPtr != NULL)
                {
                    __try {
                        pDriverPtr->Lock();  //Keeps the current instance
                        bRet = TRUE;
                        break;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        pDriverPtr = NULL;
                    }
                }
                dwConfigRetries++;
                Sleep(RECONFIG_DELAY);
            }

            if (pDriverPtr != NULL) {
                pDriverPtr->Unlock();
                g_pKato->Log(LOG_DETAIL, TEXT("New (re-configured) Driver 0x%08x at %u ready in %u msec\r\n"),
                                                pDriverPtr,iUsbDriverIndex,GetTickCount()-dwTicks);
            }
            else {
                g_pKato->Log(LOG_FAIL, TEXT("Cannot obtain Driver handle for re-configured Reflector at %i, failing!\r\n"),iUsbDriverIndex);
            }
        }
        else if (dwRet == WAIT_TIMEOUT) {
            g_pKato->Log(LOG_FAIL, TEXT("Wait for device removal at %u timed out, failing!\r\n"),iUsbDriverIndex);
        }
        else /* if (dwRet == WAIT_FAILED) */{
            g_pKato->Log(LOG_FAIL, TEXT("Wait for device removal at %u failed.  GetLastError = %u = %#x\r\n"),iUsbDriverIndex, GetLastError(), GetLastError());
        }
    }

#endif // REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION
    if(bRet)
    {
        pDriverPtr->Lock();
        pDriverPtr->ResetDefaultPipe(); // just in case
        Sleep(RECONFIG_DELAY);
        g_pTstDevLpbkInfo[iUsbDriverIndex] = GetTestDeviceLpbkInfo(pDriverPtr); // new endpoint geometry

        if(g_pTstDevLpbkInfo[iUsbDriverIndex] == NULL || g_pTstDevLpbkInfo[iUsbDriverIndex]->uNumOfPipePairs == 0) //failed or no data pipes
        {
            g_pKato->Log(LOG_FAIL, TEXT("Cannot obtain pipe pairs for re-configured Reflector, failing!\r\n"));
            bRet = FALSE;
        }
        pDriverPtr->Unlock();
    }
    else if(pDriverPtr == NULL)
    {
        g_pKato->Log(LOG_FAIL, TEXT("pDriverPtr == NULL, failing!\r\n"));
    }
    return bRet;
}


DWORD
SetTestDeviceConfiguration(UsbClientDrv **ppDriverPtr, BOOL bIsocConfig, BOOL bHighSpeed, int iUsbDriverIndex)
{
    if(*ppDriverPtr == NULL)
        return TPR_FAIL;

#ifdef REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION

    DEBUGMSG(DBG_DETAIL,(TEXT("USB_REQUEST_SET_CONFIGURATION for best performance results on %s pipe"),
        bIsocConfig?TEXT("ISOCHRONOUS"):TEXT("INTERRUPT")));

    USB_DEVICE_REQUEST  usbRequest;
    usbRequest.bmRequestType = USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_STANDARD | USB_REQUEST_FOR_DEVICE;
    usbRequest.bRequest = USB_REQUEST_SET_CONFIGURATION;
    usbRequest.wValue = bIsocConfig?CONFIG_ALTERNATIVE:CONFIG_DEFAULT;
    usbRequest.wIndex = 0;
    usbRequest.wLength = 0;

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, (PUSB_TDEVICE_REQUEST)(&usbRequest), NULL) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("USB_REQUEST_SET_CONFIGURATION failed."));
        return TPR_FAIL;
    }

    DEBUGMSG(DBG_DETAIL,(TEXT("USB_REQUEST_SET_CONFIGURATION to #%u succeeded\r\n\r\n"),
        bIsocConfig?CONFIG_ALTERNATIVE:CONFIG_DEFAULT));

#else // ~REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION

    DEBUGMSG(DBG_DETAIL,(TEXT("TEST_REQUEST_RECONFIG for best performance results on %s pipe"),
        bIsocConfig?TEXT("ISOCHRONOUS"):TEXT("INTERRUPT")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = bIsocConfig?CONFIG_ALTERNATIVE:CONFIG_DEFAULT;
    utrc.ucInterface = 0;
    utrc.ucSpeed = SPEED_HIGH_SPEED;
    utrc.wBulkPkSize = 0;
    utrc.wIntrPkSize = 0;
    utrc.wIsochPkSize = 0;

    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, &utdr, (LPVOID)&utrc) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("TEST_REQUEST_RECONFIG failed."));
        return TPR_FAIL;
    }
    DEBUGMSG(DBG_DETAIL,(TEXT("TEST_REQUEST_RECONFIG to #%u succeeded\r\n\r\n"),
        bIsocConfig?CONFIG_ALTERNATIVE:CONFIG_DEFAULT));


    // In the case of full-speed, there's no notification received that the 
    // reflector has actually been reset as part of SendVendorTransfer().
    // So we wait for the reflector to complete its reset, and then cause a
    // reset of the host side port.
    (*ppDriverPtr)->Unlock();
    if (!bHighSpeed)
    {
        g_pKato->Log(LOG_DETAIL, L"Current connection is full-speed.");
        if (g_hEvtDeviceRemoved) 
        {
            // Wait for reflector to reset after reconfiguration.
            Sleep(RECONFIG_RESET);

            // Cause a reset and re-enumeration of the host.
            if (TPR_PASS != ResetDevice(*ppDriverPtr, TRUE)) 
            {
                g_pKato->Log(LOG_FAIL, L"Device failed to reset and re-enumerate.\r\n");
                return TPR_FAIL;
            }
        }
        else 
        {
            return TPR_SKIP;
        }
    }

    // Wait for reset completion
    if(!WaitForReflectorReset(*ppDriverPtr, iUsbDriverIndex)) // must wait for REFLECTOR to detach before Reset completion
    {
        *ppDriverPtr = NULL;
        return TPR_FAIL;
    }
    
    // driver must be locked before return
    *ppDriverPtr = g_pUsbDriver->GetAt(iUsbDriverIndex);
    if (*ppDriverPtr == NULL)
    {
        return TPR_FAIL;
    }
    (*ppDriverPtr)->Lock();
    

#endif // REFLECTOR_SUPPORTS_USB_REQUEST_SET_CONFIGURATION

    return TPR_PASS;
}

//###############################################
//following API retrieve test device's config information
//###############################################

#define DEVICE_TO_HOST_MASK  0x80

PUSB_TDEVICE_LPBKINFO
GetTestDeviceLpbkInfo(UsbClientDrv * pDriverPtr)
{
    if(pDriverPtr == NULL)
    {
        return FALSE;
    }
    PUSB_TDEVICE_LPBKINFO pTDLpbkInfo = NULL;
    USB_TDEVICE_REQUEST utdr;

    DEBUGMSG(DBG_DETAIL,(TEXT("Get test device's information about loopback pairs")));

    utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
    utdr.bRequest = TEST_REQUEST_PAIRNUM;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(UCHAR);

    UCHAR uNumofPairs = 0;
    //issue command to netchip2280 -- this time get number of pairs
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)&uNumofPairs) == FALSE)
    {
        NKDbgPrintfW(TEXT("get number of loopback pairs on test device failed\r\n"));
        return NULL;
    }

    if(uNumofPairs == 0)
    {
        NKDbgPrintfW(TEXT("No loopback pairs avaliable on this test device!!!\r\n"));
        return NULL;
    }

    USHORT uSize = sizeof(USB_TDEVICE_LPBKINFO) + sizeof(USB_TDEVICE_LPBKPAIR)*(uNumofPairs-1);
    pTDLpbkInfo = NULL;
    //allocate
    pTDLpbkInfo = (PUSB_TDEVICE_LPBKINFO) new BYTE[uSize];
    if(pTDLpbkInfo == NULL)
    {

        NKDbgPrintfW(_T("Out of memory!"));
        return NULL;
    }
    else
    {
        memset(pTDLpbkInfo, 0, uSize);
    }
    //issue command to netchip2280 -- this time get pair info
    utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
    utdr.bRequest = TEST_REQUEST_LPBKINFO;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = uSize;
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)pTDLpbkInfo) == FALSE)
    {
        NKDbgPrintfW(TEXT("get info of loopback pairs on test device failed\r\n"));
        delete[]  (PBYTE)pTDLpbkInfo;
        return NULL;
    }

    return pTDLpbkInfo;

}

//###############################################
//following APIs are support functions for test driver setup
//###############################################

static  const   TCHAR   gcszThisFile[]   = { TEXT("USBPERF, USBPERF.CPP") };

BOOL RegLoadUSBTestDll(LPTSTR szDllName, BOOL bUnload)
{
    BOOL    fRet = FALSE;
    BOOL    fException;
    DWORD   dwExceptionCode = 0;

    /*
    *   Make sure that all the required entry points are present in the USBD driver
    */
    LoadDllGetAddress(TEXT("USBD.DLL"), USBDEntryPointsText, (LPDLL_ENTRY_POINTS) & USBDEntryPoints);
    UnloadDll((LPDLL_ENTRY_POINTS) & USBDEntryPoints);

    /*
    *   Make sure that all the required entry points are present in the USB client driver
    */
    fRet = LoadDllGetAddress(szDllName, USBDriverEntryText, (LPDLL_ENTRY_POINTS) & USBDriverEntry);
    if (!fRet)
    {
        return FALSE;
    }

    if (bUnload)
    {
        Log(TEXT("%s: UnInstalling library \"%s\".\r\n"), gcszThisFile, szDllName);
        __try
        {
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpUnInstallDriver)();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if (fException)
        {
            Log(TEXT("%s: USB Driver UnInstallation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"),
                gcszThisFile,
                dwExceptionCode,
                GetLastError()
                );
        }
        if (!fRet)
        {
            Fail(TEXT("%s: UnInstalling USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName);
        }
        else
        {
            Log (TEXT("%s: UnInstalling USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName);
        }
    }
    else
    {
        Log(TEXT("%s: Installing library \"%s\".\r\n"), gcszThisFile, szDllName);
        __try
        {
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpInstallDriver)(szDllName);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if (fException)
        {
            Fail(TEXT("%s: USB Driver Installation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"),
                gcszThisFile,
                dwExceptionCode,
                GetLastError()
                );
        }
        if (!fRet)
        {
            Fail(TEXT("%s: Installing USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName);
        }
        else
        {
            Log (TEXT("%s: Installing USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName);
        }
    }

    UnloadDll((LPDLL_ENTRY_POINTS) & USBDriverEntry);

    return(TRUE);
}

HWND ShowDialogBox(LPCTSTR szPromptString)
{
    if(szPromptString == NULL)
    {
        return NULL;
    }
    //create a dialog box
    HWND hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_REMOVECARD), NULL, NULL);
    if(NULL == hDiagWnd)
    {
        NKDbgPrintfW(TEXT("WARNING: Can not create dialog box! "));
    }

    //show dialog
    ShowWindow(hDiagWnd, SW_SHOW);

    //show the prompt info
    SetDlgItemText(hDiagWnd, IDC_TEXT1, szPromptString);
    UpdateWindow(hDiagWnd);

    //debug output msg for headless tests.
    NKDbgPrintfW(szPromptString);

    return hDiagWnd;

}

VOID DeleteDialogBox(HWND hDiagWnd)
{
    //destroy the dialogbox
    if(NULL != hDiagWnd)
    {
        DestroyWindow(hDiagWnd);
    }
    hDiagWnd = NULL;
}


//###############################################
//following APIs are for auotmation use. CETK users can ignore them
//###############################################

#define REG_KEY_FOR_USBCONNECTION   TEXT("\\Drivers\\USBBoardConnection")
#define REG_BOARDNO_VALNAME  TEXT("BoardNo")
#define REG_PORTNO_VALNAME  TEXT("PortNo")

VOID SetRegReady(BYTE BoardNo, BYTE PortNo)
{

    DWORD dwBoardNo = 1;
    DWORD dwPortNo = 0;
    if(BoardNo >= 1 && BoardNo <= 5)
    {
        dwBoardNo = BoardNo;
    }
    if(PortNo <= 3)
    {
        dwPortNo = PortNo;
    }

    HKEY hKey = NULL;
    DWORD dwTemp;
    DWORD dwErr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION,
                                                    0, NULL, 0, 0, NULL, &hKey, &dwTemp);
    if(dwErr == ERROR_SUCCESS)
    {
        DWORD dwLen = sizeof(DWORD);
        dwErr = RegSetValueEx(hKey, REG_BOARDNO_VALNAME, 0, REG_DWORD, (PBYTE)&dwBoardNo, dwLen);
        dwErr = RegSetValueEx(hKey, REG_PORTNO_VALNAME, NULL, REG_DWORD, (PBYTE)&dwPortNo, dwLen);
        NKDbgPrintfW(_T("SetRegReady!BoardNO = %d, PortNo = %d"), dwBoardNo, dwPortNo);
        RegCloseKey(hKey);
    }

}

VOID DeleteRegEntry()
{
    HKEY hKey = NULL;
    DWORD dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION, 0, 0, &hKey);

    if(dwErr == ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        RegDeleteKey(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION);
    }

}


//###############################################
//Vendor transfer API
//###############################################
BOOL SendVendorTransfer(UsbClientDrv *pDriverPtr, BOOL bIn, PUSB_TDEVICE_REQUEST pUtdr, LPVOID pBuffer)
{
    if(pDriverPtr == NULL || pUtdr == NULL)
    {
        return FALSE;
    }

    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL,
                                                             (bIn == TRUE)?USB_IN_TRANSFER:USB_OUT_TRANSFER,
                                                             (PUSB_DEVICE_REQUEST)pUtdr,
                                                             pBuffer, 0);
    if(hVTransfer == NULL)
    {
        NKDbgPrintfW(TEXT("issueVendorTransfer failed\r\n"));
        return FALSE;
    }

    int iCnt = 0;
    //we will wait for about 3 minutes
    while(iCnt < 1000*9)
    {
        if((pDriverPtr->lpUsbFuncs)->lpIsTransferComplete(hVTransfer) == TRUE)
        {
            break;
        }
        Sleep(20);
        iCnt++;
    }

    if(iCnt >= 1000*9) //time out
    {
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
        NKDbgPrintfW(TEXT("issueVendorTransfer time out!\r\n"));
        return FALSE;
    }

    DWORD dwError = USB_NO_ERROR;
    DWORD cbdwBytes = 0;
    BOOL bRet = (pDriverPtr->lpUsbFuncs)->lpGetTransferStatus(hVTransfer, &cbdwBytes, &dwError);
    if(bRet == FALSE || dwError != USB_NO_ERROR || cbdwBytes != pUtdr->wLength)
    {
        NKDbgPrintfW(TEXT("IssueVendorTransfer has encountered an error! dwError = 0x%x,  cbdwBytes = %d\r\n"), dwError, cbdwBytes);

        bRet = FALSE;
    }

    (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);

    return bRet;
}


DWORD ResetDevice(UsbClientDrv *pDriverPtr, BOOL bReEnum)
{
    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(bReEnum == TRUE)
        g_pKato->Log(LOG_DETAIL, _T("Now Resetting the device and will re-enumerate it\r\n"));
    else
        g_pKato->Log(LOG_DETAIL, _T("Now Resetting the device without re-enumeration\r\n"));

    if(pDriverPtr->DisableDevice(bReEnum, 0) == FALSE){
        g_pKato->Log(LOG_FAIL, _T("Failed to disable the device and %s.\r\n"),bReEnum?_T("re-enumerate it"):_T("leave it disabled"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

