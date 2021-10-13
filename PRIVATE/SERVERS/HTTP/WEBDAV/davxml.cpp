//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davmain.cpp
Abstract: WebDAV main verb handeling routines
--*/


#include "httpd.h"

// *****************************************************************
// Utility functions for parsing XML
// *****************************************************************
BOOL UTF8ToBSTR(PSTR psz, BSTR *ppBstr) {
	DWORD cp = CP_UTF8;

	int iLen = MultiByteToWideChar(cp, 0, psz, -1, NULL, NULL);
	if (0 == iLen) {
		cp = CP_ACP;
		iLen = MultiByteToWideChar(cp, 0, psz, -1, NULL, NULL);

		if (0 == iLen) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to convert UTF8 text to UNICODE.  GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}
	}

	if (NULL == ((*ppBstr) = SysAllocStringByteLen(NULL,(iLen+1)*sizeof(WCHAR)))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: SysAllocStringByteLen fails.  GLE=0x%08x\r\n",GetLastError()));
		return FALSE;
	}

	MultiByteToWideChar(cp, 0, psz, -1, *ppBstr, iLen);
	return TRUE;
}


// Creates a SAX parser and passes it pParser, which implements logic for
// particular verb.
BOOL CWebDav::ParseXMLBody(ISAXContentHandler *pParser, BOOL fSetDefaultStatus) {
	BOOL    fCoUnInit        = FALSE;
	HRESULT hr               = E_FAIL;
	RESPONSESTATUS rs        = STATUS_INTERNALERR;
	ISAXXMLReader* pReader   = NULL;
	VARIANT varInput;

	memset(&varInput,0,sizeof(varInput));

	DEBUGCHK(m_pRequest->m_rs == STATUS_OK);
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav initiating SAX parser to parse XML body\r\n"));

	if (STATUS_OK != (rs = VerifyXMLBody()))
		goto done;

	rs = STATUS_INTERNALERR;

	if (FAILED(hr = CoInitializeEx(NULL,COINIT_MULTITHREADED))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CoInitializeEx failed, GLE=0x%08x\r\n",GetLastError()));
		goto done;
	}
	fCoUnInit = TRUE;

	if (FAILED(hr = CoCreateInstance(CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER, IID_ISAXXMLReader, (void**)&pReader))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CoCreateInstance(CLSID_SAXXMLReader) failed, error=0x%08x\r\n",hr));
		goto done;
	}

	if (FAILED(hr = pReader->putContentHandler(pParser))) {
		DEBUGMSG(ZONE_ERROR, (L"HTTPD: ISAXXMLReader::putContentHandler failed, error=0x%08x\r\n",hr));
		goto done;
	}

	varInput.vt          = VT_BSTR;
	if (!UTF8ToBSTR((PSTR)m_pRequest->m_bufRequest.Data(),&varInput.bstrVal))
		goto done;

	if (FAILED(hr = pReader->parse(varInput))) {
		// If the parse routines didn't set the error otherwise, assume it was invalid XML that caused problem.
		if (m_pRequest->m_rs == STATUS_OK)
			rs = STATUS_BADREQ;

		DEBUGMSG (ZONE_ERROR, (L"HTTPD: ISAXXMLReader::parse().  Error = 0x%08x\r\n",hr));
		goto done;
	}

	hr = S_OK;
done:
	if (varInput.bstrVal)
		SysFreeString(varInput.bstrVal);

	if (pReader)
		pReader->Release();

	if (fCoUnInit)
		CoUninitialize();

	if (fSetDefaultStatus) {
		// For PROPFIND, the ISAXContentHandler implementation does all the logic processing.
		// If it did not set a status, means that the XML tags it required to see
		// never were parsed, so set appropriate errors now as appropriate.
		if ((hr==S_OK) && !m_fSetStatus) {
			rs = STATUS_BADREQ;
		}
		DEBUGCHK(rs != STATUS_OK);

		if (m_pRequest->m_rs == STATUS_OK) {
			SetStatus(rs);
			return FALSE;
		}
	}
	else if ((hr != S_OK) && !m_fSetStatus) {
		// On PROPPATCH and LOCK, routine outside of ISAXContentHandler,
		// handles most of the logic, so don't set default errors.
		SetStatus(rs);
	}
	
	return (hr == S_OK);
}

// 
BOOL CLockParse::WriteXMLTagToBuffer(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes, BOOL fIsStartTag) {

	if (fIsStartTag) {
		if (FAILED(m_OwnerXML.startElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName,pAttributes)))
			return FALSE;
	}
	else {
		if (FAILED(m_OwnerXML.endElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName)))
			return FALSE;
	}


	return TRUE;
}


#if defined (DEBUG)
inline void DebugOutUnexpectedToken(const CHAR *szTokenExpected, const CHAR *szLocalName, int ccLocalName) {
	DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error  parsing xml, expecting token \"%a:\", got %*a\r\n",szTokenExpected,ccLocalName,szLocalName));
}
inline void DebugOutInvalidChild(const CHAR *szCurrentNode) {
	DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error  parsing xml, node \"%a:\" has a child node, illegal\r\n",szCurrentNode));
}
inline void DebugOutUnknown(void) {
	DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error, xml parser in unknown state due to unhandled tags.  Reqeust fails.\r\n"));
}

#else
inline void DebugOutUnexpectedToken(const CHAR *szTokenExpected, const CHAR *szLocalName, int ccLocalName) { ; }
inline void DebugOutInvalidChild(const CHAR *szCurrentNode) { ; }
inline void DebugOutUnknown(void) { ; }
#endif


const WCHAR cwszDavNameSpace[]           = L"DAV:";
const DWORD ccDavNameSpace               = SVSUTIL_CONSTSTRLEN(cwszDavNameSpace);

BOOL CDavParse::VerifyNS(DAV_NAMESPACE ns, const WCHAR *szNamespace, DWORD ccNamespace) {
	switch (ns) {
	case NAMESPACE_DAV:
		if ( ! TokensEqualW(szNamespace,cwszDavNameSpace,ccNamespace)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error parsing DAV xml, expecting NS \"%s:\", got %*s\r\n",cwszDavNameSpace,ccNamespace,szNamespace));
			return FALSE;
		}
		break;

/*	case NAMESPACE_MS:
		if ( ! TokensEqualW(szNamespace,cwszMSNamespace,ccNamespace)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error parsing DAV xml, expecting NS \"%s:\", got %*s\r\n",cwszMSNamespace,ccNamespace,szNamespace));
			return FALSE;
		}
		break;
*/
	default:
		SVSUTIL_ASSERT(0);
		return FALSE;
	}
	return TRUE;
}

#define VerifyRequiredTagC(szTokenExpected,szLocalName,ccLocalName) VerifyRequiredTag(szTokenExpected,SVSUTIL_CONSTSTRLEN(szTokenExpected),szLocalName,ccLocalName)

BOOL CDavParse::PrepareNonEmptyBuf(void) {
	if (m_Chars.uiNextIn == 0) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV request fails, an element that requires data did not set it\r\n"));
		return FALSE;
	}

	if (! m_Chars.AppendWCHAR(L'\0')) {
		return FALSE;
	}

	return TRUE;
}

// called when a token must match szTokenExpected.
BOOL CDavParse::VerifyRequiredTag(const CHAR *szTokenExpected, DWORD ccExpected, const CHAR *szLocalName, DWORD ccLocalName) {
	if (! TokensEqualA(szTokenExpected,szLocalName,ccExpected,ccLocalName)) {
		DebugOutUnexpectedToken(szTokenExpected,szLocalName,ccLocalName);
		return FALSE;
	}
	return TRUE;
}


// Checks that content-length !=0, that the content-type is xml, and that we're not
// receiving more than m_dwPostReadSize bytes.
RESPONSESTATUS CWebDav::VerifyXMLBody(void) {
	if (! IsContentTypeXML()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav request fails, client did not set content-type to XML\r\n"));
		return STATUS_BADREQ;
	}

	if (! HasBody()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav request fails, client did not send body (required)\r\n"));
		return STATUS_LENGTH_REQUIRED;
	}

	if (! HasServerReadAllData()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav request fails, client sent %d body bytes, content-length = %d\r\n",m_pRequest->m_bufRequest.Count(),m_pRequest->m_dwContentLength));
		return STATUS_REQUEST_TOO_LARGE;
	}
	return STATUS_OK;
}


// *****************************************************************
// Implementations of classes that parse XML.
// *****************************************************************

//
// PROPFIND
//  
const CHAR  cszPropfind[]   = "propfind";
const DWORD ccPropfind      = SVSUTIL_CONSTSTRLEN(cszPropfind);
const CHAR  cszPropName[]   = "propname";
const DWORD ccPropName      = SVSUTIL_CONSTSTRLEN(cszPropName);
const CHAR  cszAllProp[]    = "allprop";
const DWORD ccAllProp       = SVSUTIL_CONSTSTRLEN(cszAllProp);


#define LOCAL_NAME_SIZE     256

inline BOOL ConvertXMLTagToANSI(const wchar_t * pwchLocalName, int cchLocalName, PSTR szLocalName) {
	int nChars;
	UINT uiCodePage = GetUTF8OrACP();

	if (0 == (nChars = WideCharToMultiByte(uiCodePage, 0, pwchLocalName, cchLocalName, szLocalName, LOCAL_NAME_SIZE-1, 0, 0))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: DAV request failed, WideCharToMultiByte(%s,len=%d) failed, GLE=0x%08x\r\n",pwchLocalName,cchLocalName));
		return FALSE;
	}
	szLocalName[nChars] = 0;

	return TRUE;
}


HRESULT STDMETHODCALLTYPE CPropFindParse::startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
                          int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes) {
	m_Chars.Reset();

	CHAR szLocalName[LOCAL_NAME_SIZE];
	if (! ConvertXMLTagToANSI(pwchLocalName,cchLocalName,szLocalName))
		return E_FAIL;

	switch (m_State) {
		case DAV_STATE_UNINITAILIZED:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri) ||
			    ! VerifyRequiredTagC(cszPropfind,szLocalName,cchLocalName))
				return E_FAIL;

			m_State = PROPFIND_STATE_PROPFIND;
		break;

		case PROPFIND_STATE_ALLPROP:
		case PROPFIND_STATE_PROPNAME:
		case DAV_STATE_UNKNOWN:
			DebugOutUnknown();
			return E_FAIL;
		break;

		case PROPFIND_STATE_PROPFIND:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri))
				return E_FAIL;

			if (TokensEqualAC(szLocalName,AllProp,cchLocalName))
				m_State = PROPFIND_STATE_ALLPROP;
			else if (TokensEqualAC(szLocalName,PropName,cchLocalName))
				m_State = PROPFIND_STATE_PROPNAME;
			else if (TokensEqualAC(szLocalName,Prop,cchLocalName))
				m_State = PROPFIND_STATE_PROP;
			else {
				DebugOutUnexpectedToken("prop, propname, or anyprop",szLocalName,cchLocalName);
				return E_FAIL;
			}
		break;

		case PROPFIND_STATE_PROP:
		//	if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri))
		//		return E_FAIL;

			if (TokensEqualAC(szLocalName,GetcontentLength,cchLocalName))
				m_State = PROPFIND_STATE_CONTENT_LEN;
			else if (TokensEqualAC(szLocalName,DisplayName,cchLocalName))
				m_State = PROPFIND_STATE_DISPLAY_NAME;
			else if (TokensEqualAC(szLocalName,CreationDate,cchLocalName))
				m_State = PROPFIND_STATE_CREATION_DATE;
			else if (TokensEqualAC(szLocalName,GetETag,cchLocalName))
				m_State = PROPFIND_STATE_GET_ETAG;
			else if (TokensEqualAC(szLocalName,LastModified,cchLocalName))
				m_State = PROPFIND_STATE_LAST_MODIFIED;
			else if (TokensEqualAC(szLocalName,ResourceType,cchLocalName))
				m_State = PROPFIND_STATE_RESOURCE_TYPE;
			else if (TokensEqualAC(szLocalName,IsHidden,cchLocalName))
				m_State = PROPFIND_STATE_IS_HIDDEN;
			else if (TokensEqualAC(szLocalName,IsCollection,cchLocalName))
				m_State = PROPFIND_STATE_IS_COLLECTION;
			else if (TokensEqualAC(szLocalName,SupportedLock,cchLocalName))
				m_State = PROPFIND_STATE_SUPPORTED_LOCK;
			else if (TokensEqualAC(szLocalName,LockDiscovery,cchLocalName))
				m_State = PROPFIND_STATE_LOCK_DISCOVERY;
			else if (TokensEqualAC(szLocalName,GetcontentType,cchLocalName))
				m_State = PROPFIND_STATE_CONTENT_TYPE;
			else {
				DebugOutUnexpectedToken("recognized prop tags",szLocalName,cchLocalName);

				if ((m_iUnknownDepth == 0) && ! m_pWebDav->AddUnknownXMLElement(pwchQName,cchQName))
					return E_FAIL;

				SetUnknownTag();
			}
		break;

		default:
			DebugOutUnexpectedToken("???",szLocalName,cchLocalName);
			//SetUnknownTag();
			return E_FAIL;
		break;
	}

	return S_OK;

}


HRESULT STDMETHODCALLTYPE CPropFindParse::endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                 int cchLocalName, const wchar_t  *pwchQName, int cchQName) {

	switch (m_State) {
		case PROPFIND_STATE_PROPFIND:
			m_State  = DAV_STATE_UNKNOWN;
		break;

		case PROPFIND_STATE_ALLPROP:
			m_State = PROPFIND_STATE_PROPFIND;
			return m_pWebDav->SendProperties(DAV_PROP_ALL,FALSE);
		break;

		case PROPFIND_STATE_PROPNAME:
			m_State = PROPFIND_STATE_PROPFIND;
			return m_pWebDav->SendProperties(DAV_PROP_ALL,TRUE);
		break;

		case PROPFIND_STATE_PROP:
			m_State = PROPFIND_STATE_PROPFIND;
			return m_pWebDav->SendProperties(m_dwDavProperties,FALSE);
		break;

		case PROPFIND_STATE_CONTENT_LEN:
			m_dwDavProperties  |= DAV_PROP_CONTENT_LENGTH; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_DISPLAY_NAME:
			m_dwDavProperties  |= DAV_PROP_DISPLAY_NAME; 
			m_State            = PROPFIND_STATE_PROP;
		break;
		
		case PROPFIND_STATE_CREATION_DATE:
			m_dwDavProperties  |= DAV_PROP_CREATION_DATE;
			m_State            = PROPFIND_STATE_PROP;
		break;
		
		case PROPFIND_STATE_GET_ETAG:
			m_dwDavProperties  |= DAV_PROP_GET_ETAG; 
			m_State            = PROPFIND_STATE_PROP;
		break;
		
		case PROPFIND_STATE_LAST_MODIFIED:
			m_dwDavProperties  |= DAV_PROP_GET_LAST_MODIFIED; 
			m_State            = PROPFIND_STATE_PROP;
		break;
		
		case PROPFIND_STATE_RESOURCE_TYPE:
			m_dwDavProperties  |= DAV_PROP_RESOURCE_TYPE; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_IS_HIDDEN:
			m_dwDavProperties  |= DAV_PROP_IS_HIDDEN; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_IS_COLLECTION:
			m_dwDavProperties  |= DAV_PROP_IS_COLLECTION; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_SUPPORTED_LOCK:
			m_dwDavProperties  |= DAV_PROP_SUPPORTED_LOCK; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_LOCK_DISCOVERY:
			m_dwDavProperties  |= DAV_PROP_LOCK_DISCOVERY; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case PROPFIND_STATE_CONTENT_TYPE:
			m_dwDavProperties  |= DAV_PROP_GET_CONTENT_TYPE; 
			m_State            = PROPFIND_STATE_PROP;
		break;

		case DAV_STATE_UNRECOGNIZED_TAG:
			RestoreUnknownTagState();
		break;

		default:
			DEBUGCHK(0);
			return E_FAIL;
		break;
	}

	return S_OK;
}



//
// PROPPATCH
//

const CHAR  cszPropertyUpdate[]         = "propertyupdate";
const DWORD ccPropertyUpdate            = SVSUTIL_CONSTSTRLEN(cszPropertyUpdate);
const CHAR  cszSet[]                    = "set";
const DWORD ccSet                       = SVSUTIL_CONSTSTRLEN(cszSet);
// const WCHAR cszRemove[]              = "remove";



HRESULT STDMETHODCALLTYPE CPropPatchParse::startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes) {

	m_Chars.Reset();
	CHAR szLocalName[LOCAL_NAME_SIZE];
	if (! ConvertXMLTagToANSI(pwchLocalName,cchLocalName,szLocalName))
		return E_FAIL;


	switch (m_State) {
		case DAV_STATE_UNKNOWN:
			DebugOutUnknown();
			return E_FAIL;
		break;
		
		case DAV_STATE_UNINITAILIZED:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri) ||
			    ! VerifyRequiredTag(cszPropertyUpdate,ccPropertyUpdate,szLocalName,cchLocalName))
				return E_FAIL;

			m_State = PROPPATCH_STATE_PROPERTYUPDATE;
		break;

		case PROPPATCH_STATE_PROPERTYUPDATE:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri))
				return E_FAIL;

			if (TokensEqualAC(szLocalName,Set,cchLocalName))
				m_State = PROPPATCH_STATE_SET;
//			else if (TokensEqualAC(szLocalName,Remove,cchLocalName)) // remove currently unsupported
//				m_State = PROPPATCH_STATE_REMOVE;
			else {
				DebugOutUnexpectedToken("set or remove",szLocalName,cchLocalName);
				return E_FAIL;
			}
		break;

		case PROPPATCH_STATE_SET:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri) ||
			    ! VerifyRequiredTag(cszProp,ccProp,szLocalName,cchLocalName))
				return E_FAIL;

			m_State = PROPPATCH_STATE_PROP;
		break;

		case PROPPATCH_STATE_PROP:
			if (TokensEqualAC(szLocalName,Win32FileAttribs,cchLocalName)) {
				m_pWebDav->m_dwPropPatchUpdate |= DAV_PROP_W32_FILE_ATTRIBUTES;
				m_State = PROPPATCH_STATE_WIN32_FILE_ATTRIBS;
			}
			else if (TokensEqualAC(szLocalName,Win32LastModifiedTime,cchLocalName)) {
				m_pWebDav->m_dwPropPatchUpdate |= DAV_PROP_W32_LAST_MODIFY_TIME;
				m_State = PROPPATCH_STATE_WIN32_LAST_MODIFIED;
			}
			else if (TokensEqualAC(szLocalName,Win32CreationTime,cchLocalName)) {
				m_pWebDav->m_dwPropPatchUpdate |= DAV_PROP_W32_CREATION_TIME;
				m_State = PROPPATCH_STATE_WIN32_CREATION_TIME;
			}
			else if (TokensEqualAC(szLocalName,Win32LastAccessTime,cchLocalName)) {
				m_pWebDav->m_dwPropPatchUpdate |= DAV_PROP_W32_LAST_ACCESS_TIME;
				m_State = PROPPATCH_STATE_WIN32_LAST_ACCESS;
			}
			else {
				DebugOutUnexpectedToken("Win32(FileAttributes, LastAccessTime, LastModifiedTime, CreationTime",szLocalName,cchLocalName);

				if ((m_iUnknownDepth == 0) && !m_pWebDav->AddUnknownXMLElement(pwchQName,cchQName))
					return E_FAIL;

				SetUnknownTag();
			}
			break;

		case PROPPATCH_STATE_WIN32_FILE_ATTRIBS:
			DebugOutInvalidChild(cszWin32FileAttribs);
			return E_FAIL;
		break;

		default:
			DebugOutUnexpectedToken("???",szLocalName,cchLocalName);
		//	SetUnknownTag();
			return E_FAIL;
		break;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CPropPatchParse::endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName) {

	switch (m_State) {
		case PROPPATCH_STATE_PROPERTYUPDATE:
			m_State  = DAV_STATE_UNKNOWN;
		break;

		case PROPPATCH_STATE_SET:
			m_State = PROPPATCH_STATE_PROPERTYUPDATE;
		break;

		case PROPPATCH_STATE_PROP:
			m_State = PROPPATCH_STATE_SET;
		break;

		case PROPPATCH_STATE_WIN32_LAST_MODIFIED:
		case PROPPATCH_STATE_WIN32_CREATION_TIME:
		case PROPPATCH_STATE_WIN32_LAST_ACCESS:
			m_State = PROPPATCH_STATE_PROP;
		break;

		case PROPPATCH_STATE_WIN32_FILE_ATTRIBS:
			if (! PrepareNonEmptyBuf())
				return E_FAIL;

			m_State  = PROPPATCH_STATE_PROP;
			return m_pWebDav->ProcessWin32FileAttribs(&m_Chars);
		break;

		case DAV_STATE_UNRECOGNIZED_TAG:
			RestoreUnknownTagState();
		break;

		default:
			DEBUGCHK(0);
			return E_FAIL;
		break;
	}

	return S_OK;
}

// Win32FileAttributes contains a 32 bit hex # (with no preceeding 0x)
#define WIN32_DATA_LEN             (8*sizeof(WCHAR))


HRESULT CWebDav::ProcessWin32FileAttribs(SVSSimpleBuffer *pBuf) {
	DWORD  dwFileAttrib;
	HRESULT hr = E_FAIL;

	if (pBuf->uiNextIn != (WIN32_DATA_LEN+sizeof(WCHAR))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Dav request to set <Win32FileAttributes> failed, length=%d, should be %d\r\n",pBuf->uiNextIn,WIN32_DATA_LEN));
		return E_FAIL;
	}

	if (1 != swscanf((WCHAR*)pBuf->pBuffer,L"%x",&dwFileAttrib)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Dav request to set <Win32FileAttributes> failed, cannot parse hex value\r\n"));
		return E_FAIL;
	}

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls SetFileAttributes(%s,%d)\r\n",m_pRequest->m_wszPath,dwFileAttrib));
	if (! SetFileAttributes(m_pRequest->m_wszPath,dwFileAttrib)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: SetFileAttributes(%s,0x%08x) failed,GLE=0x%08x\r\n",m_pRequest->m_wszPath,dwFileAttrib,GetLastError()));
		SetStatus(STATUS_CONFLICT);
		return E_FAIL;
	}

	return S_OK;
}

//
//  LOCK
//

const char  cszLockInfoTag[] = "lockinfo";
const DWORD ccLockInfoTag    = SVSUTIL_CONSTSTRLEN(cszLockInfoTag);

HRESULT STDMETHODCALLTYPE CLockParse::startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes)
{
	//if (m_iUnknownDepth == 0)
	//	m_Chars.Reset();

	CHAR szLocalName[LOCAL_NAME_SIZE];
	if (! ConvertXMLTagToANSI(pwchLocalName,cchLocalName,szLocalName))
		return E_FAIL;

	switch (m_State) {
		case DAV_STATE_UNKNOWN:
			DebugOutUnknown();
			return E_FAIL;
		break;
		
		case DAV_STATE_UNINITAILIZED:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri) ||
			    ! VerifyRequiredTag(cszLockInfoTag,ccLockInfoTag,szLocalName,cchLocalName))
				return E_FAIL;

			m_State = LOCK_STATE_LOCKINFO;
		break;

		case LOCK_STATE_LOCKINFO:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri))
				return E_FAIL;

			if (TokensEqualAC(szLocalName,LockScope,cchLocalName))
				m_State = LOCK_STATE_LOCKSCOPE;
			else if (TokensEqualAC(szLocalName,LockType,cchLocalName))
				m_State = LOCK_STATE_LOCKTYPE;
			else if (TokensEqualAC(szLocalName,Owner,cchLocalName)) {
				DEBUGCHK(m_iUnknownDepth == 0);
				m_iUnknownDepth = 1;
				m_State = LOCK_STATE_OWNER;
			}
			else {
				DebugOutUnexpectedToken("lockscope, locktype, or owner",szLocalName,cchLocalName);
				return E_FAIL;
			}
		break;

		case LOCK_STATE_LOCKSCOPE:
			if (TokensEqualAC(szLocalName,Exclusive,cchLocalName))
				m_State = LOCK_STATE_EXCLUSIVE;
			else if (TokensEqualAC(szLocalName,Shared,cchLocalName))
				m_State = LOCK_STATE_SHARED;
			else {
				DebugOutUnexpectedToken("shared or exclusive",szLocalName,cchLocalName);
				return E_FAIL;
			}
		break;
	
		case LOCK_STATE_LOCKTYPE:
			if (! VerifyNS(NAMESPACE_DAV,pwchNamespaceUri,cchNamespaceUri) ||
			    ! VerifyRequiredTag(cszWrite,ccWrite,szLocalName,cchLocalName))
				return E_FAIL;

			m_State = LOCK_STATE_WRITE;
		break;

		case LOCK_STATE_OWNER:
			// Once we're in Owner state, record everything.
			if ( ! WriteXMLTagToBuffer(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, 
			         cchLocalName, pwchQName, cchQName, pAttributes,TRUE))
				return E_FAIL;


			// After this point, any new name space mappings in <owner> will be added automatically
			// because they are attributes of the nodes we run across.
			m_fReadNameSpaceMap = FALSE;
			m_iUnknownDepth++;
			;
		break;

		default:
			DebugOutUnexpectedToken("???",szLocalName,cchLocalName);
			// SetUnknownTag();
			return E_FAIL;
		break;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CLockParse::endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName)
{
	switch (m_State) {
		case LOCK_STATE_LOCKINFO:
			m_State = DAV_STATE_UNKNOWN;
		break;

		case LOCK_STATE_LOCKSCOPE:
			m_State = LOCK_STATE_LOCKINFO;
		break;
		
		case LOCK_STATE_LOCKTYPE:
			m_State = LOCK_STATE_LOCKINFO;
		break;

		case LOCK_STATE_OWNER:
			m_iUnknownDepth--;
			DEBUGCHK(m_iUnknownDepth >= 0);

			if (m_iUnknownDepth != 0) {
				if ( ! WriteXMLTagToBuffer(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, 
				         cchLocalName, pwchQName, cchQName, NULL,FALSE))
					return E_FAIL;
			}
			else {
			//	if (! PrepareNonEmptyBuf())
			//		return E_FAIL;

				m_State = LOCK_STATE_LOCKINFO;
				return (SetLockOwner() ? S_OK : E_FAIL);
			}
		break;

		case LOCK_STATE_WRITE:
			SetWriteLockType();
			m_State = LOCK_STATE_LOCKTYPE;
		break;

		case LOCK_STATE_SHARED:
			SetSharedLock();
			m_State = LOCK_STATE_LOCKSCOPE;
		break;

		case LOCK_STATE_EXCLUSIVE:
			SetExclusiveLock();
			m_State = LOCK_STATE_LOCKSCOPE;
		break;

		case DAV_STATE_UNRECOGNIZED_TAG:
			RestoreUnknownTagState();
		break;

		default:
			DEBUGCHK(0);
			return E_FAIL;
		break;
	}

	return S_OK;
}




HRESULT STDMETHODCALLTYPE CLockParse::startPrefixMapping(const wchar_t __RPC_FAR *pwchPrefix, int cchPrefix,
                                                         const wchar_t __RPC_FAR *pwchUri, int cchUri)  
{
	if (!m_fReadNameSpaceMap)
		return S_OK;

	CNameSpaceMap *pTrav = m_NSList;

	CNameSpaceMap *pNew = (CNameSpaceMap *)svsutil_GetFixed(m_NSDescr);
	if (!pNew)
		return E_FAIL;

	while (pTrav) {
		if (0 == wcsncmp(pwchPrefix,pTrav->m_szPrefix,cchPrefix)) {
			// We've found the root node, now traverse down its children and add it to last spot.
			while (pTrav->m_pChild)
				pTrav = pTrav->m_pChild;
		
			if (! pNew->Init(NULL,pwchUri)) {
				svsutil_FreeFixed(pNew,m_NSDescr);
				return E_FAIL;
			}
			pTrav->m_pChild = pNew;
			return S_OK;
		}
		pTrav = pTrav->m_pNext;
	}

	// We're adding completly new Namespace prefix to the list.
	if (! pNew->Init(pwchPrefix,pwchUri)) {
		svsutil_FreeFixed(pNew,m_NSDescr);
		return E_FAIL;
	}

	pNew->m_pNext = m_NSList;
	m_NSList = pNew;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CLockParse::endPrefixMapping(const wchar_t __RPC_FAR *pwchPrefix, int cchPrefix)
{
	if (!m_fReadNameSpaceMap)
		return S_OK;

	CNameSpaceMap *pTrav = m_NSList;
	CNameSpaceMap *pFollow = NULL;

	while (pTrav) {
		if (0 == wcsncmp(pwchPrefix,pTrav->m_szPrefix,cchPrefix)) {
			if (pTrav->m_pChild) {
				// Case where there are multiple URI's with same NS.
				// Remove the last child in this case, but leave other elements alone.

				while (pTrav->m_pChild) {
					pFollow = pTrav;
					pTrav = pTrav->m_pChild;
				}
				pFollow->m_pChild = NULL;
			}
			else {
				// Case where this is the only NS entry, in which case remove it from the list.
				if (pFollow) {
					pFollow->m_pNext = pTrav;
				}
				else {
					DEBUGCHK(pTrav == m_NSList);
					m_NSList = pTrav->m_pNext;
				}
			}

			pTrav->DeInit();
			svsutil_FreeFixed(pTrav,m_NSDescr);
			return S_OK;
		}
		pTrav = pTrav->m_pNext;
	}

	DEBUGCHK(0); // should always find element in the list.
	return S_OK;
}

