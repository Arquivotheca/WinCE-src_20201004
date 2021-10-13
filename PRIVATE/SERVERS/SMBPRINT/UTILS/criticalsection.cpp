//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "SMB_Globals.h"
#include "CriticalSection.h"
 
CCritSection::CCritSection(LPCRITICAL_SECTION _lpCrit)
{
    lpCrit = _lpCrit;
    fLocked = FALSE;
}

CCritSection::~CCritSection()
{
    if(fLocked)
        LeaveCriticalSection(lpCrit);
}


void 
CCritSection::Lock()
{
    ASSERT(!fLocked);
 
#ifdef DEBUG
    if(0 == TryEnterCriticalSection(lpCrit)) {
        TRACEMSG(ZONE_STATS, (L"Lock contention on 0x%x\n", (UINT)lpCrit));
        EnterCriticalSection(lpCrit);
    }
#else
    if(!fLocked)
        EnterCriticalSection(lpCrit);
#endif
    fLocked = TRUE;
}

void 
CCritSection::UnLock()
{
    ASSERT(fLocked);

    if(fLocked)
        LeaveCriticalSection(lpCrit);

    fLocked = FALSE;
}




