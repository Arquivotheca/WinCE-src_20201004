//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "auth.h"
#include "proxydbg.h"

#define SEC_SUCCESS(Status) ((Status) >= 0)

BOOL NTLMServerContext(PSEC_WINNT_AUTH_IDENTITY pAuthIdentity, PAUTH_NTLM pAS, BYTE *pIn, DWORD cbIn, BYTE *pOut, DWORD *pcbOut, BOOL *pfDone);
BOOL Base64Decode(char* bufcoded, char* pbuffdecoded, DWORD* pcbDecoded);
BOOL Base64Encode(BYTE* bufin, DWORD nbytes, char* pbuffEncoded);
DWORD GetUserInfo(PAUTH_NTLM pAuth, char* szUser, int cbUser);


typedef struct _SecurityParams {
	DWORD dwRefCount;
	DWORD dwMaxNTLMBuf;
	HINSTANCE hLib;
	SecurityFunctionTable funcs;
} SecurityParams;

SecurityParams params;



DWORD HandleNTLMAuth(const char* szNTLMAuth, char* szNTLMAuthOut, int cbNTLMAuthOut, char* szUser, int cbUser, PAUTH_NTLM pAuth)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	DWORD dwIn;
	DWORD dwOut;
	BOOL fDone = FALSE;
	PBYTE pOutBuf = NULL;
	PBYTE pInBuf = NULL;   // Base64 decoded data from pszNTLMData

	ASSERT(pAuth);
	ASSERT(params.dwRefCount);
	ASSERT(szNTLMAuthOut);
	ASSERT(szUser);

	dwIn = strlen(szNTLMAuth) + 1;
	pInBuf = (PBYTE) LocalAlloc(0, dwIn);
	if (! pInBuf) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}

	Base64Decode((char *) szNTLMAuth, (PSTR)pInBuf, &dwIn);

	dwOut = params.dwMaxNTLMBuf;
	pOutBuf = (PBYTE) LocalAlloc(0, (dwOut*(4/3) + 1));	// Need extra bytes for Base64Encode
	if (! pInBuf) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	
	if (! NTLMServerContext(NULL, pAuth, pInBuf, dwIn, pOutBuf, &dwOut, &fDone)) {
		pAuth->m_Conversation = NTLM_DONE;
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: NTLMServerContext failed: %d\n"), dwRetVal));
		goto exit;
	}

	if (fDone) {
		IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Successfully authenticated user.\n")));
		dwRetVal = GetUserInfo(pAuth, szUser, cbUser);
		pAuth->m_Conversation = NTLM_DONE;
		goto exit;
	}

	pAuth->m_Conversation = NTLM_PROCESSING;
	Base64Encode(pOutBuf, dwOut, szNTLMAuthOut);
	
exit:
	if (pInBuf) {
		LocalFree(pInBuf);
	}
	if (pOutBuf) {
		LocalFree(pOutBuf);
	}
	
	return dwRetVal;
}

DWORD GetMaxNTLMBuffSize(DWORD* pdwMaxSecurityBuffSize)
{
	ASSERT(params.dwRefCount);
	ASSERT(pdwMaxSecurityBuffSize);

	*pdwMaxSecurityBuffSize = params.dwMaxNTLMBuf;
	
	return ERROR_SUCCESS;
}

DWORD InitSecurityLib(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	SECURITY_STATUS ss = 0;
	FARPROC pInit;
	PSecurityFunctionTable pSecFun;
	PSecPkgInfo pkgInfo;

	if (params.dwRefCount > 0) {
		params.dwRefCount++;
		goto exit;
	}

	params.hLib = LoadLibrary(_T("secur32.dll"));
	if (! params.hLib) {
		dwRetVal = GetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
		goto exit;
	}

	pInit = (FARPROC) GetProcAddress (params.hLib, SECURITY_ENTRYPOINT);
	if (! pInit)  {
		dwRetVal = GetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
		goto exit;
	}

	pSecFun = (PSecurityFunctionTable) pInit ();
	if (! pSecFun) {
		dwRetVal = GetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
		goto exit;
	}

	memcpy(&params.funcs,pSecFun,sizeof(SecurityFunctionTable));

	ss = params.funcs.QuerySecurityPackageInfo (_T("NTLM"), &pkgInfo);
	if (!SEC_SUCCESS(ss)) {
		dwRetVal = GetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: QuerySecurityPackageInfo failed, GLE=0x%08x\n"),dwRetVal));
		goto exit;
	}

	params.dwMaxNTLMBuf = pkgInfo->cbMaxToken;
	params.funcs.FreeContextBuffer(pkgInfo);

	params.dwRefCount = 1;

exit:
	if (ERROR_SUCCESS != dwRetVal) {
		if (params.hLib) {
			FreeLibrary(params.hLib);
			params.hLib = NULL;
		}
		memset(&params.funcs, 0, sizeof(SecurityParams));
	}
	
	return dwRetVal;
}

DWORD DeinitSecurityLib(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	if (0 == params.dwRefCount) {
		goto exit;
	}
	else if (params.dwRefCount > 1) {
		params.dwRefCount--;
		goto exit;
	}

	if (params.hLib) {
		FreeLibrary(params.hLib);
		params.hLib = NULL;
	}
	
	memset(&params.funcs, 0, sizeof(SecurityParams));

	params.dwRefCount = 0;

exit:
	return dwRetVal;
}

BOOL NTLMServerContext(
			PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
			PAUTH_NTLM pAS,		// NTLM state info
			BYTE *pIn,
			DWORD cbIn,
			BYTE *pOut,
			DWORD *pcbOut,
			BOOL *pfDone)
{
	SECURITY_STATUS	ss;
	TimeStamp		Lifetime;
	SecBufferDesc	OutBuffDesc;
	SecBuffer		OutSecBuff;
	SecBufferDesc	InBuffDesc;
	SecBuffer		InSecBuff;
	ULONG			ContextAttributes;

	
	if (NTLM_NO_INIT_CONTEXT == pAS->m_Conversation)  {
		ss = params.funcs.AcquireCredentialsHandle (
							NULL,	// principal
							_T("NTLM"),
							SECPKG_CRED_INBOUND,
							NULL,	// LOGON id
						    pAuthIdentity,	
							NULL,	// get key fn
							NULL,	// get key arg
							&pAS->m_hcred,
							&Lifetime
							);
		if (SEC_SUCCESS (ss))
			pAS->m_fHaveCredHandle = TRUE;
		else {
			IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: NTLM AcquireCreds failed: %X\n", ss));
			return(FALSE);
		}
	}

	// prepare output buffer
	//
	OutBuffDesc.ulVersion = 0;
	OutBuffDesc.cBuffers = 1;
	OutBuffDesc.pBuffers = &OutSecBuff;

	OutSecBuff.cbBuffer = *pcbOut;
	OutSecBuff.BufferType = SECBUFFER_TOKEN;
	OutSecBuff.pvBuffer = pOut;

	// prepare input buffer
	//
	InBuffDesc.ulVersion = 0;
	InBuffDesc.cBuffers = 1;
	InBuffDesc.pBuffers = &InSecBuff;

	InSecBuff.cbBuffer = cbIn;
	InSecBuff.BufferType = SECBUFFER_TOKEN;
	InSecBuff.pvBuffer = pIn;

	ss = params.funcs.AcceptSecurityContext (
						&pAS->m_hcred,
						(pAS->m_Conversation == NTLM_PROCESSING) ?  &pAS->m_hctxt : NULL,
						&InBuffDesc,
						0,	// context requirements
						SECURITY_NATIVE_DREP,
						&pAS->m_hctxt,
						&OutBuffDesc,
						&ContextAttributes,
						&Lifetime
						);
	if (!SEC_SUCCESS (ss))  {
		IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: NTLM init context failed: %X\n", ss));
		return FALSE;
	}

	pAS->m_fHaveCtxtHandle = TRUE;

	// Complete token -- if applicable
	//
	if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  {
		if (params.funcs.CompleteAuthToken) {
			ss = params.funcs.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
			if (!SEC_SUCCESS(ss))  {
				IFDBG(DebugOut(ZONE_ERROR, L"WebProxy:  NTLM complete failed: %X\n", ss));
				return FALSE;
			}
		}
		else {
			IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: Complete not supported.\n"));
			return FALSE;
		}
	}

	*pcbOut = OutSecBuff.cbBuffer;
	pAS->m_Conversation = NTLM_PROCESSING;

	*pfDone = !((SEC_I_CONTINUE_NEEDED == ss) ||
				(SEC_I_COMPLETE_AND_CONTINUE == ss));

	return TRUE;
}

void FreeNTLMHandles(PAUTH_NTLM pAuth) {
	ASSERT(pAuth);
	ASSERT(params.dwRefCount);
			
	if (pAuth->m_fHaveCtxtHandle)
		params.funcs.DeleteSecurityContext (&pAuth->m_hctxt);

	if (pAuth->m_fHaveCredHandle)
		params.funcs.FreeCredentialHandle (&pAuth->m_hcred);

	pAuth->m_fHaveCredHandle = FALSE;
	pAuth->m_fHaveCtxtHandle = FALSE;
}

DWORD GetUserInfo(PAUTH_NTLM pAuth, char* szUser, int cbUser)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	SecPkgContext_Names pkgName;

	pkgName.sUserName = NULL;
	if (! SEC_SUCCESS(params.funcs.QueryContextAttributes(&(pAuth->m_hctxt), SECPKG_ATTR_NAMES, &pkgName))) {
		dwRetVal = GetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: QueryContextAttributes failed.  Cannot determine user name: %d.\n"), dwRetVal));
		goto exit;
	}

	if (0 == WideCharToMultiByte(CP_ACP, 0, pkgName.sUserName, -1, szUser, cbUser, NULL, NULL)) {
		dwRetVal = ERROR_INSUFFICIENT_BUFFER;
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: User buffer is not large enough.\n"), dwRetVal));
		goto exit;
	}

exit:
	if (pkgName.sUserName) {
		params.funcs.FreeContextBuffer(pkgName.sUserName);
	}
	
	return dwRetVal;
}

const int base642six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

BOOL Base64Decode(char* bufcoded, char* pbuffdecoded, DWORD* pcbDecoded)
{
    int            nbytesdecoded;
    char          *bufin;
    unsigned char *bufout;
    int            nprbytes;
    const int     *rgiDict = base642six;

    /* Strip leading whitespace. */

    while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    bufin = bufcoded;
    while(rgiDict[*(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;

    if ( pcbDecoded )
        *pcbDecoded = nbytesdecoded;

    bufout = (unsigned char *)pbuffdecoded;

    bufin = bufcoded;

    while (nprbytes > 0) {
        *(bufout++) =
            (unsigned char) (rgiDict[*bufin] << 2 | rgiDict[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (rgiDict[bufin[1]] << 4 | rgiDict[bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (rgiDict[bufin[2]] << 6 | rgiDict[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    if(nprbytes & 03) {
        if(rgiDict[bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }

    ((CHAR *)pbuffdecoded)[nbytesdecoded] = '\0';

    return TRUE;
}

const char six2base64[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

BOOL Base64Encode(BYTE* bufin, DWORD nbytes, char* pbuffEncoded)
{
   unsigned char *outptr;
   unsigned int   i;
   const char    *rgchDict = six2base64;

   outptr = (unsigned char *)pbuffEncoded;

   for (i=0; i < nbytes; i += 3) {
      *(outptr++) = rgchDict[*bufin >> 2];            /* c1 */
      *(outptr++) = rgchDict[((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)]; /*c2*/
      *(outptr++) = rgchDict[((bufin[1] << 2) & 074) | ((bufin[2] >> 6) & 03)];/*c3*/
      *(outptr++) = rgchDict[bufin[2] & 077];         /* c4 */

      bufin += 3;
   }

   /* If nbytes was not a multiple of 3, then we have encoded too
    * many characters.  Adjust appropriately.
    */
   if(i == nbytes+1) {
      /* There were only 2 bytes in that last group */
      outptr[-1] = '=';
   } else if(i == nbytes+2) {
      /* There was only 1 byte in that last group */
      outptr[-1] = '=';
      outptr[-2] = '=';
   }

   *outptr = '\0';

   return TRUE;
}

