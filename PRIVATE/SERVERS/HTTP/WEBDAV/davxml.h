//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davxml.h
Abstract: Parses XML in POST data for DAV verbs
--*/


#include "httpd.h"


//
//  Base class that other parsers inherit from
//

// By default any character data sent in an XML tag will be discarded.  Set 
// this flag for any properties that character data needs to be saved for.
#define DAV_STATE_SAVE_CHARACTERS    0x1000

#define DAV_STATE_UNINITAILIZED           0
#define DAV_STATE_UNKNOWN                 1
#define DAV_STATE_UNRECOGNIZED_TAG        2


typedef enum {
	NAMESPACE_DAV,
	NAMESPACE_MS,
} DAV_NAMESPACE;

class CDavParse : public SVSSAXContentHandler {
protected:
	DWORD            m_State;
	DWORD            m_PreviousState; 
	int              m_iUnknownDepth;  // Number of unknown tags on "stack"
	CWebDav          *m_pWebDav;
	SVSSimpleBuffer  m_Chars;

public:
	CDavParse(CWebDav *pWebDav) { 
		m_pWebDav = pWebDav;
		m_PreviousState = m_State   = DAV_STATE_UNINITAILIZED;
		m_iUnknownDepth = 0;
	}

	~CDavParse() { ; }

	// Verify data sent to us is as it should be
	BOOL PrepareNonEmptyBuf(void);
	BOOL VerifyNS(DAV_NAMESPACE ns, const WCHAR *szNamespace, DWORD ccNamespace);
	BOOL VerifyRequiredTag(const CHAR *szTokenExpected, DWORD ccExpected, const CHAR *szLocalName, DWORD ccLocalName);

	// SAX implementation.  Leave startElement and endElement to particular search class.
	virtual HRESULT STDMETHODCALLTYPE characters(const wchar_t  *pwchChars, int cchChars) {
		if (! (m_State & DAV_STATE_SAVE_CHARACTERS))
			return S_OK;

		if (m_Chars.Append((PSTR) pwchChars, cchChars*sizeof(WCHAR)))
			return S_OK;

		return E_FAIL;
	}

	// When we get a tag we don't know about, save old state temporarily.
	// 
	void SetUnknownTag(void) {
		if (m_State != DAV_STATE_UNRECOGNIZED_TAG) {
			DEBUGCHK(m_iUnknownDepth == 0);
			m_iUnknownDepth++;
			m_PreviousState = m_State;
			m_State         = DAV_STATE_UNRECOGNIZED_TAG;
		}
		else {
			DEBUGCHK(m_iUnknownDepth);
			m_iUnknownDepth++;
		}
	}

	void RestoreUnknownTagState(void) {
		DEBUGCHK(m_State == DAV_STATE_UNRECOGNIZED_TAG);
		m_iUnknownDepth--;

		if (m_iUnknownDepth == 0)
			m_State = m_PreviousState;
	}
};


// 
// PROPFIND
//

// PROPFIND has the form: 
// <propfind>
//  <propname/>|<allprop/>|<prop>

// <propname> returns only property names (not values) for source URL
// <allprop>  returns all property names and values for source URL
// <prop> is theoretically free-form XML specifying which properties to receive.
//   CE supports only the fixed tags such as <getcontentlength> and <getcontentlength>.

// PropFind states
#define PROPFIND_STATE_PROPFIND             10
#define PROPFIND_STATE_PROP                 11
#define PROPFIND_STATE_ALLPROP              12
#define PROPFIND_STATE_PROPNAME             13
#define PROPFIND_STATE_CONTENT_LEN          14
#define PROPFIND_STATE_DISPLAY_NAME         15
#define PROPFIND_STATE_CREATION_DATE        16
#define PROPFIND_STATE_GET_ETAG             17
#define PROPFIND_STATE_LAST_MODIFIED        18
#define PROPFIND_STATE_RESOURCE_TYPE        19
#define PROPFIND_STATE_IS_HIDDEN            20
#define PROPFIND_STATE_IS_COLLECTION        21
#define PROPFIND_STATE_SUPPORTED_LOCK       22
#define PROPFIND_STATE_LOCK_DISCOVERY       23
#define PROPFIND_STATE_CONTENT_TYPE         24

class CPropFindParse : public CDavParse {
	DWORD m_dwDavProperties; // subset of properties to send, mask of DAV_PROP_xxx flags.

public:
	CPropFindParse(CWebDav *pWebDav) : CDavParse(pWebDav) {
		m_dwDavProperties = 0;
	}

	~CPropFindParse(void) { ; }

	// implemented SAX functions.
	virtual HRESULT STDMETHODCALLTYPE startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
	                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes);

	virtual HRESULT STDMETHODCALLTYPE endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName);
};

//
// PROPPATCH
//

// PROPPATCH has the form:
// <propertyupdate>
//  <set|remove> // one or more such elements.
//   <prop> ... </prop>

// <prop> tag is theoretically free-form XML specifying properties to set,
//   though CE has a limited set of what will be accepted.

// PropPatch states
#define PROPPATCH_STATE_PROPERTYUPDATE      10
#define PROPPATCH_STATE_SET                 11
#define PROPPATCH_STATE_REMOVE              12
#define PROPPATCH_STATE_PROP                13
#define PROPPATCH_STATE_WIN32_LAST_ACCESS   14
#define PROPPATCH_STATE_WIN32_LAST_MODIFIED 15
#define PROPPATCH_STATE_WIN32_CREATION_TIME 16
#define PROPPATCH_STATE_WIN32_FILE_ATTRIBS  (DAV_STATE_SAVE_CHARACTERS+0)

class CPropPatchParse : public CDavParse {
public:
	CPropPatchParse(CWebDav *pWebDav) : CDavParse(pWebDav) { ; }

	~CPropPatchParse(void) { ; }

	// implemented SAX functions.
	virtual HRESULT STDMETHODCALLTYPE startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
	                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes);

	virtual HRESULT STDMETHODCALLTYPE endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName);
};



//
//  LOCK
//

// LOCK has the form:
// <lockinfo>
//  <lockscope>(<exclusive/>|<shared/>)</lockscope>
//  <locktype><write/></locktype>
//  <owner><href></owner>

#define LOCK_STATE_LOCKINFO               10
#define LOCK_STATE_LOCKSCOPE              11
#define LOCK_STATE_EXCLUSIVE              12
#define LOCK_STATE_SHARED                 13
#define LOCK_STATE_LOCKTYPE               14
#define LOCK_STATE_WRITE                  15
#define LOCK_STATE_OWNER                  (DAV_STATE_SAVE_CHARACTERS+0)



// CNameSpaceMap contains mappings between namespace name and the prefix.  However
// it's possible for a namespace prefix to be overriden based on scoping rules of XML
// (i.e <a xmlns:d="Fo"><b xmlns:d="Fo2">...).  So if we get multiple name spaces of 
// same name put additional mappings under m_pChild.
class CNameSpaceMap {
private:
	// make these private as this class is only initialized through small block allocator anyway...
	CNameSpaceMap() {; }
	~CNameSpaceMap() { ; }

public:
	WCHAR *m_szPrefix;
	WCHAR *m_szURI;
	CNameSpaceMap *m_pNext;  // sibling in linked list
	CNameSpaceMap *m_pChild; 

	BOOL Init(const WCHAR *szPrefix, const WCHAR *szURI) {
		m_pChild = m_pNext = NULL;
		m_szPrefix = m_szURI = NULL;

		if (szPrefix) {
			if (NULL == (m_szPrefix = MySzDupW(szPrefix)))
				return FALSE;
		}
		if (NULL == (m_szURI = MySzDupW(szURI)))
			return FALSE;

		return TRUE;
	}

	void DeInit(void) {
		MyFree(m_szPrefix);
		MyFree(m_szURI);
	}
	
};


class CLockParse : public CDavParse { 
private:
	friend class CWebDavFileLockManager;

//	DWORD m_dwLockAccess; // CreateFile for DavLock is always opened with (GENERIC_READ | GENERIC_WRITE).
	DAV_LOCK_SHARE_MODE m_lockShareMode;

	SVSXMLWriter        m_OwnerXML;
	PSTR                m_szLockOwner;

	// keep a mapping between name space prefixes and URI's for <owner> tag printout.
	
	BOOL m_fReadNameSpaceMap;     // Do we have to do NS processing?  We don't once we've handled <owner>
	CNameSpaceMap *m_NSList;      // List of name spaces.
	FixedMemDescr *m_NSDescr;     // Memory for queue

	void SetSharedLock(void)    { m_lockShareMode = DAV_LOCK_SHARED;    }
	void SetExclusiveLock(void) { m_lockShareMode = DAV_LOCK_EXCLUSIVE; }

	BOOL SetLockOwner(void) {
		MyFreeNZ(m_szLockOwner);

		if (m_OwnerXML.GetNumChars() == 0) {
			// If the owner is empty, then store an empty string.
			if (NULL == (m_szLockOwner = MySzDupA(cszEmpty)))
				return FALSE;

			return TRUE;
		}

		if (FAILED(m_OwnerXML.characters(L"\0",1)))
			return FALSE;

		if (NULL == (m_szLockOwner = m_OwnerXML.ConvertOutput(CP_ACP)))
			return FALSE;

		return TRUE;
	}

	// LOCK accessors, when parsing XML
	// SetWriteLockType does nothing for now, because we only support write locks.  
	void SetWriteLockType(void) { ;} 

public:
	CLockParse(CWebDav *pWebDav) : CDavParse(pWebDav) { 
		m_szLockOwner = NULL;
		m_lockShareMode   = DAV_LOCK_UNKNOWN;
		m_fReadNameSpaceMap     = TRUE;

		m_NSDescr = svsutil_AllocFixedMemDescr(sizeof(CNameSpaceMap),10);
		m_NSList  = NULL;
	}

	~CLockParse(void) {
		MyFreeNZ(m_szLockOwner);	

		while (m_NSList) {
			// For each item in the list
			CNameSpaceMap *pNext  = m_NSList->m_pNext;
			CNameSpaceMap *pChild = m_NSList;

			while (pChild) {
				// For each child element.
				CNameSpaceMap *pNextChild = pChild->m_pChild;

				pChild->DeInit();
				svsutil_FreeFixed(pChild,m_NSDescr);
				pChild = pNextChild;
			}

			m_NSList = pNext;
		}

		svsutil_ReleaseFixedEmpty(m_NSDescr);
		
	}

	// implemented SAX functions.
	virtual HRESULT STDMETHODCALLTYPE startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
	                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes);

	virtual HRESULT STDMETHODCALLTYPE endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName);

	virtual HRESULT STDMETHODCALLTYPE characters(const wchar_t  *pwchChars, int cchChars) {
		// special handeling for LOCK characters.  Currently only data we care
		// about lives inside <owner> tag, but because this could be XML we need to 
		// use 
		if (m_State != LOCK_STATE_OWNER)
			return S_OK;

		return m_OwnerXML.characters(pwchChars,cchChars);
	}

	virtual HRESULT STDMETHODCALLTYPE processingInstruction(const wchar_t __RPC_FAR *pwchTarget,int cchTarget,
	                                                        const wchar_t __RPC_FAR *pwchData, int cchData)
	{
		if (m_State != LOCK_STATE_OWNER)
			return S_OK;

		return m_OwnerXML.processingInstruction(pwchTarget,cchTarget,pwchData,cchData);
	}

	// Spit out XML tags into a file stream.
	BOOL WriteXMLTagToBuffer(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
	                 int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes, BOOL fIsStartTag);

	// Keep track of Prefix Mappings because we may need to spit them out again
	// for the owner tag...
	virtual HRESULT STDMETHODCALLTYPE startPrefixMapping(
	    /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
	    /* [in] */ int cchPrefix,
	    /* [in] */ const wchar_t __RPC_FAR *pwchUri,
	    /* [in] */ int cchUri);
    
	virtual HRESULT STDMETHODCALLTYPE endPrefixMapping( 
	    /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
  	    /* [in] */ int cchPrefix);
};
