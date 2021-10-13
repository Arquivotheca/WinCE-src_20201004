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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
//    Driver entry points for SD Memory Card client driver

#include "SDTst.h"
#include <sdcommon.h>
#include "drv_tests.h"
#include "drv_tst_busReq.h"
#include "drv_tst_infoQuery.h"
#include "drv_tst_misc.h"
#include "drv_tst_memory.h"
#include "drv_tst_SetCardFeature.h"
#include "drv_tst_featurefullness.h"
#include <diskio.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>
#include <ceddk.h>

HINSTANCE g_hInstance;
CRITICAL_SECTION g_RemovalLock;
CRITICAL_SECTION g_CriticalSection;

    // initialize debug zones
SD_DEBUG_INSTANTIATE_ZONES(
    TEXT("SD Memory Card Client Driver"), // module name
    ZONE_ENABLE_INIT | ZONE_ENABLE_ERROR | ZONE_ENABLE_WARN | ZONE_ENABLE_INFO | ENABLE_ZONE_TST | ZONE_ENABLE_FUNC | ENABLE_ZONE_WRAP,   // initial settings
    TEXT("Disk I/O"),
    TEXT("Card I/O"),
    TEXT("Bus Requests"),
    TEXT("Power"),
    TEXT("Test Code"),
    TEXT("Wrappers"),
    TEXT(""),
    TEXT(""),
    TEXT(""),
    TEXT(""),
    TEXT(""));

///////////////////////////////////////////////////////////////////////////////
//  DllMain - the main dll entry point
//  Input:  hInstance - the instance that is attaching
//        Reason - the reason for attaching
//        pReserved - not much
//  Output:
//  Return: always returns TRUE
//  Notes:  this is only used to initialize the zones
///////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(HANDLE hInstDll, DWORD dwReason, LPVOID /*lpvReserved*/)
{
    BOOL fRet = TRUE;

    if ( dwReason == DLL_PROCESS_ATTACH )
    {
        if (SDInitializeCardLib())
        {
            g_hInstance = (HINSTANCE) hInstDll;
            InitializeCriticalSection(&g_RemovalLock);
            InitializeCriticalSection(&g_CriticalSection);
        }
        else
        {
            fRet = FALSE;
        }
    }

    if ( dwReason == DLL_PROCESS_DETACH )
    {
        SDDeinitializeCardLib();
        DeleteCriticalSection(&g_RemovalLock);
        DeleteCriticalSection(&g_CriticalSection);
    }

    return fRet;

}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Close - the close entry point for the memory driver
//  Input:  hOpenContext - the context returned from SMC_Open
//  Output:
//  Return: always returns TRUE
//  Notes:
///////////////////////////////////////////////////////////////////////////////
BOOL SMC_Close(DWORD /*hOpenContext*/)
{
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +-SMC_Close\n")));
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//  CleanUpDevice - cleanup the device instance
//  Input:  pDevice - device instance
//  Output:
//  Return:
//  Notes:
///////////////////////////////////////////////////////////////////////////////
VOID CleanUpDevice(PSDCLNT_CARD_INFO pDevice)
{
 //   DeinitializePowerManagement(pDevice);

        // acquire removal lock
    AcquireRemovalLock(pDevice);

    if (NULL != pDevice->pRegPath)
    {
            // free the reg path
        SDFreeMemory(pDevice->pRegPath);
    }

/*    if (NULL != pDevice->hBufferList)
    {
        // delete the buffer memory list
        SDDeleteMemList(pDevice->hBufferList);
    }*/

    ReleaseRemovalLock(pDevice);

        // free the device memory
    SDFreeMemory(pDevice);
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Deinit - the deinit entry point for the memory driver
//  Input:  hDeviceContext - the context returned from SMC_Init
//  Output:
//  Return: always returns TRUE
//  Notes:
///////////////////////////////////////////////////////////////////////////////
BOOL SMC_Deinit(DWORD hDeviceContext)
{
    PSDCLNT_CARD_INFO pDevice;

    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: +SMC_Deinit\n")));

    pDevice = (PSDCLNT_CARD_INFO)hDeviceContext;

    // now it is safe to clean up
    CleanUpDevice(pDevice);

    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Deinit\n")));

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//  SlotEventCallBack - slot event callback for fast-path events
//  Input:  hDevice - device handle
//        pContext - device specific context that was registered
//        SlotEventType - slot event type
//        pData - Slot event data (can be NULL)
//        DataLength - length of slot event data (can be 0)
//  Output:
//  Return:
//  Notes:
//
//    If this callback is registered the client driver can be notified of
//    slot events (such as device removal) using a fast path mechanism. This
//    is useful if a driver must be notified of device removal
//    before its XXX_Deinit is called.
//
//    This callback can be called at a high thread priority and should only
//    set flags or set events. This callback must not perform any
//    bus requests or call any APIs that can perform bus requests.
///////////////////////////////////////////////////////////////////////////////
VOID SlotEventCallBack(SD_DEVICE_HANDLE /*hDevice*/, PVOID pContext, SD_SLOT_EVENT_TYPE SlotEventType, PVOID /*pData*/, DWORD /*DataLength*/)
{
    PSDCLNT_CARD_INFO pDevice;
    HANDLE hEvnt = NULL;

    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: +SlotEventCallBack - %u \n"),SlotEventType));
  pDevice = (PSDCLNT_CARD_INFO)pContext;

    switch (SlotEventType)
    {
        case SDCardEjected:
            // mark that the card is being ejected
            pDevice->CardEjected = TRUE;
            // acquire the removal lock to block this callback
            // in case an ioctl is in progress
            AcquireRemovalLock(pDevice);
            ReleaseRemovalLock(pDevice);

            break;
              case SDCardSelected:
            //make sure that the card was previously sent the SDCardBeginSelectDeselect
            //message
        //    DEBUGCHK(pDevice->CardBeginSelectDeselect);
            //reset the CardBeginSelectDeselect bit
            pDevice->CardBeginSelectDeselect = FALSE;
            //mark the card as selected
            pDevice->CardSelected = TRUE;
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDIOTst: SDCardSelected\n")));
            break;
        case SDCardDeselected:
            //make sure that the card was previously sent the SDCardBeginSelectDeselect
            //message
            DEBUGCHK(pDevice->CardBeginSelectDeselect);
            //reset the CardBeginSelectDeselect bit
            pDevice->CardBeginSelectDeselect = FALSE;
            //mark the card as deselected
            pDevice->CardSelected = FALSE;
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDIOTst: SDCardDeSelected\n")));
            break;
        case SDCardBeginSelectDeselect:
            pDevice->CardBeginSelectDeselect = TRUE;
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDIOTst: SDBeginSelectDeselect\n")));
            break;
        case SDCardDeselectRequest:
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDIOTst: SDCardDeselectRequest\n")));
            pDevice->CardBeginSelectDeselect = FALSE;
            break;
        case SDCardSelectRequest:
            pDevice->CardBeginSelectDeselect = FALSE;
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDIOTst: SDCardSelectRequest\n")));
            hEvnt = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("CardSelectedEvent"));
            if (hEvnt)
            {
                SetEvent(hEvnt);
            }
            break;

    }

    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SlotEventCallBack \n")));
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Init - the init entry point for the memory driver
//  Input:  dwContext - the context for this init
//  Output:
//  Return: non-zero context
//  Notes:
///////////////////////////////////////////////////////////////////////////////
DWORD SMC_Init(DWORD dwContext)
{
    SD_DEVICE_HANDLE                hClientHandle;  // client handle
    PSDCLNT_CARD_INFO                pDevice;        // this instance of the device
    SDCARD_CLIENT_REGISTRATION_INFO ClientInfo;    // client into
    SD_API_STATUS                Status;        // intermediate status
    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: +SMC_Init\n")));
    SDIO_CARD_INFO                        sdioCI;

  //Test GetBusNamePrefix
  HANDLE hBusHandle = NULL;
  LPTSTR szBusPrefix = new TCHAR[MAX_PATH];
//    szBusPrefix = LocalAlloc(LMEM_ZEROINIT,MAX_PATH);
  hBusHandle = CreateBusAccessHandle ((LPCTSTR)dwContext);
  if(hBusHandle == NULL || hBusHandle == INVALID_HANDLE_VALUE)
  {
      DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDTst: Failed to create bus handle \n")));
          DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
          if(szBusPrefix)
           delete[] szBusPrefix;
          return 0;
  }
  if(szBusPrefix)
    {
     if(!GetBusNamePrefix(hBusHandle,szBusPrefix,MAX_PATH))
     {
             DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDTst: Failed call to GetBusNamePrefix\n")));
             DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
             if(szBusPrefix)
              delete[] szBusPrefix;
             return 0;
     }
  }
   if(szBusPrefix)
       delete[] szBusPrefix;

  CloseBusAccessHandle(hBusHandle);
  //ePrefix(

  //////////
  pDevice = (PSDCLNT_CARD_INFO)SDAllocateMemoryWithTag(sizeof(SDCLNT_CARD_INFO),
                                                        SD_MEMORY_TAG);

    if (pDevice == NULL)
    {
        DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDTst: Failed to allocate device info \n")));
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
        return 0;
    }

    pDevice->pCriticalSection = &g_RemovalLock;
    pDevice->pRemovalLock = &g_CriticalSection;

    // get the device handle from the bus driver
    hClientHandle = SDGetDeviceHandle(dwContext, &pDevice->pRegPath);
    // store device handle in local context
    pDevice->hDevice = hClientHandle;

    if (NULL == hClientHandle)
    {
        DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDTst: Failed to get client handle \n")));
        CleanUpDevice(pDevice);
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
        return 0;
    }
    pDevice->hInst = g_hInstance;
    // register our debug zones
    SDRegisterDebugZones(hClientHandle, pDevice->pRegPath);
    pDevice->pRegPathAlt = (PWSTR) malloc(sizeof(WCHAR) * MAX_KEY_PATH_LENGTH);

    pDevice->dwError = SDGetRegPathFromInitContext((PWCHAR)dwContext, pDevice->pRegPathAlt, sizeof(WCHAR) * MAX_KEY_PATH_LENGTH);

    memset(&ClientInfo, 0, sizeof(ClientInfo));

    // set client options and register as a client device
    StringCchCopy(ClientInfo.ClientName, _countof(ClientInfo.ClientName), TEXT("Memory Card"));

    // set the callback
    ClientInfo.pSlotEventCallBack = SlotEventCallBack;

    Status = SDRegisterClient(hClientHandle,
                            pDevice,
                            &ClientInfo);

    if (!SD_API_SUCCESS(Status))
    {
        DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDTst: Failed to register client : 0x%08X \n"),Status));
        CleanUpDevice(pDevice);
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
        return 0;
    }

    //decide if we want to use softblock or not
    BOOL bUseSoftBlock = false;
    NKDbgPrintfW(TEXT("Reg Path: %s\n"),pDevice->pRegPath);
    LONG rVal = 0, rVal2 = 0;
    HKEY hKey = NULL;
    DWORD dwTemp,dwSb,dwSize;
  rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,pDevice->pRegPath,0,0,&hKey);
    if (rVal != ERROR_SUCCESS)
    {
         dwTemp = GetLastError();
         DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: Couldn't open regkey, so not using SoftBlock\n")));
             NKDbgPrintfW(TEXT("\tError Code: %d returned."),rVal);
             bUseSoftBlock = false;
  }
    else
     {
       dwSize = sizeof(DWORD);
       rVal = RegQueryValueEx(hKey, TEXT("UseSoftBlock"),NULL,NULL,(LPBYTE)&dwSb, &dwSize);
            if (rVal != ERROR_SUCCESS)
            {
                   DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: Couldn't query regkey, so not using SoftBlock\n")));
                   bUseSoftBlock = false;
            }
        else
         {
           if(dwSb)
           {
              bUseSoftBlock = true;
           }
           else
           {
             bUseSoftBlock = false;
           }
         }
     }
  if (hKey)
    {
        rVal2 = RegCloseKey(hKey);
}
    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: -SMC_Init\n")));
    if(bUseSoftBlock)
    {
       DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: Using SoftBlock\n")));
    }
  else
  {
    DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDTst: NOT Using SoftBlock\n")));
  }
  if(bUseSoftBlock)
    {
  BOOL bSoftBlockSupported;

    // Determine if the bus driver supports Soft-Block
    bSoftBlockSupported = SD_API_SUCCESS
    (SDSetCardFeature(hClientHandle, SD_IS_SOFT_BLOCK_AVAILABLE, NULL,0)
    );

    if(bSoftBlockSupported)
    {
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SOFT_BLOCK IS AVAILABLE IN THE BUS DRIVER.\n")));
        Status = SDCardInfoQuery(hClientHandle,SD_INFO_SDIO, &sdioCI, sizeof(SDIO_CARD_INFO));
        if((SD_API_SUCCESS(Status)) && (sdioCI.CardCapability & SD_IO_CARD_CAP_SUPPORTS_MULTI_BLOCK_TRANS))
        {
            //Real block-mode is supported by the card.
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("Real block mode is supported, but we are forcing the use of Soft-Block.\n")));

        }
        else
        {
            DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("Real block mode is not supported, so we are using Soft-Block.\n")));

        }
    }
    else
    {
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SOFT_BLOCK IS NOT AVAILABLE IN THE BUS DRIVER.\n")));

    }

    bool bMultiBlockDisabled;
    //Disable Multi-Block operations to enable testing of
    //the bus driver's Soft-Block path.
    bMultiBlockDisabled = bSoftBlockSupported &&
        SD_API_SUCCESS(
            //Request the bus driver to breakup all multi-block
            //operations.  Only byte mode operations are passed
            //to the host control driver.
            SDSetCardFeature(hClientHandle,SD_SOFT_BLOCK_FORCE_UTILIZATION,NULL,TRUE));// &&
        //    SD_API_SUCCESS(
            //Request the bus driver to breakup all multi-block
            //operations.  Only byte mode operations are passed
            //to the host control driver.
        //    SDSetCardFeature(hClientHandle,SD_SOFT_BLOCK_DEFAULT_UTILIZATON,NULL,TRUE)
        //    );

    if(bMultiBlockDisabled)
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("Successfully disabled multi-block mode.  Using soft-block.\n")));
    else
        DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("Unable to disable multi-block mode.  Not using soft-block.\n")));
  }
    return (DWORD)pDevice;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_IOControl - the I/O control entry point for the memory driver
//  Input:  Handle - the context returned from SMC_Open
//        IoctlCode - the ioctl code
//        pInBuf - the input buffer from the user
//        InBufSize - the length of the input buffer
//        pOutBuf - the output buffer from the user
//        InBufSize - the length of the output buffer
//        pBytesReturned - the size of the transfer
//  Output:
//  Return:  TRUE if ioctl was handled
//  Notes:
///////////////////////////////////////////////////////////////////////////////
BOOL SMC_IOControl (DWORD hOpenContext, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut,
                    DWORD dwLenOut, PDWORD pdwActualOut)
{
    HANDLE hEvnt = NULL;

    BOOL retValue = TRUE;    // return value

    BOOL tmp;

    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +SMC_IOControl\n")));
    if (IS_TST_IOCTL(dwCode) && ((pBufOut == NULL) || (dwLenOut == 0) ))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        retValue = FALSE;

    }
    else
    {
        tmp = IS_TST_IOCTL(dwCode);
            // any of these IOCTLs can access the device instance or card handle
            // so we must protect it from being freed from XXX_Deinit
        AcquireRemovalLock((PSDCLNT_CARD_INFO)hOpenContext);
        DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("\t\tSMC_IOControl: Received IOCTL  = %s (0x%x) "),TranslateIOCTLS(dwCode), dwCode));
        if (IS_TST_IOCTL(dwCode) && (pBufOut != NULL))
        {
            InitializeTestParamsBufferAndResult((PTEST_PARAMS) pBufOut);
        }
        DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("\t\t%s\r\n"),TranslateIOCTLS(dwCode)));
        switch (dwCode)
        {
            case DISK_IOCTL_INITIALIZED:
                __fallthrough;
            case IOCTL_DISK_INITIALIZED:
                hEvnt = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVNT_NAME);
                if (hEvnt)
                {
                    retValue = SetEvent(hEvnt);
                }
                else
                {
                    retValue = FALSE;
                }
                break;
            case IOCTL_SAMPLE_TEST1:
                SDDummyTst1((PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_SAMPLE_TEST2:
                SDDummyTst2((PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_SIMPLE_BUSREQ_TST:
                SDBusSingleRequestTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_SIMPLE_INFO_QUERY_TST:
                SDInfoQueryTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_CANCEL_BUSREQ_TST:
                SDCancelBusRequestTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_RETRY_ACMD_BUSREQ_TST:
                SDACMDRetryBusRequestTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_INIT_TST:
                InitStuffTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_GET_BIT_SLICE_TST:
                GetBitSliceTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_OUTPUT_BUFFER_TST:
                SDOutputBufferTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_MEMLIST_TST:
                MemListTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_SETCARDFEATURE_TST:
                SetCardFeatureTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_RW_MISALIGN_OR_PARTIAL_TST:
                SDReadWritePartialMisalignTest(2, (PTEST_PARAMS)pBufOut, (PSDCLNT_CARD_INFO)hOpenContext);
                if (pdwActualOut) *pdwActualOut = dwLenOut;
                break;
            case IOCTL_GET_HOST_INTERFACE:
                GetHostInterface((PSDCLNT_CARD_INFO)hOpenContext,pBufIn,dwLenIn,pBufOut,dwLenOut,pdwActualOut);
              break;
                case IOCTL_GET_CARD_INTERFACE:
                GetCardInterface((PSDCLNT_CARD_INFO)hOpenContext,pBufIn,dwLenIn,pBufOut,dwLenOut,pdwActualOut);
              break;
            default:
                if (IS_TST_IOCTL(dwCode))
                {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    retValue = FALSE;
                }
                else
                {
                    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("Letting Unknown non-Test IOCL pass through...")));
                }
        }
        if (IS_TST_IOCTL(dwCode))
        {
            DeleteTestBufferCriticalSection();
        }
        ReleaseRemovalLock((PSDCLNT_CARD_INFO)hOpenContext);
    }
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: -SMC_IOControl\n")));
    return retValue;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Open - the open entry point for the memory driver
//  Input:  hDeviceContext - the device context from SMC_Init
//        AccessCode - the desired access
//        ShareMode - the desired share mode
//  Output:
//  Return: open context to device instance
//  Notes:
///////////////////////////////////////////////////////////////////////////////
DWORD SMC_Open(DWORD hDeviceContext, DWORD /*AccessCode*/, DWORD /*ShareMode*/)
{
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +-SMC_Open\n")));
    return hDeviceContext;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_PowerDown - the power down entry point for the bus driver
//  Input:  hDeviceContext - the device context from SMC_Init
//  Output:
//  Return:
//  Notes:  performs no actions
///////////////////////////////////////////////////////////////////////////////
void SMC_PowerDown(DWORD /*hDeviceContext*/)
{
        // no prints allowed
    return;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_PowerUp - the power up entry point for the CE file system wrapper
//  Input:  hDeviceContext - the device context from SMC_Init
//  Output:
//  Return:
//  Notes:  performs no actions
///////////////////////////////////////////////////////////////////////////////
void SMC_PowerUp(DWORD /*hDeviceContext*/)
{
        // no prints allowed
    return;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Read - the read entry point for the memory driver
//  Input:  hOpenContext - the context from SMC_Open
//        pBuffer - the user's buffer
//        Count - the size of the transfer
//  Output:
//  Return: zero
//  Notes:  always returns zero (failure)
///////////////////////////////////////////////////////////////////////////////
DWORD SMC_Read(DWORD    /*hOpenContext*/,
            LPVOID   /*pBuffer*/,
            DWORD    /*Count*/)
{
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +-SMC_Read\n")));
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Seek - the seek entry point for the memory driver
//  Input:  hOpenContext - the context from SMC_Open
//        Amount - the amount to seek
//        Type - the type of seek
//  Output:
//  Return: zero
//  Notes:  always returns zero (failure)
///////////////////////////////////////////////////////////////////////////////
DWORD SMC_Seek(DWORD /*hOpenContext*/, long /*Amount*/, DWORD /*Type*/)
{
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +-SMC_Seek\n")));
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//  SMC_Write - the write entry point for the memory driver
//  Input:  hOpenContext - the context from SMC_Open
//        pBuffer - the user's buffer
//        Count - the size of the transfer
//  Output:
//  Return: zero
//  Notes:  always returns zero (failure)
///////////////////////////////////////////////////////////////////////////////
DWORD SMC_Write(DWORD /*hOpenContext*/,LPCVOID /*pBuffer*/,DWORD /*Count*/)
{
    DbgPrintZo(SDCARD_ZONE_FUNC, (TEXT("SDTst: +-SMC_Write\n")));
    return 0;
}

