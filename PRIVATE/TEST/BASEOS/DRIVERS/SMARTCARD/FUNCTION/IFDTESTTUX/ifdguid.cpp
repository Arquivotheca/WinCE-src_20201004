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
///////////////////////////////////////////////////////////
/*++

    Module Name: ifdguid.c

    Abstract:

        This module maps UIDs to friendly IDs

    Author(s): Eric St.John (ericstj) 4.1.2006

    Environment: User Mode

    Revision History:


--*/
///////////////////////////////////////////////////////////

#include "ifdguid.h"

///////////////////////////////////////////////////////////
// FUNCTION LIST
///////////////////////////////////////////////////////////

//  GetTestCaseUID

///////////////////////////////////////////////////////////
// GLOBALS
///////////////////////////////////////////////////////////
UCHAR g_uiFriendlyPhase = 'A';
UINT g_uiFriendlyTest1 = 0;
UINT g_uiFriendlyTest2 = 0;
UINT g_uiFriendlyTest3 = 0;

///////////////////////////////////////////////////////////
// PRIVATE DATA TYPES
///////////////////////////////////////////////////////////

typedef struct _ASSERTION_ID {
    UCHAR FriendlyPhase;
    UINT FriendlyTest1;
    UINT FriendlyTest2;
    UINT FriendlyTest3;
    LPWSTR UID;
} ASSERTION_ID;

///////////////////////////////////////////////////////////
// PRIVATE DATA CONSTANTS
///////////////////////////////////////////////////////////
const LPWSTR g_szDefaultAssertion = L"";
static ASSERTION_ID g_apAssertionTable[] = {
    {'A', 1, 0, 0, L"d58966e8-cb2e-11da-94f8-00123f3a6b60"},
    {'A', 2, 0, 0, L"d58966e9-cb2e-11da-94f8-00123f3a6b60"},
    {'A', 3, 0, 0, L"d58966ea-cb2e-11da-94f8-00123f3a6b60"},
    {'A', 4, 0, 0, L"d58966eb-cb2e-11da-94f8-00123f3a6b60"},
    {'A', 5, 0, 0, L"d58966ec-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 1, 0, 0, L"d58966ed-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 2, 0, 0, L"d58966ee-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 3, 0, 0, L"d58966ef-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 4, 0, 0, L"d58966f0-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 5, 0, 0, L"d58966f1-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 6, 0, 0, L"d58966f2-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 7, 0, 0, L"d58966f3-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 8, 0, 0, L"d58966f4-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 9, 0, 0, L"d58966f5-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 10, 0, 0, L"d58966f6-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 11, 0, 0, L"d58966f7-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 12, 0, 0, L"d58966f8-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 13, 0, 0, L"d58966f9-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 14, 0, 0, L"d58966fa-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 15, 0, 0, L"d58966fb-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 16, 0, 0, L"d58966fc-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 17, 0, 0, L"d58966fd-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 18, 0, 0, L"d58966fe-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 19, 0, 0, L"d58966ff-cb2e-11da-94f8-00123f3a6b60"},
    {'B', 20, 0, 0, L"d5896700-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 1, 0, 0, L"d5896701-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 2, 0, 0, L"d5896702-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 3, 0, 0, L"d5896703-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 4, 0, 0, L"d5896704-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 5, 0, 0, L"d5896705-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 6, 0, 0, L"d5896706-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 7, 0, 0, L"d5896707-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 8, 0, 0, L"d5896708-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 9, 0, 0, L"d5896709-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 10, 0, 0, L"d589670a-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 11, 0, 0, L"d589670b-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 12, 0, 0, L"d589670c-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 13, 0, 0, L"d589670d-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 14, 0, 0, L"d589670e-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 15, 0, 0, L"d589670f-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 16, 0, 0, L"d5896710-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 17, 0, 0, L"d5896711-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 18, 0, 0, L"d5896712-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 19, 0, 0, L"d5896713-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 20, 0, 0, L"d5896714-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 21, 0, 0, L"d5896715-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 22, 0, 0, L"d5896716-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 23, 0, 0, L"d5896717-cb2e-11da-94f8-00123f3a6b60"},
    {'C', 24, 0, 0, L"d5896718-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 0, 1, L"d5896719-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 1, L"d589671a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 2, L"d589671b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 3, L"d589671c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 4, L"d589671d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 5, L"d589671e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 6, L"d589671f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 7, L"d5896720-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 8, L"d5896721-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 9, L"d5896722-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 10, L"d5896723-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 11, L"d5896724-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 12, L"d5896725-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 13, L"d5896726-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 14, L"d5896727-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 15, L"d5896728-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 16, L"d5896729-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 1, 1, 17, L"d589672a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 0, 1, L"d589672b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 1, L"d589672c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 2, L"d589672d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 3, L"d589672e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 4, L"d589672f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 5, L"d5896730-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 6, L"d5896731-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 7, L"d5896732-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 8, L"d5896733-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 9, L"d5896734-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 10, L"d5896735-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 11, L"d5896736-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 12, L"d5896737-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 13, L"d5896738-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 14, L"d5896739-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 15, L"d589673a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 16, L"d589673b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 2, 1, 17, L"d589673c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 0, 1, L"d589673d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 0, 2, L"d589673e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 1, 1, L"d589673f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 2, 1, L"d5896740-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 2, 2, L"d5896741-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 2, 3, L"d5896742-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 2, 4, L"d5896743-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 3, 1, L"d5896744-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 4, 1, L"d5896745-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 1, L"d5896746-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 2, L"d5896747-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 3, L"d5896748-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 4, L"d5896749-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 5, L"d589674a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 6, L"d589674b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 7, L"d589674c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 8, L"d589674d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 9, L"d589674e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 10, L"d589674f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 5, 11, L"d5896750-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 6, 1, L"d5896751-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 3, 6, 2, L"d5896752-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 0, 1, L"d5896753-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 0, 2, L"d5896754-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 1, 1, L"d5896755-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 1, L"d5896756-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 2, L"d5896757-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 3, L"d5896758-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 4, L"d5896759-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 5, L"d589675a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 6, L"d589675b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 7, L"d589675c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 8, L"d589675d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 9, L"d589675e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 10, L"d589675f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 11, L"d5896760-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 12, L"d5896761-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 13, L"d5896762-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 14, L"d5896763-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 2, 15, L"d5896764-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 1, L"d5896765-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 2, L"d5896766-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 3, L"d5896767-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 4, L"d5896768-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 5, L"d5896769-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 6, L"d589676a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 7, L"d589676b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 8, L"d589676c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 3, 9, L"d589676d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 1, L"d589676e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 2, L"d589676f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 3, L"d5896770-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 4, L"d5896771-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 5, L"d5896772-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 6, L"d5896773-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 7, L"d5896774-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 8, L"d5896775-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 4, 9, L"d5896776-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 5, 1, L"d5896777-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 6, 1, L"d5896778-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 7, 1, L"d5896779-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 8, 1, L"d589677a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 9, 1, L"d589677b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 1, L"d589677c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 2, L"d589677d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 3, L"d589677e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 4, L"d589677f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 5, L"d5896780-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 6, L"d5896781-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 7, L"d5896782-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 8, L"d5896783-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 9, L"d5896784-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 10, L"d5896785-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 11, L"d5896786-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 12, L"d5896787-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 13, L"d5896788-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 14, L"d5896789-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 15, L"d589678a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 16, L"d589678b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 17, L"d589678c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 4, 10, 18, L"d589678d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 0, 1, L"d589678e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 0, 2, L"d589678f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 1, 1, L"d5896790-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 2, 1, L"d5896791-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 2, 2, L"d5896792-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 2, 3, L"d5896793-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 2, 4, L"d5896794-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 2, 5, L"d5896795-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 3, 1, L"d5896796-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 3, 2, L"d5896797-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 3, 3, L"d5896798-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 4, 1, L"d5896799-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 4, 2, L"d589679a-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 5, 1, L"d589679b-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 5, 2, L"d589679c-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 1, L"d589679d-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 2, L"d589679e-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 1, L"d589679f-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 2, L"d58967a0-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 3, L"d58967a1-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 4, L"d58967a2-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 5, L"d58967a3-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 6, L"d58967a4-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 7, L"d58967a5-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 8, L"d58967a6-cb2e-11da-94f8-00123f3a6b60"},
    {'D', 5, 6, 9, L"d58967a7-cb2e-11da-94f8-00123f3a6b60"},
    {'E', 1, 0, 0, L"d58967a8-cb2e-11da-94f8-00123f3a6b60"},
    {'E', 2, 0, 0, L"d58967a9-cb2e-11da-94f8-00123f3a6b60"},
    {'E', 3, 0, 0, L"d58967aa-cb2e-11da-94f8-00123f3a6b60"},
    {'E', 4, 0, 0, L"d58967ab-cb2e-11da-94f8-00123f3a6b60"}
    };

///////////////////////////////////////////////////////////
// UIDM_GetTestCaseUID <PUBLIC FUNCTION>
///////////////////////////////////////////////////////////

const LPWSTR GetTestCaseUID() {
    UINT uCount;
    for(uCount = 0; uCount < sizeof(g_apAssertionTable) / sizeof(g_apAssertionTable[0]); uCount++) {
        if ((g_apAssertionTable[uCount].FriendlyPhase == g_uiFriendlyPhase) &&
            (g_apAssertionTable[uCount].FriendlyTest1 == g_uiFriendlyTest1) &&
            ((g_apAssertionTable[uCount].FriendlyTest2 == g_uiFriendlyTest2) || (g_apAssertionTable[uCount].FriendlyTest2 == 0)) &&
            ((g_apAssertionTable[uCount].FriendlyTest3 == g_uiFriendlyTest3) || (g_apAssertionTable[uCount].FriendlyTest3 == 0)))
        {
            return g_apAssertionTable[uCount].UID;
        }
    }
    return g_szDefaultAssertion;
}
