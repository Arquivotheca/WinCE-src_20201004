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
/**********************************************************************/
/**               
/**********************************************************************/
//
//    serial.c    The win32 apis to access a serial device.  Some of
//                these could be changed to be #define's for speed in
//                the future.
//
//    @doc    EXTERNAL SERIAL WIN32
//
//    @topic    Win32Serial functions |
//
//      The following are the functions used with communications devices
//
//        <f BuildCommDCB>            <f BuildCommDCBAndTimeouts>
//
//        <f ClearCommBreak>            <f ClearCommError>
//
//        <f EscapeCommFunction>        <f GetCommMask>
//
//        <f GetCommModemStatus>        <f GetCommProperties>
//
//        <f GetCommState>            <f GetCommTimeouts>
//
//        <f PurgeComm>                <f SetCommBreak>
//
//        <f SetCommMask>            <f SetCommState>
//
//        <f SetCommTimeouts>            <f SetupComm>
//
//        <f TransmitCommChar>            <f WaitCommEvent>
//
//


#include "windows.h"
#include "memory.h"
#include "windev.h"
#include "pegdser.h"

/*****************************************************************************
*
*
*    @func    BOOL    |    BuildCommDCB |
*            The <b BuildCommDCB> functions fills a specified DCB
*            strucutre with the values in a specified string. 
*
*    @rdesc    If the function succeeds, the return value is TRUE.
*            If the function fails, the return value is FALSE. To
*            get extended error information, call <f GetLastError> 
*
*    @parm    LPCTSTR    |    lpszDef    | 
*        Points to a null-terminated string that specifies
*        device-control information. The string must have the
*        same form as the Windows NT or MS-DOS mode command-line
*        arguments.  For example, the following line specifies a
*        baud rate of 1200, no parity, 8 data bits, and 1 stop bit:
*        <nl><tab>baud=1200 parity=N data=8 stop=1
*        <nl>The device name is ignored if it is included in the string,
*        but it must specify a valid device, as follows:
*        <nl><tab>COM1: baud=1200 parity=N data=8 stop=1
*
*    @parm    LPDCB    |    lpDCB    | 
*        Points to the DCB structure to be filled in. 
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The BuildCommDCB function adjusts only those members of
*        the DCB structure that are specifically affected by the
*        lpszDef parameter, with the following exceptions:
*        <nl><tab>If the specified baud rate is 110, the function sets
*        the stop bits to 2 to remain compatible with the Windows
*        NT or MS-DOS mode command.
*        <nl><tab>By default, BuildCommDCB disables XON/XOFF and hardware
*        flow control. To enable flow control, you must explicitly
*        set the appropriate members of the DCB structure.
*        <nl>The BuildCommDCB function only fills in the members of
*        the DCB structure. To apply these settings to a serial
*        port, use the SetCommState function.
*
*    @xref    <f SetCommState>
*
*/
BOOL WINAPI
BuildCommDCB (LPCTSTR lpszDef, LPDCB lpDCB)
{
    DEBUGMSG(1, (TEXT("function not implemented yet\r\n")));
    ASSERT(0);
    return FALSE;
}


/*****************************************************************************
*
*
*    @func    BOOL    |    BuildCommDCBAndTimeouts | 
*        The <b BuildCommDCBAndTimeouts> function translates a
*        device-definition string into appropriate device-control
*        block codes and then places these codes into a device
*        control block. The function can also set up time-out
*        values, including the possibility of no time-outs, for a
*        device; the function's behavior in this regard varies
*        based on the contents of the device-definition string.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.
*        If the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    LPCTSTR    |    lpDef    | 
*        Points to a null-terminated string that specifies
*        device-control information for the device. The function
*        takes this string, parses it, and then sets appropriate
*        values in the DCB structure pointed to by lpDCB.
*
*    @parm    LPDCB    |    lpDCB    |
*        Points to a DCB structure that the function fills with
*        information from the device-control information string
*        pointed to by lpDef. This DCB structure defines the
*        control settings for a communications device.
*
*    @parm    LPCOMMTIMEOUTS | lpCommTO |
*        Points to a COMMTIMEOUTS structure that the function can
*        use to set device time-out values.  The
*        BuildCommDcbAndTimeouts function modifies its time-out
*        setting behavior based on the presence or absence of a
*        "TO=xxx" substring in the string specified by lpDef:
*        <nl>If that string contains the substring "TO=ON", the
*        function sets up total read and write time-out values for
*        the device based on the time-out structure pointed to by
*        lpCommTimeouts.
*        <nl>If that string contains the substring "TO=OFF", the
*        function sets up the device with no time-outs.
*        <nl>If that string contains neither of
*        the aforementioned "TO=xxx" substrings, the function
*        ignores the time-out structure pointed to by
*        lpCommTimeouts. The time-out structure will not be
*        accessed.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @xref    <f BuildCommDCB> <f GetCommTimeouts> <f SetCommTimeouts>
*
*/
BOOL WINAPI
BuildCommDCBAndTimeouts(LPCTSTR lpDef,
                        LPDCB lpDCB,
                        LPCOMMTIMEOUTS lpCommTimeouts)
{
    DEBUGMSG(1, (TEXT("function not implemented yet\r\n")));
    ASSERT(0);

    return FALSE;
}


/*****************************************************************************
*
*
*    @func    BOOL    |    ClearCommBreak|
*        The ClearCommBreak function restores character
*        transmission for a specified communications device and
*        places the transmission line in a nonbreak state.
*
*    @rdesc    
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    |
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        A communications device is placed in a break state by the
*        SetCommBreak or EscapeCommFunction function. Character
*        transmission is then suspended until the break state is
*        cleared by calling ClearCommBreak.
*
*
*/
BOOL WINAPI
ClearCommBreak(HANDLE hCommDev)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_BREAK_OFF,
                            (LPVOID)NULL, 0, (LPVOID)0,
                            0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    ClearCommError| 
*        The ClearCommError function retrieves information about a
*        communications error and reports the current status of a
*        communications device. The function is called when a
*        communications error occurs, and it clears the device's
*        error flag to enable additional input and output (I/O)
*        operations.
*
*    @rdesc    
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDWORD    |    lpdwErrors    |
*        Points to a 32-bit variable to be filled with a mask
*        indicating the type of error. This parameter can be one
*        or more of the following error values:<nl>
*        @tab2    Value |    Meaning
*        @tab2    CE_BREAK | The hardware detected a break
*            condition.
*        @tab2    CE_FRAME | The hardware detected a framing
*            error.
*        @tab2    CE_IOE | An I/O error occurred during
*            communications with the device.
*        @tab2    CE_MODE | The requested
*            mode is not supported, or the hCommDev parameter is
*            invalid. If this value is specified, it is the only valid
*            error.
*        @tab2    CE_OVERRUN | A character-buffer overrun has
*            occurred. The next character is lost.
*        @tab2    CE_RXOVER | An input buffer overflow has occurred.
*            There is either no room in the input buffer, or a
*            character was received after the end-of-file (EOF)
*            character.
*        @tab2    CE_RXPARITY | The hardware detected a parity error.
*        @tab2    CE_TXFULL | The application tried to transmit a
*            character, but the output buffer was full.
*        @tab2    CE_DNS | The parallel device is not selected.
*        @tab2    CE_PTO | A time-out occurred on a parallel device.
*        @tab2    CE_OOP | The parallel device signaled that it is out
*            of paper.
*
*    @parm    LPCOMSTAT |    lpCSt        |
*        Points to a COMSTAT structure in which the device's
*        status information is returned. If lpcst is NULL, no
*        status information is returned.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        If a communications port has been set up with a TRUE
*        value for the fAbortOnError member of the setup DCB
*        structure, the communications software will terminate all
*        read and write operations on the communications port when
*        a communications error occurs. No new read or write
*        operations will be accepted until the application
*        acknowledges the communications error by calling the
*        ClearCommError function.  The ClearCommError function
*        fills the status buffer pointed to by the lpcst parameter
*        with the current status of the communications device
*        specified by the hCommDev parameter.
*
*    @xref    <f ClearCommBreak>
*
*/
BOOL WINAPI
ClearCommError(HANDLE hCommDev, LPDWORD lpdwErrors, LPCOMSTAT lpCSt)
{
    DWORD            dwBytesRet;
    SERIAL_DEV_STATUS    SerDevStat;

    if (TRUE == DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_COMMSTATUS,
                            (LPVOID)NULL, 0, (LPVOID)&SerDevStat,
                            sizeof(SERIAL_DEV_STATUS), &dwBytesRet, 0)) {
        if (NULL != lpdwErrors)
            *lpdwErrors = SerDevStat.Errors;
        if (NULL != lpCSt) {
            memcpy ((char *)lpCSt, (char *)&(SerDevStat.ComStat),
                    sizeof(COMSTAT));
        }
        return TRUE;
    }
    return FALSE;
}

/*****************************************************************************
*
*
*    @func    BOOL    |    EscapeCommFunction |
*        The EscapeCommFunction function directs a specified
*        communications device to perform an extended function.
*
*    @rdesc    
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev        | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    DWORD    |    fdwFunc            |
*        Specifies the code of the extended function to
*        perform. This parameter can be one of the following
*        values:<nl>
*        @tab2    Value |    Meaning
*        @tab2    CLRDTR |    Clears the DTR (data-terminal-ready)
*            signal.
*        @tab2    CLRRTS |    Clears the RTS (request-to-send) signal.
*        @tab2    SETDTR |    Sends the DTR (data-terminal-ready)
*            signal.
*        @tab2    SETRTS |    Sends the RTS (request-to-send) signal.
*        @tab2    SETXOFF |    Causes transmission to act as if an XOFF
*            character has been received.
*        @tab2    SETXON |    Causes transmission to act as if an XON
*            character has been received.
*        @tab2    SETBREAK |    Suspends character transmission and
*            places the transmission line in a break state until
*            the ClearCommBreak function is called (or
*            EscapeCommFunction is called with the CLRBREAK extended
*            function code). The SETBREAK extended function code is
*            identical to the SetCommBreak function. Note that this
*            extended function does not flush data that has not
*            been transmitted.
*        @tab2    CLRBREAK |    Restores character transmission and
*            places the transmission line in a nonbreak state. The
*            CLRBREAK extended function code is identical to the
*            ClearCommBreak function.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @xref    <f SetCommBreak>
*
*/
BOOL WINAPI
EscapeCommFunction(HANDLE hCommDev, DWORD fdwFunc)
{
    DWORD    dwCode = (DWORD)-1;
    DWORD    dwBytesReturned;

    switch (fdwFunc) {
    case SETXOFF :                    // Simulate XOFF received
        dwCode = IOCTL_SERIAL_SET_XOFF;
        break;
    case SETXON :                    // Simulate XON received
        dwCode = IOCTL_SERIAL_SET_XON;
        break;
    case SETRTS :                    // Set RTS high
        dwCode = IOCTL_SERIAL_SET_RTS;
        break;
    case CLRRTS :                    // Set RTS low
        dwCode = IOCTL_SERIAL_CLR_RTS;
        break;
    case SETDTR :                    // Set DTR high
        dwCode = IOCTL_SERIAL_SET_DTR;
        break;
    case CLRDTR :                    // Set DTR low
        dwCode = IOCTL_SERIAL_CLR_DTR;
        break;
// NOT Supported, fall down to unsupported return
//    case RESETDEV :                    // Reset device if possible
//        dwCode = IOCTL_SERIAL_RESETDEV;
//        break;
    case SETBREAK :                    // Set the device break line.
        dwCode = IOCTL_SERIAL_SET_BREAK_ON;
        break;
    case CLRBREAK :                    // Clear the device break line.
        dwCode = IOCTL_SERIAL_SET_BREAK_OFF;
        break;
    case SETIR :
        dwCode = IOCTL_SERIAL_ENABLE_IR;
        break;
    case CLRIR :
        dwCode = IOCTL_SERIAL_DISABLE_IR;
        break;
    default :
        DEBUGMSG(1, (TEXT("Unsupported EscapeCommFunction %d\r\n"),
                          fdwFunc)); 
        break;
    }

    if ((DWORD)-1 == dwCode) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return DeviceIoControl (hCommDev, dwCode,
                            (LPVOID)NULL, 0, (LPVOID)0,
                            0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    GetCommMask    |
*        The GetCommMask function retrieves the value of the event
*        mask for a specified communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDWORD    |    lpfdwEvtMask    |
*        Points to the 32-bit variable to be filled with a mask of
*        events that are currently enabled. This parameter can be
*        one or more of the following values:<nl>
*        @tab2 Value | Meaning
*        @tab2 EV_BREAK | A break was detected on input.
*        @tab2 EV_CTS | The CTS (clear-to-send) signal changed
*            state.
*        @tab2 EV_DSR | The DSR (data-set-ready) signal changed
*            state.
*        @tab2 EV_ERR | A line-status error occurred. Line-status
*            errors are CE_FRAME, CE_OVERRUN, and CE_RXPARITY.
*        @tab2 EV_RING | A ring indicator was detected.
*        @tab2 EV_RLSD | The RLSD (receive-line-signal-detect)
*            signal changed state.
*        @tab2 EV_RXCHAR | A character was received and placed in
*            the input buffer.
*        @tab2 EV_RXFLAG | The event character was received and
*            placed in the input buffer. The event character is
*            specified in the device's DCB structure, which is applied
*            to a serial port by using the SetCommState function.
*        @tab2 EV_TXEMPTY | The last character in the output
*            buffer was sent.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The GetCommMask function uses a 32-bit mask variable to
*        indicate the set of events that can be monitored for a
*        particular communications resource. A handle to the
*        communications resource can be specified in a call to the
*        WaitCommEvent function, which waits for one of the events
*        to occur. To modify the event mask of a communications
*        resource, use the SetCommMask function.
*
*/
BOOL WINAPI
GetCommMask(HANDLE hCommDev, LPDWORD lpfdwEvtMask)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_WAIT_MASK,
                            (LPVOID)NULL, 0, (LPVOID)lpfdwEvtMask,
                            sizeof(DWORD), &dwBytesRet, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    GetCommModemStatus |
*        The GetCommModemStatus function retrieves modem control-register
*        values. 
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    |
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDWORD |    lpfdwModemStats    |
*        Points to a 32-bit variable that specifies the current
*        state of the modem control-register values. This
*        parameter can be a combination of the following values:
*        @tab2 Value | Meaning
*        @tab2 MS_CTS_ON | The CTS (clear-to-send) signal is on.
*        @tab2 MS_DSR_ON | The DSR (data-set-ready) signal is on.
*        @tab2 MS_RING_ON | The ring indicator signal is on.
*        @tab2 MS_RLSD_ON | The RLSD (receive-line-signal-detect)
*            signal is on.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The GetCommModemStatus function is useful when you are
*        using the WaitCommEvent function to monitor the CTS,
*        RLSD, DSR, or ring indicator signals. To detect when
*        these signals change state, use WaitCommEvent and then
*        use GetCommModemStatus to determine the state after a
*        change occurs.  The function fails if the hardware does
*        not support the control-register values.
*
*    @xref    <f CreateFile>
*
*/
BOOL WINAPI
GetCommModemStatus(HANDLE hCommDev, LPDWORD lpfdwModemStats)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_MODEMSTATUS,
                            (LPVOID)NULL, 0, (LPVOID)lpfdwModemStats,
                            sizeof(DWORD), &dwBytesRet, 0);
}
/*****************************************************************************
*
*
*    @func    BOOL    |    GetCommProperties |
*        The GetCommProperties function fills a buffer with
*        information about the communications properties for a
*        specified communications device.
*
*    @rdesc    
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    |
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPCOMMPROP    |    lpCommProp    |
*        Points to a COMMPROP structure in which the
*        communications properties information is returned. This
*        information can be used in subsequent calls to the
*        SetCommState, SetCommTimeouts, or SetupComm function to
*        configure the communications device.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The GetCommProperties function returns information from a
*        device driver about the configuration settings that are
*        supported by the driver.
*
*    @xref    <f SetCommState> <f SetCommTimeouts> <f SetupComm> <f COMMPROP>
*
*/
BOOL WINAPI
GetCommProperties(HANDLE hCommDev, LPCOMMPROP lpCommProp)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_PROPERTIES,
                            (LPVOID)NULL, 0, (LPVOID)lpCommProp,
                            sizeof(COMMPROP), &dwBytesRet, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    GetCommState    |
*        The GetCommState function fills in a device-control block
*        (a DCB structure) with the current control settings for a
*        specified communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDCB    |    lpDCB        |
*        Points to the DCB structure in which the control settings
*        information is returned.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*            This function does stuff.
*
*    @xref    <f SetCommState> <f DCB>
*
*/
BOOL WINAPI
GetCommState(HANDLE hCommDev, LPDCB lpDCB)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_DCB,
                            (LPVOID)0, 0,
                            (LPVOID)lpDCB, sizeof(DCB),
                            (LPDWORD)&dwBytesRet, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    GetCommTimeouts    |
*        The GetCommTimeouts function retrieves the time-out
*        parameters for all read and write operations on a
*        specified communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPCOMMTIMEOUTS    |    lpCommTO    |
*        Points to a COMMTIMEOUTS structure in which the time-out
*        information is returned.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        For more information about time-out values for
*        communications devices, see the SetCommTimeouts function.
*
*    @xref    <f CreateFile> <f COMMTIMEOUTS>
*
*/
BOOL WINAPI
GetCommTimeouts(HANDLE hCommDev, LPCOMMTIMEOUTS lpCommTO)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_GET_TIMEOUTS,
                            (LPVOID)NULL, 0, (LPVOID)lpCommTO,
                            sizeof(COMMTIMEOUTS), &dwBytesRet, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    PurgeComm    |
*        The PurgeComm function can discard all characters from
*        the output or input buffer of a specified communications
*        resource. It can also terminate pending read or write
*        operations on the resource.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    DWORD    |    fdwAction    |
*        Specifies the action to take. This parameter can be a
*        combination of the following values:<nl>
*        @tab2 Value | Meaning
*        @tab2 PURGE_TXABORT | Terminates all outstanding write
*            operations and returns immediately, even if the write
*            operations have not been completed.
*        @tab2 PURGE_RXABORT | Terminates all outstanding read
*            operations and returns immediately, even if the read
*            operations have not been completed.
*        @tab2 PURGE_TXCLEAR | Clears the output buffer (if the
*            device driver has one).
*        @tab2 PURGE_RXCLEAR | Clears the input buffer (if the
*            device driver has one).
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        If a thread uses PurgeComm to flush an output buffer, the
*        deleted characters are not transmitted. To empty the
*        output buffer while ensuring that the contents are
*        transmitted, call the FlushFileBuffers function (a
*        synchronous operation). Note, however, that
*        FlushFileBuffers is subject to flow control but not to
*        write time-outs, and it will not return until all pending
*        write operations have been transmitted.
*
*    @xref    <f CreateFile>
*
*/
BOOL WINAPI
PurgeComm(HANDLE hCommDev, DWORD fdwAction)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_PURGE,
                            (LPVOID)&fdwAction, sizeof(DWORD),
                            (LPVOID)0, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    SetCommBreak    |
*        The SetCommBreak function suspends character transmission
*        for a specified communications device and places the
*        transmission line in a break state until the
*        ClearCommBreak function is called.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The SetCommBreak function does not flush data that has not been
*        transmitted. 
*
*    @xref    <f ClearCommBreak>
*
*/
BOOL WINAPI
SetCommBreak(HANDLE hCommDev)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_BREAK_ON,
                            (LPVOID)NULL, 0, (LPVOID)0,
                            0, &dwBytesReturned, 0);
}


/*****************************************************************************
*
*
*    @func    BOOL    |    SetCommMask    |
*        The SetCommMask function specifies a set of events to be
*        monitored for a communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    DWORD    |    fdwEvtMask    |
*        Specifies the events to be enabled. A value of zero
*        disables all events. This parameter can be a combination
*        of the following values:<nl>
*        @tab2    Value | Meaning
*        @tab2 EV_BREAK | A break was detected on input.
*        @tab2 EV_CTS | The CTS (clear-to-send) signal changed
*            state.
*        @tab2 EV_DSR | The DSR (data-set-ready) signal changed
*            state.
*        @tab2 EV_ERR | A line-status error occurred. Line-status
*            errors are CE_FRAME, CE_OVERRUN, and CE_RXPARITY.
*        @tab2 EV_RING | A ring indicator was detected.
*        @tab2 EV_RLSD | The RLSD (receive-line-signal-detect)
*            signal changed state.
*        @tab2 EV_RXCHAR | A character was received and placed in
*            the input buffer.
*        @tab2 EV_RXFLAG | The event character was received and
*            placed in the input buffer. The event character is
*            specified in the device's DCB structure, which is applied
*            to a serial port by using the SetCommState function.
*        @tab2 EV_TXEMPTY | The last character in the output
*            buffer was sent.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The SetCommMask function specifies the set of events that
*        can be monitored for a particular communications
*        resource. A handle to the communications resource can be
*        specified in a call to the WaitCommEvent function, which
*        waits for one of the events to occur. To get the current
*        event mask of a communications resource, use the
*        GetCommMask function.  If SetCommMask is called for a
*        communications resource while an overlapped wait is
*        pending for that resource, WaitCommEvent returns an
*        error.
*
*    @xref    <f GetCommMask> <f WaitCommEvent>
*
*/
BOOL WINAPI
SetCommMask(HANDLE hCommDev, DWORD fdwEvtMask)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_WAIT_MASK,
                            (LPVOID)&fdwEvtMask, sizeof(DWORD),
                            (LPVOID)0, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    SetCommState    |
*        The SetCommState function configures a communications
*        device according to the specifications in a
*        device-control block (a DCB structure). The function
*        reinitializes all hardware and control settings, but it
*        does not empty output or input queues.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDCB    |    lpDCB    |
*        Points to a DCB structure containing the configuration
*        information for the specified communications device.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The SetCommState function uses a DCB structure to specify
*        the desired configuration. The GetCommState function
*        returns the current configuration.
*
*        To set only a few members of the DCB structure, you
*        should modify a DCB structure that has been filled in by
*        a call to GetCommState. This ensures that the other
*        members of the DCB structure have appropriate values.
*        
*        The SetCommState function fails if the XonChar member
*        of the DCB structure is equal to the XoffChar member.
*
*        When SetCommState is used to configure the 8250, the
*        following restrictions apply to the values for the DCB
*        structure's ByteSize and StopBits members:
*        <nl><tab>The number of data bits must be 5 to 8 bits.
*        <nl><tab>The use of 5 data bits with 2 stop bits is an invalid
*        combination, as are 6, 7, or 8 data bits with 1.5 stop
*        bits.
*
*    @xref    <f BuildCommDCB> <f GetCommState>
*
*/
BOOL WINAPI
SetCommState(HANDLE hCommDev, LPDCB lpDCB)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_DCB,
                            (LPVOID)lpDCB, sizeof(DCB),
                            (LPVOID)0, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    SetCommTimeouts    |
*        The SetCommTimeouts function sets the time-out parameters
*        for all read and write operations on a specified
*        communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPCOMMTIMEOUTS    |    lpCommTO    |
*        Points to a COMMTIMEOUTS structure that contains the new
*        time-out values.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @xref <f GetCommTimeouts> <f ReadFile> <f WriteFile>
*
*
*/
BOOL WINAPI
SetCommTimeouts(HANDLE hCommDev, LPCOMMTIMEOUTS lpCommTO)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_TIMEOUTS,
                            (LPVOID)lpCommTO, sizeof(COMMTIMEOUTS),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    SetupComm    |
*        The SetupComm function initializes the communications
*        parameters for a specified communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    DWORD    |    cbInQueue    |
*        Specifies the recommended size, in bytes, of the device's
*        internal input buffer.
*    @parm    DWORD    |    cbOutQueue    |
*        Specifies the recommended size, in bytes, of the device's
*        internal output buffer.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The cbInQueue and cbOutQueue parameters specify the
*        recommended sizes for the internal buffers used by the
*        driver for the specified device. For example, YMODEM
*        protocol packets are slightly larger than 1024
*        bytes. Therefore, a recommended buffer size might be 1200
*        bytes for YMODEM communications. For Ethernet-based
*        communications, a recommended buffer size might be 1600
*        bytes, which is slightly larger than a single Ethernet
*        frame.
*
*        The device driver receives the recommended buffer sizes,
*        but is free to use any input and output (I/O) buffering
*        scheme, as long as it provides reasonable performance and
*        data is not lost due to overrun (except under extreme
*        circumstances). For example, the function can succeed
*        even though the driver does not allocate a buffer, as
*        long as some other portion of the operating system
*        provides equivalent functionality.
*
*        If the device driver determines that the recommended
*        buffer sizes involve transfers beyond its ability to
*        handle, the function can fail.
*
*    @xref    <f SetCommState>
*
*/
BOOL WINAPI
SetupComm(HANDLE hCommDev, DWORD cbInQueue, DWORD cbOutQueue)
{
    SERIAL_QUEUE_SIZES    SerQueueSizes;
    DWORD    dwBytesReturned;

    SerQueueSizes.cbInQueue = cbInQueue;
    SerQueueSizes.cbOutQueue = cbOutQueue;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_SET_QUEUE_SIZE,
                            (LPVOID)&SerQueueSizes, sizeof(SerQueueSizes),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    TransmitCommChar    |
*        The TransmitCommChar function transmits a specified
*        character ahead of any pending data in the output buffer
*        of the specified communications device.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    char    |    chTransmit    |
*        Specifies the character to be transmitted. 
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> No restrictions.
*
*    @remark
*        The TransmitCommChar function is useful for sending an
*        interrupt character (such as a CTRL+C) to a host system.
*
*        If the device is not transmitting, TransmitCommChar
*        cannot be called repeatedly. Once TransmitCommChar places
*        a character in the output buffer, the character must be
*        transmitted before the function can be called again. If
*        the previous character has not yet been sent,
*        TransmitCommChar returns an error.
*
*        Character transmission is subject to normal flow control
*        and handshaking. This function can only be called
*        synchronously.
*
*    @xref    <f WaitCommEvent>
*
*/
BOOL WINAPI
TransmitCommChar(HANDLE hCommDev, char chTransmit)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (hCommDev, IOCTL_SERIAL_IMMEDIATE_CHAR,
                            (LPVOID)&chTransmit, sizeof(char),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);
}

/*****************************************************************************
*
*
*    @func    BOOL    |    WaitCommEvent |
*        The WaitCommEvent function waits for an event to occur
*        for a specified communications device. The set of events
*        that are monitored by this function is contained in the
*        event mask associated with the device handle.
*
*    @rdesc
*        If the function succeeds, the return value is TRUE.  If
*        the function fails, the return value is FALSE. To get
*        extended error information, call <f GetLastError>.
*
*    @parm    HANDLE    |    hCommDev    | 
*        Identifies the communications device. The CreateFile
*        function returns this handle.
*    @parm    LPDWORD    |    lpfdwEvtMask    |
*        Points to a 32-bit variable that receives a mask
*        indicating the type of event that occurred. If an error
*        occurs, the value is zero; otherwise, it is one of the
*        following values:<nl>
*        @tab2    Value | Meaning
*        @tab2 EV_BREAK | A break was detected on input.
*        @tab2 EV_CTS | The CTS (clear-to-send) signal changed
*            state.
*        @tab2 EV_DSR | The DSR (data-set-ready) signal changed
*            state.
*        @tab2 EV_ERR | A line-status error occurred. Line-status
*            errors are CE_FRAME, CE_OVERRUN, and CE_RXPARITY.
*        @tab2 EV_RING | A ring indicator was detected.
*        @tab2 EV_RLSD | The RLSD (receive-line-signal-detect)
*            signal changed state.
*        @tab2 EV_RXCHAR | A character was received and placed in
*            the input buffer.
*        @tab2 EV_RXFLAG | The event character was received and
*            placed in the input buffer. The event character is
*            specified in the device's DCB structure, which is applied
*            to a serial port by using the SetCommState function.
*        @tab2 EV_TXEMPTY | The last character in the output
*            buffer was sent.
*    @parm    LPOVERLAPPED | lpOvLap        |
*        Points to an OVERLAPPED structure. This parameter is
*        ignored if the hCommDev handle was opened without
*        specifying the FILE_FLAG_OVERLAPPED flag. If an
*        overlapped operation is not desired, this parameter can
*        be NULL.
*
*    @devnote
*        Follows the Win32 reference description with these
*        restrictions: <nl><b M5> The lpOvLap parameter is not supported
*        and ignored.
*
*    @remark
*        The WaitCommEvent function monitors a set of events for a
*        specified communications resource. To set and query the
*        current event mask of a communications resource, use the
*        SetCommMask and GetCommMask functions.
*
*        If the lpo parameter is NULL or the hCommDev handle was
*        opened without specifying the FILE_FLAG_OVERLAPPED flag,
*        WaitCommEvent does not return until one of the specified
*        events or an error occurs.
*
*        If lpOvLap points to an OVERLAPPED structure and
*        FILE_FLAG_OVERLAPPED was specified when the hCommDev
*        handle was opened, WaitCommEvent is performed as an
*        overlapped operation. In this case, the OVERLAPPED
*        structure must contain a handle to a manual-reset event
*        object (created by using the CreateEvent function).
*
*        If the overlapped operation cannot be completed
*        immediately, the function returns FALSE and the
*        GetLastError function returns ERROR_IO_PENDING,
*        indicating that the operation is executing in the
*        background. When this happens, the system sets the hEvent
*        member of the OVERLAPPED structure to the not-signaled
*        state before WaitCommEvent returns, and then it sets it
*        to the signaled state when one of the specified events or
*        an error occurs. The calling process can use a wait
*        function to determine the event object's state and then
*        use the GetOverlappedResult function to determine the
*        results of the WaitCommEvent
*        operation. GetOverlappedResult reports the success or
*        failure of the operation, and the variable pointed to by
*        the lpfdwEvtMask parameter is set to indicate the event
*        that occurred.
*
*        If a process attempts to change the device handle's event
*        mask by using the SetCommMask function while an
*        overlapped WaitCommEvent operation is in progress,
*        WaitCommEvent returns immediately. The variable pointed
*        to by the lpfdwEvtMask parameter is set to zero.
*
*    @xref    <f GetCommMask> <f SetCommMask>
*
*/
BOOL WINAPI
WaitCommEvent(HANDLE hCommDev, LPDWORD lpfdwEvtMask, LPOVERLAPPED lpOvLap)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (hCommDev, IOCTL_SERIAL_WAIT_ON_MASK,
                            (LPVOID)0, 0,
                            (LPVOID)lpfdwEvtMask, sizeof(DWORD),
                            (LPDWORD)&dwBytesRet, lpOvLap);
}
