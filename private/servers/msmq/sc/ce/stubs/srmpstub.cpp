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

    srmpstub.cpp

Abstract:

    Stubs for SRMP


--*/


#include <windows.h>
#include <mq.h>
#include <scsrmp.hxx>
#include <svsutil.hxx>
#include <scpacket.hxx>
#include <scqueue.hxx>
#include <scsman.hxx>

const BOOL g_fHaveSRMP = FALSE;

void SrmpAcceptHttpRequest(PSrmpIOCTLPacket pSrmpPacket, DWORD *pdwHttpStatus) {
	SVSUTIL_ASSERT(FALSE);
	*pdwHttpStatus = 500; // HTTP Server Error.
}


DWORD WINAPI ScSession::ServiceThreadHttpW_s(void *arg) {
	SVSUTIL_ASSERT(FALSE);
	return FALSE;
}

void SrmpInit(void) {
	SVSUTIL_ASSERT(FALSE);
}

void ScSession::SRMPCloseSession(void) {
	SVSUTIL_ASSERT(FALSE);
}

BOOL UriToQueueFormat(const WCHAR *szQueue, DWORD dwQueueChars, QUEUE_FORMAT *pQueue, WCHAR **ppszQueueBuffer) {
	SVSUTIL_ASSERT(FALSE);
	return FALSE;
}
