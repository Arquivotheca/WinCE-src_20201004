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

    srmpmime.hxx

Abstract:

    MIME Attachment handeler


--*/


#if ! defined (__srmpmime_HXX__)
#define __srmpmime_HXX__	1

#include <windows.h>
#include <svsutil.hxx>
#include <oleauto.h>

#include "srmpdefs.hxx"
#include "scsrmp.hxx"


typedef struct _MIME_ATTACHMENT {
	_MIME_ATTACHMENT   *pNext;
	PSTR               szContentId;
	PSTR               szBody;  // do not free szBody, it's a pointer into original buffer.
	DWORD              cbBody;
} MIME_ATTACHMENT, *PMIME_ATTACHMENT;

class CSrmpMime {
public:
	FixedMemDescr	    *m_pfmdMime;
	MIME_ATTACHMENT     *m_pMime;

	CSrmpMime() {
		m_pfmdMime    = svsutil_AllocFixedMemDescr(sizeof(MIME_ATTACHMENT), 10);
		m_pMime      = NULL;
	}

	~CSrmpMime() {
		while (m_pMime) {
			PMIME_ATTACHMENT pNext = m_pMime->pNext;
			if (m_pMime->szContentId)
				g_funcFree(m_pMime->szContentId,g_pvFreeData);

			svsutil_FreeFixed(m_pMime,m_pfmdMime);
			m_pMime = pNext;
		}

		if (m_pfmdMime)
			svsutil_ReleaseFixedNonEmpty(m_pfmdMime);
	}

	BOOL IsInitialized(void) { 	return m_pfmdMime ? TRUE : FALSE; }

	BOOL CreateNewAttachment(PSTR pszContentId, DWORD cbContentLength, PSTR pszData) {
		PMIME_ATTACHMENT pNext = m_pMime;
		PMIME_ATTACHMENT pNewMime = (PMIME_ATTACHMENT) svsutil_GetFixed(m_pfmdMime);
		if (!pNewMime)
			return FALSE;

		PSTR pszTemp  = strstr(pszContentId,cszCRLF);
		*pszTemp = 0;

		if (NULL == (pNewMime->szContentId = svsutil_strdup(pszContentId))) {
			svsutil_FreeFixed(pNewMime, m_pfmdMime);
			return FALSE;
		}
		*pszTemp = '\r';
		pNewMime->cbBody  = cbContentLength;
		pNewMime->szBody  = pszData;

		m_pMime           = pNewMime;
		pNewMime->pNext   = pNext;
		return TRUE;
	}

	BOOL GetMimeAttachment(const CHAR *szContentId, CHAR **ppszData, DWORD *pcbData);
};


#endif
