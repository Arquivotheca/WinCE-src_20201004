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

    scorder.h

Abstract:

    Small client - message ordering


--*/
#if ! defined (__scorder_HXX__)
#define __scorder_HXX__	1

#include <svsutil.hxx>
#include <sc.hxx>

#define SCSEQUENCE_ENTRIES_PER_BLOCK	((SC_FS_UNIT - sizeof(ScOrderClusterHeader)) / sizeof(ScPersistOrderSeq))
#define SCSEQUENCE_HASH_BUCKETS		17
#define SCSEQUENCE_BLOCKINCR		10
#define SCSEQUENCE_STALEFACTOR		5		// 1/SCSEQUENCE_EMPTYFACTOR = max % stale entries
#define SCSEQUENCE_FILENAME			L"sequence.dat"
#define SCSEQUENCE_FILENAME_STRING	L"strings.dat"

#define SCPERSISTORDER_FLAGS_SECURESESSION		1

struct ScPersistOrderSeq {
	GUID				guidQ;					// 16 bytes. GUID of the local transactional queue
	GUID				guidQueueName;			// 16 bytes. GUID of local transactional queue as named by remote app
	GUID				guidStreamId;			// 16 bytes. Stream Id (or source QM if classic MSMQ on the other side).
	GUID				guidOrderAck;			// 16 bytes. GUID of order ack queue name.
	LONGLONG			llCurrentSeqID;			// 8  bytes
	unsigned int		uiCurrentSeqN;			// 4 or next free block ndx
	unsigned int		uiLastAccessS;			// 4 or 0 if block is free
	unsigned int		uiBlockFlags;			// Is this stream secure?
};

class ScQueue;

struct ScOrderSeq {
	ScPersistOrderSeq	*p;
	ScQueue				*pQueue;
	ScOrderSeq			*pNextHash;

	ScOrderSeq			*pNextActive;
	ScOrderSeq			*pPrevActive;

	unsigned int		fActive : 1;

	in_addr				ipAddr;
};

#define SCSEQUENCE_MAGIC1	'QMSM'
#define SCSEQUENCE_MAGIC2	' SVS'
#define SCSEQUENCE_VERSION	SC_TEXT_VERSION

struct ScOrderClusterHeader {
	unsigned int	uiMagic1;
	unsigned int	uiMagic2;
	unsigned int	uiVersion;
	unsigned int	uiFirstFreeBlock;
	unsigned int	pad[12];
};

struct ScOrderCluster {
	int		iNumFree;
	int		iDirty;

	union {
		struct {
			ScOrderClusterHeader	h;
			ScPersistOrderSeq		pos[SCSEQUENCE_ENTRIES_PER_BLOCK];
		};
		unsigned char			ac[SC_FS_UNIT];
	};
};

struct ScStringStoreEntry {
	GUID			guid;
	unsigned int	uiStringSize;		// This is in bytes, including terminating NULL!
	WCHAR			szString[1];
};

struct StringGuidHash {
	StringGuidHash			*pNextInString;
	StringGuidHash			*pNextInGUID;
	unsigned int			uiRef;
	GUID					guid;
	WCHAR					szString[1];
};

#define SCSEQUENCE_T_COMPACT		(2  * 60 * 60 * 1000)
#define SCSEQUENCE_T_PRUNE			(24 * 60 * 60 * 1000)
#define SCSEQUENCE_T_ACCESS			(15 * 60 * 1000)

#define SCSEQUENCE_S_SEQTHRESH		(45 * 24 * 60 * 60)
#define SCSEQUENCE_S_SEQTHRESH_Q	(90 * 24 * 60 * 60)

#define SCSEQUENCE_R_ACKSEND		1
#define SCSEQUENCE_R_ACKPERIOD		5

#define SCSEQUENCE_CLUSTERALLOC		10

class ScSequenceCollection : public SVSAllocClass {
private:
	HANDLE			hBackupFile;
	HANDLE			hStringFile;

	WCHAR			*lpszBackupName;
	WCHAR			*lpszStringName;

	unsigned int	uiLastBackupAccessT;
	unsigned int	uiLastStringAccessT;
	unsigned int	uiLastCompactT;
	unsigned int	uiLastPruneT;

	unsigned int	uiClustersCount;
	unsigned int	uiClustersAlloc;
	ScOrderCluster	**ppClusters;

	ScOrderSeq		*pSeqBuckets[SCSEQUENCE_HASH_BUCKETS];

	ScOrderSeq		*pSeqActive;

	FixedMemDescr	*pfmOrderSeq;

	StringGuidHash	*pStringBuckets[SCSEQUENCE_HASH_BUCKETS];
	StringGuidHash	*pGuidBuckets[SCSEQUENCE_HASH_BUCKETS];

	int				OpenBackup    (void);
	int				CloseBackup   (void);
	int				FlushBackup   (void);

	int				OpenStrings   (void);
	int				CloseStrings  (void);

	int				Compact (void);
	int				Prune   (void);

	int				AddNewPairToHash (WCHAR *szString, GUID *pguid, int fAddToFile);
	int				LoadStringHash (void);
	void			PruneStringHash (void);

	StringGuidHash  *GetHashBucket (GUID *pguid);
	StringGuidHash  *GetHashBucket (WCHAR *sz);

	int				AddNewCluster (void);

	ScPersistOrderSeq	*PersistBlock (void);

	int FindCluster (ScPersistOrderSeq *pPersSeq) {
		for (int i = 0 ; i < (int)uiClustersCount ; ++i) {
			if (((&ppClusters[i]->pos[0]) <= pPersSeq) && ((&ppClusters[i]->pos[SCSEQUENCE_ENTRIES_PER_BLOCK]) >= pPersSeq)) {
				SVSUTIL_ASSERT (((unsigned int)pPersSeq - (unsigned int)ppClusters[i]->pos) % sizeof (ScPersistOrderSeq) == 0);
				return i;
			}
		}

		return -1;
	}

	static unsigned int	Hash (GUID *pguid1, GUID *pguid2);
	static unsigned int	Hash (GUID *pguid);
	static unsigned int	Hash (WCHAR *sz);

public:
	HANDLE			hSequencePulse;
	int				fBusy;

	int				HashStringToGUID (WCHAR *uri, GUID *pGUID);
	WCHAR			*GetStringFromGUID (GUID *pguid);

	ScOrderSeq		*HashOrderSequence (ScQueue *pQueue, GUID *pguidStreamId, GUID *pguidQueueName, GUID *pguidOrderAckName, unsigned int flags);
	int				SaveSequence (ScOrderSeq *pSequence);
	void			DeleteQueue (ScQueue *pQueue);

	int				Load    (void);

	void			Maintain (void) {
		if (GetTickCount() - uiLastPruneT >= SCSEQUENCE_T_PRUNE) {
#ifdef SVSUTIL_DEBUG_ANY
			int iRes = 
#endif
				Prune   ();
			SVSUTIL_ASSERT (iRes);
		}

		if (GetTickCount() - uiLastCompactT >= SCSEQUENCE_T_COMPACT) {
#ifdef SVSUTIL_DEBUG_ANY
			int iRes = 
#endif
				Compact ();
			SVSUTIL_ASSERT (iRes);
		}


		if (GetTickCount() - uiLastBackupAccessT >= SCSEQUENCE_T_ACCESS)
			CloseBackup ();

		if (GetTickCount() - uiLastStringAccessT >= SCSEQUENCE_T_ACCESS)
			CloseStrings ();
	}

	void			SendAcks (void);

	ScSequenceCollection (void);
	~ScSequenceCollection (void);

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
	friend HRESULT scmgmt_MQMgmtAction_Internal (LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction);

};


#endif	/* __scorder_HXX__ */

