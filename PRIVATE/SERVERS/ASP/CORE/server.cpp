//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: server.cpp
Abstract: Implements Server script object
--*/

#include "aspmain.h"
/////////////////////////////////////////////////////////////////////////////
// CServer


CServer::CServer()
{
	m_pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot); 		
  	DEBUGCHK(m_pASPState); 
}


STDMETHODIMP CServer::get_MapPath(BSTR bstrName, BSTR *pbstrVal)
{
	DEBUG_CODE_INIT;
	PASP_CONTROL_BLOCK pACB = m_pASPState->m_pACB;
	HRESULT ret = DISP_E_EXCEPTION;
	PSTR pszName = NULL;
	DWORD cbBstr = 0;
	int iOutLen;
	int iPathLen  = 0;
	DWORD dwBufferSize = 0;
	DWORD dwBufferSizeOrg = 0;
	
	if (!bstrName || bstrName[0] == '\0')
	{
		myretleave(S_OK,136);
	}

	//  Codepage note:  Convert to ANSI string since it's an http header, like IIS.
	iOutLen = WideCharToMultiByte(CP_ACP, 0, bstrName, -1, 0, 0, 0, 0);
	if(!iOutLen)
	{
		myretleave(S_OK,131);
	}
	if (bstrName[0] != '/')
	{
		PSTR psz = strrchr(pACB->pszVirtualFileName,'/');
		if (!psz)
			psz = strrchr(pACB->pszVirtualFileName,'\\');
		if (!psz)
			myleave(149);

		iPathLen = (psz - pACB->pszVirtualFileName)+1;
	}
	dwBufferSizeOrg = dwBufferSize = iOutLen + iPathLen;

	if(NULL == (pszName = MySzAllocA(dwBufferSize)))
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(138);
	}
	
	if (iPathLen)
	{
		memcpy(pszName,pACB->pszVirtualFileName,iPathLen);
		WideCharToMultiByte(CP_ACP, 0, bstrName, -1, pszName+iPathLen, iOutLen, 0, 0);
	}
	else
		WideCharToMultiByte(CP_ACP, 0, bstrName, -1, pszName, iOutLen, 0, 0);


	if ( FALSE == pACB->ServerSupportFunction(pACB->ConnID, HSE_REQ_MAP_URL_TO_PATH,
							(LPVOID) pszName, &dwBufferSize, NULL))
	{
		// If not mapped, we don't get INSUF_BUFFER.  Just return, don't end script
		if ( GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			myretleave(S_OK,132);
		}

		//  buffer isn't big enough, so try again
		pszName = MyRgReAlloc(CHAR, pszName, dwBufferSizeOrg, dwBufferSize);
		if (NULL == pszName)
		{
			ASP_ERR(IDS_E_NOMEM);
			myleave(133);
		}

		if ( FALSE == pACB->ServerSupportFunction(pACB->ConnID, HSE_REQ_MAP_URL_TO_PATH,
							(LPVOID) pszName, &dwBufferSize, NULL))	
		{
			DEBUGCHK(FALSE);		// this shouldn't happen
			ASP_ERR(IDS_E_HTTPD);
			myleave(134);
		}
	}

	DEBUGMSG(ZONE_SERVER,(L"ASP: MapPath matched to <<%a>>\r\n",pszName));
	if (  FAILED(SysAllocStringFromSz(pszName, 0, pbstrVal, CP_ACP)))
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(124);
	}

	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CServer::get_MapPath failed, err = %d,GLE = %X\r\n",err,
					GetLastError()));

	MyFree(pszName);
	return ret;
}


/*===================================================================
CServer::URLEncode

Encodes a query string to URL standards

Parameters:
	BSTR		bstrIn			value: string to be URL encoded
	BSTR FAR *	pbstrEncoded	value: pointer to URL encoded version of string

Returns:
	HRESULT		NOERROR on success
===================================================================*/
STDMETHODIMP CServer::get_URLEncode ( BSTR bstrIn, BSTR  *pbstrEncoded)
{
	DEBUG_CODE_INIT;
	char*	pszstrIn 			= NULL;	
	char*	pszEncodedstr		= NULL;
	char*	pszStartEncodestr	= NULL;
	int		nstrLen				= 0;
	HRESULT	ret					= DISP_E_EXCEPTION;

	// No string or empty string, return 
	if (!bstrIn || bstrIn[0] == '\0')
		myretleave(S_OK,0);

//  Codepage note:  Convert to ANSI string, not DBCS.  Like IIS.

	pszstrIn = MySzDupWtoA(bstrIn);
	if (NULL == pszstrIn)
	{
		ASP_ERR(IDS_E_NOMEM)
		myleave(200);
	}
	
	nstrLen = URLEncodeLen(pszstrIn);
	DEBUGMSG(ZONE_SERVER,(L"ASP: URLEncode got string <<%a>>, url len = %d\r\n",pszstrIn,nstrLen));

	if (nstrLen <= 0)
		myretleave(S_OK,0);
	

	//Encode string	, NOTE this function returns a pointer to the
	// NULL so you need to keep a pointer to the start of the string
	//

	// URLEncode puts closing \0 on string, don't zero it here
	pszEncodedstr = MyRgAllocNZ(CHAR,nstrLen+2);		
	pszStartEncodestr = pszEncodedstr;

	if (NULL == pszEncodedstr)
	{
		myleave(201);
	}			

	pszEncodedstr = ::URLEncode( pszEncodedstr, pszstrIn );
	DEBUGMSG(ZONE_SERVER,(L"ASP: Url encode translates to <<%a>>\r\n",pszStartEncodestr));

	
	if ( FAILED ( SysAllocStringFromSz( pszStartEncodestr, nstrLen, pbstrEncoded, 
										CP_ACP)))
	{
		ASP_ERR(IDS_E_NOMEM)
		myleave(202);
	}


	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CServer::get_URLEncode failed, err = %d, err = %X",
								err, GetLastError()));
				
	MyFree(pszstrIn);
	MyFree(pszStartEncodestr);

	return ret;
}



