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

    SrmpFwd.hxx

Abstract:

    Forwards an SRMP data packet without modifying contents.


--*/


#if ! defined (__srmpfwd_HXX__)
#define __srmpfwd_HXX__	1

#define BOUNDARY_HYPHEN "--"
extern const char cszTextXML[];

class ScSession;

typedef enum {
	SRMP_FWD_ONFIRST,
	SRMP_FWD_ONSECOND,
	SRMP_FWD_PASTSECOND
} SRMP_FWD_VIA_STATE;

class CSrmpFwd : public SVSSAXContentHandler
{
private:
	SVSStack            m_State;
	BOOL                fFoundRevVia;
	BOOL                fSkipOutput;
	SRMP_FWD_VIA_STATE  fwdViaState;

	SRMP_STATE   GetState(void) {
		int iState = (int) m_State.Peek();
		return (SRMP_STATE) iState; 
	}

public:
	SVSXMLWriter writeBuffer;
	WCHAR        *szQueueName;
	WCHAR        *szRevEntry; // do not free in destructor.
	ScSession    *pSession;   // NOTE: Wrap any accesses (read/write) to this structure
	                          // with a critical section!  Do not free in destructor.

	CSrmpFwd(ScSession *pS, WCHAR *szQ, WCHAR *szOriginalURL) {
		szQueueName = NULL;

		if (! m_State.IsStackInitialized())
			return;
	
		m_State.Push((void*)SRMP_ST_UNINITIALIZED);
		fSkipOutput = fFoundRevVia = FALSE;
		fwdViaState = SRMP_FWD_ONFIRST;
		szQueueName = svsutil_wcsdup(szQ);
		szRevEntry  = szOriginalURL;
		pSession    = pS;
	}

	BOOL  IsInitailized(void) { return (szQueueName) ? TRUE : FALSE; }
	CHAR* SetupMIMEBuffer(ScPacketImage *pPacketImage, CHAR **ppszSoapStart, CHAR **ppszSoapEnd, DWORD *pcbMime);

	~CSrmpFwd() {
		while (! m_State.IsEmpty())
			m_State.Pop(); // since no mem was alloced, no need to free

		if (szQueueName)
			g_funcFree(szQueueName,g_pvFreeData);
	}

	BOOL SetState(SRMP_TOKEN tok);
	
    
// ISAXContentHandler
	virtual HRESULT STDMETHODCALLTYPE startDocument(void) {
		return writeBuffer.startDocument();
	}
    
	virtual HRESULT STDMETHODCALLTYPE endDocument(void)     {
		return writeBuffer.endDocument();
	}

	virtual HRESULT STDMETHODCALLTYPE characters( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
	    /* [in] */ int cchChars);

	virtual HRESULT STDMETHODCALLTYPE processingInstruction( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchTarget,
	    /* [in] */ int cchTarget,
	    /* [in] */ const wchar_t __RPC_FAR *pwchData,
	    /* [in] */ int cchData)
	{
		if (!fSkipOutput)
			return writeBuffer.processingInstruction(pwchTarget,cchTarget,pwchData,cchData);

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE skippedEntity( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchName,
	    /* [in] */ int cchName)
	{
		return writeBuffer.skippedEntity(pwchName,cchName);
	}

	virtual HRESULT STDMETHODCALLTYPE endElement( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
	    /* [in] */ int cchNamespaceUri,
	    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
	    /* [in] */ int cchLocalName,
	    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
	    /* [in] */ int cchQName);

	virtual HRESULT STDMETHODCALLTYPE startElement( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
	    /* [in] */ int cchNamespaceUri,
	    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
	    /* [in] */ int cchLocalName,
	    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
	    /* [in] */ int cchQName,
	    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);


	HRESULT WriteRevEntry(BOOL fWriteRevTag);
};

PSTR AllocURL(PCWSTR wszURL);

#endif
