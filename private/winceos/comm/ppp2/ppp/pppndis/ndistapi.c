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
*   ndistapi.c  Ndis TAPI utilities
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
#include "unimodem.h"
#include <intsafe.h>

void
ndisTapiProcessDevCaps(
    PNDISWAN_ADAPTER        pAdapter,
    PNDIS_TAPI_GET_DEV_CAPS pGetDevCaps)
//
//  This function is called after the adapter has successfully returned
//  DevCaps information. We process the returned info and add it to the
//  pDeviceInfo array for the adapter.
//
{
    DWORD                 dwDeviceID;
    LPTSTR                szDeviceName,
                          szDeviceType,
                          szMediaName,
                          szProviderName;
    USHORT                wDeviceType;
    PNDISTAPI_DEVICE_INFO pInfo;
    UNIMODEM_INFO         *pUnimodemInfo;
    BOOL                  bIsNullModem = FALSE;

    dwDeviceID = pGetDevCaps->ulDeviceID;

    DEBUGMSG(ZONE_MAC, (L"PPP: ProcessDevCaps %s::%u\n", pAdapter->szAdapterName, dwDeviceID));

    // We should never have issued a request for a deviceID that is too big
    if (dwDeviceID >= pAdapter->dwNumDevices)
    {
        RETAILMSG(1, (L"PPP: ndisTapiProcessDevCaps ERROR invalid dwDeviceID %u >= %u\n", dwDeviceID, pAdapter->dwNumDevices));
        ASSERT(FALSE);
    }
    else
    {   
        pInfo = &pAdapter->pDeviceInfo[dwDeviceID];

        // Clear existing device info, if any
        LocalFree(pInfo->pwszDeviceName);
        memset(pInfo, 0, sizeof(pInfo));

        szDeviceName = (LPTSTR)((LPBYTE)&(pGetDevCaps->LineDevCaps) + pGetDevCaps->LineDevCaps.ulLineNameOffset);

        //
        //  If we are unable to otherwise determine a device type,
        //  assume it is a "modem" device.
        //
        szDeviceType = RASDT_Modem;
        wDeviceType = 0xFFFF;

        // UNIMODEM and PPTP put TSP name in the ProviderInfo area
        if (pGetDevCaps->LineDevCaps.ulProviderInfoOffset)
        {
            //
            //  ProviderInfo is of the format:
            //      MediaName\0ProviderName\0
            //
            szMediaName = (PTSTR)((PBYTE)&(pGetDevCaps->LineDevCaps)+ pGetDevCaps->LineDevCaps.ulProviderInfoOffset);
            szProviderName = szMediaName + _tcslen(szMediaName) + 1;
            if (szProviderName > (PTSTR)((PBYTE)szMediaName + pGetDevCaps->LineDevCaps.ulProviderInfoSize))
                szProviderName = NULL;

            if (_tcscmp(TEXT("UNIMODEM"), szMediaName) == 0)
            {
                // UNIMODEM device
                if (pGetDevCaps->LineDevCaps.ulDevSpecificOffset)
                {
                    pUnimodemInfo = (UNIMODEM_INFO *)((LPBYTE)&(pGetDevCaps->LineDevCaps) + pGetDevCaps->LineDevCaps.ulDevSpecificOffset);
                    wDeviceType = pUnimodemInfo->wDeviceType;
                    switch (pUnimodemInfo->wDeviceType)
                    {
                    case DT_NULL_MODEM :
                        bIsNullModem = TRUE;
                        // FALLTHROUGH
                    case DT_IRCOMM_MODEM :
                    case DT_DYNAMIC_PORT :
                        szDeviceType = RASDT_Direct;
                        break;
                        
                    case DT_EXTERNAL_MODEM:
                    case DT_INTERNAL_MODEM:
                    case DT_PARALLEL_PORT:
                    case DT_PARALLEL_MODEM:
                    case DT_DYNAMIC_MODEM :
                    default :
                        szDeviceType = RASDT_Modem;
                        break;
                    }
                }
            }
            else if (_tcsicmp(RASDT_Direct, szMediaName) == 0)
            {
                szDeviceType = RASDT_Direct;
            }
            else if (_tcsicmp(RASDT_Modem, szMediaName) == 0)
            {
                szDeviceType = RASDT_Modem;
            }
            else if (_tcsicmp(RASDT_Vpn, szMediaName) == 0)
            {
                szDeviceType = RASDT_Vpn;
                if (szProviderName && _tcsicmp(szProviderName, RASDT_PPPoE) == 0)
                {
                    szDeviceType = RASDT_PPPoE;
                }
            }
        }
        pInfo->bIsNullModem = bIsNullModem;
        pInfo->pwszDeviceName = LocalAlloc(LPTR, (wcslen(szDeviceName) + 1) * sizeof(WCHAR));
        if (pInfo->pwszDeviceName)
        {
            wcscpy(pInfo->pwszDeviceName, szDeviceName);
            pInfo->pwszDeviceType = szDeviceType;
        }
    }
}

void
ndisTapiLineGetDevCapsCompleteCallback(
    PNDIS_REQUEST_BETTER    pRequest,
    PNDISWAN_ADAPTER        pAdapter,
    NDIS_STATUS             Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_GET_DEV_CAPS request.
//
{
    PNDIS_TAPI_GET_DEV_CAPS pGetDevCaps;
    DWORD                   dwDeviceID;
    DWORD                   dwNeededSize,
                            dwExtraSize;

    DEBUGMSG(ZONE_FUNCTION, (L"PPP: +ndisTapiLineGetDevCapsCompleteCallback Status=%x\n", Status));

    pAdapter->bEnumeratingDevices = FALSE;

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR, (L"PPP: ndisTapiLineGetDevCapsCompleteCallback got Status = %x for adapter %s\n", Status, pAdapter->szAdapterName));
    }
    else
    {
        if (pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength < sizeof(NDIS_TAPI_GET_DEV_CAPS))
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ndisTapiGetLinkInfoCompleteCallback BufferLength too small (%u < %u)\n",
                pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength, sizeof(NDIS_TAPI_GET_DEV_CAPS)));
        }
        else
        {
            pGetDevCaps = pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer;
            dwDeviceID = pGetDevCaps->ulDeviceID;
            dwNeededSize = pGetDevCaps->LineDevCaps.ulNeededSize;

            // Check to see if we need a bigger buffer
            if (dwNeededSize <= pGetDevCaps->LineDevCaps.ulTotalSize)
            {
                // Buffer was big enough to receive the required info
                // Extract the returned device information and add it to the adapter
                ndisTapiProcessDevCaps(pAdapter, pGetDevCaps);

                // Advance deviceID to the next device to be enumerated for the adapter
                dwDeviceID++;
                dwExtraSize = 0;
            }
            else
            {
                dwExtraSize = dwNeededSize - sizeof(LINE_DEV_CAPS);
            }

            // Issue a new request if more devices remain to be enumerated, or to retry
            // a request with bigger buffer.
            if (dwDeviceID < pAdapter->dwNumDevices)
            {
                NdisTapiIssueLineGetDevCaps(pAdapter, dwDeviceID, dwExtraSize);
            }
            else
            {
                // No more devices remain to be enumerated
                DEBUGMSG(ZONE_MAC, (L"PPP: Completed device enumeration for adapter %s\n", pAdapter->szAdapterName));
            }
        }
    }

    // Free the memory allocated for the request.
    LocalFree(pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -ndisTapiLineGetDevCapsCompleteCallback\n")));
}

NDIS_STATUS
NdisTapiIssueLineGetDevCaps(
    PNDISWAN_ADAPTER pAdapter,
    DWORD            dwDeviceID,
    DWORD            dwExtraSize)
//
//  Send an asynchronous OID_TAPI_GET_DEV_CAPS request to the miniport adapter
//  to retrieve device information for the specified dwDeviceID.
//
{
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    PNDIS_TAPI_GET_DEV_CAPS pGetDevCaps;

    if (dwExtraSize > 0xFFFFFF)
    {
        // Unreasonably large
        ASSERT(FALSE);
        Status = NDIS_STATUS_INVALID_LENGTH;
        goto done;
    }

    //
    // If we already have an OID_TAPI_GET_DEV_CAPS pending, we don't
    // need to issue a new one. When the current one completes it will
    // continue to enumerate all the devices for the adapter.
    //
    if (!pAdapter->bEnumeratingDevices)
    {
        DWORD dwGetDevCapsSize = sizeof(NDIS_TAPI_GET_DEV_CAPS) + dwExtraSize;

        pGetDevCaps = LocalAlloc(LPTR, dwGetDevCapsSize);
        if (NULL == pGetDevCaps)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - Unable to allocate %u bytes for NDIS_TAPI_GET_DEV_CAPS\n", dwGetDevCapsSize));
            Status = NDIS_STATUS_RESOURCES;
            goto done;
        }

        DEBUGMSG(ZONE_MAC, (L"PPP: Send OID_TAPI_GET_DEV_CAPS for %s::%u\n", pAdapter->szAdapterName, dwDeviceID));

        pAdapter->bEnumeratingDevices = TRUE;

        pGetDevCaps->LineDevCaps.ulTotalSize = dwGetDevCapsSize - offsetof(NDIS_TAPI_GET_DEV_CAPS, LineDevCaps);
        pGetDevCaps->ulRequestID = 0;
        pGetDevCaps->ulDeviceID = dwDeviceID;
        pGetDevCaps->ulExtVersion = 0;

        PppNdisIssueRequest(&Status, 
                            pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_GET_DEV_CAPS,
                            pGetDevCaps,
                            dwGetDevCapsSize,
                            NULL,
                            ndisTapiLineGetDevCapsCompleteCallback,
                            pAdapter,
                            NULL);
    }

done:
    return Status;
}

//
// To reduce the necessity of having to send 2 OID_TAPI_GET_DEV_CAPS because
// the first one doesn't have enough room for extra info, we send this many
// bytes of extra space in the initial request.
//
#define DEFAULT_GETDEVCAPS_EXTRA_BYTES  256

NDIS_STATUS
NdisTapiSetNumLineDevs(
    PNDISWAN_ADAPTER        pAdapter,
    DWORD                   dwNumLineDevs)
//
//  This function is called to set the number of line devices owned by the adapter.
//  It is called when the OID_TAPI_PROVIDER_INITIALIZE completes to set the initial
//  number of devices, and later when a LINE_CREATE indication is received from the
//  miniport adapter to indicate that new devices have been added (e.g. attaching
//  a removable modem card).
//
{
    PNDISTAPI_DEVICE_INFO pDevInfoNew = NULL;
    NDIS_STATUS           Status = NDIS_STATUS_SUCCESS;
    DWORD                 dwFirstNewDevice;

    DEBUGMSG(ZONE_MAC, (TEXT("PPP: NdisTapiSetNumLineDevs for adapter %s to %u\n"), pAdapter->szAdapterName, pAdapter->dwNumDevices));

    EnterCriticalSection(&v_AdapterCS);

    dwFirstNewDevice = pAdapter->dwNumDevices;
    if (dwNumLineDevs <= dwFirstNewDevice)
    {
        DEBUGMSG(dwNumLineDevs && ZONE_WARN, (TEXT("PPP: WARNING New numLineDevs %u <= current %u for adapter %s\n"), dwNumLineDevs, dwFirstNewDevice, pAdapter->szAdapterName));
    }
    else
    {
        // Allocate memory for larger device info array.
        pDevInfoNew = pppAllocateMemory(dwNumLineDevs * sizeof(*pAdapter->pDeviceInfo));
        if (pDevInfoNew == NULL)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR Unable to allocate memory for %u LineDevs for adapter %s\n"), dwNumLineDevs, pAdapter->szAdapterName));
            Status = NDIS_STATUS_RESOURCES;
        }
        else
        {
            // Copy old array to new
            memcpy(pDevInfoNew, pAdapter->pDeviceInfo, dwFirstNewDevice * sizeof(*pAdapter->pDeviceInfo));

            // Free the old array, switch adapter to the new
            pppFreeMemory(pAdapter->pDeviceInfo, dwFirstNewDevice * sizeof(*pAdapter->pDeviceInfo));
            pAdapter->pDeviceInfo = pDevInfoNew;

            pAdapter->dwNumDevices = dwNumLineDevs;

        }
    }
    LeaveCriticalSection(&v_AdapterCS);

    if (pDevInfoNew)
    {
        // Send requests to enumerate the new line devices to get name and type information
        Status = NdisTapiIssueLineGetDevCaps(pAdapter, dwFirstNewDevice, DEFAULT_GETDEVCAPS_EXTRA_BYTES);
    }
    return Status;
}

void
ndisTapiProviderInitializeCompleteCallback(
    PNDIS_REQUEST_BETTER    pRequest,
    PNDISWAN_ADAPTER        pAdapter,
    NDIS_STATUS             Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_PROVIDER_INITIALIZE request.
//
{
    PNDIS_TAPI_PROVIDER_INITIALIZE  pProvInit;

    DEBUGMSG(ZONE_FUNCTION, (L"PPP: +ndisTapiProviderInitializeCompleteCallback Status=%x\n", Status));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR, (L"PPP: ndisTapiProviderInitializeCompleteCallback got Status = %x for adapter %s\n", Status, pAdapter->szAdapterName));
    }
    else
    {
        if (pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength < sizeof(*pProvInit))
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ndisTapiGetLinkInfoCompleteCallback BufferLength too small (%u < %u)\n",
                pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength, sizeof(*pProvInit)));
        }
        else
        {
            pProvInit = pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer;

            // Extract the returned device information and add it to the adapter
            DEBUGMSG (ZONE_MAC, (TEXT("PPP: AddAdapter: '%s' returned ProvID=%d NumDev=%d\n"),
                pAdapter->szAdapterName, pAdapter->dwProviderID, pAdapter->dwNumDevices));
            
            pAdapter->dwProviderID = pProvInit->ulProviderID;
            NdisTapiSetNumLineDevs(pAdapter, pProvInit->ulNumLineDevs);
        }
    }

    if (Status != NDIS_STATUS_SUCCESS)
        AdapterDelRef(pAdapter);

    // Free the memory allocated for the request.
    LocalFree(pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -ndisTapiProviderInitializeCompleteCallback\n")));
}

void
NdisTapiAdapterInitialize(
    PNDISWAN_ADAPTER    pAdapter)
//
//  This function is called after we have successfully bound to a new
//  WAN miniport adapter. It gathers information from the adapter to
//  complete initialization.
//
{
    NDIS_STATUS                     Status;
    PNDIS_TAPI_PROVIDER_INITIALIZE  pProvInit;

    pProvInit = LocalAlloc (LPTR, sizeof(*pProvInit));
    if (NULL != pProvInit)
    {
        // pProvInit->ulDeviceIDBase = 0;

        PppNdisIssueRequest(&Status, 
                            pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_PROVIDER_INITIALIZE,
                            pProvInit,
                            sizeof(*pProvInit),
                            NULL,
                            ndisTapiProviderInitializeCompleteCallback,
                            pAdapter,
                            NULL);

    }
}

/*****************************************************************************
*
*
*   @func   LPTSTR  |   TapiLineTranslateAddress |
*           Get the Dialable or Displayable format of a number
*
*   @rdesc  Returns a LPTSTR or NULL.  The caller must free this string.
*
*   @parm   DWORD   | dwDeviceID            |
*           The Device ID to use
*   @parm   DWORD   | dwCountryCode         |
*           The country code
*   @parm   LPCTSTR | szAreaCode            |
*           Optional Area code (can be null or null str)
*   @parm   LPCTSTR | szLocalPhoneNumber    |
*           The phone number.
*   @parm   BOOL    | fDialable             |
*           Bool designating whether you want the Dialable or the
*           Displayable string.
*   @parm   DWORD   | dwTranslateOpts       |
*           Translate options, Normally this will be zero or
*           LINETRANSLATEOPTION_FORCELD
*
*   @comm
*           This function will return either the Dialable or the DisplayAble
*           phone number (On Pegasus they are usually the same).
*
*   @ex     An example of how to use this function follows |
*           No Example
*
*/
LPTSTR
NdisTapiLineTranslateAddress (
    PNDISWAN_ADAPTER    pAdapter,
    DWORD               dwDeviceID,
    DWORD               dwCountryCode,
    LPCTSTR             szAreaCode,
    LPCTSTR             szLocalPhoneNumber,
    BOOL                fDialable,
    DWORD               dwFlags)
{
    LPTSTR          szTempStr;
    DWORD           dwLen;
    LPTSTR          szOutStr = NULL;
    LPTSTR          szPtr;
    DWORD           dwNeededSize;
    NDIS_STATUS     Status;
    PNDIS_TAPI_LINE_TRANSLATE   pLineTranslate;
    PLINE_TRANSLATE_OUTPUT      pLineTranslateOutput;
    
    DEBUGMSG (ZONE_FUNCTION,
              (TEXT("+NdisTapiLineTranslateAddress(%s DeviceId=%u CC=%u AreaCode=%s PN=%s)\n"),
               pAdapter->szAdapterName, dwDeviceID, dwCountryCode, szAreaCode, szLocalPhoneNumber));

    // Allocate a temp string, length of the strings + null, two spaces and
    // the country code

    dwLen = _tcslen(szAreaCode) + _tcslen(szLocalPhoneNumber) + 20;
    szTempStr = LocalAlloc (LPTR, dwLen * sizeof(TCHAR));

    if (NULL == szTempStr)
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("-NdisTapiLineTranslateAddress: Cannot allocate szTempStr\r\n")));
        return NULL;
    }

    //
    // Go with szTempStr as our default result in case we are unable to get
    // a translation from the miniport.
    //
    szOutStr = szTempStr;

    if (szAreaCode && (szAreaCode[0] != TEXT('\0'))) {
        StringCchPrintfW(szTempStr, dwLen, L"+%d (%s) %s", dwCountryCode, szAreaCode, szLocalPhoneNumber);
    } else {
        StringCchPrintfW(szTempStr, dwLen, L"+%d %s", dwCountryCode, szLocalPhoneNumber);
    }

#define DWORD_ROUND(x)  (((x)+3) & 0xFFFFFFFC)

    dwNeededSize = sizeof(NDIS_TAPI_LINE_TRANSLATE) + DWORD_ROUND(dwLen*sizeof(TCHAR)) +
                   sizeof(LINE_TRANSLATE_OUTPUT);
    
    do
    {
        pLineTranslate = LocalAlloc (LPTR, dwNeededSize);

        if (!pLineTranslate)
            break;

        pLineTranslate->ulRequestID = 0;
        pLineTranslate->ulDeviceID = dwDeviceID;
        pLineTranslate->ulAddressInLen = dwLen;
        pLineTranslate->ulCard = 0;
        pLineTranslate->ulTranslateOptions = dwFlags;

        // Copy the AddressIn into the structure.
        memcpy (pLineTranslate->DataBuf, (char *)szTempStr, dwLen*sizeof(TCHAR));

        pLineTranslate->ulLineTranslateOutputOffset = DWORD_ROUND(dwLen*sizeof(TCHAR));

        pLineTranslateOutput = (PLINE_TRANSLATE_OUTPUT) (pLineTranslate->DataBuf+pLineTranslate->ulLineTranslateOutputOffset);

        pLineTranslateOutput->ulTotalSize = dwNeededSize - ((char *)pLineTranslateOutput - (char *)pLineTranslate);

        PppNdisDoSyncRequest (&Status,
                                pAdapter,
                                NdisRequestQueryInformation,
                                OID_TAPI_TRANSLATE_ADDRESS,
                                pLineTranslate,
                                dwNeededSize);

        if (Status == NDIS_STATUS_SUCCESS)
        {
            // If we had enough space to get the whole translation, we are done
            if (pLineTranslateOutput->ulNeededSize <= pLineTranslateOutput->ulTotalSize)
            {
                // Success
                DEBUGMSG (ZONE_TRACE,
                          (TEXT("DialableString='%s' DisplayString='%s'\r\n"),
                           (LPBYTE)pLineTranslateOutput + pLineTranslateOutput->ulDialableStringOffset,
                           (LPBYTE)pLineTranslateOutput +
                           pLineTranslateOutput->ulDisplayableStringOffset));

                if (fDialable) {
                    szPtr = (LPTSTR) ((LPBYTE)pLineTranslateOutput +
                                      pLineTranslateOutput->ulDialableStringOffset);
                } else {
                    szPtr = (LPTSTR) ((LPBYTE)pLineTranslateOutput +
                                      pLineTranslateOutput->ulDisplayableStringOffset);
                }
                szOutStr = LocalAlloc (LPTR, sizeof(TCHAR)*(_tcslen(szPtr)+1));
                if (szOutStr)
                {
                    _tcscpy (szOutStr, szPtr);

                    LocalFree (szTempStr);
                }
                else
                {
                    szOutStr = szTempStr;
                }
                dwNeededSize = 0;
            }
            else
            {
                // Otherwise increment our needed size and try again
                dwNeededSize += pLineTranslateOutput->ulNeededSize - pLineTranslateOutput->ulTotalSize;
            }
        }

        LocalFree (pLineTranslate);

    } while (Status == NDIS_STATUS_SUCCESS && dwNeededSize);
    
    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineTranslateAddress: returning '%s'\r\n"), szOutStr));

    return szOutStr;
}

/*****************************************************************************
*
*
*   @func   LPTSTR  |   TapiLineConfigDialogEdit |
*           Let the user modify the Device config data
*
*   @rdesc  Returns a DWORD.
*
*   @parm   DWORD   | dwDeviceID            |
*           The Device ID to use
*
*   @comm
*
*   @ex     An example of how to use this function follows |
*           No Example
*
*/
DWORD
NdisTapiLineConfigDialogEdit(
                            PNDISWAN_ADAPTER pAdapter,
                            DWORD            dwDeviceID,
                            HWND             hwndOwner,
                            LPCTSTR          szDeviceClass,
    __in_bcount_opt(dwSize) LPVOID           pDeviceConfigIn,
                            DWORD            dwSize,
                            LPVARSTRING      pDeviceConfigOut)
{
    DWORD           dwDevClassLen;
    DWORD           dwNeededSize;
    NDIS_STATUS     Status;
    PNDIS_TAPI_LINE_CONFIG_DIALOG_EDIT  pConfigDialogEdit;
    DWORD           dwRetVal = STATUS_SUCCESS;
    DWORD           dwCurOffset;
    PVAR_STRING     pVarString;
    PBYTE           pLocalConfigIn = NULL;

    DEBUGMSG (ZONE_FUNCTION,
              (TEXT("+NdisTapiLineConfigDialogEdit (0x%X, 0x%X, 0x%X, 0x%X(%s),")
               TEXT(" 0x%X %d 0x%X)\r\n"), pAdapter, dwDeviceID, hwndOwner,
               szDeviceClass, (szDeviceClass) ? szDeviceClass : TEXT("NULL"),
               pDeviceConfigIn, dwSize, pDeviceConfigOut));

    if (dwSize > 0xFFFFFF)
    {
        // Unreasonably large device config
        return NDIS_STATUS_RESOURCES;
    }

    // We allow people to pass in NULL/0 for the original struct.  If they do we'll
    // get the default
    if (NULL == pDeviceConfigIn) {
        dwRetVal = NdisTapiGetDevConfig (pAdapter, dwDeviceID, &(LPBYTE)pLocalConfigIn,
                                         &dwSize);
        if (STATUS_SUCCESS != dwRetVal) {
            if (pLocalConfigIn) {
                LocalFree (pLocalConfigIn);
            }
            DEBUGMSG (ZONE_ERROR,
                      (TEXT("-NdisTapiLineConfigDialogEdit: Error from NdisTapiGetDevConfig '%d'\r\n"),
                       dwRetVal));
            return dwRetVal;
        }
        pDeviceConfigIn = pLocalConfigIn;
    }

    if (szDeviceClass) {
        dwDevClassLen = _tcslen (szDeviceClass);
    } else {
        dwDevClassLen = 0;
    }

    if (dwDevClassLen > 0xFFFFF
    ||  dwSize > 0xFFFFF
    ||  pDeviceConfigOut->dwTotalSize > 0xFFFFF)
    {
        DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - Size for ConfigDialogEdit unreasonably large.\n"));
        pConfigDialogEdit = NULL;
    }
    else
    {
        dwNeededSize = sizeof(NDIS_TAPI_LINE_CONFIG_DIALOG_EDIT) +
                   DWORD_ROUND(dwDevClassLen) + DWORD_ROUND(dwSize) +
                   pDeviceConfigOut->dwTotalSize;
        pConfigDialogEdit = LocalAlloc (LPTR, dwNeededSize);
    }

    if (NULL == pConfigDialogEdit) {
        DEBUGMSG (ZONE_ERROR,
                  (TEXT("-NdisTapiLineConfigDialogEdit: Cannot allocate pConfigDialogEdit, returning default string\r\n")));
        if (pLocalConfigIn) {
            LocalFree (pLocalConfigIn);
        }
        return dwRetVal;
    }
    pConfigDialogEdit->ulRequestID = 0;
    pConfigDialogEdit->ulDeviceID = dwDeviceID;
    pConfigDialogEdit->hwndOwner = hwndOwner;
    pConfigDialogEdit->ulDeviceClassLen = dwDevClassLen;
    if (dwDevClassLen) {
        memcpy (pConfigDialogEdit->DataBuf, (char *)szDeviceClass,
                sizeof(TCHAR)*(dwDevClassLen+1));
        dwCurOffset = DWORD_ROUND(sizeof(TCHAR)*(dwDevClassLen+1));
    } else {
        dwCurOffset = 0;
    }
    pConfigDialogEdit->ulConfigInOffset = dwCurOffset;
    pConfigDialogEdit->ulConfigInSize = dwSize;
    memcpy (pConfigDialogEdit->DataBuf + pConfigDialogEdit->ulConfigInOffset,
            pDeviceConfigIn, dwSize);
    dwCurOffset = DWORD_ROUND(dwCurOffset + dwSize);
    pConfigDialogEdit->ulConfigOutOffset = dwCurOffset;
    pVarString = (PVAR_STRING) (pConfigDialogEdit->DataBuf +
                                pConfigDialogEdit->ulConfigOutOffset);
    memcpy ((char *)pVarString, (char *)pDeviceConfigOut, pDeviceConfigOut->dwTotalSize);

    PppNdisDoSyncRequest (&Status,
                            pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_CONFIG_DIALOG_EDIT,
                            pConfigDialogEdit,
                            dwNeededSize);

    if (Status != NDIS_STATUS_SUCCESS) {
        DEBUGMSG (ZONE_ERROR,
                  (TEXT("-NdisTapiLineConfigDialogEdit: NdisRequest returned 0x%X\r\n"),
                   Status));
        LocalFree (pConfigDialogEdit);
            
        if (pLocalConfigIn) {
            LocalFree (pLocalConfigIn);
        }
        return dwRetVal = Status;
    } else {
        // Copy the result back
        memcpy ((char *)pDeviceConfigOut, (char *)pVarString, pVarString->ulTotalSize);
    }

    if (pConfigDialogEdit) {
        LocalFree (pConfigDialogEdit);
    }
    if (pLocalConfigIn) {
        LocalFree (pLocalConfigIn);
    }
    
    DEBUGMSG (ZONE_FUNCTION, (TEXT("-NdisTapiLineConfigDialogEdit: Success, returning '%d'\r\n"), dwRetVal));

    return dwRetVal;
}

NDIS_STATUS
NdisTapiGetDevConfig(
    PNDISWAN_ADAPTER    pAdapter,
    DWORD               dwDeviceID,
    LPBYTE              *plpbDevConfig,
    DWORD               *pdwSize)
{
    PNDIS_TAPI_GET_DEV_CONFIG   pTapiGetDevConfig;
    DWORD                       dwDevConfigSize;
    DWORD                       dwDeviceClassSize = 0;
    LPCSTR                      lpszDeviceClass = NULL;
    NDIS_STATUS                 Status;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+NdisTapiGetDevConfig\n")));

    dwDevConfigSize = sizeof(VARSTRING);

    if (lpszDeviceClass)
        dwDeviceClassSize  = strlen(lpszDeviceClass) + 1;

    // Don't know the size required for the dev config going in, so we
    // need to iterate, incrementing the dev config size on each iteration.

    do
    {
        Status = NDIS_STATUS_RESOURCES;
        pTapiGetDevConfig = (PNDIS_TAPI_GET_DEV_CONFIG)LocalAlloc(LPTR, sizeof(*pTapiGetDevConfig) + dwDevConfigSize + dwDeviceClassSize);
        if (pTapiGetDevConfig)
        {
            //
            //  Build the request to send to the adapter
            //
            pTapiGetDevConfig->ulRequestID = 0;
            pTapiGetDevConfig->ulDeviceID = dwDeviceID;
            pTapiGetDevConfig->ulDeviceClassSize = dwDeviceClassSize;
            pTapiGetDevConfig->ulDeviceClassOffset = offsetof(NDIS_TAPI_GET_DEV_CONFIG, DeviceConfig) + dwDevConfigSize;
            if (lpszDeviceClass)
                memcpy((LPBYTE)pTapiGetDevConfig + pTapiGetDevConfig->ulDeviceClassOffset, lpszDeviceClass, dwDeviceClassSize);
            pTapiGetDevConfig->DeviceConfig.ulTotalSize = dwDevConfigSize;

            PppNdisDoSyncRequest (&Status,
                                    pAdapter,
                                    NdisRequestQueryInformation,
                                    OID_TAPI_GET_DEV_CONFIG,
                                    pTapiGetDevConfig,
                                    sizeof(*pTapiGetDevConfig) + dwDevConfigSize + dwDeviceClassSize);

            if (Status == NDIS_STATUS_SUCCESS)
            {
                PVAR_STRING pVStr = &pTapiGetDevConfig->DeviceConfig;

                if (pVStr->ulNeededSize <= pVStr->ulTotalSize)
                {
                    // The buffer was big enough to get the whole dev config
                    
                    *plpbDevConfig = LocalAlloc(LPTR, pTapiGetDevConfig->DeviceConfig.ulStringSize);
                    if (*plpbDevConfig)
                    {
                        *pdwSize = pTapiGetDevConfig->DeviceConfig.ulStringSize;
                        memcpy(*plpbDevConfig, ((LPBYTE)&pTapiGetDevConfig->DeviceConfig) + pTapiGetDevConfig->DeviceConfig.ulStringOffset, *pdwSize);
                    }
                    else
                    {
                        DEBUGMSG (ZONE_ERROR, (TEXT("!NdisTapiGetDevConfig: Unable to allocate %d bytes of memory for devconfig\n"), pTapiGetDevConfig->DeviceConfig.ulStringSize));
                        Status = NDIS_STATUS_RESOURCES;
                    }
                    LocalFree(pTapiGetDevConfig);
                    break;
                }

                // Dev config structure was not big enough.  Increase size and try again
                dwDevConfigSize = pVStr->ulNeededSize;
            }

            LocalFree(pTapiGetDevConfig);
        }
    } while (Status == NDIS_STATUS_SUCCESS);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-NdisTapiGetDevConfig: Status=%x\n"), Status));

    return Status;
}

NDIS_STATUS
NdisTapiSetDevConfig(macCntxt_t *pMac)
{
    PNDIS_TAPI_SET_DEV_CONFIG   pTapiSetDevConfig;
    ULONG                       cbTapiSetDevConfig;
    LPBYTE                      lpbDevConfig;
    DWORD                       dwDevConfigSize;
    NDIS_STATUS                 Status = NDIS_STATUS_RESOURCES;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+NdisTapiSetDevConfig\n")));

    lpbDevConfig = ((pppSession_t *)(pMac->session))->lpbDevConfig;
    dwDevConfigSize = ((pppSession_t *)(pMac->session))->dwDevConfigSize;

    pTapiSetDevConfig = NULL;
    if (SUCCEEDED(ULongAdd(sizeof(*pTapiSetDevConfig), dwDevConfigSize, &cbTapiSetDevConfig)))
        pTapiSetDevConfig = (PNDIS_TAPI_SET_DEV_CONFIG)LocalAlloc(LPTR, cbTapiSetDevConfig);

    DEBUGMSG(ZONE_ERROR && NULL == pTapiSetDevConfig, (L"PPP: ERROR - Unable to allocate NDIS_TAPI_SET_DEV_CONFIG.\n"));

    if (pTapiSetDevConfig)
    {
        pTapiSetDevConfig->ulRequestID = 0;
        pTapiSetDevConfig->ulDeviceID = pMac->dwDeviceID;
        pTapiSetDevConfig->ulDeviceClassSize = 0;
        pTapiSetDevConfig->ulDeviceClassOffset = 0;
        pTapiSetDevConfig->ulDeviceConfigSize = dwDevConfigSize;
        memcpy(&pTapiSetDevConfig->DeviceConfig[0], lpbDevConfig, dwDevConfigSize);

        pppUnLock(pMac->session);
        PppNdisDoSyncRequest (&Status,
                                pMac->pAdapter,
                                NdisRequestSetInformation,
                                OID_TAPI_SET_DEV_CONFIG,
                                pTapiSetDevConfig,
                                sizeof(*pTapiSetDevConfig) + dwDevConfigSize);
        pppLock(pMac->session);

        LocalFree(pTapiSetDevConfig);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-NdisTapiSetDevConfig: Status=%x\n"), Status));
    return Status;
}

NDIS_STATUS
NdisTapiLineOpen(
    macCntxt_t *pMac)
{
    NDIS_TAPI_OPEN  TapiOpen;
    NDIS_STATUS     Status;

    TapiOpen.ulRequestID = ((pppSession_t *)(pMac->session))->bIsServer;
    TapiOpen.ulDeviceID = pMac->dwDeviceID;
    TapiOpen.htLine = (HTAPI_LINE)pMac;

    pMac->bLineOpenInProgress = TRUE;
    pppUnLock(pMac->session);
    PppNdisDoSyncRequest (&Status,
                            pMac->pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_OPEN,
                            &TapiOpen,
                            sizeof(TapiOpen));
    pppLock(pMac->session);
    pMac->bLineOpenInProgress = FALSE;
    SetEvent(pMac->hEventLineOpenComplete);
    
    if (Status == NDIS_STATUS_SUCCESS)
    {
        // Save the context value
        pMac->hLine = TapiOpen.hdLine;
        pMac->bLineClosed = FALSE;
    }

    return Status;
}

NDIS_STATUS
NdisTapiSetDefaultMediaDetection(
    IN  macCntxt_t *pMac,
    IN  DWORD       ulMediaModes)
//
//  Tell the miniport to listen for incoming connections
//
{
    NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION   SetMediaDetect;
    NDIS_STATUS                             Status;

    SetMediaDetect.ulRequestID = 0;
    SetMediaDetect.hdLine = pMac->hLine;
    SetMediaDetect.ulMediaModes = ulMediaModes;

    pppUnLock(pMac->session);
    PppNdisDoSyncRequest (&Status,
                            pMac->pAdapter,
                            NdisRequestSetInformation,
                            OID_TAPI_SET_DEFAULT_MEDIA_DETECTION,
                            &SetMediaDetect,
                            sizeof(SetMediaDetect));
    pppLock(pMac->session);
    
    return Status;
}

NDIS_STATUS
NdisTapiLineMakeCall(
    macCntxt_t *pMac,
    LPCTSTR szDialStr)
//
//  Request the miniport to place a call on the specified line to the specified
//  destination address.
//
{
    PNDIS_TAPI_MAKE_CALL    pTapiMakeCall;
    NDIS_STATUS             Status = NDIS_STATUS_RESOURCES;
    DWORD                   dwNeededSize;
    LPBYTE                  lpbDevConfig;
    DWORD                   dwDevConfigSize;
    DWORD                   ccDialStr;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("ASYNCMAC: +NdisTapiLineMakeCall: dialStr=%s\n"), szDialStr));

    lpbDevConfig = ((pppSession_t *)(pMac->session))->lpbDevConfig;
    dwDevConfigSize = ((pppSession_t *)(pMac->session))->dwDevConfigSize;

    ccDialStr = _tcslen(szDialStr);
    if (ccDialStr > 0xFFFFFF || dwDevConfigSize > 0xFFFFFF)
    {
        // DialStr or DevConfig is unreasonably long
        goto done;
    }
        
    dwNeededSize = sizeof(NDIS_TAPI_MAKE_CALL) + (ccDialStr+1) * sizeof(TCHAR);
    if (lpbDevConfig) {
        dwNeededSize += dwDevConfigSize;
    }

    pTapiMakeCall = LocalAlloc (LPTR, dwNeededSize);

    if (pTapiMakeCall)
    {
        pTapiMakeCall->ulRequestID = 0;
        pTapiMakeCall->hdLine = pMac->hLine;
        pTapiMakeCall->htCall = (HTAPI_CALL)pMac;   // Do I need a call structure?
        pTapiMakeCall->bUseDefaultLineCallParams = TRUE;
        pTapiMakeCall->ulDestAddressSize = (_tcslen(szDialStr)+1)*sizeof(TCHAR);
        
        if (lpbDevConfig) {
            //
            // Pass the RASENTRY associated DEVCONFIG in LINECALLPARAMS.
            //
            pTapiMakeCall->bUseDefaultLineCallParams = FALSE;

            pTapiMakeCall->LineCallParams.ulTotalSize = sizeof(LINE_CALL_PARAMS) + dwDevConfigSize;
            pTapiMakeCall->LineCallParams.ulBearerMode = LINEBEARERMODE_DATA;
            pTapiMakeCall->LineCallParams.ulMinRate = 0;   // Any rate
            pTapiMakeCall->LineCallParams.ulMaxRate = 0;   // This should mean any max rate
            pTapiMakeCall->LineCallParams.ulMediaMode = LINEMEDIAMODE_DATAMODEM;
            pTapiMakeCall->LineCallParams.ulCallParamFlags = LINECALLPARAMFLAGS_IDLE;
            pTapiMakeCall->LineCallParams.ulAddressMode = LINEADDRESSMODE_ADDRESSID;
            pTapiMakeCall->LineCallParams.ulAddressID = 0;  // there's only one address
            pTapiMakeCall->LineCallParams.ulDeviceConfigSize = dwDevConfigSize;

            //
            // Making the assumption that ulDeviceConfigOffset is the offset from the 
            // beginning of LINE_CALL_PARAMS even though LINE_CALL_PARAMS is embedded
            // in an NDIS_TAPI_MAKE_CALL structure.
            //
            pTapiMakeCall->LineCallParams.ulDeviceConfigOffset = sizeof(LINE_CALL_PARAMS);
            memcpy((PBYTE)&pTapiMakeCall->LineCallParams + pTapiMakeCall->LineCallParams.ulDeviceConfigOffset,
                   lpbDevConfig,
                   dwDevConfigSize);

            //
            // Place the phone number at the very end of the NDIS_TAPI_MAKE_CALL allocation
            //
            pTapiMakeCall->ulDestAddressOffset = sizeof(NDIS_TAPI_MAKE_CALL) + dwDevConfigSize;
        } else {
            pTapiMakeCall->bUseDefaultLineCallParams = TRUE;
            pTapiMakeCall->ulDestAddressOffset = sizeof(NDIS_TAPI_MAKE_CALL);
        }
        memcpy((PBYTE)pTapiMakeCall + pTapiMakeCall->ulDestAddressOffset, szDialStr, pTapiMakeCall->ulDestAddressSize);

        pppUnLock(pMac->session);
        PppNdisDoSyncRequest (&Status,
                                pMac->pAdapter,
                                NdisRequestQueryInformation,
                                OID_TAPI_MAKE_CALL,
                                pTapiMakeCall,
                                dwNeededSize);
        pppLock(pMac->session);

        DEBUGMSG (ZONE_ERROR && Status != NDIS_STATUS_SUCCESS,
                  (TEXT("!NdisTapiLineMakeCall: NdisRequest returned 0x%X\r\n"),
                   Status));

        if (Status == NDIS_STATUS_SUCCESS)
        {
            pMac->hCall = pTapiMakeCall->hdCall;
            pMac->bCallClosed = FALSE;
        }
    
        LocalFree (pTapiMakeCall);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("ASYNCMAC: -NdisTapiLineMakeCall: Status=%x\n"), Status));
    
done:
    return Status;
}

NDIS_STATUS
NdisTapiGetCallInfo(
    IN  macCntxt_t               *pMac,
    OUT PNDIS_TAPI_GET_CALL_INFO  pGetCallInfo)
//
//  Request the miniport to place a call on the specified line to the specified
//  destination address.
//
{
    NDIS_STATUS             Status = NDIS_STATUS_RESOURCES;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("ASYNCMAC: +NdisTapiGetCallInfo:\n")));

    pGetCallInfo->ulRequestID = 0;
    pGetCallInfo->hdCall = pMac->hCall;

    PppNdisDoSyncRequest (&Status,
                            pMac->pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_GET_CALL_INFO,
                            pGetCallInfo,
                            sizeof(*pGetCallInfo));

    DEBUGMSG (ZONE_ERROR && Status != NDIS_STATUS_SUCCESS,
                  (TEXT("!NdisTapiGetCallInfo: NdisRequest returned 0x%X\r\n"),
                   Status));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("ASYNCMAC: -NdisTapiGetCallInfo: Status=%x\n"), Status));
    
    return Status;

}

PNDIS_TAPI_GET_ID
createGetDeviceIDRequest(
    IN  macCntxt_t  *pMac,
    IN  ULONG       ulSelect,
    IN  PTSTR       tszDeviceClass,
    IN  DWORD       cbDeviceIDString,
    OUT PDWORD      pdwRequestSize)
{

    PNDIS_TAPI_GET_ID   pTapiGetId;
    DWORD               cbDeviceClass;

    //
    //  The format of the request issued looks like:
    //
    //  ULONG       ulRequestID
    //  HDRV_LINE   hdLine
    //  ULONG       ulAddressID
    //  HDRV_CALL   hdCall
    //  ULONG       ulSelect
    //  ULONG       ulDeviceClassSize
    //  ULONG       ulDeviceClassOffset
    //  VAR_STRING  DeviceID
    //  [cbDeviceIdString bytes for DeviceID string]
    //  TCHAR       device class [ulDeviceClassSize bytes, null terminated]

    cbDeviceClass = (_tcslen(tszDeviceClass) + 1) * sizeof(TCHAR);

    pTapiGetId = NULL;
    if (SUCCEEDED(CeULongAdd3(sizeof(NDIS_TAPI_GET_ID), cbDeviceIDString, cbDeviceClass, pdwRequestSize)))
        pTapiGetId = pppAllocateMemory(*pdwRequestSize);

    DEBUGMSG(ZONE_ERROR && pTapiGetId == NULL, (L"PPP: ERROR - Unable to allocate NDIS_TAPI_GET_ID\n"));

    if (pTapiGetId)
    {
        pTapiGetId->ulRequestID = 0;
        pTapiGetId->hdLine = pMac->hLine;
        pTapiGetId->ulAddressID = (ULONG)pMac;
        pTapiGetId->hdCall = pMac->hCall;
        pTapiGetId->ulSelect = ulSelect;
        pTapiGetId->ulDeviceClassSize = cbDeviceClass;
        pTapiGetId->ulDeviceClassOffset = sizeof(NDIS_TAPI_GET_ID) + cbDeviceIDString;
        pTapiGetId->DeviceID.ulTotalSize = sizeof(VAR_STRING) + cbDeviceIDString;
        memcpy((PBYTE)pTapiGetId + pTapiGetId->ulDeviceClassOffset, (PBYTE)tszDeviceClass, cbDeviceClass);
    }

    return pTapiGetId;
}

#define TAPI_DEVICECLASS_NAME       "tapi/line"

void
NdisTapiLineGetIdCompleteCallback(
    NDIS_REQUEST_BETTER *pRequest,
    PVOID                FuncArg,
    NDIS_STATUS          Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_GET_ID request.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiLineGetIdCompleteCallback Status=%x\n"), Status));

    if (Status == NDIS_STATUS_SUCCESS) 
    {
        // copy the returned device id, currently have no use for it

        // XXX BUGBUG TBD
    }

    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, pRequest->Request.DATA.SET_INFORMATION.InformationBufferLength);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineGetIdCompleteCallback\n")));
}

NDIS_STATUS
NdisTapiGetDeviceIdAsync(
    IN  macCntxt_t  *pMac,
    IN  ULONG        ulSelect,
    IN  PTSTR        tszDeviceClass)
{
    PNDIS_TAPI_GET_ID   pTapiGetId;
    DWORD               dwRequestSize;
    NDIS_STATUS         Status = NDIS_STATUS_RESOURCES;

    pTapiGetId = createGetDeviceIDRequest(pMac, ulSelect, tszDeviceClass, 0, &dwRequestSize);

    if (pTapiGetId)
    {
        pppUnLock(pMac->session);
        PppNdisIssueRequest(&Status, 
                            pMac->pAdapter,
                            NdisRequestQueryInformation,
                            OID_TAPI_GET_ID,
                            pTapiGetId,
                            dwRequestSize,
                            pMac,
                            NdisTapiLineGetIdCompleteCallback,
                            NULL,
                            NULL);
        pppLock(pMac->session);
    }

    return Status;
}

NDIS_STATUS
NdisTapiGetDeviceId(
    IN  macCntxt_t  *pMac,
    IN  ULONG        ulSelect,
    IN  PTSTR        tszDeviceClass,
    OUT PVAR_STRING *pDeviceID OPTIONAL)
//
//  Do a synchronous request to retrieve the device ID.
//
{
    PNDIS_TAPI_GET_ID   pTapiGetId;
    NDIS_STATUS         Status;
    DWORD               dwRequestSize;
    DWORD               cbDeviceIDString;
    BOOL                bDone = FALSE;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +NdisTapiGetDeviceId\n")));

    cbDeviceIDString = 0;
    
    //
    //  Because the requested information is variable sized, we
    //  need to iterate until we pass a buffer sufficiently large
    //  to accomodate it.
    //
    do
    {
        pTapiGetId = createGetDeviceIDRequest(pMac, ulSelect, tszDeviceClass, cbDeviceIDString, &dwRequestSize);

        if (pTapiGetId == NULL)
        {
            Status = NDIS_STATUS_RESOURCES;
            break;
        }

        pppUnLock(pMac->session);
        PppNdisDoSyncRequest(&Status,
                                pMac->pAdapter,
                                NdisRequestQueryInformation,
                                OID_TAPI_GET_ID,
                                pTapiGetId,
                                dwRequestSize);
        pppLock(pMac->session);


        if (Status != NDIS_STATUS_SUCCESS)
        {
            bDone = TRUE;
        }
        else
        {
            if (pTapiGetId->DeviceID.ulNeededSize <= pTapiGetId->DeviceID.ulTotalSize)
            {
                bDone = TRUE;
                if (pDeviceID)
                {
                    // Allocate and copy the deviceID

                    *pDeviceID = pppAllocateMemory(pTapiGetId->DeviceID.ulTotalSize);
                    if (*pDeviceID == NULL)
                    {
                        Status = NDIS_STATUS_RESOURCES;
                    }
                    else
                    {
                        memcpy(*pDeviceID, &pTapiGetId->DeviceID, pTapiGetId->DeviceID.ulTotalSize);
                    }
                }
            }
            else // Need a bigger buffer
            {
                ASSERT(pTapiGetId->DeviceID.ulNeededSize > sizeof(VAR_STRING));
                cbDeviceIDString = pTapiGetId->DeviceID.ulNeededSize - sizeof(VAR_STRING);
            }
        }
        pppFreeMemory(pTapiGetId, dwRequestSize);
    } while (!bDone);

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -NdisTapiGetDeviceId: Status=%x\n"), Status));

    return Status;
}

void
NdisTapiLineAnswerCompleteCallback(
    NDIS_REQUEST_BETTER *pRequest,
    PVOID                FuncArg,
    NDIS_STATUS          Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_CLOSE request that returned NDIS_STATUS_PENDING.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiLineAnswerCompleteCallback Status=%x\n"), Status));

    // Free the memory allocated for the NDIS_TAPI_ANSWER.
    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, pRequest->Request.DATA.SET_INFORMATION.InformationBufferLength);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineAnswerCompleteCallback\n")));
}

NDIS_STATUS
NdisTapiLineAnswer(
    PVOID       pvMac,
    PUCHAR      pUserUserInfo,
    ULONG       ulUserUserInfoSize)
//
//  Used to answer an incoming call that we have been offered via the
//  LINE_NEWCALL indication from the miniport.
//
{
    macCntxt_t              *pMac = pvMac;
    PNDIS_TAPI_ANSWER       pTapiAnswer;
    NDIS_STATUS             Status;
    DWORD                   dwNeededSize;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiLineAnswer\n")));

    PREFAST_ASSERT(ulUserUserInfoSize < 0xFFFFFF);

    Status = NDIS_STATUS_RESOURCES;
    dwNeededSize = sizeof(NDIS_TAPI_ANSWER) + ulUserUserInfoSize;
    pTapiAnswer = pppAllocateMemory(dwNeededSize);
    if (pTapiAnswer)
    {
        //
        // Note that the driver set pMac->hCall upon reception of the
        // LINE_NEWCALL indication.
        //
        DEBUGMSG(ZONE_TRACE, (TEXT("NdisTapiLineAnswer: hdCall=%x\n"), pMac->hCall));
        pTapiAnswer->hdCall = pMac->hCall;
        pTapiAnswer->ulUserUserInfoSize = ulUserUserInfoSize;
        memcpy(&pTapiAnswer->UserUserInfo[0], pUserUserInfo, ulUserUserInfoSize);

        pppUnLock(pMac->session);
        PppNdisIssueRequest(&Status, 
                            pMac->pAdapter,
                            NdisRequestSetInformation,
                            OID_TAPI_ANSWER,
                            pTapiAnswer,
                            dwNeededSize,
                            pMac,
                            NdisTapiLineAnswerCompleteCallback,
                            NULL,
                            NULL);
        pppLock(pMac->session);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineAnswer Status=%x\n"), Status));

    return Status;
}

void
NdisTapiCallCloseCompleteCallback(
    NDIS_REQUEST_BETTER *pRequest,
    macCntxt_t          *pMac,
    NDIS_STATUS          Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_CLOSE_CALL request that returned NDIS_STATUS_PENDING.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiCallCloseCompleteCallback Status=%x\n"), Status));

    pMac->bCallClosed = TRUE;
    pMac->bCallCloseRequested = FALSE;

    // Issue a final request to obtain call statistics
    pppMacIssueWanGetStatsInfo(pMac);

    if (Status == NDIS_STATUS_SUCCESS)
    {
        //
        // Call has been deallocated, no longer valid
        //
        (NDIS_HANDLE)pMac->hCall = INVALID_HANDLE_VALUE;
    }

    //
    // Complete pending call closes
    //
    pppExecuteCompleteCallbacks(&pMac->pPendingCallCloseCompleteList);
    
    // Free the memory allocated for the request.
    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, sizeof(NDIS_TAPI_CLOSE_CALL));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiCallCloseCompleteCallback\n")));
}

NDIS_STATUS
NdisTapiCallClose(
    macCntxt_t      *pMac,
    void            (*pCallCloseCompleteCallback)(PVOID), OPTIONAL
    PVOID           pCallbackData)
//
//  Issue a request to the miniport to close the line.
//
{
    NDIS_TAPI_CLOSE_CALL    *pTapiCloseCall;
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    NDIS_HANDLE             hCall = (NDIS_HANDLE)pMac->hCall;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiCallClose\n")));

    if (pMac->bCallClosed)
    {
        //  The call is already closed, just invoke the completion handler

        if (pCallCloseCompleteCallback)
            pCallCloseCompleteCallback(pCallbackData);
    }
    else
    {
        if (pMac->bCallCloseRequested)
        {
            //
            // A close request is already pending, No need to issue another call close request
            // to the miniport.
            //
            pppInsertCompleteCallbackRequest(&pMac->pPendingCallCloseCompleteList, pCallCloseCompleteCallback, pCallbackData);
        }
        else
        {
            //
            //  Issue a new call close request
            //
        
            Status = NDIS_STATUS_RESOURCES;

            pTapiCloseCall = pppAllocateMemory(sizeof(*pTapiCloseCall));
            if (pTapiCloseCall)
            {
                memset ((char *)pTapiCloseCall, 0, sizeof(*pTapiCloseCall));
                pTapiCloseCall->hdCall = (ULONG)hCall;

                pppInsertCompleteCallbackRequest(&pMac->pPendingCallCloseCompleteList, pCallCloseCompleteCallback, pCallbackData);
                pMac->bCallCloseRequested = TRUE;
                pppUnLock(pMac->session);
                PppNdisIssueRequest(&Status, 
                                    pMac->pAdapter,
                                    NdisRequestSetInformation,
                                    OID_TAPI_CLOSE_CALL,
                                    pTapiCloseCall,
                                    sizeof(*pTapiCloseCall),
                                    pMac,
                                    NdisTapiCallCloseCompleteCallback,
                                    pMac,
                                    NULL);
                pppLock(pMac->session);
            }
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiCallClose Status=%x\n"), Status));

    return Status;
}

void
NdisTapiCallDropCompleteCallback(
    NDIS_REQUEST_BETTER *pRequest,
    macCntxt_t          *pMac,
    NDIS_STATUS          Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_CLOSE request that returned NDIS_STATUS_PENDING.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiCallDropCompleteCallback\n")));

    // Note that the call is not dropped until the LINECALLSTATE_IDLE state
    // or LINECALLSTATE_DISCONNECTED is indicated.  It is at that point when pending CallDrop requests are
    // completed.

    // Free the memory allocated for the request.
    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, sizeof(NDIS_TAPI_DROP));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiCallDropCompleteCallback\n")));
}

NDIS_STATUS
NdisTapiCallDrop(
    macCntxt_t      *pMac,
    void            (*pCallDropCompleteCallback)(PVOID), OPTIONAL
    PVOID           pCallbackData)
//
//  Issue a request to the miniport to drop the call
//
{
    NDIS_TAPI_DROP          *pTapiDrop;
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    NDIS_HANDLE             hCall = (NDIS_HANDLE)pMac->hCall;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiCallDrop\n")));

    if (pMac->dwLineCallState == LINECALLSTATE_IDLE
    ||  pMac->dwLineCallState == LINECALLSTATE_DISCONNECTED
    ||  pMac->bLineCloseRequested 
    ||  pMac->bLineClosed)
    {
        // Call already dropped, just call completion handler, if any
        if (pCallDropCompleteCallback)
            pCallDropCompleteCallback(pCallbackData);
    }
    else if (pMac->pPendingCallDropCompleteList)
    {
        //
        // A drop request is already pending, just add this to the
        // list of pending drops.  No need to issue another call drop request
        // to the miniport.
        //
        pppInsertCompleteCallbackRequest(&pMac->pPendingCallDropCompleteList, pCallDropCompleteCallback, pCallbackData);
    }
    else
    {
        //
        // Issue a call drop request and call the completion handler when the
        // line enters LINECALLSTATE_IDLE.
        //
        pppInsertCompleteCallbackRequest(&pMac->pPendingCallDropCompleteList, pCallDropCompleteCallback, pCallbackData);

        Status = NDIS_STATUS_RESOURCES;
        pTapiDrop = pppAllocateMemory(sizeof(*pTapiDrop));
        if (pTapiDrop)
        {
            memset ((char *)pTapiDrop, 0, sizeof(*pTapiDrop));
            pTapiDrop->hdCall = (ULONG)hCall;

            pppUnLock(pMac->session);
            PppNdisIssueRequest(&Status, 
                                pMac->pAdapter,
                                NdisRequestSetInformation,
                                OID_TAPI_DROP,
                                pTapiDrop,
                                sizeof(*pTapiDrop),
                                pMac,
                                NdisTapiCallDropCompleteCallback,
                                pMac,
                                NULL);
            pppLock(pMac->session);
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiCallDrop Status=%x\n"), Status));

    return Status;
}

typedef struct
{
    macCntxt_t  *pMac;
    void        (*pCompleteCallback)(PVOID);
    PVOID       pCallbackData;
} HangupCompleteInfo_t;

static void
NdisTapiHangupCallDropComplete(
    PVOID   *pData)
//
//  Called when the call is in the IDLE state
//
{
    NDIS_STATUS Status;

    HangupCompleteInfo_t *pInfo = (HangupCompleteInfo_t *)pData;

    Status = NdisTapiCallClose(pInfo->pMac, pInfo->pCompleteCallback, pInfo->pCallbackData);

    pppFreeMemory(pInfo, sizeof(*pInfo));
}

NDIS_STATUS
NdisTapiHangup(
    macCntxt_t *pMac,
    void        (*pHangupCompleteCallback)(PVOID),
    PVOID       pCallbackData)
//
//  Drop and deallocate the call.
//  After the call is deallocated, call pHangupCompleteCallback.
//
{
    NDIS_STATUS             Status = NDIS_STATUS_RESOURCES;
    HangupCompleteInfo_t    *pInfo;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiHangup\n")));

    pInfo = pppAllocateMemory(sizeof(*pInfo));
    if (pInfo)
    {
        pInfo->pMac              = pMac;
        pInfo->pCompleteCallback = pHangupCompleteCallback;
        pInfo->pCallbackData     = pCallbackData;
        Status = NdisTapiCallDrop(pMac, NdisTapiHangupCallDropComplete, pInfo);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiHangup Status=%x\n"), Status));

    return Status;
}

void
NdisTapiLineCloseCompleteCallback(
    NDIS_REQUEST_BETTER *pRequest,
    macCntxt_t          *pMac,
    NDIS_STATUS          Status)
//
//  This function is called when the miniport driver completes
//  an OID_TAPI_CLOSE request that returned NDIS_STATUS_PENDING.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiLineCloseCompleteCallback\n")));

    pMac->bLineClosed = TRUE;
    pMac->bLineCloseRequested = FALSE;

    //
    // It seems that if a lineDrop request is issued shortly
    // after a lineMakeCall, TAPI can fail to indicate a LINECALLSTATE_IDLE ever.
    // After the line is closed, TAPI will not issue any LINECALLSTATE_xxx indications
    // on that line any more, so if a call drop request is still pending we complete it
    // now.
    // 
    pppExecuteCompleteCallbacks(&pMac->pPendingCallDropCompleteList);

    // Complete any pending close requests
    pppExecuteCompleteCallbacks(&pMac->pPendingLineCloseCompleteList);

    // Free the memory allocated for the request.
    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, sizeof(NDIS_TAPI_CLOSE));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineCloseCompleteCallback\n")));
}

void
closeCallBeforeClosingLineCompleteCallback(
    macCntxt_t *pMac)
//
// This is called when a CallClose request, implicitly issued due to a LineClose, completes.
// Now that the Call is closed we can proceed to close the line.
//
{
    // Note that the line close complete handler was already queued in the prior
    // NdisTapiLineClose call.
    NdisTapiLineClose(pMac, NULL, NULL);
}

NDIS_STATUS
NdisTapiLineClose(
    macCntxt_t *pMac,
    void       (*pLineCloseCompleteCallback)(PVOID), OPTIONAL
    PVOID      pCallbackData)
//
//  Issue a request to the miniport to close the line.
//
{
    NDIS_TAPI_CLOSE         *pTapiClose = NULL;
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +NdisTapiLineClose\n")));

    //
    // Wait for any pending OID_TAPI_OPEN request to complete prior
    // to closing.
    //
    if (pMac->bLineOpenInProgress)
    {
        pppUnLock(pMac->session);
        WaitForSingleObject(pMac->hEventLineOpenComplete, INFINITE);
        pppLock(pMac->session);
    }

    if (pMac->bLineClosed)
    {
        //
        // Line is already closed, just call the completion handler
        //
        if (pLineCloseCompleteCallback)
            pLineCloseCompleteCallback(pCallbackData);
    }
    else if (pMac->bLineCloseRequested)
    {
        //
        // A close request is already pending, just add this to the
        // list of pending closes.  No need to issue another line close request
        // to the miniport.
        //
        pppInsertCompleteCallbackRequest(&pMac->pPendingLineCloseCompleteList, pLineCloseCompleteCallback, pCallbackData);
    }
    else if (!pMac->bCallClosed)
    {
        //
        // Ensure that the call on the line is closed prior to closing the line.
        //
        pppInsertCompleteCallbackRequest(&pMac->pPendingLineCloseCompleteList, pLineCloseCompleteCallback, pCallbackData);
        Status = NdisTapiCallClose(pMac, closeCallBeforeClosingLineCompleteCallback, pMac);
    }
    else
    {
        //
        //  Initiate a new line close request
        //
        Status = NDIS_STATUS_RESOURCES;
        pTapiClose = pppAllocateMemory(sizeof(*pTapiClose));
        if (pTapiClose)
        {
            memset ((char *)pTapiClose, 0, sizeof(*pTapiClose));
            pTapiClose->hdLine = (ULONG)pMac->hLine;
            (NDIS_HANDLE)pMac->hLine = INVALID_HANDLE_VALUE;

            pppInsertCompleteCallbackRequest(&pMac->pPendingLineCloseCompleteList, pLineCloseCompleteCallback, pCallbackData);

            pMac->bLineCloseRequested = TRUE;

            pppUnLock(pMac->session);
            PppNdisIssueRequest(&Status, 
                                pMac->pAdapter,
                                NdisRequestSetInformation,
                                OID_TAPI_CLOSE,
                                pTapiClose,
                                sizeof(*pTapiClose),
                                pMac,
                                NdisTapiLineCloseCompleteCallback,
                                pMac,
                                NULL);              
            pppLock(pMac->session);
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -NdisTapiLineClose Status=%x\n"), Status));

    return Status;
}
