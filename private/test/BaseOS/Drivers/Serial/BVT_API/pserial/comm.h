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

     pserial.h  

Abstract:
Functions:
Notes:
--*/
#ifndef __COMM_H_
#define __COMM_H_
#include <Windows.h>
class CommPort {
public :
    CommPort( ) { m_hComm = INVALID_HANDLE_VALUE;};
    virtual ~CommPort();
    BOOL    IsValid() { return m_hComm!=INVALID_HANDLE_VALUE; };
    // Function Supported.
    virtual HANDLE CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile);
    virtual BOOL CloseHandle( ); 
    virtual BOOL ReadFile( LPVOID lpBuffer,  DWORD nNumberOfBytesToRead,   LPDWORD lpNumberOfBytesRead,   LPOVERLAPPED lpOverlapped) ;
    virtual BOOL WriteFile( LPCVOID lpBuffer,   DWORD nNumberOfBytesToWrite,   LPDWORD lpNumberOfBytesWritten,   LPOVERLAPPED lpOverlapped); 
    virtual BOOL ClearCommBreak(); 
    virtual BOOL ClearCommError( LPDWORD lpErrors,  LPCOMSTAT lpStat); 
    virtual BOOL EscapeCommFunction(  DWORD dwFunc); 
    virtual BOOL GetCommMask( LPDWORD lpEvtMask); 
    virtual BOOL GetCommModemStatus(  LPDWORD lpModemStat); 
    virtual BOOL GetCommProperties( LPCOMMPROP lpCommProp); 
    virtual BOOL GetCommState(  LPDCB lpDCB); 
    virtual BOOL GetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts); 
    virtual BOOL PurgeComm(  DWORD dwFlags ); 
    virtual BOOL SetCommBreak(); 
    virtual BOOL SetCommMask( DWORD dwEvtMask); 
    virtual BOOL SetCommState(  LPDCB lpDCB); 
    virtual BOOL SetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts); 
    virtual BOOL SetupComm(  DWORD dwInQueue,   DWORD dwOutQueue); 
    virtual BOOL TransmitCommChar( char cChar); 
    virtual BOOL WaitCommEvent( LPDWORD lpEvtMask,  LPOVERLAPPED lpOverlapped); 
protected:
    HANDLE m_hComm;
};
class CommDevice :public CommPort {
public:
    CommDevice() {};
    ~CommDevice() {};
    // Function Supported.
    virtual HANDLE CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile);
    virtual BOOL ClearCommBreak(); 
    virtual BOOL ClearCommError( LPDWORD lpErrors,  LPCOMSTAT lpStat); 
    virtual BOOL EscapeCommFunction(  DWORD dwFunc); 
    virtual BOOL GetCommMask( LPDWORD lpEvtMask); 
    virtual BOOL GetCommModemStatus(  LPDWORD lpModemStat); 
    virtual BOOL GetCommProperties( LPCOMMPROP lpCommProp); 
    virtual BOOL GetCommState(  LPDCB lpDCB); 
    virtual BOOL GetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts); 
    virtual BOOL PurgeComm(  DWORD dwFlags ); 
    virtual BOOL SetCommBreak(); 
    virtual BOOL SetCommMask(  DWORD dwEvtMask); 
    virtual BOOL SetCommState(  LPDCB lpDCB); 
    virtual BOOL SetCommTimeouts(  LPCOMMTIMEOUTS lpCommTimeouts); 
    virtual BOOL SetupComm(  DWORD dwInQueue,   DWORD dwOutQueue); 
    virtual BOOL TransmitCommChar( char cChar); 
    virtual BOOL WaitCommEvent( LPDWORD lpEvtMask,  LPOVERLAPPED lpOverlapped); 
};
CommPort * CreateCommObject(BOOL bDevice);
#endif
