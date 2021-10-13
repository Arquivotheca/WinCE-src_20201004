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
*   mac.c   Mac Layer interface
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

#define TAPI_DEVICECLASS_NAME       TEXT("tapi/line")

// ----------------------------------------------------------------
//
//  Global Data
//
// ----------------------------------------------------------------
NDIS_HANDLE v_PROTHandle;      // Our NDIS protocol handle.
HANDLE              v_hRequestEvent;
CRITICAL_SECTION    v_RequestCS;
NDIS_STATUS         v_RequestStatus;
PNDISWAN_ADAPTER    v_AdapterList;
CRITICAL_SECTION    v_AdapterCS;

DWORD
pppMac_NdisToRasErrorCode(
    DWORD   dwNdisErrorCode)
//
//  Translate an NDIS error code returned by the miniport driver
//  into a RAS error code.
//
{
    DWORD   dwRasErrorCode;

    switch(dwNdisErrorCode)
    {
        case NDIS_STATUS_SUCCESS:
            dwRasErrorCode = ERROR_SUCCESS;
            break;

        case NDIS_STATUS_CLOSED:
            dwRasErrorCode = ERROR_PORT_NOT_OPEN;
            break;

        case NDIS_STATUS_TAPI_CALLUNAVAIL:
        case NDIS_STATUS_TAPI_RESOURCEUNAVAIL:
            dwRasErrorCode = ERROR_PORT_NOT_AVAILABLE;
            break;

        case NDIS_STATUS_FAILURE:
        case NDIS_STATUS_INVALID_OID:
            dwRasErrorCode = ERROR_UNKNOWN;
            break;

        case NDIS_STATUS_TAPI_INVALPARAM:
            dwRasErrorCode = ERROR_WRONG_INFO_SPECIFIED;
            break;

        case NDIS_STATUS_TAPI_NODEVICE:
            // BUGBUG: Should this be ERROR_DEVICE_DOES_NOT_EXIST?
            dwRasErrorCode = ERROR_DEVICE_NOT_READY;
            break;

        case NDIS_STATUS_TAPI_NODRIVER:
            dwRasErrorCode = ERROR_DEVICE_NOT_READY;
            break;

        case NDIS_STATUS_TAPI_OPERATIONUNAVAIL:
            dwRasErrorCode = ERROR_DEVICE_NOT_READY;
            break;

        case NDIS_STATUS_RESOURCES:
            dwRasErrorCode = ERROR_NOT_ENOUGH_MEMORY;
            break;

        case NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION:
            dwRasErrorCode = ERROR_UNKNOWN;
            break;

        case NDIS_STATUS_TAPI_INVALLINESTATE:
            dwRasErrorCode = ERROR_PORT_NOT_CONFIGURED;
            break;

        case NDIS_STATUS_TAPI_INUSE:
            dwRasErrorCode = ERROR_PORT_ALREADY_OPEN;
            break;

        default:
            //
            // All error types that the miniport returns should be
            // covered explicitly.  If any are not, they will be caught here
            // and can have a case added.
            //
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: pppMac_NdisToRasErrorCode doesn't know error %x\n"), dwNdisErrorCode));
            dwRasErrorCode = ERROR_UNKNOWN;
            break;
    }

    return dwRasErrorCode;
}

DWORD
pppMac_Initialize()
{
    DWORD   dwRetVal;
    
    DEBUGMSG (ZONE_TRACE, (TEXT("+pppMac_Initialize\r\n")));

    v_hRequestEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    InitializeCriticalSection (&v_RequestCS);
    InitializeCriticalSection (&v_AdapterCS);

    if (dwRetVal = DoNDISRegisterProtocol())
    {
        CloseHandle (v_hRequestEvent);
        return dwRetVal;
    }

    // Note that adapters are dynamically bound via the
    // PROTBindAdapter callback.  No static initialization
    // takes place here.
    
    return SUCCESS;
}

#define COUNTOF(array) (sizeof(array) / sizeof(array[0]))

DWORD
pppMac_InstanceCreate (
    void *SessionContext, 
    void **ReturnedContext,
    LPCTSTR szDeviceName,
    LPCTSTR szDeviceType)
{
    NDIS_STATUS     Status;
    macCntxt_t      *pMac;
    
    DEBUGMSG (ZONE_FUNCTION, (TEXT("!pppMac_InstanceCreate devName=%s devType=%s\n"), szDeviceName, szDeviceType));
    
    // Allocate our context structure.
    pMac = (macCntxt_t *)pppAllocateMemory(sizeof (macCntxt_t));

    if (NULL == pMac) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pMac->session = SessionContext;
    
    pMac->bCallCloseRequested = FALSE;
    pMac->bCallClosed = TRUE;
    pMac->bLineCloseRequested = FALSE;
    pMac->bLineClosed = TRUE;

    pMac->bMacStatsObtained = FALSE;

    pMac->dwLineCallState = LINECALLSTATE_IDLE;

    StringCchCopyW(pMac->szDeviceName, COUNTOF(pMac->szDeviceName), szDeviceName);
    StringCchCopyW(pMac->szDeviceType, COUNTOF(pMac->szDeviceType), szDeviceType);

    (NDIS_HANDLE)pMac->hCall = INVALID_HANDLE_VALUE;
    (NDIS_HANDLE)pMac->hLine = INVALID_HANDLE_VALUE;

    if (SUCCESS != FindAdapter (szDeviceName, szDeviceType, &(pMac->pAdapter),
                                &(pMac->dwDeviceID))) {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: Can't find device '%s'\n"), szDeviceName));
        pppFreeMemory (pMac, sizeof (macCntxt_t));
        return ERROR_DEVICE_DOES_NOT_EXIST;
    }

    //
    //  Do not allow new connections on an adapter from which we are unbinding.
    //
    if (pMac->pAdapter->bClosingAdapter)
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: No new connections on '%s' allowed due to adapter close in progress\n"), pMac->szDeviceName));
        AdapterDelRef(pMac->pAdapter);
        pppFreeMemory (pMac, sizeof (macCntxt_t));
        return ERROR_DEVICE_DOES_NOT_EXIST;
    }

    DEBUGMSG (ZONE_TRACE, (TEXT("pppMac_InstanceCreate: Found DeviceID %d\r\n"),
                           pMac->dwDeviceID));

    pMac->pPendingLineCloseCompleteList = NULL;
    pMac->pPendingCallDropCompleteList = NULL;
    pMac->pPendingCallCloseCompleteList = NULL;
    
    PppNdisDoSyncRequest (&Status,
                            pMac->pAdapter,
                            NdisRequestQueryInformation,
                            OID_WAN_GET_INFO,
                            &(pMac->WanInfo),
                            sizeof(pMac->WanInfo));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG (ZONE_ERROR,
                  (TEXT(" pppMac_InstanceCreate: Error 0x%X from OID_WAN_GET_INFO\r\n"),
                   Status));
        AdapterDelRef (pMac->pAdapter);
        pppFreeMemory (pMac, sizeof (macCntxt_t));
        return ERROR_DEVICE_DOES_NOT_EXIST;
    }

    pMac->hNdisTapiEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    pMac->hEventLineOpenComplete = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (pMac->hNdisTapiEvent == NULL || pMac->hEventLineOpenComplete == NULL)
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR - pppMac_InstanceCreate CreateEvent failed\n")));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    NdisAllocateSpinLock (&pMac->PacketLock);

    Status = NdisWanAllocateSendResources (pMac);

    if (NDIS_STATUS_SUCCESS != Status) {
        // This will do the AdapterDelRef()
        pppMac_InstanceDelete (pMac);
        return Status;
    }
    
    *ReturnedContext = pMac;
    
    DEBUGMSG(ZONE_MAC, (L"PPP: Created MAC %x adapter=%s %x refcnt=%d\n", pMac, pMac->pAdapter->szAdapterName, pMac->pAdapter, pMac->pAdapter->dwRefCnt));

    return 0;
}

void
pppMac_GetFramingInfo(
    IN  PVOID   context,
    OUT PDWORD  pFramingBits,
    OUT PDWORD  pDesiredACCM)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;

    *pFramingBits = pMac->WanInfo.FramingBits;
    *pDesiredACCM = pMac->WanInfo.DesiredACCM;
}

DWORD
pppMac_LineOpen(
    void *context)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;
    DWORD           dwRetVal;

    // Configure device specific parameter settings, if any
    if (((pppSession_t *)(pMac->session))->lpbDevConfig != NULL)
    {
        NdisTapiSetDevConfig (pMac);
    }

    // Open the line
    dwRetVal = NdisTapiLineOpen (pMac);
    return pppMac_NdisToRasErrorCode(dwRetVal);
}

DWORD
pppMac_LineListen(
    void *context)
//
//  Listen for incoming connections on the specified line.
//
{
    macCntxt_t      *pMac = (macCntxt_t *)context;
    DWORD           dwRetVal;

    dwRetVal = NdisTapiSetDefaultMediaDetection(pMac, LINEMEDIAMODE_DIGITALDATA | LINEMEDIAMODE_DATAMODEM);
    return pppMac_NdisToRasErrorCode(dwRetVal);
}

static void
pppHangupComplete(
    PVOID   pData)
{
    HANDLE          hHangupCompleteEvent = (HANDLE)pData;

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:+pppHangupComplete\n" ) ));

    SetEvent(hHangupCompleteEvent);

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:-pppHangupComplete\n" ) ));
}

//
//  Custom Scripting Dll Support functions
//

DWORD
RasGetBuffer(
        OUT PBYTE *   ppBuffer,
    IN  OUT PDWORD    pdwSize)
//
//  The custom-scripting DLL calls RasGetBuffer to allocate memory
//  for sending or receiving data over the port connected to the server.
//
//  Return values
//  If the function succeeds, the return value is ERROR_SUCCESS.
//
//  If the function fails, the return value is the following error code.
//
//  ERROR_OUT_OF_BUFFERS    -   RAS cannot allocate anymore buffer space.
//
{
    DWORD   dwResult = ERROR_SUCCESS;

    // To simplify buffer management, always allocate max size buffers.
    *pdwSize = MAX_CUSTOM_SCRIPT_RX_BUFFER_SIZE;

    *ppBuffer = pppAllocateMemory(*pdwSize);
    if (*ppBuffer == NULL)
    {
        dwResult = ERROR_OUT_OF_BUFFERS;
        DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:ERROR in RasGetBuffer %d byte alloc, error = %x\n" ), *pdwSize, GetLastError() ));
    }

    return dwResult;
}

DWORD
RasFreeBuffer(
    IN  PBYTE   pBuffer)
//
//  The custom-scripting DLL calls RasFreeBuffer to release a
//  memory buffer that was allocated by a previous call to RasGetBuffer
//
//  Return values
//  If the function succeeds, the return value is ERROR_SUCCESS.
//
//  If the function fails, the return value is the following error code.
//
//  ERROR_BUFFER_INVALID        - The pointer to the buffer passed in the
//                                pBuffer parameter is invalid.
//
{
    DWORD   dwResult = ERROR_SUCCESS;

    pppFreeMemory(pBuffer, MAX_CUSTOM_SCRIPT_RX_BUFFER_SIZE);

    return dwResult;
}

DWORD
RasSendBuffer(
    IN  HANDLE    hPort,
    IN  PBYTE     pBuffer,
    IN  DWORD     dwSize)
//
//  The custom-scripting DLL calls the RasSendBuffer function
//  to send data to the server over the specified port.
//
//  Return values
//  If the function succeeds, the return value is ERROR_SUCCESS.
//  If the function fails, the return value can be one of the following error codes.
//  
//  Value                       Meaning 
//  ERROR_BUFFER_INVALID        The pointer to the buffer passed in the pBuffer parameter is invalid. 
//  ERROR_INVALID_PORT_HANDLE   The handle specified by the hPort parameter is invalid.
//
{
    macCntxt_t      *pMac = (macCntxt_t *)hPort;
    DWORD           dwResult = ERROR_SUCCESS,
                    dwBytesSent,
                    bytesWritten;

    for (dwBytesSent = 0;
         dwBytesSent < dwSize;
         dwBytesSent += bytesWritten)
    {
        if (!WriteFile (pMac->hComPort, pBuffer + dwBytesSent, dwSize - dwBytesSent, &bytesWritten, 0)
        ||  bytesWritten == 0)
        {
            dwResult = GetLastError();
            DEBUGMSG(ZONE_ERROR, (
                TEXT( "PPP: ERROR RasSendBuffer-WriteFile Error %d Aborting Packet after sending %d of %d bytes\n" ),
                dwResult, dwBytesSent, dwSize) );
            break;
        }
    }

    return dwResult;
}

DWORD
RasRetrieveBuffer(
    IN  HANDLE    hPort,
    OUT PBYTE     pBuffer,
    OUT PDWORD    pdwBytesRead)
//
//  The custom-scripting DLL calls the RasRetrieveBuffer function
//  to obtain data received from the RAS server over the specified port.
//  The custom-scripting DLL should call RasRetrieveBuffer only after
//  RAS has signaled the event object passed in the call to RasReceiveBuffer.
//
//  Return values
//  If the function succeeds, the return value is ERROR_SUCCESS.
//  
//  If the function fails, the return value can be one of the following error codes.
//  
//  Value                       Meaning 
//  ERROR_BUFFER_INVALID        The pointer to the buffer passed in the pBuffer parameter is invalid. 
//  ERROR_INVALID_PORT_HANDLE   The handle specified by the hPort parameter is invalid.
//
{
    macCntxt_t      *pMac = (macCntxt_t *)hPort;
    DWORD           dwResult = ERROR_SUCCESS;

    if (ReadFile(pMac->hComPort, pBuffer, MAX_CUSTOM_SCRIPT_RX_BUFFER_SIZE, pdwBytesRead, 0 ) == FALSE)
    {
        // Serial functions will return an error if a removable device has
        // been removed. If the error is INVALID_HANDLE or GEN_FAILURE
        // the removable device was removed. In this case the MAC layer is
        // down.

        dwResult = GetLastError();
        DEBUGMSG(ZONE_ERROR, (TEXT( "PPP:RasRetrieveBuffer ReadFile failed %d\n"), dwResult));
    }

    return dwResult;
}

DWORD WINAPI
rasReceiveEventThread (
    IN  LPVOID pVArg)
{
    macCntxt_t      *pMac = (macCntxt_t *)pVArg;
    DWORD           dwMask;

    DEBUGMSG( ZONE_FUNCTION, (TEXT( "PPP: +rasReceiveEventThread\n") ));

    while ( WaitCommEvent(pMac->hComPort, &dwMask, NULL ) == TRUE )
    {
        if (dwMask & (EV_POWER | EV_RLSD))
        {
            // Line down
            DEBUGMSG(ZONE_ERROR, (TEXT( "PPP:rasReceiveEventThread - LINE DOWN eventmask=%x\n"), dwMask));
        }
        else if (dwMask & EV_RXCHAR)
        {
            // Data ready to be read
            DEBUGMSG(ZONE_ERROR, (TEXT( "PPP:rasReceiveEventThread - EV_RXCHAR\n"), dwMask));
        }

        //
        //  Signal the custom dll script to call RasRetrieveBuffer to read any rx data
        //
        SetEvent(pMac->hRxEvent);

        //
        //  Paranoia?  In case WaitCommEvent returns for pending rx data instead of
        //  waiting for new rx data.
        //
        Sleep(50);
    }

    DEBUGMSG( ZONE_FUNCTION, (TEXT( "PPP: -rasReceiveEventThread\n") ));
    return 0;
}

DWORD
RasReceiveBuffer(
    IN  HANDLE    hPort,
    OUT PBYTE     pBuffer,
    OUT PDWORD    pdwSize,
    IN  DWORD     dwTimeoutMilliseconds,
    IN  HANDLE    hEvent)
//
//  The custom-scripting DLL calls the RasReceiveBuffer function
//  to inform RAS that it is ready to receive data from the server over the specified port.
//
//  Return values
//  If the function succeeds, the return value is ERROR_SUCCESS.
//  
//  If the function fails, the return value can be one of the following error codes.
//  
//  Value                       Meaning 
//  ERROR_BUFFER_INVALID        The pointer to the buffer passed in the pBuffer parameter is invalid. 
//  ERROR_INVALID_PORT_HANDLE   The handle specified by the hPort parameter is invalid.
//
{
    macCntxt_t      *pMac = (macCntxt_t *)hPort;
    DWORD           dwResult = ERROR_SUCCESS,
                    dwID;
    COMMTIMEOUTS    CommTimeouts;

    do
    {
        if (SetCommMask(pMac->hComPort, EV_RXCHAR | EV_RLSD | EV_POWER ) == FALSE)
        {
            dwResult = GetLastError();
            break;
        }

        // Configure the port timeouts to:
        //      1. Make a ReadFile return after no more than dwTimeoutMilliseconds
        //      2. Allow 500 ms + 2ms/byte before timing out a WriteFile

        if (GetCommTimeouts( pMac->hComPort, &CommTimeouts ) == FALSE)
        {
            dwResult = GetLastError();
            break;
        }
        CommTimeouts.ReadIntervalTimeout = dwTimeoutMilliseconds;
        CommTimeouts.ReadTotalTimeoutMultiplier = 0;
        CommTimeouts.ReadTotalTimeoutConstant = 0;
        CommTimeouts.WriteTotalTimeoutMultiplier = 2;
        CommTimeouts.WriteTotalTimeoutConstant = 500;

        if (SetCommTimeouts( pMac->hComPort, &CommTimeouts ) == FALSE)
        {
            dwResult = GetLastError();
            break;
        }

        // Return no bytes read.  To read bytes "RasRetrieveBuffer" must be called
        *pdwSize = 0;

        // Save context information for use by the receive thread
        pMac->pRxBuffer = pBuffer;
        pMac->dwRxBufferSize = MAX_CUSTOM_SCRIPT_RX_BUFFER_SIZE;
        pMac->dwRxBytesReceived = 0;
        pMac->hRxEvent = hEvent;
        if (pMac->hRxEventThread == NULL)
        {
            pMac->hRxEventThread = CreateThread (NULL, 0, rasReceiveEventThread, pMac, 0, &dwID);
            if (pMac->hRxEventThread == NULL)
            {
                dwResult = GetLastError();
                break;
            }
        }
    } while (FALSE);

    return dwResult;
}


DWORD
pppMacCustomScriptExecute(
    IN      macCntxt_t      *pMac,
    IN  OUT LPRASDIALPARAMS  pRasDialParams)
//
//  Run the "script" on the line, after the connection has been established
//  but before LCP starts.
//
//  RAS calls the RasCustomScriptExecute function when establishing a connection for a phone-book entry that has the RASEO_CustomScript option set.
//
//  The RasCustomScriptExecute function should be in the dll specified in the registry value
//      \\HKEY_LOCAL_MACHINE\Comm\PPP\Parms\CustomScriptDllPath REG_SZ 
//
//  Optionally, if the "szScript" parameter of the RasEntry is of the format:
//      <dllName>[|<parameterName>]
//  then the name specified before the '|' character will be the name of the dll loaded.
//  That is, it will be loaded in place of the registry setting.
//
// DWORD RasCustomScriptExecute(
//  HANDLE hPort,
//  LPCWSTR lpszPhonebook,
//  LPCWSTR lpszEntryName,
//  PFNRASGETBUFFER pfnRasGetBuffer,
//  PFNRASFREEBUFFER pfnRasFreeBuffer,
//  PFNRASSENDBUFFER pfnRasSendBuffer,
//  PFNRASRECEIVEBUFFER pfnRasReceiveBuffer,
//  PFNRASRETRIEVEBUFFER pfnRasRetrieveBuffer,
//  HWND hWnd,
//  RASDIALPARAMS *pRasDialParams,
//  RASCUSTOMSCRIPTEXTENSIONS *pRasCustomScriptExtensions
// );
// 
//  hPort is the handle returned by the Miniport in an OID_TAPI_GET_ID query of
//  the deviceClass "comm/datamodem".  If the Miniport returns an error on that query,
//  the script will not be run.
//
//  If the RasCustomScriptExecute function returns SUCCESS (0), then the connection attempt
//  will proceed to the LCP negotiations and so forth.
//  A non-0 return value will cause the connection to be terminated.
//
{
    pppSession_t    *s_p = pMac->session;
    DWORD           dwResult,
                    nRead;
    LPTSTR          pStr;
    WCHAR           wszDllName[MAX_PATH+1];
    HMODULE         hScriptDLL;
    DWORD           (*pfnRasCustomScriptExecute)(); 
    PVAR_STRING     pDeviceID;
    NDIS_STATUS     Status;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMacCustomScriptExecute\n")));

    do
    {
        //
        //  Get the handle to the COM port from the miniport driver
        //
        Status = NdisTapiGetDeviceId(pMac, LINECALLSELECT_CALL, TEXT("comm/datamodem"), &pDeviceID);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            dwResult = E_FAIL;
            break;
        }

        pMac->hComPort = *((HANDLE *)((LPBYTE)pDeviceID + pDeviceID->ulStringOffset));

        pppFreeMemory(pDeviceID, pDeviceID->ulTotalSize);

        //
        //  Get the name of the custom script dll, from the
        //      szScript field of the RASENTRY, if present, otherwise
        //      HKLM\Comm\Ppp\Parms\CustomScriptDllPath
        //
        wcscpy(wszDllName, s_p->rasEntry.szScript);
        pStr = _tcschr(wszDllName, TEXT('|'));
        if (pStr)
        {
            *(pStr++) = TEXT('\0');
        }
        else
        {
            nRead = ReadRegistryValues(
                        HKEY_LOCAL_MACHINE, L"Comm\\PPP\\Parms",
                        L"CustomScriptDllPath", REG_SZ, 0, (PVOID)wszDllName, sizeof(wszDllName),
                        NULL);
            if (nRead != 1)
            {
                dwResult = E_FAIL;
                break;
            }
        }

        DEBUGMSG (ZONE_PPP, (TEXT("PPP: Custom Script DLL='%s'\n"), wszDllName));

        hScriptDLL = LoadLibrary (wszDllName);
        if (hScriptDLL == NULL)
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR - Unable to LoadLibrary ('%s'), GetLastError=%d\n"),
                          wszDllName, GetLastError()));
            dwResult = E_FAIL;
            break;
        }

        pfnRasCustomScriptExecute = GetProcAddress(hScriptDLL, L"RasCustomScriptExecute");
        if (pfnRasCustomScriptExecute == NULL)
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR - Unable to find RunScript function in DLL '%s'\n"),
                              s_p->rasEntry.szScript));
            dwResult = E_FAIL;
        }
        else
        {
            DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: Calling %s!RasCustomScriptExecute()\n"),
                              wszDllName));

            dwResult = pfnRasCustomScriptExecute(
                            pMac, 
                            NULL,       // lpszPhonebook,
                            s_p->rasDialParams.szEntryName,
                            RasGetBuffer,
                            RasFreeBuffer,
                            RasSendBuffer,
                            RasReceiveBuffer,
                            RasRetrieveBuffer,
                            NULL,       // hWnd
                            pRasDialParams,
                            NULL        // pRasCustomScriptExtensions
                            );

            DEBUGMSG(dwResult && ZONE_ERROR,
                            (TEXT("PPP: ERROR - Script %s - RasCustomScriptExecute returned error %d\n"),
                                wszDllName, dwResult));

            // Terminate the receive thread, if any
            (void)SetCommMask (pMac->hComPort, 0);

            if (pMac->hRxEventThread)
            {
                WaitForSingleObject(pMac->hRxEventThread, INFINITE);
                CloseHandle(pMac->hRxEventThread);
                pMac->hRxEventThread = NULL;
            }

            pMac->hRxEvent = NULL;
            pMac->pRxBuffer = NULL;
            pMac->dwRxBufferSize = 0;
            pMac->dwRxBytesReceived = 0;

        }
        FreeLibrary (hScriptDLL);
    } while (FALSE);

    pMac->hComPort = NULL;

    return dwResult;
}

DWORD
pppMac_Dial(
    void            *context,
    LPRASPENTRY      pRasEntry,
    LPRASDIALPARAMS  pRasDialParams)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;
    pppSession_t    *s_p = (pppSession_t *)pMac->session;
    LPTSTR          szDialStr;
    DWORD           dwFlag;
    DWORD           dwRetVal = NO_ERROR;
    HANDLE          hHangupCompleteEvent;
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    
    DEBUGMSG (ZONE_FUNCTION,
              (TEXT("+pppMac_Dial(0x%X, 0x%X, 0x%X) (%s) %s\r\n"),
               context, pRasEntry, pRasDialParams,
               pRasEntry->szAreaCode,
               pRasEntry->szLocalPhoneNumber));

    do
    {
        //
        // For PPPoE, set flag to suppress prepending the address and
        // control bytes when sending a frame.
        //
        if (_tcscmp(pRasEntry->szDeviceType, RASDT_PPPoE) == 0)
        {
            pMac->bIsPPPoE = TRUE;
        }

        //
        //  For Vpn lines, we don't want to translate the VPN server name
        //  (phone number)
        //
        if ((_tcscmp(pRasEntry->szDeviceType, RASDT_Vpn)   != 0) &&
            (_tcscmp(pRasEntry->szDeviceType, RASDT_PPPoE) != 0))
        {
            pppUnLock(s_p);

            dwFlag = 0;
            if (pRasEntry->dwfOptions & RASEO_UseCountryAndAreaCodes)
                dwFlag = LINETRANSLATEOPTION_FORCELD;
            else if (pRasEntry->dwfOptions & RASEO_DialAsLocalCall)
                dwFlag = LINETRANSLATEOPTION_FORCELOCAL;

            szDialStr = NdisTapiLineTranslateAddress (pMac->pAdapter, pMac->dwDeviceID,
                                                    pRasEntry->dwCountryCode,
                                                    pRasEntry->szAreaCode,
                                                    pRasEntry->szLocalPhoneNumber,
                                                    TRUE, dwFlag);

            pppLock(s_p);

            if (NULL == szDialStr)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR from NdisTapiLineTranslateAddress\r\n")));
                dwRetVal = ERROR_BAD_PHONE_NUMBER;
                break;
            }
        }
        else
        {
            szDialStr = &pRasEntry->szLocalPhoneNumber[0];
        }

        //
        // Not all devices require devconfig. These will return NDIS_STATUS_NOT_SUPPORTED,
        // NDIS_STATUS_NOT_RECOGNIZED, or NDIS_STATUS_INVALID_OID, 
        // when we try to get/set OID_TAPI_GET/SET_DEV_CONFIG. This is not an error, we
        // continue to try to establish the connection in that case.
        //

        // If the RAS entry doesn't have a dev config, then get the default config
        // for the line.
        if (((pppSession_t *)(pMac->session))->lpbDevConfig == NULL)
        {
            pppUnLock(s_p);
            Status = NdisTapiGetDevConfig (pMac->pAdapter, pMac->dwDeviceID,
                            &(((pppSession_t *)(pMac->session))->lpbDevConfig),
                            &((pppSession_t *)(pMac->session))->dwDevConfigSize);
            pppLock(s_p);

            if (Status != NDIS_STATUS_SUCCESS
            &&  Status != NDIS_STATUS_NOT_SUPPORTED
            &&  Status != NDIS_STATUS_NOT_RECOGNIZED
            &&  Status != NDIS_STATUS_INVALID_OID)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR from NdisTapiGetDevConfig %x\r\n"), Status));
                break;
            }
        }

        //
        // Bluetooth modems need the devconfig set before lineOpen since there is Bluetooth
        // connection information in the devconfig.
        //
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Status = NdisTapiSetDevConfig (pMac);
            if (Status != NDIS_STATUS_SUCCESS
            &&  Status != NDIS_STATUS_NOT_SUPPORTED
            &&  Status != NDIS_STATUS_NOT_RECOGNIZED
            &&  Status != NDIS_STATUS_INVALID_OID)
            {
                // Failing this request is not fatal, the devconfig gets passed again later in the
                // NdisTapiLineMakeCall.
                DEBUGMSG(ZONE_MAC, (TEXT("PPP: NdisTapiSetDevConfig returned Status %x\r\n"), Status));
            }
        }

        DEBUGMSG (ZONE_RAS, (TEXT("PPP: DialableString='%s'\r\n"), szDialStr));

        Status = NdisTapiLineOpen (pMac);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR from NdisTapiLineOpen %x\r\n"), Status));
            break;
        }

        DEBUGMSG (ZONE_RAS, (TEXT("NdisTapiLineOpen successful\r\n")));

        //
        // Non-zero TapiEcode means that another thread called NdisTapiHangUp while
        // we were doing the LineOpen, so we should proceed to abort the dialing.
        //
        if (pMac->TapiEcode)
        {
            DEBUGMSG (ZONE_RAS, (TEXT("PPP: TapiEcode=%d after NdisTapiLineOpen\n")));
            dwRetVal = pMac->TapiEcode;
            break;
        }

        Status = NdisTapiLineMakeCall (pMac, szDialStr);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR from NdisTapiLineMakeCall %x\r\n"), Status));
            break;
        }
    } while (FALSE); // end do

    // Done with the dial string, free it if space allocated for it
    if (szDialStr != &pRasEntry->szLocalPhoneNumber[0])
        LocalFree (szDialStr);

    // Return with error if something went wrong
    if (Status != NDIS_STATUS_SUCCESS)
        dwRetVal =  pppMac_NdisToRasErrorCode(Status);
    if (dwRetVal != NO_ERROR)
        return dwRetVal;

    DEBUGMSG (ZONE_RAS, (TEXT("NdisTapiLineMakeCall successful\r\n")));

    DEBUGMSG (ZONE_RAS, (TEXT("PPP: NdisTapiLineMakeCall waiting for event\n")));

    //
    // Unlock the session while we are waiting for the NdisTapiEvent so that
    // pppSessionStop can initiate an abort (e.g. user presses "Cancel").
    //
    pppUnLock( s_p );

    //
    // Wait for something to complete, either a status indication
    // from the adapter (e.g. CALL_CONNECTED) or a pppSessionStop call.
    //
    WaitForSingleObject (pMac->hNdisTapiEvent, INFINITE);

    pppLock( s_p );

    DEBUGMSG (ZONE_RAS, (TEXT("PPP: NdisTapiLineMakeCall got event\n")));
    DEBUGMSG (ZONE_RAS, (TEXT("pppMac_Dial: TapiEcode=%d\r\n"), pMac->TapiEcode));

    if (pMac->TapiEcode == 0)
    {
        // Do we have a script?
        if (s_p->rasEntry.dwfOptions & RASEO_CustomScript)
        {
            pppChangeOfState (pMac->session, RASCS_Interactive, 0);
            pMac->TapiEcode = pppMacCustomScriptExecute(pMac, pRasDialParams);
        }

        if (pMac->TapiEcode == 0)
        {
            pppChangeOfState (pMac->session, RASCS_DeviceConnected, 0);
        }

        //
        //  This will trigger the miniport to begin listening for data,
        //  e.g. on AsyncMac it causes the RxThread to be created.
        //
        (void)NdisTapiGetDeviceId(pMac, LINECALLSELECT_CALL, L"ndis", NULL);
    }

    if (pMac->TapiEcode)
    {
        // Close all of our handles etc.
        hHangupCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        NdisTapiHangup (pMac, pppHangupComplete, hHangupCompleteEvent);

        if (hHangupCompleteEvent != NULL)
        {
            //
            // Unlock the session while we are waiting for the NdisTapiEvent so that
            // pppSessionStop can initiate an abort (e.g. user presses "Cancel").
            //
            pppUnLock( s_p );

            WaitForSingleObject(hHangupCompleteEvent, INFINITE);

            pppLock( s_p );

            CloseHandle(hHangupCompleteEvent);
        }
    }

    return pMac->TapiEcode;
}

void
pppMac_Reset(void *context)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;
    NDIS_STATUS      Status;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMac_Reset\n")));

    // We have to force L2TP to abort IKE negotiations,
    // since we cannot get L2TP an OID_TAPI_CLOSE request until
    // the OID_TAPI_MAKE_CALL request completes.
    NdisReset (&Status, pMac->pAdapter->hAdapter);
}

void
pppMac_AbortDial(void *context)
//
//  This function will abort a dialing operation if one is in progress.
//
{
    macCntxt_t      *pMac = (macCntxt_t *)context;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMac_AbortDial\n")));

    pMac->TapiEcode = ERROR_USER_DISCONNECTION;

    SetEvent (pMac->hNdisTapiEvent);

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -pppMac_AbortDial\n")));
}

void
pppMac_CallClose(
    void        *context,
    void        (*pCallCloseCompleteCallback)(PVOID),
    PVOID       pCallbackData)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+pppMac_Close\n")));

    // Let's just hang up....
    NdisTapiHangup (pMac, pCallCloseCompleteCallback, pCallbackData);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-pppMac_Close\n"))); 
}

void
pppMac_LineClose (
    void        *context,
    void        (*pLineCloseCompleteCallback)(PVOID),
    PVOID       pCallbackData)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;
    NDIS_STATUS     Status;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+pppMac_LineClose\n")));
        
    Status = NdisTapiLineClose(pMac, pLineCloseCompleteCallback, pCallbackData);

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-pppMac_LineClose\n")));
}

void
pppMac_InstanceDelete (void *context)
{
    macCntxt_t      *pMac = (macCntxt_t *)context;

    DEBUGMSG(ZONE_MAC, (L"PPP: Delete MAC %x adapter=%x\n", pMac,  pMac->pAdapter));

    DEBUGMSG (ZONE_TRACE, (TEXT("!pppMac_InstanceDelete\r\n")));
    if (pMac->pAdapter) {
        AdapterDelRef (pMac->pAdapter);
        pMac->pAdapter = NULL;
    }
    CloseHandle (pMac->hNdisTapiEvent);
    CloseHandle (pMac->hEventLineOpenComplete);
    NdisFreeSpinLock (&pMac->PacketLock);
    pppFreeMemory(pMac->PacketMemory, pMac->PacketMemorySize);
    pppFreeMemory (pMac, sizeof (macCntxt_t));
}

void
pppMac_GetCallSpeed(
    void    *context, 
    PDWORD  pSpeed)
{
    macCntxt_t              *pMac = (macCntxt_t *)context;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("+pppMac_GetCallSpeed\n")));

    *pSpeed = pMac->LinkSpeed * 100;
    DEBUGMSG( ZONE_MAC, 
              (TEXT("PPP: (%hs) - Interface Bit Rate is %u b/s\n"),
              __FUNCTION__,
              *pSpeed ) );

}
