//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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