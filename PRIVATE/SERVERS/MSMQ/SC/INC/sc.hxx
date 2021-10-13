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

    sc.hxx

Abstract:

    Small client global header file


  Important note about lock hierarchy:
		QueueManager -> SessionManager -> Session -> Main Queue(s) -> Dead letter/journal(s) -> ACK/NACK queue

--*/
#if ! defined (__sc_HXX__)
#define __sc_HXX__	1

#include <windows.h>
#include <tchar.h>

#if defined (winCE)
#include <winsock.h>
#endif

#if ! defined (MSMQ_ANCIENT_OS)
#include <msxml2.h>
#endif

#include <svsutil.hxx>

#include <scdefs.hxx>

#if defined (winCE) && defined (SDK_BUILD)
#include <pstub.h>
#endif

//
//	Enable this for verbose debugging and flow output
//
#if (defined (_DEBUG) || defined (DEBUG)) && (! defined (MSMQ_ANCIENT_OS))
#define SC_INCLUDE_CONSOLE	1
#define SC_COUNT_MEMORY		1
#endif

#define SC_MAX_HANDLES		128

#if ! defined (ASSERT)
#define ASSERT SVSUTIL_ASSERT
#endif

//
//	Constants
//
#if ! defined (TRUE)
#define TRUE (1==1)
#endif

#if ! defined (FALSE)
#define FALSE (1==2)
#endif

#define MSMQ_SC_FORMAT_DIRECT_OS	L"DIRECT=OS:"
#define MSMQ_SC_FORMAT_DIRECT_TCP	L"DIRECT=TCP:"
#define MSMQ_SC_FORMAT_DIRECT_HTTP	L"DIRECT=HTTP://"
#define MSMQ_SC_FORMAT_DIRECT_HTTPS L"DIRECT=HTTPS://"
#define MSMQ_SC_FORMAT_PUBLIC		L"PUBLIC="
#define MSMQ_SC_FORMAT_PRIVATE		L"PRIVATE="

#define MSMQ_SC_FORMAT_MACHINE		L"MACHINE="
#define MSMQ_SC_FORMAT_JOURNAL		L";JOURNAL"
#define MSMQ_SC_FORMAT_DEADLT		L";DEADLETTER"

#define MSMQ_SC_PATHNAME_PRIVATE	L"PRIVATE$"

#define MSMQ_SC_ORDERQUEUENAME		L"order_queue$"

#define MSMQ_SC_HTTPD_VROOT_REG_KEY L"COMM\\HTTPD\\VROOTS"
#define MSMQ_SC_HTTPD_ISAPI_NAME    L"srmpIsapi.dll"

#define MSMQ_SC_EXT_IQ			L".iq"
#define MSMQ_SC_EXT_OQ			L".oq"
#define MSMQ_SC_EXT_JQ			L".jr"
#define MSMQ_SC_EXT_DL			L".dq"
#define MSMQ_SC_EXT_MJ			L".jq"

#define MSMQ_SC_SEARCH_PATTERN	L"*.?q"

#define MSMQ_SC_FILENAME_PUBLIC	L"public - "
#define MSMQ_SC_FILENAME_HTTP	L"http - "
#define MSMQ_SC_FILENAME_HTTPS	L"https - "

#define MSMQ_SC_PURGED			L"Purged message"

#define MSMQ_SC_BIGBUFFER		512
#define MSMQ_SC_REGBUFFER		128
#define MSMQ_SC_SMALLBUFFER		64
#define MSMQ_SC_IPBUFFER		20

#define MSMQ_SC_MAX_URI_BYTES	1024

#define MSMQ_SC_MAX_IP_INTERFACES	128

#define SC_GUID_ELEMENTS(p) \
    p->Data1,                 p->Data2,    p->Data3,\
    p->Data4[0], p->Data4[1], p->Data4[2], p->Data4[3],\
    p->Data4[4], p->Data4[5], p->Data4[6], p->Data4[7]

#define SC_GUID_FORMAT     L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define SC_GUID_STR_LENGTH (8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12)

#define SC_FS_UNIT					4096
#define SC_TEXT_VERSION				'0.4 '
#define SC_VERSION_MAJOR			4
#define SC_VERSION_MINOR			2002
#define SC_VERSION_BUILD_NUMBER		0

//
//	Errors
//
#define MSMQ_SC_ERR_BASE			0x100
#define MSMQ_SC_ERR_INITFAILED		(MSMQ_SC_ERR_BASE + 1)
#define MSMQ_SC_ERR_FINALFAILED		(MSMQ_SC_ERR_BASE + 2)

//
//	Error msgs
//
#if defined (USE_RESOURCES)

#define MSMQ_SC_STRINGBASE				0

#define MSMQ_SC_ERRMSG_CANTOPENREGISTRY (MSMQ_SC_STRIGNBASE+1)
#define MSMQ_SC_ERRMSG_INVALIDKEY		(MSMQ_SC_STRINGBASE+2)
#define MSMQ_SC_ERRMSG_NOSOCKETS		(MSMQ_SC_STRINGBASE+3)
#define MSMQ_SC_ERRMSG_NETWORKERROR		(MSMQ_SC_STRINGBASE+4)
#define MSMQ_SC_ERRMSG_NONIPNETWORK		(MSMQ_SC_STRINGBASE+5)
#define MSMQ_SC_ERRMSG_MULTIIPNETWORK	(MSMQ_SC_STRINGBASE+6)
#define MSMQ_SC_ERRMSG_INVALIDDIR		(MSMQ_SC_STRINGBASE+7)
#define MSMQ_SC_ERRMSG_CANTOPENFILE		(MSMQ_SC_STRINGBASE+8)
#define MSMQ_SC_ERRMSG_FILECORRUPT		(MSMQ_SC_STRINGBASE+9)
#define MSMQ_SC_ERRMSG_OUTOFMEMORY		(MSMQ_SC_STRINGBASE+10)
#define MSMQ_SC_ERRMSG_CANTWRITETOFILE	(MSMQ_SC_STRINGBASE+11)
#define MSMQ_SC_ERRMSG_NAMETOOLONG		(MSMQ_SC_STRINGBASE+12)
#define MSMQ_SC_ERRMSG_RPCPROTOFAILED	(MSMQ_SC_STRINGBASE+13)
#define MSMQ_SC_ERRMSG_RPCSERVREGFAILED	(MSMQ_SC_STRINGBASE+14)
#define MSMQ_SC_ERRMSG_RPCLISTENFAILED  (MSMQ_SC_STRINGBASE+15)
#define MSMQ_SC_ERRMSG_INITSHUTDOWN     (MSMQ_SC_STRINGBASE+16)
#define MSMQ_SC_ERRMSG_FAILORDER        (MSMQ_SC_STRINGBASE+17)
#define MSMQ_SC_ERRMSG_FAILQM           (MSMQ_SC_STRINGBASE+18)
#define MSMQ_SC_ERRMSG_FAILSM           (MSMQ_SC_STRINGBASE+19)
#define MSMQ_SC_ERRMSG_FAILOV           (MSMQ_SC_STRINGBASE+20)
#define MSMQ_SC_ERRMSG_ORPHANEDJOURNAL  (MSMQ_SC_STRINGBASE+21)
#define MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE (MSMQ_SC_STRINGBASE+22)
#define MSMQ_SC_ERRMSG_SHUTDOWNUNSTABLE (MSMQ_SC_STRINGBASE+22)
#define MSMQ_SC_ERRMSG_NONETWORKTRACK   (MSMQ_SC_STRINGBASE+23)
#define MSMQ_SC_ERRMSG_MSMQSTARTED		(MSMQ_SC_STRINGBASE+24)
#define MSMQ_SC_ERRMSG_MSMQSTOPPED		(MSMQ_SC_STRINGBASE+25)
#define MSMQ_SC_ERRMSG_UNNAMEDCONNECT	(MSMQ_SC_STRINGBASE+26)
#define MSMQ_SC_ERRMSG_LISTENKILLED		(MSMQ_SC_STRINGBASE+27)
#define MSMQ_SC_ERRMSG_READERKILLED		(MSMQ_SC_STRINGBASE+28)
#define MSMQ_SC_ERRMSG_WRITERKILLED		(MSMQ_SC_STRINGBASE+29)
#define MSMQ_SC_ERRMSG_PINGKILLED		(MSMQ_SC_STRINGBASE+30)
#define MSMQ_SC_ERRMSG_MSMQIGNOREDNACK	(MSMQ_SC_STRINGBASE+31)

#else

#define MSMQ_SC_ERRMSG_CANTOPENREGISTRY L"Registry key is not found (Error %d). Please run configuration utility."
#define MSMQ_SC_ERRMSG_INVALIDKEY		L"Registry parameter %s is invalid (error code %d)"
#define MSMQ_SC_ERRMSG_NOSOCKETS		L"Could not bind to Windows sockets (error code %d)"
#define MSMQ_SC_ERRMSG_NETWORKERROR		L"Network inaccessible."
#define MSMQ_SC_ERRMSG_NONIPNETWORK		L"Network type is incorrect (%d). Need TCP/IP protocol."
#define MSMQ_SC_ERRMSG_MULTIIPNETWORK	L"Multi IP protocols detected on the host. Please specify override value in registry."
#define MSMQ_SC_ERRMSG_INVALIDDIR		L"Directory %s is invalid or unaccessible."
#define MSMQ_SC_ERRMSG_CANTOPENFILE		L"Can't open file %s."
#define MSMQ_SC_ERRMSG_FILECORRUPT		L"Can't read file %s, incorrect format."
#define MSMQ_SC_ERRMSG_OUTOFMEMORY		L"Out of memory!"
#define MSMQ_SC_ERRMSG_CANTWRITETOFILE	L"Can't write to file %s."
#define MSMQ_SC_ERRMSG_NAMETOOLONG		L"Name is too long (%s)."
#define MSMQ_SC_ERRMSG_RPCPROTOFAILED	L"RPC protocol selection failed."
#define MSMQ_SC_ERRMSG_RPCSERVREGFAILED	L"Cannot register as RPC server."
#define MSMQ_SC_ERRMSG_RPCLISTENFAILED  L"Listen faild."
#define MSMQ_SC_ERRMSG_INITSHUTDOWN     L"MSMQ shutdown due to initialization errors."
#define MSMQ_SC_ERRMSG_FAILORDER        L"Order information was not loaded. Check file for corruption."
#define MSMQ_SC_ERRMSG_FAILQM           L"Queue manager initialization failure."
#define MSMQ_SC_ERRMSG_FAILSM           L"Session manager initialization failure. Check network initialization."
#define MSMQ_SC_ERRMSG_FAILOV           L"API manager initialization failure."
#define MSMQ_SC_ERRMSG_ORPHANEDJOURNAL  L"Orphaned journal %s."
#define MSMQ_SC_ERRMSG_CANTMAKEINTQUEUE L"Cannot create Internal Queue %s (%08x)."
#define MSMQ_SC_ERRMSG_SHUTDOWNUNSTABLE L"MSMQ shutdown was not stabilized. Memory/handle leaks are possible."
#define MSMQ_SC_ERRMSG_NONETWORKTRACK   L"Network tracking not supported on this system."
#define MSMQ_SC_ERRMSG_MSMQSTARTED		L"MSMQ service started."
#define MSMQ_SC_ERRMSG_MSMQSTOPPED		L"MSMQ service stopped."
#define MSMQ_SC_ERRMSG_UNNAMEDCONNECT	L"Connection rejected - cannot get host name for %d.%d.%d.%d"
#define MSMQ_SC_ERRMSG_LISTENKILLED		L"Listening thread has been forcefully terminated. Memory may have been leaked."
#define MSMQ_SC_ERRMSG_READERKILLED		L"Reader thread for session %s has been forcefully terminated. Memory may have been leaked."
#define MSMQ_SC_ERRMSG_WRITERKILLED		L"Writer thread for session %s has been forcefully terminated. Memory may have been leaked."
#define MSMQ_SC_ERRMSG_PINGKILLED		L"Ping thread has been forcefully terminated. Memory may have been leaked."
#define MSMQ_SC_ERRMSG_MSMQIGNOREDNACK  L"Unsecure session: NACK request ignored."

#endif

//
//	Structures and typedefs
//
class ScPacket;
class ScQueue;
class ScQueueManager;

typedef int (*CeGenerateGUID_t) (GUID *);

struct ROUTER_LIST	{
	ROUTER_LIST		*pNext;

	union {
		struct {
			unsigned int	cUriLen  : 16;
			unsigned int	fGUID       : 1;
			unsigned int	fWildCard   : 1;
			unsigned int	unused		: 14;
		};
		unsigned int uiFlags;
	};

	WCHAR			*szFormatName;

	union {
		GUID		guid;
		WCHAR		uri[1];
	};
};


#define MAX_VROOTS          5
struct VROOT_ENTRY {
	WCHAR *wszVRoot;
	DWORD  ccVRoot;  // length of wszVRoot in WCHARs.
};


class MachineParameters : public SVSAllocClass {
public:
	unsigned int		uiPort;
	unsigned int		uiPingPort;

	unsigned int		uiDefaultOutQuotaK;
	unsigned int		uiDefaultInQuotaK;
	unsigned int		uiMachineQuotaK;

	int					iMachineQuotaUsedB;

	unsigned int		uiOrderAckWindow;
	unsigned int		uiOrderAckScale;
	unsigned int		uiStartID;
	unsigned int		uiPingTimeout;

	unsigned int		fNetworkTracking : 1;
	unsigned int		fUntrustedNetwork : 1;
	unsigned int        fResponseByIp : 1;
	unsigned int		fUseBinary : 1;
	unsigned int		fUseSRMP : 1;

	GUID				guid;

	unsigned short		asRetrySchedule[MSMQ_SC_RETRY_MAX];
	unsigned int		uiRetrySchedule;
	unsigned int		uiLanOffDelay;
	unsigned int		uiConnectionTimeout;
	unsigned int		uiIdleTimeout;

	unsigned int		uiMinAckTimeout;
	unsigned int		uiMaxAckTimeout;
	unsigned int		uiMinStoreAckTimeout;

	unsigned int		uiSeqTimeout;
	unsigned int		uiOrderQueueTimeout;

	WCHAR				*lpszDirName;
	WCHAR				*lpszHostName;
	WCHAR				*lpszDebugQueueFormatName;
	WCHAR				*lpszOutFRSQueueFormatName;
	VROOT_ENTRY         VRootList[MAX_VROOTS];

	ROUTER_LIST			*pRouteTo;
	ROUTER_LIST			*pRouteFrom;
	ROUTER_LIST			*pRouteLocal;

	HMODULE				hLibLPCRT;
	CeGenerateGUID_t	CeGenerateGUID;

	static int FindInRouteList (ROUTER_LIST *pList, GUID *pGUID, WCHAR **ppFormatName) {
		while (pList) {
			if (pList->fGUID && (pList->guid == *pGUID)) {
				*ppFormatName = pList->szFormatName;
				return TRUE;
			}
			pList = pList->pNext;
		}

		return FALSE;
	}

	static int FindInRouteList (ROUTER_LIST *pList, WCHAR *uri, WCHAR **ppFormatName) {
		while (pList) {
			if (! pList->fGUID) {
				int fFound = FALSE;
				if (pList->fWildCard) {
					if (wcsnicmp (uri, pList->uri, pList->cUriLen) == 0)
						fFound = TRUE;
				} else if (0 == wcsicmp (uri, pList->uri))
					fFound = TRUE;

				if (fFound) {
					*ppFormatName = pList->szFormatName;
					return TRUE;
				}
			}
			pList = pList->pNext;
		}

		return FALSE;
	}

	// performs reverse lookup of FindInRouteList
	static int FindGuidInRouteList (ROUTER_LIST *pList, WCHAR *szFormatName, GUID **ppGUID) {
		while (pList) {
			if (pList->fGUID && (0 == wcsicmp (szFormatName, pList->szFormatName))) {
				*ppGUID = &pList->guid;
				return TRUE;
			}
			pList = pList->pNext;
		}

		return FALSE;
	}

	static int FindUriInRouteList(ROUTER_LIST *pList, WCHAR *szFormatName, WCHAR **ppUri) {
		while (pList) {
			if ((! pList->fGUID) && (0 == wcsicmp (szFormatName, pList->szFormatName))) {
				*ppUri = pList->uri;
				return TRUE;
			}
			pList = pList->pNext;
		}

		return FALSE;
	}

	int RouteTo (GUID *pGUID, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteTo, pGUID, ppFormatName);
	}

	int RouteFrom (GUID *pGUID, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteFrom, pGUID, ppFormatName);
	}

	int RouteLocal (GUID *pGUID, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteLocal, pGUID, ppFormatName);
	}

	int RouteTo (WCHAR *uri, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteTo, uri, ppFormatName);
	}

	int RouteFrom (WCHAR *uri, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteFrom, uri, ppFormatName);
	}

	int RouteLocal (WCHAR *uri, WCHAR **ppFormatName) {
		return FindInRouteList (pRouteLocal, uri, ppFormatName);
	}

	// reverse lookup
	int RouteLocalReverseLookup(WCHAR *szFormatName, WCHAR **ppUri) {
		return FindUriInRouteList(pRouteLocal,szFormatName,ppUri);
	}

	int RouteLocalReverseLookup(WCHAR *szFormatName, GUID **ppGUID) {
		return FindGuidInRouteList(pRouteLocal,szFormatName,ppGUID);
	}

	MachineParameters (void) {
		memset (this, 0, sizeof(*this));
	}

	~MachineParameters (void) {
		int i;

		if (hLibLPCRT)
			FreeLibrary (hLibLPCRT);

		while (pRouteTo) {
			ROUTER_LIST	*pNext = pRouteTo->pNext;
			g_funcFree (pRouteTo, g_pvFreeData);
			pRouteTo = pNext;
		}

		while (pRouteFrom) {
			ROUTER_LIST	*pNext = pRouteFrom->pNext;
			g_funcFree (pRouteFrom, g_pvFreeData);
			pRouteFrom = pNext;
		}

		while (pRouteLocal) {
			ROUTER_LIST	*pNext = pRouteLocal->pNext;
			g_funcFree (pRouteLocal, g_pvFreeData);
			pRouteLocal = pNext;
		}

		if (lpszDirName)
			g_funcFree (lpszDirName, g_pvFreeData);

		if (lpszHostName)
			g_funcFree (lpszHostName, g_pvFreeData);

		if (lpszDebugQueueFormatName)
			g_funcFree (lpszDebugQueueFormatName, g_pvFreeData);

		if (lpszOutFRSQueueFormatName)
			g_funcFree (lpszOutFRSQueueFormatName, g_pvFreeData);

		for (i = 0; i < MAX_VROOTS; i++) {
			if (!VRootList[i].wszVRoot)
				break;

			g_funcFree (VRootList[i].wszVRoot, g_pvFreeData);
		}
	}
};

class GlobalMemory : public SVSAllocClass, public SVSSynch {
public:
	SVSAttrTimer		*pTimer;
	FixedMemDescr		*pPacketMem;
	FixedMemDescr		*pTreeNodeMem;
	FixedMemDescr		*pAckNodeMem;
	HashDatabase		*pStringHash;

	SVSTree				*pTimeoutTree;

	int				    fInitialized;

	GlobalMemory (void);

	~GlobalMemory (void) {
		delete pTimeoutTree;

		svsutil_ReleaseFixedNonEmpty (pPacketMem);
		svsutil_ReleaseFixedNonEmpty (pTreeNodeMem);
		svsutil_ReleaseFixedEmpty    (pAckNodeMem);
		svsutil_FreeAttrTimer        (pTimer);
		svsutil_DestroyStringHash    (pStringHash);
	}

	void Cycle (BOOL fReInitialize);

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#define SCFILE_QP_FORMAT_OS			0
#define SCFILE_QP_FORMAT_TCP		1
#define SCFILE_QP_FORMAT_HTTP		2
#define SCFILE_QP_FORMAT_HTTPS		3

//
//	Externs
//
extern class MachineParameters		*gMachine;
extern class GlobalMemory			*gMem;
extern class ScQueueManager			*gQueueMan;
extern class ScSessionManager		*gSessionMan;
extern class ScOverlappedSupport	*gOverlappedSupport;
extern class ScSequenceCollection	*gSeqMan;

extern int				 fApiInitialized;
extern int               gfCallerExecutable;
extern unsigned long     glServiceState;

//
//	Function prototypes
//
void scapi_EnterInputLoop (void);

int scmain_Shutdown (BOOL fReInitialize);
int scmain_Init (void);
int scmain_ForceExit (void);

DWORD WINAPI scmain_Startup (LPVOID pvParam);

HRESULT scapi_Console (void);

void scerror_Complain (WCHAR *lpszFormat, ...);
void scerror_Complain (int iFormat, ...);
void scerror_Inform (WCHAR *lpszFormat, ...);
void scerror_Inform (int iFormat, ...);

int scerror_AssertOut (void *pvParam, WCHAR *lpszFormat, ...);

#if defined (winCE)
#define MAP(p, type) ((type)MapPtrToProcess ((p), hCallerProc))
#else
#define MAP(p, type) ((type)(p))
#endif

#if defined (SC_VERBOSE)
extern unsigned int g_bCurrentMask;

void scerror_DebugOutPrintArgs(WCHAR *lpszFormat, va_list args);
void scerror_DebugOutPrint(WCHAR *lpszFormat, ...);

int scerror_DebugOut (unsigned int fMask, WCHAR *lpszFormat, ...);
void scerror_DebugInitialize (void);

#define scerror_DebugOutM(mask,format)    if (mask & g_bCurrentMask) scerror_DebugOutPrint format
#else
#define scerror_DebugOutM(mask,format)
#endif

#endif			/* __sc_H__ */

