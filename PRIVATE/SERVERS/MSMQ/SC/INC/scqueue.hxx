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

    scqueue.h

Abstract:

    Small client queue manager class


--*/
#if ! defined (__scqueue_HXX__)
#define __scqueue_HXX__	1

#include <svsutil.hxx>

#include <scfile.hxx>
#include <scutil.hxx>

class ScPacket;
class ScPacketImage;
class ScQueueManager;
class ScSession;

struct ScHandleInfo;
extern const BOOL g_fHaveSRMP;

class ScQueue : public SVSAllocClass {
private:
	int					fInitialized;

	ScFile				*sFile;
	SVSTree				*pPackets;

	void Init (void);
	void InitSequence (void);

public:
	ScQueue				*pJournal;

	WCHAR				*lpszQueueHost;
	WCHAR				*lpszQueueName;
	WCHAR				*lpszFormatName;
	WCHAR				*lpszQueueLabel;

	ScQueueParms		qp;

	int					iQueueSizeB;

	unsigned int		tCreation;
	unsigned int		tModification;

	unsigned int		fDenyAll;
	unsigned int		uiOpenRecv;
	unsigned int		uiOpen;

	LONGLONG			llSeqID;
	unsigned int		uiSeqNum;
	unsigned int		uiFirstUnackedSeqN;

	SVSCKey				hkReceived;

	HANDLE				hUpdateEvent;

	ScSession			*pSess;

	ScQueue  (WCHAR *a_lpszFileName, HRESULT *phr);
	ScQueue  (WCHAR *a_lpszFormatName, ScQueueParms *a_pqp, HRESULT *phr);
	ScQueue  (WCHAR *a_lpszPathName, WCHAR *a_lpszLabel, ScQueueParms *a_pqp, HRESULT *phr);
	~ScQueue (void);

	int			Advance (ScHandleInfo *);
	int			Backup  (ScHandleInfo *);
	int			Reset   (ScHandleInfo *);

	ScPacket *MakePacket     (ScPacketImage *pvPacketImage, int iDirEntry, int fPutInFile);
	ScPacket *InsertPacket	 (ScPacket *pPacket);
	int		 DisposeOfPacket (ScPacket *pPacket);
	int		 BringToMemory   (ScPacket *pPacket);
	void	 FreeFromMemory  (ScPacket *pPacket);
	void	 UpdateFile      (void);

	void	 Purge (unsigned int uiPurgeType);
	int		 PurgeMessage (OBJECTID *poid, unsigned int uiPurgeType);

	int Size (void) { return pPackets->Size (); };
	int IsHttpOrHttps() { return (qp.bFormatType == SCFILE_QP_FORMAT_HTTP) || (qp.bFormatType == SCFILE_QP_FORMAT_HTTPS);}

	friend class ScQueueManager;
	friend class ScFile;
	friend class ScSessionManager;
	friend class ScSession;
	friend class ScSequenceCollection;
	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#endif /* __scqueue_HXX__ */

