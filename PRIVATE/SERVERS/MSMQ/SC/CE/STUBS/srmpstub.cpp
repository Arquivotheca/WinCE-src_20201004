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

    scmain.cxx

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
