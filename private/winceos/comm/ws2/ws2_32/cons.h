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
    cons.h

    This file contains global constant & macro definitions for
    WINSOCK.DLL and WSOCK32.DLL.


*/


#ifndef _CONS_H_
#define _CONS_H_


//
//  Miscellaneous constants.
//

#define MAX_UDP_DG          (65535-68)
#define NUM_SOCKETS         200

#define FIRST_SOCKET_HANDLE 1

#define DOS_MAX_PATH_LENGTH MAX_PATH

#ifndef ARGUMENT_PRESENT
#define ARGUMENT_PRESENT(ptr)   (ptr != NULL)
#endif 

#ifndef ERROR_SERVICE_NOT_FOUND
#define ERROR_SERVICE_NOT_FOUND ERROR_SERVICE_NOT_ACTIVE
#endif  // ERROR_SERVICE_NOT_FOUND


//
//  Portability macros.
//

#if defined(WIN32)

#define WIN32_ONLY(x)               x
#define WIN16_ONLY(x)
#define DOS_ONLY(x)

#define SOCKAPI						__cdecl

#define FSTRCPY(d,s)                strcpy( (d), (s) )
#define FSTRCAT(d,s)                strcat( (d), (s) )
#define FSTRICMP(x,y)               _stricmp( (x), (y) )
#define FSTRLWR(s)                  _strlwr( (s) )

#define INVALID_TLS_INDEX           (DWORD)-1L

#elif defined(WIN16)

#define WIN32_ONLY(x)
#define WIN16_ONLY(x)               x
#define DOS_ONLY(x)

#define SOCKAPI                     __export FAR PASCAL

#define FSTRCPY(d,s)                _fstrcpy( (d), (s) )
#define FSTRCAT(d,s)                _fstrcat( (d), (s) )
#define FSTRICMP(x,y)               _fstricmp( (x), (y) )
#define FSTRLWR(s)                  _fstrlwr( (s) )

#else

#define WIN32_ONLY(x)
#define WIN16_ONLY(x)
#define DOS_ONLY(x)                 x

#define SOCKAPI                     FAR PASCAL

#define FSTRCPY(d,s)                _fstrcpy( (d), (s) )
#define FSTRCAT(d,s)                _fstrcat( (d), (s) )
#define FSTRICMP(x,y)               _fstricmp( (x), (y) )
#define FSTRLWR(s)                  _fstrlwr( (s) )

#endif


//
//  Map a socket handle to a pointer to a socket descriptor.
//

#define MAP_HANDLE_TO_SOCK_INFO(s) \
            (((s) < NUM_SOCKETS) ? socket_handles[s] : NULL)


// For now just have a null for the APC function
// This is used for Select()
#define TARGET_APC	NULL


#endif  // _CONS_H_

