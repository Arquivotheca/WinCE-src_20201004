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


Module Name:

    _ta.h

Abstract:

    Definition of the address type

--*/

#ifndef __TA_H
#define __TA_H
//
// AddressType values
//
#define IP_ADDRESS_TYPE         1
#define IP_RAS_ADDRESS_TYPE     2

#define IPX_ADDRESS_TYPE        3
#define IPX_RAS_ADDRESS_TYPE    4

#define FOREIGN_ADDRESS_TYPE    5


#define IP_ADDRESS_LEN           4
#define IPX_ADDRESS_LEN         10
#define FOREIGN_ADDRESS_LEN     16

#define TA_ADDRESS_SIZE         4  // To be changed if following struct is changing
typedef struct  _TA_ADDRESS
{
    USHORT AddressLength;
    USHORT AddressType;
    UCHAR Address[ 1 ];
} TA_ADDRESS;

#endif // _TA_H

