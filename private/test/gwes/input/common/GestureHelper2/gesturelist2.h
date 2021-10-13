//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#define CPRGI(x, msg)          if(NULL == (x)) { info(FAIL, msg); goto Error; }

// Double linked list
typedef struct GestureItemTAG {
    PBYTE           pExtraGestureData;      // Pointer to extra gesture data (if exists)
    DWORD           cbExtraGestureDataSize; // Size of estra gesture data (if exists)
    DWORD           dwTimeStamp;            // Timestamp for when the gesture was received by the window
    GestureItemTAG  *pNext;                 // Pointer to the next structure in the list
    GestureItemTAG  *pPrev;                 // Pointer to the previous structure in the list
    HGESTUREINFO    hGestureInfo;           // Handle to the gesture (may be NULL if already closed)
    GESTUREINFO     giGesture;              // GEstureInfo struct associated with the gesture
} GestureItem;

class CGestureList
{
public:
    CGestureList();
    virtual ~CGestureList();

    // Add a new gesture struct to the list
    HRESULT Add(HGESTUREINFO hGestureInfo, GESTUREINFO *pGesture);
    // Add a new gesture struct plus gesture extra info
    HRESULT Add(HGESTUREINFO hGestureInfo, const GESTUREINFO *pGesture, PBYTE pExtraGestureData, DWORD cbExtraGestureDataSize);
    
    // Retrieves the last gesture received
    GestureItem *GetLast();
    // Retrieves the last but one gesture received
    GestureItem *GetBeforeLast();

    // Checks if the list is empty
    inline const BOOL IsEmpty();

    // Checks if the specific gesture item is of a specific ID
    static inline BOOL IsID(const GestureItem *gi, DWORD dwID);
    // Checks if the specific gesture item has the specified flags
    static inline BOOL HasFlags(const GestureItem *gi, DWORD dwFlags);
    // Checks if the specific gesture item has the specified instance ID
    static inline BOOL IsInstanceID(const GestureItem *gi, DWORD dwInstanceID);

    // If we close the gesture handle, we need to make sure we clean it up from our list
    // Note: calling this method when the gesture handle hasn't been closed may leak the handle if not explicitly closed
    inline void CleanLastAddedGestureHandle();

    // Cleans up the list - effectivelly deletes all it's members
    void Clear();
    
    // For iteration
    GestureItem *SearchStart();
    GestureItem *SearchNext();

    // Checks if the whole list is the given sequence
    BOOL IsWholeSequence(DWORD *GestureSequenceList, DWORD dwSequenceLength);
    // Checks if the give sequence exists starting at the gesture giStart. It 
    // only checks for dwSequenceLength, even if there are more elements in the gesture list
    BOOL HasSequence(const GestureItem *giStart, const DWORD *GestureSequenceList, DWORD dwSequenceLength);

    // Debug
    static void Dump(const GestureItem *gi);
    void DumpList();

protected:
    void DeleteAll();

    // Header
    GestureItem *m_pList;
    // Iterator (non thread-safe)
    GestureItem *m_pIterator;

    DWORD               m_dwItemCount;
    CRITICAL_SECTION    m_csCritSect;
};


inline const BOOL CGestureList::IsEmpty()
{
    return (m_pList == NULL);
}

inline BOOL CGestureList::IsID(const GestureItem *gi, DWORD dwID)
{
    return (gi && (gi->giGesture.dwID == dwID));
}

inline BOOL CGestureList::HasFlags(const GestureItem *gi, DWORD dwFlags)
{
    return (gi && (gi->giGesture.dwFlags & dwFlags));
}

inline void CGestureList::CleanLastAddedGestureHandle()
{
    if(m_pList)
    {
        m_pList->hGestureInfo = NULL;
    }
}

inline BOOL CGestureList::IsInstanceID(const GestureItem *gi, DWORD dwInstanceID)
{
    return (gi && (gi->giGesture.dwInstanceID == dwInstanceID));
}