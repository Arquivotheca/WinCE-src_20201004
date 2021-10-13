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
// ****************************************************************************
//
//  Module:     Unimdm
//  File:       modem.c
//
//
//  Revision History
//
//  Description: Intermediate modem SPI layer
//
// ****************************************************************************

#include "windows.h"
#include "types.h"
#include "memory.h"
#include "tspi.h"
#include "linklist.h"
#include "tspip.h"
#include "tapicomn.h"
#include "mcx.h"
#include "dial.h"

static const WCHAR szAnswer[] =   TEXT("Answer");   // Registry value name under Settings
static const CHAR szDefaultAnswerCmd[] = "ATA\r\n";
static const WCHAR szAnswerTimeout[] =   TEXT("AnswerTimeout");
static const WCHAR szMonitor[] =   TEXT("Monitor"); // Registry value name under Settings
static const CHAR szDefaultMonitorCmd[] = "ATS0=0\r\n";
static const WCHAR szMdmLogBase[] =   TEXT("mdmlog");
static const WCHAR szMdmLogExt[] =   TEXT(".txt");
extern const WCHAR szSettings[];

extern DWORD SetWatchdog(PTLINEDEV pLineDev,DWORD dwTimeout);
LONG ProcessModemFailure(PTLINEDEV pLineDev);

// Unimodem thread priority
extern DWORD g_dwUnimodemThreadPriority;
extern HANDLE g_hCoreDLL;

#ifdef DEBUG
LPWSTR
GetPendingName(
    DWORD dwPendingType
    )
{
    LPWSTR lpszType;
    
    switch (dwPendingType) {
    case PENDING_LINEACCEPT:        lpszType = TEXT("LINEACCEPT");          break;
    case PENDING_LINEANSWER:        lpszType = TEXT("LINEANSWER");          break;
    case PENDING_LINEDEVSPECIFIC:   lpszType = TEXT("LINEDEVSPECIFIC");     break;
    case PENDING_LINEDIAL:          lpszType = TEXT("LINEDIAL");            break;
    case PENDING_LINEDROP:          lpszType = TEXT("LINEDROP");            break;
    case PENDING_LINEMAKECALL:      lpszType = TEXT("LINEMAKECALL");        break;
    case PENDING_LISTEN:            lpszType = TEXT("LISTEN");              break;
    case PENDING_EXIT:              lpszType = TEXT("EXIT");                break;
    case INVALID_PENDINGOP:         lpszType = TEXT("INVALIDOP");           break;
    default:                        lpszType = TEXT("UNKNOWN!!!");          break;
    }
    
    return lpszType;
}
#endif

#define MDMLOGNAME_CHARS 32

//
// Function to open the modem command log file for the specified line device
//
void
OpenModemLog(
    PTLINEDEV pLineDev
    )
{
    WCHAR FileName[MDMLOGNAME_CHARS];
    HANDLE hFile;

    if (pLineDev->bMdmLogFile == FALSE) {
        return;
    }

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+OpenModemLog\n")));

    if (pLineDev->hMdmLog != (HANDLE)INVALID_DEVICE) {
        CloseHandle(pLineDev->hMdmLog);
    }

    //
    // Format modem log file name "mdmlog<dwDeviceID>.txt"
    //
    StringCchPrintfW(FileName, MDMLOGNAME_CHARS, L"%s%d.%s", szMdmLogBase, pLineDev->dwDeviceID, szMdmLogExt);

    hFile = CreateFile(FileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DEBUGMSG(ZONE_CALLS|ZONE_FUNC|ZONE_ERROR,
            (TEXT("UNIMODEM:OpenModemLog CreateFile(%s) failed %d\n"), FileName, GetLastError()));
        pLineDev->hMdmLog = (HANDLE)INVALID_DEVICE;
    } else {
        DEBUGMSG(ZONE_CALLS|ZONE_FUNC, (TEXT("UNIMODEM:OpenModemLog CreateFile(%s) succeeded\n"), FileName));
        pLineDev->hMdmLog = hFile;
    }

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-OpenModemLog\n")));
} // OpenModemLog


//
// Function to write modem commands to the line device's command history file.
// 
void
WriteModemLog(
    PTLINEDEV pLineDev,
    UCHAR * szCommand,
    DWORD   dwOp
    )
{
    CHAR Buf[MAXSTRINGLENGTH];
    LPSTR  lpsz;
    DWORD  dwLen;
    DWORD  dwIO;

    if (pLineDev->bMdmLogFile == FALSE) {
        return;
    }

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+WriteModemLog %a\n"), szCommand));

    switch (dwOp) {
    case MDMLOG_COMMAND_OK:
        lpsz = "Modem Command:   ";
        break;
    case MDMLOG_COMMAND_FAIL:
        lpsz = "Failed Command:  ";
        break;
    case MDMLOG_RESPONSE:
        lpsz = "Modem Response:  ";
        break;

    default:
        lpsz = "                 ";
        break;
    }

    Buf[MAXSTRINGLENGTH-2] = 0;
    strcpy(Buf, lpsz);
    dwLen = strlen(Buf);
    strncat(Buf, szCommand, MAXSTRINGLENGTH - dwLen - 2);
    dwLen += strlen(szCommand);
    if (dwLen > (MAXSTRINGLENGTH - 2)) {
        dwLen = MAXSTRINGLENGTH - 2;
    }
    Buf[dwLen] = '\n';
    dwLen++;
    Buf[dwLen] = '\0';

    DEBUGMSG(ZONE_CALLS|ZONE_FUNC, (TEXT("UNIMODEM:WriteModemLog(%a) %d bytes\n"), Buf, dwLen));
    if (!WriteFile(pLineDev->hMdmLog, (PUCHAR)Buf, dwLen, &dwIO, NULL)) {
        DEBUGMSG(ZONE_CALLS|ZONE_FUNC, (TEXT("UNIMODEM:WriteModemLog WriteFile() failed %d\n"), GetLastError()));
    }
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-WriteModemLog\n")));
}   // WriteModemLog


void
SetDCBfromDevMiniCfg(
    DCB * pDCB,
    PDEVMINICFG lpDevMiniCfg
    )
{
    pDCB->BaudRate = lpDevMiniCfg->dwBaudRate;
    pDCB->ByteSize = lpDevMiniCfg->ByteSize;
    pDCB->StopBits = lpDevMiniCfg->StopBits;
    pDCB->fParity = (NOPARITY == lpDevMiniCfg->Parity) ? FALSE : TRUE;
    pDCB->Parity   = lpDevMiniCfg->Parity;
    pDCB->fDsrSensitivity = FALSE;
    pDCB->fDtrControl     = DTR_CONTROL_ENABLE;
    
    if( MDM_FLOWCONTROL_HARD & lpDevMiniCfg->dwModemOptions ) {
        // Enable RTS/CTS Flow Control
        pDCB->fRtsControl = RTS_CONTROL_HANDSHAKE;
        pDCB->fOutxCtsFlow = 1;
        pDCB->fOutX = 0;
        pDCB->fInX = 0;
    } else if( MDM_FLOWCONTROL_SOFT & lpDevMiniCfg->dwModemOptions ) {
        // Enable XON/XOFF Flow Control
        pDCB->fRtsControl = RTS_CONTROL_ENABLE;
        pDCB->fOutxCtsFlow = 0;
        pDCB->fOutX = 1;
        pDCB->fInX  = 1;  
    } else {
        pDCB->fRtsControl = RTS_CONTROL_ENABLE;
        pDCB->fOutxCtsFlow = 0;
        pDCB->fOutX = 0;
        pDCB->fInX  = 0;
    }
}   // SetDCBfromDevMiniCfg


void
SetDialerTimeouts(
    PTLINEDEV pLineDev
    )
{
    COMMTIMEOUTS commTimeouts;

    switch (pLineDev->DevMiniCfg.dwBaudRate) {
    //
    // These millisecond timeouts are the expected inter-character delay for
    // the various baud rates + 50%. Previously, the fixed "read" timeouts
    // were too short for low baud rates so the connection attempt would
    // get aborted.
    //
    case 110:   commTimeouts.ReadTotalTimeoutMultiplier = 73+36; break;
    case 300:   commTimeouts.ReadTotalTimeoutMultiplier = 27+14; break;
    case 600:   commTimeouts.ReadTotalTimeoutMultiplier = 14+7; break;
    case 1200:  commTimeouts.ReadTotalTimeoutMultiplier = 7+3;  break;
    case 4800:  commTimeouts.ReadTotalTimeoutMultiplier = 4+2;  break;
    case 9600:  commTimeouts.ReadTotalTimeoutMultiplier = 2+1;  break;
    default:    commTimeouts.ReadTotalTimeoutMultiplier = 1;  break;
    }

    commTimeouts.ReadIntervalTimeout = 50;
    commTimeouts.ReadTotalTimeoutConstant = 50;

    commTimeouts.WriteTotalTimeoutMultiplier = 5;
    commTimeouts.WriteTotalTimeoutConstant = 500;
    SetCommTimeouts(pLineDev->hDevice, &commTimeouts);
}   // SetDialerTimeouts

DWORD
OpenModem (
    PTLINEDEV pLineDev
    )
{
    HANDLE hComDev;
    HANDLE hComDev0;

    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:+OpenModem('%s')\r\n"), pLineDev->szDeviceName));

     // Open the device getting a full access handle and a monitor handle
     //
    if ((hComDev = CreateFile(pLineDev->szDeviceName, GENERIC_READ | GENERIC_WRITE, 0,
                        NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
        goto om_fail;
    }
    if ((hComDev0 = CreateFile(pLineDev->szDeviceName, 0, 0,
                        NULL, OPEN_EXISTING, 0, 0)) != INVALID_HANDLE_VALUE) {
        DCB          commDCB;

        DEBUGMSG(ZONE_MISC|ZONE_CALLS,
                 (TEXT("UNIMODEM:OpenModem - createfile OK, handle x%X\n"),
                  hComDev));
        
        // ********************************************************
        // Set the device configuration based on DEVMINICFG
        //
        GetCommState( hComDev, &commDCB );
        SetDCBfromDevMiniCfg(&commDCB, &pLineDev->DevMiniCfg);

        DEBUGMSG(ZONE_FUNCTION|ZONE_CALLS,
                 (TEXT("UNIMODEM:OpenModem Setting port configuration :\n")));
        DEBUGMSG(ZONE_FUNCTION|ZONE_CALLS,
                 (TEXT("UNIMODEM:OpenModem Baud %d, Byte Size %d, Stop bits %d, Parity %d\n"),
                  commDCB.BaudRate, commDCB.ByteSize, commDCB.StopBits, commDCB.Parity));
        DEBUGMSG(ZONE_FUNCTION|ZONE_CALLS,
                 (TEXT("UNIMODEM:OpenModem RTS Control %d, CTS Out Flow %d, XON/XOFF out/in %d/%d\n"),
                  commDCB.fRtsControl, commDCB.fOutxCtsFlow, commDCB.fOutX, commDCB.fInX));
        SetCommState( hComDev, &commDCB );
        
        pLineDev->hDevice = hComDev;
        pLineDev->hDevice_r0 = hComDev0;

        // Adjust the read/write timeouts as required by dialer thread
        SetDialerTimeouts(pLineDev);
        
        DEBUGMSG(ZONE_FUNCTION, (TEXT("UNIMODEM:-OpenModem\n")));
        return ERROR_SUCCESS;
    } else {
        CloseHandle(hComDev);
    }

om_fail:
    DEBUGMSG(ZONE_FUNCTION,
             (TEXT("UNIMODEM:-OpenModem : Unable to open %s, hComdev x%X\n"),
              pLineDev->szDeviceName, hComDev));

    return ERROR_OPEN_FAILED;
}

// ****************************************************************************
// lineOpen()
//
// Function: Emulate the TAPI lineOpen call.
//
// ****************************************************************************

LONG
DevlineOpen (
    PTLINEDEV pLineDev
    )
{
    DWORD dwRet;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+DevlineOpen\r\n")));
    EnterCriticalSection(&pLineDev->OpenCS);
    
    // The line must be closed
    if (pLineDev->hDevice != (HANDLE)INVALID_DEVICE) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevlineOpen - device already open\r\n")));
        dwRet = LINEERR_ALLOCATED;
        goto exit_Point;
    }
    
    if (0 == pLineDev->wDeviceAvail) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevlineOpen - device has been removed\r\n")));
        dwRet = LINEERR_NODEVICE;
        goto exit_Point;
    }

     // Open the modem port
    DEBUGMSG(ZONE_MISC, (TEXT("UNIMODEM:DevlineOpen - calling openmodem\r\n")));
    dwRet = OpenModem(pLineDev);
    if (dwRet == SUCCESS) {                                             
        // we successfully opened the modem
        pLineDev->DevState      = DEVST_DISCONNECTED;
        OpenModemLog(pLineDev);

    } else {
        dwRet = LINEERR_RESOURCEUNAVAIL;
    };

exit_Point:
    LeaveCriticalSection(&pLineDev->OpenCS);
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-DevlineOpen x%X\r\n"), dwRet));
    return dwRet;
}

//****************************************************************************
// lineClose()
//
// Function: Emulate the TAPI lineClose call.
//
// NOTE : The fDoDrop flag is used to indicate that we should
//        make sure the line gets dropped before we close it.
//
//****************************************************************************

LONG
DevlineClose (
    PTLINEDEV pLineDev,
    BOOL fDoDrop
    )
{
    DWORD dwPrevCallState;
    HTAPICALL hPrevCall;
    HANDLE hDeviceKernel = NULL;
    HANDLE hDeviceExternal = NULL;
    BOOL ret;
    //DWORD dwPrevProcPerms;
    DEBUGMSG(ZONE_FUNC|ZONE_CALLS,
             (TEXT("UNIMODEM:+DevlineClose, handle x%X, callstate x%X\r\n"),
              pLineDev->hDevice, pLineDev->dwCallState));

    // Grab a critical section to ensure the app doesn't call in and
    // cause a line close while we are in the middle of doing a close
    // ourselves.
    EnterCriticalSection(&pLineDev->OpenCS);

    dwPrevCallState = pLineDev->dwCallState;
    hPrevCall = pLineDev->htCall;
    
    if( (HANDLE)INVALID_DEVICE != pLineDev->hDevice )
    {
        try
        {
            //
            // We (device.exe PSL) were the current process when the device
            // was opened, so we need to be the owner for DevlineDrop and
            // the CloseHandle() to work as expected.
            //

            // If a call is in progress, terminate it
            if(pLineDev->hDeviceOwnerProc != NULL) {
                //user call process owns this handle
                ret = DuplicateHandle(
                    pLineDev->hDeviceOwnerProc,
                    pLineDev->hDevice,
                    GetCurrentProcess(),
                    &hDeviceKernel,
                    0,
                    FALSE,
                    DUPLICATE_CLOSE_SOURCE|DUPLICATE_SAME_ACCESS);
                ASSERT(ret);
                pLineDev->hDeviceOwnerProc = NULL;
                if(ret){
                    pLineDev->hDevice = hDeviceKernel;
                }
                else {
                    DEBUGMSG(ZONE_CALLS, (L"UNIMODEM:DevlineClose - DuplicateHandle failed\r\n"));
                }
                   
                    
                    
            
            }
            if (fDoDrop) {
                DEBUGMSG(ZONE_CALLS, (L"UNIMODEM:DevlineClose - Callstate x%X, dropping call\r\n", pLineDev->dwCallState));
                LeaveCriticalSection(&pLineDev->OpenCS);
                DevlineDrop(pLineDev);   
                EnterCriticalSection(&pLineDev->OpenCS);
            }
    
            CloseHandle( pLineDev->hDevice );
            CloseHandle( pLineDev->hDevice_r0);
            CloseHandle( pLineDev->hMdmLog);
            NullifyLineDevice(pLineDev, FALSE);    // Reinit the line device
        }
        except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            // Oops, we must have hit a bad handle.  Fall thru & release the CS.
            PREFAST_SUPPRESS(322, L"this empty except is on porpuse, just fall through since closing")
        }
    }
    
    pLineDev->dwPendingID = INVALID_PENDINGID;
    LeaveCriticalSection(&pLineDev->OpenCS);

    if (dwPrevCallState != LINECALLSTATE_IDLE) {
        pLineDev->htCall = hPrevCall;
        NewCallState(pLineDev, LINECALLSTATE_IDLE, 0L);
        pLineDev->htCall = NULL;
    }

    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:-DevlineClose\r\n")));
    return SUCCESS;
}

//
// Function to signal any threads stuck in WaitCommEvent
//
LONG
SignalCommMask(
    PTLINEDEV pLineDev
    )
{
    LONG rc = 0;
    EnterCriticalSection(&pLineDev->OpenCS);

    if ((HANDLE)INVALID_DEVICE != pLineDev->hDevice) {
        try {
            if (!SetCommMask( pLineDev->hDevice, 0)) {
                DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:SignalCommMask SetCommMask(0) failed %d\n"), GetLastError()));
                
                if (pLineDev->hDevice != INVALID_HANDLE_VALUE) {
                    //
                    // handle is closed, but don't have an invalid value in hDevice, so have to open it again
                    //

                    if ((pLineDev->hDevice = CreateFile(pLineDev->szDeviceName, GENERIC_READ | GENERIC_WRITE, 0,
                                                        NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
                        rc = LINEERR_OPERATIONFAILED;                           
                    } else {
                        rc = 0;
                    }

                    
                    if (0 == rc) {
                        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:SignalCommMask recovered handle\n")));
                        SetCommMask(pLineDev->hDevice, 0);
                    }

                }
                else {
                    rc = LINEERR_OPERATIONFAILED;
                }
            }
        } except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            // Exception means bad handle, so we can't do anything to cancel call
            rc = LINEERR_OPERATIONFAILED;
        }
    }
    if ((HANDLE)INVALID_DEVICE != pLineDev->hDevice_r0) {
        if (!SetCommMask( pLineDev->hDevice_r0, 0)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:SignalCommMask SetCommMask(r0) failed %d\n"), GetLastError()));
            rc = LINEERR_OPERATIONFAILED;
        }
    }
    pLineDev->dwWaitMask = 0;
    LeaveCriticalSection(&pLineDev->OpenCS);
    return rc;
}   // SignalCommMask


void
MdmCommandMode(
    PTLINEDEV pLineDev
    )
{
    DCB  commDCB;    
    //
    // Turn off hardware flow control while sending commands.
    //
    GetCommState( pLineDev->hDevice, &commDCB );
    commDCB.fRtsControl = RTS_CONTROL_ENABLE;
    commDCB.fOutxCtsFlow = 0;
    SetCommState( pLineDev->hDevice, &commDCB );
}


typedef LRESULT (WINAPI * PFN_SENDMESSAGE)(HWND,UINT,WPARAM,LPARAM);

#define HANGUP_TIME 4000

//
// Function to retrieve a modem specific command string or its default.
//
void
GetModemCommand(
    PTLINEDEV pLineDev,
    LPWSTR    CommandName,
    LPSTR     CommandString,
    LPSTR     DefaultCommandString
    )
{
    WCHAR szTemp[MAX_CMD_LENGTH];
    DWORD Size;

    if (CommandString) {
        Size = MAX_CMD_LENGTH;
        if (MdmRegGetValue(
                pLineDev,
                szSettings,
                CommandName,
                REG_SZ,
                (PUCHAR)szTemp,
                &Size) == ERROR_SUCCESS) {
            MdmConvertCommand(szTemp, CommandString, &Size);
        } else {
            strcpy(CommandString, DefaultCommandString);
        }
    }
}

//
// Function to retrieve modem specific hangup command strings or defaults
//
void
GetHangupCommands(
    PTLINEDEV pLineDev,
    LPSTR pEscapeCmd,
    LPSTR pHangupCmd,
    LPSTR pResetCmd
    )
{
    static const UCHAR szEscapeCmd[] = "+++";
    static const UCHAR szHangup[] = "ATH\r\n";
    static const UCHAR szReset[]  = "ATZ\r\n";

    GetModemCommand(pLineDev, L"Escape", pEscapeCmd, (LPSTR)szEscapeCmd);
    GetModemCommand(pLineDev, L"Hangup", pHangupCmd, (LPSTR)szHangup);
    GetModemCommand(pLineDev, L"Reset",  pResetCmd,  (LPSTR)szReset);
}

void
DoHangUp(
    PTLINEDEV pLineDev
    )
{
    DWORD dwWaitReturn;
    UCHAR EscapeCmd[MAX_CMD_LENGTH];
    UCHAR HangupCmd[MAX_CMD_LENGTH];
    UCHAR ResetCmd[MAX_CMD_LENGTH];

#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
    // If a TermCtrl is displayed, get rid of it.
    if( pLineDev->hwTermCtrl )
    {
        PFN_SENDMESSAGE pfnSendMessage;

        if (pfnSendMessage = (PFN_SENDMESSAGE)GetProcAddress(g_hCoreDLL, L"SendMessageW")) {
            // send a WM_CLOSE to the handle
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DoHangUp - WM_CLOSE TermWnd\r\n")));            
            pfnSendMessage (pLineDev->hwTermCtrl, WM_CLOSE, 0, 0);
        }
    }
#endif

    SignalCommMask(pLineDev);

    // We don't want to finish up before the dialer thread has
    // done his thing.  So wait for the CallComplete event that
    // the thread sets before exiting.  Lets assume he'll finish
    // within 4 seconds or we just give up.
    dwWaitReturn = WaitForSingleObject(pLineDev->hCallComplete, 4*1000);
#ifdef DEBUG
    if( WAIT_TIMEOUT == dwWaitReturn )
        DEBUGMSG(ZONE_CALLS | ZONE_ERROR,
                 (TEXT("UNIMODEM:DoHangUp - timeout waiting for call complete\r\n")));
#endif         
     // Drop the line, aborting any call in progress.
    if ((HANDLE)INVALID_DEVICE != pLineDev->hDevice ) {
        try
        {
            PurgeComm(pLineDev->hDevice, PURGE_RXCLEAR|PURGE_TXCLEAR);
            
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DoHangUp - hanging up\r\n")));

            // Make sure the modem hangs up, etc.
            if(IS_NULL_MODEM(pLineDev) ) {
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DoHangUp EscapeCommFunction - DTR off\r\n")));
                EscapeCommFunction ( pLineDev->hDevice, CLRDTR);
                Sleep( 400 );

                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DoHangUp EscapeCommFunction - DTR on\r\n")));
                EscapeCommFunction ( pLineDev->hDevice, SETDTR);
                Sleep( 200 );
            } else {
                GetHangupCommands(pLineDev, EscapeCmd, HangupCmd, ResetCmd);

                MdmCommandMode(pLineDev);

                pLineDev->dwWaitMask = EV_DEFAULT;

                SetWatchdog(pLineDev, pLineDev->HangupWait + pLineDev->EscapeDelay + pLineDev->EscapeWait);
                Sleep(pLineDev->EscapeDelay);
                MdmSendCommand(pLineDev, EscapeCmd);
                EscapeCommFunction ( pLineDev->hDevice, CLRDTR);
                Sleep(pLineDev->EscapeWait);
                MdmGetResponse(pLineDev, (PUCHAR)EscapeCmd, NOCS, CLOSING);

                SetWatchdog(pLineDev, pLineDev->HangupWait);
                MdmSendCommand(pLineDev, HangupCmd);
                MdmGetResponse(pLineDev, (PUCHAR)HangupCmd, NOCS, CLOSING);

                SetWatchdog(pLineDev, pLineDev->HangupWait/2);
                MdmSendCommand(pLineDev, ResetCmd);
                MdmGetResponse(pLineDev, (PUCHAR)ResetCmd, NOCS, CLOSING);

                SetWatchdog(pLineDev, 0);
            }

            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DoHangUp - hangup done\r\n")));
        }
        except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            dwWaitReturn = 0;
        }

        if (PENDING_LINEDROP == pLineDev->dwPendingType) {
            SetAsyncStatus(pLineDev, 0);
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DoHangUp - PendingType = %d\r\n"), pLineDev->dwPendingType));
        }
    }
#ifdef DEBUG
    else
    {
        DEBUGMSG(ZONE_CALLS | ZONE_ERROR,
                 (TEXT("UNIMODEM:DoHangUp - handle invalid.  Can't cancel call\r\n")));
    }
#endif
}


//****************************************************************************
// DevlineDrop()
//
// Function: Emulate the TAPI lineDrop call.
//
//****************************************************************************

LONG
DevlineDrop (
    PTLINEDEV pLineDev
    )
{
    DEBUGMSG(ZONE_FUNC|ZONE_CALLS,
             (TEXT("UNIMODEM:+DevlineDrop\r\n")));

    EnterCriticalSection(&pLineDev->OpenCS);
    pLineDev->dwNumRings = 0;

    // Don't unnecessarily attempt to drop
    if ((LINECALLSTATE_DISCONNECTED == pLineDev->dwCallState ) || (LINECALLSTATE_IDLE == pLineDev->dwCallState )) {
        switch(pLineDev->DevState) {
        case DEVST_DISCONNECTED:
        case DEVST_PORTLISTENINIT:
        case DEVST_PORTLISTENING:
            pLineDev->DevState = DEVST_DISCONNECTED;
            LeaveCriticalSection(&pLineDev->OpenCS);
            SetAsyncStatus(pLineDev, 0);
            DEBUGMSG(ZONE_FUNC|ZONE_CALLS,
                 (TEXT("UNIMODEM:-DevlineDrop, line already dropped\r\n")));
            return SUCCESS;
        }
    }

    pLineDev->dwCallFlags |= CALL_DROPPING;  // Flag that this call is going away
    pLineDev->dwCallFlags &= ~CALL_ACTIVE;

     // Do we need to do a hangup?
    if (LINECALLSTATE_IDLE == pLineDev->dwCallState &&
        DEVST_DISCONNECTED == pLineDev->DevState)

    {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DevlineDrop - no action required\r\n")));
        SetAsyncStatus(pLineDev, 0);
    }
    else
    {
        LeaveCriticalSection(&pLineDev->OpenCS);    
        DoHangUp(pLineDev);
        EnterCriticalSection(&pLineDev->OpenCS);
    };
    
    if (pLineDev->dwCallFlags & CALL_ALLOCATED) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:DevlineDrop - sending LINECALLSTATE_IDLE\n")));
        NewCallState(pLineDev, LINECALLSTATE_IDLE, 0L);
    }
        
    pLineDev->DevState = DEVST_DISCONNECTED;
    pLineDev->dwCallState = LINECALLSTATE_IDLE;
    pLineDev->dwCallFlags = 0;
    
    LeaveCriticalSection(&pLineDev->OpenCS);    
    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:-DevlineDrop\r\n")));
    return SUCCESS;
}

LONG
DoDevlineDrop(
    PTLINEDEV pLineDev
    )
{
    LONG rc;

    rc = DevlineDrop(pLineDev);
    SetAsyncStatus(pLineDev, 0);
    return rc;
}

void
FreeNextCmd(
    PTLINEDEV pLineDev
    )
{
    if (pLineDev->lpstrNextCmd) {
        TSPIFree(pLineDev->lpstrNextCmd);
        pLineDev->lpstrNextCmd = NULL;
    }
}

//
// Wait for a disconnect on a connected modem. Send a LINE_CALLSTATE
// upon disconnect.
//
void
WatchForDisconnect(
    PTLINEDEV pLineDev
    )
{
    DWORD Mask;
    DWORD ModemStatus;
    DWORD BaudRate;
    DCB commDCB;
    BOOL bExit;

    DEBUGMSG(ZONE_FUNCTION|ZONE_CALLS, (TEXT("UNIMODEM:+WatchForDisconnect\n")));
    DEBUGMSG(ZONE_CALLS, (L"UNIMODEM:WatchForDisconnect: Call(0x%x) is %s, CallState(0x%x) is %s\n", 
        pLineDev->dwCallFlags, (pLineDev->dwCallFlags & CALL_ACTIVE) ? L"ACTIVE" : L"NOT ACTIVE",
        pLineDev->dwCallState, (pLineDev->dwCallState == LINECALLSTATE_CONNECTED) ? L"CONNECTED" : L"NOT CONNECTED"));

    // Enable devconfig settings
    GetCommState( pLineDev->hDevice, &commDCB );
    BaudRate = commDCB.BaudRate;    // Preserve connected baud rate
    SetDCBfromDevMiniCfg(&commDCB, &(pLineDev->DevMiniCfg));
    commDCB.BaudRate = BaudRate;
    SetCommState( pLineDev->hDevice, &commDCB );

    DEBUGMSG(ZONE_FUNCTION|ZONE_CALLS,
             (TEXT("UNIMODEM:WatchForDisconnect - RTS Control %d, CTS Out Flow %d, XON/XOFF out/in %d/%d\r\n\r\n"),
              commDCB.fRtsControl, commDCB.fOutxCtsFlow, commDCB.fOutX, commDCB.fInX));

    EnterCriticalSection(&pLineDev->OpenCS);
    bExit = (pLineDev->dwWaitMask == 0);
    if (!bExit) {
        SetCommMask(pLineDev->hDevice_r0, EV_RLSD);
    }
    LeaveCriticalSection(&pLineDev->OpenCS);
    if (bExit) {
        goto wfd_exit;
    }

    while ((pLineDev->dwCallState == LINECALLSTATE_CONNECTED) && (pLineDev->dwCallFlags & CALL_ACTIVE)) {
        if (WaitCommEvent(pLineDev->hDevice_r0, &Mask, NULL )) {
            //
            // Remote disconnect detection
            //
            if (Mask & EV_RLSD) {
                ModemStatus = 0;
                if (GetCommModemStatus(pLineDev->hDevice_r0, &ModemStatus)) {
                    if (!(ModemStatus & MS_RLSD_ON)) {
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:WatchForDisconnect RLSD went low\n")));
                        DoHangUp(pLineDev);
                        pLineDev->DevState = DEVST_DISCONNECTED;
                        pLineDev->dwCallFlags &= ~CALL_ACTIVE;
                        pLineDev->dwNumRings = 0;
                        NewCallState(pLineDev, LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_NORMAL);
                        NewCallState(pLineDev, LINECALLSTATE_IDLE, 0L);
                        break;
                    }
                } else {
                    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:WatchForDisconnect GetCommModemStatus failed %d\n"), GetLastError()));
                    break;
                }
            }

            //
            // See if we got signalled to drop via TSPI_lineDrop
            //
            if ((Mask == 0) || (pLineDev->dwCallFlags & CALL_DROPPING)) {
                break;
            }
        } else {
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:WatchForDisconnect WaitCommEvent failed %d\n"), GetLastError()));
            break;
        }
    }

wfd_exit:
    EnterCriticalSection(&pLineDev->OpenCS);
    pLineDev->dwWaitMask = EV_DEFAULT;
    LeaveCriticalSection(&pLineDev->OpenCS);
    MdmCommandMode(pLineDev);

    DEBUGMSG(ZONE_ASYNC, (TEXT("UNIMODEM:WatchForDisconnect Current Op=%s\n"), GetPendingName(pLineDev->dwPendingType)));
    if (pLineDev->dwPendingType != PENDING_LINEDROP) {
        DoDevlineDrop(pLineDev);
    }

    DEBUGMSG(ZONE_CALLS|ZONE_FUNCTION, (TEXT("UNIMODEM:-WatchForDisconnect\n")));
}   // WatchForDisconnect

//
// Process a RING response from the modem
//
LONG
ProcessModemRing(
    PTLINEDEV pLineDev
    )
{
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ProcessModemRing\n")));

    if (pLineDev->dwNumRings == 0) {
        if (SetWatchdog(pLineDev, 25000)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:ProcessModemRing - SetWatchDog failed!!\n")));
        }

        pLineDev->dwCallFlags = CALL_INBOUND;
        pLineDev->dwCallState = LINECALLSTATE_OFFERING;
        pLineDev->DevState = DEVST_PORTLISTENOFFER;
        
        CallLineEventProc(
            pLineDev,
            0,
            LINE_NEWCALL,
            (DWORD)pLineDev,            // HDRVCALL
            (DWORD)&pLineDev->htCall,   // LPHTAPICALL
            0);
        
        pLineDev->dwCurMediaModes = pLineDev->dwDefaultMediaModes | LINEMEDIAMODE_UNKNOWN;
        NewCallState(pLineDev, LINECALLSTATE_OFFERING, 0);
    }
    CallLineEventProc(
        pLineDev,
        0,
        LINE_LINEDEVSTATE,
        LINEDEVSTATE_RINGING,
        0,
        pLineDev->dwNumRings);

    pLineDev->dwNumRings++;

    //
    // If lineAnswer has been called and the answer command has not been issued, then do it now.
    //
    if ((PENDING_LINEANSWER == pLineDev->dwPendingType) && (!(pLineDev->dwCallFlags & CALL_ALLOCATED))) {
        return ProcessModemFailure(pLineDev);
    }
    
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ProcessModemRing\n")));
    return 0;
}   // ProcessModemRing


//
// Process a CARRIER, PROTOCOL or PROGRESS response from the modem
//
LONG
ProcessModemProceeding(
    PTLINEDEV pLineDev
    )
{
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ProcessModemProceeding\n")));
    if ((pLineDev->DevState == DEVST_PORTLISTENING) || (pLineDev->DevState == DEVST_PORTLISTENOFFER)) {
        SetAsyncStatus(pLineDev, 0);
    }
    NewCallState(pLineDev, LINECALLSTATE_PROCEEDING, 0);
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ProcessModemProceeding\n")));
    return 0;
}   // ProcessModemProceeding


//
// Process a CONNECT response from the modem
//
LONG
ProcessModemConnect(
    PTLINEDEV pLineDev
    )
{
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ProcessModemConnect\n")));

    SetWatchdog( pLineDev, 0 ); // Cancel watchdog
    pLineDev->dwCallFlags |= CALL_ACTIVE;
    if ((pLineDev->DevState == DEVST_PORTLISTENING) || (pLineDev->DevState == DEVST_PORTLISTENOFFER)) {
        pLineDev->DevState = DEVST_CONNECTED;
        SetAsyncStatus(pLineDev, 0);
    }
    NewCallState(pLineDev, LINECALLSTATE_CONNECTED, 0);
       
    //
    // Now watch the comm state to notify of disconnect
    //
    WatchForDisconnect(pLineDev);
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ProcessModemConnect\n")));
    return 0;
}   // ProcessModemConnect


//
// Process a SUCCESS response from the modem
//
LONG
ProcessModemSuccess(
    PTLINEDEV pLineDev
    )
{
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ProcessModemSuccess\n")));
    if ((pLineDev->DevState == DEVST_PORTLISTENING) || (pLineDev->DevState == DEVST_PORTLISTENOFFER)) {
        SetAsyncStatus(pLineDev, 0);
    }
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ProcessModemSuccess\n")));
    return 0;
}   // ProcessModemSuccess


#define DEFAULT_ANSWER_TIMEOUT 25

extern const UCHAR pchDCCResp[];

//
// Process a FAILURE response from the modem
//
// SetCommMask(0) is used to signal UnimodemControlThread when some new command
// has been requested. This gets interpreted by MdmGetResponse as a
// MODEM_FAILURE.
//
LONG
ProcessModemFailure(
    PTLINEDEV pLineDev
    )
{
    WCHAR szTemp[256];
    UCHAR szCmd[MAX_CMD_LENGTH];
    DWORD dwSize;
    LONG rc = 0;
    int cmdLen;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ProcessModemFailure\n")));

pmf_next:
    
    switch (pLineDev->dwPendingType) {
    case PENDING_LINEANSWER:
    {
        DWORD dwTimeout;

        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure PENDING_LINEANSWER\n")));
        
        FreeNextCmd(pLineDev);

        dwTimeout = DEFAULT_ANSWER_TIMEOUT;
        dwSize = sizeof(DWORD);
        MdmRegGetValue(
                pLineDev,
                szSettings,
                szAnswerTimeout,
                REG_DWORD,
                (PUCHAR)&dwTimeout,
                &dwSize);

        if (SetWatchdog(pLineDev, dwTimeout * 1000)) {
            return 1;
        }

        pLineDev->dwCallFlags |= CALL_ALLOCATED;

        if (IS_NULL_MODEM(pLineDev)) {
            GetModemCommand(pLineDev, L"DCCResponse", szCmd,(LPSTR)pchDCCResp);
            cmdLen = strlen(szCmd);
            if(cmdLen > 0)
            {
                pLineDev->lpstrNextCmd = TSPIAlloc(cmdLen+1); // answer DCC w/"CLIENTSERVER"
                if (pLineDev->lpstrNextCmd) {
                    strcpy(pLineDev->lpstrNextCmd, szCmd);
                }
            } else {
                DEBUGMSG(ZONE_THREAD,(TEXT("UNIMODEM: ProcessModemFailure Skipping DCCResponse...\n" )));
                break;
            }
        } else {
            dwSize = sizeof(szTemp);
            if (MdmRegGetValue(
                    pLineDev,
                    szSettings,
                    szAnswer,
                    REG_SZ,
                    (PUCHAR)szTemp,
                    &dwSize) == ERROR_SUCCESS) {
                pLineDev->lpstrNextCmd = MdmConvertCommand(szTemp, NULL, NULL);
            } else {
                pLineDev->lpstrNextCmd = TSPIAlloc(sizeof(szDefaultAnswerCmd));
                if (pLineDev->lpstrNextCmd) {
                    strcpy(pLineDev->lpstrNextCmd, szDefaultAnswerCmd);
                }
            }
        }

        if (pLineDev->lpstrNextCmd) {
            MdmCommandMode(pLineDev);
            if (!MdmSendCommand(pLineDev, pLineDev->lpstrNextCmd)) {
                FreeNextCmd(pLineDev);
                SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);
                return 2;
            }

            FreeNextCmd(pLineDev);

        } else {
            SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);
            return 3;
        }
    }
    break;

    case PENDING_EXIT:
    {
        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure PENDING_EXIT\n")));
        return 4;
    }
    break;

    case PENDING_LINEMAKECALL:
    case PENDING_LINEDIAL:
    {
        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure PENDING_LINEDIAL/PENDING_LINEMAKECALL\n")));
        Dialer(pLineDev);
        switch (pLineDev->dwCallState) {
        case LINECALLSTATE_CONNECTED:
            WatchForDisconnect(pLineDev);
            goto pmf_next;

        case LINECALLSTATE_DIALING:
            MdmGetResponse(pLineDev, "listening", NOCS, OPENED);
            goto pmf_next;
        }
     
    }
    break;

    case PENDING_LINEDROP:
    {
        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure PENDING_LINEDROP\n")));
        rc = DoDevlineDrop(pLineDev);
    }
    break;

    case PENDING_LISTEN:
    {
        // The WatchDog that was set after the first RING has timed out
        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure PENDING_LISTEN\n")));
        rc = DevlineDrop(pLineDev);
    }
    break;

    default:
    {
        DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:ProcessModemFailure default case %d\n"), pLineDev->dwPendingType));
        return 5;
    }

    }   // switch
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ProcessModemFailure %d\n"), rc));
    return rc;
}   // ProcessModemFailure


BOOL
GetMonitorCommand(
    PTLINEDEV pLineDev
    )
{
    LPWSTR lpszTemp;
    DWORD dwSize;

    //
    // For manual dial modems, don't listen for incoming calls
    //
    if (pLineDev->DevMiniCfg.fwOptions & MANUAL_DIAL) {
        FreeNextCmd(pLineDev);
        return TRUE;
    }

    if (MdmRegGetValue(
            pLineDev,
            szSettings,
            szMonitor,
            REG_SZ,
            NULL,
            &dwSize) == ERROR_SUCCESS) {
        lpszTemp = TSPIAlloc(dwSize);
        if (!lpszTemp) {
            DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:GetMonitorCommand - Monitor cmd alloc failed 1\n")));
            goto gmc_DefaultMonitorCmd;
        }
        if (MdmRegGetValue(
                pLineDev,
                szSettings,
                szMonitor,
                REG_SZ,
                (PUCHAR)lpszTemp,
                &dwSize) == ERROR_SUCCESS) {    
            pLineDev->lpstrNextCmd = MdmConvertCommand(lpszTemp, NULL, NULL);
            TSPIFree(lpszTemp);
        } else {
            goto gmc_DefaultMonitorCmd;
        }
    } else {
gmc_DefaultMonitorCmd:
        pLineDev->lpstrNextCmd = TSPIAlloc(strlen(szDefaultMonitorCmd)+1);
        if (!pLineDev->lpstrNextCmd) {
            DEBUGMSG(ZONE_THREAD, (TEXT("UNIMODEM:GetMonitorCommand - Monitor cmd alloc failed 2\n")));
            return FALSE;
        }
        strcpy(pLineDev->lpstrNextCmd, szDefaultMonitorCmd);
    }
    return TRUE;
}

BOOL
DoModemInit(
    PTLINEDEV pLineDev
    )
{
    BOOL bRet;

    LeaveCriticalSection(&pLineDev->OpenCS);
    bRet = ModemInit(pLineDev);
    EnterCriticalSection(&pLineDev->OpenCS);

    //
    // If ModemInit got interrupted by a lineMakeCall, then proceed because
    // Dialer() will perform the modem init.
    //
    if (FALSE == bRet) {
        if (bRet = (pLineDev->dwPendingType == PENDING_LINEMAKECALL)) {
            pLineDev->dwWaitMask = EV_DEFAULT;
            SetCommMask(pLineDev->hDevice, EV_DEFAULT);
        }
    }

    return bRet;
}


//
// Send commands and listen for responses until the line is closed
//
DWORD
UnimodemControlThread(
    PTLINEDEV pLineDev
    )
{
    MODEMRESPCODES MdmResp;
    DWORD rc;
    BOOL  bPendingOp;
    HTAPICALL hPrevCall;

    EnterCriticalSection(&pLineDev->OpenCS);

    bPendingOp = !((PENDING_LISTEN == pLineDev->dwPendingType) || (INVALID_PENDINGOP == pLineDev->dwPendingType));

    DEBUGMSG(ZONE_FUNC|ZONE_THREAD, (TEXT("+UnimodemControlThread(%d)\n"), pLineDev->dwDeviceID));

    if (bPendingOp) {
        DEBUGMSG(ZONE_THREAD, (L"UnimodemControlThread(%d) PendingOp=%s\n",
            pLineDev->dwDeviceID, GetPendingName(pLineDev->dwPendingType)));
    }

    if (DEVST_DISCONNECTED == pLineDev->DevState) {
        pLineDev->DevState = DEVST_PORTLISTENINIT;
    }

    if (pLineDev->dwPendingType == PENDING_LINEMAKECALL) {
        hPrevCall = pLineDev->htCall;
    } else {
        hPrevCall = NULL;
    }

    rc = DevlineOpen(pLineDev);
    if ((rc) && (rc != LINEERR_ALLOCATED)) {
        DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) DevlineOpen failed\n"), pLineDev->dwDeviceID));
        goto uct_leave;
    }

    FreeNextCmd(pLineDev);
    MdmCommandMode(pLineDev);

    pLineDev->dwWaitMask = EV_DEFAULT;
    SetCommMask(pLineDev->hDevice, EV_DEFAULT);

    if (!IS_NULL_MODEM(pLineDev)) {
        //
        // Don't init a manual dial modem
        //
        if (!(pLineDev->DevMiniCfg.fwOptions & MANUAL_DIAL)) {
            //
            // If modeminit fails, we sure won't be able to anything!
            //
            if (!DoModemInit(pLineDev)) {
                if (pLineDev->fTakeoverMode) {
                    goto uct_calldone;
                }
                pLineDev->DevState = DEVST_DISCONNECTED;
                DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) ModemInit failed\n"), pLineDev->dwDeviceID));
                goto uct_leave;
            }
        }
    }

    if (pLineDev->dwPendingType == PENDING_LINEMAKECALL) {
        pLineDev->DevState = DEVST_PORTCONNECTDIAL;
        LeaveCriticalSection(&pLineDev->OpenCS);
        if (ProcessModemFailure(pLineDev)) {
            EnterCriticalSection(&pLineDev->OpenCS);
            goto uct_leave;
        }
        EnterCriticalSection(&pLineDev->OpenCS);
    } else {
        pLineDev->DevState = DEVST_PORTLISTENING;
        if (!IS_NULL_MODEM(pLineDev)) {
            //
            // Not making a call so monitor the line for incoming calls.
            //
            if (!GetMonitorCommand(pLineDev)) {
                goto uct_leave;
            }
        }
    }

    bPendingOp = FALSE;
    MdmResp = MODEM_FAILURE;

uct_monitor:    // NOTE: OpenCS must be owned here
    //
    // Monitor modem responses
    //
    while (pLineDev->htLine && (pLineDev->DevState != DEVST_DISCONNECTED)) {
        if ((pLineDev->DevState == DEVST_PORTLISTENING) || (pLineDev->DevState == DEVST_PORTLISTENOFFER)) {
            if (pLineDev->lpstrNextCmd) {
                if (!MdmSendCommand(pLineDev, pLineDev->lpstrNextCmd)) {
                    goto uct_calldone;
                }
            }

            switch (pLineDev->dwPendingType) {
            //
            // If the user did an open/close in quick succession then check for it.
            //
            case PENDING_EXIT:
                MdmResp = MODEM_EXIT;
                goto uct_leave;
                break;

            case PENDING_LINEMAKECALL:
                pLineDev->DevState = DEVST_PORTCONNECTDIAL;
                MdmResp = MODEM_FAILURE;
                LeaveCriticalSection(&pLineDev->OpenCS);
                goto uct_process;
                break;

            case PENDING_LINEANSWER:
                if (IS_NULL_MODEM(pLineDev)) {
                    //
                    // We only get here if we just responded with "CLIENTSERVER" to someone's "CLIENT",
                    // so no further responses are needed. The connection is established.
                    //
                    MdmResp = MODEM_CONNECT;
                    SetEvent(pLineDev->hCallComplete);
                    LeaveCriticalSection(&pLineDev->OpenCS);
                    goto uct_process;
                }
                break;
            }

            if (pLineDev->lpstrNextCmd) {
                //
                // Some modems don't respond to the monitor command until an incoming call is made,
                // but some modems do respond to the monitor command so we wait for a MODEM_RING response.
                //
                if ((MdmResp = MdmGetResponse(pLineDev, pLineDev->lpstrNextCmd, LEAVECS, OPENED)) == MODEM_SUCCESS) {
                    if (pLineDev->dwPendingType == PENDING_LISTEN) {
                        MdmResp = MdmGetResponse(pLineDev, pLineDev->lpstrNextCmd, NOCS, OPENED);
                    }
                }
            } else {
                //
                // Still listening for an incoming call
                //
                MdmResp = MdmGetResponse(pLineDev, "listening", LEAVECS, OPENED);
            }
        } else {
            LeaveCriticalSection(&pLineDev->OpenCS);
        }


uct_process:    // NOTE: OpenCS must not be owned here
        FreeNextCmd(pLineDev);

        switch (MdmResp) {
        case MODEM_RING:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) MODEM_RING\n"), pLineDev->dwDeviceID));
            if (ProcessModemRing(pLineDev)) {
                EnterCriticalSection(&pLineDev->OpenCS);
                goto uct_calldone;
            }
            break;

        case MODEM_CARRIER:
        case MODEM_PROTOCOL:
        case MODEM_PROGRESS:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) LINECALLSTATE_PROCEEDING\n"), pLineDev->dwDeviceID));
            ProcessModemProceeding(pLineDev);
            break;

        case MODEM_CONNECT:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) LINECALLSTATE_CONNECTED\n"), pLineDev->dwDeviceID));
            ProcessModemConnect(pLineDev);
            break;

        case MODEM_SUCCESS:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) MODEM_SUCCESS\n"), pLineDev->dwDeviceID));
            ProcessModemSuccess(pLineDev);
            break;
        
        case MODEM_FAILURE:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) MODEM_FAILURE\n"), pLineDev->dwDeviceID));
            if (ProcessModemFailure(pLineDev)) {
                EnterCriticalSection(&pLineDev->OpenCS);
                goto uct_calldone;
            }
            break;

        case MODEM_EXIT:
            DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) MODEM_EXIT\n"), pLineDev->dwDeviceID));
            EnterCriticalSection(&pLineDev->OpenCS);
            goto uct_calldone;
            break;

        }   // switch
        MdmResp = MODEM_FAILURE;
        EnterCriticalSection(&pLineDev->OpenCS);
    }   // while


uct_calldone:

    FreeNextCmd(pLineDev);
    //
    // Special case for PASSTHROUGH connections
    //
    if (pLineDev->fTakeoverMode) {
        pLineDev->bControlThreadRunning = FALSE;
        LeaveCriticalSection(&pLineDev->OpenCS);
        DEBUGMSG(ZONE_THREAD|ZONE_FUNC, (TEXT("-UnimodemControlThread(%d) - PASSTHROUGH\n"), pLineDev->dwDeviceID));
        return 0;
    }

    //
    // Go back to monitoring for incoming calls if needed
    //
    if (pLineDev->dwDetMediaModes) {
        LeaveCriticalSection(&pLineDev->OpenCS);
        DoDevlineDrop(pLineDev);
        EnterCriticalSection(&pLineDev->OpenCS);

        //
        // Make sure the handle is still valid.
        //
        if (SetCommMask(pLineDev->hDevice, EV_DEFAULT)) {
            pLineDev->dwWaitMask = EV_DEFAULT;
            pLineDev->DevState = DEVST_PORTLISTENING;
            if (IS_NULL_MODEM(pLineDev)) {
                pLineDev->dwNumRings = 0;
                SetDialerTimeouts(pLineDev);
                DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) Continue monitoring\n"), pLineDev->dwDeviceID));
                goto uct_monitor;
            }
            if (GetMonitorCommand(pLineDev)) {
                MdmCommandMode(pLineDev);
                pLineDev->dwNumRings = 0;
                if (pLineDev->dwPendingStatus == LINEERR_ALLOCATED) {
                    SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);
                }
                DEBUGMSG(ZONE_THREAD, (TEXT("UnimodemControlThread(%d) Continue monitoring\n"), pLineDev->dwDeviceID));
                SetDialerTimeouts(pLineDev);
                goto uct_monitor;
            }
        }
    }

uct_leave:
    FreeNextCmd(pLineDev);

    // Make sure that we do not leave anything open
    SetWatchdog(pLineDev, 0 ); // Cancel watchdog
    pLineDev->bControlThreadRunning = FALSE;
    LeaveCriticalSection(&pLineDev->OpenCS);

    SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);

    if (bPendingOp) {
        if (pLineDev->dwPendingType == PENDING_LINEMAKECALL) {
            pLineDev->htCall = hPrevCall;
            NewCallState(pLineDev, LINECALLSTATE_DISCONNECTED, 0L);
            pLineDev->htCall = NULL;
        }
    }

    DevlineClose(pLineDev, TRUE);

    DEBUGMSG(ZONE_THREAD|ZONE_FUNC, (TEXT("-UnimodemControlThread(%d)\n"), pLineDev->dwDeviceID));
    return 0;
}   // UnimodemControlThread


LONG
StartControlThread(
    PTLINEDEV pLineDev
    )
{
    HANDLE hThd;
    LONG rc;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+StartControlThread\n")));

    rc = 0;
    if (pLineDev->bControlThreadRunning == FALSE) {
        pLineDev->bControlThreadRunning = TRUE;

        hThd = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)UnimodemControlThread,
                             pLineDev, 0, NULL );
        if (!hThd) {
            NKDbgPrintfW( TEXT("Unable to Create UnimodemControlThread\n") );
            pLineDev->bControlThreadRunning = FALSE;
            rc = LINEERR_OPERATIONFAILED;
        } else {
            CeSetThreadPriority(hThd, g_dwUnimodemThreadPriority);
            CloseHandle(hThd);
        }
    }

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-StartControlThread returning %x\n"), rc));
    return rc;
}   // StartControlThread


LONG
SignalControlThread(
    PTLINEDEV pLineDev
    )
{
    LONG rc;
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+SignalControlThread\n")));

    //
    // Signal UnimodemListenThread
    //
    rc = SignalCommMask(pLineDev);
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-SignalControlThread returning %x\n"), rc));
    return rc;
}   // SignalControlThread

//
// All TSPI_line* functions that need access to the modem go through here.
// The UnimodemControlThread sends the actual commands and processes the
// responses
//
LONG
ControlThreadCmd(
    PTLINEDEV pLineDev,
    DWORD dwPendingOP,
    DWORD dwPendingID
    )
{
    LONG rc;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ControlThreadCmd\n")));

    // Fail current outstanding async request (if any) and remember this one
    SetAsyncOp(pLineDev, dwPendingOP);

    EnterCriticalSection(&pLineDev->OpenCS);

    if (pLineDev->bControlThreadRunning) {
        switch (dwPendingOP) {
        case PENDING_LINEDROP:
        case PENDING_EXIT:
        case PENDING_LINEANSWER:
        case PENDING_LINEDIAL:
        case PENDING_LINEMAKECALL:
        case PENDING_LISTEN:
            rc = SignalControlThread(pLineDev);
            break;

        default:
            rc = 0;
            break;
        }
    } else {
        switch (dwPendingOP) {
        case PENDING_LINEMAKECALL:
        case PENDING_LISTEN:
            rc = StartControlThread(pLineDev);
            break;

        case PENDING_EXIT:
        default:
            rc = LINEERR_OPERATIONFAILED;
            break;
        }
    }

    if (rc == 0) {
        switch (dwPendingOP) {
        case PENDING_LINEANSWER:
        case PENDING_LINEDIAL:
        case PENDING_LINEDROP:
        case PENDING_LINEMAKECALL:
            rc = dwPendingID;
            break;
        }
    }

    LeaveCriticalSection(&pLineDev->OpenCS);

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-ControlThreadCmd returning %x\n"), rc));
    return rc;
}   // ControlThreadCmd

