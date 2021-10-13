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
#include "stdafx.h"

/*************************************************************************************************/
void HostToWire16(IN WORD  wHostFormat, IN OUT PBYTE pWireFormat)
{
    *((PBYTE)(pWireFormat)+0) = (BYTE) ((DWORD)(wHostFormat) >>  8);
    *((PBYTE)(pWireFormat)+1) = (BYTE) (wHostFormat);
}

/*************************************************************************************************/
WORD WireToHost16(IN PBYTE pWireFormat)
{
    WORD wHostFormat = ((*((PBYTE)(pWireFormat)+0) << 8) + (*((PBYTE)(pWireFormat)+1)));
    return( wHostFormat );
}

/*************************************************************************************************/
void HostToWire32(IN DWORD dwHostFormat, IN OUT PBYTE pWireFormat)
{
    *((PBYTE)(pWireFormat)+0) = (BYTE) ((DWORD)(dwHostFormat) >> 24);
    *((PBYTE)(pWireFormat)+1) = (BYTE) ((DWORD)(dwHostFormat) >> 16);
    *((PBYTE)(pWireFormat)+2) = (BYTE) ((DWORD)(dwHostFormat) >>  8);
    *((PBYTE)(pWireFormat)+3) = (BYTE) (dwHostFormat);
}

/*************************************************************************************************/
DWORD WireToHost32(IN PBYTE pWireFormat)
{
    DWORD dwHostFormat = ((*((PBYTE)(pWireFormat)+0) << 24)	+
    			  	 (*((PBYTE)(pWireFormat)+1) << 16) 	+
        		  	 (*((PBYTE)(pWireFormat)+2) << 8)  	+
                    	 (*((PBYTE)(pWireFormat)+3) ));
    return( dwHostFormat );
}

/*************************************************************************************************/
void HostToWire64(IN ULONGLONG ullHostFormat, IN OUT PBYTE pWireFormat)
{
    *((PBYTE)(pWireFormat)+0) = (BYTE) (ullHostFormat >> 56);
    *((PBYTE)(pWireFormat)+1) = (BYTE) (ullHostFormat >> 46);
    *((PBYTE)(pWireFormat)+2) = (BYTE) (ullHostFormat >> 40);
    *((PBYTE)(pWireFormat)+3) = (BYTE) (ullHostFormat >> 32);
    *((PBYTE)(pWireFormat)+4) = (BYTE) (ullHostFormat >> 24);
    *((PBYTE)(pWireFormat)+5) = (BYTE) (ullHostFormat >> 16);
    *((PBYTE)(pWireFormat)+6) = (BYTE) (ullHostFormat >>  8);
    *((PBYTE)(pWireFormat)+7) = (BYTE) (ullHostFormat      );
}

/*************************************************************************************************/
ULONGLONG WireToHost64(IN PBYTE pWireFormat)
{
    ULONGLONG   ullHostFormat = (
								 (((ULONGLONG)(*((PBYTE)(pWireFormat)+0))) << 56) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+1))) << 48) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+2))) << 40) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+3))) << 32) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+4))) << 24) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+5))) << 16) +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+6))) << 8)  +
                                 (((ULONGLONG)(*((PBYTE)(pWireFormat)+7))))
								);
    return (ullHostFormat);
}