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

    scmain.cxx

Abstract:

    Small client driver


--*/
#include <sc.hxx>

#include <mq.h>
#include <scapi.h>

#include <scqman.hxx>
#include <scsman.hxx>
#include <scoverio.hxx>
#include <scqueue.hxx>
#include <scpacket.hxx>
#include <scorder.hxx>
#include <scsrmp.hxx>

#include <service.h>

MachineParameters    *gMachine           = NULL;
GlobalMemory         *gMem               = NULL;
ScQueueManager       *gQueueMan          = NULL;
ScSessionManager     *gSessionMan        = NULL;
ScOverlappedSupport  *gOverlappedSupport = NULL;
ScSequenceCollection *gSeqMan            = NULL;
int gfCallerExecutable                   = SERVICE_CALLER_PROCESS_DEVICE_EXE;

HANDLE ghStartThread = 0;     // administration thread

int fApiInitialized = FALSE;
unsigned int gMemCount = 0;
long gfInitStarted = FALSE;

static int scmain_InitializeGlobalSubsystems (void) {
    static sfSubsystemsInitialized = FALSE;

    if (sfSubsystemsInitialized)
        return TRUE;

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Initializing global subsystems...\n");
#endif

    WSADATA wsd;

    int err = WSAStartup (MAKEWORD(1,1), &wsd);

    if (err != 0) {
        scerror_Complain (MSMQ_SC_ERRMSG_NOSOCKETS, err);
        return FALSE;
    }

    sfSubsystemsInitialized = TRUE;

    return TRUE;
}

#define LANA_UP_FL            0x01

static void scsmain_NetChangeHook(unsigned char lananum, int flags, int unused) {
	if (! gMem)
		return;

	gMem->Lock ();
	int iIntf = scutil_IsLocalTCP (NULL);	// This updates the IP table
	gMem->Unlock ();

	if (iIntf == 1)
		SetEvent (ScSessionManager::hNetUP);

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_SESSION, L"NETWORK change (%d interfaces)!!!\n");
#endif
}

static int scmain_InitializeNetworkTracker (void) {
    static int fAlreadyInitialized = FALSE;

    if (fAlreadyInitialized)
        return TRUE;

    if (! gMachine->fNetworkTracking)
        return TRUE;

    fAlreadyInitialized = TRUE;

#if defined (winCE)
#if defined(USE_IPHELPER_INTERFACE)
    scce_RegisterNET((LPVOID)scsmain_NetChangeHook);
#elif ! defined (MSMQ_ANCIENT_OS)
    if (gfCallerExecutable == SERVICE_CALLER_PROCESS_DEVICE_EXE)
        scce_RegisterNET ((LPVOID)scsmain_NetChangeHook);
#endif
#endif

    return fAlreadyInitialized;
}

static ROUTER_LIST *scmain_GetRoutingInfo (HKEY hk) {
    ROUTER_LIST *pList = NULL;

    for (DWORD dwIndex = 0 ; ; ++dwIndex) {
        WCHAR szValueName[_MAX_PATH];
        DWORD cchValueName = _MAX_PATH;
        DWORD dwType;
        WCHAR szRouterQueueName[_MAX_PATH];
        DWORD cbRouterNameSize = sizeof(szRouterQueueName);

        szRouterQueueName[0] = L'\0';

        LONG res = RegEnumValue (hk, dwIndex, szValueName, &cchValueName, NULL, &dwType, (LPBYTE)szRouterQueueName, &cbRouterNameSize);
        if (res == ERROR_NO_MORE_ITEMS)
            break;

        if ((res != ERROR_SUCCESS) || (dwType != REG_SZ) || (cbRouterNameSize > sizeof(szRouterQueueName) - 2) || (cbRouterNameSize & 1))
            continue;

        szRouterQueueName[cbRouterNameSize/2] = '\0';

        ROUTER_LIST *pNew = NULL;
        GUID guid;

        WCHAR *szEOL = (WCHAR *)scutil_ParseGuidString (szValueName, &guid);

        if ((szEOL == NULL) || (*szEOL != '\0')) {    // string
            unsigned char cb = offsetof (ROUTER_LIST, uri) + (wcslen (szValueName) + 1) * sizeof(WCHAR);
            pNew = (ROUTER_LIST *)g_funcAlloc (cb, g_pvAllocData);
            if (! pNew)
                continue;

			pNew->uiFlags = 0;

			WCHAR *p = wcschr (szValueName, '*');

			if (p) {
				pNew->fWildCard = TRUE;
				pNew->cUriLen = p - szValueName;
			}

            wcscpy (pNew->uri, szValueName);
        } else {                                    // GUID
            pNew = (ROUTER_LIST *)g_funcAlloc (sizeof(ROUTER_LIST), g_pvAllocData);
            if (! pNew)
                continue;

			pNew->uiFlags = 0;

            pNew->fGUID = TRUE;
            pNew->guid = guid;
        }

        pNew->pNext = pList;
        pList = pNew;

        if (szRouterQueueName[0] != L'\0')
            pNew->szFormatName = svsutil_StringHashAlloc (gMem->pStringHash, szRouterQueueName);
        else
            pNew->szFormatName = NULL;
    }

    return pList;
}

static int scmain_LoadGlobalParameters (void) {
    //
    //    Check ever more global things
    //
    if (! scmain_InitializeGlobalSubsystems ())
        return FALSE;

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Loading global parameters...\n");
#endif

    if (! gMem)
        gMem = new GlobalMemory;

    if ((! gMem) || (! gMem->fInitialized))
        return FALSE;

    gMem->Lock ();

    if (fApiInitialized) {
        gMem->Unlock ();
        return FALSE;
    }

#if defined (SC_COUNT_MEMORY)
    gMemCount = svsutil_TotalAlloc ();
#endif

    gMachine = new MachineParameters;

    if (! gMachine) {
        scerror_Complain (MSMQ_SC_ERRMSG_OUTOFMEMORY);
        gMem->Unlock();
        return FALSE;
    }

    memset (gMachine, 0, sizeof (*gMachine));

    gMachine->hLibLPCRT = LoadLibrary (L"lpcrt.dll");

    if (gMachine->hLibLPCRT) {
#if defined (winCE)
        gMachine->CeGenerateGUID = (CeGenerateGUID_t)GetProcAddress (gMachine->hLibLPCRT, L"CeGenerateGUID");
#else
        gMachine->CeGenerateGUID = (CeGenerateGUID_t)GetProcAddress (gMachine->hLibLPCRT, "CeGenerateGUID");
#endif
	}

	//
	// Reset cached local IP table
	//
	scutil_IsLocalTCP (NULL);

    //
    //    First, deal with registry
    //
    HKEY hKey;
    LONG hr = RegOpenKeyEx (HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey);

    if (hr != ERROR_SUCCESS) {
        scerror_Complain (MSMQ_SC_ERRMSG_CANTOPENREGISTRY, hr);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();

        return FALSE;
    }

    HKEY hk2;
    if (ERROR_SUCCESS == RegOpenKeyEx (hKey, L"RouteTo", 0, KEY_READ, &hk2)) {
        gMachine->pRouteTo = scmain_GetRoutingInfo (hk2);
        RegCloseKey (hk2);
    }

    if (ERROR_SUCCESS == RegOpenKeyEx (hKey, L"RouteFrom", 0, KEY_READ, &hk2)) {
        gMachine->pRouteFrom = scmain_GetRoutingInfo (hk2);
        RegCloseKey (hk2);
    }

    if (ERROR_SUCCESS == RegOpenKeyEx (hKey, L"RouteLocal", 0, KEY_READ, &hk2)) {
        gMachine->pRouteLocal = scmain_GetRoutingInfo (hk2);
        RegCloseKey (hk2);
    }

    DWORD    dwType;
    DWORD    dwSize = sizeof(gMachine->uiPort);

    hr = RegQueryValueEx (hKey, L"Port", NULL, &dwType, (LPBYTE)&gMachine->uiPort, &dwSize);

    if ((hr != ERROR_SUCCESS) || (dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiPort))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("Port"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiPingPort);

    hr = RegQueryValueEx (hKey, L"PingPort", NULL, &dwType, (LPBYTE)&gMachine->uiPingPort, &dwSize);

    if ((hr != ERROR_SUCCESS) || (dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiPingPort))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("PingPort"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiDefaultOutQuotaK);

    hr = RegQueryValueEx (hKey, L"DefaultQuota", NULL, &dwType, (LPBYTE)&gMachine->uiDefaultOutQuotaK, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiDefaultOutQuotaK = MSMQ_SC_DEFAULT_OUTGOING_QUOTA;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiDefaultOutQuotaK))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("DefaultQuota"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiDefaultInQuotaK);

    hr = RegQueryValueEx (hKey, L"DefaultLocalQuota", NULL, &dwType, (LPBYTE)&gMachine->uiDefaultInQuotaK, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiDefaultInQuotaK = (unsigned long)MSMQ_SC_DEFAULT_INCOMING_QUOTA;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiDefaultInQuotaK))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("DefaultLocalQuota"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiMachineQuotaK);

    hr = RegQueryValueEx (hKey, L"MachineQuota", NULL, &dwType, (LPBYTE)&gMachine->uiMachineQuotaK, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiMachineQuotaK = (unsigned long)MSMQ_SC_DEFAULT_MACHINE_QUOTA;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiMachineQuotaK))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("MachineQuota"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiOrderAckWindow);

    hr = RegQueryValueEx (hKey, L"OrderedAckWindow", NULL, &dwType, (LPBYTE)&gMachine->uiOrderAckWindow, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiOrderAckWindow = MSMQ_SC_DEFAULT_ORDERWINDOW;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiOrderAckWindow))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("OrderedAckWindow"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiOrderAckScale);

    hr = RegQueryValueEx (hKey, L"OrderedAckScale", NULL, &dwType, (LPBYTE)&gMachine->uiOrderAckScale, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiOrderAckScale = MSMQ_SC_DEFAULT_ORDERACKSCALE;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiOrderAckScale))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("OrderedAckScale"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(gMachine->uiPingTimeout);

    hr = RegQueryValueEx (hKey, L"PingTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiPingTimeout, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiPingTimeout = MSMQ_SC_DEFAULT_PINGTIMEOUT;
    else if ((dwType != REG_DWORD) || (dwSize != sizeof(gMachine->uiPingTimeout))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("PingTimeout"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    dwSize = sizeof(GUID);

    hr = RegQueryValueEx (hKey, L"QueueManagerGUID", NULL, &dwType, (LPBYTE)&gMachine->guid, &dwSize);

    if ((hr != ERROR_SUCCESS) || (dwType != REG_BINARY) || (dwSize != sizeof(GUID))) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("QueueManagerGUID"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    WCHAR szOutFRSFormat[2 * _MAX_PATH];
    dwSize = sizeof(szOutFRSFormat);

    hr = RegQueryValueEx (hKey, L"OutFRSQueue", NULL, &dwType, (LPBYTE)szOutFRSFormat, &dwSize);

    if ((hr == ERROR_SUCCESS) && (dwType == REG_SZ) && (szOutFRSFormat[0] != L'\0'))
        gMachine->lpszOutFRSQueueFormatName = svsutil_wcsdup (szOutFRSFormat);

    WCHAR szDebugFormat[2 * _MAX_PATH];
    dwSize = sizeof(szDebugFormat);

    hr = RegQueryValueEx (hKey, L"DebugQueue", NULL, &dwType, (LPBYTE)szDebugFormat, &dwSize);

    if ((hr == ERROR_SUCCESS) && (dwType == REG_SZ) && (szDebugFormat[0] != L'\0'))
        gMachine->lpszDebugQueueFormatName = svsutil_wcsdup (szDebugFormat);

    WCHAR szDirectoryPath[_MAX_PATH];
    dwSize = sizeof(szDirectoryPath);

    hr = RegQueryValueEx (hKey, L"BaseDir", NULL, &dwType, (LPBYTE)szDirectoryPath, &dwSize);

    if ((hr != ERROR_SUCCESS) || (dwType != REG_SZ) || (szDirectoryPath[0] == L'\0')) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("BaseDir"), hr);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    int ccDirLen = wcslen (szDirectoryPath);

    while ((ccDirLen > 1) && szDirectoryPath[ccDirLen-1] == L'\\')
        szDirectoryPath[--ccDirLen] = L'\0';

    int iMaxRetry = -1;
    dwSize = sizeof(iMaxRetry);

    hr = RegQueryValueEx (hKey, L"FSMaxTimeout", NULL, &dwType, (BYTE *)&iMaxRetry, &dwSize);
    if ((hr != ERROR_SUCCESS) || (iMaxRetry < 0) || (dwType != REG_DWORD))
        iMaxRetry = MSMQ_SC_DEFAULT_FSTIMEOUT;

    DWORD fAttr = GetFileAttributes(szDirectoryPath);

    for (int iCount = 0 ; (iCount < iMaxRetry) && (fAttr == 0xFFFFFFFF) ; ++iCount, (fAttr = GetFileAttributes(szDirectoryPath)))
        Sleep(1000);

    if ((fAttr == 0xFFFFFFFF) || (! (fAttr & FILE_ATTRIBUTE_DIRECTORY)) ||
                        (fAttr & FILE_ATTRIBUTE_READONLY)) {
        scerror_Complain (MSMQ_SC_ERRMSG_INVALIDDIR, szDirectoryPath);
        RegCloseKey (hKey);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    gMachine->lpszDirName = svsutil_wcsdup (szDirectoryPath);

    int fIPSelected = FALSE;

    WCHAR szHostName[MSMQ_SC_SMALLBUFFER];
    dwSize = sizeof(szHostName);

    hr = RegQueryValueEx (hKey, L"HostName", NULL, &dwType, (LPBYTE)szHostName, &dwSize);
    if (hr == ERROR_SUCCESS) {
        if (dwType != REG_SZ) {
            scerror_Complain (MSMQ_SC_ERRMSG_INVALIDKEY, TEXT("HostName"), hr);
            RegCloseKey (hKey);
            delete gMachine;
            gMachine = NULL;
            gMem->Unlock();
            return FALSE;
        }
        gMachine->lpszHostName = svsutil_wcsdup (szHostName);
        
        fIPSelected = TRUE;
    }

    hr = RegQueryValueEx (hKey, L"MessageID", NULL, &dwType, (LPBYTE)&gMachine->uiStartID, &dwSize);

    if (hr != ERROR_SUCCESS)
        gMachine->uiStartID = 1;
    else
        gMachine->uiStartID += SCQMAN_ID_SAVE_FREQ_MASK + 1;

    RegSetValueEx (hKey, L"MessageID", 0, REG_DWORD, (BYTE *)&gMachine->uiStartID, sizeof(DWORD));

    WCHAR szTrackNet[4];
    szTrackNet[0] = L'\0';
    dwSize = sizeof (szTrackNet);

    hr = RegQueryValueEx (hKey, L"CETrackNetwork", NULL, &dwType, (LPBYTE)szTrackNet, &dwSize);
    if ((hr == ERROR_SUCCESS) && ((szTrackNet[0] == L'y') || (szTrackNet[0] == L'Y')))
        gMachine->fNetworkTracking = TRUE;
    else
        gMachine->fNetworkTracking = FALSE;

    WCHAR szYN[4];
    szYN[0] = L'\0';
    dwSize = sizeof (szYN);

    hr = RegQueryValueEx (hKey, L"UntrustedNetwork", NULL, &dwType, (LPBYTE)szYN, &dwSize);
    if ((hr != ERROR_SUCCESS) || ((szYN[0] != L'n') && (szYN[0] != L'N')))
        gMachine->fUntrustedNetwork = TRUE;
    else
        gMachine->fUntrustedNetwork = FALSE;

    szYN[0] = L'\0';
    dwSize = sizeof (szYN);

    hr = RegQueryValueEx (hKey, L"AllowResponse", NULL, &dwType, (LPBYTE)szYN, &dwSize);
    if ((hr == ERROR_SUCCESS) && ((szYN[0] == L'y') || (szYN[0] == L'Y')))
        gMachine->fResponseByIp = TRUE;
    else
        gMachine->fResponseByIp = FALSE;

    szYN[0] = L'\0';
    dwSize = sizeof (szYN);

    hr = RegQueryValueEx (hKey, L"SrmpEnabled", NULL, &dwType, (LPBYTE)szYN, &dwSize);
    if ((hr == ERROR_SUCCESS) && ((szYN[0] == L'y') || (szYN[0] == L'Y')))
        gMachine->fUseSRMP = g_fHaveSRMP;
    else
        gMachine->fUseSRMP = FALSE;

    szYN[0] = L'\0';
    dwSize = sizeof (szYN);

    hr = RegQueryValueEx (hKey, L"BinaryEnabled", NULL, &dwType, (LPBYTE)szYN, &dwSize);
    if ((hr == ERROR_SUCCESS) && ((szYN[0] == L'y') || (szYN[0] == L'Y')))
        gMachine->fUseBinary = TRUE;
    else if (hr != ERROR_SUCCESS)
        gMachine->fUseBinary = (! gMachine->fUntrustedNetwork);
	else
        gMachine->fUseBinary = FALSE;

    dwSize = sizeof(gMachine->asRetrySchedule);
    hr = RegQueryValueEx (hKey, L"RetrySchedule", NULL, &dwType, (LPBYTE)gMachine->asRetrySchedule, &dwSize);
    if ((hr == ERROR_SUCCESS) && (dwSize <= sizeof(gMachine->asRetrySchedule)) && (dwSize > 0) && ((dwSize & 1) == 0) && (dwType == REG_BINARY))
        gMachine->uiRetrySchedule = dwSize / sizeof(short);
    else {
        gMachine->uiRetrySchedule = SCSESSION_ATTEMPT_NUM;

        gMachine->asRetrySchedule[0] = SCSESSION_ATTEMPT_FIRST;
        gMachine->asRetrySchedule[1] = SCSESSION_ATTEMPT_SECOND;
        gMachine->asRetrySchedule[2] = SCSESSION_ATTEMPT_THIRD;
        gMachine->asRetrySchedule[3] = SCSESSION_ATTEMPT_FOURTH;
        gMachine->asRetrySchedule[4] = SCSESSION_ATTEMPT_FIFTHANDUP;
    }

    dwSize = sizeof(gMachine->uiLanOffDelay);
    hr = RegQueryValueEx (hKey, L"LanRetrySchedule", NULL, &dwType, (LPBYTE)&gMachine->uiLanOffDelay, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiLanOffDelay)) && (dwType == REG_DWORD)))
        gMachine->uiLanOffDelay = SCSESSION_LANAUP_DELAY;

    dwSize = sizeof(gMachine->uiConnectionTimeout);
    hr = RegQueryValueEx (hKey, L"ConnectionTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiConnectionTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiConnectionTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiConnectionTimeout = SCSESSION_INITIAL_TIMEOUT;

    dwSize = sizeof(gMachine->uiIdleTimeout);
    hr = RegQueryValueEx (hKey, L"IdleTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiIdleTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiIdleTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiIdleTimeout = SCSESSION_IDLE_TIMEOUT;

    dwSize = sizeof(gMachine->uiMinAckTimeout);
    hr = RegQueryValueEx (hKey, L"MinAckTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiMinAckTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiMinAckTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiMinAckTimeout = MSMQ_MIN_ACKTIMEOUT;

    dwSize = sizeof(gMachine->uiMaxAckTimeout);
    hr = RegQueryValueEx (hKey, L"MaxAckTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiMaxAckTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiMaxAckTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiMaxAckTimeout = MSMQ_MAX_ACKTIMEOUT;

    dwSize = sizeof(gMachine->uiMinStoreAckTimeout);
    hr = RegQueryValueEx (hKey, L"MinStoreAckTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiMinStoreAckTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiMinStoreAckTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiMinStoreAckTimeout = MSMQ_MIN_STORE_ACKTIMEOUT;


    if (gMachine->uiMaxAckTimeout <= gMachine->uiMinAckTimeout) {
        gMachine->uiMinAckTimeout = MSMQ_MIN_ACKTIMEOUT;
        gMachine->uiMaxAckTimeout = MSMQ_MAX_ACKTIMEOUT;
    }

    dwSize = sizeof(gMachine->uiSeqTimeout);
    hr = RegQueryValueEx (hKey, L"SequenceTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiSeqTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiSeqTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiSeqTimeout = SCSEQUENCE_S_SEQTHRESH;

    dwSize = sizeof(gMachine->uiOrderQueueTimeout);
    hr = RegQueryValueEx (hKey, L"OrderQueueTimeout", NULL, &dwType, (LPBYTE)&gMachine->uiOrderQueueTimeout, &dwSize);
    if (! ((hr == ERROR_SUCCESS) && (dwSize == sizeof(gMachine->uiOrderQueueTimeout)) && (dwType == REG_DWORD)))
        gMachine->uiOrderQueueTimeout = SCSEQUENCE_S_SEQTHRESH_Q;

    RegCloseKey (hKey);

    //
    //    Second, set IP parameters
    //
    if (! fIPSelected) {
#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_INIT, L"IP parameters are not set in registry. Querying network for host name and IP address...\n");
#endif

        char cszBuffer[MSMQ_SC_SMALLBUFFER];

        if (gethostname (cszBuffer, MSMQ_SC_SMALLBUFFER)) {
            scerror_Complain (MSMQ_SC_ERRMSG_NETWORKERROR);
            delete gMachine;
            gMachine = NULL;
            gMem->Unlock();
            return FALSE;
        }

        if (gMachine->lpszHostName)
            free (gMachine->lpszHostName);

        int iWStrLen = MultiByteToWideChar (CP_ACP, 0, cszBuffer, -1, NULL, 0) + 1;
        SVSUTIL_ASSERT (iWStrLen > 0);
        gMachine->lpszHostName = (WCHAR *)g_funcAlloc (iWStrLen * sizeof(WCHAR), g_pvAllocData);
        MultiByteToWideChar (CP_ACP, 0, cszBuffer, -1, gMachine->lpszHostName, iWStrLen);

        HOSTENT *phe = gethostbyname (cszBuffer);
        if (! phe) {
            scerror_Complain (MSMQ_SC_ERRMSG_NETWORKERROR);
            delete gMachine;
            gMachine = NULL;
            gMem->Unlock();
            return FALSE;
        }

        if ((phe->h_addrtype != AF_INET) || (phe->h_length != sizeof (IN_ADDR)) ||
            (*phe->h_addr_list == NULL)) {
            scerror_Complain (MSMQ_SC_ERRMSG_NONIPNETWORK, phe->h_addrtype);
            delete gMachine;
            gMachine = NULL;
            gMem->Unlock();
            return FALSE;
        }
    }

    if (! scmain_InitializeNetworkTracker ()) {
        scerror_Complain (MSMQ_SC_ERRMSG_NONETWORKTRACK);
        delete gMachine;
        gMachine = NULL;
        gMem->Unlock();
        return FALSE;
    }

    if (gMachine->fUseSRMP)
    	SrmpInit();

    return TRUE;
}

void scmain_GeneralCleanup (BOOL fReInitialize) {
    SVSUTIL_ASSERT (gMem && gMem->IsLocked());

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Queue manager cleanup...\n");
#endif

    fApiInitialized = FALSE;

    //
    //    The only guarantee that thread termination will not leave the system
    //    in inconsistent state is obtaining all the locks.
    //
    if (gQueueMan)
        gQueueMan->Stop ();

    if (gSessionMan)
        gSessionMan->Stop ();

    //
    //    Then, we need to delete all queues
    //
    if (gQueueMan) {
        gQueueMan->pQueueDLQ      = NULL;
        gQueueMan->pQueueJournal  = NULL;
        gQueueMan->pQueueOrderAck = NULL;
        gQueueMan->pQueueOutFRS   = NULL;

#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_INIT, L"Deleting queue memory images...\n");
#endif

        while (gQueueMan->pqlIncoming)
            gQueueMan->DeleteQueue (gQueueMan->pqlIncoming->pQueue, FALSE);

        while (gQueueMan->pqlOutgoing)
            gQueueMan->DeleteQueue (gQueueMan->pqlOutgoing->pQueue, FALSE);

    }
    //
    //    Finally, the only thing we need is to 
    //
    if (gSessionMan)
        delete gSessionMan;

    gSessionMan = NULL;

    if (gQueueMan)
        delete gQueueMan;

    gQueueMan = NULL;

    if (gSeqMan)
        delete gSeqMan;

    gSeqMan = NULL;

    if (gOverlappedSupport)
        delete gOverlappedSupport;

    gOverlappedSupport = NULL;

    if (gMachine)
        delete gMachine;

    gMachine = NULL;

    gMem->Cycle (fReInitialize);
}

int scmain_Init (void) {
    if (fApiInitialized)
        return FALSE;

#if defined (SC_VERBOSE)
    scerror_DebugInitialize ();
#endif

    //
    //    Record network parameters etc.
    //
    if (! scmain_LoadGlobalParameters ())
        return FALSE;


#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Initializing queue manager for local machine...\n");
#endif

    gQueueMan = new ScQueueManager (gMachine->uiStartID);

    if (! gQueueMan) {
        scerror_Complain (MSMQ_SC_ERRMSG_FAILQM);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    gSessionMan = new ScSessionManager ();

    if ((! gSessionMan) || (! gSessionMan->fInitialized)) {
        scerror_Complain (MSMQ_SC_ERRMSG_FAILSM);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    gOverlappedSupport = new ScOverlappedSupport;

    if (! gOverlappedSupport) {
        scerror_Complain (MSMQ_SC_ERRMSG_FAILOV);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    gSeqMan = new ScSequenceCollection;

    if (! gSeqMan) {
        scerror_Complain (MSMQ_SC_ERRMSG_FAILORDER);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    //
    //    Load whatever is in a directory
    //
    WIN32_FIND_DATA    wfd;
    WCHAR szFullName[_MAX_PATH];
    int iDirLen = wcslen (gMachine->lpszDirName);

    SVSUTIL_ASSERT (iDirLen > 0);
    if (iDirLen > _MAX_PATH - 4) {
        scerror_Complain (MSMQ_SC_ERRMSG_NAMETOOLONG, gMachine->lpszDirName);
        scmain_GeneralCleanup (TRUE);

        gMem->Unlock ();

        return FALSE;
    }

    memcpy (szFullName, gMachine->lpszDirName, sizeof(WCHAR) * iDirLen);

    szFullName[iDirLen] = L'\\';
    ++iDirLen;

    wcscpy (&szFullName[iDirLen], MSMQ_SC_SEARCH_PATTERN);

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Querying directory %s for queue storage files.\n", szFullName);
#endif

    HANDLE hSearch = FindFirstFile (szFullName, &wfd);

    int fError = FALSE;

    if (hSearch != INVALID_HANDLE_VALUE) {
        do {
            int iFileNameLen = wcslen (wfd.cFileName) + 1;
            if (iDirLen + iFileNameLen >= _MAX_PATH) {
                scerror_Complain (MSMQ_SC_ERRMSG_NAMETOOLONG, wfd.cFileName);
                fError = TRUE;
                break;
            }
            memcpy (&szFullName[iDirLen], wfd.cFileName, sizeof(WCHAR) * (iFileNameLen + 1));

#if defined (SC_VERBOSE)
            scerror_DebugOut (VERBOSE_MASK_INIT, L"Processing %s...\n", szFullName);
#endif

            ScQueue *pQueue = gQueueMan->MakeQueueFromFile (szFullName, NULL);

            if (! pQueue) {
                scerror_Complain (MSMQ_SC_ERRMSG_FILECORRUPT, szFullName);

                fError = TRUE;
                break;
            }

            if (pQueue->qp.bIsDeadLetter) {
                if (gQueueMan->pQueueDLQ  || pQueue->qp.bHasJournal || pQueue->qp.bTransactional || pQueue->qp.bAuthenticate || 
                    pQueue->qp.bIsInternal || (! pQueue->qp.bIsProtected) || (! pQueue->qp.bIsIncoming) ||
                    pQueue->qp.bIsOrderAck || pQueue->qp.bIsOutFRS || pQueue->qp.bIsJournal || pQueue->qp.bIsMachineJournal) {
                    scerror_Complain (MSMQ_SC_ERRMSG_FILECORRUPT, szFullName);

                    fError = TRUE;
                    break;
                }

                gQueueMan->pQueueDLQ = pQueue;

            } else if (pQueue->qp.bIsOrderAck && pQueue->qp.bIsIncoming) {
                if (gQueueMan->pQueueOrderAck  || pQueue->qp.bHasJournal || pQueue->qp.bTransactional || pQueue->qp.bAuthenticate || 
                    (! pQueue->qp.bIsInternal) || (! pQueue->qp.bIsProtected) ||
                    pQueue->qp.bIsDeadLetter || pQueue->qp.bIsOutFRS || pQueue->qp.bIsJournal || pQueue->qp.bIsMachineJournal || 
                    (! pQueue->qp.bIsIncoming)) {
                    scerror_Complain (MSMQ_SC_ERRMSG_FILECORRUPT, szFullName);

                    fError = TRUE;
                    break;
                }

                gQueueMan->pQueueOrderAck = pQueue;

            } else if (pQueue->qp.bIsOutFRS) {
                if (gQueueMan->pQueueOutFRS  || pQueue->qp.bHasJournal || pQueue->qp.bTransactional || pQueue->qp.bAuthenticate || 
                    pQueue->qp.bIsInternal || (! pQueue->qp.bIsProtected) || pQueue->qp.bIsIncoming ||
                    pQueue->qp.bIsOrderAck || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsJournal || pQueue->qp.bIsMachineJournal) {
                    scerror_Complain (MSMQ_SC_ERRMSG_FILECORRUPT, szFullName);

                    fError = TRUE;
                    break;
                }

                gQueueMan->pQueueOutFRS = pQueue;
            } else if (pQueue->qp.bIsMachineJournal) {
                if (gQueueMan->pQueueJournal  || pQueue->qp.bHasJournal || pQueue->qp.bTransactional || pQueue->qp.bAuthenticate || 
                    pQueue->qp.bIsInternal || (! pQueue->qp.bIsProtected) || (! pQueue->qp.bIsIncoming) ||
                    pQueue->qp.bIsOrderAck || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsJournal || pQueue->qp.bIsOutFRS) {
                    scerror_Complain (MSMQ_SC_ERRMSG_FILECORRUPT, szFullName);

                    fError = TRUE;
                    break;
                }

                gQueueMan->pQueueJournal = pQueue;
            }

        } while (FindNextFile (hSearch, &wfd));
        FindClose (hSearch);
    }

    if ((! fError) && (! gQueueMan->pQueueDLQ)) {
        ScQueueParms    qp;

        memset (&qp, 0, sizeof(qp));
        qp.uiPrivacyLevel = MQ_PRIV_LEVEL_NONE;
        qp.uiQuotaK       = (unsigned int)-1;
        qp.bIsIncoming    = TRUE;
        qp.bIsDeadLetter  = TRUE;
        qp.bIsProtected   = TRUE;

        WCHAR szPathName[_MAX_PATH];
        wsprintf (szPathName, L".\\private$\\" SC_GUID_FORMAT, SC_GUID_ELEMENTS((&gMachine->guid)));
        WCHAR szLabel[_MAX_PATH];
        wsprintf (szLabel, L"Dead Letter Queue for " SC_GUID_FORMAT, SC_GUID_ELEMENTS((&gMachine->guid)));

#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_INIT, L"Dead letter queue not found. Creating using pathname %s\n", szPathName);
#endif

        HRESULT hr = MQ_OK;
        gQueueMan->pQueueDLQ = gQueueMan->MakeIncomingQueue (szPathName, szLabel, &qp, 0, &hr);
        if (! gQueueMan->pQueueDLQ) {
            scerror_Complain (MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE, szPathName, hr);
            fError = TRUE;
        }
    }

    if ((! fError) && (! gQueueMan->pQueueJournal)) {
        ScQueueParms    qp;

        memset (&qp, 0, sizeof(qp));
        qp.uiPrivacyLevel    = MQ_PRIV_LEVEL_NONE;
        qp.uiQuotaK          = (unsigned int)-1;
        qp.bIsIncoming       = TRUE;
        qp.bIsMachineJournal = TRUE;
        qp.bIsProtected      = TRUE;

        WCHAR szPathName[_MAX_PATH];
        wsprintf (szPathName, L".\\private$\\" SC_GUID_FORMAT, SC_GUID_ELEMENTS((&gMachine->guid)));
        WCHAR szLabel[_MAX_PATH];
        wsprintf (szLabel, L"Machine Journal Queue for " SC_GUID_FORMAT, SC_GUID_ELEMENTS((&gMachine->guid)));

#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_INIT, L"Machine journal queue not found. Creating using pathname %s\n", szPathName);
#endif

        HRESULT hr = MQ_OK;
        gQueueMan->pQueueJournal = gQueueMan->MakeIncomingQueue (szPathName, szLabel, &qp, 0, &hr);
        if (! gQueueMan->pQueueJournal) {
            scerror_Complain (MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE, szPathName, hr);
            fError = TRUE;
        }
    }

    if ((! fError) && (! gQueueMan->pQueueOrderAck)) {
        ScQueueParms    qp;

        memset (&qp, 0, sizeof(qp));
        qp.uiPrivacyLevel = MQ_PRIV_LEVEL_NONE;
        qp.uiQuotaK       = (unsigned int)-1;
        qp.bIsIncoming    = TRUE;
        qp.bIsOrderAck    = TRUE;
        qp.bIsProtected   = TRUE;
        qp.bIsInternal    = TRUE;

        WCHAR szPathName[_MAX_PATH];
        wcscpy (szPathName, L".\\" MSMQ_SC_PATHNAME_PRIVATE L"\\" MSMQ_SC_ORDERQUEUENAME);
        WCHAR szLabel[_MAX_PATH];
        wsprintf (szLabel, L"Order ACK Queue for " SC_GUID_FORMAT, SC_GUID_ELEMENTS((&gMachine->guid)));

#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_INIT, L"Order ACK queue not found. Creating using pathname %s\n", szPathName);
#endif

        HRESULT hr = MQ_OK;
        gQueueMan->pQueueOrderAck = gQueueMan->MakeIncomingQueue (szPathName, szLabel, &qp, 0, &hr);
        if (! gQueueMan->pQueueOrderAck) {
            scerror_Complain (MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE, szPathName, hr);
            fError = TRUE;
        }
    }

    if (! fError) {
        if (gQueueMan->pQueueOutFRS && ((! gMachine->lpszOutFRSQueueFormatName) ||
            (wcsicmp (gQueueMan->pQueueOutFRS->lpszFormatName, gMachine->lpszOutFRSQueueFormatName) != 0))) {
            gQueueMan->pQueueOutFRS->qp.bIsProtected = FALSE;
            gQueueMan->pQueueOutFRS->qp.bIsOutFRS    = FALSE;
            gQueueMan->pQueueOutFRS->UpdateFile ();
            gQueueMan->pQueueOutFRS = NULL;        // But let it flush...
        }

        if (gMachine->lpszOutFRSQueueFormatName && (! gQueueMan->pQueueOutFRS)) {
            ScQueueParms    qp;

            memset (&qp, 0, sizeof(qp));
            qp.uiPrivacyLevel = MQ_PRIV_LEVEL_NONE;
            qp.uiQuotaK       = (unsigned int)-1;
            qp.bIsOutFRS      = TRUE;
            qp.bIsProtected   = TRUE;

#if defined (SC_VERBOSE)
            scerror_DebugOut (VERBOSE_MASK_INIT, L"Out-FRS queue not found. Creating using formatname %s\n", gMachine->lpszOutFRSQueueFormatName);
#endif

            HRESULT hr = MQ_OK;
            gQueueMan->pQueueOutFRS = gQueueMan->MakeOutgoingQueue (gMachine->lpszOutFRSQueueFormatName, &qp, &hr);
            if (! gQueueMan->pQueueOutFRS) {
                scerror_Complain (MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE, gMachine->lpszOutFRSQueueFormatName, hr);
                fError = TRUE;
            }
        }
    }

    if (fError) {
        scerror_Complain (MSMQ_SC_ERRMSG_INITSHUTDOWN);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    if (! gSeqMan->Load()) {
        scerror_Complain (MSMQ_SC_ERRMSG_FAILORDER);
        scmain_GeneralCleanup (TRUE);
        gMem->Unlock ();
        return FALSE;
    }

    gSessionMan->Start ();
    gQueueMan->Start ();

    fApiInitialized = TRUE;
    glServiceState = SERVICE_STATE_ON;

    gMem->Unlock ();

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Initialization of queue manager complete...\n");
#endif
    scerror_Inform (MSMQ_SC_ERRMSG_MSMQSTARTED);

    return TRUE;
}



DWORD WINAPI scmain_ShutdownThread (LPVOID lpv) {
	BOOL fReInitialize = (BOOL) lpv;
    if (! fApiInitialized)
        return FALSE;

    glServiceState = SERVICE_STATE_SHUTTING_DOWN;

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Commencing queue manager shutdown...\n");
#endif

    //shut down the tracker thread
#if defined (winCE) && defined(USE_IPHELPER_INTERFACE)
        scce_UnregisterNET ();
#endif

    //
    //    Wait for all systems to enter stable shutdownable state
    //
    gMem->Lock ();

    for (int i = 0 ; i < 10 ; ++i) {
        if (((! gOverlappedSupport) || (! gOverlappedSupport->fBusy)) &&
            ((! gQueueMan) || (! gQueueMan->fBusy)) &&
            ((! gSessionMan) || (! gSessionMan->fBusy)) &&
            ((! gSeqMan) || (! gSeqMan->fBusy)))
            break;

        gMem->Unlock ();
        Sleep(100);
        gMem->Lock ();
    }

    if (! fApiInitialized) {
        gMem->Unlock ();
        return FALSE;
    }

    if (i == 10)
        scerror_Inform (MSMQ_SC_ERRMSG_SHUTDOWNUNSTABLE);

    fApiInitialized = FALSE;
    gfInitStarted = FALSE;

    scmain_GeneralCleanup (fReInitialize);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"On startup: %d bytes, on shutdown: %d bytes\n", gMemCount, svsutil_TotalAlloc());
#endif
    gMem->Unlock ();

#if defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Queue manager shutdown complete...\n");
#endif

    scerror_Inform (MSMQ_SC_ERRMSG_MSMQSTOPPED);
    glServiceState = SERVICE_STATE_OFF;

    return TRUE;
}

int scmain_Shutdown(BOOL fReInitialize) {
	DWORD dwRet = 0; 

	// need to spin a thread in the event we're shutting down Wininet because of TLS issues.
	HANDLE h = CreateThread(NULL, 0, scmain_ShutdownThread, (LPVOID)fReInitialize, 0, NULL);
	if (h) {
		WaitForSingleObject(h,INFINITE);
		GetExitCodeThread(h,&dwRet);
		CloseHandle(h);
	}
	return dwRet;
}

GlobalMemory::GlobalMemory (void) {
    pPacketMem   = svsutil_AllocFixedMemDescr (sizeof (ScPacket),   SCPACKET_PACKETS_PER_BLOCK);
    pTreeNodeMem = svsutil_AllocFixedMemDescr (sizeof (SVSTNode),   SVSUTIL_TREE_INITIAL);
    pAckNodeMem  = svsutil_AllocFixedMemDescr (sizeof (SentPacket), SVSUTIL_TREE_INITIAL);

    pStringHash  = svsutil_GetStringHash (0, FALSE);

    pTimeoutTree = new SVSTree (pTreeNodeMem);

    pTimer       = svsutil_AllocAttrTimer ();

    fInitialized = pTimer && pTimeoutTree && pStringHash && pAckNodeMem && pTreeNodeMem && pPacketMem;
}

void GlobalMemory::Cycle (BOOL fReInitialize) {
    Lock ();

    //
    //        Free all resources
    //
#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
	unsigned int uiBase = svsutil_TotalAlloc ();
#endif

    delete pTimeoutTree;

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Timeout tree: %d bytes freed\n", uiBase - svsutil_TotalAlloc ());

	unsigned int uiFree, uiTotal;
#endif

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
	uiFree = uiTotal = 0;
	svsutil_GetFixedStats (pPacketMem, &uiFree, &uiTotal);
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Packet heap: %d blocks not freed out of %d total\n", uiTotal - uiFree, uiTotal);
#endif

    svsutil_ReleaseFixedNonEmpty (pPacketMem);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
	uiFree = uiTotal = 0;
	svsutil_GetFixedStats (pTreeNodeMem, &uiFree, &uiTotal);
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Tree nodes: %d blocks not freed out of %d total\n", uiTotal - uiFree, uiTotal);
#endif

	svsutil_ReleaseFixedNonEmpty (pTreeNodeMem);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
	uiFree = uiTotal = 0;
	svsutil_GetFixedStats (pAckNodeMem, &uiFree, &uiTotal);
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Ack nodes: %d blocks not freed out of %d total\n", uiTotal - uiFree, uiTotal);
#endif

    svsutil_ReleaseFixedNonEmpty (pAckNodeMem);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
	uiBase = svsutil_TotalAlloc ();
#endif

    svsutil_FreeAttrTimer        (pTimer);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"Attribute timer: %d bytes freed\n", uiBase - svsutil_TotalAlloc ());

	unsigned int uiEntries = 0;
	svsutil_GetStringHashStats (pStringHash, &uiEntries);
    scerror_DebugOut (VERBOSE_MASK_INIT, L"String hash entries: %d unfreed\n", uiEntries);
#endif

    svsutil_DestroyStringHash    (pStringHash);

#if defined (SC_COUNT_MEMORY) && defined (SC_VERBOSE)
    scerror_DebugOut (VERBOSE_MASK_INIT, L"lowest memory on shutdown: %d bytes\n", svsutil_TotalAlloc());
#endif

    //
    //    And then rebuild anew...
    //
    if (fReInitialize) {
        pPacketMem   = svsutil_AllocFixedMemDescr (sizeof (ScPacket),   SCPACKET_PACKETS_PER_BLOCK);
        pTreeNodeMem = svsutil_AllocFixedMemDescr (sizeof (SVSTNode),   SVSUTIL_TREE_INITIAL);
        pAckNodeMem  = svsutil_AllocFixedMemDescr (sizeof (SentPacket), SVSUTIL_TREE_INITIAL);

        pStringHash  = svsutil_GetStringHash (0, FALSE);

        pTimeoutTree = new SVSTree (pTreeNodeMem);

        pTimer       = svsutil_AllocAttrTimer ();
    }
    Unlock ();
}

extern "C" int wmain(int argc, WCHAR **argv) {
    svsutil_Initialize ();
    g_funcDebugOut = scerror_AssertOut;

    if ((argc > 1) && (wcsicmp (argv[1], L"-start") == 0)) {
        if (! scmain_Init  ())
            return MSMQ_SC_ERR_INITFAILED;

        SVSUTIL_ASSERT (fApiInitialized);
    }

    scapi_EnterInputLoop ();

    if (fApiInitialized) {
        if (! scmain_Shutdown (FALSE))
            return MSMQ_SC_ERR_FINALFAILED;
    }

    return 0;
}

DWORD WINAPI scmain_Startup (LPVOID pvParam) {
    DWORD dwRet = (DWORD)scmain_Init ();
    ghStartThread =0;
    if (dwRet == 0) {
        // on failure we only will set state=OFF in the event that we're starting up.
        // We do this because theoretically we could already be in state=SERVICE_STATE_ON,
        // in which case we'd want to leave the state as-is.
        InterlockedCompareExchange((LONG*)&glServiceState,SERVICE_STATE_OFF,SERVICE_STATE_STARTING_UP);
    }

    return dwRet;
}

extern "C" DWORD WINAPI scmain_StartDLL (LPVOID lpParm) {
#if defined (winCE)

    if (InterlockedExchange (&gfInitStarted, TRUE))
        return FALSE;

    svsutil_Initialize ();
    g_funcDebugOut = scerror_AssertOut;

    SVSUTIL_ASSERT(glServiceState == SERVICE_STATE_OFF);
    glServiceState = SERVICE_STATE_STARTING_UP;

    for ( ; ; ) {
        //
        //    First, deal with registry
        //
        HKEY hKey;
        LONG hr = RegOpenKeyEx (HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey);

        if (hr != ERROR_SUCCESS)
            break;

        DWORD    dwType            = 0;
        DWORD    dwSize            = sizeof(DWORD);
        DWORD   dwStartAtBoot    = FALSE;

        hr = RegQueryValueEx (hKey, L"CEStartAtBoot", NULL, &dwType, (LPBYTE)&dwStartAtBoot, &dwSize);

        if ((hr != ERROR_SUCCESS) || (dwType != REG_DWORD) || (dwSize != sizeof(DWORD)) || (! dwStartAtBoot))
            break;

        DWORD dwTID = 0;
        ghStartThread = CreateThread (NULL, 0, scmain_Startup, NULL, 0, &dwTID);

        if (ghStartThread)
            CloseHandle (ghStartThread);

        break;
    }

    if (!ghStartThread)
        glServiceState = SERVICE_STATE_OFF;

    if (! scce_RegisterDLL ()) {
        glServiceState = SERVICE_STATE_OFF;
        gfInitStarted = FALSE;
        return FALSE;
    }

    return TRUE;
#else
    return FALSE;
#endif
}
extern "C" HRESULT    scmain_StopDLL (void) {
#if defined (winCE)
    if (! scce_UnregisterDLL ())
        return MQ_ERROR;

    return MQ_OK;
#else
    return MQ_ERROR;
#endif
}

int scmain_ForceExit (void) {
    // make sure creation thread isn't running when we try to pull DLL down.
    if (ghStartThread)
        WaitForSingleObject(ghStartThread,INFINITE);

    // there's the possible condition scmain_Startup sets ghStartThread=NULL but
    // hasn't exited.
    Sleep(100); 
    scmain_Shutdown (FALSE);
    scmain_StopDLL ();
#if defined (winNT)
    RpcMgmtStopServerListening (NULL);
#endif
    return TRUE;
}
