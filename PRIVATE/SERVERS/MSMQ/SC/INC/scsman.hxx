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
    scsmgr.hxx

Abstract:
    Small client session manager


--*/
#if !defined(__scsmgr_HXX__)
#define __scsmgr_HXX__	1

#include <svsutil.hxx>
#include <winsock.h>
#include <sc.hxx>
#include <scsrmp.hxx>

#define SCSESSION_MAX_THREADPAIRS		5

#define SCSESSION_STATE_INACTIVE			1
#define SCSESSION_STATE_CONNECTING			2
#define SCSESSION_STATE_CONNECTED_CLIENT	3
#define SCSESSION_STATE_CONNECTED_SERVER	4
#define SCSESSION_STATE_OPERATING			5
#define SCSESSION_STATE_EXITING				6
#define SCSESSION_STATE_WAITING				7

#define SCSESSION_INITIAL_TIMEOUT			(5 * 60)
#define SCSESSION_IDLE_TIMEOUT				(5 * 60)

#define SCSESSION_ATTEMPT_NUM				5
#define SCSESSION_ATTEMPT_FIRST				(5 * 60)
#define SCSESSION_ATTEMPT_SECOND			(5 * 60)
#define SCSESSION_ATTEMPT_THIRD				(10 * 60)
#define SCSESSION_ATTEMPT_FOURTH			(10 * 60)
#define SCSESSION_ATTEMPT_FIFTHANDUP		(15 * 60)

#define SCSESSION_ATTEMPT_LANOFF			(5 * 60)

#define SCSESSION_LANAUP_DELAY				15

//
//	Timeout values copied form qm\session.cpp. Make sure these are in sync!
//
#define MSMQ_MIN_ACKTIMEOUT          1000*20     // define minimum ack timeout to 20 seconds
#define MSMQ_MAX_ACKTIMEOUT          1000*60*2   // define maximum ack timeout to 2 minutes
#define MSMQ_MIN_STORE_ACKTIMEOUT    500         // define minimum ack timeout to 0.5 second

class ScQueueManager;
class ScSessionManager;
class ScPacketImage;
class ScPacket;
struct CBaseHeader;
struct CSessionSection;
class CSrmpCallBack;
class CSrmpFwd;

struct SentPacket {
	ScPacket			*pPacket;
	unsigned int		usNum;

	SentPacket			*pNext;
};

class ScSession : public SVSAllocClass, public SVSRefObj {
public:
	int			fSessionState;
	WCHAR		*lpszHostName;

private:
	char		*lpszmbHostName;
	int			iFailures;
	unsigned int uiNextAttemptTime;

	HANDLE      hEvent;
	HANDLE		hServiceThreadR;
	HANDLE		hServiceThreadW;

	IN_ADDR		ipPeerAddr;

	SOCKET		s;

	GUID			guidDest;

	union {
		unsigned int	uiConnectionStamp;
		unsigned int	uiConnectionDelta;
	};

	unsigned int		uiAckTimeout;
	unsigned int		uiStoreAckTimeout;
	unsigned int		uiMyWindowSize;
	unsigned int		uiOtherWindowSize;

	unsigned short		usLastAckSent;
	unsigned short		usLastRelAckSent;
	unsigned short		usPacketsSent;
	unsigned short		usRelPacketsSent;

	unsigned short		usLastAckReceived;
	unsigned short		usPacketsReceived;
	unsigned short		usRelPacketsReceived;

	unsigned int		uiAckDue;

	ScSession			*pNext;
	ScSession			*pPrev;

	SentPacket			*pSentPackets;
	SentPacket			*pSentRelPackets;

	int                 qType;

	ScSession (WCHAR *a_lpszHostName, ScSession *pTail, int qt) {
		SVSUTIL_ASSERT( (qt == SCFILE_QP_FORMAT_TCP) || (qt == SCFILE_QP_FORMAT_HTTP) || (qt == SCFILE_QP_FORMAT_HTTPS) || (qt == SCFILE_QP_FORMAT_OS));
		qType = qt;

		fSessionState		= SCSESSION_STATE_INACTIVE;
		lpszHostName		= a_lpszHostName;

		int iSz = WideCharToMultiByte (CP_ACP, 0, lpszHostName, -1, NULL, 0, NULL, NULL);

		if (iSz && (lpszmbHostName = (char *)g_funcAlloc (iSz, g_pvAllocData)))
			WideCharToMultiByte (CP_ACP, 0, lpszHostName, -1, lpszmbHostName, iSz, NULL, NULL);
		else
			SVSUTIL_ASSERT(0);

		iFailures           = 0;
		uiNextAttemptTime	= 0;

		hEvent              = NULL;
		hServiceThreadR     = hServiceThreadW = NULL;

		s					= INVALID_SOCKET;

		ipPeerAddr.S_un.S_addr  = INADDR_NONE;
		memset (&guidDest, 0, sizeof(guidDest));

		uiConnectionStamp	= 0;
		uiAckTimeout		= 0;
		uiStoreAckTimeout	= 0;
		uiMyWindowSize		= 0;
		uiOtherWindowSize   = 0;

		usLastAckSent        = 0;
		usLastRelAckSent     = 0;
		usPacketsSent        = 0;
		usRelPacketsSent     = 0;

		usLastAckReceived    = 0;
		usPacketsReceived    = 0;
		usRelPacketsReceived = 0;

		uiAckDue			 = 0;

		pNext = pTail;

		if (pTail)
			pTail->pPrev = this;

		pPrev = NULL;

		pSentPackets	     = NULL;
		pSentRelPackets		 = NULL;

		hInternetSession     = 0;
		hInternetConnect     = 0;
		hRequest             = 0;
	}

	~ScSession (void) {
		SVSUTIL_ASSERT (! hServiceThreadR);
		SVSUTIL_ASSERT (! hServiceThreadW);

		SVSUTIL_ASSERT (! lpszHostName);

		SVSUTIL_ASSERT (gMem->IsLocked());

		while (pSentPackets) {
			SentPacket *pNext = pSentPackets->pNext;
			svsutil_FreeFixed (pSentPackets, gMem->pAckNodeMem);
			pSentPackets = pNext;
		}

		SVSUTIL_ASSERT (! pSentPackets);

		while (pSentRelPackets) {
			SentPacket *pNext = pSentRelPackets->pNext;
			svsutil_FreeFixed (pSentRelPackets, gMem->pAckNodeMem);
			pSentRelPackets = pNext;
		}

		SVSUTIL_ASSERT (! pSentRelPackets);

		if (lpszmbHostName)
			g_funcFree (lpszmbHostName, g_pvFreeData);

		if (s != INVALID_SOCKET)
			closesocket (s);
	}

	void InitConnection (void);
	void OkConnection   (unsigned int uiConnectionType);
	void FailConnection (void);
	void FinishSession  (void);

	int MakeSessionSect (CSessionSection *pSessSect, ScPacket *pPacket, int fForce);

	int SendECP (int fRefuseConnection);
	int RecvECP (void);
	int SendCPP (void);
	int RecvCPP (void);
	int SendAck (void);
	int SendUP  (ScPacket *pPacket, ScQueue *pQueue);


	void ReturnUnacketPacketsToQueues(void);
	int HandleSessionSection (CSessionSection *pcSessionSection);
	int HandleUserPacket     (ScPacketImage *pImage, CSessionSection *pcSessionSection);
	int HandleInternalPacket (CBaseHeader *pBaseHeader);
	int ExtractNextPacket    (ScPacket *&pPacket, ScQueue *&pQueue, ScQueue *pPrevQueue);

	int Connect (void);

	void ServiceThreadR (void);
	void ServiceThreadW (void);
	void ServiceThreadHttpW(void);

	static DWORD WINAPI ServiceThreadR_s (void *arg);
	static DWORD WINAPI ServiceThreadW_s (void *arg);
	static DWORD WINAPI ServiceThreadHttpW_s (void *arg);

	friend class ScSessionManager;
	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);

	friend HRESULT scmgmt_MQMgmtAction_Internal(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction);
	friend HRESULT scmgmt_MQMgmtGetInfo2(LPCWSTR pMachineName, LPCWSTR pObjectName, DWORD cp, PROPID aPropID[], PROPVARIANT aPropVar[]);

	// SRMP specific
	int    BuildAndSendSRMP(ScPacket *pPacket, ScQueue *pQueue);
	int    SendHttpMsg(CHAR *szURL, BOOL fSecure, PSrmpIOCTLPacket pIOPacket);
	void   SRMPCloseSession(void);
	int    InitializeWininet(CSrmpCallBack *pCallback);
	int    ForwardSrmpMessage(CSrmpFwd *pSrmpFwd, ScPacketImage *pPacket, WCHAR *wszURL);

	HANDLE hInternetSession;
	HANDLE hInternetConnect;
	HANDLE hRequest;

public:
	int    IsSecure(void) { return (qType == SCFILE_QP_FORMAT_HTTPS); }
};



class ScSessionManager : public SVSAllocClass {
	ScSession		*pSessList;

	HANDLE			hAccThread;

	SOCKET			s_listen;
	int				iThreadPairs;

	int SpinSession (ScSession *pSess, SOCKET s);

	void AccThread   (void);

	static DWORD WINAPI AccThread_s      (void *arg);

public:
	int				fInitialized;
	int				fBusy;

	HANDLE			hBuzz;

	static HANDLE	hNetUP;

	ScSessionManager (void);
	~ScSessionManager (void);

	ScSession *GetSession (WCHAR *a_lpszHostName, int qType);
	ScSession *GetInactiveOsSession (WCHAR *a_lpszHostName);
	void ReleaseSession (ScSession *pSess);

	void PacketInserted (ScSession *pSess);

	void ReleaseThreadPair (void) {
		if (InterlockedDecrement ((long *)&iThreadPairs) == (SCSESSION_MAX_THREADPAIRS - 1))
			SetEvent (hBuzz);
	}

	int BookThreadPair (void) {
		if (InterlockedIncrement ((long *)&iThreadPairs) > SCSESSION_MAX_THREADPAIRS) {
			ReleaseThreadPair();
			return FALSE;
		}

		return TRUE;
	}

	void Start (void);
	void Stop  (void);
	void ConnService (void);
	void ConnectToNet (unsigned int uiDelta);

	friend class ScSession;
	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
	friend HRESULT scmgmt_MQMgmtAction_Internal(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction);
	friend HRESULT scmgmt_MQMgmtGetInfo2(LPCWSTR pMachineName, LPCWSTR pObjectName, DWORD cp, PROPID aPropID[], PROPVARIANT aPropVar[]);
};

#endif          /* __scsmgr_HXX__ */


