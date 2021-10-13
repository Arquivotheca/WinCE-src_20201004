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

    scapi.h

Abstract:

    Small client - Windows CE nonport part


--*/
#include <windows.h>
#include <mq.h>

#include <svsutil.hxx>
#include <service.h>

#include <scapi.h>

#if defined (SDK_BUILD)
#include <pstub.h>
#endif

static HANDLE hGoAway;

void MSMQNotifyCallback (DWORD cause, HPROCESS hprc, HTHREAD hthd);

PFNVOID Methods[] = {
//xref ApiSetStart
    (PFNVOID)MSMQNotifyCallback,				/* 0  */
    NULL,									    /* 1  */
	(PFNVOID)scapi_MQCreateQueue,				/* 2  */
	(PFNVOID)scapi_MQDeleteQueue,				/* 3  */
	(PFNVOID)scapi_MQGetQueueProperties,		/* 4  */
	(PFNVOID)scapi_MQSetQueueProperties,		/* 5  */
	(PFNVOID)scapi_MQOpenQueue,					/* 6  */
	(PFNVOID)scapi_MQCloseQueue,				/* 7  */
	(PFNVOID)scapi_MQCreateCursor,				/* 8  */
	(PFNVOID)scapi_MQCloseCursor,				/* 9  */
	(PFNVOID)scapi_MQHandleToFormatName,		/* 10 */
	(PFNVOID)scapi_MQPathNameToFormatName,		/* 11 */
	(PFNVOID)scapi_MQSendMessage,				/* 12 */
	(PFNVOID)scapi_MQReceiveMessage,			/* 13 */
    (PFNVOID)scapi_MQGetMachineProperties,      /* 14 */
	(PFNVOID)scapi_MQBeginTransaction,          /* 15 */
	(PFNVOID)scapi_MQGetQueueSecurity,		    /* 16 */
	(PFNVOID)scapi_MQSetQueueSecurity,		    /* 17 */
	(PFNVOID)scapi_MQGetSecurityContext,	    /* 18 */
	(PFNVOID)scapi_MQFreeSecurityContext,	    /* 19 */
	(PFNVOID)scapi_MQInstanceToFormatName,	    /* 20 */
	(PFNVOID)scapi_MQLocateBegin,			    /* 21 */
	(PFNVOID)scapi_MQLocateNext,			    /* 22 */
	(PFNVOID)scapi_MQLocateEnd,				    /* 23 */
	(PFNVOID)scmgmt_MQMgmtGetInfo2,				/* 24 */
	(PFNVOID)scmgmt_MQMgmtAction,				/* 25 */
//xref ApiSetEnd
};

const DWORD SigTbl[] = {
    FNSIG3(DW,DW,DW),                                   /* 0  */
    FNSIG0(),                                           /* 1  */
    FNSIG4(DW, PTR, PTR, PTR),                          /* 2  */
    FNSIG1(PTR),               							/* 3  */
    FNSIG2(PTR, PTR),               					/* 4  */
    FNSIG2(PTR, PTR),               					/* 5  */
    FNSIG4(PTR, DW, DW, PTR),               			/* 6  */
    FNSIG1(DW),               							/* 7  */
    FNSIG2(DW, PTR),               						/* 8  */
    FNSIG1(DW),               							/* 9  */
    FNSIG3(DW, PTR, PTR),               				/* 10 */
    FNSIG3(PTR, PTR, PTR),               				/* 11 */
    FNSIG3(DW, PTR, DW),               					/* 12 */
    FNSIG8(DW, DW, DW, DW, DW, DW, DW, DW),             /* 13 */
    FNSIG3(PTR, PTR, PTR),               				/* 14 */
    FNSIG1(PTR),               							/* 15 */
	FNSIG5(PTR, DW, PTR, DW, PTR),						/* 16 */
	FNSIG3(PTR, DW, PTR),								/* 17 */
	FNSIG3(PTR, DW, PTR),								/* 18 */
	FNSIG1(DW), 										/* 19 */
	FNSIG3(PTR, PTR, PTR),								/* 20 */
	FNSIG1(PTR),										/* 21 */
    FNSIG3(DW, PTR, PTR),               				/* 22 */
	FNSIG1(DW), 										/* 23 */
	FNSIG5(PTR, PTR, DW, PTR, PTR), 					/* 24 */
	FNSIG3(PTR, PTR, PTR), 								/* 25 */
};

void MSMQNotifyCallback (DWORD cause, HPROCESS hprc, HTHREAD hthd) {
    if (cause == DLL_PROCESS_DETACH) {
		scapi_ProcExit (hprc);
	}
}

LONG fPSLRegistered = FALSE;

void scce_Listen (void) {
	if (InterlockedExchange (&fPSLRegistered, TRUE))
		return;

	//
	//	Initialize API set here...
	//
    int cFuncs = SVSUTIL_ARRLEN(Methods);
	SVSUTIL_ASSERT(cFuncs == SVSUTIL_ARRLEN(SigTbl));

	HANDLE hApis = CreateAPISet ("MSMQ", cFuncs, Methods, SigTbl);

	if (! hApis)
	    return;

	if (! RegisterAPISet (hApis, 0)) {
	    CloseHandle (hApis);
	    return;
	}

	//
	//	Just linger...
	//
	hGoAway = CreateEvent (NULL, FALSE, FALSE, NULL);
    WaitForSingleObject (hGoAway, INFINITE);

	//
	//	Deregister...
	//
	CloseHandle (hApis);

	fPSLRegistered = FALSE;
}

HRESULT scce_Shutdown (void) {
	SetEvent (hGoAway);
	return MQ_OK;
}

HANDLE g_hApis = NULL;

int scce_RegisterDLL (void) {
	if (InterlockedExchange (&fPSLRegistered, TRUE))
		return FALSE;

	//
	//	Initialize API set here...
	//
    int cFuncs = SVSUTIL_ARRLEN(Methods);
	SVSUTIL_ASSERT(cFuncs == SVSUTIL_ARRLEN(SigTbl));

	g_hApis = CreateAPISet ("MSMQ", cFuncs, Methods, SigTbl);

	if (! g_hApis)
	    return FALSE;

	if (! RegisterAPISet (g_hApis, 0)) {
	    CloseHandle (g_hApis);
	    return FALSE;
	}

	return TRUE;
}

int scce_UnregisterDLL (void) {
	if (! fPSLRegistered)
		return FALSE;

	//
	//	Deregister...
	//
	CloseHandle (g_hApis);

	fPSLRegistered = FALSE;

	return TRUE;
}

//
//	For executable only - no netbios registration...
//
unsigned long  glServiceState  = SERVICE_STATE_OFF;

int scce_RegisterNET (void *ptr) {
	return TRUE;
}

void scce_UnregisterNET (void) {
}
