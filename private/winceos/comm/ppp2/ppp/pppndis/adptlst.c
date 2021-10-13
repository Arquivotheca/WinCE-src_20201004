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
/*****************************************************************************
* 
*
*   @doc EX_RAS
*   adptlst.c   Miniport utilities
*
*   Date: 2/25/99
*
*/


//  Include Files

#include "windows.h"
#include "ndis.h"
#include "ras.h"
#include "raserror.h"
#include "cxport.h"
#include "protocol.h"
#include "ppp.h"
#include "macp.h"

#define COUNTOF(array) sizeof(array)/sizeof(array[0])

//
//  This is the RASDEVINFOW structure used in CE 3.0 and earlier.
//
#define RAS_MaxDeviceName_V3 32
typedef struct
{
    DWORD       dwSize;
    WCHAR       szDeviceType[RAS_MaxDeviceType+1];
    WCHAR       szDeviceName[RAS_MaxDeviceName_V3+1];
} RASDEVINFOW_V3, *LPRASDEVINFOW_V3;

// List of the types of miniport media that PPP can bind to
NDIS_MEDIUM MediaArray[1] = {NdisMediumWan};

DWORD
AddAdapter(
    IN      PNDIS_STRING    pAdapterString)
{
    PNDISWAN_ADAPTER    pNew;
    NDIS_STATUS         Status;         // Status for NDIS calls.
    NDIS_STATUS         OpenStatus;
    UINT                SelectedMediumIndex;
    uint                cAdapterStr;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +AddAdapter(%s)\n"), pAdapterString->Buffer));

    // Try to allocate a new adapter structure.
    pNew = (PNDISWAN_ADAPTER)pppAllocateMemory (sizeof (NDISWAN_ADAPTER));
    if (NULL == pNew) {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: -AddAdapter: Unable to allocate new adapter\n")));
        return NDIS_STATUS_RESOURCES;
    }

    cAdapterStr = (wcslen(pAdapterString->Buffer)+1)*sizeof(WCHAR);

    // Allocate space for the name
    pNew->szAdapterName = (LPTSTR)pppAllocateMemory(cAdapterStr);

    if (NULL == pNew->szAdapterName)
    {
        pppFreeMemory (pNew, sizeof (NDISWAN_ADAPTER));
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: -AddAdapter: Unable to allocate new adaptername\n")));
        return NDIS_STATUS_RESOURCES;
    }
    _tcscpy (pNew->szAdapterName, pAdapterString->Buffer);
    pNew->hUnbindContext = NULL;
    pNew->bClosingAdapter = FALSE;
    
    // Open the adapter.
    NdisOpenAdapter(&Status, &OpenStatus, &pNew->hAdapter, &SelectedMediumIndex,
                    &MediaArray[0], COUNTOF(MediaArray), v_PROTHandle, (NDIS_HANDLE)pNew, pAdapterString, 0, NULL);
    
    DEBUGMSG (ZONE_INIT, (TEXT("PPP: NdisOpenAdapter returned 0x%X, 0x%X\r\n"), Status, OpenStatus));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG (ZONE_ERROR && Status != NDIS_STATUS_UNSUPPORTED_MEDIA, (TEXT("PPP: -AddAdapter:NdisOpenAdapter(%s) failed with 0x%X\n"),
                                pAdapterString->Buffer, Status));
        // we need to clean up
        pppFreeMemory(pNew->szAdapterName, cAdapterStr);
        pppFreeMemory(pNew, sizeof (NDISWAN_ADAPTER));

        return Status;
    }

    //
    // Add the adapter to our global list of adapters
    //
    EnterCriticalSection (&v_AdapterCS);
    // Just insert at head...
    pNew->pNext = v_AdapterList;
    v_AdapterList = pNew;
    pNew->dwRefCnt = 1; // Initial reference count
    LeaveCriticalSection (&v_AdapterCS);

    // Do the provider initialize stuff
    NdisTapiAdapterInitialize(pNew);

    DEBUGMSG (ZONE_MAC, (TEXT("PPP: AddAdapter: Added '%s'\n"), pNew->szAdapterName));

    return NDIS_STATUS_SUCCESS;
}


BOOL
AdapterAddRef (
    IN  OUT PNDISWAN_ADAPTER    pAdapter)
{
    PNDISWAN_ADAPTER    pTemp;
    BOOL                fRetVal = FALSE;

    EnterCriticalSection (&v_AdapterCS);
    // Search for the adapter
    for (pTemp = v_AdapterList; NULL != pTemp; pTemp = pTemp->pNext)
    {
        if (pTemp == pAdapter)
        {
            fRetVal = TRUE;
            // Increment the reference count.
            pAdapter->dwRefCnt++;
            break;
        }
    }
    LeaveCriticalSection (&v_AdapterCS);
    return fRetVal;
}

extern void PROTCAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status);

BOOL
AdapterDelRef (
    IN  OUT PNDISWAN_ADAPTER pAdapter)
//
//  Delete a reference to the adapter.
//  If reference count goes to 0, initiate the closing of the adapter binding.
//  If pAdapter is invalid, return FALSE. (Should not happen)
//
{
    PNDISWAN_ADAPTER    pTemp, pLast;
    NDIS_STATUS         Status;
    BOOL                bFound = FALSE;
    
    EnterCriticalSection (&v_AdapterCS);
    // Search for the adapter
    pLast = NULL;
    for (pTemp = v_AdapterList; NULL != pTemp; pTemp = pTemp->pNext)
    {
        if (pTemp == pAdapter)
        {
            bFound = TRUE;
            // Decrement the reference count.
            pAdapter->dwRefCnt--;
            if (0 == pAdapter->dwRefCnt)
            {
                DEBUGMSG(ZONE_MAC, (L"PPP: Closing adapter=%s %x since refcnt is 0\n", pAdapter->szAdapterName, pAdapter));

                if (pLast) {
                    pLast->pNext = pAdapter->pNext;
                } else {
                    v_AdapterList = pAdapter->pNext;
                }

                NdisCloseAdapter(&Status, pAdapter->hAdapter);

                //
                // As soon as a protocol calls NdisCloseAdapter,
                // the handle at NdisBindingHandle should be considered invalid by the caller.
                // It is a programming error to pass this handle in any subsequent call to an NdisXxx function
                //

                // The ProtocolCloseAdapterComplete function is a required driver
                // function that completes processing for an unbinding operation
                // for which NdisCloseAdapter returned NDIS_STATUS_PENDING. 

                if (Status != NDIS_STATUS_PENDING)
                {

                    //
                    //  NdisCloseAdapter completed synchronously, invoke the completion
                    //  handler directly.
                    //
                    PROTCAComplete(pAdapter, Status);
                }
            }
            break;
        }
        pLast = pTemp;
    }
    LeaveCriticalSection (&v_AdapterCS);
    return bFound;
}

static VOID
AdapterAddRefLocked(
    IN  OUT PNDISWAN_ADAPTER    pAdapter)
//
//  This function is called to add a reference to an
//  adapter known to be on the v_AdapterList while
//  the v_AdapterCS is held.
//
{
    pAdapter->dwRefCnt++;
}

static VOID
AdapterDelRefLocked(
    IN  OUT PNDISWAN_ADAPTER    pAdapter)
//
//  This function is called to delete a reference to an
//  adapter known to be on the v_AdapterList while
//  the v_AdapterCS is held.
//
{
    if (pAdapter->dwRefCnt > 1)
        pAdapter->dwRefCnt--;
    else
        AdapterDelRef(pAdapter);
}

typedef struct _tagEnumDevicesInfo
{
    LPRASDEVINFOW   pRasDevInfo;
    DWORD           cbMax;              // Initial buffer size
    DWORD           cb;                 // End or required size
    DWORD           cDevices;           // Number of devices returned
    DWORD           dwRetVal;
    DWORD           dwSizeDevInfo;      // Size of RASDEVINFO structure
} ENUMDEVICESINFO, *PENUMDEVICESINFO;

typedef BOOL (WINAPI *PFN_DEVWALK) (PNDISWAN_ADAPTER pAdapter, DWORD dwDevID,
                                    PVOID pContext, LPCTSTR szDeviceName, LPCTSTR szDeviceType);
typedef struct _tagWalkDevicesInfo 
{
    PFN_DEVWALK pfnDevWalk;
    BOOL        bOnlyWalkNullModem;
}  WALKDEVICESINFO, *PWALKDEVICESINFO;

BOOL
WalkDevEnum (
    IN      PNDISWAN_ADAPTER pAdapter,
    IN      DWORD dwDevID,
    IN  OUT PVOID pContext,
    IN      LPCTSTR szDeviceName,
    IN      LPCTSTR szDeviceType)
//
//  This function is called by WalkDevices to build the list of device
//  info when we are enumerating all the devices available to RAS.
//
{
    PENUMDEVICESINFO pEnumDevInfo = (PENUMDEVICESINFO)pContext;
    LPRASDEVINFOW    pRasDevInfo;
    DWORD            dwElemSize;

    dwElemSize = pEnumDevInfo->dwSizeDevInfo;
    ASSERT(dwElemSize == sizeof(RASDEVINFOW) || dwElemSize == sizeof(RASDEVINFOW_V3));

    pRasDevInfo = (LPRASDEVINFOW)((PBYTE)(pEnumDevInfo->pRasDevInfo) + pEnumDevInfo->cb);
    pEnumDevInfo->cb += dwElemSize;
    
    // Room?
    if (pEnumDevInfo->cb <= pEnumDevInfo->cbMax)
    {
        pRasDevInfo->dwSize = dwElemSize;

        if (dwElemSize == sizeof(RASDEVINFOW_V3))
        {
            LPRASDEVINFOW_V3 pRasDevInfoV3;

            pRasDevInfoV3 = (LPRASDEVINFOW_V3)pRasDevInfo;

            _tcsncpy(pRasDevInfoV3->szDeviceType,szDeviceType, RAS_MaxDeviceType);
            pRasDevInfoV3->szDeviceType[RAS_MaxDeviceType] = TEXT('\0');

            _tcsncpy(pRasDevInfoV3->szDeviceName, szDeviceName, RAS_MaxDeviceName_V3);
            pRasDevInfoV3->szDeviceName[RAS_MaxDeviceName_V3] = TEXT('\0');
        }
        else
        {
            _tcsncpy(pRasDevInfo->szDeviceType,szDeviceType, RAS_MaxDeviceType);
            pRasDevInfo->szDeviceType[RAS_MaxDeviceType] = TEXT('\0');

            _tcsncpy(pRasDevInfo->szDeviceName, szDeviceName, RAS_MaxDeviceName);
            pRasDevInfo->szDeviceName[RAS_MaxDeviceName] = TEXT('\0');
        }
                
        DEBUGMSG (ZONE_MAC, (TEXT("WalkDevEnum: %d '%s' '%s'\r\n"),
                      pEnumDevInfo->cDevices,
                      pRasDevInfo->szDeviceName,
                      pRasDevInfo->szDeviceType));
                      
        pEnumDevInfo->cDevices++;
    }

    // Return FALSE to keep walking since we need to enumerate all devices
    return FALSE;
}


BOOL
WalkDevices (
    PNDISWAN_ADAPTER pAdapter,
    PVOID            pContext1,
    PVOID            pContext2)
//
//  Call a function for each device that the adapter has.
//
{
    DWORD                   i;
    PWALKDEVICESINFO        pWalkDevicesInfo = (PWALKDEVICESINFO)pContext1;
    PFN_DEVWALK             pfnDevWalk = pWalkDevicesInfo->pfnDevWalk;
    PNDISTAPI_DEVICE_INFO   pDevInfo;
    BOOL                    bStopWalking = FALSE;

    for (i=0; i < pAdapter->dwNumDevices; i++)
    {
        pDevInfo = &pAdapter->pDeviceInfo[i];

        //
        //  Stop enumerating if we encounter an uninitialized device.
        //  That is, we may be in the process of gathering info on the device
        //  via an OID_TAPI_GET_DEV_CAPS request. Until that request completes,
        //  the device is not enumerable.
        //
        if (NULL == pDevInfo->pwszDeviceName
        ||  NULL == pDevInfo->pwszDeviceType)
        {
            break;
        }

        //  If we only want a null modem device, ignore others
        //
        if (FALSE == pWalkDevicesInfo->bOnlyWalkNullModem
        ||  TRUE  == pDevInfo->bIsNullModem)
        {
            DEBUGMSG (ZONE_MAC, (TEXT("WalkEnumDev: Adpt '%s' Dev %d Name=%s Type=%s\r\n"),
                                    pAdapter->szAdapterName, i, pDevInfo->pwszDeviceName, pDevInfo->pwszDeviceType));

            bStopWalking = pfnDevWalk(pAdapter, i, pContext2, pDevInfo->pwszDeviceName, pDevInfo->pwszDeviceType);
            if (bStopWalking)
                break;
        }
    }
    
    return bStopWalking;
}

DWORD
WalkAdapters(
    IN  PFN_ADPTWALK pfnAdapterWalk,
    IN  PVOID        pContext1,
    IN  PVOID        pContext2)
//
//  Iterate through the list of NDIS WAN miniport drivers registered with PPP,
//  calling pfnAdapterWalk on each one until either it returns TRUE or the
//  end of the list is reached.
//
{
    PNDISWAN_ADAPTER    pAdapter;
    BOOL                bStopWalking;
    DWORD               Result = ERROR_SUCCESS;
    
    // For each adapter, call the pfnAdapterWalk function.
    EnterCriticalSection(&v_AdapterCS);
    for (pAdapter = v_AdapterList; NULL != pAdapter; pAdapter = pAdapter->pNext)
    {
        //
        // The v_AdapterCS MUST be released by pfnAdapterWalk as appropriate if
        // it would block.
        // Otherwise, pfnAdapterWalk may block on something and create a potential
        // deadlock scenario if some other thread needs v_AdapterCS.
        //
        // To prevent pAdapter from being deleted while we are using it,
        // we take a reference to it.
        //
        AdapterAddRefLocked(pAdapter);

        __try
        {
            // Call the specified function
            bStopWalking = pfnAdapterWalk (pAdapter, pContext1, pContext2);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            Result = ERROR_INVALID_PARAMETER;
            bStopWalking = TRUE;
        }

        AdapterDelRefLocked(pAdapter);

        if (bStopWalking)
        {
            // pfnAdapterWalk requested loop termination
            break;
        }
    }
    LeaveCriticalSection(&v_AdapterCS);

    return Result;
}

DWORD
pppMac_EnumDevices (
    IN  OUT LPRASDEVINFOW   pRasDevInfo, OPTIONAL
    IN      BOOL            bOnlyWalkNullModem,
    IN  OUT LPDWORD         lpcb,
        OUT LPDWORD         lpcDevices)
//
//  Return information on all the RAS devices.
//
{
    ENUMDEVICESINFO EnumDevInfo;
    WALKDEVICESINFO WalkDevInfo;

    DEBUGMSG(ZONE_RAS, (L"PPP: +pppMAC_EnumDevices( 0x%08X, 0x%08X, 0x%08X )\n", pRasDevInfo, lpcb, lpcDevices));

    __try
    {
        //
        //  Validate input parameters
        //
        EnumDevInfo.dwRetVal = NO_ERROR;
        if (lpcb == NULL || lpcDevices == NULL)
            EnumDevInfo.dwRetVal = ERROR_INVALID_PARAMETER;
        else
        {
            //
            //  The application indicates which version of the RASDEVINFOW
            //  structure it is using by setting pRasDevInfo->dwSize.
            //
            //  If the app does not provide a pRasDevInfo structure from
            //  which to get the size, default to the larger structure.
            //  This will cause an old app to allocate more memory than
            //  required, but its subsequent call should work ok.
            //
            EnumDevInfo.dwSizeDevInfo = sizeof(RASDEVINFOW);
            EnumDevInfo.pRasDevInfo   = pRasDevInfo;
            EnumDevInfo.cbMax         = 0;

            if (pRasDevInfo)
            {
                EnumDevInfo.cbMax         = *lpcb;
                if (EnumDevInfo.cbMax < sizeof(DWORD))
                    EnumDevInfo.dwRetVal = ERROR_BUFFER_TOO_SMALL;
                else if (pRasDevInfo->dwSize < sizeof(RASDEVINFOW_V3))
                    EnumDevInfo.dwRetVal = ERROR_BUFFER_TOO_SMALL;
                else if (pRasDevInfo->dwSize == sizeof(RASDEVINFOW_V3))
                    EnumDevInfo.dwSizeDevInfo = sizeof(RASDEVINFOW_V3);
                else if (pRasDevInfo->dwSize < sizeof(RASDEVINFOW))
                    EnumDevInfo.dwRetVal = ERROR_BUFFER_TOO_SMALL;
                else
                    EnumDevInfo.dwSizeDevInfo = sizeof(RASDEVINFOW);
            }
        }

        if (EnumDevInfo.dwRetVal == NO_ERROR)
        {
            DWORD WalkResult;

            EnumDevInfo.cb          = 0;
            EnumDevInfo.cDevices    = 0;
            EnumDevInfo.dwRetVal    = SUCCESS;

            WalkDevInfo.pfnDevWalk         = WalkDevEnum;
            WalkDevInfo.bOnlyWalkNullModem = bOnlyWalkNullModem;

            WalkResult = WalkAdapters(WalkDevices, &WalkDevInfo, &EnumDevInfo);
            if (ERROR_SUCCESS != WalkResult)
            {
                EnumDevInfo.dwRetVal = WalkResult;
            }
            else
            {
                DEBUGMSG(ZONE_MAC, (L"pppMAC_EnumDevices: cb=%d cDevices=%d\r\n", EnumDevInfo.cb, EnumDevInfo.cDevices));

                // Copy the result data back.
                *lpcb = EnumDevInfo.cb;
                *lpcDevices = EnumDevInfo.cDevices;
                if (EnumDevInfo.cb > EnumDevInfo.cbMax)
                {
                    EnumDevInfo.dwRetVal = ERROR_BUFFER_TOO_SMALL;
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        EnumDevInfo.dwRetVal = ERROR_INVALID_PARAMETER;
    }

    DEBUGMSG(ZONE_RAS | (ZONE_ERROR && EnumDevInfo.dwRetVal), (L"PPP: -pppMAC_EnumDevices: Return Value %d\n", EnumDevInfo.dwRetVal));
    return EnumDevInfo.dwRetVal;
}

typedef struct _tagFindAdapter
{
    LPCTSTR szDeviceName;
    LPCTSTR szDeviceType;
    PNDISWAN_ADAPTER    pAdapter;
    DWORD               dwDeviceID;
    DWORD   dwRetVal;
} FINDADAPTER, *PFINDADAPTER;

BOOL
WalkDevFindDevice(
    PNDISWAN_ADAPTER        pAdapter,
    DWORD                   dwDevID,
    PVOID                   pContext,
    LPCTSTR                 szDeviceName,
    LPCTSTR                 szDeviceType)
//
//  This function is called by WalkDevices to see if a device is the
//  one we are looking for.
//
//  Returns TRUE if this device matches, which terminates the walk.
//
{
    PFINDADAPTER    pFindAdapter = (PFINDADAPTER)pContext;
    BOOL            bStopWalking = FALSE;
    
    if (0 == _tcsncmp(pFindAdapter->szDeviceName, szDeviceName, RAS_MaxDeviceName) &&
        0 == _tcscmp(pFindAdapter->szDeviceType, szDeviceType))
    {
        pFindAdapter->pAdapter = pAdapter;
        pFindAdapter->dwDeviceID = dwDevID;
        pFindAdapter->dwRetVal = SUCCESS;
        AdapterAddRef (pAdapter);
        // Stop the search....
        bStopWalking = TRUE;
    }
    return bStopWalking;
}

BOOL
FindAdapter (
    IN      LPCTSTR             szDeviceName,
    IN      LPCTSTR             szDeviceType,
        OUT PNDISWAN_ADAPTER    *ppAdapter,
        OUT LPDWORD             pdwDevID)
//
//  Return value:
//      TRUE    if successful (adapter found)
//      FALSE   if the adapter is not found
//
//  NOTE: If successful, a reference is added to the adapter returned
//        in *ppAdapter.  The caller must delete this reference when
//        done using the returned pointer by calling AdapterDelRef.
//
{
    FINDADAPTER FindAdaptInfo;
    WALKDEVICESINFO WalkDevInfo;

    // Setup the param struct
    FindAdaptInfo.szDeviceName = szDeviceName;
    FindAdaptInfo.szDeviceType = szDeviceType;
    FindAdaptInfo.pAdapter = NULL;
    FindAdaptInfo.dwDeviceID = 0;
    FindAdaptInfo.dwRetVal = ERROR_DEVICENAME_NOT_FOUND;
    
    WalkDevInfo.pfnDevWalk         = WalkDevFindDevice;
    WalkDevInfo.bOnlyWalkNullModem = FALSE;

    WalkAdapters(WalkDevices, &WalkDevInfo, &FindAdaptInfo);

    if (SUCCESS == FindAdaptInfo.dwRetVal)
    {
        *ppAdapter = FindAdaptInfo.pAdapter;
        *pdwDevID = FindAdaptInfo.dwDeviceID;
    }

    return FindAdaptInfo.dwRetVal;
}
