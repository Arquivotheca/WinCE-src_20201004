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
/**************************************************************************/
/**************************************************************************/
//
//  misc.c      Support files for Telephony Service Provider Interface
//
//  @doc    EX_TSPI
//
//  @topic  TSPI |
//
//      Tspi utilities
//
//
#include "windows.h"
#include "types.h"
#include "memory.h"
#include "mcx.h"
#include "tspi.h"
#include "tspip.h"
#include "linklist.h"

const WCHAR szDefaultsName[] = TEXT("Drivers\\Unimodem");

// Unimodem thread priority
#define DEFAULT_THREAD_PRIORITY 150
#define REGISTRY_PRIORITY_VALUE TEXT("Priority256")
DWORD g_dwUnimodemThreadPriority;

const DEVMINICFG DefaultDevCfg =
{
    DEVMINCFG_VERSION,          // Version
    0x00,                       // WaitBong
    90,                         // CallSetupFail
    0,                          // ModemOptions
    19200,                      // Baud Rate
    0,                          // Options
    8,                          // Byte Size
    0,                          // Stop Bits
    0,                          // Parity
    {0},                        // Dial Modifier
    {0},                        // Driver Name (for dynamic ports)
    {0},                        // Config Blob (for dynamic ports)
    {0}                         // RegisterDevice Port handle (for dynamic ports)
};


extern HANDLE g_hCoreDLL;

//
// Function to complete an outstanding async operation
// (unimodem can only handle one outstanding async operation per linedev)
//
// NOTE: Must be called holding the OpenCS
//
void
CompleteAsyncOp(
    PTLINEDEV pLineDev
    )
{
    DWORD dwID;
    DWORD dwStatus;
    DWORD dwType;

    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:+CompleteAsyncOp\n")));


    dwID = pLineDev->dwPendingID;
    dwStatus = pLineDev->dwPendingStatus;
    dwType = pLineDev->dwPendingType;
    pLineDev->dwPendingID = INVALID_PENDINGID;
    pLineDev->dwPendingType = INVALID_PENDINGOP;
    pLineDev->dwPendingStatus = 0;

    switch (dwType) {
    case PENDING_LINEACCEPT:
    case PENDING_LINEANSWER:
    case PENDING_LINEDEVSPECIFIC:
    case PENDING_LINEDIAL:
    case PENDING_LINEDROP:
    case PENDING_LINEMAKECALL:
        LeaveCriticalSection(&pLineDev->OpenCS);
        DEBUGMSG(ZONE_ASYNC, (TEXT("UNIMODEM:CompleteAsyncOp completing Op %s(0x%x)\n"),
                             GetPendingName(dwType), dwID));
        TspiGlobals.fnCompletionCallback(dwID, dwStatus);
        EnterCriticalSection(&pLineDev->OpenCS);
        break;
    }

    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:-CompleteAsyncOp\n")));
}   // CompleteAsyncOp


//
// NOTE on async operations:
// The function that invokes an async operation (TSPI_lineMakeCall, TSPI_lineAnswer, etc)
// first calls ControlThreadCmd which calls SetAsyncOp. Just before returning it calls
// SetAsyncID in order to avoid calling the TAPI completion callback before the user
// knows the async ID. The UnimodemControlThread will call SetAsyncStatus when the
// async operation has completed. If the ID has been set, then SetAsyncStatus will perform
// the callback.
//

//
// Set the async ID for a pending operation. This signals that the function which
// invoked the async operation has completed so any subsequent SetAsyncStatus call
// will cause a completion callback to TAPI. If the invoked async operation has
// already completed (SetAsyncStatus already called), then we need to kick off a
// thread to make the callback for this completed async operation.
//
// Return LINEERR_OPERATIONFAILED if this async operation has been aborted, else
// return async ID.
//
DWORD
SetAsyncID(
    PTLINEDEV pLineDev,
    DWORD dwID
    )
{
    DWORD dwRet = LINEERR_OPERATIONFAILED;

    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:+SetAsyncID\n")));
    EnterCriticalSection(&pLineDev->OpenCS);
    if (pLineDev->dwPendingID != INVALID_PENDINGID) {
        LeaveCriticalSection(&pLineDev->OpenCS);
    } else {
        pLineDev->dwPendingID = dwID;
        dwRet = dwID;
        //
        // If the operation has completed then we need to do the callback on another thread
        //
        if (pLineDev->dwPendingStatus != LINEERR_ALLOCATED) {
            LeaveCriticalSection(&pLineDev->OpenCS);
            if (!StartCompleteAsyncOp(pLineDev)) {
                dwRet = LINEERR_OPERATIONFAILED;
            }
        } else {
            LeaveCriticalSection(&pLineDev->OpenCS);
        }
    }
    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:-SetAsyncID 0x%x\n"), dwRet));
    return dwRet;
}   // SetAsyncID


//
// Set the status for a pending async operation and complete it if possible.
//
void
SetAsyncStatus(
    PTLINEDEV pLineDev,
    DWORD dwNewStatus
    )
{
    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:+SetAsyncStatus\n")));
    EnterCriticalSection(&pLineDev->OpenCS);
    //
    // Is there a pending operation to complete?
    //
    if (pLineDev->dwPendingType != INVALID_PENDINGOP) {
        pLineDev->dwPendingStatus = dwNewStatus;
        if (pLineDev->dwPendingID != INVALID_PENDINGID) {
            CompleteAsyncOp(pLineDev);
        }
    }
    LeaveCriticalSection(&pLineDev->OpenCS);
    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:-SetAsyncStatus\n")));
}   // SetAsyncStatus


//
// Abort any previously pending operation and record new pending operation
// (called by async TSPI_line* functions)
//
void
SetAsyncOp(
    PTLINEDEV pLineDev,
    DWORD dwNewOp
    )
{
    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:+SetAsyncOp\n")));
    EnterCriticalSection(&pLineDev->OpenCS);

    //
    // Is there a pending operation that we need to abort?
    //
    if (pLineDev->dwPendingType != INVALID_PENDINGOP) {
        LeaveCriticalSection(&pLineDev->OpenCS);
        SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);
        EnterCriticalSection(&pLineDev->OpenCS);
    }

#ifdef DEBUG
    pLineDev->dwLastPendingOp = pLineDev->dwPendingType;
#endif
    pLineDev->dwPendingType = dwNewOp;
    pLineDev->dwPendingStatus = LINEERR_ALLOCATED;
    pLineDev->dwPendingID = INVALID_PENDINGID;

    LeaveCriticalSection(&pLineDev->OpenCS);
    DEBUGMSG(ZONE_FUNC|ZONE_ASYNC, (TEXT("UNIMODEM:-SetAsyncOp\n")));
}   // SetAsyncOp


typedef struct _ASYNCCMD {
    DWORD dwRequestID;
    DWORD dwResult;
} ASYNCCMD, * PASYNCCMD;


DWORD
AsyncCompleteThread(
    PVOID lpvIn
    )
{
    PASYNCCMD pCmd = (PASYNCCMD)lpvIn;

    DEBUGMSG(ZONE_ASYNC, (TEXT("UNIMODEM:AsyncCompleteThread completing ID(0x%x) result 0x%x\n"),
                         pCmd->dwRequestID, pCmd->dwResult));
    TspiGlobals.fnCompletionCallback(pCmd->dwRequestID, pCmd->dwResult);
    TSPIFree(pCmd);
    return 0;
}   // AsyncCompleteThread


BOOL
StartCompleteAsyncOp(
    PTLINEDEV pLineDev
    )
{
    HANDLE hThd;
    PASYNCCMD pCmd;

    if (NULL == (pCmd = TSPIAlloc(sizeof(ASYNCCMD)))) {
        DEBUGMSG(1, (TEXT("UNIMODEM:StartCompleteAsyncOp Unable to allocate ASYNCCMD!!!\n")));
        return FALSE;
    }

    EnterCriticalSection(&pLineDev->OpenCS);
    pCmd->dwRequestID = pLineDev->dwPendingID;
    pCmd->dwResult = pLineDev->dwPendingStatus;
    pLineDev->dwPendingID = INVALID_PENDINGID;
    pLineDev->dwPendingType = INVALID_PENDINGOP;
    pLineDev->dwPendingStatus = 0;
    LeaveCriticalSection(&pLineDev->OpenCS);

    hThd = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)AsyncCompleteThread,
                         pCmd, 0, NULL );
    if (!hThd) {
        DEBUGMSG(1, (TEXT("UNIMODEM:StartCompleteAsyncOp Unable to Create AsyncCompleteThread!!!\n")));
        TSPIFree(pCmd);
        return FALSE;
    }

    CloseHandle(hThd);
    return TRUE;

}   // StartCompleteAsyncOp

//
// Wrapper to retrieve a DWORD value from the "Config" key of the line device's registry
//
DWORD
MdmRegGetConfigDWORDValue(
    PTLINEDEV ptLineDev,
    LPWSTR    pszValueName,
    LPDWORD   pdwValue
    )
{
    DWORD dwSize;

    dwSize = sizeof(DWORD);
    return MdmRegGetValue(ptLineDev, L"Config", pszValueName, REG_DWORD, (LPBYTE)pdwValue, &dwSize);
}


//
// Arrays to simplify setting the DEVMINICFG.dwModemOptions field from the registry
//
DWORD ModemOptionFlags[] = {
    MDM_SPEED_ADJUST,
    MDM_BLIND_DIAL,
    MDM_FLOWCONTROL_HARD,
    MDM_FLOWCONTROL_SOFT,
    MDM_TONE_DIAL,
    0
};

LPWSTR ModemOptionValues[] = {
    L"EnableAutoBaud",
    L"EnableBlindDial",
    L"EnableFlowHard",
    L"EnableFlowSoft",
    L"EnableToneDial"
};

//
// Arrays to simplify setting the DEVMINICFG.fwOptions field from the registry
//
DWORD TerminalOptionFlags[] = {
    MANUAL_DIAL,
    TERMINAL_POST,
    TERMINAL_PRE,
    0
};

LPWSTR TerminalOptionValues[] = {
    L"ManualDial",
    L"PostDialTerminal",
    L"PreDialTerminal",
};

//
// Copy and/or convert any version of DEVMINICFG to the current version
//
void
ConvertDevConfig(
    PDEVMINICFG pInDevMiniCfg,
    DWORD dwSize,
    PDEVMINICFG pOutDevMiniCfg
    )
{
    BOOL bCopy = ((DWORD)pInDevMiniCfg != (DWORD)pOutDevMiniCfg);

    switch (pInDevMiniCfg->wVersion) {
    case DEVMINCFG_VERSION_0010:
        if (dwSize > sizeof(DEVMINICFG_0010)) {
            dwSize = sizeof(DEVMINICFG_0010);
        }
        // FALLTHROUGH
    case DEVMINCFG_VERSION:
        if (bCopy && (dwSize <= sizeof(*pOutDevMiniCfg))) {
            memcpy(pOutDevMiniCfg, pInDevMiniCfg, dwSize);
        }
        if (dwSize < sizeof(DEVMINICFG)) {
            memset((PBYTE)((DWORD)pOutDevMiniCfg + dwSize), 0, sizeof(DEVMINICFG) - dwSize);
        }
        break;

    case DEVMINCFG_VERSION_0020:
    {
        PDEVMINICFG_0020 pDevMiniCfg_0020 = (PDEVMINICFG_0020)pInDevMiniCfg;

        if (dwSize > sizeof(DEVMINICFG_0020)) {
            dwSize = sizeof(DEVMINICFG_0020);
        }

        if (bCopy) {
            memcpy(pOutDevMiniCfg, pInDevMiniCfg, dwSize);
        }

        memset((PBYTE)((DWORD)pOutDevMiniCfg + dwSize), 0, sizeof(DEVMINICFG) - dwSize);

        //
        // Copy the dynamic modem fields, hPort, wszDriverName and pConfigBlob, around
        //  the now larger dial modifier field.
        //
        if (dwSize >= sizeof(DEVMINICFG_0020)) {
            pOutDevMiniCfg->hPort = pDevMiniCfg_0020->hPort;
        }

        if (dwSize >= OFFSETOF(DEVMINICFG_0020, hPort)) {
            memcpy(pOutDevMiniCfg->pConfigBlob, pDevMiniCfg_0020->pConfigBlob, MAX_CFG_BLOB);
        }

        if (dwSize >= OFFSETOF(DEVMINICFG_0020, pConfigBlob)) {
            memcpy(pOutDevMiniCfg->wszDriverName, pDevMiniCfg_0020->wszDriverName, sizeof(WCHAR)*(MAX_NAME_LENGTH+1));
        }

        if (dwSize >= OFFSETOF(DEVMINICFG_0020, szDialModifier)) {
            pOutDevMiniCfg->szDialModifier[DIAL_MODIFIER_LEN_0020] = 0;
        }
    }
    break;

    default:
        *pOutDevMiniCfg = DefaultDevCfg;
        break;
    }

    pOutDevMiniCfg->wVersion = DEVMINCFG_VERSION;
}   // ConvertDevConfig

//
// Read the default devconfig for this device from the registry
//
VOID
getDefaultDevConfig(
    PTLINEDEV ptLineDev,
    PDEVMINICFG pDevMiniCfg
    )
{
    static const WCHAR szDevConfig[]    = TEXT("DevConfig");
    DWORD dwSize;
    DWORD dwRetCode;
    DWORD dwTmp;
    DWORD i;
    WCHAR szDialModifier[DIAL_MODIFIER_LEN+1];
    
    DEBUGMSG(ZONE_FUNC | ZONE_INIT,
             (TEXT("UNIMODEM:+getDefaultDevConfig\r\n")));
    // Get the default device config from the registry
    dwSize = sizeof(DEVMINICFG);
    dwRetCode = MdmRegGetValue(ptLineDev, NULL, szDevConfig, REG_BINARY,
                               (LPBYTE)pDevMiniCfg, &dwSize);

    if ((dwRetCode != ERROR_SUCCESS) || (dwSize < (MIN_DEVCONFIG_SIZE))) {
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,
                 (TEXT("UNIMODEM:Device Config error, retcode x%X, version x%X (x%X), Size x%X (x%X)\r\n"),
                  dwRetCode,
                  pDevMiniCfg->wVersion, DEVMINCFG_VERSION,
                  dwSize, (MIN_DEVCONFIG_SIZE)));
        // Either the registry value is missing, or it is invalid.
        // We need some default, use the one in ROM
        *pDevMiniCfg = DefaultDevCfg;
    } else {
        ConvertDevConfig(pDevMiniCfg, dwSize, pDevMiniCfg);
    }

    //
    // Read individual field overrides if present
    //
    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"BaudRate", &dwTmp)) {
        pDevMiniCfg->dwBaudRate = dwTmp;
    }

    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"CallSetupFailTimer", &dwTmp)) {
        pDevMiniCfg->dwCallSetupFailTimer = dwTmp;
    }

    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"WaitBong", &dwTmp)) {
        pDevMiniCfg->wWaitBong = (WORD)dwTmp;
    }

    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"ByteSize", &dwTmp)) {
        pDevMiniCfg->ByteSize = (BYTE)dwTmp;
    }

    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"Parity", &dwTmp)) {
        pDevMiniCfg->Parity = (BYTE)dwTmp;
    }

    if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, L"StopBits", &dwTmp)) {
        pDevMiniCfg->StopBits = (BYTE)dwTmp;
    }

    //
    // Modem Options are MDM_* flags from mcx.h
    //
    for (i = 0; ModemOptionFlags[i]; i++) {
        if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, ModemOptionValues[i], &dwTmp)) {
            if (dwTmp) {
                pDevMiniCfg->dwModemOptions |= ModemOptionFlags[i];
            } else {
                pDevMiniCfg->dwModemOptions &= ~ModemOptionFlags[i];
            }
        }
    }

    //
    // Terminal Options are: manual dial, pre-dial terminal, and post-dial terminal
    //
    for (i = 0; TerminalOptionFlags[i]; i++) {
        if (ERROR_SUCCESS == MdmRegGetConfigDWORDValue(ptLineDev, TerminalOptionValues[i], &dwTmp)) {
            if (dwTmp) {
                pDevMiniCfg->fwOptions |= (WORD)TerminalOptionFlags[i];
            } else {
                pDevMiniCfg->fwOptions &= (WORD)(~TerminalOptionFlags[i]);
            }
        }
    }

    dwSize = sizeof(szDialModifier);
    if (ERROR_SUCCESS == MdmRegGetValue(ptLineDev, L"Config", L"DialModifier", REG_SZ, (LPBYTE)szDialModifier, &dwSize)) {
        memcpy(pDevMiniCfg->szDialModifier, szDialModifier, dwSize);
    }

    pDevMiniCfg->szDialModifier[DIAL_MODIFIER_LEN] = 0; // make sure it is null terminated
    pDevMiniCfg->wszDriverName[MAX_NAME_LENGTH] = 0; // make sure it is null terminated

    DEBUGMSG(ZONE_INIT,
             (TEXT("UNIMODEM:Done reading Device Config, retcode x%X\r\n"),
              dwRetCode));
    DEBUGMSG(ZONE_INIT,
             (TEXT("UNIMODEM:Version x%X\tWaitBong x%X\tCall Setup Fail x%X\r\n"),
              pDevMiniCfg->wVersion,
              pDevMiniCfg->wWaitBong,
              pDevMiniCfg->dwCallSetupFailTimer));
    DEBUGMSG(ZONE_INIT,
             (TEXT("UNIMODEM:Baud %d\tByte Size %d\tStop Bits %d\tParity %d\r\n"),
              pDevMiniCfg->dwBaudRate,
              pDevMiniCfg->ByteSize,
              pDevMiniCfg->StopBits,
              pDevMiniCfg->Parity));

    DEBUGMSG(ZONE_FUNC | ZONE_INIT,
             (TEXT("UNIMODEM:-getDefaultDevConfig\r\n")));
}

//
// Create a LineDev
//
PTLINEDEV
createLineDev(
    HKEY    hActiveKey,
    LPCWSTR lpszDevPath,
    LPCWSTR lpszDeviceName
    )
{
    PTLINEDEV ptLineDev;
    PTLINEDEV ptExistingDev;
    static const WCHAR szDeviceType[]   = TEXT("DeviceType");
    static const WCHAR szPortName[]     = TEXT("Port");
    static const WCHAR szFriendlyName[] = TEXT("FriendlyName");
    static const WCHAR szPnpIdName[]    = TEXT("PnpId");
    static const WCHAR szDialBilling[]  = TEXT("DialBilling");
    DWORD dwSize, dwRetCode, dwTemp;
    
    DEBUGMSG(ZONE_FUNC|ZONE_INIT,
             (TEXT("UNIMODEM:+createLineDev\r\n")));

    // We are being asked to create a new linedev - do it
    if (!(ptLineDev = (PTLINEDEV) TSPIAlloc(sizeof(TLINEDEV)) )) {
        return NULL;
    }

     // Initialize all of the fields
    ptLineDev->dwPendingID = INVALID_PENDINGID;
    ptLineDev->dwDefaultMediaModes = LINEMEDIAMODE_DATAMODEM;  // Do this early, NullifyLine uses it
    ptLineDev->bControlThreadRunning = FALSE;
    ptLineDev->dwDeviceID = INVALID_DEVICE;

    // Get the device registry handle for later use.
    dwRetCode = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        lpszDevPath,
        0,
        KEY_READ,
        &ptLineDev->hSettingsKey);
    if (ERROR_SUCCESS != dwRetCode) {
        // We can't very well continue if the registry values aren't there.
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev - RegOpenKeyEx %s returned %d.\r\n"),
                  lpszDevPath, dwRetCode));
        goto abort_buffer_allocated;
    } else {
        DEBUGMSG(ZONE_MISC|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev - RegOpenKeyEx %s handle x%X.\r\n"),
                  lpszDevPath, ptLineDev->hSettingsKey));
    }
    
    NullifyLineDevice( ptLineDev, TRUE );  // also calls getDefaultDevConfig

    // Record the port name, or read it ourselves if NULL
    if( NULL == lpszDeviceName ) {
        // Get the port name from registry
        dwSize = MAXDEVICENAME;
        dwRetCode = MdmRegGetValue(ptLineDev, NULL, szPortName, REG_SZ,
                                   (LPBYTE)ptLineDev->szDeviceName, &dwSize);
        if( dwRetCode != ERROR_SUCCESS ) {
            // We can't do much without a port, abort now.
            goto abort_registry_opened;
        }
    } else {
        wcscpy( ptLineDev->szDeviceName, lpszDeviceName );
    }
    
    // Get the device type from registry
    dwSize = sizeof( DWORD );
    dwRetCode = MdmRegGetValue(ptLineDev, NULL, szDeviceType, REG_DWORD,
                               (LPBYTE)&dwTemp, &dwSize);
    if( dwRetCode == ERROR_SUCCESS ) {
        DEBUGMSG(ZONE_MISC|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev - Device type x%X.\r\n"), dwTemp));
        
        ptLineDev->wDeviceType = (WORD)dwTemp;
    } else {    
        DEBUGMSG(ZONE_MISC|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev - Device type defaulting to NULL_MODEM.\r\n")));        
        ptLineDev->wDeviceType = DT_NULL_MODEM;  // default type is null
    }
    
    //
    // Get modem dial capabilities
    //
    ptLineDev->dwDevCapFlags =  LINEDEVCAPFLAGS_DIALQUIET |
                                LINEDEVCAPFLAGS_DIALDIALTONE |
                                LINEDEVCAPFLAGS_DIALBILLING;
        
    //
    // Assume dial billing capability (wait for dial, '$' in a dial string)
    //
    dwSize = sizeof(DWORD);
    if (ERROR_SUCCESS == MdmRegGetValue(ptLineDev, szSettings, szDialBilling,
                                    REG_DWORD, (PUCHAR)&dwTemp, &dwSize) ) {
        if (dwTemp == 0) {
            ptLineDev->dwDevCapFlags &= ~LINEDEVCAPFLAGS_DIALBILLING;
        }
    }

    MdmRegGetLineValues(ptLineDev);

    // Get the friendly name from registry
    dwSize = MAXDEVICENAME;
    dwRetCode = MdmRegGetValue(ptLineDev, NULL, szFriendlyName, REG_SZ,
                               (LPBYTE)ptLineDev->szFriendlyName, &dwSize);
    if( dwRetCode != ERROR_SUCCESS ) { // Friendly not found, use something else
            // Non removable & no friendly name, just use the port name
            wcsncpy( ptLineDev->szFriendlyName, ptLineDev->szDeviceName, MAXDEVICENAME );
    }
    
    DEBUGMSG(ZONE_INIT,
             (TEXT("UNIMODEM:Done reading friendly name, retcode x%X, name %s\r\n"),
              dwRetCode, ptLineDev->szFriendlyName));

    //----------------------------------------------------------------------
    // OK, now that we have all these names, lets see if this same device
    // already exists (it may be a removable device that was removed and then
    // reinserted).  If it exists, then just reuse the existing entry and
    // let TAPI know that the device reconnected.
    //----------------------------------------------------------------------
    if( ptExistingDev = LineExists( ptLineDev ) ) {
        // We already know about this device
        ptExistingDev->hSettingsKey = ptLineDev->hSettingsKey;  // Get new handle
        
        TSPIFree( ptLineDev );  // Free the entry we started to create.

        // If card is reiniserted, mark it as available.
        ptExistingDev->wDeviceAvail = 1;

        CallLineEventProc(
            ptExistingDev,
            0,
            LINE_LINEDEVSTATE,
            LINEDEVSTATE_CONNECTED|LINEDEVSTATE_INSERVICE,
            0,
            0);
        
        return ptExistingDev;   // And return the original device
    } else {
        // This really is a new device.  Create whatever all we need.
        
        // If we are creating a device, it must be available.
        ptLineDev->wDeviceAvail = 1;

        // Create an event for timeouts, and one for call completion
        ptLineDev->hTimeoutEvent = CreateEvent(0,FALSE,FALSE,NULL);
        ptLineDev->hCallComplete = CreateEvent(0,FALSE,FALSE,NULL);

        // Close needs a critical section
        InitializeCriticalSection(&ptLineDev->OpenCS);
                                  
        if( IS_NULL_MODEM(ptLineDev)  )
            ptLineDev->dwBearerModes = LINEBEARERMODE_DATA | LINEBEARERMODE_PASSTHROUGH;
        else
            ptLineDev->dwBearerModes = LINEBEARERMODE_DATA | LINEBEARERMODE_VOICE | LINEBEARERMODE_PASSTHROUGH;
        
        // And add this new line device to list
        InsertHeadLockedList(&TspiGlobals.LineDevs,
                             &ptLineDev->llist,
                             &TspiGlobals.LineDevsCS);
        DEBUGMSG(ZONE_ALLOC, (TEXT("UNIMODEM:Insert Line Device in List\r\n")));

        // OK, lets notify TAPI about our new device
        DEBUGMSG(ZONE_FUNCTION|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev, calling TAPI Line Create Proc\r\n")));

        // Let TAPI know about our new device
        CallLineEventProc(NULL, NULL, LINE_CREATE,
                        (DWORD)TspiGlobals.hProvider,
                        (DWORD)&ptLineDev->dwDeviceID, 0L);

        DEBUGMSG(ZONE_FUNCTION|ZONE_INIT,
                 (TEXT("UNIMODEM:createLineDev, TAPI assigned device ID 0x%X\r\n"), ptLineDev->dwDeviceID));
    
        DEBUGMSG(ZONE_FUNC|ZONE_INIT,
                 (TEXT("UNIMODEM:-createLineDev\r\n")));
    
        return ptLineDev;
    }
    
abort_registry_opened:
    RegCloseKey( ptLineDev->hSettingsKey );
abort_buffer_allocated:
    TSPIFree( ptLineDev );

    DEBUGMSG(ZONE_FUNC|ZONE_INIT,
             (TEXT("UNIMODEM:-createLineDev, aborted\r\n")));
    return NULL;
}

// ****************************************************************************
// GetLineDevfromID()
//
// Function: This function looks for the LineDev associated with an ID
//
// Returns:  PLINEDEV pointer (or NULL if not found)
//       
// ****************************************************************************
PTLINEDEV
GetLineDevfromID(
    DWORD dwDeviceID
    )
{
    PLIST_ENTRY ptEntry;  
    PTLINEDEV ptLineDev;

    DEBUGMSG(ZONE_LIST,
             (TEXT("UNIMODEM:+GetLineDevfromID\r\n")));

     // Walk the Line List and find corresponding handle
    ptEntry = TspiGlobals.LineDevs.Flink;
    while( ptEntry != &TspiGlobals.LineDevs )
    {
        ptLineDev = CONTAINING_RECORD( ptEntry, TLINEDEV, llist);
        
        DEBUGMSG(ZONE_LIST,
                 (TEXT("UNIMODEM:GetLineDevfromID ptLineDev->dwDeviceID x%X, dwID x%X\r\n"),
                  ptLineDev->dwDeviceID, dwDeviceID));
         // See if this is the correct Line
        if( ptLineDev->dwDeviceID == dwDeviceID )
        {
            DEBUGMSG(ZONE_LIST,
                     (TEXT("UNIMODEM:-GetLineDevfromID - Success\r\n")));
            return ptLineDev;  // Cool, we found it
        }

         // No match yet, advance to next link
        ptEntry = ptEntry->Flink;
    }
    
    DEBUGMSG(ZONE_LIST,
             (TEXT("UNIMODEM:-GetLineDevfromID - Fail\r\n")));
    return NULL;
}


PTLINEDEV
GetLineDevfromName(
    LPCWSTR lpszDeviceName,
    LPCWSTR lpszFriendlyName
    )
{
    PLIST_ENTRY ptEntry;  
    PTLINEDEV ptLineDev;

    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:+GetLineDevfromName\r\n")));

     // Walk the Line List and find corresponding handle
    ptEntry = TspiGlobals.LineDevs.Flink;
    while( ptEntry != &TspiGlobals.LineDevs )
    {
        ptLineDev = CONTAINING_RECORD( ptEntry, TLINEDEV, llist);

         // See if this is the correct Line
        if (lpszDeviceName && lpszFriendlyName) {
            if (!wcscmp(lpszDeviceName, ptLineDev->szDeviceName)) {
                if (!wcscmp(lpszFriendlyName, ptLineDev->szFriendlyName)) {
                    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-GetLineDevfromName - Success both\r\n")));
                    return ptLineDev;
                }
            }
        } else if (lpszDeviceName) {
            //
            // When going by device name, only look at active devices since the removal code
            // is calling us and there may be multiple line devices on the same COMn: device.
            //
            if (ptLineDev->wDeviceAvail) {
                if (!wcscmp(lpszDeviceName, ptLineDev->szDeviceName)) {
                    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-GetLineDevfromName - Success device name\r\n")));
                    return ptLineDev;
                }
            }
        } else if (lpszFriendlyName) {
            if (!wcscmp(lpszFriendlyName, ptLineDev->szFriendlyName)) {
                DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-GetLineDevfromName - Success friendly name\r\n")));
                return ptLineDev;
            }
        }

        // Advance to next link.
        ptEntry = ptEntry->Flink;
    }
    
    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-GetLineDevfromName - Fail\r\n")));
    return NULL;
}   // GetLineDevfromName

// ****************************************************************************
//
// Function: This function looks for the LineDev which matches the names and
//           device types of the passed in LineDev.  This is so that we can
//           determine if a lineCreate is really just the reinsertion of a
//           removable device that we already know about.
//
// Returns:  PLINEDEV pointer (or NULL if not found)
//       
// ****************************************************************************
PTLINEDEV
LineExists(
    PTLINEDEV ptNewLine
    )
{
    PLIST_ENTRY ptEntry;  
    PTLINEDEV ptOldLine;

    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:+LineExists - Device type x%X\r\n"), ptNewLine->wDeviceType));
    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists\tDevice Name : %s\r\n"), ptNewLine->szDeviceName));
    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists\tFriendly Name : %s\r\n"), ptNewLine->szFriendlyName));

     // Walk the Line List and find corresponding handle
    ptEntry = TspiGlobals.LineDevs.Flink;
    while( ptEntry != &TspiGlobals.LineDevs )
    {
        ptOldLine = CONTAINING_RECORD( ptEntry, TLINEDEV, llist);
        
        DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists - checking old dwDeviceID x%X\r\n"), ptOldLine->dwDeviceID));
        DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists\tDevice Type : %x\r\n"), ptOldLine->wDeviceType));
        DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists\tDevice Name : %s\r\n"), ptOldLine->szDeviceName));
        DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:LineExists\tFriendly Name : %s\r\n"), ptOldLine->szFriendlyName));

        // See if this is the correct Line
        if( (ptOldLine->wDeviceType == ptNewLine->wDeviceType) &&
            (! wcscmp(ptOldLine->szDeviceName, ptNewLine->szDeviceName)) &&
            (! wcscmp(ptOldLine->szFriendlyName, ptNewLine->szFriendlyName)) )
        {
            DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-LineExists - previous was ID x%X\r\n"), ptOldLine->dwDeviceID ));
            return ptOldLine;  // this card has been reinserted
        }

         // No match yet, advance to next link
        ptEntry = ptEntry->Flink;
    }
    
    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-LineExists - Fail\r\n")));
    return NULL;
}


// ****************************************************************************
// GetLineDevfromHandle()
//
// Function: This function gets the Line Dev Pointer from a handle
//
// Returns:  a pointer to PLINEDEV structure if the handle is valid, or
//           NULL otherwise
//
// ****************************************************************************

PTLINEDEV
GetLineDevfromHandle (
    DWORD handle
    )
{

    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:+GetLineDevfromHandle\r\n")));
     // The handle IS the pointer.
     // However in the future we may need to provide some abstraction (as well
     // potential validity checks) so this function prepares for that, 
    
    DEBUGMSG(ZONE_LIST, (TEXT("UNIMODEM:-GetLineDevfromHandle\r\n")));
    return (PTLINEDEV)handle;
}

//
// A NULL modem uses the "CLIENT"/"CLIENTSERVER" command set instead of Hayes modem commands
//
BOOL
IsNullModem(
    PTLINEDEV ptLineDev
    )
{
    switch (ptLineDev->wDeviceType) {
    case DT_NULL_MODEM:
    case DT_IRCOMM_MODEM:
    case DT_DYNAMIC_PORT:
        return TRUE;
    }

    return FALSE;
}

//
// A dynamic device is one where unimodem calls ActivateDeviceEx to create it at TSPI_lineOpen
// and DeactivateDevice to remove it at TSPI_lineClose
//
BOOL
IsDynamicDevice(
    PTLINEDEV ptLineDev
    )
{
    switch (ptLineDev->wDeviceType) {
    case DT_DYNAMIC_MODEM:
    case DT_DYNAMIC_PORT:
        return TRUE;
    }

    return FALSE;
}

//
// 
void
InitVarData(
    LPVOID lpData,
    DWORD  dwSize
    )
{
    DWORD temp = 0;
    LPVARSTRING pVarData = (LPVARSTRING)lpData;     // Cast the pointer to a varstring

    if (pVarData->dwTotalSize - sizeof(DWORD) > pVarData->dwTotalSize){ //no overflow
        return ;
    }

    temp = pVarData->dwTotalSize - sizeof(DWORD);
    memset(&(pVarData->dwNeededSize), 0, temp);
    pVarData->dwNeededSize = dwSize;
    pVarData->dwUsedSize = dwSize;
}



//
// This routine is called when the TSPI DLL is loaded.  It is primarily
// responsible for initializing TspiGlobals.
//
// Return : void 
//
void
TSPIDLL_Load(
    void
    )
{
    DWORD dwRet;
    
    DEBUGMSG(ZONE_FUNCTION|ZONE_INIT,
             (TEXT("+TSPIDLL_Load\r\n")));

    TspiGlobals.hInstance = 0;
    TspiGlobals.dwProviderID = 0;
    TspiGlobals.hProvider = 0;
    TspiGlobals.fnLineEventProc =0 ;
    TspiGlobals.fnCompletionCallback = 0;

     // Let's init our linked lists to empty
    InitializeListHead( &TspiGlobals.LineDevs );

     // And init the linked list critical sections
    InitializeCriticalSection( &TspiGlobals.LineDevsCS );

    // Go ahead and open the unimodem defaults registry key. 
    dwRet = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        szDefaultsName,
        0,
        KEY_READ,
        &TspiGlobals.hDefaultsKey);
    if (ERROR_SUCCESS != dwRet)
    {
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
                 (TEXT("TSPIDLL_LOAD - RegOpenKeyEx %s returned %d.\r\n"),
                  szDefaultsName, dwRet));
    }
    else
    {
        DWORD dwValType;
        DWORD dwValLen;
    
        DEBUGMSG(ZONE_MISC|ZONE_INIT,
                 (TEXT("TSPIDLL_LOAD - RegOpenKeyEx %s handle x%X.\r\n"),
                  szDefaultsName, TspiGlobals.hDefaultsKey));

        // Get thread priority
        g_dwUnimodemThreadPriority = DEFAULT_THREAD_PRIORITY;

        dwValLen = sizeof(DWORD);
        RegQueryValueEx(
            TspiGlobals.hDefaultsKey,
            REGISTRY_PRIORITY_VALUE,
            NULL,
            &dwValType,
            (PUCHAR)&g_dwUnimodemThreadPriority,
            &dwValLen);

        DEBUGMSG(ZONE_MISC|ZONE_INIT,
                 (TEXT("TSPIDLL_LOAD - Priority %d\r\n"),
                  g_dwUnimodemThreadPriority));
    }

    g_hCoreDLL = LoadLibrary(L"coredll.dll");

    DEBUGMSG(ZONE_FUNCTION|ZONE_INIT,
             (TEXT("-TSPIDLL_Load\r\n")));
}

//****************************************************************************
//
//  NullifyLineDevice()
//
//****************************************************************************
DWORD
NullifyLineDevice (
    PTLINEDEV pLineDev,
    BOOL bDefaultConfig
    )
{
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+NullifyLineDevice\r\n")));

     // Turn the line device back to its initial state
    pLineDev->hDevice      = (HANDLE)INVALID_DEVICE;
    pLineDev->hDevice_r0   = (HANDLE)INVALID_DEVICE;
    pLineDev->hMdmLog      = (HANDLE)INVALID_DEVICE;
    pLineDev->pidDevice    = 0L;
    pLineDev->lpfnEvent    = NULL;
    pLineDev->DevState     = DEVST_DISCONNECTED;
    pLineDev->hwndLine     = NULL;
    pLineDev->szAddress[0] = '\0';
#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
    pLineDev->hwTermCtrl   = NULL;
#endif
    pLineDev->dwCallFlags  = 0L;
    pLineDev->dwCallState  = LINECALLSTATE_IDLE;
    pLineDev->dwCurMediaModes = 0L;
    pLineDev->dwDetMediaModes = 0L;
    pLineDev->fTakeoverMode   = FALSE;
    pLineDev->dwMediaModes    = pLineDev->dwDefaultMediaModes;
    pLineDev->dwDialOptions = 0L;
    pLineDev->dwNumRings    = 0;
    if (bDefaultConfig) {
        getDefaultDevConfig( pLineDev, &pLineDev->DevMiniCfg ); // reset the devconfig
    }
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-NullifyLineDevice\r\n")));

    return SUCCESS;
}


//****************************************************************************
//
//  ValidateDevCfgClass()
//
//****************************************************************************

BOOL
ValidateDevCfgClass (
    LPCWSTR lpszDeviceClass
    )
{
    UINT        idClass;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ValidateDevCfgClass\r\n")));

     // Need the device class
    if (lpszDeviceClass == NULL)
        return FALSE;

     // Determine the device class
     //
    for (idClass = 0; idClass < MAX_SUPPORT_CLASS; idClass++)
    {
        if (lstrcmpi(lpszDeviceClass, aGetID[idClass].szClassName) == 0)
            break;
    };

     // Do we support the requested class?
    switch (idClass)
    {
        case TAPILINE:
        case COMM:
        case COMMMODEM:
            DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ValidateDevCfgClass (Valid)\r\n")));
            return TRUE;

        default:
            DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ValidateDevCfgClass (Invalid)\r\n")));
            return FALSE;
    };
}

//****************************************************************************
// ValidateAddress()
//
// Function: This function validates a tapi address and creates a version of
//           it to pass to the VxD.
//
// Returns:  SUCCESS or LINEERR_xxx depending on the failure reason
//
//****************************************************************************

LONG
ValidateAddress(
    PTLINEDEV pLineDev,
    LPCWSTR   lpszInAddress,
    LPWSTR    lpszOutAddress)
{
    LPCWSTR lpszSrc;
    int cbOutLen = MAXADDRESSLEN;
    WCHAR chContinuation = pLineDev->chContinuation;

    if (NULL == lpszOutAddress) {
        return LINEERR_INVALPARAM;
    }

     // is lpszInAddress NULL?
    if (lpszInAddress == NULL || *lpszInAddress == 0)
    {
        *lpszOutAddress = 0;
        return SUCCESS;
    }

     // Verify that the first char is a valid single-byte char.
    if (AnsiNext(lpszInAddress) - lpszInAddress != 1)
    {
        return LINEERR_INVALADDRESS;
    }

     // tone or pulse?  set dwDialOptions appropriately
     // also, set lpszSrc
    switch (*lpszInAddress) {
    case 'T':   // tone
    case 't':
        lpszSrc = lpszInAddress + 1;
        pLineDev->dwDialOptions |= MDM_TONE_DIAL;
        break;

    case 'P':   // pulse
    case 'p':
        lpszSrc = lpszInAddress + 1;
        pLineDev->dwDialOptions &= ~MDM_TONE_DIAL;
        break;

    default:
        lpszSrc = lpszInAddress;
        pLineDev->dwDialOptions |= MDM_TONE_DIAL;
        break;
    }

     // copy In to Out scanning for various dialoptions, returning error if we
     // don't support something.
    while (*lpszSrc && cbOutLen)
    {
        switch (*lpszSrc)
        {
            case '$':
                if (!(pLineDev->dwDevCapFlags & LINEDEVCAPFLAGS_DIALBILLING))
                {
                    UINT  cCommas;

                     // Get the wait-for-dial period
                    cCommas = pLineDev->DevMiniCfg.wWaitBong;

                     // Calculate the number of commas we need to insert
                    cCommas = (cCommas/INC_WAIT_BONG) +
                        (cCommas%INC_WAIT_BONG ? 1 : 0);

                     // Insert the strings of commas
                    while (cbOutLen && cCommas)
                    {
                        *lpszOutAddress++ = ',';
                        cbOutLen--;
                        cCommas--;
                    };
                    goto Skip_This_Character;
                }
                break;

            case '@':
                if (!(pLineDev->dwDevCapFlags & LINEDEVCAPFLAGS_DIALQUIET))
                {
                    return LINEERR_DIALQUIET;
                }
                break;

            case 'W':
            case 'w':
                if (!(pLineDev->dwDevCapFlags & LINEDEVCAPFLAGS_DIALDIALTONE))
                {
                    return LINEERR_DIALDIALTONE;
                }
                break;

            case '?':
                return LINEERR_DIALPROMPT;

            case '|':  // subaddress
            case '^':  // name field
                goto Skip_The_Rest;

            case ' ':
            case '-':
                 // skip these characters
                goto Skip_This_Character;
        }

        if (*lpszSrc == chContinuation) {
             // This signifies the end of a dialable address.
             // Use it and skip the rest.
            *lpszOutAddress++ = *lpszSrc;
            goto Skip_The_Rest;
        }

         // Copy this character
        *lpszOutAddress++ = *lpszSrc;
        cbOutLen--;

Skip_This_Character:
         // Verify that the next char is a valid single-byte char.
        if (AnsiNext(lpszSrc) - lpszSrc != 1)
        {
            return LINEERR_INVALADDRESS;
        }
        lpszSrc++;
    }

     // Did we run out of space in the outgoing buffer?
    if (*lpszSrc && cbOutLen == 0)
    {
         // yes
        DEBUGMSG(ZONE_ERROR,
                 (TEXT("\t*** Address too large.\r\n")));
        return LINEERR_INVALADDRESS;
    }

Skip_The_Rest:
    *lpszOutAddress = 0;
    return SUCCESS;
}


//
// Call TAPI's Line Event Proc with status message filtering
//
void
CallLineEventProc(
    PTLINEDEV   ptLine,
    HTAPICALL   htCall,
    DWORD       dwMsg,
    DWORD       dwParam1,
    DWORD       dwParam2,
    DWORD       dwParam3
    )
{
    HTAPILINE htLine;

    htLine = (ptLine) ? ptLine->htLine : NULL;

    if (ptLine && (dwMsg == LINE_LINEDEVSTATE)) {
        //
        // Honor the lineSetStatusMessages request
        //
        if ((dwParam1 & ptLine->dwLineStatesMask) == 0) {
            return;
        }
    }

    TspiGlobals.fnLineEventProc(
                    htLine,
                    htCall,
                    dwMsg,
                    dwParam1,
                    dwParam2,
                    dwParam3
                    );
}   // CallLineEventProc

#ifdef DEBUG
LPWSTR
CallStateName(
    DWORD dwCallState
    )
{
    LPWSTR lpszRet;

    switch (dwCallState) {
    case LINECALLSTATE_IDLE:                lpszRet = L"LINECALLSTATE_IDLE";                break;
    case LINECALLSTATE_OFFERING:            lpszRet = L"LINECALLSTATE_OFFERING";            break;
    case LINECALLSTATE_ACCEPTED:            lpszRet = L"LINECALLSTATE_ACCEPTED";            break;
    case LINECALLSTATE_DIALTONE:            lpszRet = L"LINECALLSTATE_DIALTONE";            break;
    case LINECALLSTATE_DIALING:             lpszRet = L"LINECALLSTATE_DIALING";             break;
    case LINECALLSTATE_RINGBACK:            lpszRet = L"LINECALLSTATE_RINGBACK";            break;
    case LINECALLSTATE_BUSY:                lpszRet = L"LINECALLSTATE_BUSY";                break;
    case LINECALLSTATE_SPECIALINFO:         lpszRet = L"LINECALLSTATE_SPECIALINFO";         break;
    case LINECALLSTATE_CONNECTED:           lpszRet = L"LINECALLSTATE_CONNECTED";           break;
    case LINECALLSTATE_PROCEEDING:          lpszRet = L"LINECALLSTATE_PROCEEDING";          break;
    case LINECALLSTATE_ONHOLD:              lpszRet = L"LINECALLSTATE_ONHOLD";              break;
    case LINECALLSTATE_CONFERENCED:         lpszRet = L"LINECALLSTATE_CONFERENCED";         break;
    case LINECALLSTATE_ONHOLDPENDCONF:      lpszRet = L"LINECALLSTATE_ONHOLDPENDCONF";      break;
    case LINECALLSTATE_ONHOLDPENDTRANSFER:  lpszRet = L"LINECALLSTATE_ONHOLDPENDTRANSFER";  break;
    case LINECALLSTATE_DISCONNECTED:        lpszRet = L"LINECALLSTATE_DISCONNECTED";        break;
    case LINECALLSTATE_UNKNOWN:             lpszRet = L"LINECALLSTATE_UNKNOWN";             break;
    default: lpszRet = L"UNDEFINED!!!"; break;
    }
    return lpszRet;
}
#endif

void
NewCallState(
    PTLINEDEV pLineDev,
    DWORD dwCallState,
    DWORD dwCallState2
    )
{
    DEBUGMSG(ZONE_CALLS, (L"UNIMODEM:NewCallState for Device %d = %s\n", pLineDev->dwDeviceID, CallStateName(dwCallState)));

    pLineDev->dwCallState = dwCallState;
    pLineDev->dwCallStateMode = dwCallState2;
    CallLineEventProc(
        pLineDev,
        pLineDev->htCall,
        LINE_CALLSTATE,
        dwCallState,
        dwCallState2,
        pLineDev->dwCurMediaModes
        );
}   // NewCallState


#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
typedef int (WINAPI * PFN_LOADSTRING)(HINSTANCE,UINT,LPWSTR,int);

//
// Wrapper to load the termctrl DLL and then call TerminalWindow
//
BOOL
DisplayTerminalWindow(
    PTLINEDEV pLineDev,
    DWORD dwTitle
)
{
    PFNTERMINALWINDOW pfnTerminalWindow;
    HANDLE hTermCtrl;
    WCHAR  szWindowTitle[128];
    BOOL bRet;
    PFN_LOADSTRING pfnLoadString;

    bRet = FALSE;
    
    // Load the termctrl dll, and get the entrypoint
    if ((hTermCtrl = LoadLibrary(TEXT("termctrl.dll"))) == NULL) {
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
            (TEXT("UNIMODEM:DisplayTerminalWindow *** Unable to load termctrl dll\r\n")));
        goto dtw_exit;
    }

    // OK, lets get the function pointer.
    if ((pfnTerminalWindow = (PFNTERMINALWINDOW)GetProcAddress(hTermCtrl, TEXT("TerminalWindow"))) == NULL) {
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
            (TEXT("UNIMODEM:DisplayTerminalWindow *** GetProcAddress(TerminalWindow) failed\r\n")));
        goto dtw_exit;
    }

    if ((pfnLoadString = (PFN_LOADSTRING)GetProcAddress(g_hCoreDLL, L"LoadStringW")) == NULL) {
        DEBUGMSG(ZONE_ERROR|ZONE_INIT,
            (TEXT("UNIMODEM:DisplayTerminalWindow *** GetProcAddress(LoadStringW) failed\r\n")));
        goto dtw_exit;
    }

    pfnLoadString(TspiGlobals.hInstance, dwTitle, szWindowTitle, sizeof(szWindowTitle)/sizeof(WCHAR));

    //
    // Make UI run in non-realtime thread priority
    //
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    pLineDev->hwTermCtrl = NULL;  // make sure handle is NULL before calling
    if (TW_SUCCESS == pfnTerminalWindow(pLineDev->hDevice, szWindowTitle, &pLineDev->hwTermCtrl)) {
        bRet = TRUE;
    }
    pLineDev->hwTermCtrl = NULL;  // Zero Window handle now that we are done

    CeSetThreadPriority(GetCurrentThread(), g_dwUnimodemThreadPriority);
    
    
dtw_exit:
    if(hTermCtrl) {
        FreeLibrary(hTermCtrl);
    }
    return bRet;
    
}
#endif
