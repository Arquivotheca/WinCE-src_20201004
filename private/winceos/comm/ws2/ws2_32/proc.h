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
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    proc.h

    This file contains global procedure declarations for WINSOCK.DLL
    and WSOCK32.DLL.



*/


#ifndef _PROC_H_
#define _PROC_H_


//
//  Miscellaneous utilities.
//

INT
GetSockInfo(
    SOCKET s,
    LPSOCK_INFO FAR * sock
    );

BOOL
enterAPI(
    BOOL started,
    BOOL blocking,
    BOOL getXbyY,
    LPSOCK_THREAD FAR * pthread
    );

SOCKET
GetNewSocketHandle(
    VOID
    );

VOID
FreeSocketHandle(
    SOCKET sock
    );

VOID
AbortSocket(
	LPSOCK_INFO sock
	);

BOOL
GetThreadData (
	LPSOCK_THREAD *ppThread
	);


//
//  Portable internal SetLastError utility.
//

#define I_SetLastError(t,e)     SetLastError(e)

//
//  Platform-dependent context code.
//

//
//  Platform-dependent VxD access from VXD[16|32].C.
//

//
//  Platform-dependent DLL routines in DLL[16|32].C.
//

//
//  Portable alloc/free implementations.
//

__inline
LPVOID
DllAllocMem(DWORD Length)
{
    return (LPVOID)LocalAlloc( 0, Length );
}   // DllAllocMem


__inline
VOID
DllFreeMem(LPVOID Buffer)
{
	LocalFree( (HGLOBAL)Buffer );
}   // DllFreeMem

#ifdef DEBUG

extern LPTSTR lpszWsDllCSOwner;

__inline VOID EnterDllCritSec(LPTSTR lpszFunc)
{
    LPTSTR lpszOwner = lpszWsDllCSOwner;
    if (v_DllCS.OwnerThread)
    {
        DEBUGMSG(ZONE_MISC,
                 (TEXT("WS:[%s] Wait for CS (0x%.8X) locked by %s.\r\n"),
                  lpszFunc, &v_DllCS, lpszOwner)
                 );
    }

    EnterCriticalSection(&v_DllCS);

    DEBUGMSG(ZONE_MISC,
             (TEXT("WS:[%s] EnterCS (0x%.8X). LockCount = %d. OwnerThread = 0x%.8X\r\n"),
              lpszFunc, &v_DllCS, v_DllCS.LockCount, v_DllCS.OwnerThread)
             );

    if (v_DllCS.LockCount > 1)
    {
        DEBUGMSG(ZONE_MISC,
                 (TEXT("WS: CS prev owner [%s].\r\n"),
                  lpszWsDllCSOwner)
                 );
    }

    lpszWsDllCSOwner = lpszFunc;
}

__inline VOID LeaveDllCritSec()
{
    DEBUGMSG(ZONE_MISC,
             (TEXT("WS: [%s] LeaveCS. LockCount = %d.\r\n"),
              lpszWsDllCSOwner, v_DllCS.LockCount - 1)
             );

    lpszWsDllCSOwner = TEXT("");
    LeaveCriticalSection(&v_DllCS);
}

#define ENTER_DLL_CS(s)    EnterDllCritSec(s)
#define LEAVE_DLL_CS()     LeaveDllCritSec()

#else // DEBUG

#define ENTER_DLL_CS(s)    EnterCriticalSection(&v_DllCS)
#define LEAVE_DLL_CS()     LeaveCriticalSection(&v_DllCS)

#endif // !DEBUG

#endif  // _PROC_H_

