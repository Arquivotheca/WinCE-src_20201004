//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    servacpt.cpp
 *
 * Purpose: WinCE service manager.  Handles multiple ports on services
 *
 */


#include <windows.h>
#include "serv.h"
#include <svsutil.hxx>
#include <service.h>
#include <iphlpapi.h>

#define SERVICES_MAXIMUM_SOCKETS FD_SETSIZE-1
#define MAX_WRITE_KEY_LEN          (MAX_PATH+1)


typedef struct _ServiceSocket {
	struct _ServiceSocket *pNext;
	fsdev_t               *pOwner;  // device owner context (corresponds to  "TEL0", "HTP0",...)
	SOCKET                sock;     // socket owner
	SOCKADDR_STORAGE      sockAddr;
	int                   cbSockAddr;
	int                   iProtocol;
	WCHAR                 szRegWritePath[MAX_WRITE_KEY_LEN];
} ServiceSocket;

// Global information concerning the socket state
static SOCKET g_sockNotify;
static BOOL   g_fSockNotifyOpen;

static ServiceSocket        *g_pServiceSockets;
static struct FixedMemDescr *g_pServiceSocketsMemDescr;
static DWORD         g_dwOpenSockets;
static BOOL          g_fIsAcceptThreadRunning;

// iphlpapi functions
typedef DWORD (WINAPI * PFN_GETADAPTERSADDRESSES)(ULONG Family, DWORD Flags, PVOID Reserved, PIP_ADAPTER_ADDRESSES pAdapterAddresses, PULONG pOutBufLen);

PFN_GETADAPTERSADDRESSES  pfnGetAdaptersAddresses;

DWORD WINAPI ServicesAcceptThread(LPVOID lpv);
DWORD WINAPI ServicesNotifyAddrChangeThread(LPVOID lpv);

// Creates ServicesAcceptThread first time through.  g_ServCS is held by caller.
static BOOL CreateAcceptThreadIfNeeded(void)  {
	HANDLE hThread;
	if (g_fIsAcceptThreadRunning) {
		return TRUE;
	}
	DEBUGCHK(g_dwOpenSockets == 0 && g_fSockNotifyOpen == FALSE);

	hThread = CreateThread(NULL, 0, ServicesAcceptThread, NULL, 0, NULL);
	if (!hThread)
		return FALSE;
	
	CloseHandle(hThread);
	g_fIsAcceptThreadRunning = TRUE;

	return TRUE;
}

static void CreateNotifySocket(void) {
	DEBUGCHK(g_fSockNotifyOpen == FALSE);
	g_sockNotify = socket(AF_INET,SOCK_STREAM,0);

	if (g_sockNotify == INVALID_SOCKET) {
		// for the day when IPv6 can be on a device without IPv4
		g_sockNotify = socket(AF_INET6,SOCK_STREAM,0);
	}

	// If we can't create notification socket we're in trouble.
	DEBUGCHK(g_sockNotify != INVALID_SOCKET);
	g_fSockNotifyOpen = TRUE;
}

// Called with global CritSec call to notify accept thread of socket change in main list.
static void NotifyAcceptThreadOfChange(void) {
	if (g_fSockNotifyOpen) {
		closesocket(g_sockNotify);
		g_fSockNotifyOpen = FALSE;
	}
}

static void RemoveEntryFromServiceList(ServiceSocket *pRemove, ServiceSocket *pPrevious) {
	if (pRemove == g_pServiceSockets) {
		DEBUGCHK(!pPrevious);
		g_pServiceSockets = pRemove->pNext;
	}
	else {
		DEBUGCHK(pPrevious);
		pPrevious->pNext = pRemove->pNext;
	}
}

static BOOL IsSockAddrValid(PSOCKADDR pSockAddr) {
	if ((pSockAddr->sa_family != AF_INET) && (pSockAddr->sa_family != AF_INET6)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICS! Super-services accept thread only listens on IPv4 and IPv6 addresses.  Failing request to listen on type %d\r\n",pSockAddr->sa_family));
		return FALSE;
	}
	return TRUE;
}

// Caller holds g_ServCS and has already validated that pOwner is valid service.
BOOL InternalServiceIOControl(fsdev_t *pOwner, DWORD dwIOControl, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut) {
	BOOL  fRet   = FALSE;
	DWORD dwActualOut;

	pOwner->dwRefCnt++;
	LeaveCriticalSection(&g_ServCS);

	__try {
		fRet = pOwner->fnControl(pOwner->dwData,dwIOControl,pBufIn,dwLenIn,pBufOut,dwLenOut,&dwActualOut);
	} __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!IOControl(0x%08x) for service 0x%08x throws exception 0x%08x\r\n",dwIOControl,pOwner,GetExceptionCode()));
	}
	EnterCriticalSection(&g_ServCS);
	pOwner->dwRefCnt--;

	if (pOwner->dwRefCnt == 0) {
		FreeService(pOwner);
		fRet = FALSE;
	}

	return fRet;
}


// Gets next available socket in list.
void SetNewServiceSocket(ServiceSocket *pServSocket, SOCKET sockNew, fsdev_t *pOwner, SOCKADDR_STORAGE *pSockAddr,
                            DWORD cbSockAddr, int iProtocol,WCHAR *szRegWritePath) {
	pServSocket->pOwner     = pOwner;
	pServSocket->sock       = sockNew;
	pServSocket->iProtocol  = iProtocol;
	pServSocket->cbSockAddr = cbSockAddr;
	memcpy(&pServSocket->sockAddr,pSockAddr,cbSockAddr);

	if (szRegWritePath)
		wcscpy(pServSocket->szRegWritePath,szRegWritePath);
	else
		pServSocket->szRegWritePath[0] = 0;

	pServSocket->pNext    = g_pServiceSockets;
	g_pServiceSockets     = pServSocket;
}

#define MAX_PORT_KEYS_TO_OPEN  16

// ServiceAddPortInternal is called both on services.exe initialization and from ServiceAddPort API.
// In initialization case we don't want to write out szRegPath because we're reading it from registry already, assume it's valid.
// In ServiceAddPort (fWriteRegistry=TRUE if szRegWritePath!=NULL) then we do write it out, need extra validation.
BOOL ServiceAddPortInternal(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, WCHAR *szRegWritePath, BOOL fWriteRegistry) {
	fsdev_t *pOwner = (fsdev_t*) hService;
	DWORD  dwErr = ERROR_INTERNAL_ERROR;
	SOCKET sockNew = INVALID_SOCKET;
	BOOL   fDecrement = FALSE;    // on error do we decrement g_dwOpenSockets?
	WCHAR  szRegPath[MAX_WRITE_KEY_LEN];
	ServiceSocket *pServSocket = NULL;

	if (fWriteRegistry && szRegWritePath && (NULL == (szRegWritePath=ValidatePWSTR(szRegWritePath)))) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	EnterCriticalSection(&g_ServCS);
	if (!IsValidService(pOwner)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!ServiceAddPort:Service (0x%08x) not running\r\n",pOwner));
		dwErr = ERROR_SERVICE_NOT_ACTIVE;
		goto doneInCS;
	}

	if (fWriteRegistry && szRegWritePath) {
		if (!pOwner->szRegPath || ((pOwner->dwRegPathLen + wcslen(szRegWritePath) + 1 + SERVICE_ACCEPT_KEY_STR_LEN) >= MAX_WRITE_KEY_LEN)) {
			DEBUGMSG(ZONE_ERROR,(L"SERVICES! ServiceAddPort: szRegWritePath!=NULL, service either has no reg path or passed reg path would be too long\r\n"));
			dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
			goto doneInCS;
		}
	}

	if (NULL == (pServSocket = (ServiceSocket*) svsutil_GetFixed(g_pServiceSocketsMemDescr))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES! ServiceAddPort: Ran out of memory on svsutil_GetFixed().\r\n"));
		dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		goto doneInCS;
	}
	LeaveCriticalSection(&g_ServCS);

	if (INVALID_SOCKET == (sockNew = socket(pSockAddr->sa_family,SOCK_STREAM,iProtocol))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Unable to create socket, GLE=0x%08x\r\n",WSAGetLastError()));
		dwErr = WSAGetLastError();
		goto done;
	}

	if (ERROR_SUCCESS != (bind(sockNew,pSockAddr,cbSockAddr))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Unable to bind to socket, GLE=0x%08x\r\n",WSAGetLastError()));
		dwErr = WSAGetLastError();
		goto done;
	}

	if (ERROR_SUCCESS != (listen(sockNew,SOMAXCONN))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Unable to listen to socket, GLE=0x%08x\r\n",WSAGetLastError()));
		dwErr = WSAGetLastError();
		goto done;
	}

	EnterCriticalSection(&g_ServCS);
	if (!IsValidService(pOwner)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Service (0x%08x) not running\r\n",pOwner));
		dwErr = ERROR_SERVICE_NOT_ACTIVE;
		goto doneInCS;
	}
	if (g_dwOpenSockets >= SERVICES_MAXIMUM_SOCKETS) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Too many sockets registered.\r\n"));
		dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		goto doneInCS;
	}

	if (! CreateAcceptThreadIfNeeded()) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Cannot create ServicesAcceptThread!  GLE=0x%08x, function will return error %0x08x\r\n",GetLastError(),ERROR_SERVICE_NO_THREAD));
		dwErr = ERROR_SERVICE_NO_THREAD;
		goto doneInCS;
	}
	g_dwOpenSockets++;

	if (! InternalServiceIOControl(pOwner,IOCTL_SERVICE_REGISTER_SOCKADDR,(PBYTE)pSockAddr,cbSockAddr)) {
		DEBUGMSG(ZONE_ACTIVE,(L"SERVICES!xxx_IOControl(IOCTL_SERVICE_REGISTER_SOCKADDR) returns FALSE, not adding socket to accept list\r\n"));
		dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		g_dwOpenSockets--;
		goto doneInCS;
	}
	if (!IsValidService(pOwner)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Service (0x%08x) not running\r\n",pOwner));
		dwErr = ERROR_SERVICE_NOT_ACTIVE;
		g_dwOpenSockets--;
		goto doneInCS;
	}	

	if (fWriteRegistry && szRegWritePath) {
		HKEY  hKey = 0;

		wcscpy(szRegPath,pOwner->szRegPath);
		wcscpy(szRegPath+pOwner->dwRegPathLen,SERVICE_ACCEPT_KEY);
		szRegPath[pOwner->dwRegPathLen+SERVICE_ACCEPT_KEY_STR_LEN] = L'\\';
		wcscpy(szRegPath+pOwner->dwRegPathLen+SERVICE_ACCEPT_KEY_STR_LEN+1,szRegWritePath);

		DEBUGCHK((wcslen(szRegPath) + 1) < SVSUTIL_ARRLEN(szRegPath));
		
		if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, KEY_ALL_ACCESS, &hKey)) {
			DWORD dwDisp;
			if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, NULL, REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS, NULL, &hKey, &dwDisp)) {
				DEBUGMSG(ZONE_ERROR,(L"SERVICES!ServiceAddPort cannot create registry key %s\r\n",szRegPath));
				dwErr = ERROR_INTERNAL_ERROR;
				g_dwOpenSockets--;
				goto doneInCS;
			}
		}
		DEBUGCHK(hKey);

		RegSetValueEx(hKey, SERVICE_ACCEPT_SOCKADDR, 0, REG_BINARY, (LPBYTE)pSockAddr, cbSockAddr);
		RegSetValueEx(hKey, SERVICE_ACCEPT_PROTOCOL, 0, REG_DWORD, (LPBYTE)&iProtocol, sizeof(DWORD));
		RegCloseKey(hKey);		
	}

	WCHAR *szStoreRegWritePath;

	if (szRegWritePath && !fWriteRegistry)
		szStoreRegWritePath = szRegWritePath; // being called from StartSuperService(), which has registry key formatted properly.
	else if (szRegWritePath)
		szStoreRegWritePath = szRegPath; // use our built up path during ServiceAddPort call
	else
		szStoreRegWritePath = NULL;

	SetNewServiceSocket(pServSocket,sockNew,pOwner,(SOCKADDR_STORAGE*)pSockAddr,cbSockAddr,iProtocol,szStoreRegWritePath);
	NotifyAcceptThreadOfChange();

	dwErr = ERROR_SUCCESS;
doneInCS:
	if (dwErr != ERROR_SUCCESS) {
		// Free pServSocket if we had allocated it.
		if (pServSocket)
			svsutil_FreeFixed(pServSocket,g_pServiceSocketsMemDescr);
	}

	LeaveCriticalSection(&g_ServCS);
done:
	if (dwErr != ERROR_SUCCESS) {
		SetLastError(dwErr);
		if (sockNew != INVALID_SOCKET)  {
			closesocket(sockNew);
		}
	}
	return (dwErr==ERROR_SUCCESS);
}

// Handles a single sockaddress item, which can either be read in from the registry on init
// or dynamically.  Attempts to bind the socket, notifies thread responsible for
// handling particular socket family (or creates a thread if needed), and notifies
// the service.
BOOL SERV_ServiceAddPort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, WCHAR *szRegWritePath) {
	if ((PSLGetCallerTrust() != OEM_CERTIFY_TRUST) && pSockAddr)
		pSockAddr = (SOCKADDR*) MapCallerPtr(pSockAddr,cbSockAddr);

	if (!pSockAddr || !IsSockAddrValid(pSockAddr)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return ServiceAddPortInternal(hService, pSockAddr, cbSockAddr, iProtocol, szRegWritePath,szRegWritePath ? TRUE : FALSE);
}

// Unbind all services associated with a given service.
BOOL SERV_ServiceUnbindPorts(HANDLE hService) {
	return UnloadSuperServices((fsdev_t*)hService,FALSE,TRUE);
}

BOOL SERV_ServiceClosePort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, BOOL fRemoveFromRegistry) {
	ServiceSocket *pTrav, *pFollow;
	fsdev_t *pOwner = (fsdev_t*) hService;
	DWORD dwErr = ERROR_INTERNAL_ERROR;
	BOOL  fFoundService = FALSE;

	if (!pSockAddr || (0 == cbSockAddr)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!ServiceClosePort:pSockAddr and/or cbSockAddr are invalid, must be non-NULL\r\n",pOwner));
		dwErr = ERROR_INVALID_PARAMETER;
		goto done;
	}

	EnterCriticalSection(&g_ServCS);
	if (!IsValidService(pOwner)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!ServiceClosePort:Service (0x%08x) not running\r\n",pOwner));
		dwErr = ERROR_SERVICE_NOT_ACTIVE;
		goto doneInCs;
	}

	pTrav   = g_pServiceSockets;
	pFollow = NULL;

	while (pTrav) {
		if ((pTrav->pOwner == pOwner) && (pTrav->cbSockAddr == cbSockAddr) && (pTrav->iProtocol == iProtocol) && (0 == memcmp(&pTrav->sockAddr,pSockAddr,cbSockAddr))) {
			if (pTrav == g_pServiceSockets) {
				DEBUGCHK(!pFollow);
				g_pServiceSockets = pTrav->pNext;
			}
			else {
				DEBUGCHK(pFollow);
				pFollow->pNext = pTrav->pNext;
			}
			fFoundService = TRUE;
			closesocket(pTrav->sock);
			g_dwOpenSockets--;

			if (fRemoveFromRegistry && pTrav->szRegWritePath[0]) {
				RegDeleteKey(HKEY_LOCAL_MACHINE,pTrav->szRegWritePath);
			}

			svsutil_FreeFixed(pTrav,g_pServiceSocketsMemDescr);
			break;
		}
		else {
			pFollow = pTrav;
			pTrav   = pTrav->pNext;
		}
	}

	if (fFoundService) {
		NotifyAcceptThreadOfChange();
		InternalServiceIOControl(pOwner,IOCTL_SERVICE_DEREGISTER_SOCKADDR,(BYTE*)pSockAddr,cbSockAddr);
		dwErr = ERROR_SUCCESS;
	}
	else {
		DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!No super services associated with handle (0x%08x)\r\n",pOwner));
		dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
	}

doneInCs:
	LeaveCriticalSection(&g_ServCS);
done:
	if (dwErr != ERROR_SUCCESS) {
		SetLastError(dwErr);
		return FALSE;
	}
	return TRUE;
}

// Closes all sockets associated with a particular service and notifies client of as much.
// fDeregisterService=TRUE if caller is SERV_DeregisterService.
BOOL UnloadSuperServices(fsdev_t *pOwner, BOOL fCallerHoldingCS, BOOL fReleaseCS) {
	ServiceSocket *pTrav, *pFollow;
	DWORD         dwErr = ERROR_INTERNAL_ERROR;
	BOOL          fFoundService = FALSE;

	// If fCallerHoldingCS=FALSE (i.e. we grab CS here), we must release when done.
	DEBUGCHK(! ((fCallerHoldingCS==FALSE) && (fReleaseCS==FALSE)));

	if (!fCallerHoldingCS) {
		EnterCriticalSection(&g_ServCS);
		if (!IsValidService(pOwner)) {
			dwErr = ERROR_SERVICE_NOT_ACTIVE;
			goto doneInCS;
		}
	}
	pTrav   = g_pServiceSockets;
	pFollow = NULL;

	while (pTrav)  {
		if (pTrav->pOwner == pOwner) {
			ServiceSocket *pRemove;
		  	if (pTrav == g_pServiceSockets) {
				DEBUGCHK(!pFollow);
				g_pServiceSockets = pTrav->pNext;
			}
			else {
				DEBUGCHK(pFollow);
				pFollow->pNext = pTrav->pNext;
			}
			fFoundService = TRUE;
			closesocket(pTrav->sock);
			g_dwOpenSockets--;

			pRemove = pTrav;
			pTrav   = pTrav->pNext;
			svsutil_FreeFixed(pRemove,g_pServiceSocketsMemDescr);
		}
		else {
			pFollow = pTrav;
			pTrav   = pTrav->pNext;
		}
	}

	if (fFoundService) {
		NotifyAcceptThreadOfChange();
		InternalServiceIOControl(pOwner,IOCTL_SERVICE_DEREGISTER_SOCKADDR,NULL,0);
	}
	else {
		DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!No super services associated with handle (0x%08x)\r\n",pOwner));
	}

	dwErr = ERROR_SUCCESS;
doneInCS:
	if (fReleaseCS)
		LeaveCriticalSection(&g_ServCS);
	if (dwErr != ERROR_SUCCESS)
		SetLastError(dwErr);

	return (dwErr==ERROR_SUCCESS);
}


// Called when services.exe is starting up, does initial allocations
BOOL SuperServicesInit(void) {
	WSADATA		wsadata;

	svsutil_Initialize();
	g_pServiceSocketsMemDescr = svsutil_AllocFixedMemDescr(sizeof(ServiceSocket),10);

	if (!g_pServiceSocketsMemDescr || WSAStartup(MAKEWORD(2,2), &wsadata)) {
		RETAILMSG(1,(L"SERVICES!Cannot initialize, services.exe will not start.  GLE=0x%08x\r\n",GetLastError()));
		return FALSE;
	}

	HINSTANCE hIphlpLib;

	// Load IPHlpapi functions.
	if (NULL == (hIphlpLib = LoadLibrary(L"\\windows\\iphlpapi.dll"))) {
		RETAILMSG(1,(L"LoadLibrary(iphlpapi.dll) failed, GLE=0x%08x.  Services will not receive IOCTL_SERVICE_NOTIFY_ADDR_CHANGE notification.\r\n",GetLastError()));
		return TRUE; // not a fatal error.
	}

	pfnGetAdaptersAddresses = (PFN_GETADAPTERSADDRESSES) GetProcAddress(hIphlpLib,L"GetAdaptersAddresses");

	if (!pfnGetAdaptersAddresses) {
		DEBUGCHK(0); // iphlpapi is messed up somehow.
		pfnGetAdaptersAddresses = NULL;
		FreeLibrary(hIphlpLib);
		return TRUE;
	}

	// iphlapi thread is optional
	HANDLE hThread = CreateThread(NULL,0,ServicesNotifyAddrChangeThread,NULL,0,NULL);
	if (hThread)
		CloseHandle(hThread);

	return TRUE;
}

// Called from StartOneService if a service registers is registered
// and if its registry flags have SERVICE_CONTEXT_FLAGS_SUPERSERVICE set.
// Read HKLM\Services\(CurrentService) and binds to appropriate sockets.
BOOL StartSuperService(LPCWSTR szBuiltinPath, HANDLE hService, BOOL fSignalStarted)  {
	WCHAR szRegKey[MAX_WRITE_KEY_LEN];
	WCHAR *szEndORegKey;
	DWORD ccAppendToRegKey;
	HKEY  hKey = 0;
	HKEY  hSubKey;
	DWORD dwIndex = 0;
	CHAR  szDataBuffer[256];
	DWORD dwDataBufSize;
	DWORD dwType;
	DWORD dwErr = 0;
	WCHAR szSubName[MAX_WRITE_KEY_LEN];
	DWORD dwLen;
	DWORD dwMaxLen;
	DWORD dwProtocol;
	fsdev_t *pOwner = (fsdev_t*)hService;
	CHAR  *pszBuffer = NULL;  // needed in case szDataBuffer isn't big enough.
	CHAR  *pszData   = NULL;
	DWORD dwValues;
	BOOL  fRet       = TRUE;

	if ((wcslen(szBuiltinPath) + SERVICE_ACCEPT_KEY_STR_LEN + 2) > MAX_WRITE_KEY_LEN) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!StartSuperServices: Registry path too long!\r\n"));
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// build up base registry key.
	wcscpy(szRegKey,szBuiltinPath);
	wcscat(szRegKey,SERVICE_ACCEPT_KEY);

	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegKey, 0, KEY_ALL_ACCESS, &hKey)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!StartSuperService cannot open reg key %s\r\n",szRegKey));
		SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
		return FALSE;
	}

	EnterCriticalSection(&g_ServCS);
	if (! IsValidService(pOwner)) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES!Service 0x%08x is invalid\r\n",pOwner));
		SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
		goto doneInCS;
	}

	// Make sure that service supports super services before we get rolling.
	if (! InternalServiceIOControl(pOwner,IOCTL_SERVICE_REGISTER_SOCKADDR,(PBYTE)NULL,0)) {
		DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!Key %s does not support xxx_IOControl(IOCTL_SERVICE_REGISTER_SOCKADDR)\r\n",szBuiltinPath));
		SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		goto doneInCS;
	}

	// szRegKey will be passed to ServiceAddPortInternal() in event an app wants to delete an existing
	// service registry key.  Just copy data to end of buffer, put '\' in now.
	szEndORegKey     = szRegKey + wcslen(szRegKey);
	*szEndORegKey++  = L'\\';
	ccAppendToRegKey = SVSUTIL_ARRLEN(szRegKey) - (szEndORegKey-szRegKey) - 1;

	// Create the ports to accept() on and pass the config information.
	while (1) {
		BOOL fPassRegPath = FALSE;

		// Critical Section is held at top of loop.
		dwLen = sizeof(szSubName) / sizeof(szSubName[0]);
		if (ERROR_SUCCESS != RegEnumKeyEx(hKey, dwIndex++, szSubName, &dwLen, NULL, NULL, NULL, NULL))
			break;

		if (ERROR_SUCCESS != RegOpenKeyEx(hKey,szSubName,0,KEY_ALL_ACCESS,&hSubKey))
			continue;

		dwDataBufSize = sizeof(szDataBuffer);
		if (ERROR_SUCCESS != RegQueryValueEx(hSubKey,SERVICE_ACCEPT_SOCKADDR,NULL,&dwType,(LPBYTE)szDataBuffer,&dwDataBufSize) || (dwType!=REG_BINARY)) {
			DEBUGMSG(ZONE_ERROR,(L"SERVICES!Reg Key %s doesn't have SockAddr (REG_BINARY) set\r\n",szRegKey));
			RegCloseKey(hSubKey);
			continue;
		}

		if (! IsSockAddrValid((SOCKADDR *) szDataBuffer)) {
			RegCloseKey(hSubKey);
			continue;
		}

		dwLen = sizeof(DWORD);
		if (ERROR_SUCCESS != RegQueryValueEx(hSubKey,SERVICE_ACCEPT_PROTOCOL,NULL,&dwType,(LPBYTE)&dwProtocol,&dwLen) || (dwType!=REG_DWORD))  {
			dwProtocol = 0;
		}

		// create szRegWriteKey if there's space.  If there's not, only negative impact is that an app won't
		// be able to delete this particular registy key programatically (can still do via registry directly).
		if (dwLen < ccAppendToRegKey) {
			wcscpy(szEndORegKey,szSubName);
			fPassRegPath = TRUE;
		}
		
		LeaveCriticalSection(&g_ServCS);
		fRet = ServiceAddPortInternal(pOwner,(SOCKADDR *) szDataBuffer,dwDataBufSize,dwProtocol, fPassRegPath ? szRegKey : NULL, FALSE);
		EnterCriticalSection(&g_ServCS);

		if (!IsValidService(pOwner)) {
			DEBUGMSG(ZONE_ERROR,(L"SERVICES!Cannot start service 0x%08x, it was closed during initialization\r\n",hService));
			goto doneInCS;
		}

		if (!fRet) {
			// We stop reading the registry only on specific error codes.  All other err codes
			// returned to us will be non-fatal, for instance if a socket addr can't be bound to
			// because another socket is bound to it already.
			dwErr = GetLastError();
			if (dwErr == ERROR_SERVICE_NO_THREAD || dwErr == ERROR_SERVICE_CANNOT_ACCEPT_CTRL ||
				dwErr == ERROR_SERVICE_NOT_ACTIVE || dwErr == ERROR_INTERNAL_ERROR) 
			{
				DEBUGMSG(ZONE_ERROR,(L"SERVICES!Not reading anymore service ports from %s\r\n",szRegKey));
				RegCloseKey(hSubKey);
				SetLastError(dwErr);
				fRet = TRUE; // non-fatal error for caller, though.  Don't deregister service even if this occurs.
				goto doneInCS;
			}
		}
		fRet = TRUE;
		RegCloseKey(hSubKey);
	} // while (1)

	RegCloseKey(hKey);
	hKey = 0;

	// Read in the config paramaters.
	wcscpy(szRegKey,szBuiltinPath);
	wcscat(szRegKey,SERVICE_CONFIG_KEY);

	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegKey, 0, KEY_ALL_ACCESS, &hKey)) {
		DEBUGMSG(ZONE_INIT,(L"SERVICES!StartSuperService cannot open reg key %s\r\n",szRegKey));
		goto doneStartSuperServices;
	}

	if (ERROR_SUCCESS != RegQueryInfoKey(hKey,NULL,NULL,NULL,NULL,NULL,NULL,
	                                     &dwValues,NULL,&dwMaxLen,NULL,NULL))
		goto doneStartSuperServices;

	if (dwMaxLen > sizeof(szDataBuffer)) {
		if (NULL == (pszBuffer = (CHAR*) LocalAlloc(LMEM_FIXED,dwMaxLen))) {
			goto doneInCS;
		}
		pszData = pszBuffer;
	}
	else
		pszData = szDataBuffer;


	for (dwIndex=0; dwIndex < dwValues; dwIndex++) {
		DWORD dwNameLen = sizeof(szSubName) / sizeof(WCHAR);
		dwLen = dwMaxLen;
		if (ERROR_SUCCESS != RegEnumValue(hKey,dwIndex,szSubName,&dwNameLen,
		                                  NULL,NULL,(UCHAR*)pszData,&dwLen))
			break;

		InternalServiceIOControl(pOwner,IOCTL_SERVICE_CONTROL,(UCHAR*)pszData,dwLen);
		if (! IsValidService(pOwner)) {
			DEBUGMSG(ZONE_ERROR,(L"SERVICES!Service(0x%08x) removed during startup, stopping startup\r\n",hService));
			SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
			goto doneInCS;
		}
	}

doneStartSuperServices:
	DEBUGCHK(fRet == TRUE);

	// fSignalStarted = TRUE only when we load the service.  The only other time this
	// code path is called is from ServIOControl on an app generated IOCTL Refresh or Start,
	// in whach case we've already called Refresh or Start into service and don't want to call
	// started.
	if (fSignalStarted)
		fRet = InternalServiceIOControl(pOwner,IOCTL_SERVICE_STARTED,(PBYTE)&hService,sizeof(hService));
doneInCS:
	LeaveCriticalSection(&g_ServCS);

	if (hKey)
		RegCloseKey(hKey);

	if (pszBuffer)
		LocalFree(pszBuffer);

	return fRet;
}

#define SELECT_FAIL_SLEEP_TIME                  (10*1000)

DWORD WINAPI ServicesAcceptThread(LPVOID lpv)  {
	ServiceSocket  *pTrav;
	unsigned short addrFamily = (unsigned short) lpv;
	fd_set         sockSet;
	int            iReadySockets, i;
#if defined (DEBUG)
	BOOL           fBreakOnSelectFailure = TRUE;
#endif

	while (1)  {
		FD_ZERO(&sockSet);
		EnterCriticalSection(&g_ServCS);

		if (!g_fSockNotifyOpen)
			CreateNotifySocket();

		if (g_sockNotify != INVALID_SOCKET)
			FD_SET(g_sockNotify,&sockSet);

		for (pTrav = g_pServiceSockets; pTrav; pTrav = pTrav->pNext) {
			FD_SET(pTrav->sock,&sockSet);
		}
		LeaveCriticalSection(&g_ServCS);

		DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!About to call select()\r\n"));
		iReadySockets = select(0,&sockSet,NULL,NULL,NULL);
		DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!Woke up from select() call, iReadySockets=%d\r\n",iReadySockets));

		if (iReadySockets == 0 || iReadySockets == SOCKET_ERROR) {
			// This can only happen if winsock fails an internal CreateEvent().
			DEBUGMSG(ZONE_ERROR,(L"services.exe: select() returned 0 or SOCKET_ERROR, GLE=0x%08x.  Sleeping for %d ms and trying to call select again\r\n",
			              GetLastError(),SELECT_FAIL_SLEEP_TIME));

#if 0
			if (fBreakOnSelectFailure) {
				// Possible condition won't go away, in which case DEBUGCHK every 30 seconds is obnoxious.
				fBreakOnSelectFailure = FALSE;
				DEBUGCHK(0);
			}
#endif
			// Sleep and retry in case this is a temporary condition.
			Sleep(SELECT_FAIL_SLEEP_TIME);
			continue;
		}

		for (i = 0; i < iReadySockets; i++) {
			EnterCriticalSection(&g_ServCS);
			if (!g_fSockNotifyOpen)  {
				// If we get a notification message play it safe and reread the list.
				DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!Received notification message.\r\n"));
				LeaveCriticalSection(&g_ServCS);
				break;
			}

			for (pTrav = g_pServiceSockets; pTrav; pTrav = pTrav->pNext) {
				if (sockSet.fd_array[i] == pTrav->sock) {
					SOCKET sockAccept = accept(sockSet.fd_array[i],0,0);
					if (sockAccept != INVALID_SOCKET) {
						DEBUGMSG(ZONE_ACCEPT,(L"SERVICES!ServicesAcceptThread calling service 0x%08x with connection socket\r\n",pTrav->pOwner));
						InternalServiceIOControl(pTrav->pOwner,IOCTL_SERVICE_CONNECTION,(PBYTE)(&sockAccept), sizeof(SOCKET));
					}
					LeaveCriticalSection(&g_ServCS);
					break;
				}
			}
			DEBUGCHK(pTrav);
			// If we couldn't create notify socket on last pass, try again this time.
			if (g_sockNotify == INVALID_SOCKET)
				g_fSockNotifyOpen = FALSE;
		} // for (iReadySockets)
	} // while (1)

	DEBUGCHK(0);
	return 0;
}

// Currently services.exe works for IPv4 and IPv6 only.
#define NUM_ADDRESS_FAMILIES               2


DWORD WINAPI ServicesNotifyAddrChangeThread(LPVOID lpv) {
	// For receiving GetAdaptersAddresses info.
	BYTE   bAddrBuf[2048];
	BYTE   *pBuf    = bAddrBuf;
	DWORD  cbBuf    = sizeof(bAddrBuf);
	DWORD  cbNeeded = 0;

	// for setting up async call to winsock2
	int             af[] = {AF_INET, AF_INET6};
	HANDLE          hEvents[NUM_ADDRESS_FAMILIES];
	SOCKET          sockList[NUM_ADDRESS_FAMILIES];
	WSAOVERLAPPED   ov[NUM_ADDRESS_FAMILIES] = {0};
	int             i;
	DWORD           dwNumEvents = 0;
	DWORD           dwTimeout   = INFINITE;

#if defined (DEBUG)
	BOOL           fBreakOnWaitFailure = TRUE;
#endif

	// set up notification events for each address family
	for (i = 0; i < NUM_ADDRESS_FAMILIES; i++) {
		sockList[dwNumEvents] = socket(af[i],SOCK_STREAM,0);

		if (sockList[dwNumEvents] != INVALID_SOCKET) {
			// create event to be used for notifications for this address family
			ov[dwNumEvents].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

			// register notification
			if (ERROR_SUCCESS != WSAIoctl(sockList[dwNumEvents], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &ov[dwNumEvents], NULL) &&
			   ERROR_IO_PENDING != GetLastError()) 
			{
				closesocket(sockList[dwNumEvents]);
				CloseHandle(ov[dwNumEvents].hEvent);
			}
			else 
			{
				hEvents[dwNumEvents] = ov[dwNumEvents].hEvent;
				dwNumEvents++;
			}
		}
	}

	// main loop to receive network table notifications and dispatch them to services.
	while (1) {
		DWORD dw = WaitForMultipleObjects(dwNumEvents, hEvents, FALSE, dwTimeout);

		if (dw == WAIT_FAILED) {
			RETAILMSG(1,(L"SERVICES.EXE: WaitForMultipleObjects in ServicesNotifyAddrChangeThread failed, GLE=0x%08x\r\n",GetLastError()));
			// something strange is going on, because this event should always be valid.
			// In case it's a transient condition just wait for a bit.
#if defined (DEBUG)
			// only break in DBG builds, and only 1st time we hit this to avoid constant interruptions to debuggee.
			if (fBreakOnWaitFailure) {
				DEBUGCHK(0);
				fBreakOnWaitFailure = FALSE;
			}
#endif
			Sleep(SELECT_FAIL_SLEEP_TIME);
			continue;
		}
		else if (dw != WAIT_TIMEOUT) {
			// give time for the network to settle
			// wait for 5 seconds and see if there is another address change

			// reenable notification
			if ((dw - WAIT_OBJECT_0) < dwNumEvents) {
				DEBUGCHK(dw - WAIT_OBJECT_0 < SVSUTIL_ARRLEN(sockList));
				DEBUGCHK(dw - WAIT_OBJECT_0 < SVSUTIL_ARRLEN(ov));

				WSAIoctl(sockList[dw - WAIT_OBJECT_0], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &ov[dw - WAIT_OBJECT_0], NULL);

				dwTimeout = 5000;
				continue;
			}

			// Otherwise we got a totally unexpected error returned from WaitForMultipleObjects().  Complain.
#if defined (DEBUG)
			// only break in DBG builds, and only 1st time we hit this to avoid constant interruptions to debuggee.
			if (fBreakOnWaitFailure) {
				DEBUGCHK(0);
				fBreakOnWaitFailure = FALSE;
			}
#endif
			Sleep(SELECT_FAIL_SLEEP_TIME);
			continue;
		}

		dwTimeout = INFINITE;

		// Call GetAdaptersAddresses(), realloc buffer if needed.
		cbNeeded = cbBuf;
		
		DWORD dwError = pfnGetAdaptersAddresses(AF_UNSPEC,0,NULL,(PIP_ADAPTER_ADDRESSES)pBuf,&cbNeeded);
		if (dwError == ERROR_BUFFER_OVERFLOW) {
			DEBUGCHK(cbNeeded > sizeof(bAddrBuf));
			BYTE *pNewBuf;
			
			if (pBuf != bAddrBuf) {
				DEBUGCHK(cbNeeded > cbBuf);
				pNewBuf = (BYTE*)LocalReAlloc(pBuf,cbNeeded,LMEM_MOVEABLE);
			}
			else {
				pNewBuf = (BYTE*)LocalAlloc(LMEM_MOVEABLE,cbNeeded);
			}

			if (!pNewBuf) {
				RETAILMSG(1,(L"SERVICS.EXE: Could not alloc %d bytes for GetAdaptersAddresses.  No notifications to services.  Sleeping in case this is transient condition.\r\n",cbNeeded));
				Sleep(SELECT_FAIL_SLEEP_TIME);
				continue;
			}
			pBuf  = pNewBuf;
			cbBuf = cbNeeded;

			dwError = pfnGetAdaptersAddresses(AF_UNSPEC,0,NULL,(PIP_ADAPTER_ADDRESSES)pBuf,&cbNeeded);
		}

		if (dwError != ERROR_SUCCESS) {
			RETAILMSG(1,(L"SERVICS.EXE: GetAdaptersAddresses failed, GLE=0x%08x.  Sleeping in case this is transient condition.\r\n",GetLastError()));
			Sleep(SELECT_FAIL_SLEEP_TIME);
			continue;
		}

		// tell all services about it.
		NotifyAllServices(IOCTL_SERVICE_NOTIFY_ADDR_CHANGE,pBuf,cbNeeded);
	}

	// no fall-through.
	DEBUGCHK(0);
	return 0;
}

