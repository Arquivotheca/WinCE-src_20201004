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

    scqman.hxx

Abstract:

    Small client queue manager class


--*/
#if ! defined (__scqman_HXX__)
#define __scqman_HXX__	1
#include <svsutil.hxx>

#define SCQMAN_PERIODIC				(120 * 1000)		// 2 min
#define SCQMAN_ORDERACKPERIODIC		3					// In scale units set in registry

#define SCQMAN_OUTQUEUE_DELETION	1200				// 20 min - time in seconds

class ScQueue;
class ScPacket;
class ScPacketImage;
class ScSessionManager;
struct ScOrderSeq;

struct ScQueueParms;

class ScQueueList : public SVSAllocClass {
public:
	ScQueue		*pQueue;
	ScQueueList *pqlNext;
	ScQueueList *pqlPrev;

	ScQueueList (ScQueue *a_pQueue, ScQueueList *a_pqlNext) {
		ASSERT ((! a_pqlNext) || (! a_pqlNext->pqlPrev));

		pqlPrev = NULL;
		pqlNext = a_pqlNext;

		if (pqlNext)
			pqlNext->pqlPrev = this;

		pQueue = a_pQueue;
	}
};

struct ScPacketList {
	ScPacket     *pPacket;
	ScPacketList *pNext;
};

#define SCQMAN_HANDLE_INCR		20

#define SCQMAN_HANDLE_QUEUE		0
#define SCQMAN_HANDLE_CURSOR	1

struct ScHandleInfo	{
	unsigned int			uiHandleType;
	ScQueue					*pQueue;
	void					*pProcId;

	union {
		struct {
			SVSHandle		hQueue;
			SVSTNode		*pNode;
			int				fPosValid;
		} c;
		struct {
			unsigned int	uiShareMode;
			unsigned int	uiAccess;
			SVSHandle	    hDefaultCursor;
			unsigned int    uiQueueType;
		} q;
	};
};

#define SCQMAN_ORDER_ACK_TITLE       L"QM Ordering Ack"

//
//	This MUST be packed - otherwise LONGLONG forces bad sizeof...
//
#pragma pack(push,1)

struct  ScOrderAckBody {
    LONGLONG  m_liSeqID;
    ULONG     m_ulSeqN;
    ULONG     m_ulPrevSeqN;
    OBJECTID  MessageID;
};

#pragma pack(pop)

#define SCQMAN_ID_SAVE_FREQ_MASK		0xfff

class ScQueueManager : public SVSAllocClass {
	unsigned int			uiMessageID;

	SVSSimpleHandleSystem	*pHandles;
	FixedMemDescr		    *pHandleMem;

	HANDLE					hMainThread;

	ScQueue *FinishCreation (ScQueue *pQueue, ScQueue *pJournal, int fDelFiles);

	static int CloseAllHandlesForQueue (SVSHandle h, void *pvArg);
	static int CloseAllHandlesForProc  (SVSHandle h, void *pvArg);
	static int CloseAllHandlesForQueueHandle (SVSHandle h, void *pvArg);

	void SaveMessageID (void);

public:
	ScQueueList				*pqlIncoming;
	ScQueueList				*pqlOutgoing;

	ScQueue					*pQueueDLQ;								// Dead-letter queue
	ScQueue					*pQueueOrderAck;						// Dead-letter queue
	ScQueue					*pQueueOutFRS;							// Out-FRS queue
	ScQueue					*pQueueJournal;							// Machine Journal

	HANDLE					hPacketExpired;
	HANDLE					hOrderAckTimer;

	int						iPacketsWaitingOrderAck;

	int						fBusy;

	unsigned int GetNextID (void) {
		if ((uiMessageID & SCQMAN_ID_SAVE_FREQ_MASK) == 0)
			SaveMessageID();

		unsigned int uiRes = ++uiMessageID;

		if (! uiRes)
			return ++uiMessageID;

		return uiRes;
	}

	ScQueueManager (unsigned int);
	~ScQueueManager(void);

	//
	//	Queue location
	//
	ScQueue *FindIncomingByFormat      (WCHAR *lpszFormatName, int *pfQueueType=NULL);
	ScQueue *FindOutgoingByFormat      (WCHAR *lpszFormatName);

	ScQueue *FindQueueByPacketImage (ScPacketImage *pImage, int fResponse);

	//
	//	Queue construction/destruction
	//
	ScQueue *MakeQueueFromFile (WCHAR *lpszFileName, HRESULT *phr);
	ScQueue *MakeIncomingQueue (WCHAR *a_lpszPathName, WCHAR *a_lpszLabel, ScQueueParms *pqp, unsigned int uiJournalQuota, HRESULT *phr);
	ScQueue *MakeOutgoingQueue (WCHAR *a_lpszFormatName, ScQueueParms *pqp, HRESULT *phr);
	ScQueue *FindOrMakeOutgoingQueue(LPWSTR lpszFormatName);
	int		DeleteQueue (ScQueue *pQueue, int fPerm);

	int		StoreTransactionalPacket (ScQueue *pQueue, ScPacketImage *pPacketImage, WCHAR *lpszHostName, GUID *pSourceQM);
	int		RejectPacket (ScPacketImage *pPacketImage, int fWhy);
	int		RejectPacket (ScPacket *pPacket,		   int fWhy, ScQueue *pQueue);
	int		AcceptPacket (ScPacket *pPacket,		   int fHow, ScQueue *pQueue);

	int		JournalPacket (ScPacket *pPacket);

	void	ForwardAck     (ScPacketImage *pImage);
	int		SendOrderAck (ScOrderSeq *pSeq);
	void    ForwardTransactionalResponse (ScPacketImage *pImage, int fWhat, WCHAR *lpszHostName, GUID *pSourceQM);
	void	ReceiveOrderAck (void);
	void	ExpirationCheck(void);
	void	PeriodicCheck  (void);
	void    OrderAckCheck  (void);
	void	MainThread     (void);
	void	Start		   (void);
	void	Stop		   (void);

	static DWORD WINAPI ScQueueManager::MainThread_s (void *arg);

	//
	//	Handle support
	//
	SVSHandle		AllocHandle (ScHandleInfo *pHInfo);
	ScHandleInfo	*QueryHandle (SVSHandle h);
	int				CloseHandle  (SVSHandle h);
	void			CloseAllHandles  (void);
	void			CloseAllHandles	 (ScQueue   *pQueue);
	void			CloseAllHandles	 (SVSHandle hQueue);
	void			CloseProcHandles (void      *pvProcId);

	int				CountPacketRefs (ScPacket *pPacket);

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

ScPacketImage *scqman_DupImage (ScPacketImage *pProto);

#endif		/* __scqman_HXX__ */
