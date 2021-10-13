//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name: 
	usbtest.cpp

Abstract:  
    USB Client Driver for testing USBDriver.


    davli

Modified:
    weichen

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

//globals
PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[4];

HINSTANCE g_hInst = NULL;

BOOL g_fUnloadDriver = FALSE; //unload usbtest.dll from usbd.dll if true
BOOL g_fRegUpdated = FALSE; //registry update only needs to be done once

USBDriverClass *pUsbDriver=NULL; //USB Bus APIs

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

// ----------------------------------------------------------------------------
//
// Debugger
//
// ----------------------------------------------------------------------------
#ifdef DEBUG

//
// These defines must match the ZONE_* defines
//
#define INIT_ERR        1
#define INIT_WARN       2
#define INIT_FUNC       4
#define INIT_DETAIL     8
#define INIT_INI        16
#define INIT_USB        32
#define INIT_IO         64
#define INIT_WEIRD      128
#define INIT_EVENTS             256
#define INIT_USB_CONTROL 0x200
#define INIT_USB_BULK 0x400
#define INIT_USB_INTERRUPT 0x800
#define INIT_USB_USB_ISOCH 0x1000
#define INIT_THREAD 0x2000

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



extern "C" BOOL DllEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) 
{
    switch (dwReason)    {
      case DLL_PROCESS_ATTACH:
		DEBUGREGISTER((HINSTANCE)hDllHandle);
		pUsbDriver= new USBDriverClass();
             g_hInst = (HINSTANCE)hDllHandle;
		break;
      case DLL_PROCESS_DETACH:
		delete pUsbDriver;
		pUsbDriver=NULL;
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

        DriverSettings.dwVendorId          = 0x17cc;
        DriverSettings.dwProductId         = 0x2280;
        DriverSettings.dwReleaseNumber = USB_NO_INFO;

        DriverSettings.dwDeviceClass = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol = USB_NO_INFO;

        DriverSettings.dwInterfaceClass = 0; 
        DriverSettings.dwInterfaceSubClass = 0x00; // boot device
        DriverSettings.dwInterfaceProtocol = 0x00; // mouse

        fRet=usbDriver.RegisterClientDriverID(TEXT("NetChip2280 UFN Loopback Driver"));
        ASSERT(fRet);
        if(fRet){
            fRet = usbDriver.RegisterClientSettings(szDriverLibFile,
                                TEXT("NetChip2280 UFN Loopback Driver"), NULL, &DriverSettings);
            ASSERT(fRet);
        }

        //set up setttings for Xscale USB function data loopback driver
        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = 0x0301;
        fRet=usbDriver.RegisterClientDriverID(TEXT("Xscale UFN Loopback Driver"));
        ASSERT(fRet);
        if(fRet){
            fRet = usbDriver.RegisterClientSettings(szDriverLibFile,
                                        TEXT("Xscale UFN Loopback Driver"), NULL, &DriverSettings);
            ASSERT(fRet);
        }

        //set up setttings for SC2410 USB function data loopback driver
        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = 0x00ce;
        fRet=usbDriver.RegisterClientDriverID(TEXT("SC2410 UFN Loopback Driver"));
        ASSERT(fRet);
        if(fRet){
            fRet = usbDriver.RegisterClientSettings(szDriverLibFile,
                                        TEXT("SC2410 UFN Loopback Driver"), NULL, &DriverSettings);
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
        DriverSettings.dwVendorId          = 0x17cc;
        DriverSettings.dwProductId         = 0x2280;
        DriverSettings.dwReleaseNumber = USB_NO_INFO;

        DriverSettings.dwDeviceClass = USB_NO_INFO;
        DriverSettings.dwDeviceSubClass = USB_NO_INFO;
        DriverSettings.dwDeviceProtocol = USB_NO_INFO;

        DriverSettings.dwInterfaceClass = 0x03; 
        DriverSettings.dwInterfaceSubClass = 0x00; // boot device
        DriverSettings.dwInterfaceProtocol = 0x00; // mouse

        fRet =usbDriver.UnRegisterClientSettings(TEXT("NetChip2280 UFN Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("NetChip2280 UFN Loopback Driver"));

        //Xscale Cotulla
        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = 0x0301;
        fRet = fRet && usbDriver.UnRegisterClientSettings(TEXT("Xscale UFN Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("Xscale UFN Loopback Driver"));

        //SC2410 
        DriverSettings.dwVendorId          = 0x045e;
        DriverSettings.dwProductId         = 0x00ce;
        fRet = fRet && usbDriver.UnRegisterClientSettings(TEXT("SC2410 UFN Loopback Driver"), NULL, &DriverSettings);
        fRet = fRet && usbDriver.UnRegisterClientDriverID(TEXT("SC2410 UFN Loopback Driver"));
    }

    return fRet;
}


BOOL WINAPI DeviceNotify(LPVOID lpvNotifyParameter,DWORD dwCode,LPDWORD * ,LPDWORD * ,LPDWORD * ,LPDWORD * )
{
    switch(dwCode){
        case USB_CLOSE_DEVICE:
            // All processing done in destructor
            if (pUsbDriver) {
                UsbClientDrv * pClientDrv=(UsbClientDrv *)lpvNotifyParameter;
                BOOL fReturn=pUsbDriver->RemoveClientDrv(pClientDrv,FALSE);
                ASSERT(fReturn);

		    (*pClientDrv->lpUsbFuncs->lpUnRegisterNotificationRoutine)(
					pClientDrv->GetUsbHandle(),
					DeviceNotify, 
					pClientDrv);
                pClientDrv->Lock();
                delete pClientDrv;
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
    DEBUGMSG(DBG_USB, (TEXT("Device Attach")));
    *fAcceptControl = FALSE;

    if(pUsbDriver == NULL || lpInterface == NULL)
        return FALSE;

    DEBUGMSG(DBG_USB, (TEXT("USBTest: DeviceAttach, IF %u, #EP:%u, Class:%u, Sub:%u, Prot:%u\r\n"),
			lpInterface->Descriptor.bInterfaceNumber,lpInterface->Descriptor.bNumEndpoints,
			lpInterface->Descriptor.bInterfaceClass,lpInterface->Descriptor.bInterfaceSubClass,
			lpInterface->Descriptor.bInterfaceProtocol));

	UsbClientDrv *pLocalDriverPtr=new UsbClientDrv(hDevice,lpUsbFuncs,lpInterface,szUniqueDriverId,lpDriverSettings);
	ASSERT(pLocalDriverPtr);
	if(pLocalDriverPtr == NULL)
		return FALSE;

    if (!pLocalDriverPtr->Initialization() || !(pUsbDriver->AddClientDrv(pLocalDriverPtr)))   {
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

    if (pUsbDriver==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
        return TPR_SKIP;
    };

    DWORD dwRet = TPR_PASS;
    DWORD dwDriverNumber=0;
    USB_PARAM m_UsbParam;


    ParseCommandLine(&m_UsbParam,DdlxGetCmdLine( tpParam ) );
    DWORD dwActiveDevice=m_UsbParam.dwSelectDevice!=0?m_UsbParam.dwSelectDevice:1;
    dwActiveDevice--;

    //make phsyical mem ready
    PPhysMemNode PMemN = new PhysMemNode[MY_PHYS_MEM_SIZE];
    
    for (int i=0;i<(int)(pUsbDriver->GetArraySize());i++) {
        UsbClientDrv *pClientDrv=pUsbDriver->GetAt(i);
        if (pClientDrv && (dwActiveDevice==0)) {
            pClientDrv->Lock();
            if(g_pTstDevLpbkInfo[i] == NULL){
                if(lpFTE->dwUniqueID < SPECIAL_CASE){
                    g_pTstDevLpbkInfo[i] = GetTestDeviceLpbkInfo(pClientDrv);
                    if(g_pTstDevLpbkInfo[i] == NULL || g_pTstDevLpbkInfo[i]->uNumOfPipePairs == 0){//failed or no data pipes
                        NKDbgPrintfW(_T("Can not get test device's loopback pairs' information or there's no data loopback pipe pair, test will be skipped for this device!!!"));
                        continue;
                    }
                }
            }
            DEBUGMSG(DBG_INI, (TEXT("Device retrive from %d"),i));
            dwDriverNumber++;
            BOOL fAsync = FALSE;

            if((lpFTE->dwUniqueID % 100) > 10)
                fAsync = TRUE;
            DWORD dwTransType = (lpFTE->dwUniqueID % 1000 )/100;
            
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
                    dwRet = SetLowestPacketSize(pClientDrv, FALSE);
                    break;
                case CONFIG_CASE+2: //set lowest packet size for high speed config
                    dwRet = SetLowestPacketSize(pClientDrv, TRUE);
                    break;
                case CONFIG_CASE+3: //set alternative packet size for full speed config
                    dwRet = SetAlternativePacketSize(pClientDrv, FALSE);
                    break;
                case CONFIG_CASE+4: //set alternative packet size for high speed config
                    dwRet = SetAlternativePacketSize(pClientDrv, TRUE);
                    break;
                case CONFIG_CASE+5: //set highest packet size for high speed config
                    dwRet = SetHighestPacketSize(pClientDrv);
                    break;
                case CONFIG_CASE+11: //restore default packet size for full speed config
                    dwRet = RestoreDefaultPacketSize(pClientDrv, FALSE);
                    break;
                case CONFIG_CASE+12: //restore default packet size for highl speed config
                    dwRet = RestoreDefaultPacketSize(pClientDrv, TRUE);
                    break;

               //special test cases     
		  case 9001: //reset device test
		  	dwRet = ResetDevice(pClientDrv, TRUE);
			break;

		  case 9002: //reset device test
		  	dwRet = ResetDevice(pClientDrv, FALSE);
			break;
        
		  case 9003: //reset device test
		  	dwRet = SuspendDevice(pClientDrv);
			break;
		  case 9004: //reset device test
		  	dwRet = ResumeDevice(pClientDrv);
			break;

		  case 9005: //EP0 test
		  	dwRet = EP0Test(pClientDrv);
			break;

               default:
                    pClientDrv->Unlock();
                    delete [] PMemN;
                    return TPR_NOT_HANDLED;
		}
		pClientDrv->Unlock();
		break;			
        }
        else if (pClientDrv) 
                dwActiveDevice--;
    }
    
    delete [] PMemN;
    if (dwDriverNumber==0){
        g_pKato->Log(LOG_FAIL, TEXT("Fail at Device Driver not loaded"));
        return TPR_SKIP;
    }

    return dwRet;
}



////////////////////////////////////////////////////////////////////////////////
// TUX Function table
extern "C"{

FUNCTION_TABLE_ENTRY g_lpFTE[] = { 
    {TEXT("USB Normal Driver Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+4,  LoopTestProc } ,
    { TEXT("All type Loopbacks Test "), 1,  0,  BASE+5,  LoopTestProc } ,

    {TEXT("USB Normal Driver Test(ASync)"),  0, 0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+4,  LoopTestProc } ,
    { TEXT("All type Loopbacks Test "), 1,  0,  BASE+ASYNC_TEST+5,  LoopTestProc } ,

    {TEXT("USB Normal Physical Mem Driver Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+MEM_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+MEM_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+MEM_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+MEM_TEST+4,  LoopTestProc } ,

    {TEXT("USB Normal Physical Mem Driver Test(ASync)"),  0, 0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+MEM_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+MEM_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+MEM_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+MEM_TEST+4,  LoopTestProc } ,

    {TEXT("USB Align Physical Mem Driver Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+ALIGN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+ALIGN_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+ALIGN_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+ALIGN_TEST+4,  LoopTestProc } ,

    {TEXT("USB Align Physical Mem Driver Test(ASync)"),  0, 0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+ALIGN_TEST+4,  LoopTestProc } ,

    {TEXT("USB short transfer Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+SHORT_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  0,  BASE+SHORT_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  0,  BASE+SHORT_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  0,  BASE+SHORT_TEST+4,  LoopTestProc } ,

    {TEXT("USB short transfer Test(ASync)"),  0, 0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  1,  BASE+ASYNC_TEST+SHORT_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test "),     1,  1,  BASE+ASYNC_TEST+SHORT_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test "),   1,  1,  BASE+ASYNC_TEST+SHORT_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopbacks Test "), 1,  1,  BASE+ASYNC_TEST+SHORT_TEST+4,  LoopTestProc } ,

    {TEXT("Additional transfer Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Normal transfer Test"),           1,  0,  BASE+ADD_TEST+1,  LoopTestProc } ,
    { TEXT("Close transfer Test "),   1,  0,  BASE+ADD_TEST+3,  LoopTestProc } ,
    { TEXT("Special test-large transfer "),   1,  0,  BASE+ADD_TEST+5,  LoopTestProc } ,
    { TEXT("Special test-with delays"),   1,  0,  BASE+ADD_TEST+6,  LoopTestProc } ,
    { TEXT("Special test-close in reverse order "),   1,  0,  BASE+ADD_TEST+7,  LoopTestProc } ,
    
    {TEXT("Additional transfer Test(Async)"),  0, 	0,  0, NULL},
    { TEXT("Normal transfer Test"),           1,  0,  BASE+ADD_TEST+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Abort transfer Test "),     1,  0,  BASE+ADD_TEST+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Close transfer Test "),   1,  0,  BASE+ADD_TEST+ASYNC_TEST+3,  LoopTestProc } ,

    {TEXT("USB Short Stress Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Bulk Loopback Test"),           1,  0,  BASE+SHORTSTRESS_TEST+1,  LoopTestProc } ,
    { TEXT("Interrupt Loopback Test"),           1,  0,  BASE+SHORTSTRESS_TEST+2,  LoopTestProc } ,
    { TEXT("Isochronous Loopback Test"),           1,  0,  BASE+SHORTSTRESS_TEST+3,  LoopTestProc } ,
    { TEXT("Simultaneous Loopback Test"),           1,  0,  BASE+SHORTSTRESS_TEST+4,  LoopTestProc } ,

    {TEXT("Chapter 9 Test(Sync)"),  0, 	0,  0, NULL},
    { TEXT("Descriptor related Test"),           1,  0,  CHAPTER9_CASE+1,  LoopTestProc } ,
    { TEXT("Interface related Test "),   1,  0,  CHAPTER9_CASE+2,  LoopTestProc } ,
    { TEXT("Device related Test "),   1,  0,  CHAPTER9_CASE+3,  LoopTestProc } ,

    {TEXT("Chapter 9 Test(Async)"),  0, 	0,  0, NULL},
    { TEXT("Descriptor related Test"),           1,  0,  CHAPTER9_CASE+ASYNC_TEST+1,  LoopTestProc } ,
    { TEXT("Interface related Test "),   1,  0,  CHAPTER9_CASE+ASYNC_TEST+2,  LoopTestProc } ,
    { TEXT("Device related Test "),   1,  0,  CHAPTER9_CASE+ASYNC_TEST+3,  LoopTestProc } ,

    {TEXT("USB Driver set net2280 device config"),  0, 0,  0, NULL},
    { TEXT("set lowest packet sizes for full speed config"),     1,  0, CONFIG_CASE+1,  LoopTestProc } ,
    { TEXT("set lowest packet sizes for high speed config"),     1,  0, CONFIG_CASE+2,  LoopTestProc } ,
    { TEXT("set alternative packet sizes for full speed config"),     1,  0, CONFIG_CASE+3,  LoopTestProc } ,
    { TEXT("set alternative packet sizes for high speed config"),     1,  0, CONFIG_CASE+4,  LoopTestProc } ,
    { TEXT("set highest packet sizes for highl speed config"),     1,  0, CONFIG_CASE+5,  LoopTestProc } ,
    { TEXT("restore default packet sizes for full speed config"),     1,  0, CONFIG_CASE+11,  LoopTestProc } ,
    { TEXT("restore default packet sizes for high speed config"),     1,  0, CONFIG_CASE+12,  LoopTestProc } ,

    {TEXT("USB Driver device reset/suspend/resume test"),  0, 0,  0, NULL},
    { TEXT("Reset device"),     1,  0, 9001,  LoopTestProc } ,
    { TEXT("Reset device no enum"),     1,  0, 9002,  LoopTestProc } ,
    { TEXT("Suspend device"),     1,  0, 9003,  LoopTestProc } ,
    { TEXT("Resume device"),     1,  0, 9004,  LoopTestProc } ,
    { TEXT("EP0 data transfer test"),     1,  0, 9005,  LoopTestProc } ,

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
            memset(g_pTstDevLpbkInfo, 0, sizeof(PUSB_TDEVICE_LPBKINFO));
            //update registry if needed
            if(g_fRegUpdated == FALSE && pUsbDriver->GetCurAvailDevs () == 0){
                NKDbgPrintfW(_T("Please stand by, installing usbtest.dll as the test driver..."));
                if(RegLoadUSBTestDll(TEXT("usbtest.dll"), FALSE) == FALSE){
                    NKDbgPrintfW(_T("can not install usb test driver,  make sure it does exist!"));
                    return TPR_NOT_HANDLED;
                }
                g_fRegUpdated = TRUE;
            }

            //we are waiting for the connection
            if(pUsbDriver->GetCurAvailDevs () == 0){
                HWND hDiagWnd = ShowDialogBox(TEXT("Please connect USB host with USB test device now!"));

                //CETK users: Please ignore following statement, this is only for automated test setup
                SetRegReady(1, 3);
                int iCnt = 0;
                while(pUsbDriver->GetCurAvailDevs () == 0){
                    if(++iCnt == 10){
                        DeleteDialogBox(hDiagWnd);
                        NKDbgPrintfW(_T("!!!NO device is attatched, test exit without running any test cases.!!!"));
                        return TPR_NOT_HANDLED;
                    }

                    NKDbgPrintfW(_T("Please connect USB host with USB test device now!"));
                    Sleep(2000);
                }
                DeleteDialogBox(hDiagWnd);
            }
            Sleep(2000);
            break;

        case SPM_UNLOAD_DLL:
            for(int i = 0; i < 4; i++){
                if(g_pTstDevLpbkInfo[i] != NULL)
                    delete g_pTstDevLpbkInfo[i];
            }
            DeleteRegEntry();
            if(g_fUnloadDriver == TRUE)
                RegLoadUSBTestDll(TEXT("usbtest.dll"), TRUE);
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
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: USBBVT.DLL"));
            break;

        case SPM_END_GROUP:
            g_pKato->EndLevel(TEXT("END GROUP: USBBVT.DLL"));
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
    NKDbgPrintfW(_T("CommandLine Option %s "),lpCommandLine);
    DWORD dwNum;

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

            default:
                lpCommandLine++;
                break;
        }
    }
    return TRUE;
}


//following are the packetsizes we will use in different cases
//default values
#define HIGH_SPEED_BULK_PACKET_SIZES        	0x200
#define FULL_SPEED_BULK_PACKET_SIZES        	0x40
#define HIGH_SPEED_ISOCH_PACKET_SIZES       0x200
#define FULL_SPEED_ISOCH_PACKET_SIZES       0x40
#define HIGH_SPEED_INT_PACKET_SIZES        	0x8
#define FULL_SPEED_INT_PACKET_SIZES        	0x40
//small packet sizes
#define BULK_PACKET_LOWSIZE        	0x8
#define ISOCH_PACKET_LOWSIZE      	0x10
#define INT_PACKET_LOWSIZE			0x10
//irregular packet sizes
#define BULK_PACKET_ALTSIZE        	0x20
#define ISOCH_PACKET_ALTSIZE      	0x400-4 // 1023 bytes
#define INT_PACKET_ALTSIZE			0x40-4  //63 bytes
//big packet sizes (high speed only, this is not used due the limitation of net2280
#define BULK_PACKET_HIGHSIZE        	0x40
#define ISOCH_PACKET_HIGHSIZE       0xc00  //  1024*2
#define INT_PACKET_HIGHSIZE        	0xc00   //  1024*2

//###############################################
//following APIs reconfig test device side
//###############################################


DWORD 
SetLowestPacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed)
{

    if(pDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have lowest packet sizes on each endpoint")));

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
    if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utrc) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT(" Set Lowest Packet Size failed"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

DWORD 
SetAlternativePacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed)
{

    if(pDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have alternative packet sizes on each endpoint")));

    USB_TDEVICE_REQUEST utdr;
    USB_TDEVICE_RECONFIG utrc = {0};

    utrc.ucConfig = CONFIG_DEFAULT;
    utrc.ucSpeed = (bHighSpeed == TRUE)?SPEED_HIGH_SPEED:SPEED_FULL_SPEED;
    utrc.wBulkPkSize = BULK_PACKET_ALTSIZE;
    utrc.wIntrPkSize = INT_PACKET_ALTSIZE;
    utrc.wIsochPkSize = ISOCH_PACKET_ALTSIZE;
      
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_RECONFIG;
    utdr.wValue = 0;
    utdr.wIndex = 0;
    utdr.wLength = sizeof(USB_TDEVICE_RECONFIG);

	//issue command to netchip2280
    if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utrc) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT(" Set Alternative Packet Size failed"));
        return TPR_FAIL;
    }

    return TPR_PASS;

}

DWORD 
SetHighestPacketSize(UsbClientDrv *pDriverPtr)
{

    if(pDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("Reconfigure the loopback device to have highest packet sizes on each endpoint")));

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
    if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utrc) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT(" SetHighest Packet Size failed"));
        return TPR_FAIL;
    }

    return TPR_PASS;

}


DWORD 
RestoreDefaultPacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed)
{

    if(pDriverPtr == NULL)
        return TPR_FAIL;

    DEBUGMSG(DBG_DETAIL,(TEXT("restore default packet sizes on each endpoint")));

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
    if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utrc) == FALSE){
        g_pKato->Log(LOG_FAIL, TEXT(" Restore default packet Size failed"));
        return TPR_FAIL;
    }

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
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)&uNumofPairs) == FALSE){
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
    if(SendVendorTransfer(pDriverPtr, TRUE, (PUSB_TDEVICE_REQUEST)&utdr, (LPVOID)pTDLpbkInfo) == FALSE){
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
    DWORD   dwExceptionCode;

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
SendVendorTransfer(UsbClientDrv *pDriverPtr, BOOL bIn, PUSB_TDEVICE_REQUEST pUtdr, LPVOID pBuffer){
    if(pDriverPtr == NULL || pUtdr == NULL)
        return FALSE;

    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, 
                                                                                             (bIn == TRUE)?USB_IN_TRANSFER:USB_OUT_TRANSFER, 
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
    if(bRet == FALSE || dwError != USB_NO_ERROR || cbdwBytes != pUtdr->wLength){
        NKDbgPrintfW(TEXT("issueVendorTransfer has encountered some error!\r\n"));
        bRet = FALSE;        
    }

    (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);

    return bRet;
}


//###############################################
//Some additional test cases
//###############################################

DWORD 
ResetDevice(UsbClientDrv *pDriverPtr, BOOL bReEnum)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(bReEnum == TRUE)
        NKDbgPrintfW(_T("Now Reset the device and re-enumerate it"));
    else 
        NKDbgPrintfW(_T("Now Reset the device and without re-enumerate it"));
        
    if(pDriverPtr->DisableDevice(bReEnum, 0) == FALSE){
        NKDbgPrintfW(_T("Failed on disable device with automatic re-enumeration!"));
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

    return TPR_PASS;
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

    return TPR_PASS;
}

#define EP0TEST_MAX_DATASIZE    256
DWORD
EP0Test(UsbClientDrv *pDriverPtr){

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    USB_TDEVICE_REQUEST utdr= {0};
    BYTE pBuf[EP0TEST_MAX_DATASIZE ]= {0};
    BYTE bStartVal = 0;
    BYTE bOdd = 0;
    USHORT usLen = 0;
    BOOL fRet = FALSE;

    for(int i = 0; i < 1024; i ++){
        bStartVal = rand() % 256;
        bOdd = rand() % 2;
        usLen = rand() % 256;
        if(usLen == 0)
            usLen = 1;
        NKDbgPrintfW(_T("EP0Test: No. %d run, data size = %d, %s transfer \r\n"), 
                                    i, usLen, (bOdd == 0)? _T("IN"):_T("OUT"));

        if(bOdd == 0){//in transfer
            utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
            utdr.bRequest = TEST_REQUEST_EP0TESTIN;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            if(SendVendorTransfer(pDriverPtr, TRUE, &utdr, pBuf) == FALSE){
                g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed!"));
                goto EXIT;
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
            utdr.bRequest = TEST_REQUEST_EP0TESTOUT;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            for(int j = 0; j < usLen; j++){
                pBuf[j] = bStartVal ++;
            }
            if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, pBuf) == FALSE){
                g_pKato->Log(LOG_FAIL, TEXT("Send OUT transfer command failed!"));
                goto EXIT;
            }
        }
        memset(pBuf, 0, sizeof(pBuf));
    }

    fRet = TRUE;

EXIT:

    g_pKato->Log(LOG_DETAIL, TEXT("!!!PLEASE CHECK FUNCTION SIDE OUTPUT TO SEE WHETHER THERE ARE SOME OUT TRANSFER ERRORS!!!"));

    return (fRet == TRUE)?TPR_PASS:TPR_FAIL;
}

