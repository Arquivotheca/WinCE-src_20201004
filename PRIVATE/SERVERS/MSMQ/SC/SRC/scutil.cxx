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

    scutil.cxx

Abstract:

    Small client utilities


--*/
#include <sc.hxx>

#include <mq.h>
#include <scapi.h>

#include <scfile.hxx>
#include <scutil.hxx>

#include <scpacket.hxx>
#include <scqueue.hxx>
#include <scqman.hxx>
#include <srmpDefs.hxx>

#include <mq.h>
#include <mqformat.h>

#if defined (UNDER_CE)
#if defined (winCE) && defined (SDK_BUILD)
#include <pstub.h>
#else
#include <pkfuncs.h>
#endif
#else
unsigned long glServiceState;	// Stub the CE-only var.
#endif

//
//	Attention! Do NOT call this function from anything other than API thread!!!
//	It will NOT work from any native MSMQ thread context!!!
//
void *scutil_OutOfProcAlloc (void **ppvOutOfProcPtr, int iSize) {
#if defined (winCE)
	void *pRet = (WCHAR *)RemoteLocalAlloc (LMEM_FIXED, iSize);

	*ppvOutOfProcPtr = (void *)(CeZeroPointer (pRet));

	return pRet;
#else
	return *ppvOutOfProcPtr = g_funcAlloc (iSize, g_pvAllocData);
#endif
}

WCHAR *scutil_OutOfProcDup (WCHAR *lpsz) {
#if defined (winCE)
	int sz = (wcslen(lpsz) + 1) * sizeof(WCHAR);

	//	CALLBACKINFO cbi;
	//	cbi.hProc  = (HANDLE)GetCallerProcess();
	//	cbi.pfn    = (FARPROC)LocalAlloc;
	//	cbi.pvArg0 = LMEM_FIXED;
	//
	//	WCHAR *pszNew = (WCHAR *)PerformCallBack (&cbi, sz);
	WCHAR *pszNew = (WCHAR *)RemoteLocalAlloc (LMEM_FIXED, sz);

	if (! pszNew)
		return NULL;

	memcpy (pszNew, lpsz, sz);

	return (WCHAR *)(CeZeroPointer (pszNew));
#else
	return svsutil_wcsdup (lpsz);
#endif
}

void scutil_uuidgen (GUID *pguid) {
	if (gMachine->CeGenerateGUID && (gMachine->CeGenerateGUID (pguid) == 0))
		return;

	static int iserial = 0;

	SYSTEMTIME st;
	GetLocalTime (&st);

	SystemTimeToFileTime (&st, (FILETIME *)pguid->Data4);
	pguid->Data1 = ++iserial;

	if (! iserial)
		srand (*(unsigned int *)pguid->Data4);

	pguid->Data2 = rand ();
	pguid->Data3 = rand ();
}

unsigned int scutil_now (void) {
	SYSTEMTIME st;
	GetSystemTime (&st);

	LARGE_INTEGER ft;
	SystemTimeToFileTime (&st, (FILETIME *)&ft);

    ft.QuadPart -= 0x019db1ded53e8000;
    ft.QuadPart /= 10000000;

    return ft.LowPart;
}

static WCHAR *scutil_MachineJournalQueue (void) {
	WCHAR szMJQ[_MAX_PATH];
	wsprintf (szMJQ, L"MACHINE=" SC_GUID_FORMAT L";JOURNAL", SC_GUID_ELEMENTS((&gMachine->guid)));
	return svsutil_wcsdup (szMJQ);
}

static WCHAR *scutil_DeadLetterQueue (void) {
	WCHAR szDLQ[_MAX_PATH];
	wsprintf (szDLQ, L"MACHINE=" SC_GUID_FORMAT L";DEADLETTER", SC_GUID_ELEMENTS((&gMachine->guid)));
	return svsutil_wcsdup (szDLQ);
}

WCHAR *scutil_GUIDtoString (GUID *pguid) {
	WCHAR szGUID[_MAX_PATH];
	wsprintf (szGUID, SC_GUID_FORMAT, SC_GUID_ELEMENTS(pguid));
	return svsutil_wcsdup (szGUID);
}

/*
int scutil_IsDeadLetterFormatName (WCHAR *a_lpszFormatName) {
	if (wcsnicmp (a_lpszFormatName, MSMQ_SC_FORMAT_MACHINE, SVSUTIL_CONSTSTRLEN (MSMQ_SC_FORMAT_MACHINE)) != 0)
		return FALSE;

	WCHAR *p = wcsrchr (a_lpszFormatName, L';');
	if ((! p) || (wcsicmp (p, MSMQ_SC_FORMAT_DEADLT) != 0))
		return FALSE;

	WCHAR *lpTrueDLQ = scutil_DeadLetterQueue ();
	int iRes = wcsicmp (a_lpszFormatName, lpTrueDLQ) == 0;
	g_funcFree  (lpTrueDLQ, g_pvFreeData);
	return iRes;
}

*/

//
//	Pathname is of a form machine[\\private$]\\filename
//
//	Only private and direct formats are supported, so insist on them.
//
WCHAR	*scutil_MakeFormatName (WCHAR *a_lpszPathName, ScQueueParms *a_pqp) {
	SVSUTIL_ASSERT (a_lpszPathName && a_pqp);
    SVSUTIL_ASSERT (a_pqp->bIsIncoming);
	SVSUTIL_ASSERT (a_pqp->bFormatType == SCFILE_QP_FORMAT_OS);

	if (a_pqp->bIsDeadLetter)
		return scutil_DeadLetterQueue ();

	if (a_pqp->bIsMachineJournal)
		return scutil_MachineJournalQueue ();

	WCHAR *lpHost = a_lpszPathName;
	WCHAR *lpPath = wcschr (a_lpszPathName, L'\\');

	if (! lpPath)
		return NULL;

	int ccHostName = lpPath - a_lpszPathName;

	if ((ccHostName == 1) && (*lpHost == L'.')) {
		lpHost = gMachine->lpszHostName;
		ccHostName = wcslen (lpHost);
	}

	int ccQueueName = wcslen (lpPath) + 1;
	int ccTag = 0;

	if (a_pqp->bIsJournal)
		ccTag = SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_JOURNAL);

	WCHAR *lpszFormatName = (WCHAR *)g_funcAlloc ((ccQueueName + ccHostName + ccTag +
				SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)) * sizeof(WCHAR), g_pvAllocData);

	if (! lpszFormatName)
		return NULL;

	memcpy (lpszFormatName, MSMQ_SC_FORMAT_DIRECT_OS, sizeof(WCHAR) * SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS));
	memcpy (&lpszFormatName[SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)], lpHost, ccHostName * sizeof(WCHAR));
	memcpy (&lpszFormatName[ccHostName + SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)], lpPath, ccQueueName * sizeof(WCHAR));

	if (a_pqp->bIsJournal)
		memcpy (&lpszFormatName[ccHostName + ccQueueName + SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS) - 1], MSMQ_SC_FORMAT_JOURNAL, sizeof (MSMQ_SC_FORMAT_JOURNAL));

	return lpszFormatName;
}

WCHAR	*scutil_MakeFormatName (WCHAR *a_lpszHostName, WCHAR *a_lpszQueueName, ScQueueParms *a_pqp) {
	if (a_pqp->bIsDeadLetter)
		return scutil_DeadLetterQueue ();

	if (a_pqp->bIsMachineJournal)
		return scutil_MachineJournalQueue ();

	int ccHostNameLen  = wcslen (a_lpszHostName);
	int ccQueueNameLen = wcslen (a_lpszQueueName);
	int ccx1 =   (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTP)  ? SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTP) :
				((a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTPS) ? SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTPS) :
				((a_pqp->bFormatType == SCFILE_QP_FORMAT_TCP)   ? SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_TCP) :
																  SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)));

	int ccx2 = a_pqp->bIsPublic ? 0 : SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE) + 1;

	int ccTag = 0;

	if (a_pqp->bIsJournal)
		ccTag = SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_JOURNAL);

	WCHAR *lpszFormatName = (WCHAR *)g_funcAlloc ((ccQueueNameLen + ccHostNameLen + ccx1 + ccx2 + 2 + ccTag) * sizeof(WCHAR), g_pvAllocData);

	if (! lpszFormatName)
		return NULL;

	WCHAR chPathSeparator;

	switch (a_pqp->bFormatType) {
	case SCFILE_QP_FORMAT_HTTP:
		memcpy (lpszFormatName, MSMQ_SC_FORMAT_DIRECT_HTTP, sizeof(MSMQ_SC_FORMAT_DIRECT_HTTP));
		chPathSeparator = L'/';
		break;

	case SCFILE_QP_FORMAT_HTTPS:
		memcpy (lpszFormatName, MSMQ_SC_FORMAT_DIRECT_HTTPS, sizeof(MSMQ_SC_FORMAT_DIRECT_HTTPS));
		chPathSeparator = L'/';
		break;

	case SCFILE_QP_FORMAT_TCP:
		memcpy (lpszFormatName, MSMQ_SC_FORMAT_DIRECT_TCP, sizeof(MSMQ_SC_FORMAT_DIRECT_TCP));
		chPathSeparator = L'\\';
		break;

	default:
		memcpy (lpszFormatName, MSMQ_SC_FORMAT_DIRECT_OS, sizeof(MSMQ_SC_FORMAT_DIRECT_OS));
		chPathSeparator = L'\\';
		break;
	}

	memcpy (&lpszFormatName[ccx1], a_lpszHostName, ccHostNameLen * sizeof(WCHAR));
	lpszFormatName[ccx1 + ccHostNameLen] = chPathSeparator;

	if (ccx2) {
		memcpy (&lpszFormatName[ccHostNameLen + ccx1 + 1], MSMQ_SC_PATHNAME_PRIVATE, (ccx2 - 1) * sizeof(WCHAR));
		lpszFormatName[ccx1 + ccx2 + ccHostNameLen] = chPathSeparator;
	}

	memcpy (&lpszFormatName[ccHostNameLen + ccx1 + ccx2 + 1], a_lpszQueueName, (ccQueueNameLen + 1) * sizeof(WCHAR));

	if (a_pqp->bIsJournal)
		memcpy (&lpszFormatName[ccHostNameLen + ccQueueNameLen + ccx1 + ccx2 + 1], MSMQ_SC_FORMAT_JOURNAL, sizeof (MSMQ_SC_FORMAT_JOURNAL));

	return lpszFormatName;
}

//
//	Note: supported format names are
//		DIRECT=OS:machinename[\private$]\queuename
//		DIRECT=TCP:ipaddress[\private$]\queuename
//		DIRECT=HTTP://machinename/queuename
//		DIRECT=HTTPS://machinename/queuename
//
int scutil_ParseNonLocalDirectFormatName (WCHAR *a_lpszFormatName, WCHAR *&a_lpszHostName, WCHAR *&a_lpszQueueName, ScQueueParms *a_pqp) {
	SVSUTIL_ASSERT (a_lpszFormatName && a_pqp);
	SVSUTIL_ASSERT (! a_pqp->bIsIncoming);

	a_lpszHostName = NULL;
	a_lpszQueueName = NULL;

	WCHAR *ptr, *ptr2;

	if (wcsnicmp (a_lpszFormatName, MSMQ_SC_FORMAT_DIRECT_OS, SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)) == 0) {
		a_pqp->bFormatType = SCFILE_QP_FORMAT_OS;
		ptr = &a_lpszFormatName[SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)];
		ptr2 = wcschr (ptr, L'\\');
	} else if (wcsnicmp (a_lpszFormatName, MSMQ_SC_FORMAT_DIRECT_TCP, SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_TCP)) == 0) {
		a_pqp->bFormatType = SCFILE_QP_FORMAT_TCP;
		ptr = &a_lpszFormatName[SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_TCP)];
		ptr2 = wcschr (ptr, L'\\');
	} else if (wcsnicmp (a_lpszFormatName, MSMQ_SC_FORMAT_DIRECT_HTTP, SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTP)) == 0) {
		a_pqp->bFormatType = SCFILE_QP_FORMAT_HTTP;
		ptr = &a_lpszFormatName[SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTP)];
		ptr2 = wcschr (ptr, L'/');
	} else if (wcsnicmp (a_lpszFormatName, MSMQ_SC_FORMAT_DIRECT_HTTPS, SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTPS)) == 0) {
		a_pqp->bFormatType = SCFILE_QP_FORMAT_HTTPS;
		ptr = &a_lpszFormatName[SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTPS)];
		ptr2 = wcschr (ptr, L'/');
	} else
		return FALSE;

	if (! ptr2)
		return FALSE;

	WCHAR *ptr3;

	if (((a_pqp->bFormatType == SCFILE_QP_FORMAT_OS) || (a_pqp->bFormatType == SCFILE_QP_FORMAT_TCP)) &&
			(wcsnicmp (ptr2 + 1, MSMQ_SC_PATHNAME_PRIVATE, SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE)) == 0)) {
		ptr3 = ptr2 + 1 + SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE);
		a_pqp->bIsPublic = FALSE;
	} else {
		ptr3 = ptr2;
		a_pqp->bIsPublic = TRUE;
	}

	if (*ptr3 != *ptr2)
		return FALSE;

	++ptr3;

	if (*ptr3 == '\0')
		return FALSE;

	WCHAR *ptr4 = wcschr (ptr3, L';');

	int ccHostName = ptr2 - ptr;
	if ((ccHostName == 1) && (*ptr == L'.')) {
		ptr = gMachine->lpszHostName;
		ccHostName = wcslen (ptr);
	}

	if (wcsnicmp(gMachine->lpszHostName, ptr, ccHostName) == 0) {
		a_pqp->bIsIncoming = TRUE;
		return FALSE;
	}

	a_lpszHostName = (WCHAR *)g_funcAlloc ((ccHostName + 1) * sizeof(WCHAR), g_pvAllocData);

	if (! a_lpszHostName)
		return FALSE;

	memcpy (a_lpszHostName, ptr, ccHostName * sizeof(WCHAR));
	a_lpszHostName[ccHostName] = L'\0';

	if (scutil_IsLocalTCP (a_lpszHostName)) {
		g_funcFree (a_lpszHostName, g_pvFreeData);
		a_lpszHostName = NULL;
		a_pqp->bIsIncoming = TRUE;
		return FALSE;
	}

	int ccQueueName = ptr4 ? ptr4 - ptr3 : wcslen(ptr3);

	a_lpszQueueName = (WCHAR *)g_funcAlloc ((ccQueueName + 1) * sizeof(WCHAR), g_pvAllocData);

	if (! a_lpszQueueName) {
		g_funcFree (a_lpszHostName, g_pvFreeData);
		a_lpszHostName = NULL;
		return FALSE;
	}

	memcpy (a_lpszQueueName, ptr3, ccQueueName * sizeof(WCHAR));
	a_lpszQueueName[ccQueueName] = L'\0';

	return TRUE;
}

int scutil_ParsePathName (WCHAR *a_lpszPathName, WCHAR *&a_lpszHostName, WCHAR *&a_lpszQueueName, ScQueueParms *a_pqp) {
	SVSUTIL_ASSERT (a_lpszPathName && a_pqp);

	a_lpszHostName = NULL;
	a_lpszQueueName = NULL;

	WCHAR chPathSeparator = ((a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTP) || (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTPS)) ? L'/' : L'\\';

	if (wcschr (a_lpszPathName, L';'))
		return FALSE;

	WCHAR *ptr = a_lpszPathName;
	WCHAR *ptr2 = wcschr (ptr, chPathSeparator);

	if (! ptr2)
		return FALSE;

	WCHAR *ptr3;
	int bIsPublic;

	if (wcsnicmp (ptr2 + 1, MSMQ_SC_PATHNAME_PRIVATE, SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE)) == 0) {
		ptr3 = ptr2 + 1 + SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE);
		bIsPublic = FALSE;
	} else {
		ptr3 = ptr2;
		bIsPublic = TRUE;
	}

	if (*ptr3 != chPathSeparator)
		return FALSE;

	++ptr3;

	if (*ptr3 == '\0')
		return FALSE;

	int ccHostName = ptr2 - ptr;
	if ((ccHostName == 1) && (*ptr == L'.')) {
		ptr = gMachine->lpszHostName;
		ccHostName = wcslen (ptr);
	}

	int bIsIncoming = (wcsnicmp(gMachine->lpszHostName, ptr, ccHostName) == 0);

	if (bIsIncoming && bIsPublic)
		return FALSE;

	a_lpszHostName = (WCHAR *)g_funcAlloc ((ccHostName + 1) * sizeof(WCHAR), g_pvAllocData);

	if (! a_lpszHostName)
		return FALSE;

	memcpy (a_lpszHostName, ptr, ccHostName * sizeof(WCHAR));
	a_lpszHostName[ccHostName] = L'\0';

	int ccQueueName = wcslen(ptr3);

	a_lpszQueueName = (WCHAR *)g_funcAlloc ((ccQueueName + 1) * sizeof(WCHAR), g_pvAllocData);

	if (! a_lpszQueueName) {
		g_funcFree (a_lpszHostName, g_pvFreeData);
		a_lpszHostName = NULL;
		return FALSE;
	}

	memcpy (a_lpszQueueName, ptr3, ccQueueName * sizeof(WCHAR));
	a_lpszQueueName[ccQueueName] = L'\0';

	a_pqp->bIsIncoming = bIsIncoming;
	a_pqp->bIsPublic   = bIsPublic;

	return TRUE;
}

WCHAR *scutil_MakePathName   (WCHAR *a_lpszHostName,   WCHAR *a_lpszQueueName, ScQueueParms *a_pqp) {
	SVSUTIL_ASSERT (a_lpszHostName && a_lpszQueueName && a_pqp);

	int ccHostLen = wcslen(a_lpszHostName);
	int ccQueueLen = wcslen (a_lpszQueueName);
	int ccPrivateTag = a_pqp->bIsPublic ? 0 : SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE) + 1;
	int iSz = (ccHostLen + ccQueueLen + 2 + ccPrivateTag) * sizeof(WCHAR);

	WCHAR chPathSeparator = ((a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTP) || (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTPS)) ? L'/' : L'\\';

	WCHAR *lpszPathName = (WCHAR *)g_funcAlloc (iSz, g_pvAllocData);

	memcpy (lpszPathName, a_lpszHostName, sizeof(WCHAR) * ccHostLen);
	lpszPathName[ccHostLen] = chPathSeparator;

	if (ccPrivateTag) {
		memcpy (&lpszPathName[ccHostLen+1], MSMQ_SC_PATHNAME_PRIVATE, sizeof (WCHAR) * SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE));
		lpszPathName[ccHostLen + 1 + SVSUTIL_CONSTSTRLEN(MSMQ_SC_PATHNAME_PRIVATE)] = chPathSeparator;
	}

	memcpy (&lpszPathName[ccHostLen + 1 + ccPrivateTag], a_lpszQueueName, sizeof(WCHAR) * (ccQueueLen + 1));

	return lpszPathName;
}

WCHAR	*scutil_MakeFileName (WCHAR *a_lpszQueueHost, WCHAR *a_lpszQueueName, ScQueueParms *a_pqp) {
	SVSUTIL_ASSERT (a_lpszQueueHost && a_lpszQueueName && a_pqp);

	if (a_pqp->bIsIncoming)
		a_lpszQueueHost = MSMQ_SC_LOCALHOST;

	WCHAR *lpszFileExt = (! a_pqp->bIsIncoming) ? MSMQ_SC_EXT_OQ :
							 (a_pqp->bIsJournal ? MSMQ_SC_EXT_JQ :
					  (a_pqp->bIsMachineJournal ? MSMQ_SC_EXT_MJ : MSMQ_SC_EXT_IQ));

	int ccDirLen   = wcslen(gMachine->lpszDirName);
	int ccQueueLen = wcslen(a_lpszQueueName);
	int ccHostLen  = wcslen(a_lpszQueueHost);
	int ccPublicToken = a_pqp->bIsPublic ? SVSUTIL_CONSTSTRLEN (MSMQ_SC_FILENAME_PUBLIC) : 0;
	int ccHttpToken = 0;

	if (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTP)
		ccHttpToken = SVSUTIL_CONSTSTRLEN(MSMQ_SC_FILENAME_HTTP);
	else if (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTPS)
		ccHttpToken = SVSUTIL_CONSTSTRLEN(MSMQ_SC_FILENAME_HTTPS);

	int ccFileName = ccDirLen + ccHostLen + ccQueueLen + ccPublicToken + ccHttpToken + wcslen(lpszFileExt) + 2 + 3;

	WCHAR *lpszFileName = (WCHAR *)g_funcAlloc (ccFileName * sizeof(WCHAR), g_pvAllocData);

	memcpy (lpszFileName, gMachine->lpszDirName, sizeof(WCHAR) * ccDirLen);
	lpszFileName[ccDirLen] = L'\\';
	memcpy (lpszFileName + ccDirLen + 1, a_lpszQueueHost, sizeof(WCHAR) * ccHostLen);
	lpszFileName[ccDirLen + ccHostLen + 1] = L' ';
	lpszFileName[ccDirLen + ccHostLen + 2] = L'-';
	lpszFileName[ccDirLen + ccHostLen + 3] = L' ';
	if (ccPublicToken)
		memcpy (lpszFileName + ccDirLen + ccHostLen + 4, MSMQ_SC_FILENAME_PUBLIC, sizeof(MSMQ_SC_FILENAME_PUBLIC));

	if (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTP)
		memcpy (lpszFileName + ccDirLen + ccPublicToken + ccHostLen + 4, MSMQ_SC_FILENAME_HTTP, sizeof(WCHAR) * ccHttpToken);
	else if (a_pqp->bFormatType == SCFILE_QP_FORMAT_HTTPS)
		memcpy (lpszFileName + ccDirLen + ccPublicToken + ccHostLen + 4, MSMQ_SC_FILENAME_HTTPS, sizeof(WCHAR) * ccHttpToken);

	memcpy (lpszFileName + ccDirLen + ccPublicToken + ccHttpToken + ccHostLen + 4, a_lpszQueueName, sizeof(WCHAR) * ccQueueLen);
	wcscpy (lpszFileName + ccDirLen + ccPublicToken + ccHttpToken + ccHostLen + ccQueueLen + 4, lpszFileExt);

	WCHAR *p = lpszFileName + ccDirLen + ccPublicToken + ccHttpToken + ccHostLen + 4;

	while (*p) {
		if ((*p == '\\') || (*p == '/'))
			*p = '_';
		++p;
	}

	return lpszFileName;
}

WCHAR *scutil_QFtoString (QUEUE_FORMAT *pqft) {
	WCHAR   szBuffer[_MAX_PATH];
	ULONG	ulSize = 0;

	NTSTATUS nts = MQpQueueFormatToFormatName(pqft, szBuffer, _MAX_PATH, &ulSize);
	if (nts == MQ_OK)
		return svsutil_wcsdup (szBuffer);
	if (nts == MQ_ERROR_FORMATNAME_BUFFER_TOO_SMALL) {
		WCHAR *lpRes = (WCHAR *)g_funcAlloc (ulSize * sizeof (WCHAR), g_pvAllocData);
		if (! lpRes)
			return NULL;
		if (MQ_OK == MQpQueueFormatToFormatName(pqft, lpRes, ulSize, &ulSize))
			return lpRes;

		g_funcFree (lpRes, g_pvFreeData);
	}
	return NULL;
}

/*++
Abstract:

    Format Name parsing.
    QUEUE_FORMAT <--> Format Name String conversion routines
--*/

//=========================================================
//
//  Format Name String -> QUEUE_FORMAT conversion routines
//
//=========================================================

//---------------------------------------------------------
//
//  Skip white space characters, return next non ws char
//
//  N.B. if no white space is needed uncomment next line
//#define skip_ws(p) (p)
inline LPCWSTR skip_ws(LPCWSTR p)
{
    //
    //  Don't skip first non white space
    //
    while(iswspace(*p))
    {
        ++p;
    }

    return p;
}


//---------------------------------------------------------
//
//  Skip white space characters, return next non ws char
//
inline LPCWSTR FindPathNameDelimiter(LPCWSTR p)
{
    return wcschr(p, PN_DELIMITER_C);
}


//---------------------------------------------------------
//
//  Parse Format Name Type prefix string.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParsePrefixString(LPCWSTR p, QUEUE_FORMAT_TYPE& qft)
{
    const int unique = 1;
    //----------------0v-------------------------
    ASSERT(L'U' == FN_PUBLIC_TOKEN    [unique]);
    ASSERT(L'R' == FN_PRIVATE_TOKEN   [unique]);
    ASSERT(L'O' == FN_CONNECTOR_TOKEN [unique]);
    ASSERT(L'A' == FN_MACHINE_TOKEN   [unique]);
    ASSERT(L'I' == FN_DIRECT_TOKEN    [unique]);
    //----------------0^-------------------------

    //
    //  accelarate token recognition by checking 3rd character
    //
    switch(towupper(p[unique]))
    {
        //  pUblic
        case L'U':
            qft = QUEUE_FORMAT_TYPE_PUBLIC;
            if(_wcsnicmp(p, FN_PUBLIC_TOKEN, FN_PUBLIC_TOKEN_LEN) == 0)
                return (p + FN_PUBLIC_TOKEN_LEN);
            break;

        //  pRivate
        case L'R':
            qft = QUEUE_FORMAT_TYPE_PRIVATE;
            if(_wcsnicmp(p, FN_PRIVATE_TOKEN, FN_PRIVATE_TOKEN_LEN) == 0)
                return (p + FN_PRIVATE_TOKEN_LEN);
            break;

        //  cOnnector
        case L'O':
            qft = QUEUE_FORMAT_TYPE_CONNECTOR;
            if(_wcsnicmp(p, FN_CONNECTOR_TOKEN, FN_CONNECTOR_TOKEN_LEN) == 0)
                return (p + FN_CONNECTOR_TOKEN_LEN);
            break;
    
        //  mAchine
        case L'A':
            qft = QUEUE_FORMAT_TYPE_MACHINE;
            if(_wcsnicmp(p, FN_MACHINE_TOKEN, FN_MACHINE_TOKEN_LEN) == 0)
                return (p + FN_MACHINE_TOKEN_LEN);
            break;
            
        //  dIrect
        case L'I':
            qft = QUEUE_FORMAT_TYPE_DIRECT;
            if(_wcsnicmp(p, FN_DIRECT_TOKEN, FN_DIRECT_TOKEN_LEN) == 0)
                return (p + FN_DIRECT_TOKEN_LEN);
            break;
    }

    return 0;
}


//---------------------------------------------------------
//
//  Parse a guid string, into guid.
//  Return next char to parse on success, 0 on failure.
//
LPCWSTR scutil_ParseGuidString(LPCWSTR p, GUID* pg)
{
    //
    //  N.B. scanf stores the results in an int, no matter what the field size
    //      is. Thus we store the result in tmp variabes.
    //
    int n;
    UINT w2, w3, d[8];
    if(swscanf(
            p,
            GUID_FORMAT L"%n",
            &pg->Data1,
            &w2, &w3,                       //  Data2, Data3
            &d[0], &d[1], &d[2], &d[3],     //  Data4[0..3]
            &d[4], &d[5], &d[6], &d[7],     //  Data4[4..7]
            &n                              //  number of characters scaned
            ) != 11)
    {
        //
        //  not all 11 fields where not found.
        //
        return 0;
    }

    pg->Data2 = (WORD)w2;
    pg->Data3 = (WORD)w3;
    for(int i = 0; i < 8; i++)
    {
        pg->Data4[i] = (BYTE)d[i];
    }

    return (p + n);
}

// called on scapi_MQSendMessage on determining how big a buffer to allocate for packet.
ULONG CalcFwdRevViaSectionSize(CAPROPVARIANT *pEntry, HANDLE hCallerProc) {
	ULONG ulNeeded = sizeof(CDataHeader);

	for (unsigned int i =0; i < pEntry->cElems; i++) {
		PROPVARIANT *pVar = &pEntry->pElems[i];
		pVar = MAP(pVar,PROPVARIANT*);

		if (! pVar || pVar->vt != VT_LPWSTR) 
			return 0;

		WCHAR *szVia = MAP(pVar->pwszVal, WCHAR *);
		if (!szVia)
			return 0;

		ulNeeded += ((wcslen(szVia)+1) * sizeof(WCHAR));
	}

	SVSUTIL_ASSERT(ulNeeded > sizeof(CDataHeader));
	return ALIGNUP4(ulNeeded);
}

// called on scapi_RetrievePacketInfo to determine how space required by calling app.
ULONG CalcFwdRevViaSectionSize(CDataHeader *pFwdRevEntry, DWORD *pdwElements) {
	SVSUTIL_ASSERT(*pdwElements == 0);
	if (!pFwdRevEntry)
		return 0;

	ULONG ulNeeded  = sizeof(CAPROPVARIANT);

	int   iLen      = pFwdRevEntry->GetDataLengthInWCHARs();
	WCHAR *szStart  = (WCHAR*) pFwdRevEntry->GetData();
	WCHAR *pszTrav  = szStart;

	// VIA list is stored as a NULL separated multi-string.  Elements
	// may be set to \0 to indicate an empty tag.
	while (pszTrav - szStart < iLen) {
		int iElementLen = wcslen(pszTrav)+1;

		ulNeeded += (iElementLen*sizeof(WCHAR) + sizeof(CAPROPVARIANT));
		pszTrav  += iElementLen;
		(*pdwElements)++;
	}
	SVSUTIL_ASSERT(pszTrav-szStart == iLen);
	return ulNeeded;
}

void SetFwdRevViaSection(CDataHeader *pFwdRevEntry, CAPROPVARIANT *pPropVar, DWORD dwElements) {
	int          iLen      = pFwdRevEntry->GetDataLengthInWCHARs();
	WCHAR        *szStart  = (WCHAR*) pFwdRevEntry->GetData();
	WCHAR        *pszTrav  = szStart;
	WCHAR        *szWrite;
	PROPVARIANT  *pPropWrite;

	pPropVar->cElems  = dwElements;
	pPropWrite        = pPropVar->pElems;
	szWrite           = (WCHAR*) ((CHAR*)pPropWrite+ sizeof(PROPVARIANT)*dwElements);
	memset(pPropWrite,0,dwElements*sizeof(PROPVARIANT));
	
	// VIA list is stored as a NULL separated multi-string.  Elements
	// may be set to \0 to indicate an empty tag.
	while (pszTrav - szStart < iLen) {
		int iElementLen = wcslen(pszTrav)+1;
		memcpy(szWrite,pszTrav,iElementLen*sizeof(WCHAR));

		pPropWrite->vt      = VT_LPWSTR;
		pPropWrite->pwszVal = szWrite;
		pPropWrite++;
		
		szWrite	+= iElementLen;
		pszTrav += iElementLen;
	}
	SVSUTIL_ASSERT(pszTrav-szStart == iLen);
}

WCHAR * GetNextHopOnFwdList(ScPacketImage *pImage) {
	// If we're receiving a message off wire (fSRMPGenerated=TRUE), then the 1st entry in pFwdVia header
	// is the current machine name's <fwd><via> entry, skip it because we want to route
	// to the next queue in line.  When we're generating the message locally (fSRMPGenerated=FALSE) then 
	// 1st element in pFwdViaHeader is queue to route to.

	if (! pImage->flags.fSRMPGenerated)
		return (WCHAR*) pImage->sect.pFwdViaHeader->GetData();

	WCHAR *szFirstElement  = (WCHAR*) pImage->sect.pFwdViaHeader->GetData();
	int    ccElementList   = pImage->sect.pFwdViaHeader->GetDataLengthInWCHARs();
	int    ccFirstElement  = wcslen(szFirstElement) + 1;

	// only one entry is in <fwd> list.
	if (ccElementList == ccFirstElement)
		return NULL;

	return (szFirstElement+ccFirstElement);
}

//---------------------------------------------------------
//
//  Parse private id uniquifier, into guid.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParsePrivateIDString(LPCWSTR p, ULONG* pUniquifier)
{
    int n;
    if(swscanf(
            p,
            PRIVATE_ID_FORMAT L"%n",
            pUniquifier,
            &n                              //  number of characters scaned
            ) != 1)
    {
        //
        //  private id field was not found.
        //
        return 0;
    }

    return (p + n);
}

extern const WCHAR cszHttpPrefix[]    = L"HTTP://";
extern const WCHAR cszHttpsPrefix[]   = L"HTTPS://";
extern const DWORD ccHttpPrefix      = SVSUTIL_CONSTSTRLEN(cszHttpPrefix);
extern const DWORD ccHttpsPrefix     = SVSUTIL_CONSTSTRLEN(cszHttpsPrefix);


BOOL IsUriHttp(const WCHAR *szQueue) {
	return (0 == _wcsnicmp(szQueue,cszHttpPrefix,ccHttpPrefix));
}

BOOL IsUriHttps(const WCHAR *szQueue) {
	return (0 == _wcsnicmp(szQueue,cszHttpsPrefix,ccHttpsPrefix));
}


//---------------------------------------------------------
//
//  Parse direct token type infix string.
//  Return next char to parse on success, 0 on failure.
//
LPCWSTR ParseDirectTokenString(LPCWSTR p, DIRECT_TOKEN_TYPE& dtt)
{
    const int unique = 0;
    //-----------------------v-------------------
    ASSERT(L'O' == FN_DIRECT_OS_TOKEN   [unique]);
    ASSERT(L'T' == FN_DIRECT_TCP_TOKEN  [unique]);
    ASSERT(L'S' == FN_DIRECT_SPX_TOKEN  [unique]);
    //-----------------------^-------------------

    //
    //  accelarate token recognition by checking 1st character
    //
    switch(towupper(p[unique]))
    {
        // Os:
        case L'O':
            dtt = DT_OS;
            if(_wcsnicmp(p, FN_DIRECT_OS_TOKEN, FN_DIRECT_OS_TOKEN_LEN) == 0)
                return (p + FN_DIRECT_OS_TOKEN_LEN);
            break;

        // Tcp:
        case L'T':
            dtt = DT_TCP;
            if(_wcsnicmp(p, FN_DIRECT_TCP_TOKEN, FN_DIRECT_TCP_TOKEN_LEN) == 0)
                return (p + FN_DIRECT_TCP_TOKEN_LEN);
            break;

        // Spx:
        case L'S':
            dtt = DT_SPX;
            if(_wcsnicmp(p, FN_DIRECT_SPX_TOKEN, FN_DIRECT_SPX_TOKEN_LEN) == 0)
                return (p + FN_DIRECT_SPX_TOKEN_LEN);
            break;

        // http:// or https://
        case L'H':
			if (IsUriHttp(p))
			{
				dtt = DT_HTTP;
				return p + ccHttpPrefix;
			}
			if (IsUriHttps(p))
			{
				dtt = DT_HTTPS;
				return p + ccHttpsPrefix;
			}
			break;
    }

    return 0;
}

//---------------------------------------------------------
//
//  Parse queue name string, (private, public)
//  N.B. queue name must end with either format name suffix
//      delimiter aka ';' or with end of string '\0'
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseQueueNameString(LPCWSTR p, BOOL* pfPrivate)
{
    if(_wcsnicmp(p, PRIVATE_QUEUE_PATH_INDICATIOR, PRIVATE_QUEUE_PATH_INDICATIOR_LENGTH) == 0)
    {
        *pfPrivate = TRUE;
        p += PRIVATE_QUEUE_PATH_INDICATIOR_LENGTH;
    }
    else
    {
        *pfPrivate = FALSE;
    }

    //
    //  Zero length queue name is illeagal
    //
    if(*p == L'\0')
        return 0;

    while(
        (*p != L'\0') &&
        (*p != PN_DELIMITER_C) &&
        (*p != FN_SUFFIX_DELIMITER_C)
        )
    {
        ++p;
    }

    //
    //  Path name delimiter is illeagal in queue name
    //
    if(*p == PN_DELIMITER_C)
        return 0;

    return p;
}


//---------------------------------------------------------
//
//  Parse machine name in a path name
//  N.B. machine name must end with a path name delimiter aka slash '\\'
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseMachineNameString(LPCWSTR p)
{
    //
    //  Zero length machine name is illeagal
    //  don't fall off the string (p++)
    //
    if(*p == PN_DELIMITER_C)
        return 0;

    if((p = FindPathNameDelimiter(p)) == 0)
        return 0;

    return (p + 1);
}


//---------------------------------------------------------
//
//  Check if this is an expandable machine path. i.e., ".\\"
//
inline BOOL IsExpandableMachinePath(LPCWSTR p)
{
    return ((p[0] == PN_LOCAL_MACHINE_C) && (p[1] == PN_DELIMITER_C));
}

//---------------------------------------------------------
//
//  Optionally expand a path name with local machine name.
//  N.B. expansion is done if needed.
//  return pointer to new/old string
//
static LPCWSTR ExpandPathName(LPCWSTR pStart, ULONG offset, LPWSTR* ppStringToFree)
{
    if((ppStringToFree == 0) || !IsExpandableMachinePath(&pStart[offset]))
        return pStart;

    int len = wcslen(pStart);
	int iHostNameLen = wcslen (gMachine->lpszHostName);

    LPWSTR pCopy = (WCHAR *)g_funcAlloc (sizeof(WCHAR) * (len + iHostNameLen + 1 - 1), g_pvAllocData);

    //
    //  copy prefix, till offset '.'
    //
    memcpy(
        pCopy,
        pStart,
        offset * sizeof(WCHAR)
        );

    //
    //  copy computer name to offset
    //

    memcpy(
        pCopy + offset,
        gMachine->lpszHostName,
        iHostNameLen * sizeof(WCHAR)
        );

    //
    //  copy rest of string not including dot '.'
    //
    memcpy(
        pCopy + offset + iHostNameLen,
        pStart + offset + 1,                        // skip dot
        (len - offset - 1 + 1) * sizeof(WCHAR)      // skip dot, include '\0'
        );

    *ppStringToFree = pCopy;
    return pCopy;
}


//---------------------------------------------------------
//
//  Parse OS direct format string. (check validity of path
//  name and optionally expand it)
//  ppDirectFormat - expended direct format string. (in out)
//  ppStringToFree - return string to free if needed.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseDirectOSString(LPCWSTR p, LPCWSTR* ppDirectFormat, LPWSTR* ppStringToFree)
{
    LPCWSTR pMachineName = p;
    LPCWSTR pStringToCopy = *ppDirectFormat;

    if((p = ParseMachineNameString(p)) == 0)
        return 0;

    BOOL fPrivate;
    if((p = ParseQueueNameString(p, &fPrivate)) == 0)
        return 0;


    *ppDirectFormat = ExpandPathName(pStringToCopy, (pMachineName - pStringToCopy), ppStringToFree);

    return p;
}


//---------------------------------------------------------
//
//  Parse net (tcp/spx) address part of direct format string.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseNetAddressString(LPCWSTR p)
{
    //
    //  Zero length address string is illeagal
    //  don't fall off the string (p++)
    //
    if(*p == PN_DELIMITER_C)
        return 0;

    if((p = FindPathNameDelimiter(p)) == 0)
        return 0;

    return (p + 1);
}

//---------------------------------------------------------
//
//  Parse HTTP / HTTPS direct format string.
//  Return next char to parse on success, 0 on failure.
//
LPCWSTR
ParseDirectHttpString(
    LPCWSTR p,
    LPCWSTR* ppDirectFormat, 
    LPWSTR* ppStringToFree
    )
{
    size_t MachineNameOffset = p - *ppDirectFormat;

    //
    // Check host name is available
    //
    if(wcschr(FN_HTTP_SEPERATORS, *p) != 0)
        return NULL;

	for(; *p != L'\0' && *p != FN_SUFFIX_DELIMITER_C && *p != FN_MQF_SEPARATOR_C; p++) 	{
        NULL;
	}

    *ppDirectFormat = ExpandPathName(*ppDirectFormat, MachineNameOffset, ppStringToFree);
    return p;
}



//---------------------------------------------------------
//
//  Parse net (tcp/spx) direct format string. (check validity of queue name)
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseDirectNetString(LPCWSTR p)
{
    if((p = ParseNetAddressString(p)) == 0)
        return 0;

    BOOL fPrivate;
    if((p = ParseQueueNameString(p, &fPrivate)) == 0)
        return 0;

    return p;
}


//---------------------------------------------------------
//
//  Parse direct format string.
//  return expended direct format string.
//  return string to free if needed.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseDirectString(LPCWSTR p, LPCWSTR* ppExpandedDirectFormat, LPWSTR* ppStringToFree)
{
    *ppExpandedDirectFormat = p;

    DIRECT_TOKEN_TYPE dtt;
    if((p = ParseDirectTokenString(p, dtt)) == 0)
        return 0;

    switch(dtt)
    {
        case DT_OS:
            p = ParseDirectOSString(p, ppExpandedDirectFormat, ppStringToFree);
            break;
                
        case DT_TCP:
        case DT_SPX:
            p = ParseDirectNetString(p);
            break;

        case DT_HTTP:
        case DT_HTTPS:
            p = ParseDirectHttpString(p, ppExpandedDirectFormat, ppStringToFree);
            break;

        default:
            ASSERT(0);
    }

    if (p) {
        SVSUTIL_ASSERT(*p != FN_SUFFIX_DELIMITER_C);
    }

    return p;
}


//---------------------------------------------------------
//
//  Parse format name suffix string.
//  Return next char to parse on success, 0 on failure.
//
static LPCWSTR ParseSuffixString(LPCWSTR p, QUEUE_SUFFIX_TYPE& qst)
{
    const int unique = 5;
    //---------------01234v----------------------
    ASSERT(L'N' == FN_JOURNAL_SUFFIX    [unique]);
    ASSERT(L'L' == FN_DEADLETTER_SUFFIX [unique]);
    ASSERT(L'X' == FN_DEADXACT_SUFFIX   [unique]);
    ASSERT(L'O' == FN_XACTONLY_SUFFIX   [unique]);
    //---------------01234^----------------------

    //
    //  we already know that first character is ";"
    //
    ASSERT(*p == FN_SUFFIX_DELIMITER_C);

    //
    //  accelarate token recognition by checking 6th character
    //
    switch(towupper(p[unique]))
    {
        // ;jourNal
        case L'N':
            qst = QUEUE_SUFFIX_TYPE_JOURNAL;
            if(_wcsnicmp(p, FN_JOURNAL_SUFFIX, FN_JOURNAL_SUFFIX_LEN) == 0)
                return (p + FN_JOURNAL_SUFFIX_LEN);
            break;

        // ;deadLetter
        case L'L':
            qst = QUEUE_SUFFIX_TYPE_DEADLETTER;
            if(_wcsnicmp(p, FN_DEADLETTER_SUFFIX, FN_DEADLETTER_SUFFIX_LEN) == 0)
                return (p + FN_DEADLETTER_SUFFIX_LEN);
            break;

        // ;deadXact
        case L'X':
            qst = QUEUE_SUFFIX_TYPE_DEADXACT;
            if(_wcsnicmp(p, FN_DEADXACT_SUFFIX, FN_DEADXACT_SUFFIX_LEN) == 0)
                return (p + FN_DEADXACT_SUFFIX_LEN);
            break;

        // ;xactOnly
        case L'O':
            qst = QUEUE_SUFFIX_TYPE_XACTONLY;
            if(_wcsnicmp(p, FN_XACTONLY_SUFFIX, FN_XACTONLY_SUFFIX_LEN) == 0)
                return (p + FN_XACTONLY_SUFFIX_LEN);
            break;
    }

    return 0;
}


//---------------------------------------------------------
//
//  Function:
//      RTpFormatNameToQueueFormat
//
//  Description:
//      Convert a format name string to a QUEUE_FORMAT union.
//
//---------------------------------------------------------
static BOOL
RTpFormatNameToQueueFormat(
    LPCWSTR pfn,            // pointer to format name string
    QUEUE_FORMAT* pqf,      // pointer to QUEUE_FORMAT
    LPWSTR* ppStringToFree  // pointer to allocated string need to free at end of use
    )                       // if null, format name will not get expanded
{
    LPCWSTR p = pfn;
    QUEUE_FORMAT_TYPE qft;

    if((p = ParsePrefixString(p, qft)) == 0)
        return FALSE;

    p = skip_ws(p);

    if(*p++ != FN_EQUAL_SIGN_C)
        return FALSE;

    p = skip_ws(p);

    GUID guid;
    switch(qft)
    {
        //
        //  "PUBLIC=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
        //
        case QUEUE_FORMAT_TYPE_PUBLIC:
            if((p = scutil_ParseGuidString(p, &guid)) == 0)
                return FALSE;

            pqf->PublicID(guid);
            break;

        //
        //  "PRIVATE=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\\xxxxxxxx\0"
        //
        case QUEUE_FORMAT_TYPE_PRIVATE:
            if((p = scutil_ParseGuidString(p, &guid)) == 0)
                return FALSE;

            p = skip_ws(p);

            if(*p++ != FN_PRIVATE_SEPERATOR_C)
                return FALSE;

            p = skip_ws(p);

            ULONG uniquifier;
            if((p = ParsePrivateIDString(p, &uniquifier)) == 0)
                return FALSE;

            pqf->PrivateID(guid, uniquifier);
            break;

        //
        //  "DIRECT=OS:bla-bla\0"
        //
        case QUEUE_FORMAT_TYPE_DIRECT:
            LPCWSTR pExpandedDirectFormat;
            if((p = ParseDirectString(p, &pExpandedDirectFormat, ppStringToFree)) == 0)
                return FALSE;

            pqf->DirectID(const_cast<LPWSTR>(pExpandedDirectFormat));
            break;

        //
        //  "MACHINE=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
        //
        case QUEUE_FORMAT_TYPE_MACHINE:
           if((p = scutil_ParseGuidString(p, &guid)) == 0)
                return FALSE;

            pqf->MachineID(guid);
            break;

        //
        //  "CONNECTOR=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
        //
        case QUEUE_FORMAT_TYPE_CONNECTOR:
           if((p = scutil_ParseGuidString(p, &guid)) == 0)
                return FALSE;

            pqf->ConnectorID(guid);
            break;

        default:
            ASSERT(0);
    }

    p = skip_ws(p);

    //
    //  We're at end of string, return now.
    //  N.B. Machine format name *must* have a suffix
    //
    if(*p == L'\0')
        return (qft != QUEUE_FORMAT_TYPE_MACHINE);

    if(*p != FN_SUFFIX_DELIMITER_C)
        return FALSE;

    QUEUE_SUFFIX_TYPE qst;
    if((p = ParseSuffixString(p, qst)) == 0)
        return FALSE;

    p = skip_ws(p);

    //
    //  Only white space padding is allowed.
    //
    if(*p != L'\0')
        return FALSE;

    pqf->Suffix(qst);

    return pqf->Leagal();
}

int scutil_StringToQF (QUEUE_FORMAT *pqf, WCHAR *lpszName) {
	return RTpFormatNameToQueueFormat (lpszName, pqf, NULL);
}

int scutil_GetLanNum (void) {
	char name[MSMQ_SC_REGBUFFER];

	if (gethostname (name, sizeof(name)))
		return 0;

	HOSTENT *phe = gethostbyname (name);
	
	if ((phe->h_addrtype != AF_INET) || (phe->h_length != sizeof (IN_ADDR)) || (*phe->h_addr_list == NULL))
		return 0;

	int iRes = 0;

	IN_ADDR **ppia = (IN_ADDR **)phe->h_addr_list;

	while (*ppia != NULL) {
		if (ntohl((**ppia).S_un.S_addr) != INADDR_LOOPBACK)
			++iRes;

		++ppia;
	}

	return iRes;
}

int scutil_IsLocalTCP (WCHAR *szHostName) {
	static int iTcpAddresses = 0;
	static unsigned long aulTcpAddresses[MSMQ_SC_MAX_IP_INTERFACES];

	if (! szHostName) {
		iTcpAddresses = 0;

		char name[MSMQ_SC_REGBUFFER];

		if (gethostname (name, sizeof(name)))
			return 0;

		HOSTENT *phe = gethostbyname (name);
		if (NULL == phe) {
			scerror_DebugOutM (VERBOSE_MASK_FATAL, (L"gethostbyname(%S) fails.  WSAGetLastError returns 0x%08x\n",name,WSAGetLastError()));
			SVSUTIL_ASSERT(0);
			return 0;
		}
	
		if ((phe->h_addrtype != AF_INET) || (phe->h_length != sizeof (IN_ADDR)) || (*phe->h_addr_list == NULL))
			return FALSE;

		IN_ADDR **ppia = (IN_ADDR **)phe->h_addr_list;

		while ((*ppia != NULL) && (iTcpAddresses < MSMQ_SC_MAX_IP_INTERFACES)) {
			unsigned long ulInterfaceAddress = (**ppia).S_un.S_addr;
			if (ulInterfaceAddress != INADDR_LOOPBACK)
				aulTcpAddresses[iTcpAddresses++] = ulInterfaceAddress;

			++ppia;
		}

		return iTcpAddresses;
	}

	char szaddr[MSMQ_SC_IPBUFFER];

	if (! WideCharToMultiByte (CP_ACP, 0, szHostName, -1, szaddr, sizeof(szaddr), NULL, NULL)) 
		return FALSE;

	unsigned long ul_addr = inet_addr (szaddr);

	if (ul_addr == INADDR_NONE)
		return FALSE;

	for (int i = 0 ; i < iTcpAddresses ; ++i) {
		if (ul_addr == aulTcpAddresses[i])
			return TRUE;
	}

	return FALSE;
}

WCHAR* scutil_StrStrWI(const WCHAR * str1, const WCHAR * str2) {
	WCHAR *cp = (WCHAR *) str1;
	WCHAR *s1, *s2;
	while (*cp) {
		s1 = cp;
		s2 = (WCHAR *) str2;
		while ( *s1 && *s2 && (towlower(*s1) == towlower(*s2)) )
			s1++, s2++;
		if (!*s2)
			return(cp);
		cp++;
	}
	return(NULL);
}

void scutil_ReplaceBackSlashesWithSlashes(WCHAR *a_lpszName) {
	for (WCHAR *p = a_lpszName; *p; p++) {
		if (*p == '\\')
			*p = '/';
	}
}



