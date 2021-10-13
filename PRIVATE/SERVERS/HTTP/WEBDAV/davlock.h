//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davimpl.cpp
Abstract: WebDAV implementation of locking functions
--*/


class CWebDavFileLockManager;


// Always open files for locking with this level of access and sharing.
#define DAV_FILE_ACCESS                (GENERIC_READ | GENERIC_WRITE)
#define DAV_SHARE_ACCESS               (FILE_SHARE_READ)
// Locks last for 3 minutes unless overridden by http client or registry.  In milliseconds (because registry is stored in milliseconds)
#define DEFAULT_DAV_LOCK_TIMEOUT       (180*1000)
// Longest amount of time a Dav LOCK can be requested for (86700 seconds=1 day).
#define MAX_DAV_LOCK_TIMEOUT           (60*60*24)

// Design of lock classes:
// There may be multiple locks (CWebDavLock's) per file if access is <shared>, and 
// each lock may have different owners and expiration times.  File name and pointer
// to the locks is stored in a (CWebDavFileNode) class.  All of these are managed
// by CWebDavFileLockManager global.

class CNameSpaceMap;


class CWebDavLock {
private:
	// Allocated via a fixed block array.  Dissallow calls to new and delete.
	CWebDavLock();
	~CWebDavLock();

public:
	CWebDavLock        *m_pNext;         // next ptr in list
	CHAR               *m_szOwner;       // Corresponds to <HREF> tag sent back when lock access is denied.
	DWORD              m_ccOwner;        // strlen(m_szOwner)
	CHAR               *m_szOwnerNSTags; // When we parse out <owner> tag, if there are any namespaces in XML tags contained by owner store them in here.
	DWORD              m_ccOwnerNSTags;  // strlen(m_szOwnerTags)
	__int64            m_i64LockId;      // LockID
	__int64            m_ftExpires;      // time the lock expires and will be removed from cache.
	DWORD              m_dwBaseTimeout;  // Timeout initial

	void SetTimeout(DWORD dwSecondsToLive);

	BOOL HasLockExpired(FILETIME *pCurrentTime) {
		return (*((__int64*)pCurrentTime) > m_ftExpires);
	}

	BOOL InitLock(PSTR szOwner, CNameSpaceMap *pNSMap, __int64 i64LockId, DWORD dwBaseTimeout);
	void DeInitLock(void);
};

typedef enum {
	DAV_LOCK_UNKNOWN,
	DAV_LOCK_SHARED,
	DAV_LOCK_EXCLUSIVE
} DAV_LOCK_SHARE_MODE;

inline BOOL IsValidLockState(DAV_LOCK_SHARE_MODE sm) {
	return ((sm == DAV_LOCK_SHARED) || (sm == DAV_LOCK_EXCLUSIVE));
}

class CWebDavFileNode {
private:
	// Allocated via a fixed block array.  Dissallow calls to new and delete.
	CWebDavFileNode()  { ; }
	~CWebDavFileNode() { ; }

public:
	CWebDavFileNode        *m_pNext;         // pointer to next item in list
	CWebDavLock            *m_pLockList;     // LOCKs associated with this file.
	WCHAR                  *m_szFileName;    // name of file that is locked
	// DWORD               m_dwAccess;       // dwAccess passed to CreateFile // always (GENERIC_READ | GENERIC_WRITE)
	DAV_LOCK_SHARE_MODE    m_lockShareMode;  // dwShareMode passed to CreateFile (shared vs exclusive locks)
	HANDLE                 m_hFile;          // locked file handle.
	BOOL                   m_fBusy;          // If we're doing a potentially long running operation under a file lock (i.e move/copy/PUT) then don't allow LOCK/UNLOCK requests to succeed.

	BOOL InitFileNode(WCHAR *szFile, DAV_LOCK_SHARE_MODE lShareMode, HANDLE hFile);
	void DeInitFileNode(void);

	BOOL IsSharedLock(void)    { return (m_lockShareMode == DAV_LOCK_SHARED);    }
	BOOL IsExclusiveLock(void) { return (m_lockShareMode == DAV_LOCK_EXCLUSIVE); }

	BOOL HasLocks(void) { return (m_pLockList != NULL); }
	void RemoveTimedOutLocks(FILETIME *pft);

	BOOL AddLock(PSTR szOwner, CNameSpaceMap *pNSMap, __int64 i64LockId, DWORD dwBaseTimeout);
	BOOL DeleteLock(CWebDavLock *pLockList);
	void DeInitAndFreeLock(CWebDavLock *pLockList);

	BOOL HasLockIdInArray(LockIdArray *pLockIDs, int numLockIds);
	BOOL HasLockId(__int64 iLockId, CWebDavLock **ppLockMatch=NULL);

	void SetBusy(BOOL b) { m_fBusy = b;    }
	BOOL IsBusy(void)    { return m_fBusy; }
};


class CLockParse;

// Note: There is only one CWebDavFileLockManager for all websites, because locks
// refer to resources on the server (aka files) rather than URLs.  Also note that the
// lock manager is deleted when the web server is unloaded, not when the web server is
// refreshed (this is contrary to all other high level classes).  Just changing some
// parameters in web server and refreshing should not invalidate filesystem lock state.

class CWebDavFileLockManager : public SVSSynch { 
	CWebDavFileNode  *m_pFileLockList;    // pointer to active file locks
	__int64          m_iNextLockId;       // next ID to assign for next lock created

	FixedMemDescr	*m_pfmdFileNodes;  // CWebDavFileNode fixed mem blocks
	FixedMemDescr	*m_pfmdLocks;         // CWebDavLock fixed mem blocks


public:
	CHAR             m_szLockGUID[SVSUTIL_GUID_STR_LENGTH+1]; // GUID that prepends all lock IDs.

	CWebDavFileLockManager();
	~CWebDavFileLockManager();
	
	void RemoveTimedOutLocks(void);

	BOOL CreateLock(CLockParse *pLockParams, CWebDav *pWebDav);

	CWebDavFileNode * GetNodeFromID(__int64 i64LockId, CWebDavLock **ppLockMatch=NULL);
	CWebDavFileNode * GetNodeFromFileName(WCHAR *szFileName);
	BOOL IsFileAssociatedWithLockId(WCHAR *szFileName, __int64 iLockId);


	BOOL IsInitialized(void) {
		return (m_pfmdFileNodes && m_pfmdLocks);
	}

	BOOL DeleteLock(__int64 iLockID);

	CWebDavLock *AllocFixedLock(void)      { return (CWebDavLock*) svsutil_GetFixed(m_pfmdLocks); }
	void FreeFixedLock(CWebDavLock *pLock) { svsutil_FreeFixed(pLock,m_pfmdLocks); }

	__int64 GetNextID(void) { return m_iNextLockId++; }

	void RemoveFileNodeFromLinkedList(CWebDavFileNode *pNode);
	void DeInitAndFreeFileNode(CWebDavFileNode *pFileNode);

	BOOL CanPerformLockedOperation(WCHAR *szFile1, WCHAR *szFile2, LockIdArray *pLockIDs, int numLocks, CWebDavFileNode **ppFileNode1, CWebDavFileNode **ppFileNode2);
};

// Utility class that walks along a comma separated header
class CHeaderIter { 
	PSTR m_szStartString;
	PSTR m_szNextString;
	PSTR m_szSave;
	CHAR cSave;

public:
	CHeaderIter(PSTR szHeaderStart) {
		m_szNextString = m_szStartString = szHeaderStart;
		m_szSave = NULL;
	}

	void ResetSaved(void) {
		if (m_szSave) {
			*m_szSave = cSave;
			m_szSave = NULL;
		}
	}

	~CHeaderIter() {
		ResetSaved();
	}

	PSTR GetNext(void); 
};

