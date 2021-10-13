//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/**********************************************************************/
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    wscntl.h

    private include file for the WsControl API in the Windows Sockets implementation.


*/


#ifndef _WSCNTL_H_
#define _WSCNTL_H_


//
//  Function prototypes.
//

DWORD
FAR PASCAL
WsControl(
    DWORD   Protocol,
    DWORD   Action,
    LPVOID  InputBuffer,
    LPDWORD InputBufferLength,
    LPVOID  OutputBuffer,
    LPDWORD OutputBufferLength
    );

typedef DWORD (FAR PASCAL * LPWSCONTROL)( DWORD   Protocol,
                                          DWORD   Action,
                                          LPVOID  InputBuffer,
                                          LPDWORD InputBufferLength,
                                          LPVOID  OutputBuffer,
                                          LPDWORD OutputBufferLength );


//
//  Ws Control action codes.
//

#define WSCNTL_TCPIP_QUERY_INFO             0x00000000
#define WSCNTL_TCPIP_SET_INFO               0x00000001
#define WSCNTL_TCPIP_ICMP_ECHO              0x00000002
#define WSCNTL_TCPIP_TEST                   0x00000003
#define WSCNTL_AFD_INFO						0x00000004
#define WSCNTL_AFD_GATHER_RAND              0x00000005


// For WSCNTL_AFD_INFO
// All selections must fit in the mask.
#define WSCNTL_AFD_INFO_MASK				0x0000ffff
#define	WSCNTL_AFD_INFO_LOCK				0x00000001
#define WSCNTL_AFD_INFO_SOCK				0x00000002
#define WSCNTL_AFD_INFO_CONN				0x00000004
#define WSCNTL_AFD_INFO_ENDP				0x00000008
#define WSCNTL_AFD_INFO_BUFFER				0x00000010
#define WSCNTL_AFD_IRDA_INFO				0x00000020
#define WSCNTL_AFD_CXPORT_INFO				0x00000040
#define WSCNTL_AFD_SET_MACHINE_NAME			0x00000080

// Option bits.
#define WSCNTL_AFD_INFO_VERBOSE				0x80000000


#endif  // _WSCNTL_H_


