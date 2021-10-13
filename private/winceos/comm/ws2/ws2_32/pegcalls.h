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
    pegcalls.h

    Inline functions for interfacing to the Sockets Kernel module


*/


#ifndef _PEGCALLS_H_
#define _PEGCALLS_H_


// Until we have real OS support
//extern PWINSOCKDispatchTable v_pWSDispatch;

#define OsBlockingHook(pthread, WaitTime)	// do nothing with this for now
/*		 { \
		pthread->st_status.IoCancelled = FALSE; \
		pthread->st_status.IoTimedOut = FALSE; \
		pthread->st_status.IoStatus = 0; }
*/
#define CALL_WINSOCK(x)	WSAESOCKTNOSUPPORT

//
//  Socket APIs.
//





INT vxd_accept(
	LPSOCK_INFO       ListeningSocket,
	LPSOCK_INFO FAR * ConnectedSocket,
    LPVOID            Address,
    LPINT             AddressLength,
    SOCKET            ConnectedSocketHandle,
    LPVOID            ApcRoutine,
    LPVOID            ApcContext
    );


INT
vxd_bind(
    LPSOCK_INFO Socket,
    LPVOID      Address,
    INT         AddressLength,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext
    );

INT
vxd_closesocket(
    LPSOCK_INFO Socket
    );

INT
vxd_connect(
    LPSOCK_INFO Socket,
    LPVOID      Address,
    INT         AddressLength,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext
    );

#if 0
INT
vxd_getsockopt(
    LPSOCK_INFO Socket,
    INT         OptionLevel,
    INT         OptionName,
    LPVOID      Value,
    LPINT       ValueLength
    );


INT
vxd_ioctlsocket(
    LPSOCK_INFO Socket,
    LONG        Command,
    LPULONG     Argp
    );
#endif

INT
vxd_listen(
    LPSOCK_INFO Socket,
    INT         Backlog
    );


INT
vxd_recv(
    LPSOCK_INFO Socket,
    LPVOID      Buffer,
    INT         BufferLength,
    INT         Flags,
    LPINT       BytesReceived,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext,
    LPDWORD     Timeout
    );



INT
vxd_recvfrom(
    LPSOCK_INFO Socket,
    LPVOID      Buffer,
    INT         BufferLength,
    INT         Flags,
    LPVOID      Address,
    LPINT       AddressLength,
    LPINT       BytesReceived,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext,
    LPDWORD     Timeout
    );

#if 0

INT
vxd_select_setup(
    UINT        ReadCount,
    LPSOCK_LIST ReadList,
    UINT        WriteCount,
    LPSOCK_LIST WriteList,
    UINT        ExceptCount,
    LPSOCK_LIST ExceptList,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext
    )
{
    WSOCK_SELECT_SETUP_PARAMS Params;

    Params.ReadCount   = (DWORD)ReadCount;
    Params.ReadList    = ReadList;
    Params.WriteCount  = (DWORD)WriteCount;
    Params.WriteList   = WriteList;
    Params.ExceptCount = (DWORD)ExceptCount;
    Params.ExceptList  = ExceptList;
    Params.ApcRoutine  = ApcRoutine;
    Params.ApcContext  = (DWORD)ApcContext;

    return CALL_WINSOCK( SELECT_SETUP );

}   // vxd_select_setup
#define vxd_select_setup(a,b,c,d,e,f,g,h)	0

VOID
vxd_select_cleanup(
    UINT        ReadCount,
    LPSOCK_LIST ReadList,
    UINT        WriteCount,
    LPSOCK_LIST WriteList,
    UINT        ExceptCount,
    LPSOCK_LIST ExceptList
    );

INT
vxd_async_select(
    LPSOCK_INFO Socket,
    HWND        Window,
    UINT        Message,
    LONG        Events
    );

INT
vxd_setsockopt(
    LPSOCK_INFO Socket,
    INT         OptionLevel,
    INT         OptionName,
    LPVOID      Value,
    INT         ValueLength
    );
#endif

INT
vxd_shutdown(
    LPSOCK_INFO Socket,
    INT         How
    );

INT
vxd_socket(
    INT               AddressFamily,
    INT               SocketType,
    INT               Protocol,
    LPSOCK_INFO FAR * NewSocket,
    SOCKET            NewSocketHandle
    );


//
//  Notification APIs.
//

#if 0
LPWSNOTIFY
vxd_create_notify(
    LPSOCK_INFO Socket,
    INT         Event,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext
    );


INT
vxd_create_multiple_notify(
    UINT        ReadCount,
    LPSOCK_LIST ReadList,
    UINT        WriteCount,
    LPSOCK_LIST WriteList,
    UINT        ExceptCount,
    LPSOCK_LIST ExceptList,
    LPVOID      ApcRoutine,
    LPVOID      ApcContext
    );


VOID
vxd_destroy_notify(
    LPWSNOTIFY Notify
    );


VOID
vxd_destroy_notify_by_socket(
    LPSOCK_INFO Socket
    );


VOID
vxd_destroy_notify_by_thread(
    VOID
    );
#endif


VOID
vxd_signal_notify(
    LPSOCK_INFO Socket,
    INT         Event,
    INT         Status
    );


VOID
vxd_signal_all_notify(
    LPSOCK_INFO Socket,
    INT         Status
    );


VOID
vxd_register_postmessage_callback(
    LPVOID PostMessageCallback
    );

DWORD
vxd_control(
    DWORD   Protocol,
    DWORD   Action,
    LPVOID  InputBuffer,
    LPDWORD InputBufferLength,
    LPVOID  OutputBuffer,
    LPDWORD OutputBufferLength
    );




#endif  // _PEGCALLS_H_
