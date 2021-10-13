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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

select.c

pass select from the CE WSP to the PM (afd) layer


FILE HISTORY:
	OmarM     16-Oct-2000
		copied most functionality from PM's select

*/


#include "wspmp.h"



#if defined(WIN32) && !defined(UNDER_CE)
DWORD APIENTRY GetHandleContext(HANDLE s);  // in kernel32.dll
#endif

BOOL
SelectpBuildSocketList(
    UINT FAR        * SocketCount,
    LPSOCK_LIST		* SocketList,
    FD_SET FAR      * fdset,
    DWORD             Events
    );

int
SelectpScanSockets(
    UINT         SocketCount,
    LPSOCK_LIST  SocketList,
    FD_SET FAR * fdset
    );

//
//  Public functions.
//

int TestTimeout(const struct timeval * timeout) {
	long i;

	__try {
		i = timeout->tv_sec + timeout->tv_usec;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return WSAEFAULT;
	}
	return 0;
}


int WSPAPI WSPSelect (
	int nfds,
	fd_set FAR * readfds,
	fd_set FAR * writefds,
	fd_set FAR * exceptfds,
	const struct timeval FAR * timeout,
	LPINT lpErrno) {

    LPSOCK_LIST     ReadList   = NULL;
    UINT            ReadCount;
    LPSOCK_LIST     WriteList  = NULL;
    UINT            WriteCount;
    LPSOCK_LIST     ExceptList = NULL;
    UINT            ExceptCount;
    UINT            SocketCount;
   	int				LastError;
	int				Result;
	
	if (timeout) {
		if (LastError = TestTimeout(timeout))
			goto Cleanup;
	}

    //  Build the socket lists.

    if( (LastError = SelectpBuildSocketList( &ReadCount,   &ReadList,   readfds,   READ_EVENTS   )) ||
        (LastError = SelectpBuildSocketList( &WriteCount,  &WriteList,  writefds,  WRITE_EVENTS  )) ||
        (LastError = SelectpBuildSocketList( &ExceptCount, &ExceptList, exceptfds, EXCEPT_EVENTS )) )
    {
//		LEAVE_DLL_CS();
        goto Cleanup;
    }

    SocketCount = ReadCount + WriteCount + ExceptCount;

    if( SocketCount == 0 )
    {
        //
        //  Nothing to do.
        //

        ASSERT( ReadList   == NULL );
        ASSERT( WriteList  == NULL );
        ASSERT( ExceptList == NULL );
//		LEAVE_DLL_CS();

        return 0;
    }


    LastError = AFDSelect(
        ReadCount, ReadList,
        WriteCount, WriteList,
        ExceptCount, ExceptList,
        timeout
        );


    if (LastError)
    {
        goto Cleanup;
    }

    __try {
        Result  = SelectpScanSockets( ReadCount,   ReadList,   readfds   );
        Result += SelectpScanSockets( WriteCount,  WriteList,  writefds  );
        Result += SelectpScanSockets( ExceptCount, ExceptList, exceptfds );
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LastError = WSAEFAULT;
        // fall thru to Cleanup;
    }

Cleanup:

    if( ReadList != NULL )
        LocalFree(ReadList);

    if( WriteList != NULL )
        LocalFree( WriteList );

    if( ExceptList != NULL )
        LocalFree( ExceptList );

	if (LastError) {
		*lpErrno = LastError;
		Result = SOCKET_ERROR;
	}

    return Result;
    
}	// WSPSelect



/*******************************************************************

    NAME:       __WSAFDIsSet

    SYNOPSIS:   This function is used by the FD_ISSET macro;
                applications should not call it directly.  This
                function determines whether a socket handle is
                included in the specified set.

    ENTRY:      fd - The socket handle to scan for.

                set - The set to scan.

    RETURNS:    int - 1 if the socket was found, 0 if not.

********************************************************************/
int
__WSAFDIsSet(
    SOCKET       fd,
    FD_SET FAR * set
    )
{
    int i = (set->fd_count & 0xFFFF);

    while (i--)
        if (set->fd_array[i] == fd)
            return 1;

    return 0;

}   // __WSAFDIsSet


//
//  Private functions.
//


/*******************************************************************

    NAME:       SelectpBuildSocketList

    SYNOPSIS:   Allocates and initializes an array of SOCK_LIST
                structures to pass to WsCreateMultipleNotify.

    ENTRY:      SocketCount - Will receive the number of sockets
                    in the list.

                SocketList - Will receive the socket list.

                fds - The sockets to check for specific events.

                Events - The events to check for.

    RETURNS:    int - 0 if successful, WSA...error otherwise. - OM modified
					used to return BOOL

********************************************************************/
int
SelectpBuildSocketList(
    UINT FAR        * SocketCount,
    LPSOCK_LIST FAR * SocketList,
    FD_SET FAR      * fdset,
    DWORD             Events
    )
{
    LPSOCK_LIST   List;
    UINT          Count;
    SOCKET FAR  * Array;
	int		      Error = 0;

    //
    //  Count the sockets in the set.
    //
#define DllAllocMem(x)	LocalAlloc(0, x)

	__try {
		Count = ( fdset == NULL ) ? 0 : ( fdset->fd_count & 0xFFFF );

		if( Count == 0 )
		{
			*SocketCount = 0;
			*SocketList  = NULL;
		}
		else 
		{

			//
			//  Allocate the structure array.
			//

			List = (LPSOCK_LIST)DllAllocMem( Count * sizeof(SOCK_LIST) );

			if (!List) {
				Error = WSAENOBUFS;
			} else {

				memset(List, 0, (Count * sizeof(SOCK_LIST)));

				//  Build the list.
				*SocketCount = Count;
				*SocketList  = List;
				Array        = fdset->fd_array;

				while ( Count-- ) {
					List->Context   = (DWORD)*Array;
#if defined(WIN32) && !defined(UNDER_CE)
					List->Socket    = (LPSOCK_INFO)GetHandleContext( (HANDLE)*Array );
#else
					//        List->Socket    = MAP_HANDLE_TO_SOCK_INFO( *Array );
					// note in CE we used ->hSocket, AFD later converts that handle to
					//   an LPSOCK_INFO and puts it in ->Socket
					List->hSocket = *Array;
//			if ( Error = GetSockInfo(*Array, (LPSOCK_INFO *)&List->hSocket) )
//				break;
#endif
					List->EventMask = Events;

					List++;
					Array++;
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		Error = WSAEFAULT;
	}

    return Error;

}   // SelectpBuildSocketList

/*******************************************************************

    NAME:       SelectpScanSockets

    SYNOPSIS:   Scans the given socket set, looking for sockets
                with the specified events ready.  Those sockets
                that are not ready are removed from the set.

    ENTRY:      SocketCount - The number of sockets in the list.

                SocketList - The socket list to scan.

                fdset - The socket set to update.

    RETURNS:    int - The number of sockets remaining in the set.

********************************************************************/
int
SelectpScanSockets(
    UINT         SocketCount,
    LPSOCK_LIST  SocketList,
    FD_SET FAR * fdset
    )
{
    SOCKET FAR * Dest;

    if( ( fdset == NULL ) || ( SocketCount == 0 ) )
    {
        //
        //  NULL set is OK.
        //

        return 0;
    }

    ASSERT( SocketList != NULL );

    //
    //  Update the FD_SET.  Note that vxd_select_update has NULLed
    //  out all sockets that are not ready to fire.  We'll just nuke
    //  these from the FD_SET.
    //

    Dest = fdset->fd_array;

    while( SocketCount-- )
    {
        if( SocketList->Socket != NULL )
        {
            *Dest++ = (SOCKET)SocketList->Context;
        }

        SocketList++;
    }

    fdset->fd_count = Dest - fdset->fd_array;

    return (int)fdset->fd_count;

}   // SelectpScanSockets


int WSPAPI WSPEnumNetworkEvents (
	SOCKET s,
	WSAEVENT hEventObject,
	LPWSANETWORKEVENTS lpNetworkEvents,
	LPINT lpErrno) {

	int	Err;

	Err = AFDEnumNetworkEvents((SOCKHAND)s, hEventObject, lpNetworkEvents, 
		NULL);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;
	
}	// WSPEnumNetworkEvents()


int WSPAPI WSPEventSelect (
	SOCKET s,
	WSAEVENT hEventObject,
	long lNetworkEvents,
	LPINT lpErrno) {

	int	Err;

	Err = AFDEventSelect((SOCKHAND)s, hEventObject, lNetworkEvents, NULL);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;
	
}	// WSPEventSelect()


