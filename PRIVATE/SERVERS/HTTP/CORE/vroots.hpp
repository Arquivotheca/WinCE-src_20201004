//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: VROOTS.CPP
Abstract: Virtual roots handling mechanism.
--*/

typedef enum {
	AUTH_PUBLIC = 0,
	AUTH_USER	= 1,
	AUTH_ADMIN	= 2,
	AUTH_MAX    = 99
}
AUTHLEVEL;
	
typedef struct {
	int		iURLLen;
	PSTR	pszURL;
	int		iPathLen;
	PWSTR	wszPath;
	PSTR    pszRedirectURL;

	DWORD	dwPermissions;    // HSE_URL_xxx flags
	AUTHLEVEL	AuthLevel;    // What level of auth is required to gain access to this page?
	SCRIPT_TYPE ScriptType;   // Does vroot physical path map to an ASP or ISAPI?
	WCHAR *wszUserList;       // List of users that have access to this page.

	// Number of '/' (and '\\') chars in the vroot name, not counting initial '/' or last slash if no chars are after it.
	// So '/a' has 0, /a/b has 1, /a/b/c and /a/b/c/ both have 2.
	int   iNumSlashes;        


	unsigned int fRootDir     : 1;   // When pointing to root of filesys, special case handeling is needed.
	unsigned int fRedirect    : 1;   // TRUE if we should send a "302 redirect" response to client.
	unsigned int fDirBrowse   : 1;   // Is directory browsing allowed?
	unsigned int fNTLM        : 1;   // Will we do NTLM auth?
	unsigned int fBasic       : 1;   // Will we do BASIC auth?
	unsigned int fBasicToNTLM : 1;   // are we willing to contact the DC using client's Basic credentials?
}
VROOTINFO, *PVROOTINFO;


inline BOOL IsSlash(CHAR c) {
	return ((c == '/') || (c == '\\'));
}

inline int CountSignificantSlashesInURL(PCSTR pszURL, int iURLLen) {
	DWORD iSlashes = 0;
	// Skip first char '/', and don't check last character for ending '/'.
	for (int j = 1; j < iURLLen-1; j++) {
		if (IsSlash(pszURL[j]))
			iSlashes++;
	}

	return iSlashes;
}


// Used to split a URL and Path Translated apart for ISAPI/ASP scripts
inline void SetPathInfo(PSTR *ppszPathInfo,PSTR pszInputURL,int iURLLen) {
	int  iLen = strlen(pszInputURL+iURLLen) + 2;


	// If we've mapped the virtual root "/" to a script, need an extra "/" for the path
	// (normally we use the origial trailing "/", but in this case the "/" is the URL
	*ppszPathInfo = MySzAllocA((iURLLen == 1) ? iLen + 1 : iLen);  
	if (! (*ppszPathInfo))
		goto done;

	if (iURLLen == 1) {
		(*ppszPathInfo)[0] = '/';
		memcpy( (*ppszPathInfo) +1, pszInputURL + iURLLen, iLen);
	}
	else
		memcpy(*ppszPathInfo, pszInputURL + iURLLen, iLen);	


done:
	// URL shouldn't contain path info, break it apart
	pszInputURL[iURLLen] = 0;		
}

// We convert all '/' in URL to '\' in the filename just in case there's filesystems
// that are picky between / and \.  This won't affect most cases because even if user
// enters a '\' in URL, browser will change it to a '/' before sending it across wire.
inline void ConvertSlashes(WCHAR *szPath) {
	while (*szPath) {
		if (*szPath == '/')
			*szPath = '\\';

		szPath++;
	}
}


// Returns next occurence of either '/' or '\\'
inline PSTR GetNextSlash(PCSTR szString) {
	const CHAR cszSlashes[] = {'/', '\\', '\0'};
	return strpbrk(szString,cszSlashes);
}


class CVRoots {
	int	m_nVRoots;
	PVROOTINFO m_pVRoots;

	PVROOTINFO MatchVRoot(PCSTR pszInputURL, int iInputLen) {
		int i;

		// If there was an error on setting up the vroots, m_pVRoots = NULL.
		if (!m_pVRoots)
			return NULL;

		int iInputSlashes = CountSignificantSlashesInURL(pszInputURL,iInputLen);
		PCSTR szFirstUrlSlashInit = GetNextSlash(pszInputURL+1);
		if (NULL == szFirstUrlSlashInit)
			szFirstUrlSlashInit = strchr(pszInputURL,'\0');

		for (i = 0; i < m_nVRoots; i++) {
			int iLen = m_pVRoots[i].iURLLen;
			PCSTR szFirstUrlSlash = szFirstUrlSlashInit;
			
			// If this root maps to physical path "\", special case.
			// In general we store pszURL without trailing "/", however we have
			// to store trailing "/" for root directory.
			
			if (m_pVRoots[i].fRootDir && iLen != 1)
				iLen--;

			// URL isn't long enough to possibly match the vroot
			if (! (iLen && iInputLen >= iLen))
				continue;

			// If it's path '/', always match.
			if (iLen == 1) {
				DEBUGMSG(ZONE_VROOTS, (L"HTTPD: URL %a matched VRoot %a (path %s, perm=%d, auth=%d)\r\n", 
					pszInputURL, m_pVRoots[i].pszURL, m_pVRoots[i].wszPath, m_pVRoots[i].dwPermissions, m_pVRoots[i].AuthLevel));
				return &(m_pVRoots[i]);
			}

			// It's possible for a virtual root name to have multiple slashes, i.e.
			// '/a/b' could map to '\windows\www' whereas '/a' could be something else.
			// We need to accomodate this case, and also the case where a file name begins
			// with the same sequence of characters as a virtual root name -> i.e.
			// a request for '/webAdmin.htm' should not map to vroot '/webAdmin/', but instead to '/'.

			// First, figure out how much of URL to compare to a vroot based on number of slashes in vroot.
			int iNumSlashesInVRoot = m_pVRoots[i].iNumSlashes;

			// There's not enough slashes to possibly match, i.e. a request for /ABCD on vroot[i] = /A/B can't work.
			if (iInputSlashes < iNumSlashesInVRoot)
				continue;

			// Skip to either end of string or first '/'.  Go iNumSlashesInVRoot after
			// the first '/' if there is one, and then stop looking ahead.  This is len of URL to check against.
			while (iNumSlashesInVRoot) {
				PCSTR szFirstUrlSlashSave = szFirstUrlSlash;
				szFirstUrlSlash = GetNextSlash(szFirstUrlSlash+1);
				DEBUGCHK(szFirstUrlSlash || (iNumSlashesInVRoot == 1));

				if (NULL == szFirstUrlSlash)
					szFirstUrlSlash = strchr(szFirstUrlSlashSave,'\0');
					
				iNumSlashesInVRoot--;
			}

			int iURLLen = szFirstUrlSlash-pszInputURL;
			DEBUGCHK(iURLLen <= iInputLen);

			if ((iURLLen == m_pVRoots[i].iURLLen) && (0 == _memicmp(pszInputURL, m_pVRoots[i].pszURL, iURLLen))) {
				/*
				if (m_pVRoots[i].fRootDir) {
					// If it's not root dir, always matched.  Otherwise it's possible
					// there wasn't a match.  For root dirs, pszURL[iLen] is always "/"
					if (! (m_pVRoots[i].iURLLen == 1 || pszInputURL[iLen] == '/' || pszInputURL[iLen] == '\0'))
						continue;
				}
				*/

				DEBUGMSG(ZONE_VROOTS, (L"HTTPD: URL %a matched VRoot %a (path %s, perm=%d, auth=%d)\r\n", 
					pszInputURL, m_pVRoots[i].pszURL, m_pVRoots[i].wszPath, m_pVRoots[i].dwPermissions, m_pVRoots[i].AuthLevel));
				return &(m_pVRoots[i]);
			}
		}
		DEBUGMSG(ZONE_VROOTS, (L"HTTPD: URL %a did not matched any VRoot\r\n", pszInputURL));
		return NULL;
	}

	BOOL Init(CReg *pWebsite, BOOL fDefaultDirBrowse, BOOL fDefaultBasic, BOOL fDefaultNTLM, BOOL fDefaultBasicToNTLM) {
		const WCHAR cszDLL[] = L".dll";
		const WCHAR cszASP[] = L".asp";
		
		int err  = 0;		//  err variable is used in non-Debug mode
		BOOL fChange;
		int i=0;

		// Registry doesn't allow keynames longer than MAX_PATH so we won't map URL prefixes longer than MAX_PATH
		WCHAR wszURL[MAX_PATH+1]; 
		WCHAR wszPath[MAX_PATH+1]; 
		wszURL[0]=wszPath[0]=0;

		// open the VRoots key
		CReg topreg((HKEY) (*pWebsite), RK_HTTPDVROOTS);

		// allocate space for as many VRoots as we have subkeys
		m_nVRoots = topreg.NumSubkeys();
		if(!m_nVRoots)
			myleave(80);
		// Zero the memory so we know what to deallocate and what not to.
		if(!(m_pVRoots = MyRgAllocZ(VROOTINFO, m_nVRoots)))
			myleave(81);

		// enumerate all subkeys. Their names are URLs, their default value is the corresponding path
		// Note: EnumKey takes sizes in chars, not bytes!
		for(i = 0; topreg.EnumKey(wszURL, CCHSIZEOF(wszURL)); ) {
			CReg subreg(topreg, wszURL);
			DEBUGCHK(i < m_nVRoots);

			memset(&m_pVRoots[i],0,sizeof(VROOTINFO));
			
			// get the unnamed value. Again size is in chars, not bytes.
			if(!subreg.ValueSZ(NULL, wszPath, CCHSIZEOF(wszPath)))  {  
				// iURLLen and iPathLen set to 0 already, so no case of corruption in MatchVRoot
				subreg.Reset();
				continue;
			} 
			else {
				CHAR pszURL[MAX_PATH+1];
				// convert URL to MBCS
				int iLen = m_pVRoots[i].iURLLen = MyW2A(wszURL, pszURL, sizeof(pszURL));
				if(!iLen)
					{ myleave(83); } 

				m_pVRoots[i].iURLLen--;	// -1 for null-term
				m_pVRoots[i].iPathLen = wcslen(wszPath);

				// If a URL is named '/foo/', store it as '/foo' for requests that request /foo.
				if (m_pVRoots[i].iURLLen != 1 && IsSlash(pszURL[m_pVRoots[i].iURLLen-1])) {
					pszURL[m_pVRoots[i].iURLLen-1] = L'\0';
					m_pVRoots[i].iURLLen--;
				}

				// If the path is "$REDIRECT", web server will send "302" with location in Redirect REG_SZ.
				if (0 == wcsicmp(wszPath,REDIRECT_VALUE)) {
					WCHAR wszRedirectURL[MAX_PATH];
					m_pVRoots[i].fRedirect = TRUE;

					if (! subreg.ValueSZ(RV_REDIRECT,wszRedirectURL,CCHSIZEOF(wszRedirectURL)) ||
					    (NULL == (m_pVRoots[i].pszRedirectURL = MySzDupWtoA(wszRedirectURL)))) 
					{
						DEBUGMSG(ZONE_ERROR,(L"HTTPD: Did not initialize vroot %s, %s value must be present on redirects!\r\n",wszURL,RV_REDIRECT));
						subreg.Reset();
						continue;
					}

					if (NULL == (m_pVRoots[i].pszURL = MySzDupA(pszURL)))
						{ myleave(84); } 

					// don't bother reading any other values in this case.
					subreg.Reset();
					i++; 
					continue;
				}

				// check to see if Vroot ends in .dll or .asp, in this case we send
				// client not to the directory but to the script page.
				if 	(m_pVRoots[i].iPathLen > SVSUTIL_CONSTSTRLEN(cszDLL) && 
 					 0 == wcsicmp(wszPath + m_pVRoots[i].iPathLen - SVSUTIL_CONSTSTRLEN(cszDLL),cszDLL))
				{
 					m_pVRoots[i].ScriptType = SCRIPT_TYPE_EXTENSION;
				}
				else if (m_pVRoots[i].iPathLen > SVSUTIL_CONSTSTRLEN(cszASP) && 
 					 0 == wcsicmp(wszPath + m_pVRoots[i].iPathLen - SVSUTIL_CONSTSTRLEN(cszASP),cszASP))
				{
 					m_pVRoots[i].ScriptType = SCRIPT_TYPE_ASP;
				}
				else 
				{
 					m_pVRoots[i].ScriptType = SCRIPT_TYPE_NONE;
 				}

				// If one of URL or path ends in a slash, the other must too.
				// If either the URL ends in a "/" or when the path ends in "\", we remove
				// the extra symbol.  However, in the case where either URL or path is
				// root we don't do this.
				
				if (m_pVRoots[i].iURLLen == 1 && pszURL[0]=='/' && m_pVRoots[i].ScriptType == SCRIPT_TYPE_NONE) {
					// if it's the root URL, make sure correspinding path ends with "\"
					// (if it's a directory only, leave ASP + ISAPI's alone)
					if (wszPath[m_pVRoots[i].iPathLen-1] != L'\\') {
						wszPath[m_pVRoots[i].iPathLen] = L'\\';
						m_pVRoots[i].iPathLen++;
						wszPath[m_pVRoots[i].iPathLen] = L'\0';						
					}
				}

				// If Path ends in "\" (and it's not the root path or root virtual root)
				// remove the "\"
				if (m_pVRoots[i].iURLLen != 1 && m_pVRoots[i].iPathLen != 1 && wszPath[m_pVRoots[i].iPathLen-1]==L'\\') {
					wszPath[m_pVRoots[i].iPathLen-1] = L'\0';
					m_pVRoots[i].iPathLen--;
				}
				else if (m_pVRoots[i].iPathLen == 1 && wszPath[0]==L'\\') {	
					/*
				 	// Trailing "/" must match "\".
					if (pszURL[m_pVRoots[i].iURLLen-1] != '/') {
						pszURL[m_pVRoots[i].iURLLen] = '/';
						m_pVRoots[i].iURLLen++;
						pszURL[m_pVRoots[i].iURLLen] = '\0';
					}
					*/
					m_pVRoots[i].fRootDir = TRUE;
				}

				m_pVRoots[i].fDirBrowse   = subreg.ValueDW(RV_DIRBROWSE,fDefaultDirBrowse);
				m_pVRoots[i].fNTLM        = subreg.ValueDW(RV_NTLM,fDefaultNTLM);
				m_pVRoots[i].fBasic       = subreg.ValueDW(RV_BASIC,fDefaultBasic);
				m_pVRoots[i].fBasicToNTLM = subreg.ValueDW(RV_BASICTONTLM,fDefaultBasicToNTLM);

				m_pVRoots[i].wszPath = MySzDupW(wszPath);
				m_pVRoots[i].pszURL = MySzDupA(pszURL);

				if (! m_pVRoots[i].wszPath || ! m_pVRoots[i].pszURL) {
					MyFree(m_pVRoots[i].wszPath);
					MyFree(m_pVRoots[i].pszURL);
					myleave(85);
				}

				// OK for userlist to be NULL.
				m_pVRoots[i].wszUserList = MySzDupW( subreg.ValueSZ(RV_USERLIST));

				// default permissions is Read & Execute
				m_pVRoots[i].dwPermissions = subreg.ValueDW(RV_PERM, HTTP_DEFAULTP_PERMISSIONS);

				if ((m_pVRoots[i].dwPermissions & HSE_URL_FLAGS_REQUIRE_CERT) && !(m_pVRoots[i].dwPermissions & (HSE_URL_FLAGS_SSL | HSE_URL_FLAGS_SSL128))) {
					DEBUGMSG(ZONE_ERROR,(L"HTTPD: Client requires HSE_URL_FLAGS_REQUIRE_CERT but did not set SSL for VRoot %s, auto-setting HSE_URL_FLAGS_SSL to this vroot\r\n",wszPath));
					m_pVRoots[i].dwPermissions |= HSE_URL_FLAGS_SSL;
				}
				
				// default authentication is public
				m_pVRoots[i].AuthLevel = (AUTHLEVEL)subreg.ValueDW(RV_AUTH, (DWORD)AUTH_PUBLIC);

				// Count # of slashes now, skipping initial one.
				m_pVRoots[i].iNumSlashes = CountSignificantSlashesInURL(m_pVRoots[i].pszURL,m_pVRoots[i].iURLLen);

				DEBUGMSG(ZONE_VROOTS, (L"HTTPD: VROOT: (%a)=>(%s) perm=%d auth=%d ScriptType=%d\r\n", m_pVRoots[i].pszURL, 
					m_pVRoots[i].wszPath, m_pVRoots[i].dwPermissions, m_pVRoots[i].AuthLevel,m_pVRoots[i].ScriptType));
			}

			subreg.Reset();
			i++;
		}
		// We now want to sort the VRoots in descending order of URL-length so 
		// that when we match we'll find the longest match first!!
		do{
			fChange = FALSE;
			for(i=0; i<m_nVRoots-1; i++) {
				if(m_pVRoots[i].iURLLen < m_pVRoots[i+1].iURLLen) {
					// swap the 2 vroots
					VROOTINFO vtemp = m_pVRoots[i+1];
					m_pVRoots[i+1] = m_pVRoots[i];
					m_pVRoots[i] = vtemp;
					fChange = TRUE;
				}
			}
		} while(fChange);

	done:
		if(err) {
			DEBUGMSG(ZONE_ERROR, (L"HTTPD: CVRoots::ctor FAILED due to err=%d GLE=%d (num=%d i=%d pVRoots=0x%08x url=%s path=%s)\r\n", 
				err, GetLastError(), m_nVRoots, i, m_pVRoots, wszURL, wszPath));
			return FALSE;
		}
		return TRUE;
	}

	void Cleanup() {
		if(!m_pVRoots)
			return;
		for(int i=0; i<m_nVRoots; i++) {
			MyFree(m_pVRoots[i].pszURL);
			MyFree(m_pVRoots[i].wszPath);
			MyFree(m_pVRoots[i].wszUserList);
			MyFree(m_pVRoots[i].pszRedirectURL);
		}
		MyFree(m_pVRoots);
	}

public:
	CVRoots(CReg *pWebsite, BOOL fDefaultDirBrowse, BOOL fDefaultBasic, BOOL fDefaultNTLM, BOOL fDefaultBasicToNTLM)  { ZEROMEM(this); Init(pWebsite, fDefaultDirBrowse, fDefaultBasic, fDefaultNTLM, fDefaultBasicToNTLM); }
	~CVRoots() { Cleanup(); }
	DWORD      Count()  { return m_nVRoots; }

	
	PWSTR URLAtoPathW(PSTR pszInputURL, PVROOTINFO *ppVRootInfo=NULL, PSTR *ppszPathInfo=0, BOOL fAcceptRedirect=FALSE)  {
		WCHAR *wszTemp = NULL;
		int iInputLen = strlen(pszInputURL);
		PVROOTINFO pVRoot = MatchVRoot(pszInputURL, iInputLen);
		if(!pVRoot)
			return NULL;

		if (ppVRootInfo)
			*ppVRootInfo = pVRoot;

		if (pVRoot->fRedirect) {
			// There are cases where if we see a redirect URL we want to act like it 
			// doesn't exist (i.e. during Script Mapping).
			if (!fAcceptRedirect)
				return FALSE;

			DEBUGCHK(ppVRootInfo);
			return NULL; // calling function ignores return value, looks at *ppVRootInfo for where to redirect to.
		}
		
		int iOutLen = 2 + pVRoot->iPathLen + (iInputLen - pVRoot->iURLLen);
		PWSTR wszOutPath = MyRgAllocNZ(WCHAR, iOutLen+1);
		if (!wszOutPath)
			return NULL;
			
		// assemble the path. First, the mapped base path
		if (pVRoot->iPathLen != 1)
			memcpy(wszOutPath, pVRoot->wszPath, sizeof(WCHAR)*pVRoot->iPathLen);
	
		// If the vroot specifies an ISAPI dll or ASP page don't copy path info over.
		if (pVRoot->ScriptType != SCRIPT_TYPE_NONE) {		
			if ( ppszPathInfo && pszInputURL[pVRoot->iURLLen] != 0) {
				SetPathInfo(ppszPathInfo,pszInputURL,pVRoot->iURLLen);
			}
			wszOutPath[pVRoot->iPathLen] = L'\0';
			ConvertSlashes(wszOutPath);
			return wszOutPath;
		}

		// next the remainder of the URL, converted to wide
		if (iOutLen-pVRoot->iPathLen == 1) {
			wszOutPath[pVRoot->iPathLen] = L'\0';
		}
		else {
			int iOffset;
			if (pVRoot->iPathLen == 1 && iOutLen==3) {
				// This is special case where virtual root mapped to '\' receives request to '/'.
				wszOutPath[0] = L'\\';
				wszOutPath[1] = 0;
				return wszOutPath;
			}
			else if (pVRoot->iPathLen == 1 && pVRoot->iURLLen == 1) { 
				// This is special case where virtual root '/' maps to '\', need to include the beginning '/' in URL
				wszOutPath[0] = L'\\';
				iOffset = 1;
			}
			else if (pVRoot->iPathLen == 1) {
				// virtual root maps to '/' but we're requesting a file.
				// We have to do this extra special case handeling because when a vroot
				// maps to a directory the directory doesn't store trailing '\', but in this case
				// where filename is only '\' we do have it.
				iOffset = 0;
			}
			else {
				iOffset = pVRoot->iPathLen;
			}
			int iRet = MyA2W(pszInputURL+pVRoot->iURLLen, wszOutPath+iOffset, iOutLen-iOffset-1);
			DEBUGCHK(iRet);
		}
		DEBUGCHK((int)(wcslen(wszOutPath) + 1) <= iOutLen);
		ConvertSlashes(wszOutPath);
		return wszOutPath;
	}
};
