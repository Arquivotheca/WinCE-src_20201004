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

    scfile.h

Abstract:

    Small client file format


--*/
#if ! defined (__scfile_HXX__)
#define __scfile_HXX__	1

#include <svsutil.hxx>
#include <sc.hxx>

#define SCFILE_MAGIC1				'QMSM'
#define SCFILE_MAGIC2				'EUEU'
#define SCFILE_VERSION				SC_TEXT_VERSION

#define SCFILE_DIRECTORY_ALIGNMENT	SC_FS_UNIT
#define SCFILE_DIRECTORY_INITSIZE	((SC_FS_UNIT - offsetof (ScFilePacketDir, sPacketEntry)) / sizeof (int))
#define SCFILE_DIRECTORY_INCREMENT	(SC_FS_UNIT / sizeof (int))

#define SCFILE_PACKET_EMPTY		0
#define SCFILE_PACKET_LIVE		1
#define SCFILE_PACKET_WAITACK	2
#define SCFILE_PACKET_LAST		3	// Always empty marker

#define SCFILE_INACTIVITY_THRESHOLD	10

#define SCFILE_UPDATE_DIR			1
#define SCFILE_UPDATE_HEADER		2
#define SCFILE_UPDATE_PACKETDIR		4
#define SCFILE_UPDATE_PACKETWRITTEN	8

struct ScFileDirectory {
	unsigned int	uiMagic1;
	unsigned int	uiMagic2;
	unsigned int	uiVersion;
	unsigned int	uiQueueHeaderOffset;
	unsigned int	uiQueueHeaderSize;
	unsigned int	uiPacketDirOffset;		// Should be aligned
	unsigned int	uiPacketDirSize;
	unsigned int	uiPacketBackupOffset;
	unsigned int	uiPacketBackupSize;
	unsigned int	uiReserved[7];
};

struct ScQueueParms {
	unsigned int		bAuthenticate		: 1;
	unsigned int		bTransactional		: 1;
	unsigned int		bHasJournal			: 1;
	unsigned int		bIsIncoming			: 1;
	unsigned int		bIsInternal			: 1;
	unsigned int		bIsProtected		: 1;
	unsigned int		bIsOrderAck			: 1;
	unsigned int		bIsOutFRS			: 1;
	unsigned int		bIsDeadLetter		: 1;
	unsigned int		bIsJournal			: 1;
	unsigned int		bIsMachineJournal   : 1;
	unsigned int		bIsPublic           : 1;
	unsigned int		bIsJournalOn        : 1;
	unsigned int		bIsRouterQueue      : 1;
	unsigned int		bFormatType 		: 4;
	unsigned int		bReserved			: 14;

	unsigned int		uiPrivacyLevel;
	unsigned int		uiQuotaK;
	int					iBasePriority;

	GUID				guid;
	GUID				guidQueueType;
};

struct ScFileQueueHeader {
	ScQueueParms	qp;
	unsigned int	tCreationTime;
	unsigned int	tModificationTime;
	unsigned int	uiLabelOffset;
	unsigned int	uiPathNameOffset;
};

struct ScPacketEntry {
	unsigned int		uiOffset : 30;	// 1 GB
	unsigned int		uiType   : 2;
};

struct ScFilePacketDir {
	unsigned int			uiNumEntries;
	unsigned int			uiUsedEntries;
	ScPacketEntry			sPacketEntry[1];
};

class ScPacket;
class ScQueue;

class ScFile : public SVSAllocClass {
private:
	HANDLE					hBackupFile;
	int						fFileCreated;

	WCHAR					*lpszFileName;

	unsigned int			tLastUsed;

	ScFileDirectory			*pFileDir;
	ScFilePacketDir			*pPacketDir;

	SVSExpArray<int>		*pMap;
	int						iFreeIndex;
	int						iMapSize;

	int						iUsedSpace;

	ScQueue					*pQueue;

	ScFileQueueHeader		*ComputeHeaderSection(int *piSize);

	int		GetFreePacketNdx (void);
	void	ReleasePacketNdx (int iNdx);

	int		GetSuitableChunk (ScPacket *pPacket, unsigned int &uiWhichSection);

	int		IntegrityCheck (void);

	ScFile (WCHAR *a_lpszFileName, ScQueue *pQueue);
	~ScFile (void);

	int CreateNew (void);
	int Restore   (void);

	int Open   (void);
	int Compact(unsigned int &uiWhichSection);

	int UpdateSection (unsigned int uiSections);

	int HaveSpaceInQuota (int a_iSize);

public:
	int Rewrite (int fIncPacketDir);
	int Close   (int fForce);
	int Delete  (int fForce);

	int BackupPacket  (ScPacket *pPacket, unsigned int &uiWhichSection);
	int RestorePacket (ScPacket *pPacket);
	int GetPacketSize (ScPacket *pPacket);
	int DeletePacket  (ScPacket *pPacket, unsigned int &uiWhichSection);

	friend class ScQueue;

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#endif	/* __scfile_HXX__ */

