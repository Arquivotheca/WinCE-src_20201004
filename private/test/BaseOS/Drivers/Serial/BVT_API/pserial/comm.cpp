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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  2003  Microsoft Corporation.  All Rights Reserved.

Module Name:

     Comm.cpp  

Abstract:
Functions:
Notes:
*/
#include <Windows.h>
#include <stdio.h>
#include <TCHAR.h>
#include "comm.h"
#include "pserial.h"
#include "ntddser.h"
#define __THIS_FILE__ TEXT("Comm.cpp")
CommPort::~CommPort( ) 
{
    CloseHandle( ); 

};
HANDLE CommPort::CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile)
{
    m_hComm=::CreateFile(lpFileName, dwDesiredAccess,  dwShareMode, lpSecurityAttributes, 
      dwCreationDispostion,dwFlagsAndAttributes,hTemplateFile);
    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Open Device by Name(%s) return handle =%x"),
                  __THIS_FILE__, __LINE__, lpFileName,m_hComm );
    return m_hComm;
}
BOOL CommPort::CloseHandle( )
{
    if (IsValid()) {
        ::CloseHandle(m_hComm);
        m_hComm=INVALID_HANDLE_VALUE;
        return TRUE;
    }
    else
        return FALSE;
}
BOOL CommPort::ReadFile( LPVOID lpBuffer,  DWORD nNumberOfBytesToRead,   LPDWORD lpNumberOfBytesRead,   LPOVERLAPPED lpOverlapped) 
{
    return ::ReadFile(m_hComm,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,lpOverlapped);
}
BOOL CommPort::WriteFile( LPCVOID lpBuffer,   DWORD nNumberOfBytesToWrite,   LPDWORD lpNumberOfBytesWritten,   LPOVERLAPPED lpOverlapped)
{
    return ::WriteFile(m_hComm,lpBuffer,nNumberOfBytesToWrite,lpNumberOfBytesWritten,lpOverlapped);
}
BOOL CommPort::ClearCommBreak()
{
    return ::ClearCommBreak(m_hComm);
}
BOOL CommPort::ClearCommError( LPDWORD lpErrors,  LPCOMSTAT lpStat)
{
    return ::ClearCommError(m_hComm,lpErrors,lpStat);
}
BOOL CommPort::EscapeCommFunction(  DWORD dwFunc)
{
    return ::EscapeCommFunction(m_hComm,dwFunc);
}
BOOL CommPort::GetCommMask( LPDWORD lpEvtMask)
{
    return ::GetCommMask(m_hComm,lpEvtMask);
}
BOOL CommPort::GetCommModemStatus(  LPDWORD lpModemStat)
{
    return ::GetCommModemStatus(m_hComm,lpModemStat);
}
BOOL CommPort::GetCommProperties( LPCOMMPROP lpCommProp)
{
    return ::GetCommProperties(m_hComm, lpCommProp);
}
BOOL CommPort::GetCommState(  LPDCB lpDCB)
{
    return ::GetCommState(m_hComm,lpDCB);
}
BOOL CommPort::GetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts)
{
    return ::GetCommTimeouts(m_hComm,lpCommTimeouts);
}
BOOL CommPort::SetCommBreak()
{
    return ::SetCommBreak(m_hComm);
}
BOOL CommPort::SetCommMask( DWORD dwEvtMask)
{
    return ::SetCommMask(m_hComm,dwEvtMask);
}
BOOL CommPort::PurgeComm(  DWORD dwFlags )
{
    return ::PurgeComm(m_hComm, dwFlags);
}
BOOL CommPort::SetCommState(  LPDCB lpDCB)
{
    return ::SetCommState(m_hComm,lpDCB);
}
BOOL CommPort::SetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts)
{
    return ::SetCommTimeouts(m_hComm,lpCommTimeouts);
}
BOOL CommPort::SetupComm(  DWORD dwInQueue,   DWORD dwOutQueue)
{
    return ::SetupComm(m_hComm,dwInQueue, dwOutQueue);
}
BOOL CommPort::TransmitCommChar( char cChar)
{
    return ::TransmitCommChar(m_hComm,cChar);
}
BOOL CommPort::WaitCommEvent( LPDWORD lpEvtMask,  LPOVERLAPPED lpOverlapped)
{
    return ::WaitCommEvent(m_hComm,lpEvtMask,lpOverlapped);
}
HANDLE CommDevice::CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile)
{
    TCHAR deviceName[0x200];

    swprintf_s(deviceName, _countof(deviceName),TEXT("\\\\.\\%s"),lpFileName);
    deviceName[0x200-1]=NULL;    //fixing a prefast bug

    m_hComm=::CreateFile(deviceName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDispostion,dwFlagsAndAttributes,hTemplateFile);
    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Open Device by Name(%s) return handle =%x"),
                  __THIS_FILE__, __LINE__, deviceName,m_hComm );

    return m_hComm;
}
BOOL CommDevice::ClearCommBreak()
{
    DWORD dwReturnLength;
    return ::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_BREAK_OFF, NULL, 0, NULL, 0, &dwReturnLength, NULL);
}
BOOL CommDevice::ClearCommError( LPDWORD lpdwErrors, LPCOMSTAT lpCSt)
{
    DWORD            dwBytesRet;
    SERIAL_STATUS    SerDevStat;

    if (TRUE == ::DeviceIoControl (m_hComm, IOCTL_SERIAL_GET_COMMSTATUS,
                            (LPVOID)NULL, 0, (LPVOID)&SerDevStat,
                            sizeof(SERIAL_STATUS), &dwBytesRet, 0)) {
        *lpdwErrors = SerDevStat.Errors;
        if (NULL != lpCSt) {
            memcpy ((char *)lpCSt, (char *)&(SerDevStat.HoldReasons),
                    sizeof(COMSTAT));
        }
        return TRUE;
    }
    return FALSE;

}
BOOL CommDevice::EscapeCommFunction(  DWORD fdwFunc)
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
    case SETBREAK :                    // Set the device break line.
        dwCode = IOCTL_SERIAL_SET_BREAK_ON;
        break;
    case CLRBREAK :                    // Clear the device break line.
        dwCode = IOCTL_SERIAL_SET_BREAK_OFF;
        break;
    default :
        break;
    }

    if ((DWORD)-1 == dwCode) {
        ::SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return ::DeviceIoControl (m_hComm, dwCode,
                            (LPVOID)NULL, 0, (LPVOID)0,
                            0, &dwBytesReturned, 0);
}
BOOL CommDevice::GetCommMask(  LPDWORD lpfdwEvtMask)
{
    DWORD    dwBytesRet;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_GET_WAIT_MASK,
                            (LPVOID)NULL, 0, (LPVOID)lpfdwEvtMask,
                            sizeof(DWORD), &dwBytesRet, 0);

}
BOOL CommDevice::GetCommModemStatus(  LPDWORD lpfdwModemStats)
{
    DWORD    dwBytesRet;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_GET_MODEMSTATUS,
                            (LPVOID)NULL, 0, (LPVOID)lpfdwModemStats,
                            sizeof(DWORD), &dwBytesRet, 0);
}
BOOL CommDevice::GetCommProperties(  LPCOMMPROP lpCommProp)
{
    DWORD    dwBytesRet;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_GET_PROPERTIES,
                            (LPVOID)NULL, 0, (LPVOID)lpCommProp,
                            sizeof(COMMPROP), &dwBytesRet, 0);

}
typedef struct _BuatRateTable {
    DWORD appRate;
    DWORD driverRate;
}BautRateTable;
const BautRateTable baudrate[] = {
{    CBR_110 ,SERIAL_BAUD_110},
{ CBR_300 ,SERIAL_BAUD_300   },
{ CBR_600 ,SERIAL_BAUD_600   },
{ CBR_1200 ,SERIAL_BAUD_1200  },
{ CBR_2400 ,SERIAL_BAUD_2400  },
{ CBR_4800 ,SERIAL_BAUD_4800  },
{ CBR_9600 ,SERIAL_BAUD_9600  },
{ CBR_14400 ,SERIAL_BAUD_14400 },
{ CBR_19200 ,SERIAL_BAUD_19200 },
{ CBR_38400 ,SERIAL_BAUD_38400 },
{ CBR_56000 ,SERIAL_BAUD_56K  },
{ CBR_57600 ,SERIAL_BAUD_57600 },
{ CBR_115200 ,SERIAL_BAUD_115200},
{ CBR_128000 ,SERIAL_BAUD_128K },
{ 0,0}
};
BOOL CommDevice::GetCommState(  LPDCB lpDCB)
{
    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: GetCommState  handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
    DWORD    dwBytesRet;
    if (lpDCB->DCBlength<sizeof(DCB)) {
        ::SetLastError (ERROR_INVALID_PARAMETER);
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: GetCommState  lpDCB->Length=%d<%d"),
                  __THIS_FILE__, __LINE__,lpDCB->DCBlength, sizeof(DCB));
        return FALSE;        
    };
    SERIAL_HANDFLOW serialHandFlow;
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_GET_HANDFLOW,
            (LPVOID)NULL, 0,(LPVOID)&serialHandFlow,sizeof( serialHandFlow),
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_GET_HANDFLOW return false, handle =%x"),
                  __THIS_FILE__, __LINE__, m_hComm );
        return FALSE;
    }
    DWORD dwDTRRTS;
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_GET_DTRRTS,
            (LPVOID)NULL, 0,(LPVOID)&dwDTRRTS,sizeof( dwDTRRTS),
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_GET_DTRRTS return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }        
    SERIAL_BAUD_RATE serialBuadRate;
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_GET_BAUD_RATE,
            (LPVOID)NULL, 0,(LPVOID)&serialBuadRate,sizeof( serialBuadRate),
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_GET_BAUD_RATE return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    for (DWORD dwIndex=0;baudrate[dwIndex].driverRate!=0;dwIndex++)  {
        if (serialBuadRate.BaudRate == baudrate[dwIndex].driverRate)
            break;
    }
    SERIAL_LINE_CONTROL serialLineControl;
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_GET_LINE_CONTROL,
            (LPVOID)NULL, 0,(LPVOID)&serialLineControl,sizeof(serialLineControl),
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_GET_LINE_CONTROL return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    SERIAL_CHARS serialChars;
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_GET_CHARS,
            (LPVOID)NULL, 0,(LPVOID)&serialChars,sizeof(serialChars),
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_GET_CHARS return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    lpDCB->BaudRate=(baudrate[dwIndex].driverRate!=0?baudrate[dwIndex].appRate:CBR_9600);  
    lpDCB->fBinary=1;
    lpDCB->fParity= ((serialLineControl.Parity!=SERIAL_PARITY_NONE)?1:0);  
    lpDCB->fOutxCtsFlow = ((serialHandFlow.ControlHandShake & SERIAL_CTS_HANDSHAKE)!=0?1:0) ;
    lpDCB->fOutxDsrFlow =((serialHandFlow.ControlHandShake & SERIAL_DSR_HANDSHAKE)!=0?1:0);
    lpDCB->fDtrControl= ((serialHandFlow.ControlHandShake & SERIAL_DTR_HANDSHAKE)!=0?DTR_CONTROL_HANDSHAKE:
        (dwDTRRTS & SERIAL_DTR_STATE)!=0?DTR_CONTROL_ENABLE:DTR_CONTROL_DISABLE);
    lpDCB->fDsrSensitivity = ((serialHandFlow.ControlHandShake & SERIAL_DSR_HANDSHAKE)!=0?1:0);
    lpDCB->fTXContinueOnXoff = ((serialHandFlow.FlowReplace &  SERIAL_XOFF_CONTINUE)!=0?1:0);
    lpDCB->fOutX =((serialHandFlow.FlowReplace & SERIAL_AUTO_TRANSMIT)!=0?1:0);  //????
    lpDCB->fInX  =((serialHandFlow.FlowReplace & SERIAL_AUTO_RECEIVE)!=0?1:0);  //????
    lpDCB->fErrorChar =((serialHandFlow.FlowReplace & SERIAL_ERROR_CHAR)!=0?1:0);
    lpDCB->fNull  = ((serialHandFlow.FlowReplace & SERIAL_NULL_STRIPPING)!=0?1:0);
    lpDCB->fRtsControl = ((serialHandFlow.FlowReplace & SERIAL_RTS_MASK)== SERIAL_TRANSMIT_TOGGLE? RTS_CONTROL_TOGGLE:
        ((serialHandFlow.FlowReplace & SERIAL_RTS_MASK)==SERIAL_RTS_HANDSHAKE?RTS_CONTROL_HANDSHAKE:
            ((dwDTRRTS  & SERIAL_RTS_STATE)!=0?RTS_CONTROL_ENABLE:RTS_CONTROL_DISABLE)));
    lpDCB->fAbortOnError= ((serialHandFlow.ControlHandShake & SERIAL_ERROR_ABORT)!=0?1:0);
    lpDCB->XonLim=(WORD)serialHandFlow.XonLimit; 
    lpDCB->XoffLim=(WORD)serialHandFlow.XoffLimit;
    lpDCB->ByteSize=serialLineControl.WordLength;
    lpDCB->Parity=serialLineControl.Parity;
    lpDCB->StopBits=serialLineControl.StopBits;
    lpDCB->XonChar=serialChars.XonChar;
    lpDCB->XoffChar=serialChars.XoffChar;
    lpDCB->ErrorChar=serialChars.ErrorChar;
    lpDCB->EofChar=serialChars.EofChar;
    lpDCB->EvtChar=serialChars.EventChar; 
    return TRUE;
}
BOOL CommDevice::GetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts)
{
    DWORD    dwBytesRet;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_GET_TIMEOUTS,
                            (LPVOID)NULL, 0, (LPVOID)lpCommTimeouts,
                            sizeof(COMMTIMEOUTS), &dwBytesRet, 0);
}
BOOL CommDevice::PurgeComm(  DWORD dwFlags )
{
    DWORD    dwBytesReturned;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_PURGE,
                            (LPVOID)&dwFlags, sizeof(DWORD),
                            (LPVOID)0, 0, &dwBytesReturned, 0);

}
BOOL CommDevice::SetCommBreak()
{
    DWORD    dwBytesReturned;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_SET_BREAK_ON,
                            (LPVOID)NULL, 0, (LPVOID)0,
                            0, &dwBytesReturned, 0);
}

BOOL CommDevice::SetCommMask(  DWORD dwEvtMask)
{
    DWORD    dwBytesReturned;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_SET_WAIT_MASK,
                            (LPVOID)& dwEvtMask, sizeof(DWORD),
                            (LPVOID)0, 0, &dwBytesReturned, 0);

}
BOOL CommDevice::SetCommState(  LPDCB lpDCB)
{
    DWORD dwBytesRet = 0;
    if (lpDCB->DCBlength<sizeof(DCB)) {
        ::SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;        
    };
    SERIAL_HANDFLOW serialHandFlow;
    memset(&serialHandFlow,0,sizeof(SERIAL_HANDFLOW));
    DWORD dwDTRRTS=0;
    SERIAL_BAUD_RATE serialBuadRate;
    memset(&serialBuadRate,0,sizeof(serialBuadRate));
    SERIAL_LINE_CONTROL serialLineControl;
    memset(&serialLineControl,0,sizeof(serialLineControl));


    for (DWORD dwIndex=0;baudrate[dwIndex].appRate!=0;dwIndex++)  {
        if (lpDCB->BaudRate == baudrate[dwIndex].appRate)
            break;
    }
    serialBuadRate.BaudRate = ((baudrate[dwIndex].appRate!=0)?baudrate[dwIndex].driverRate:SERIAL_BAUD_9600);
    serialHandFlow.ControlHandShake |= ((lpDCB->fOutxCtsFlow!=0)?SERIAL_CTS_HANDSHAKE:0);
    serialHandFlow.ControlHandShake |= ((lpDCB->fOutxDsrFlow!=0)?SERIAL_DSR_HANDSHAKE:0);
    serialHandFlow.ControlHandShake |= ((lpDCB->fDtrControl==DTR_CONTROL_HANDSHAKE)?SERIAL_DTR_HANDSHAKE:SERIAL_DTR_CONTROL);
    dwDTRRTS|=((lpDCB->fDtrControl==DTR_CONTROL_DISABLE)?0:SERIAL_DTR_STATE);
    serialHandFlow.ControlHandShake |= ((lpDCB->fDsrSensitivity!=0)?SERIAL_DSR_SENSITIVITY:0);
    serialHandFlow.FlowReplace |= ((lpDCB->fOutX!=0)?SERIAL_AUTO_TRANSMIT:0);
    serialHandFlow.FlowReplace |= ((lpDCB->fInX!=0)?SERIAL_AUTO_RECEIVE:0);
    serialHandFlow.FlowReplace |= ((lpDCB->fTXContinueOnXoff!=0)?SERIAL_XOFF_CONTINUE:0);
    serialHandFlow.FlowReplace |= ((lpDCB->fErrorChar!=0)?SERIAL_ERROR_CHAR:0);
    serialHandFlow.FlowReplace |= ((lpDCB->fNull!=0)?SERIAL_NULL_STRIPPING:0);
    serialHandFlow.FlowReplace |=  ((lpDCB->fRtsControl==RTS_CONTROL_TOGGLE)?SERIAL_TRANSMIT_TOGGLE:
        ((lpDCB->fRtsControl==RTS_CONTROL_HANDSHAKE)?SERIAL_RTS_HANDSHAKE:SERIAL_RTS_CONTROL));
    dwDTRRTS |= ((lpDCB->fRtsControl==RTS_CONTROL_DISABLE)?0:SERIAL_RTS_STATE);
    serialHandFlow.ControlHandShake |=((lpDCB->fAbortOnError!=0)?SERIAL_ERROR_ABORT:0);
    serialHandFlow.XonLimit =  lpDCB->XonLim;
    serialHandFlow.XoffLimit = lpDCB->XoffLim;
    serialLineControl.WordLength=lpDCB->ByteSize;
    serialLineControl.Parity=lpDCB->Parity;
    serialLineControl.StopBits=lpDCB->StopBits;
    SERIAL_CHARS serialChars;
    serialChars.XonChar=lpDCB->XonChar;
    serialChars.XoffChar=lpDCB->XoffChar;
    serialChars.ErrorChar=lpDCB->ErrorChar;
    serialChars.EofChar=lpDCB->EofChar;
    serialChars.EventChar=lpDCB->EvtChar; 
    
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_HANDFLOW,
            (LPVOID)&serialHandFlow,sizeof( serialHandFlow),(LPVOID)NULL, 0,
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_SET_HANDFLOW return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    BOOL bReturn;
    if (lpDCB->fDtrControl==DTR_CONTROL_DISABLE)
        bReturn=(::DeviceIoControl(m_hComm,IOCTL_SERIAL_CLR_DTR,NULL,0,(LPVOID)NULL, 0, &dwBytesRet,0));
    else
    if (lpDCB->fDtrControl==DTR_CONTROL_ENABLE)
        bReturn=(::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_DTR,NULL,0,(LPVOID)NULL, 0, &dwBytesRet,0));
    else
        bReturn=TRUE;
    if (!bReturn) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: DTR_CONTROL_DISABLE or IOCTL_SERIAL_SET_DTR,NULL return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    
    if (lpDCB->fRtsControl==RTS_CONTROL_DISABLE)
        bReturn=(::DeviceIoControl(m_hComm,IOCTL_SERIAL_CLR_RTS,NULL,0,(LPVOID)NULL, 0, &dwBytesRet,0));
    else
    if (lpDCB->fRtsControl==RTS_CONTROL_ENABLE)
        bReturn=(::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_RTS,NULL,0,(LPVOID)NULL, 0, &dwBytesRet,0));
    else
        bReturn=TRUE;
    if (!bReturn) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_CLR_RTS or IOCTL_SERIAL_SET_RTS,,NULL return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_BAUD_RATE,
            (LPVOID)&serialBuadRate,sizeof( serialBuadRate),(LPVOID)NULL, 0,
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_SET_BAUD_RATE return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_LINE_CONTROL,
            (LPVOID)&serialLineControl,sizeof(serialLineControl),(LPVOID)NULL, 0,
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_SET_LINE_CONTROL return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    if (!::DeviceIoControl(m_hComm,IOCTL_SERIAL_SET_CHARS,
            (LPVOID)&serialChars,sizeof(serialChars),(LPVOID)NULL, 0,
            &dwBytesRet,0)) {
        g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d: IOCTL_SERIAL_SET_CHARS return false, handle =%x"),
                  __THIS_FILE__, __LINE__,m_hComm );
        return FALSE;
    }
    return TRUE;

}
BOOL CommDevice::SetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts)
{
    DWORD    dwBytesReturned;
    return ::DeviceIoControl (m_hComm, IOCTL_SERIAL_SET_TIMEOUTS,
                            (LPVOID)lpCommTimeouts, sizeof(COMMTIMEOUTS),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);
}

BOOL CommDevice::SetupComm(  DWORD dwInQueue,   DWORD dwOutQueue)
{
    SERIAL_QUEUE_SIZE    SerQueueSizes;
    DWORD    dwBytesReturned;

    SerQueueSizes.InSize =  dwInQueue;
    SerQueueSizes.OutSize = dwOutQueue;
    return DeviceIoControl (m_hComm, IOCTL_SERIAL_SET_QUEUE_SIZE,
                            (LPVOID)&SerQueueSizes, sizeof(SerQueueSizes),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);

}
BOOL CommDevice::TransmitCommChar( char cChar)
{
    DWORD    dwBytesReturned;
    return DeviceIoControl (m_hComm, IOCTL_SERIAL_IMMEDIATE_CHAR,
                            (LPVOID)&cChar, sizeof(char),
                            (LPVOID)NULL, 0, &dwBytesReturned, 0);

}
BOOL CommDevice::WaitCommEvent( LPDWORD lpEvtMask,  LPOVERLAPPED lpOverlapped)
{
    DWORD    dwBytesRet;

    return DeviceIoControl (m_hComm, IOCTL_SERIAL_WAIT_ON_MASK,
                            (LPVOID)0, 0,
                            (LPVOID)lpEvtMask, sizeof(DWORD),
                            (LPDWORD)&dwBytesRet, lpOverlapped);

}
CommPort * CreateCommObject(BOOL bDevice)
{
    return (bDevice?new CommDevice():new CommPort);
}

