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

//
// Module Name:
//   USBFnClient.cpp
//
// Abstract:  This file includes functions that related the function driver initialization
//                reset, close, and other events' handling.
//
//
// Notes:
//



#include <windows.h>
#include <usbfn.h>
#include <usbfntypes.h>
#include "config.h"
#include "USBFnClient.h"
#include "tuxmain.h"


//-------------------------GLOABLE VARS-------------------------

static UFN_CLIENT_REG_INFO                  g_RegInfo = { sizeof(UFN_CLIENT_REG_INFO) };

static LPCWSTR g_rgpszStrings0409[] = {
    g_RegInfo.szVendor, g_RegInfo.szProduct, g_RegInfo.szSerialNumber};

static UFN_STRING_SET g_rgStringSets[] = {
    0x0409, g_rgpszStrings0409, _countof(g_rgpszStrings0409)
};

static CDeviceConfig* g_pDevConfig;



// **************************************************************************
//                                            P U B L I C  F U N C T I O N S
// **************************************************************************
// Process a device event.
static
BOOL
WINAPI
USBFNBVT_DeviceNotify(
    PVOID   pvNotifyParameter,
    DWORD   dwMsg,
    DWORD   dwParam
    )
{
    SETFNAME_DBG(_T("USBFNBVT_DeviceNotify"));
    FUNCTION_ENTER_MSG();

    PUFL_CONTEXT pContext = (PUFL_CONTEXT)pvNotifyParameter;
    if(pContext == NULL) //  this should not happen
        return FALSE;

    EnterCriticalSection(&pContext->cs);

    LOG(_T("Event is: 0x%x, param is: 0x%x \r\n"), dwMsg, dwParam);

    LOG(_T("BVT_EVENT"));

    FUNCTION_LEAVE_MSG();

    LeaveCriticalSection(&pContext->cs);

    return TRUE;
}
//
// Initialiaztion
//
extern "C"
DWORD
BVT_Init(
    LPCTSTR pszActiveKey
    )
{
    SETFNAME(_T("BVT_Init:"));
    FUNCTION_ENTER_MSG();

      if(pszActiveKey == NULL){
        return 0;
        }
    //setup device configuation
        PDEVICE_SETTING pDS = InitializeSettings();
        if(pDS == NULL){
            ERRORMSG(TRUE,(_T("%s Can not get device-specific data!!!\r\n"), pszFname));
            return FALSE;
        }
        g_pDevConfig = (CDeviceConfig *) new CDeviceConfig;
        if(g_pDevConfig == NULL){
            ERRORMSG(TRUE,(_T("%s Out of memory\r\n"), pszFname));
            return FALSE;
        }
        g_pDevConfig->SetDevSetting(pDS);

        if(g_pDevConfig->InitDescriptors() == FALSE){
            ERRORMSG(TRUE,(_T("%s Can not create Test Client device settings data structures!!!\r\n"), pszFname));
            delete g_pDevConfig;
            g_pDevConfig = NULL;
            return FALSE;
        }


    PUFL_CONTEXT pContext = (PUFL_CONTEXT) new UFL_CONTEXT;
    if(pContext == NULL){
        ERRORMSG(TRUE,(_T("%s Can not initialize context!!!\r\n"), pszFname));
        return 0;
    }
    memset(pContext, 0, sizeof(UFL_CONTEXT));

    BOOL fInit = FALSE;

    wcscpy_s(pContext->szActiveKey, (_countof(pContext->szActiveKey)), pszActiveKey);
    pContext->pUfnFuncs = (PUFN_FUNCTIONS)new UFN_FUNCTIONS;
    if(pContext->pUfnFuncs == NULL){
        ERRORMSG(TRUE,(_T("%s Can not initialize BVT function table!!!\r\n"), pszFname));
        goto EXIT;
    }

    DWORD dwRet = UfnInitializeInterface(pszActiveKey, &pContext->hDevice,
                                                            pContext->pUfnFuncs, &pContext->pvInterface);

    if (dwRet != ERROR_SUCCESS) {
        goto EXIT;
    }

    InitializeCriticalSection(&pContext->cs);
    pContext->pHighSpeedDeviceDesc = g_pDevConfig->GetCurHSDeviceDesc();
    pContext->pHighSpeedConfig = g_pDevConfig->GetCurHSConfig();
    pContext->pFullSpeedDeviceDesc = g_pDevConfig->GetCurFSDeviceDesc();
    pContext->pFullSpeedConfig = g_pDevConfig->GetCurFSConfig();

    if(pContext->pHighSpeedConfig == NULL || pContext->pHighSpeedDeviceDesc == NULL ||
            pContext->pFullSpeedConfig == NULL ||pContext->pFullSpeedDeviceDesc == NULL){//this should not happen
        ERRORMSG(TRUE,(_T("%s got invalid descriptor or config!!!\r\n"), pszFname));
        goto EXIT;
    }

    dwRet = UfnGetRegistryInfo(pszActiveKey, &g_RegInfo);
    if (dwRet != ERROR_SUCCESS) {
        goto EXIT;
    }

    // Adjust device descriptors
    pContext->pHighSpeedDeviceDesc->idVendor = (USHORT) g_RegInfo.idVendor;
    pContext->pHighSpeedDeviceDesc->idProduct = (USHORT) g_RegInfo.idProduct;
    pContext->pHighSpeedDeviceDesc->bcdDevice = (USHORT) g_RegInfo.bcdDevice;

    pContext->pFullSpeedDeviceDesc->idVendor = pContext->pHighSpeedDeviceDesc->idVendor;
    pContext->pFullSpeedDeviceDesc->idProduct = pContext->pHighSpeedDeviceDesc->idProduct;
    pContext->pFullSpeedDeviceDesc->bcdDevice = pContext->pHighSpeedDeviceDesc->bcdDevice;

    DWORD cStrings = _countof(g_rgpszStrings0409);
    DWORD iSerialNumber = 3;
    if (g_RegInfo.szSerialNumber[0] == 0) {
        DWORD dwSuccessSerialNumber = UfnGetSystemSerialNumber(
            g_RegInfo.szSerialNumber, _countof(g_RegInfo.szSerialNumber));

        if (dwSuccessSerialNumber != ERROR_SUCCESS) {
            // No serial number
            cStrings = _countof(g_rgpszStrings0409) - 1;
            iSerialNumber = 0;
        }
    }

    g_rgStringSets[0].cStrings = cStrings;
    pContext->pHighSpeedDeviceDesc->iSerialNumber = (UCHAR) iSerialNumber;
    pContext->pFullSpeedDeviceDesc->iSerialNumber = (UCHAR) iSerialNumber;


    LOG(_T("%s Register device \r\n"), pszFname);
    dwRet = (pContext->pUfnFuncs)->lpRegisterDevice(pContext->hDevice,
                                                             pContext->pHighSpeedDeviceDesc, pContext->pHighSpeedConfig,
                                                            pContext->pFullSpeedDeviceDesc, pContext->pFullSpeedConfig,
                                                            g_rgStringSets, _countof(g_rgStringSets));
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(TRUE, (_T("%s Descriptor registration using default config failed\r\n"), pszFname));
        goto EXIT;
    }


    // Start the device controller
    dwRet = pContext->pUfnFuncs->lpStart(pContext->hDevice, USBFNBVT_DeviceNotify, pContext,
        &pContext->hDefaultPipe);
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(TRUE, (_T("%s Device controller failed to start\r\n"), pszFname));
        goto EXIT;
    }

    fInit = TRUE;

EXIT:

    if(fInit == FALSE){//initialization failed

        if(pContext->pUfnFuncs != NULL){
            pContext->pUfnFuncs->lpStop(pContext->hDevice); //we may need to stop device
            DeleteCriticalSection(&pContext->cs);
            delete pContext->pUfnFuncs;
            pContext->pUfnFuncs = NULL;
            UfnDeinitializeInterface(pContext->pvInterface);
        }

        delete pContext;
        pContext = NULL;
    }

    FUNCTION_LEAVE_MSG();
    return (DWORD)pContext;
}
extern "C"
BOOL
BVT_Deinit(
    DWORD               dwCtx)
{

    PUFL_CONTEXT pContext = (PUFL_CONTEXT)dwCtx;
    if(pContext == NULL || pContext->pUfnFuncs == NULL || pContext->hDevice == NULL
        || pContext->pvInterface == NULL) {//nothing to do here
        return TRUE;
    }

    SETFNAME_DBG(_T("USBFNBVT_Deinit:"));
    FUNCTION_ENTER_MSG();

    pContext->pUfnFuncs->lpStop(pContext->hDevice);
    pContext->pUfnFuncs->lpDeregisterDevice(pContext->hDevice);

    UfnDeinitializeInterface(pContext->pvInterface);
    delete pContext->pUfnFuncs;
    pContext->pUfnFuncs = NULL;

    DeleteCriticalSection(&pContext->cs);

    delete pContext;
    pContext = NULL;

     if(g_pDevConfig != NULL){
            delete g_pDevConfig;
            g_pDevConfig = NULL;
     }

    FUNCTION_LEAVE_MSG();
    return TRUE;
}

extern "C"
DWORD
BVT_Open(
    DWORD               dwCtx,
    DWORD               dwAccMode,
    DWORD               dwShrMode
    )
{
    UNREFERENCED_PARAMETER(dwCtx);
    UNREFERENCED_PARAMETER(dwAccMode);
    UNREFERENCED_PARAMETER(dwShrMode);
    return 1;
}

extern "C"
DWORD
BVT_Close()
{
    return ERROR_SUCCESS;
}

extern "C"
DWORD
BVT_Read(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{
    UNREFERENCED_PARAMETER(dwA);
    UNREFERENCED_PARAMETER(lpA);
    UNREFERENCED_PARAMETER(dwB);
    return (DWORD)-1;
}

extern "C"
DWORD
BVT_Write(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{
    UNREFERENCED_PARAMETER(dwA);
    UNREFERENCED_PARAMETER(lpA);
    UNREFERENCED_PARAMETER(dwB);
    return (DWORD)-1;
}

extern "C"
DWORD
BVT_Seek(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{
    UNREFERENCED_PARAMETER(dwA);
    UNREFERENCED_PARAMETER(lpA);
    UNREFERENCED_PARAMETER(dwB);
    return (DWORD)-1;
}

extern "C"
BOOL
BVT_IOControl(
    LPVOID              pHidKbd,
    DWORD               dwCode,
    PBYTE               pBufIn,
    DWORD               dwLenIn,
    PBYTE               pBufOut,
    DWORD               dwLenOut,
    PDWORD              pdwActualOut
    )
{
    UNREFERENCED_PARAMETER(pHidKbd);
    UNREFERENCED_PARAMETER(dwCode);
    UNREFERENCED_PARAMETER(pBufIn);
    UNREFERENCED_PARAMETER(dwLenIn);
    UNREFERENCED_PARAMETER(pBufOut);
    UNREFERENCED_PARAMETER(dwLenOut);
    UNREFERENCED_PARAMETER(pdwActualOut);
    return TRUE;
}


