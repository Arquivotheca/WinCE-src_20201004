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
    usbtest.cpp

Abstract:
    USB Client Driver for testing USBDriver.

Author:
    davli

Modified:
    shivss

Functions:

Notes:
--*/

#define __THIS_FILE__   TEXT("usbtest.cpp")
#include <windows.h>
#include <usbdi.h>
#include "loadfn.h"
#include "usbtest.h"
#include "UsbClass.h"
#include "PipeTest.h"
#include "TransTst.h"
#include "resource.h"
#include "basetst.h"
#include "usbbts.h"


//globals
PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[MAX_USB_CLIENT_DRIVER] = {NULL,};

HINSTANCE g_hInst = NULL;

BOOL g_fUnloadDriver = FALSE; //unload usbtest.dll from usbd.dll if true
BOOL g_fRegUpdated = FALSE; //registry update only needs to be done once, unless unloaded

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

CKato            *g_pKato=NULL;
SPS_SHELL_INFO   *g_pShellInfo;

DWORD   g_dwNotifyMsg = 0;

//Currently, only one Reflector is used at a time, so only one event is needed.
HANDLE  g_hEvtDeviceRemoved = NULL;


// ----------------------------------------------------------------------------
//
// Debugger
//
// ----------------------------------------------------------------------------
#ifdef DEBUG

//
// These defines must match the ZONE_* defines
//
#define INIT_ERR            1
#define INIT_WARN           2
#define INIT_FUNC           4
#define INIT_DETAIL         8
#define INIT_INI            16
#define INIT_USB            32
#define INIT_IO             64
#define INIT_WEIRD          128
#define INIT_EVENTS         256
#define INIT_USB_CONTROL    0x200
#define INIT_USB_BULK       0x400
#define INIT_USB_INTERRUPT  0x800
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

BOOL WINAPI DdlxDLLEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) ;
extern "C" LPCTSTR DdlxGetCmdLine( TPPARAM tpParam );



extern "C" BOOL WINAPI DllMain(HANDLE hDllHandle, ULONG dwReason, LPVOID lpreserved)
{
    switch (dwReason)    {
      case DLL_PROCESS_ATTACH:
        DEBUGREGISTER((HINSTANCE)hDllHandle);
        g_pUsbDriver= new USBDriverClass();
        g_hInst = (HINSTANCE)hDllHandle;
        DisableThreadLibraryCalls((HMODULE) hDllHandle);
        break;
      case DLL_PROCESS_DETACH:
        delete g_pUsbDriver;
        g_pUsbDriver=NULL;
      default:
        break;
    }
    return DdlxDLLEntry((HINSTANCE)hDllHandle,dwReason,lpreserved) ;
}


//###############################################
//following APIs deal with test driver (usbtest.dll) registration issues
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


        DriverSettings.dwVendorId           = 0x045e;
        DriverSettings.dwProductId          = USB_NO_INFO;
        DriverSettings.dwReleaseNumber      = USB_NO_INFO;

        DriverSettings.dwDeviceClass        = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass     = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol     = USB_NO_INFO;

        DriverSettings.dwInterfaceClass     = 0x0F;        //
        DriverSettings.dwInterfaceSubClass  = USB_NO_INFO; // boot device
        DriverSettings.dwInterfaceProtocol  = USB_NO_INFO; // mouse



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
        DriverSettings.dwVendorId           = 0x045e;
        DriverSettings.dwProductId          = USB_NO_INFO;
        DriverSettings.dwReleaseNumber      = USB_NO_INFO;

        DriverSettings.dwDeviceClass        = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass     = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol     = USB_NO_INFO;

        DriverSettings.dwInterfaceClass     = 0x0F;
        DriverSettings.dwInterfaceSubClass  = USB_NO_INFO; // boot device
        DriverSettings.dwInterfaceProtocol  = USB_NO_INFO; // mouse

        fRet =usbDriver.UnRegisterClientSettings(TEXT("Generic UFN Test Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("Generic UFN Test Loopback Driver"));

    }

    return fRet;
}

//This gets called when the old instance is removed and a new one comes back.
BOOL WINAPI DeviceNotify(LPVOID lpvNotifyParameter,DWORD dwCode,LPDWORD * ,LPDWORD * ,LPDWORD * ,LPDWORD * )
{
    //Write the Notification Message
    g_dwNotifyMsg = dwCode;

    switch(dwCode)
    {
        case USB_CLOSE_DEVICE:
            g_pKato->Log(LOG_DETAIL, TEXT("USBTest: Close Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
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
            g_pKato->Log(LOG_DETAIL, TEXT("USBTest: Suspend Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            break;
        case USB_RESUMED_DEVICE:
            g_pKato->Log(LOG_DETAIL, TEXT("USBTest: Resume Device Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            break;
        default:
            g_pKato->Log(LOG_DETAIL, TEXT("USBTest: Unknown Notification: NotifyParam=%#x\r\n"),lpvNotifyParameter);
            break;
    }
    return TRUE;
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
    DEBUGMSG(DBG_USB, (TEXT("Device Attach")));
    *fAcceptControl = FALSE;

    if(g_pUsbDriver == NULL || lpInterface == NULL)
        return FALSE;

    DEBUGMSG(DBG_USB, (TEXT("USBTest: DeviceAttach, IF %u, #EP:%u, Class:%u, Sub:%u, Prot:%u\r\n"),
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
             DEBUGMSG(DBG_USB, (TEXT("Device Attach failed")));
        return FALSE;
    }
    (*lpUsbFuncs->lpRegisterNotificationRoutine)(hDevice,DeviceNotify, pLocalDriverPtr);
    *fAcceptControl = TRUE;
    DEBUGMSG(DBG_USB, (TEXT("Device Attach success")));
    return TRUE;
}


//###############################################
// TUX test cases and distribution
//###############################################

TESTPROCAPI LoopTestProc(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{

    if(uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    DEBUGMSG(DBG_FUNC, (TEXT("LoopTestProc")));

    if(!g_fRegUpdated && lpFTE->dwUniqueID != USBDAPI_CASE+14)
    {
        g_pKato->Log(LOG_COMMENT, TEXT("WARNING:  usbtest.dll not loaded in usbd!  Try re-running or run test case %u"),USBDAPI_CASE+14);
    }

    if (g_pUsbDriver==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("No USB Function Device Driver loaded, failing!"));
        return TPR_SKIP;
    }
    if(g_pUsbDriver->GetCurAvailDevs() == 0)  //Driver loaded, but no devices are attached.  Skip to prevent a search through the entire array.
    {
        g_pKato->Log(LOG_FAIL, TEXT("No REFLECTOR detected, failing!"));
        return TPR_SKIP;
    }

    DWORD dwRet = TPR_PASS;
    DWORD dwDriverNumber=0;
    USB_PARAM m_UsbParam = {0,0};

    LPCTSTR lpCommandLine = DdlxGetCmdLine( tpParam );

    if(lpCommandLine)
        ParseCommandLine(&m_UsbParam,lpCommandLine );
    else
        ParseCommandLine(&m_UsbParam,g_pShellInfo->szDllCmdLine);

    DWORD dwActiveDevice = m_UsbParam.dwSelectDevice!=0?m_UsbParam.dwSelectDevice:1;
    dwActiveDevice--;

    //make phsyical mem ready
    PPhysMemNode PMemN = new PhysMemNode[MY_PHYS_MEM_SIZE];

    for (int i=0;i<(int)(g_pUsbDriver->GetArraySize());i++) 
    {
        UsbClientDrv* pClientDrv = g_pUsbDriver->GetAt(i);

        // for tests which require RECONFIG (or RESET) of Reflector,
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

            if(g_pTstDevLpbkInfo[i] == NULL) {
                if(lpFTE->dwUniqueID < SPECIAL_CASE) {
                    g_pTstDevLpbkInfo[i] = GetTestDeviceLpbkInfo(pClientDrv);
                    if (g_pTstDevLpbkInfo[i] == NULL || g_pTstDevLpbkInfo[i]->uNumOfPipePairs == 0) {//failed or no data pipes
                        NKDbgPrintfW(_T("Can not get test device's loopback pairs' information or there's no data loopback pipe pair, test will be skipped for this device!!!"));
                        continue;
                    }
                } // SPECIAL_CASE
            } // (g_pTstDevLpbkInfo[i] == NULL)

            LPCUSB_DEVICE pDevice = pClientDrv->GetDeviceInfo();
            if (pDevice!=NULL) {
                //Set Configuration Selected
                if (m_UsbParam.bSelectConfig !=0) {
                    if (m_UsbParam.bSelectConfig <= pDevice->Descriptor.bNumConfigurations) {
                        if (!SetConfigDescriptor( pClientDrv,m_UsbParam.bSelectConfig)) {
                            NKDbgPrintfW(_T("SetConfigurationDescriptor Failed for Index = %u"),m_UsbParam.bSelectConfig);
                        }
                    }
                }
            } // (pDevice!=NULL)

            dwDriverNumber++;
            DEBUGMSG(DBG_INI, (TEXT("Device retrive from %d, drvNum=%u"),i,dwDriverNumber));

            BOOL    fAsync =    ( (lpFTE->dwUniqueID % 100) > 10) ? TRUE : FALSE;
            DWORD   dwTransType = (lpFTE->dwUniqueID % 1000 )/100;

            switch (lpFTE->dwUniqueID) {

            case BASE+1:
            case BASE+ASYNC_TEST+1:
            case BASE+MEM_TEST+1:
            case BASE+MEM_TEST+ASYNC_TEST+1:
            case BASE+ALIGN_TEST+1:
            case BASE+ALIGN_TEST+ASYNC_TEST+1:
            case BASE+SHORT_TEST+1:
            case BASE+SHORT_TEST+ASYNC_TEST+1:// bulk transfer
            case BASE+SHORTSTRESS_TEST+1:
            case BASE+ZEROLEN_TEST+1:
            case BASE+ZEROLEN_TEST+ASYNC_TEST+1:
                dwRet=SinglePairLoopback(pClientDrv,dwTransType, USB_ENDPOINT_TYPE_BULK, fAsync, PMemN, i)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+2:
            case BASE+ASYNC_TEST+2:
            case BASE+MEM_TEST+2:
            case BASE+MEM_TEST+ASYNC_TEST+2:
            case BASE+ALIGN_TEST+2:
            case BASE+ALIGN_TEST+ASYNC_TEST+2:
            case BASE+SHORT_TEST+2:
            case BASE+SHORT_TEST+ASYNC_TEST+2:// interrupt transfer
            case BASE+SHORTSTRESS_TEST+2:
            case BASE+ZEROLEN_TEST+2:
            case BASE+ZEROLEN_TEST+ASYNC_TEST+2:
                dwRet=SinglePairLoopback(pClientDrv, dwTransType, USB_ENDPOINT_TYPE_INTERRUPT, fAsync, PMemN, i)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+3:
            case BASE+ASYNC_TEST+3:
            case BASE+MEM_TEST+3:
            case BASE+MEM_TEST+ASYNC_TEST+3:
            case BASE+ALIGN_TEST+3:
            case BASE+ALIGN_TEST+ASYNC_TEST+3:
            case BASE+SHORT_TEST+3:
            case BASE+SHORT_TEST+ASYNC_TEST+3:// Isoch transfer
            case BASE+SHORTSTRESS_TEST+3:
                dwRet=SinglePairLoopback(pClientDrv, dwTransType, USB_ENDPOINT_TYPE_ISOCHRONOUS,fAsync, PMemN, i)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+4:
            case BASE+ASYNC_TEST+4:
            case BASE+MEM_TEST+4:
            case BASE+MEM_TEST+ASYNC_TEST+4:
            case BASE+ALIGN_TEST+4:
            case BASE+ALIGN_TEST+ASYNC_TEST+4:
            case BASE+SHORT_TEST+4:
            case BASE+SHORT_TEST+ASYNC_TEST+4:// all endpoints transfer
            case BASE+SHORTSTRESS_TEST+4:
                dwRet=AllPairLoopback(pClientDrv, dwTransType,  fAsync, PMemN, i)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+5:
            case BASE+ASYNC_TEST+5:
            case BASE+MEM_TEST+5:
            case BASE+MEM_TEST+ASYNC_TEST+5:
            case BASE+ALIGN_TEST+5:
            case BASE+ALIGN_TEST+ASYNC_TEST+5:
            case BASE+SHORT_TEST+5:
            case BASE+SHORT_TEST+ASYNC_TEST+5:// all types transfer
            case BASE+SHORTSTRESS_TEST+5:
                dwRet=AllTypeLoopback(pClientDrv, dwTransType,  fAsync, PMemN, i)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+ADD_TEST+1:
            case BASE+ADD_TEST+ASYNC_TEST+1:
                dwRet=SinglePairTrans(pClientDrv, 1, fAsync, i)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+ASYNC_TEST+2://only async can do abort
                dwRet=SinglePairTrans(pClientDrv, 2, fAsync, i)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+3:
            case BASE+ADD_TEST+ASYNC_TEST+3:
                dwRet=SinglePairTrans(pClientDrv, 3, fAsync, i)?TPR_PASS:TPR_FAIL;
                break;

            //special tests
            case BASE+ADD_TEST+5://large transfer
                dwRet=SinglePairSpecialTrans(pClientDrv, i, 1)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+6://bulk transfer with delays
                dwRet=SinglePairSpecialTrans(pClientDrv, i, 2)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+7://bulk transfer - close transfers in reverse order
                dwRet=SinglePairSpecialTrans(pClientDrv, i, 3)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+ADD_TEST+21://bulk transfer with device side stall
                dwRet=TransWithDeviceSideStalls(pClientDrv, i, 1)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+22://interrupt transfer with device side stall
                dwRet=TransWithDeviceSideStalls(pClientDrv, i, 2)?TPR_PASS:TPR_FAIL;
                break;

            case BASE+ADD_TEST+41://bulk short transfers
                dwRet=ShortTransferExercise(pClientDrv, i, 1)?TPR_PASS:TPR_FAIL;
                break;
            case BASE+ADD_TEST+42://interrupt short transfers
                dwRet=ShortTransferExercise(pClientDrv, i, 2)?TPR_PASS:TPR_FAIL;
                break;

            case CHAPTER9_CASE+1:
            case CHAPTER9_CASE+ASYNC_TEST+1:
                dwRet=GetDespscriptorTest(pClientDrv,fAsync)?TPR_PASS:TPR_FAIL;
                break;
            case CHAPTER9_CASE+2:
            case CHAPTER9_CASE+ASYNC_TEST+2:
                dwRet=GetInterfaceTest_A(pClientDrv,fAsync)?TPR_PASS:TPR_FAIL;
                break;
            case CHAPTER9_CASE+3:
            case CHAPTER9_CASE+ASYNC_TEST+3:
                dwRet=DeviceRequest(pClientDrv,fAsync)?TPR_PASS:TPR_FAIL;
                break;

            //following cases are used to change test device's configuation
            case CONFIG_CASE+1: //set lowest packet size for full speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = SetLowestPacketSize(&pClientDrv, FALSE, i);
                break;
            case CONFIG_CASE+2: //set lowest packet size for high speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = SetLowestPacketSize(&pClientDrv, TRUE, i);
                break;
            case CONFIG_CASE+3: //set alternative packet size for full speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = SetAlternativePacketSize(&pClientDrv, FALSE, i);
                break;
            case CONFIG_CASE+4: //set alternative packet size for high speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = SetAlternativePacketSize(&pClientDrv, TRUE, i);
                break;
            case CONFIG_CASE+5: //set highest packet size for high speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = SetHighestPacketSize(&pClientDrv, i);
                break;
            case CONFIG_CASE+11: //restore default packet size for full speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = RestoreDefaultPacketSize(&pClientDrv, FALSE, i);
                break;
            case CONFIG_CASE+12: //restore default packet size for high speed config
                if (g_hEvtDeviceRemoved == NULL)
                    dwRet = TPR_SKIP;
                else 
                    dwRet = RestoreDefaultPacketSize(&pClientDrv, TRUE, i);
                break;

            //Addtional test cases
            case USBDAPI_CASE+1: //Displays the USBDVersion
                dwRet = GetUSBDVersion(pClientDrv);
                break;

            case USBDAPI_CASE+2: //FrameLenght Control test 1
                dwRet = FrameLengthControlErrHandling(pClientDrv);
                break;

            case USBDAPI_CASE+3: //FrameLenght Control test 2
                dwRet = FrameLengthControlDoubleRelease(pClientDrv);
                break;

            case USBDAPI_CASE+4: //UnregisterClientSettings
                dwRet = AddRemoveRegistrySetting(pClientDrv);
                break;

            case USBDAPI_CASE+5: // HCD Select configuration
                dwRet = HcdSelectConfigurationTest(pClientDrv);
                break;

            case USBDAPI_CASE+6: //Find interfaces on USB device
                dwRet = FindInterfaceTest(pClientDrv);
                break;

            case USBDAPI_CASE+7: //Get client registry key
                dwRet = ClientRegistryKeyTest(pClientDrv);
                break;

            case USBDAPI_CASE+8: // Invalid parameter test for frame control and pipe control functions
                dwRet = InvalidParameterTest(pClientDrv);
                break;

            case USBDAPI_CASE+9: // Open same pipe twice
                dwRet = PipeParameterTest(pClientDrv);
                break;

            case USBDAPI_CASE+10: // Find a valid interface and load driver
                dwRet = LoadGenericInterfaceDriverTest(pClientDrv);
                break;

            case USBDAPI_CASE+11: // Vender tranfer control with invalid parameters
                dwRet = InvalidParameterForVendorTransfers(pClientDrv);
                break;

            case USBDAPI_CASE+12: // Pipe transfer control with invalid parameters
                dwRet = InvalidParameterTransfers(pClientDrv);
                break;

            case USBDAPI_CASE+13: //unload usbtest.dll.  usbtest.dll will get reloaded at next test run.
                dwRet = UnloadTestDriver();
                break;

            case USBDAPI_CASE+14: //load usbtest.dll.
                dwRet = LoadTestDriver();
                break;

            //special test cases
            case SPECIAL_CASE+1: //reset device test
                if (g_hEvtDeviceRemoved) {
                    dwRet = ResetDevice(pClientDrv, TRUE);
                    if (TPR_PASS == dwRet) {
                        if (WaitForReflectorReset(pClientDrv,i)) {
                            // if WaitForReflectorRest had succeeded, this implies
                            // that Client Driver Pointer will be a valid one
                            pClientDrv = g_pUsbDriver->GetAt(i);
                            pClientDrv->Lock();
                        }
                        else {
                            g_pKato->Log(LOG_FAIL, L"Device failed to reset and re-enumerate.\r\n");
                            dwRet = TPR_FAIL;
                        }
                    }
                }
                else {
                    dwRet = TPR_SKIP;
                }
                break;

            case SPECIAL_CASE+3: //reset device test
                dwRet = SuspendDevice(pClientDrv);
                break;
            case SPECIAL_CASE+4: //reset device test
                dwRet = ResumeDevice(pClientDrv);
                break;

            case SPECIAL_CASE+5: //EP0 test
                dwRet = EP0Test(pClientDrv, FALSE);
                break;
            case SPECIAL_CASE+6: //EP0 test with stalls
                dwRet = EP0Test(pClientDrv, TRUE);
                break;

            case SPECIAL_CASE+7: //EP0 test, IN zero length transfer
                dwRet = EP0TestWithZeroLenIn(pClientDrv);
                break;

            case SPECIAL_CASE+991: //reset device test
                if (g_hEvtDeviceRemoved) {
                    g_pKato->Log(LOG_DETAIL, L"*** NOTE: AFTER EXECUTION OF THIS TEST, YOU WILL NEED TO PHYSICALLY DETACH THE USB CABLE, OTHERWISE THE PORT WILL REMAIN DISABLED ***.\r\n");

                    dwRet = ResetDevice(pClientDrv, FALSE);
                    if (TPR_PASS == dwRet) {
                        if (WAIT_OBJECT_0 == WaitForSingleObject(g_hEvtDeviceRemoved,8000)) {
                            ResetEvent(g_hEvtDeviceRemoved);
                            // Device will not be re-enumerated, Client Driver is destroyed
                            // Physical detach and re-attach is required, else port will stay disabled
                            pClientDrv = NULL; 
                        }
                        else {
                            g_pKato->Log(LOG_FAIL, L"Reset Device without re-enumeration did not get removal notification.\r\n");
                            dwRet = TPR_FAIL;
                        }
                    }

                    g_pKato->Log(LOG_DETAIL, L"*** NOTE: AFTER EXECUTION OF THIS TEST, YOU WILL NEED TO PHYSICALLY DETACH THE USB CABLE, OTHERWISE THE PORT WILL REMAIN DISABLED ***.\r\n");
                }
                else {
                    dwRet = TPR_SKIP;
                }
                break;

            default:
                pClientDrv->Unlock();
                delete [] PMemN;
                g_pKato->Log(LOG_FAIL, TEXT("Unrecognized test case %u, failing."),lpFTE->dwUniqueID);
                ASSERT(0);
                return TPR_NOT_HANDLED;
            } // switch (lpFTE->dwUniqueID)

            if(pClientDrv != NULL)  //if Reflector Reconfiguration times out, this would be NULL, and already Unlocked.
            {
                //In certain cases, USBD can deallocate pClientDrv even though it is already locked.
                //This will catch this normal use Exception, and inform the tester.
                 __try
                {
                     pClientDrv->Unlock();
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    g_pKato->Log(LOG_DETAIL, L"ERROR:  %s encountered an exception (%u = %#x), while trying to unlock pClientDrv.\r\n",lpFTE->lpDescription,GetExceptionCode(),GetExceptionCode());
                    dwRet = TPR_FAIL;
                }
            }
            break;

        } // (pClientDrv && (dwActiveDevice==0))
        else {
            NKDbgPrintfW(_T("Unusual pair of client driver and index number: pClient[%u]=%x; dwActiveDevice%u"),i,pClientDrv,dwActiveDevice);
            if (pClientDrv) {
                dwActiveDevice--;
            }
        }

    } // for (i=0;...

    delete [] PMemN;

    if (dwDriverNumber==0) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
        return TPR_SKIP;
    }

    return dwRet;
}



////////////////////////////////////////////////////////////////////////////////
// TUX Function table
extern "C"{

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    {TEXT("USB Normal Driver Test(Sync)"),  0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+4,  LoopTestProc } ,
    { TEXT("All type Loopbacks Test "), 1,  0,  BASE+5,  LoopTestProc } ,

    {TEXT("USB Normal Driver Test(ASync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+4,  LoopTestProc } ,
    { TEXT("All type Loopbacks Test "), 1,  0,  BASE+ASYNC_TEST+5,  LoopTestProc } ,

    {TEXT("USB Normal Physical Mem Driver Test(Sync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+MEM_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+MEM_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+MEM_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+MEM_TEST+4,  LoopTestProc } ,

    {TEXT("USB Normal Physical Mem Driver Test(ASync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+MEM_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+MEM_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+MEM_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+MEM_TEST+4,  LoopTestProc } ,

    {TEXT("USB Align Physical Mem Driver Test(Sync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+ALIGN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+ALIGN_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+ALIGN_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+ALIGN_TEST+4,  LoopTestProc } ,

    {TEXT("USB Align Physical Mem Driver Test(ASync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+4,  LoopTestProc } ,

    {TEXT("USB short packet transfer Test(Sync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+SHORT_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+SHORT_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+SHORT_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+SHORT_TEST+4,  LoopTestProc } ,

    {TEXT("USB short packet transfer Test(ASync)"), 0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+SHORT_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+SHORT_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+SHORT_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+SHORT_TEST+4,  LoopTestProc } ,

    {TEXT("Additional transfer Test(Sync)"), 0,  0,  0, NULL},
    { TEXT("Normal transfer Test"),                 1,  0,  BASE+ADD_TEST+1,  LoopTestProc } ,
    { TEXT("Close transfer Test "),                 1,  0,  BASE+ADD_TEST+3,  LoopTestProc } ,
    { TEXT("Special test-large transfer "),         1,  0,  BASE+ADD_TEST+5,  LoopTestProc } ,
    { TEXT("Special test-with delays"),             1,  0,  BASE+ADD_TEST+6,  LoopTestProc } ,
    { TEXT("Special test-close in reverse order "), 1,  0,  BASE+ADD_TEST+7,  LoopTestProc } ,

    {TEXT("Additional transfer with stalls Test"), 0,  0,  0, NULL},
    { TEXT("Bulk transfer Test"),         1,  0,  BASE+ADD_TEST+21,  LoopTestProc } ,
    { TEXT("Interrupt transfer Test "),   1,  0,  BASE+ADD_TEST+22,  LoopTestProc } ,

    {TEXT("Short transfer tests"), 0,  0,  0, NULL},
    { TEXT("Bulk transfer Test"),       1,  0,  BASE+ADD_TEST+41,  LoopTestProc } ,
    { TEXT("Interrupt transfer Test "), 1,  0,  BASE+ADD_TEST+42,  LoopTestProc } ,

    {TEXT("Additional transfer Test(Async)"),   0,  0,  0, NULL},
    { TEXT("Normal transfer Test"),             1,  0,  BASE+ADD_TEST+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Abort transfer Test "),             1,  0,  BASE+ADD_TEST+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Close transfer Test "),             1,  0,  BASE+ADD_TEST+ASYNC_TEST+3,  LoopTestProc } ,

    {TEXT("USB Short packet stress Test(Sync)"),    0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),                   1,  0,  BASE+SHORTSTRESS_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test"),              1,  0,  BASE+SHORTSTRESS_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test"),            1,  0,  BASE+SHORTSTRESS_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopback Test"),           1,  0,  BASE+SHORTSTRESS_TEST+4,  LoopTestProc } ,

    {TEXT("USB zero-length transfer Test(Sync)"),   0,  0,  0, NULL},
    { TEXT("Bulk Loopback Test"),                   1,  0,  BASE+ZEROLEN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),             1,  0,  BASE+ZEROLEN_TEST+2,  LoopTestProc } ,

    {TEXT("USB zero-length transfer Test(ASync)"),  0, 0,  0, NULL},
    { TEXT("Bulk Loopback Test"),                   1,  1,  BASE+ASYNC_TEST+ZEROLEN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),             1,  1,  BASE+ASYNC_TEST+ZEROLEN_TEST+2,  LoopTestProc } ,

    {TEXT("Chapter 9 Test(Sync)"), 0,  0,  0, NULL},
    { TEXT("Descriptor related Test"),  1,  0,  CHAPTER9_CASE+1,  LoopTestProc } ,
    { TEXT("Interface related Test "),  1,  0,  CHAPTER9_CASE+2,  LoopTestProc } ,
    { TEXT("Device related Test "),     1,  0,  CHAPTER9_CASE+3,  LoopTestProc } ,

    {TEXT("Chapter 9 Test(Async)"), 0,  0,  0, NULL},
    { TEXT("Descriptor related Test"),  1,  0,  CHAPTER9_CASE+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Interface related Test "),  1,  0,  CHAPTER9_CASE+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Device related Test "),     1,  0,  CHAPTER9_CASE+ASYNC_TEST+3,  LoopTestProc } ,

    {TEXT("USB Driver set net2280 device config"), 0,  0,  0, NULL},
    { TEXT("set lowest packet sizes for full speed config"),        1,  0, CONFIG_CASE+1,  LoopTestProc } ,
    { TEXT("set lowest packet sizes for high speed config"),        1,  0, CONFIG_CASE+2,  LoopTestProc } ,
    { TEXT("set alternative packet sizes for full speed config"),   1,  0, CONFIG_CASE+3,  LoopTestProc } ,
    { TEXT("set alternative packet sizes for high speed config"),   1,  0, CONFIG_CASE+4,  LoopTestProc } ,
    { TEXT("set highest packet sizes for highl speed config"),      1,  0, CONFIG_CASE+5,  LoopTestProc } ,
    { TEXT("restore default packet sizes for full speed config"),   1,  0, CONFIG_CASE+11,  LoopTestProc } ,
    { TEXT("restore default packet sizes for high speed config"),   1,  0, CONFIG_CASE+12,  LoopTestProc } ,

    {TEXT("USB API  Test"),  0, 0,  0, NULL},
    { TEXT("Get USBD Version"),                         1,  0, USBDAPI_CASE+1,  LoopTestProc } ,
    { TEXT("FrameLengthControl Error Handling"),        1,  0, USBDAPI_CASE+2,  LoopTestProc } ,
    { TEXT("FrameLengthControl Double Release"),        1,  0, USBDAPI_CASE+3,  LoopTestProc } ,
    { TEXT("Register Unregister Client Driver Test"),   1,  0, USBDAPI_CASE+4,  LoopTestProc } ,
    { TEXT("HcdSelectConfiguration Test"),              1,  0, USBDAPI_CASE+5,  LoopTestProc } ,
    { TEXT("Find Interface"),                           1,  0, USBDAPI_CASE+6,  LoopTestProc } ,
    { TEXT("Client Registry Key Test"),                 1,  0, USBDAPI_CASE+7,  LoopTestProc } ,
    { TEXT("InvalidParameters Test"),                   1,  0, USBDAPI_CASE+8,  LoopTestProc } ,
    { TEXT("Pipe Parameter Test"),                      1,  0, USBDAPI_CASE+9,  LoopTestProc } ,
    { TEXT("LoadGenericInterfaceDriver Test"),          1,  0, USBDAPI_CASE+10,  LoopTestProc } ,
    { TEXT("InvalidVendorTransfer Test"),               1,  0, USBDAPI_CASE+11,  LoopTestProc } ,
    { TEXT("InvalidParameterTransfers Test"),           1,  0, USBDAPI_CASE+12,  LoopTestProc } ,
    { TEXT("UnloadTestDriver"),                         1,  0, USBDAPI_CASE+13,  LoopTestProc } ,
    { TEXT("LoadTestDriver"),                           1,  0, USBDAPI_CASE+14,  LoopTestProc } ,

    {TEXT("USB Driver device reset/suspend/resume test"),  0, 0,  0, NULL},
    { TEXT("Reset device"),     1,  0, SPECIAL_CASE+1,  LoopTestProc } ,
    { TEXT("Suspend device"),   1,  0, SPECIAL_CASE+3,  LoopTestProc } ,
    { TEXT("Resume device"),    1,  0, SPECIAL_CASE+4,  LoopTestProc } ,

    {TEXT("Additional test"), 0,  0,  0, NULL},
    { TEXT("EP0 data transfer test"),                       1,  0, SPECIAL_CASE+5,  LoopTestProc } ,
    { TEXT("EP0 data transfer with stall test"),            1,  0, SPECIAL_CASE+6,  LoopTestProc } ,
    { TEXT("EP0 data transfer with IN zero length packet"), 1,  0, SPECIAL_CASE+7,  LoopTestProc } ,

    {TEXT("USB Driver device remove test"), 0,  0,  0, NULL},
    { TEXT("Reset device no enum"),         1,  0, SPECIAL_CASE+991,  LoopTestProc } ,
    { NULL, 0, 0, 0, NULL }
};
}

////////////////////////////////////////////////////////////////////////////////
extern "C"
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{

    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg){
        case SPM_LOAD_DLL:
        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE

            //initialize Kato
            g_pKato =(CKato *)KatoGetDefaultObject();
            if(g_pKato == NULL)
            return SPR_NOT_HANDLED;
            KatoDebug( TRUE, KATO_MAX_VERBOSITY, KATO_MAX_VERBOSITY, KATO_MAX_LEVEL );
            g_pKato->Log(LOG_COMMENT,TEXT("g_pKato Loaded"));

            //reset test device info
            memset(g_pTstDevLpbkInfo, 0, sizeof(PUSB_TDEVICE_LPBKINFO)*MAX_USB_CLIENT_DRIVER);
            //update registry if needed
            if(g_fRegUpdated == FALSE) //&& g_pUsbDriver->GetCurAvailDevs () == 0)
            {
                NKDbgPrintfW(_T("Please stand by, installing usbtest.dll as the test driver..."));
                if(RegLoadUSBTestDll(TEXT("usbtest.dll"), FALSE) == FALSE){
                    NKDbgPrintfW(_T("can not install usb test driver,  make sure it does exist!"));
                    return TPR_NOT_HANDLED;
                }
                g_fRegUpdated = TRUE;
            }

            if (g_hEvtDeviceRemoved == NULL) {
                g_hEvtDeviceRemoved = CreateEvent(NULL,TRUE,FALSE,TEXT("UsbTestDeviceRemove"));
                if (g_hEvtDeviceRemoved == NULL) {
                    g_pKato->Log(LOG_COMMENT,TEXT("WARNING!!! USBTEST cannot create removal notification event, tests 3XXX, 9001 and 9991 will be skipped."));
                }
            }

            //we are waiting for the connection
            if(g_pUsbDriver->GetCurAvailDevs () == 0){
                HWND hDiagWnd = ShowDialogBox(TEXT("Connect USB Host device with USB Function device now!"));

                //CETK users: Please ignore following statement, this is only for automated test setup
                SetRegReady(1, 3);
                int iCnt = 0;
                while(g_pUsbDriver->GetCurAvailDevs () == 0){
                    if(++iCnt == 10){
                        DeleteDialogBox(hDiagWnd);
                        NKDbgPrintfW(_T("!!! NO device is attatched, test exit without running any test cases !!!\r\n"));
                        return TPR_NOT_HANDLED;
                    }
                    NKDbgPrintfW(_T("Connect USB Host device with USB Function device now!"));
                    Sleep(2000);
                }
                DeleteDialogBox(hDiagWnd);
            }
            Sleep(2000);
            break;

        case SPM_UNLOAD_DLL:
            for(int i = 0; i < MAX_USB_CLIENT_DRIVER; i++){
                if(g_pTstDevLpbkInfo[i] != NULL)
                    delete g_pTstDevLpbkInfo[i];
            }
            DeleteRegEntry();
            if(g_fUnloadDriver == TRUE)
                if(RegLoadUSBTestDll(TEXT("usbtest.dll"), TRUE))
                    g_fRegUpdated = FALSE;  //Must reload driver at next run.
            if (g_hEvtDeviceRemoved != NULL) {
                CloseHandle(g_hEvtDeviceRemoved);
                g_hEvtDeviceRemoved = NULL;
            }
            break;

        case SPM_SHELL_INFO:
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
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
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: USBTEST.DLL"));
            break;

        case SPM_END_GROUP:
            g_pKato->EndLevel(TEXT("END GROUP: USBTEST.DLL"));
            break;

        case SPM_BEGIN_TEST:
            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(
                                        pBT->lpFTE->dwUniqueID,
                                        TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                        pBT->lpFTE->lpDescription,
                                        pBT->dwThreadCount,
                                        pBT->dwRandomSeed);
            break;

        case SPM_END_TEST:
            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(
                                        TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                                        pET->lpFTE->lpDescription,
                                        pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                                        pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                                        pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                                        pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
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
    UCHAR  bConfig = 0 ;

    while (lpCommandLine && *lpCommandLine) {
        switch (*lpCommandLine) {
            case _T('d'):case _T('D'):
                dwNum = 0;
                lpCommandLine++;
                while (*lpCommandLine>=_T('0') && *lpCommandLine<= _T('9')) {
                    dwNum = dwNum*10 + *lpCommandLine - _T('0');
                    lpCommandLine++;
                }
                lpUsbParam->dwSelectDevice = dwNum;
                DEBUGMSG(DBG_INI, (_T("Select Device %d "),lpUsbParam->dwSelectDevice));
                break;
            case _T('c'):case _T('C'):
                lpCommandLine++;
                bConfig = (UCHAR)(*lpCommandLine) - _T('0');
                lpUsbParam->bSelectConfig = bConfig;
                break;
            default:
                lpCommandLine++;
                break;
        }
    }
    return TRUE;
}


//following are the packetsizes we will use in different cases
//default values
#define HIGH_SPEED_BULK_PACKET_SIZES    0x200
#define FULL_SPEED_BULK_PACKET_SIZES    0x40
#define HIGH_SPEED_ISOCH_PACKET_SIZES   0x40
#define FULL_SPEED_ISOCH_PACKET_SIZES   0x40
#define HIGH_SPEED_INT_PACKET_SIZES     0x200
#define FULL_SPEED_INT_PACKET_SIZES     0x40
//small packet sizes
#define BULK_PACKET_LOWSIZE             0x8
#define ISOCH_PACKET_LOWSIZE            0x10
#define INT_PACKET_LOWSIZE              0x10
//irregular packet sizes
#define BULK_PACKET_ALTSIZE             0x20
#define ISOCH_PACKET_ALTSIZE_HS         0x400-4 // 1023 byte limit
#define ISOCH_PACKET_ALTSIZE_FS         0x1F4   // 500; OHCI will fail at 0x400-4=1020.
#define INT_PACKET_ALTSIZE              0x40-4  // 63 bytes
//big packet sizes (high speed only, this is not used due the limitation of net2280
#define BULK_PACKET_HIGHSIZE            0x40
#define ISOCH_PACKET_HIGHSIZE           0xc00  //  1024*2
#define INT_PACKET_HIGHSIZE             0xc00  //  1024*2

//###############################################
//following APIs reconfig test device side
//###############################################


#define RECONFIG_TIMEOUT (DWORD)(30000)                                 // 30 seconds
#define RECONFIG_RESET   (DWORD)(5000)                                  // initial delay is 5 sec
#define RECONFIG_DELAY   (DWORD)(100)                                   // granularity 0.1 sec
#define RECONFIG_RETRIES (DWORD)((RECONFIG_TIMEOUT-RECONFIG_RESET)/RECONFIG_DELAY) // for a total of RECONFIG_TIMEOUT msec

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
            if (g_pUsbDriver->GetAt(iUsbDriverIndex) != NULL) {
                g_pKato->Log(LOG_COMMENT, TEXT("Old driver removal from array at %u not detected, test case should be 9001, or in full speed: 3001,3003!\r\n"),iUsbDriverIndex);
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
                g_pKato->Log(LOG_COMMENT, TEXT("New (re-configured) Driver 0x%08x at %u ready in %u msec\r\n"),
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


/// <summary>
/// Reads the endpoint descriptors for all configurations, and compares all
/// endpoint wMaxPacketSize's against expected values.
/// </summary>
/// <returns>
/// If the comparison succeeds, the return value is TRUE.  FALSE, otherwise.
/// </returns>
/// <exception>
/// None.
/// </exception>
/// <param name = UsbClientDrv* pDriver>Pointer to USB client driver</param>
/// <param name = DWORD in_dwIsocMaxPacketSize>Expected Isochronous max packet size</param>
/// <param name = DWORD in_dwBulkMaxPacketSize>Expected Bulk max packet size</param>
/// <param name = DWORD in_dwIntrMaxPacketSize>Expected Interrupt max packet size</param>
BOOL CompareMaxPacketSizes(UsbClientDrv* pDriver, 
                           DWORD in_dwIsocMaxPacketSize, 
                           DWORD in_dwBulkMaxPacketSize,
                           DWORD in_dwIntrMaxPacketSize)
{
    const DWORD NUM_EPs = USB_MAX_ENDPOINTS * 2; // IN + OUT, don't count EP0.  Theoretical max.
    const DWORD NUM_IFs = 1;                     // All of our loop back configs have a single USB IF per configuration.
                                                 // We make the assumption here that this will not change.
    
    // Per configuration, the size we need for this transfer is potentially as big as:
    //    NUM_EPs*sizeof(EP_desc) + NUM_IFs*sizeof(IF_desc) + sizeof(config_desc)
    const DWORD MEM_BLOCK_SIZE = NUM_EPs * sizeof(USB_ENDPOINT_DESCRIPTOR)
                                 + NUM_IFs * sizeof(USB_INTERFACE_DESCRIPTOR) 
                                 + sizeof(USB_CONFIGURATION_DESCRIPTOR);

    BOOL bRet = TRUE;  // Assume success until something goes wrong.

    USB_DEVICE_DESCRIPTOR          dd  = {0};
    PUSB_INTERFACE_DESCRIPTOR      pID = NULL;
    PUSB_CONFIGURATION_DESCRIPTOR  pCD = NULL;
    PUSB_ENDPOINT_DESCRIPTOR       pED = NULL;

    // Allocate descriptor memory.
    PBYTE pMem = (PBYTE)malloc(MEM_BLOCK_SIZE);
    if (NULL == pMem)
    {
        g_pKato->Log(LOG_DETAIL, L"CompareMaxPacketSizes: Could not allocate enough descriptor memory.");
        return FALSE;
    }

    // Get a pointer to the function table
    LPCUSB_FUNCS lpUsbFuncs = pDriver->GetUsbFuncsTable();


    // 1. Get the device descriptor to see how many configurations there are.
    USB_TRANSFER hTransfer = pDriver->GetDescriptor(NULL, NULL, 0, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, sizeof(dd), &dd);
    if (!hTransfer || !(lpUsbFuncs->lpIsTransferComplete(hTransfer)))
    {
        g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Getting device descriptor failed");
        bRet = FALSE;
    }

    if (!lpUsbFuncs->lpCloseTransfer(hTransfer))
    {
        g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Failed to close transfer.");
        bRet = FALSE;
        goto Exit;
    }

    // Sanity check the result.
    if (0 == dd.bNumConfigurations)
    {
        g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Read of device descriptor returned unexpected data (bNumConfigurations==0).");
        bRet = FALSE;
        goto Exit;
    }
        
    // 2. Get the descriptor tree for each configuration.
    if (bRet)
    {
        for (UCHAR i = 0; i < dd.bNumConfigurations; i++)
        {
            ZeroMemory(pMem, MEM_BLOCK_SIZE);

            hTransfer = pDriver->GetDescriptor(NULL, NULL, 0, USB_CONFIGURATION_DESCRIPTOR_TYPE, i, 0, MEM_BLOCK_SIZE, pMem);
            if (!hTransfer || !(lpUsbFuncs->lpIsTransferComplete(hTransfer)))
            {
                g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Getting configuration device descriptor failed");
                bRet = FALSE;
            }

            if (!lpUsbFuncs->lpCloseTransfer(hTransfer))
            {
                g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Failed to close transfer.");
                bRet = FALSE;
                goto Exit;
            }

            pCD = (PUSB_CONFIGURATION_DESCRIPTOR)&pMem[0];
            pID = (PUSB_INTERFACE_DESCRIPTOR)&pMem[sizeof(USB_CONFIGURATION_DESCRIPTOR)];
            pED = (PUSB_ENDPOINT_DESCRIPTOR)&pMem[sizeof(USB_CONFIGURATION_DESCRIPTOR)+sizeof(USB_INTERFACE_DESCRIPTOR)];

            // Perform a sanity check.
            if (pCD->bNumInterfaces != NUM_IFs)
            {
                g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Unexpected number of IFs: %d.  Expecting: %d.  Test code change required to support more IFs.", pCD->bNumInterfaces, NUM_IFs);
                bRet = FALSE;
                goto Exit;
            }
            else if (pID->bNumEndpoints > NUM_EPs)
            {
                g_pKato->Log(LOG_FAIL, L"CompareMaxPacketSizes: Unexpected number of EPs: %d.  Expecting up to %d.  Test code change required to support more EPs.", pID->bNumEndpoints, NUM_EPs);
                bRet = FALSE;
                goto Exit;
            }

            // Go through all endpoints.
            DWORD dwExpectedPacketSize = 0;
            for (UCHAR j = 0; j < pID->bNumEndpoints; j++)
            {
                switch(pED[j].bmAttributes & USB_ENDPOINT_TYPE_MASK)
                {
                case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                    dwExpectedPacketSize = in_dwIsocMaxPacketSize;
                    break;

                case USB_ENDPOINT_TYPE_BULK:
                    dwExpectedPacketSize = in_dwBulkMaxPacketSize;
                    break;

                case USB_ENDPOINT_TYPE_INTERRUPT:
                    dwExpectedPacketSize = in_dwIntrMaxPacketSize;
                    break;

                default:
                     g_pKato->Log(LOG_FAIL, L"FAIL: CompareMaxPacketSizes: Unexpected endpoint type");
                     bRet = FALSE;
                     goto Exit;
                } // switch

                // Perform the comparison
                if (dwExpectedPacketSize != pED[j].wMaxPacketSize)
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: EP MaxPacketSize comparison failure.  EP[%d], %s, TYPE=%s. Expected wMaxPacketSize=%d, actual=%d", 
                                     pED[j].bEndpointAddress & 0xF, 
                                     USB_ENDPOINT_DIRECTION_IN(pED[j].bEndpointAddress) ? L"IN" : L"OUT",
                                     g_szEPTypeStrings[pED[j].bmAttributes & USB_ENDPOINT_TYPE_MASK],
                                     dwExpectedPacketSize,
                                     pED[j].wMaxPacketSize);
                    bRet = FALSE;
                    goto Exit;
                }
            } // for (j)
        } // for (i)
    } // if (bRet)

Exit:
    free(pMem);
    pMem = NULL;

    g_pKato->Log(LOG_DETAIL, L"CompareMaxPacketSizes: Verification of wMaxPacketSizes %s.", (bRet) ? L"SUCCEEDED" : L"FAILED");
    return bRet;
}


/// <summary>
/// If necessary (full-speed) cause a ResetDevice(), and then wait for reflector reset (full-speed and high-speed).
/// </summary>
/// <returns>
/// If the function succeeds, the return value is TPR_PASS
/// If the function fails, the return value is TPR_FAIL, TPR_SKIP
/// </returns>
/// <param name = UsbClientDrv** ppDriverPtr>Driver pointer (pointer)</param>
/// <param name = BOOL bHighSpeed>Are we dealing with highspeed (or fullspeed)?</param>
/// <param name = int iUsbDriverIndex>Driver index</param>
DWORD ConditionalResetDeviceAndWaitForReflectorReset(UsbClientDrv** ppDriverPtr, BOOL bHighSpeed, int iUsbDriverIndex)
{
    // In the case of full-speed, there's no notification received that the 
    // reflector has actually been reset as part of SendVendorTransfer().
    // So we wait for the reflector to complete its reset, and then cause a
    // reset of the host side port.
    if (!bHighSpeed)
    {
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
    (*ppDriverPtr)->Unlock();
    if(!WaitForReflectorReset(*ppDriverPtr, iUsbDriverIndex))
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


    return TPR_PASS;
}


DWORD
SetLowestPacketSize(UsbClientDrv** ppDriverPtr, BOOL bHighSpeed, int iUsbDriverIndex)
{
    if(*ppDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have lowest packet sizes on each endpoint\r\n")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = CONFIG_DEFAULT;
    utrc.ucSpeed = (bHighSpeed == TRUE)?SPEED_HIGH_SPEED:SPEED_FULL_SPEED;
    utrc.wBulkPkSize = BULK_PACKET_LOWSIZE;
    utrc.wIntrPkSize = INT_PACKET_LOWSIZE;
    utrc.wIsochPkSize = ISOCH_PACKET_LOWSIZE;

    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, &utdr, (LPVOID)&utrc, NULL) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("Set Lowest Packet Size failed\r\n"));
        return TPR_FAIL;
    }

    // Cause a reset (if full speed), and wait for reflector reset (full and high-speed)
    DWORD dwRet = ConditionalResetDeviceAndWaitForReflectorReset(ppDriverPtr, bHighSpeed, iUsbDriverIndex);
    if (TPR_FAIL == dwRet)
    {
        return TPR_FAIL;
    }
    else if (TPR_SKIP == dwRet)
    {
        return TPR_SKIP;
    }

    // Verify that the configuration settings changes were made.
    BOOL bRet = CompareMaxPacketSizes(*ppDriverPtr, utrc.wIsochPkSize, utrc.wBulkPkSize, utrc.wIntrPkSize);
    return (bRet) ? TPR_PASS : TPR_FAIL;
}

DWORD
SetAlternativePacketSize(UsbClientDrv** ppDriverPtr, BOOL bHighSpeed, int iUsbDriverIndex)
{
    if(*ppDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have alternative packet sizes on each endpoint\r\n")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = CONFIG_ALTERNATIVE;
    utrc.ucSpeed = (bHighSpeed == TRUE) ? SPEED_HIGH_SPEED : SPEED_FULL_SPEED;
    utrc.wBulkPkSize = BULK_PACKET_ALTSIZE;
    utrc.wIntrPkSize = INT_PACKET_ALTSIZE;
    utrc.wIsochPkSize = (bHighSpeed == TRUE) ? ISOCH_PACKET_ALTSIZE_HS : ISOCH_PACKET_ALTSIZE_FS;

    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, &utdr, (LPVOID)&utrc, NULL) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("Set Alternative Packet Size failed\r\n"));
        return TPR_FAIL;
    }
        
    // Cause a reset (if full speed), and wait for reflector reset (full and high-speed)
    DWORD dwRet = ConditionalResetDeviceAndWaitForReflectorReset(ppDriverPtr, bHighSpeed, iUsbDriverIndex);
    if (TPR_FAIL == dwRet)
    {
        return TPR_FAIL;
    }
    else if (TPR_SKIP == dwRet)
    {
        return TPR_SKIP;
    }

    // Verify that the configuration settings changes were made.
    BOOL bRet = CompareMaxPacketSizes(*ppDriverPtr, utrc.wIsochPkSize, utrc.wBulkPkSize, utrc.wIntrPkSize);
    return (bRet) ? TPR_PASS : TPR_FAIL;
}

DWORD
SetHighestPacketSize(UsbClientDrv** ppDriverPtr, int iUsbDriverIndex)
{
    if(*ppDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have highest packet sizes on each endpoint\r\n")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = CONFIG_ALTERNATIVE;
    utrc.ucSpeed = SPEED_HIGH_SPEED;
    utrc.wBulkPkSize = BULK_PACKET_HIGHSIZE;
    utrc.wIntrPkSize = INT_PACKET_HIGHSIZE;
    utrc.wIsochPkSize = ISOCH_PACKET_HIGHSIZE;

    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, &utdr, (LPVOID)&utrc, NULL) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("SetHighest Packet Size failed\r\n"));
        return TPR_FAIL;
    }
    (*ppDriverPtr)->Unlock();
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

    BOOL bRet = CompareMaxPacketSizes(*ppDriverPtr, utrc.wIsochPkSize, utrc.wBulkPkSize, utrc.wIntrPkSize);
    return (bRet) ? TPR_PASS : TPR_FAIL;
}

DWORD
RestoreDefaultPacketSize(UsbClientDrv** ppDriverPtr, BOOL bHighSpeed, int iUsbDriverIndex)
{
    if(*ppDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("restore default packet sizes on each endpoint\r\n")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = CONFIG_DEFAULT;
    utrc.ucSpeed = (bHighSpeed == TRUE)?SPEED_HIGH_SPEED:SPEED_FULL_SPEED;

    if(bHighSpeed == TRUE){
        utrc.ucSpeed = SPEED_HIGH_SPEED;
        utrc.wBulkPkSize = HIGH_SPEED_BULK_PACKET_SIZES;
        utrc.wIntrPkSize = HIGH_SPEED_INT_PACKET_SIZES;
        utrc.wIsochPkSize = HIGH_SPEED_ISOCH_PACKET_SIZES;
    }
    else{
        utrc.ucSpeed = SPEED_FULL_SPEED;
        utrc.wBulkPkSize = FULL_SPEED_BULK_PACKET_SIZES;
        utrc.wIntrPkSize = FULL_SPEED_INT_PACKET_SIZES;
        utrc.wIsochPkSize = FULL_SPEED_ISOCH_PACKET_SIZES;
    }

    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

    //issue command to netchip2280
    if(SendVendorTransfer(*ppDriverPtr, FALSE, &utdr, (LPVOID)&utrc, NULL) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("Restore default packet Size failed\r\n"));
        return TPR_FAIL;
    }

    // Cause a reset (if full speed), and wait for reflector reset (full and high-speed)
    DWORD dwRet = ConditionalResetDeviceAndWaitForReflectorReset(ppDriverPtr, bHighSpeed, iUsbDriverIndex);
    if (TPR_FAIL == dwRet)
    {
        return TPR_FAIL;
    }
    else if (TPR_SKIP == dwRet)
    {
        return TPR_SKIP;
    }
    
    // Verify that the configuration settings changes were made.
    BOOL bRet = CompareMaxPacketSizes(*ppDriverPtr, utrc.wIsochPkSize, utrc.wBulkPkSize, utrc.wIntrPkSize);
    return (bRet) ? TPR_PASS : TPR_FAIL;
}

//###############################################
//following API retrieve test device's config information
//###############################################

#define DEVICE_TO_HOST_MASK  0x80

PUSB_TDEVICE_LPBKINFO
GetTestDeviceLpbkInfo(UsbClientDrv * pDriverPtr)
{
    if(pDriverPtr == NULL)
        return FALSE;
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
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)&uNumofPairs, NULL) == FALSE){
        NKDbgPrintfW(TEXT("get number of loopback pairs on test device failed\r\n"));
        return NULL;
    }

    if(uNumofPairs == 0){
        NKDbgPrintfW(TEXT("No loopback pairs avaliable on this test device!!!\r\n"));
        return NULL;
    }

    USHORT uSize = sizeof(USB_TDEVICE_LPBKINFO) + sizeof(USB_TDEVICE_LPBKPAIR)*(uNumofPairs-1);
    pTDLpbkInfo = NULL;
    //allocate
    pTDLpbkInfo = (PUSB_TDEVICE_LPBKINFO) new BYTE[uSize];
    if(pTDLpbkInfo == NULL){

        NKDbgPrintfW(_T("Out of memory!"));
        return NULL;
    }
    else
        memset(pTDLpbkInfo, 0, uSize);

    //issue command to netchip2280 -- this time get pair info
    utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
    utdr.bRequest = TEST_REQUEST_LPBKINFO;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = uSize;
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)pTDLpbkInfo, NULL) == FALSE){
        NKDbgPrintfW(TEXT("get info of loopback pairs on test device failed\r\n"));
        delete[]  (PBYTE)pTDLpbkInfo;
        return NULL;
    }

    return pTDLpbkInfo;

}

//###############################################
//following APIs are support functions for test driver setup
//###############################################

static  const   TCHAR   gcszThisFile[]   = { TEXT("USBTEST, USBTEST.CPP") };

BOOL    RegLoadUSBTestDll( LPTSTR szDllName, BOOL bUnload )
{
    BOOL    fRet = FALSE;
    BOOL    fException;
    DWORD   dwExceptionCode = 0;

    /*
    *   Make sure that all the required entry points are present in the USBD driver
    */
    LoadDllGetAddress( TEXT("USBD.DLL"), USBDEntryPointsText, (LPDLL_ENTRY_POINTS) & USBDEntryPoints );
    UnloadDll( (LPDLL_ENTRY_POINTS) & USBDEntryPoints );

    /*
    *   Make sure that all the required entry points are present in the USB client driver
    */
    fRet = LoadDllGetAddress( szDllName, USBDriverEntryText, (LPDLL_ENTRY_POINTS) & USBDriverEntry );
    if ( ! fRet )
        return FALSE;


    if (bUnload){
        Log( TEXT("%s: UnInstalling library \"%s\".\r\n"), gcszThisFile, szDllName );
        __try{
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpUnInstallDriver)();
        }
        __except(EXCEPTION_EXECUTE_HANDLER){
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if ( fException ){
            Log( TEXT("%s: USB Driver UnInstallation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"),
                gcszThisFile,
                dwExceptionCode,
                GetLastError()
                );
        }
        if ( ! fRet )
            Fail( TEXT("%s: UnInstalling USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName );
        else
            Log ( TEXT("%s: UnInstalling USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName );
    }
    else{
        Log( TEXT("%s: Installing library \"%s\".\r\n"), gcszThisFile, szDllName );
        __try
        {
            fException = FALSE;
            fRet = FALSE;
            fRet = (*USBDriverEntry.lpInstallDriver)(szDllName);
        }
        __except(EXCEPTION_EXECUTE_HANDLER){
            fException = TRUE;
            dwExceptionCode = _exception_code();
        }
        if ( fException ){
            Fail( TEXT("%s: USB Driver Installation FAILED! Exception 0x%08X! GetLastError()=%u.\r\n"),
                gcszThisFile,
                dwExceptionCode,
                GetLastError()
                );
        }
        if ( ! fRet )
            Fail( TEXT("%s: Installing USB driver library \"%s\", NOT successfull.\r\n"), gcszThisFile, szDllName );
        else
            Log ( TEXT("%s: Installing USB driver library \"%s\", successfull.\r\n"), gcszThisFile, szDllName );
    }

    UnloadDll( (LPDLL_ENTRY_POINTS) & USBDriverEntry );

    return( TRUE );
}

HWND ShowDialogBox(LPCTSTR szPromptString){

    if(szPromptString == NULL)
        return NULL;

    //create a dialog box
    HWND hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_REMOVECARD), NULL, NULL);
    if(NULL == hDiagWnd){
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

VOID DeleteDialogBox(HWND hDiagWnd){

    //destroy the dialogbox
    if(NULL != hDiagWnd)
        DestroyWindow(hDiagWnd);
    hDiagWnd = NULL;
}


//###############################################
//following APIs are for auotmation use. CETK users can ignore them
//###############################################

#define REG_KEY_FOR_USBCONNECTION   TEXT("\\Drivers\\USBBoardConnection")
#define REG_BOARDNO_VALNAME  TEXT("BoardNo")
#define REG_PORTNO_VALNAME  TEXT("PortNo")

VOID
SetRegReady(BYTE BoardNo, BYTE PortNo){

    DWORD dwBoardNo = 1;
    DWORD dwPortNo = 0;
    if(BoardNo >= 1 && BoardNo <= 5)
        dwBoardNo = BoardNo;
    if(PortNo <= 3)
        dwPortNo = PortNo;

    HKEY hKey = NULL;
    DWORD dwTemp;
    DWORD dwErr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION,
                                                    0, NULL, 0, 0, NULL, &hKey, &dwTemp);
    if(dwErr == ERROR_SUCCESS){
        DWORD dwLen = sizeof(DWORD);
        dwErr = RegSetValueEx(hKey, REG_BOARDNO_VALNAME, 0, REG_DWORD, (PBYTE)&dwBoardNo, dwLen);
        dwErr = RegSetValueEx(hKey, REG_PORTNO_VALNAME, NULL, REG_DWORD, (PBYTE)&dwPortNo, dwLen);
        NKDbgPrintfW(_T("SetRegReady!BoardNO = %d, PortNo = %d"), dwBoardNo, dwPortNo);
        RegCloseKey(hKey);
    }

}

VOID
DeleteRegEntry(){

    HKEY hKey = NULL;
    DWORD dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION, 0, 0, &hKey);

    if(dwErr == ERROR_SUCCESS){
        RegCloseKey(hKey);
        RegDeleteKey(HKEY_LOCAL_MACHINE, REG_KEY_FOR_USBCONNECTION);
    }

}


//###############################################
//Vendor transfer API
//###############################################
BOOL
SendVendorTransfer(UsbClientDrv *pDriverPtr, BOOL bIn, PUSB_TDEVICE_REQUEST pUtdr, LPVOID pBuffer, PDWORD pcbRetBytes){
    if(pDriverPtr == NULL || pUtdr == NULL)
        return FALSE;

    DWORD dwFlags = (bIn == TRUE)?USB_IN_TRANSFER:USB_OUT_TRANSFER;
    if(pcbRetBytes != NULL){//we can expect short transfer here
        dwFlags |= USB_SHORT_TRANSFER_OK;
    }
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL,
                                                                         dwFlags,
                                                                         (PUSB_DEVICE_REQUEST)pUtdr,
                                                                         pBuffer, 0);
    if(hVTransfer == NULL){
        NKDbgPrintfW(TEXT("issueVendorTransfer failed\r\n"));
        return FALSE;
    }

    int iCnt = 0;
    //we will wait for about 3 minutes
    while(iCnt < 1000*9){
        if((pDriverPtr->lpUsbFuncs)->lpIsTransferComplete(hVTransfer) == TRUE){
            break;
        }
        Sleep(20);
        iCnt++;
    }

    if(iCnt >= 1000*9){//time out
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
        NKDbgPrintfW(TEXT("issueVendorTransfer time out!\r\n"));
        return FALSE;
    }

    DWORD dwError = USB_NO_ERROR;
    DWORD cbdwBytes = 0;
    BOOL bRet = (pDriverPtr->lpUsbFuncs)->lpGetTransferStatus(hVTransfer, &cbdwBytes, &dwError);
    if(bRet == FALSE || dwError != USB_NO_ERROR ||((pcbRetBytes == NULL) && (cbdwBytes != pUtdr->wLength))){
        NKDbgPrintfW(TEXT("IssueVendorTransfer has encountered some error!\r\n"));
        NKDbgPrintfW(TEXT("dwError = 0x%x,  cbdwBytes = %d\r\n"), dwError, cbdwBytes);
        bRet = FALSE;
    }
    else if(pcbRetBytes != NULL){//if this para is not null, we just return the actual r/w bytes.
        *pcbRetBytes = cbdwBytes;
    }

    (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);

    return bRet;
}


//###############################################
//Some additional test cases
//###############################################

DWORD
UnloadTestDriver()
{
    if (g_fUnloadDriver == FALSE)
    {
        g_fUnloadDriver = TRUE;
        NKDbgPrintfW(_T("UnloadTestDriver: Try to unload the USB test dll \r\n"));
        if(RegLoadUSBTestDll(TEXT("usbtest.dll"), TRUE))
        {
            g_fUnloadDriver = FALSE;
            g_fRegUpdated = FALSE;  //Must reload driver at next run.
            NKDbgPrintfW(_T("UnloadTestDriver: Unload the USB test dll successfully \r\n"));
            return TPR_PASS;
        }
        else {
            NKDbgPrintfW(_T("UnloadTestDriver: Unload the USB test dll failed \r\n"));
            return TPR_FAIL;
        }
    }
    else {
        NKDbgPrintfW(_T("UnloadTestDriver: the USB test dll has already been unloaded, test skipped \r\n"));
        return TPR_SKIP;
    }
}

DWORD
LoadTestDriver()
{
    if (g_fRegUpdated == FALSE)
    {
        g_pKato->Log(LOG_COMMENT, _T("LoadTestDriver: Try to load the USB test dll \r\n"));
        if(RegLoadUSBTestDll(TEXT("usbtest.dll"), FALSE))
        {
            g_fRegUpdated = TRUE;  //Do not need to reload driver at next run.
            g_pKato->Log(LOG_PASS, _T("LoadTestDriver: Load the USB test dll successfully \r\n"));
            return TPR_PASS;
        }
        else {
            g_pKato->Log(LOG_FAIL, _T("LoadTestDriver: Load the USB test dll failed \r\n"));
            return TPR_FAIL;
        }
    }
    else {
        g_pKato->Log(LOG_SKIP, _T("LoadTestDriver: the USB test dll is already loaded, test skipped \r\n"));
        return TPR_SKIP;
    }
}

DWORD
ResetDevice(UsbClientDrv *pDriverPtr, BOOL bReEnum)
{
    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(bReEnum == TRUE)
        g_pKato->Log(LOG_COMMENT, _T("Now Resetting the device and will re-enumerate it\r\n"));
    else
        g_pKato->Log(LOG_COMMENT, _T("Now Resetting the device without re-enumeration\r\n"));

    if(pDriverPtr->DisableDevice(bReEnum, 0) == FALSE){
        g_pKato->Log(LOG_FAIL, _T("Failed to disable the device and %s.\r\n"),bReEnum?_T("re-enumerate it"):_T("leave it disabled"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}


DWORD
SuspendDevice(UsbClientDrv *pDriverPtr)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(pDriverPtr->SuspendDevice(0) == FALSE){
        NKDbgPrintfW(_T("Failed on suspend call!"));
        return TPR_FAIL;
    }

    Sleep(5000);

   if(g_dwNotifyMsg & USB_SUSPENDED_DEVICE)
   {
       g_dwNotifyMsg = 0 ;
        return TPR_PASS;
   }
    else
    {
        g_dwNotifyMsg = 0 ;
        return TPR_FAIL;
     }

}

DWORD
ResumeDevice(UsbClientDrv *pDriverPtr)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(pDriverPtr->ResumeDevice(0) == FALSE){
        NKDbgPrintfW(_T("Failed on resume call!"));
        return TPR_FAIL;
    }

    Sleep(5000);

   if(g_dwNotifyMsg & USB_RESUMED_DEVICE)
   {
       g_dwNotifyMsg = 0 ;
        return TPR_PASS;
   }
    else
    {
        g_dwNotifyMsg = 0 ;
        return TPR_FAIL;
     }
}

#define EP0TEST_MAX_DATASIZE    512
DWORD
EP0Test(UsbClientDrv *pDriverPtr, BOOL fStall){

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    USB_TDEVICE_REQUEST utdr= {0};
    BYTE pBuf[EP0TEST_MAX_DATASIZE ]= {0};
    BYTE bStartVal = 0;
    BYTE bOdd = 0;
    USHORT usLen = 0;
    BOOL fRet = FALSE;
    BYTE bRetry = 0;
    const BYTE bRetryTimes = (fStall == TRUE)?6:1;
    UINT uiRand = 0;

    for(int i = 0; i < 1024; i ++){
        rand_s(&uiRand);
        bStartVal = uiRand % 256;
        rand_s(&uiRand);
        bOdd = uiRand % 2;
        rand_s(&uiRand);
        usLen = uiRand % 256;
        if(usLen == 0)
            usLen = 1;
        NKDbgPrintfW(_T("EP0Test: No. %d run, data size = %d, %s transfer \r\n"),
                                    i, usLen, (bOdd == 0)? _T("IN"):_T("OUT"));

        if(bOdd == 0){//in transfer
            utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
            utdr.bRequest = (fStall == TRUE)?TEST_REQUEST_EP0TESTINST:TEST_REQUEST_EP0TESTIN;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            bRetry = 0;
            while(SendVendorTransfer(pDriverPtr, TRUE, &utdr, pBuf, NULL) == FALSE){
                bRetry ++;
                if(bRetry < bRetryTimes){
                    g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed in No. %d try!"), bRetry);
                    continue;
                }
                else{
                    g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed!"));
                    goto EXIT;
                }
            }
            for(int j = 0; j < usLen; j++){
                if(pBuf[j] != bStartVal){
                    g_pKato->Log(LOG_FAIL, TEXT("EP0 IN: at offset %d, expected value = %d, real value = %d!"),
                                                                    j, bStartVal, pBuf[j]);
                    goto EXIT;
                }
                bStartVal ++;
            }
        }
        else{//out transfer
            utdr.bmRequestType = USB_REQUEST_CLASS;
            utdr.bRequest = (fStall == TRUE)?TEST_REQUEST_EP0TESTOUTST:TEST_REQUEST_EP0TESTOUT;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            for(int j = 0; j < usLen; j++){
                pBuf[j] = bStartVal ++;
            }
            bRetry = 0;
            while(SendVendorTransfer(pDriverPtr, FALSE, &utdr, pBuf, NULL) == FALSE){
                bRetry ++;
                if(bRetry < bRetryTimes){
                    g_pKato->Log(LOG_FAIL, TEXT("Send OUT transfer command failed in No. %d try!"), bRetry);
                    continue;
                }
                else{
                    g_pKato->Log(LOG_FAIL, TEXT("Send OUT transfer command failed!"));
                    goto EXIT;
                }
            }
        }
        memset(pBuf, 0, sizeof(pBuf));
    }

    fRet = TRUE;

EXIT:

    g_pKato->Log(LOG_DETAIL, TEXT("!!!PLEASE CHECK FUNCTION SIDE OUTPUT TO SEE WHETHER THERE ARE SOME OUT TRANSFER ERRORS!!!"));

    return (fRet == TRUE)?TPR_PASS:TPR_FAIL;
}

DWORD
EP0TestWithZeroLenIn(UsbClientDrv *pDriverPtr){

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    USB_TDEVICE_REQUEST utdr= {0};
    PBYTE pBuf = NULL;
    BOOL fRet = FALSE;
    const UCHAR bRetryTimes = 4;
    DWORD dwMaxPacketSize = pDriverPtr->GetEP0PacketSize();

    pBuf = (PBYTE) new BYTE[dwMaxPacketSize*3];
    if(pBuf == NULL){
        NKDbgPrintfW(_T("OOM!"));
    }

    for(int i = 0; i < 16; i ++){

        NKDbgPrintfW(_T("EP0Test: No. %d run: data size = %d,  IN transfer \r\n"),
                                    i, dwMaxPacketSize*3);
        utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
        utdr.bRequest = TEST_REQUEST_EP0ZEROINLEN;
        utdr.wLength = (USHORT)(dwMaxPacketSize*3);
        UCHAR bRetry = 0;
        DWORD cbReturnBytes = 0;
        while(SendVendorTransfer(pDriverPtr, TRUE, &utdr, pBuf, &cbReturnBytes) == FALSE){
            bRetry ++;
            if(bRetry < bRetryTimes){
                g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed in No. %d try!"), bRetry);
                continue;
            }
            else{
                g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed!"));
                goto EXIT;
            }
        }

        if(cbReturnBytes != dwMaxPacketSize){
            NKDbgPrintfW(_T("Expected return bytes is: %d, actual return is: %d"), dwMaxPacketSize, cbReturnBytes);
            goto EXIT;
        }
        else{
            NKDbgPrintfW(_T("Succcessfully read %d bytes, which is a short transfer with exactly one packet size"), dwMaxPacketSize);
        }
    }

    fRet = TRUE;

EXIT:

    if(pBuf){
        delete[] pBuf;
    }
    g_pKato->Log(LOG_DETAIL, TEXT("!!!PLEASE CHECK FUNCTION SIDE OUTPUT TO SEE WHETHER THERE ARE SOME OUT TRANSFER ERRORS!!!"));

    return (fRet == TRUE)?TPR_PASS:TPR_FAIL;
}