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

    scnb.hxx

Abstract:

    Small client netbios interface


--*/

#if ! defined (__scnb_HXX__)
#define __scnb_HXX__	1

int NBStatusQuery (char *szNameBuffer, int cBufferSize, unsigned long ip);
int NBGetWins (unsigned long *ipWinsBuffer, int nBufferSize);

unsigned long NBQueryWins (char *szHostName, unsigned long ipWins);

#endif