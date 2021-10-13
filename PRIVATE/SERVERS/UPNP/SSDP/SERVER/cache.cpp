//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>
#pragma hdrstop



#define BUF_SIZE 100
#define CACHE_RESULT_SIZE 2

// To-Do: Set cache size limit and clean up cache when it reaches the limit.

// USN is the key to the list, no duplicate USNs.

static LIST_ENTRY listCache;
static CRITICAL_SECTION CSListCache;
static CTETimer timerCleanup;
static PSSDP_CACHE_ENTRY pCacheEntryFirst; 

#ifndef UNDER_CE
static LPCTSTR g_szSystemDir = TEXT("windir");
#else
static const TCHAR g_szWinDir[] = TEXT("\\windows");
#endif
static LPCTSTR g_szCacheFileName = TEXT("\\ssdpcache.txt"); 

BOOL CacheEntryExpired(SSDP_CACHE_ENTRY *CacheEntry);
VOID FreeSsdpCacheEntry(PSSDP_CACHE_ENTRY CacheEntry);
VOID PrintListCache();
VOID RemoveFromListCache(PSSDP_CACHE_ENTRY CacheEntry, PSSDP_REQUEST pSsdpRequest);
VOID RemoveEntryWithNotification(PSSDP_CACHE_ENTRY CacheEntry, PSSDP_REQUEST pSsdpRequest);
VOID FileTimeToString(FILETIME FileTime, CHAR *szBuf, INT BufSize);

VOID CacheTimerProc (CTETimer *Timer, VOID *Arg)
{
    PSSDP_CACHE_ENTRY pCacheEntryExpired = (PSSDP_CACHE_ENTRY) Arg;

    TraceTag(ttidSsdpCache, "+Enter CacheTimerProc %x", pCacheEntryExpired);

    EnterCriticalSection(&CSListCache);

    ConvertToByebyeNotify(&pCacheEntryExpired->SsdpRequest); 

    if (pCacheEntryExpired == pCacheEntryFirst)
    {
        TraceTag(ttidSsdpCache, "+Expired entry %x is the current ealiest %x",
                 pCacheEntryExpired, pCacheEntryFirst);

        RemoveFromListCache(pCacheEntryExpired, &pCacheEntryExpired->SsdpRequest); 
    } else 
    {
        // This can only happen if update occurred while we are in this proc,
        // but before EnterCriticalSection.  The update was treated as adding
        // a new item and a cleanup timer is already restarted as a result. 
        
        TraceTag(ttidSsdpCache, "+Expired entry %x is not the current ealiest %x",
                 pCacheEntryExpired, pCacheEntryFirst);

        RemoveEntryWithNotification(pCacheEntryExpired, &pCacheEntryExpired->SsdpRequest); 
    }
    LeaveCriticalSection(&CSListCache);

    TraceTag(ttidSsdpCache, "-Leave CacheTimerProc %x", pCacheEntryExpired);

}

VOID InitializeListCache()
{
    InitializeCriticalSection(&CSListCache);
    EnterCriticalSection(&CSListCache);
    InitializeListHead(&listCache);
    CTEInitTimer(&timerCleanup);
    pCacheEntryFirst = NULL; 
    LeaveCriticalSection(&CSListCache);
}

// This should only be called during service shut down.
// Call CleanupListCache to clean up the cache while service is running.
VOID DestroyListCache()
{
    CTEStopTimer(&timerCleanup);
    CleanupListCache();
    DeleteCriticalSection(&CSListCache);
}

#if 0
VOID CheckListCacheForNotification(PSSDP_NOTIFY_REQUEST pNotifyRequest)
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &listCache;

    TraceTag(ttidSsdpCache, "----- Check List Cache for Notification -----");

    EnterCriticalSection(&CSListCache);
    for (p = pListHead->Flink; p != pListHead;)
    {

        PSSDP_CACHE_ENTRY CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        p = p->Flink;

        if (IsMatchingAliveByebye(pNotifyRequest, &CacheEntry->SsdpRequest) == TRUE)
        {
             QueuePendingNotification(pNotifyRequest, &CacheEntry->SsdpRequest); 
        }
    }

    LeaveCriticalSection(&CSListCache);

}
#endif

VOID CleanupListCache()
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &listCache;

    TraceTag(ttidSsdpCache, "----- Cleanup SSDP Cache List -----");

    EnterCriticalSection(&CSListCache);
    for (p = pListHead->Flink; p != pListHead;)
    {

        PSSDP_CACHE_ENTRY CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        p = p->Flink;

        RemoveEntryList(&(CacheEntry->linkage));

        FreeSsdpCacheEntry(CacheEntry);
    }

    LeaveCriticalSection(&CSListCache);

    // To-Do: Stop the cache cleanup timer. 
}

BOOL UpdateExpireTime(PSSDP_CACHE_ENTRY CacheEntry, SSDP_REQUEST *SsdpRequest)
{
    INT Temp;

    FILETIME TempTime;

#ifdef DBG
    CHAR szBuf[BUF_SIZE];
#endif // DBG

    Temp = GetMaxAgeFromCacheControl(SsdpRequest->Headers[SSDP_CACHECONTROL]);

    if (Temp <= 0)
    {
        TraceTag(ttidSsdpCache, "Negative value of max_age %s",
                 SsdpRequest->Headers[SSDP_CACHECONTROL]);
        return FALSE;
    }

    GetSystemTimeAsFileTime(&TempTime);

    // FILETIME is in 100 nano seconds, max-age is in seconds.
    CacheEntry->ExpireTime = ULONGLONG_FROM_FILETIME(TempTime) + (((ULONGLONG) Temp) * 10000000);

#ifdef DBG

    FileTimeToString(TempTime, szBuf, BUF_SIZE);

    TraceTag(ttidSsdpCache, "Current Time is %s", szBuf);

    FileTimeToString(FILETIME_FROM_ULONGLONG(CacheEntry->ExpireTime), szBuf, BUF_SIZE);

    TraceTag(ttidSsdpCache, "Expire Time is %s", szBuf);

#endif //DBG

    return TRUE;
}

SSDP_CACHE_ENTRY *CreateCacheEntryFromRequest(SSDP_REQUEST *SsdpRequest, ULONGLONG ExpireTime)
{

    PSSDP_CACHE_ENTRY CacheEntry;

    INT Size;

    Size = sizeof(SSDP_CACHE_ENTRY);

    CacheEntry = (PSSDP_CACHE_ENTRY) malloc (Size);

    if (CacheEntry == NULL)
    {
        TraceTag(ttidSsdpCache, "Couldn't allocate memory for CacheEntry "
                 "of %s", SsdpRequest->Headers[SSDP_NT]);
        return NULL;
    }
    CacheEntry->Type =  SSDP_CACHE_ENTRY_SIGNATURE;

    CacheEntry->Size = Size;

    if (ExpireTime != 0)
    {
        CacheEntry->ExpireTime = ExpireTime; 
    } else {
        if (UpdateExpireTime(CacheEntry, SsdpRequest) == FALSE)
        {
            free(CacheEntry);
            return NULL;
        }
    }
    
    CacheEntry->SsdpRequest = (* SsdpRequest);

    return CacheEntry;
}

VOID FreeSsdpCacheEntry(PSSDP_CACHE_ENTRY CacheEntry)
{
    FreeSsdpRequest(&CacheEntry->SsdpRequest);
    free(CacheEntry);
}
// Pre-Condition: The lock for cache list is held. 

BOOL UpdateCacheEntry(PSSDP_CACHE_ENTRY pCacheEntry, SSDP_REQUEST *pSsdpRequest)
{
    if (UpdateExpireTime(pCacheEntry, pSsdpRequest) == FALSE)
    {
        return FALSE;
    }

    if (CompareSsdpRequest(&pCacheEntry->SsdpRequest, pSsdpRequest) == FALSE)
    {
        CheckListNotifyForAliveByebye(pSsdpRequest); 
    }

    FreeSsdpRequest(&pCacheEntry->SsdpRequest);

    pCacheEntry->SsdpRequest = (* pSsdpRequest);

    return TRUE;
}

VOID AddToListCache(PSSDP_CACHE_ENTRY pCacheEntry)
{
    TraceTag(ttidSsdpCache, "+Enter AddToListCache %x", pCacheEntry);
    EnterCriticalSection(&CSListCache);
    
    if (IsListEmpty(&listCache))
    {
        ULONGLONG Timeout;
        FILETIME ftCurrent; 
        VOID *pTimer;

#ifdef DBG
        CHAR szBuf[BUF_SIZE];
#endif 
        TraceTag(ttidSsdpCache, "The list is empty when adding %x", pCacheEntry);          

        GetSystemTimeAsFileTime(&ftCurrent); 
#ifdef DBG
        FileTimeToString(ftCurrent, szBuf, BUF_SIZE);

        TraceTag(ttidSsdpCache, "Current Time is %s", szBuf);

        FileTimeToString(FILETIME_FROM_ULONGLONG(pCacheEntry->ExpireTime), szBuf, BUF_SIZE);

        TraceTag(ttidSsdpCache, "Expire Time is %s", szBuf);
#endif

        if (pCacheEntry->ExpireTime > ULONGLONG_FROM_FILETIME(ftCurrent))
        {
            TraceTag(ttidSsdpCache, "%x has not expired, insert to list", pCacheEntry);
            InsertHeadList(&listCache, &(pCacheEntry->linkage));
            Timeout = (pCacheEntry->ExpireTime - ULONGLONG_FROM_FILETIME(ftCurrent))/10000;
            pCacheEntryFirst = pCacheEntry; 
            // currently CTEStartTimer only takes 32 bit timeouts
            pTimer = CTEStartTimer(&timerCleanup, (ULONG)Timeout, CacheTimerProc,
                                   (VOID *) pCacheEntry);

            // Check return value of CTEStartTimer. 

            // Queue notifications

            CheckListNotifyForAliveByebye(&pCacheEntry->SsdpRequest); 

        } else {
            TraceTag(ttidSsdpCache, "%x already expired, no need to add", pCacheEntry);
        }
    } else 
    {
        TraceTag(ttidSsdpCache, "The list is NOT empty when adding %x", pCacheEntry);      
        if (pCacheEntry->ExpireTime >= pCacheEntryFirst->ExpireTime)
        {
            TraceTag(ttidSsdpCache, "%x is not ealier than the current earliest, just insert", pCacheEntry);   
            InsertHeadList(&listCache, &(pCacheEntry->linkage));
            CheckListNotifyForAliveByebye(&pCacheEntry->SsdpRequest); 
        } else {
            TraceTag(ttidSsdpCache, "%x is earlier than the current earliest", pCacheEntry);   
            if (CTEStopTimer(&timerCleanup))
            {
                ULONGLONG Timeout;
                FILETIME ftCurrent; 
                VOID *pTimer;

                TraceTag(ttidSsdpCache, "Successfully stopped the cleanup timer", pCacheEntry);   
                InsertHeadList(&listCache, &(pCacheEntry->linkage));
                
                CheckListNotifyForAliveByebye(&pCacheEntry->SsdpRequest); 

                GetSystemTimeAsFileTime(&ftCurrent); 

                if (pCacheEntry->ExpireTime > ULONGLONG_FROM_FILETIME(ftCurrent))
                {
                    Timeout = (pCacheEntry->ExpireTime - ULONGLONG_FROM_FILETIME(ftCurrent))/10000;
                } else 
                {
                    Timeout = 0; 
                }

                pCacheEntryFirst = pCacheEntry; 

				// currently CTEStartTimer only takes 32 bit timeouts
                pTimer = CTEStartTimer(&timerCleanup, (ULONG)Timeout, CacheTimerProc,
                                       (VOID *) pCacheEntry);
            } else {
                // The timer proc is running and I am ealier than the expired entry, 
                // so I am expired as well, thus ignore 
                TraceTag(ttidSsdpCache, "Failed to stop the cleanup timer for %x, skip add", pCacheEntry);   
            }
        }
    }

    LeaveCriticalSection(&CSListCache);

    TraceTag(ttidSsdpCache, "-Leave AddToListCache %x.", pCacheEntry);
}

VOID RemoveEntryWithNotification(PSSDP_CACHE_ENTRY CacheEntry, PSSDP_REQUEST pSsdpRequest)
{
    RemoveEntryList(&(CacheEntry->linkage));
    
    CheckListNotifyForAliveByebye(pSsdpRequest); 

    FreeSsdpCacheEntry(CacheEntry);

}
VOID StartCacheCleanupTimer()
{
    EnterCriticalSection(&CSListCache);

    if (IsListEmpty(&listCache)) {
        TraceTag(ttidSsdpCache, "No more cache entry left."); 
    } else 
    {
        ULONGLONG ullFirst = _UI64_MAX; 
        PLIST_ENTRY p;
        PLIST_ENTRY pListHead = &listCache; 

        TraceTag(ttidSsdpCache, "Cache list is not empty after removing"); 
        
        for (p = pListHead->Flink; p != pListHead;)
        {
            PSSDP_CACHE_ENTRY CacheEntry;

            CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

            p = p->Flink;

            if (CacheEntry->ExpireTime <= ullFirst) {
                ullFirst = CacheEntry->ExpireTime; 
                pCacheEntryFirst = CacheEntry; 
            }
        }

        TraceTag(ttidSsdpCache, "The new earliest cache entry is %x", pCacheEntryFirst); 

        Assert(pCacheEntryFirst != NULL); 

        ULONGLONG Timeout;
        FILETIME ftCurrent; 
        VOID *pTimer;

        GetSystemTimeAsFileTime(&ftCurrent); 

        if (ullFirst > ULONGLONG_FROM_FILETIME(ftCurrent))
        {
            Timeout = (ullFirst - ULONGLONG_FROM_FILETIME(ftCurrent))/10000; 
        } else {
            // Expired
            Timeout = 0; 
        }

        // currently CTEStartTimer only takes 32 bit timeouts
        pTimer = CTEStartTimer(&timerCleanup, (ULONG)Timeout,CacheTimerProc,
                               (VOID *) pCacheEntryFirst);
    }
    LeaveCriticalSection(&CSListCache);
}

VOID RemoveFromListCache(PSSDP_CACHE_ENTRY CacheEntry, PSSDP_REQUEST pSsdpRequest)
{
    TraceTag(ttidSsdpCache, "+Enter RemoveFromListCache %x", CacheEntry);

    EnterCriticalSection(&CSListCache);

    RemoveEntryWithNotification(CacheEntry, pSsdpRequest); 

    pCacheEntryFirst = NULL; 

    StartCacheCleanupTimer(); 

    LeaveCriticalSection(&CSListCache);
    
}

//+---------------------------------------------------------------------------
//
//  Function:   UpdateListCache
//
//  Purpose:    Update the cache with ssdp:alive messagesand M-SEARCH results
//
//  Arguments:
//
//  Returns:    TRUE if listCache takes ownership of the memory within 
//              SsdpRequest; FALSE to have caller free the memory. 
//
//  
//
//  Notes:
//
//  If no CacheControl header, don't cache. 
//  If exists in cache, update if alive or search result, remove if byebye, 
//  If not in cache, add to the cache if subscribed and it is alive 
//  Don't do bRetVal = FALSE, in case you have to, make sure you don't 
//  override a TRUE; 

BOOL UpdateListCache(PSSDP_REQUEST pSsdpRequest, BOOL IsSubscribed)
{
    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listCache;
    BOOL found = FALSE;
    BOOL IsByebye;
    BOOL bRetVal = FALSE; 

    if (pSsdpRequest->Headers[SSDP_NTS] != NULL &&
        strcmp(pSsdpRequest->Headers[SSDP_NTS], "ssdp:byebye") == 0)
    {
        IsByebye = TRUE;
    }
    else
    {
        IsByebye = FALSE;
    }

    if (pSsdpRequest->Headers[SSDP_CACHECONTROL] == NULL)
    {
        // To-Do: 

        TraceTag(ttidSsdpCache, "Couldn't find cache control header, not cachable");
        return bRetVal;
    }

    EnterCriticalSection(&CSListCache);

    for (p = pListHead->Flink; p != pListHead; )
    {
        SSDP_CACHE_ENTRY *CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        p = p->Flink;

        if (strcmp(CacheEntry->SsdpRequest.Headers[SSDP_USN],
                   pSsdpRequest->Headers[SSDP_USN]) == 0)
        {
            found = TRUE;  // even if update fails.

            TraceTag(ttidSsdpCache, "Found matching cache entry %x", CacheEntry);

            if (IsByebye)
            {
                // byebye
                if (CacheEntry == pCacheEntryFirst)
                {
                    TraceTag(ttidSsdpCache, "Byebye is for the current earliest %x", CacheEntry); 

                    if (CTEStopTimer(&timerCleanup))
                    {
                        TraceTag(ttidSsdpCache, "Stopped cleanup timer for %x", CacheEntry); 
                        RemoveFromListCache(CacheEntry, pSsdpRequest); 
                    } else {
                        TraceTag(ttidSsdpCache, "Received byebye for cache entry %x, timer \
                                 is running, skip remove!", CacheEntry);
                    }
                } else 
                {
                    TraceTag(ttidSsdpCache, "Byebye for %x which is not current earliest, just remove", CacheEntry); 
                    RemoveEntryWithNotification(CacheEntry, pSsdpRequest); 
                }

            }
            else
            {
                // alive or search result, cache list owns the memory in SsdpRequest if updated
                if (CacheEntry != pCacheEntryFirst)
                {
                    bRetVal = UpdateCacheEntry(CacheEntry, pSsdpRequest);
                } else 
                {
                    if (CTEStopTimer(&timerCleanup))
                    {
                        bRetVal = UpdateCacheEntry(CacheEntry, pSsdpRequest);
                        
                        StartCacheCleanupTimer(); 
                    
                    } else 
                    {
                        // Too late, the timer has fired. 
                        // The current entry will be removed by timer proc. 
                        // Insert a new one. 

						found = FALSE;
						break;
                    }
                }

            }
#ifdef DBG
            PrintListCache();
#endif // DBG
            break; 
        }
    }

    if (!found && !IsByebye && IsSubscribed)
    {
        PSSDP_CACHE_ENTRY CacheEntry = CreateCacheEntryFromRequest(pSsdpRequest,0);
        if (CacheEntry != NULL)
        {
            AddToListCache(CacheEntry);
            TraceTag(ttidSsdpCache, "Created cache entry %x", CacheEntry);
            bRetVal = TRUE; 
#ifdef DBG
            PrintListCache();
#endif // DBG
        }
        else
        {
            TraceTag(ttidSsdpCache, "Couldn't create cache entry");
        }
    } 

    LeaveCriticalSection(&CSListCache);

    return bRetVal; 
}

VOID PrintCacheEntry(SSDP_CACHE_ENTRY *CacheEntry)
{
    CHAR szBuf[BUF_SIZE];

    TraceTag(ttidSsdpCache, "----- SSDP Cache Entry %x-----",CacheEntry);

    FileTimeToString(FILETIME_FROM_ULONGLONG(CacheEntry->ExpireTime), szBuf, BUF_SIZE);

    TraceTag(ttidSsdpCache, "Expired ? : %d", CacheEntryExpired(CacheEntry));
    TraceTag(ttidSsdpCache, "Expire Time is %s", szBuf);

    PrintSsdpRequest(&CacheEntry->SsdpRequest);
}

VOID PrintListCache()
{
    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listCache;

    TraceTag(ttidSsdpCache, "----- SSDP Cache List -----");

    EnterCriticalSection(&CSListCache);

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {

        SSDP_CACHE_ENTRY *CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        PrintCacheEntry(CacheEntry);

    }
    LeaveCriticalSection(&CSListCache);
}

BOOL WriteCacheEntryToFile(HANDLE CacheFile, PSSDP_CACHE_ENTRY CacheEntry)
{
    CHAR *Buffer = NULL;
    DWORD BufferSize = 0;
    DWORD BytesWritten = 0;

    ComposeSsdpRequest(&CacheEntry->SsdpRequest, &Buffer);
    BufferSize = strlen(Buffer) + 1;

    // Write the absolute expire time to file.

    WriteFile(CacheFile, (CHAR *) &BufferSize, sizeof(INT), &BytesWritten, NULL);
    if (BytesWritten != sizeof(INT))
    {
        TraceTag(ttidSsdpCache, "----- Write size failed. %d written, should be "
                 "%d -----", BytesWritten, BufferSize);
        free(Buffer);
        return FALSE;
    }

    WriteFile(CacheFile, Buffer, BufferSize, &BytesWritten, NULL);
    free(Buffer);
   
    if (BufferSize != BytesWritten)
    {
        TraceTag(ttidSsdpCache, "----- Write failed. %d written, should "
                 "be %d -----", BytesWritten, BufferSize);
        return FALSE;
    }

    WriteFile(CacheFile, (CHAR *) &CacheEntry->ExpireTime, 
              sizeof(ULONGLONG), &BytesWritten, NULL); 

    if (BytesWritten != sizeof(ULONGLONG))
    {
        TraceTag(ttidSsdpCache, "----- Write Expire time failed. %d written, should be "
                 "%d -----", BytesWritten, BufferSize);
        free(Buffer);
        return FALSE;
    }

    return TRUE;
}

VOID ReadCacheFileToList()
{
    HANDLE CacheFile;
    DWORD EntrySize;
    DWORD BytesRead = 0;

    TCHAR CacheFilePath[BUF_SIZE]; 

    TraceTag(ttidSsdpCache, "----- Reading SSDP Cache File -----");

#ifdef UNDER_CE
	memcpy(CacheFilePath,g_szWinDir,sizeof(g_szWinDir));
#else
    GetEnvironmentVariable(g_szSystemDir, CacheFilePath, BUF_SIZE); 
#endif    

    lstrcat(CacheFilePath, g_szCacheFileName);  

    CacheFile = CreateFile(CacheFilePath, GENERIC_READ, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (CacheFile == INVALID_HANDLE_VALUE)
    {
        TraceTag(ttidSsdpCache, "Failed to open cache file, error (%d).",
                 GetLastError());
        // To-Do: Event Log.
        return;
    }

    while (ReadFile(CacheFile, (CHAR *) &EntrySize, sizeof(INT),
                    &BytesRead, NULL) && BytesRead > 0)
    {
        CHAR *Buffer;
        SSDP_REQUEST SsdpRequest;
        ULONGLONG ExpireTime; 
        PSSDP_CACHE_ENTRY CacheEntry; 
		BOOL bOK;

        Assert(EntrySize > 0);

        // Should terminate with '\0'
        Buffer = (CHAR *) malloc(sizeof(CHAR) * EntrySize);

        bOK = ReadFile(CacheFile, Buffer, EntrySize, &BytesRead, NULL);

        if (!bOK || BytesRead != EntrySize)
        {
            TraceTag(ttidSsdpCache, "Need %d, read %d, error %d.", EntrySize,
                     BytesRead, GetLastError());
            return;
        }
        Assert(Buffer[EntrySize-1] == '\0');

        bOK = ReadFile(CacheFile, (CHAR *) &ExpireTime, sizeof(ULONGLONG),
                 &BytesRead, NULL); 

        if (!bOK || BytesRead != sizeof(ULONGLONG))
        {
            TraceTag(ttidSsdpCache, "Need %d, read %d, error %d.", sizeof(ULONGLONG),
                     BytesRead, GetLastError());
            return;
        }

        InitializeSsdpRequest(&SsdpRequest);

        ParseHeaders(Buffer, &SsdpRequest);

        CacheEntry = CreateCacheEntryFromRequest(&SsdpRequest,ExpireTime);
        if (CacheEntry != NULL && !CacheEntryExpired(CacheEntry))
        {
            AddToListCache(CacheEntry);
            TraceTag(ttidSsdpCache, "Created cache entry %x", CacheEntry);
        }
        else
        {
            TraceTag(ttidSsdpCache, "Couldn't create cache entry");
            FreeSsdpRequest(&SsdpRequest); 
        }

        free(Buffer);
    }

    CloseHandle(CacheFile);
}

VOID WriteListCacheToFile()
{
    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listCache;
    HANDLE CacheFile;
    TCHAR CacheFilePath[BUF_SIZE]; 

    TraceTag(ttidSsdpCache, "----- Writing SSDP Cache List to File -----");

#ifdef UNDER_CE
	memcpy(CacheFilePath,g_szWinDir,sizeof(g_szWinDir));
#else
    GetEnvironmentVariable(g_szSystemDir, CacheFilePath, BUF_SIZE);
#endif

    lstrcat(CacheFilePath, g_szCacheFileName);  

    CacheFile = CreateFile(CacheFilePath, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, 0, NULL);

    if (CacheFile == INVALID_HANDLE_VALUE)
    {
        TraceTag(ttidSsdpCache, "Failed to open file to write the cache. (%d)",
                 GetLastError());
        // To-Do: Event Log.
        return;
    }

    EnterCriticalSection(&CSListCache);

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        SSDP_CACHE_ENTRY *CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        WriteCacheEntryToFile(CacheFile, CacheEntry);
    }

    LeaveCriticalSection(&CSListCache);
    CloseHandle(CacheFile);
}

BOOL CacheEntryExpired(SSDP_CACHE_ENTRY *CacheEntry)
{
    FILETIME TempTime;

    ULONGLONG CurrentTime;

#ifdef DBG
    CHAR szBuf[BUF_SIZE];
#endif // DBG

    GetSystemTimeAsFileTime(&TempTime);

    // FILETIME is in 100 nano seconds, max-age is in seconds.
    CurrentTime = ULONGLONG_FROM_FILETIME(TempTime); 

#ifdef DBG
    FileTimeToString(TempTime, szBuf, BUF_SIZE);

    TraceTag(ttidSsdpCache, "Current Time is %s", szBuf);

    FileTimeToString(FILETIME_FROM_ULONGLONG(CacheEntry->ExpireTime), szBuf, BUF_SIZE);

    TraceTag(ttidSsdpCache, "Expire Time is %s", szBuf);
#endif // DBG

    if (CurrentTime < CacheEntry->ExpireTime)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

INT SearchListCache(CHAR *szType, SSDP_MESSAGE ***svcList)
{
    LIST_ENTRY *pListHead = &listCache;
    PLIST_ENTRY p;
    SSDP_MESSAGE **pSsdpMessageList = NULL;

    INT nHits = 0;

    

    EnterCriticalSection(&CSListCache);

    TraceTag(ttidSsdpCache, "Searching cache list for %s", szType);

	// First scan cache to calculate number of matches
    p = pListHead->Flink;
    while (p != pListHead)
    {
        SSDP_CACHE_ENTRY *CacheEntry;

        CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

        p = p->Flink;

        if (CacheEntryExpired(CacheEntry))
        {
            TraceTag(ttidSsdpCache, " !!!!! WARNING !!!!!! : Cache entry 0x%08X has expired. Continue to the next one.",
                     CacheEntry);
            continue;
        }

        // To-Do: Should this be case insensitive?

        if ((strcmp(szType, "ssdp:all") == 0) ||
            (CacheEntry->SsdpRequest.Headers[SSDP_NT] != NULL &&
             strcmp(CacheEntry->SsdpRequest.Headers[SSDP_NT], szType) == 0) ||
            (CacheEntry->SsdpRequest.Headers[SSDP_ST] != NULL &&
             strcmp(CacheEntry->SsdpRequest.Headers[SSDP_ST], szType) == 0))
        {
			nHits++;
		}
	}
	if (nHits)
	{
		// allocate memory
		pSsdpMessageList = (PSSDP_MESSAGE *)malloc(sizeof(PSSDP_MESSAGE) * nHits);
		nHits = 0;
		if (pSsdpMessageList)
		{
			// Now scan cache to copy the matched contents
			p = pListHead->Flink;
			while (p != pListHead)
			{
			    SSDP_CACHE_ENTRY *CacheEntry;

			    CacheEntry = CONTAINING_RECORD (p, SSDP_CACHE_ENTRY, linkage);

			    p = p->Flink;

			    if (CacheEntryExpired(CacheEntry))
			    {
			        TraceTag(ttidSsdpCache, " !!!!! WARNING !!!!!! : Cache entry 0x%08X has expired. Continue to the next one.",
			                 CacheEntry);
			        continue;
			    }

				
			    // To-Do: Should this be case insensitive?

			    if ((strcmp(szType, "ssdp:all") == 0) ||
			        (CacheEntry->SsdpRequest.Headers[SSDP_NT] != NULL &&
			         strcmp(CacheEntry->SsdpRequest.Headers[SSDP_NT], szType) == 0) ||
			        (CacheEntry->SsdpRequest.Headers[SSDP_ST] != NULL &&
			         strcmp(CacheEntry->SsdpRequest.Headers[SSDP_ST], szType) == 0))
			    {
					pSsdpMessageList[nHits] = InitializeSsdpMessageFromRequest(&CacheEntry->SsdpRequest);
					if (!pSsdpMessageList[nHits])
						break;	// out of memory
					++nHits;
				}
			}

        }
    }
    *svcList = pSsdpMessageList;
    LeaveCriticalSection(&CSListCache);

    return nHits;
}

void FreeSsdpMessageList(SSDP_MESSAGE **pSsdpMessageList, int nEntries)
{
	int i;
	for (i=0; i < nEntries;i++)
	{
		if (pSsdpMessageList[i])
			FreeSsdpMessage(pSsdpMessageList[i]);
	}
	free(pSsdpMessageList);
}

VOID FileTimeToString(FILETIME FileTime, CHAR *szBuf, INT BufSize)
{
    SYSTEMTIME SystemTime;
    INT Size;

    FileTimeToLocalFileTime(&FileTime, &FileTime);

    FileTimeToSystemTime(&FileTime,&SystemTime);

#ifdef UNDER_CE
	WCHAR wszTemp[128];
	Size=GetDateFormatW(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, NULL,
                          wszTemp, min(sizeof(wszTemp)/sizeof(WCHAR),BufSize-1) );
    wcstombs(szBuf,wszTemp,Size);

    szBuf[Size-1] = ' ';
    szBuf += Size;
    
    Size= GetTimeFormatW(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, NULL,
                   wszTemp, min(sizeof(wszTemp)/sizeof(WCHAR),BufSize-Size));
    wcstombs(szBuf, wszTemp, Size);
#else
    Size = GetDateFormatA(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, NULL,
                          szBuf, BufSize-1 );
    szBuf[Size-1] = ' ';
    GetTimeFormatA(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, NULL,
                   szBuf+Size, BufSize-Size);
#endif                   
}


VOID InitializeSsdpRequestFromMessage(
	PSSDP_REQUEST pSsdpRequest,
	const SSDP_MESSAGE *pSsdpMessage)
{
	memset(pSsdpRequest,0, sizeof(SSDP_REQUEST));
	pSsdpRequest->Method = SSDP_NOTIFY;
	pSsdpRequest->Headers[SSDP_NT] = SsdpDup(pSsdpMessage->szType);
	pSsdpRequest->Headers[SSDP_LOCATION] = SsdpDup(pSsdpMessage->szLocHeader);
	pSsdpRequest->Headers[SSDP_USN] = SsdpDup(pSsdpMessage->szUSN);
	if ((int)(pSsdpMessage->iLifeTime) >= 0
		&& (pSsdpRequest->Headers[SSDP_CACHECONTROL] = (LPSTR)SsdpAlloc(64)))
	{
		sprintf(pSsdpRequest->Headers[SSDP_CACHECONTROL],"max-age=%d", pSsdpMessage->iLifeTime);
	}
}

// Public APIs
VOID _UpdateCache(PSSDP_MESSAGE pSsdpMessage)
{
    SSDP_REQUEST SsdpRequest;


    InitializeSsdpRequestFromMessage(&SsdpRequest, pSsdpMessage);

    if (UpdateListCache(&SsdpRequest, TRUE) == FALSE)
    {
        FreeSsdpRequest(&SsdpRequest);
    }
}


//+---------------------------------------------------------------------------
//
//  Function:   CleanupCache
//
//  Purpose:    Public API to clean up SSDP cache
//
BOOL WINAPI _CleanupCache()
{
    BOOL    fResult = TRUE;

    if (cInitialized == 0)
    {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }

    CleanupListCache();

    TraceResult("CleanupCache", fResult);
    return fResult;
}


