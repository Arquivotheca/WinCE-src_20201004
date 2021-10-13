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

    SrmpIsapi.cxx

Abstract:

    SRMP ISAPI extension
    Forwards HTTP requests to MSMQ

--*/

#include <windows.h>
#include <stdio.h>
#include <httpext.h>
#include <creg.hxx>

#include <mq.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "scapi.h"
#include "scsrmp.hxx"


#if defined(UNDER_CE) && defined (DEBUG)
  DBGPARAM dpCurSettings = {
    TEXT("SrmpIsapi"), {
    TEXT("Error"),TEXT("Init"),TEXT("MSMQ Interface"),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT("") },
    0x0003
  }; 

#define ZONE_ERROR  DEBUGZONE(0)
#define ZONE_INIT   DEBUGZONE(1)
#define ZONE_MSMQ   DEBUGZONE(2)
#endif


BOOL GetHttpHeaders(LPEXTENSION_CONTROL_BLOCK pECB, LPSTR *ppszHeaders, BOOL *pfSSL);
BOOL GetPostData   (LPEXTENSION_CONTROL_BLOCK pECB, LPSTR *ppszPost);
void SendResponse  (LPEXTENSION_CONTROL_BLOCK pECB, DWORD dwHttpCode, DWORD *pdwStatus);
void CleanupRequest(LPEXTENSION_CONTROL_BLOCK pECB, SrmpIOCTLPacket *pSRMPPacket, HANDLE hDevice);

// Maximum buffer to receive off the wire
static DWORD g_dwMaxPOSTBuffer;

//
// ISAPI Extension Interface Functions
//
extern "C" BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO* pVer) {
	DEBUGMSG(ZONE_INIT,(L"SrmpISAPI: GetExtensionVersion()\r\n"));

	CReg reg(HKEY_LOCAL_MACHINE,MSMQ_SC_SRMP_REGISTRY_KEY);
	g_dwMaxPOSTBuffer = reg.ValueDW(MSMQ_SC_SRMP_MAX_POST,MSMQ_SC_SRMP_MAX_POST_DEFAULT);

	pVer->dwExtensionVersion = HSE_VERSION;
	return TRUE;
}

static const char cszTextXML[]          = "text/xml";
static const char cszMime[]             = "multipart/related";

extern "C" DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK pECB)  {
	SrmpIOCTLPacket  SRMPPacket;
	DWORD            dwHttpCode = 500;
	HANDLE           hDevice = INVALID_HANDLE_VALUE;
	DWORD            dwStatus;

	CHAR             szContentType[MAX_PATH];
	CHAR             szIpAddr[INET6_ADDRSTRLEN+1];
	DWORD            cbContentType = sizeof(szContentType);
	DWORD            cbIpAddr = sizeof(szIpAddr);

	memset(&SRMPPacket,0,sizeof(SRMPPacket));

	if (pECB->cbTotalBytes > g_dwMaxPOSTBuffer) {
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: Client attempts to send %d bytes, maximum allowable buffer size = %d\r\n",pECB->cbTotalBytes,g_dwMaxPOSTBuffer));
		dwHttpCode = 413;
		goto done;
	}

	if (0 != _stricmp(pECB->lpszMethod,"post")) {
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: Received method (%a), must be post\r\n",pECB->lpszMethod));
		dwHttpCode = 403; // access denied, we only allow posts
		goto done;
	}

	// content-type
	if (! pECB->GetServerVariable(pECB->ConnID,"CONTENT_TYPE",szContentType,&cbContentType)) {
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: GetServerVariable(\"CONTENT_TYPE\") failed, request aborting.  GLE=0x%08x\r\n",GetLastError()));
		goto done;
	}

	if (0 == _strnicmp(szContentType,cszTextXML,SVSUTIL_CONSTSTRLEN(cszTextXML)))
		SRMPPacket.contentType = CONTENT_TYPE_XML;
	else if (0 == _strnicmp(szContentType,cszMime,SVSUTIL_CONSTSTRLEN(cszMime)))
		SRMPPacket.contentType = CONTENT_TYPE_MIME;
	else {
		dwHttpCode = 400; // bad request
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: Receieved unknown content type (%s), request aborting.\r\n",szContentType));
		goto done;
	}

	// Remote IP addr
	if (! pECB->GetServerVariable(pECB->ConnID,"REMOTE_ADDR",szIpAddr,&cbIpAddr)) {
		DEBUGCHK(0); // should always be able to retrieve this.
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: GetServerVariable(\"REMOTE_ADDR\") failed, request aborting.  GLE=0x%08x\r\n",GetLastError()));
		goto done;
	}
	SRMPPacket.dwIP4Addr = inet_addr(szIpAddr);

	if (! GetHttpHeaders(pECB,&SRMPPacket.pszHeaders,&SRMPPacket.fSSL))
		goto done;

	if (! GetPostData(pECB,&SRMPPacket.pszPost))
		goto done;

	SRMPPacket.cbPost    = pECB->cbTotalBytes;
	SRMPPacket.cbHeaders = strlen(SRMPPacket.pszHeaders);

	if (INVALID_HANDLE_VALUE == (hDevice = CreateFile (L"MMQ1:", 0, 0, NULL, OPEN_EXISTING, 0, NULL)))
		goto done;

	DeviceIoControl(hDevice,MQAPI_Code_SRMPControl,&SRMPPacket,sizeof(SRMPPacket),&dwHttpCode,sizeof(DWORD),NULL,NULL);
done:
	SendResponse(pECB,dwHttpCode,&dwStatus);
	CleanupRequest(pECB,&SRMPPacket,hDevice);
	return dwStatus;
}

BOOL WINAPI TerminateExtension(DWORD dwFlags) {
	DEBUGMSG(ZONE_INIT,(L"SrmpISAPI: TerminateExtension()\r\n"));
	return TRUE;
}

extern "C" BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) {
	switch(fdwReason) {
		case DLL_PROCESS_ATTACH:
			DEBUGREGISTER((HINSTANCE)hInstDll);
			DEBUGMSG(ZONE_INIT,(TEXT("SrmpISAPI: DllMain attached\r\n")));

#if ! defined (MSMQ_ANCIENT_OS)
			DisableThreadLibraryCalls((HMODULE)hInstDll);
#endif

			break;

		default:
			break;
	}
	return TRUE;
}

// 
// Helper functions
// 

const char cszAllRaw[] = "ALL_RAW";
const char cszServerPortSecure[] = "SERVER_PORT_SECURE";

BOOL GetHttpHeaders(LPEXTENSION_CONTROL_BLOCK pECB, LPSTR *ppszHeaders, BOOL *pfSSL) {
	CHAR  szBuf[256];
	DWORD cbBuffer = sizeof(szBuf);

	if (!pECB->GetServerVariable(pECB->ConnID,(PSTR)cszServerPortSecure,szBuf,&cbBuffer)) {
		DEBUGCHK(0);
		return FALSE;
	}
	*pfSSL = (szBuf[0] == '1'); 

	cbBuffer = 0;
	if (pECB->GetServerVariable(pECB->ConnID,(PSTR)cszAllRaw,NULL,&cbBuffer) || GetLastError() != ERROR_INSUFFICIENT_BUFFER || cbBuffer == 0) {
		DEBUGCHK(0);  // This scenario should always succeed.  Hitting indicates .
		return FALSE;
	}

	if (NULL == ((*ppszHeaders) = (LPSTR) LocalAlloc(LMEM_FIXED,cbBuffer))) {
		DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: LocalAlloc failed, GLE=0x%08x\r\n",GetLastError()));
		return FALSE;
	}

	if (! pECB->GetServerVariable(pECB->ConnID,(PSTR)cszAllRaw,*ppszHeaders,&cbBuffer)) {
		DEBUGCHK(0);
		return FALSE;
	}
	
	return TRUE;
}

BOOL GetPostData(LPEXTENSION_CONTROL_BLOCK pECB, LPSTR *ppszPost) {
	DWORD   cbAvailable  = pECB->cbAvailable;
	DWORD   cbTotalBytes = pECB->cbTotalBytes;

	if (cbTotalBytes > cbAvailable) {
		DWORD cbRead = cbTotalBytes - cbAvailable;

		// There's more to be read off the wire.
		if (NULL == ((*ppszPost) = (LPSTR) LocalAlloc(LMEM_FIXED,cbTotalBytes))) {
			DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: LocalAlloc failed, GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}

		if (!pECB->ReadClient(pECB->ConnID,*ppszPost + cbAvailable,&cbRead) ||
		     (cbRead != cbTotalBytes - cbAvailable) ) {
			DEBUGMSG(ZONE_ERROR,(L"SrmpISAPI: ReadClient failed to receive remainder of POST data, GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}

		memcpy(*ppszPost,pECB->lpbData,cbAvailable);
	}
	else {
		*ppszPost = (LPSTR) pECB->lpbData;
	}
	return TRUE;
}

void SendResponse(LPEXTENSION_CONTROL_BLOCK pECB, DWORD dwHttpCode, DWORD *pdwStatus) {
	HSE_SEND_HEADER_EX_INFO  SendHeaderExInfo;
	LPCSTR szHeader = "Content-Length: 0\r\n\r\n";
	CHAR   szStatus[10];

	if (dwHttpCode == 500) {
		*pdwStatus = HSE_STATUS_ERROR;
		return;
	}
	sprintf(szStatus,"%d",dwHttpCode);
	
	SendHeaderExInfo.pszStatus = szStatus;
	SendHeaderExInfo.cchStatus = strlen(szStatus);
	SendHeaderExInfo.pszHeader = szHeader;
	SendHeaderExInfo.cchHeader = sizeof(szHeader) - 1;
	SendHeaderExInfo.fKeepConn = TRUE;

	if (! pECB->ServerSupportFunction(pECB->ConnID,HSE_REQ_SEND_RESPONSE_HEADER_EX,&SendHeaderExInfo,NULL,NULL))
		*pdwStatus = HSE_STATUS_ERROR;
	else
		*pdwStatus = HSE_STATUS_SUCCESS_AND_KEEP_CONN;
}


void CleanupRequest(LPEXTENSION_CONTROL_BLOCK pECB, SrmpIOCTLPacket *pSRMPPacket, HANDLE hDevice) {
	if (pSRMPPacket->pszHeaders)
		LocalFree(pSRMPPacket->pszHeaders);

	if (pSRMPPacket->pszPost && (pSRMPPacket->pszPost != (PSTR) pECB->lpbData))
		LocalFree(pSRMPPacket->pszPost);

	if (hDevice != INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);
}
