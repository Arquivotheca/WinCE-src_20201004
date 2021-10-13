//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "SMB_Globals.h"
#include "CriticalSection.h"
 
CCritSection::CCritSection(LPCRITICAL_SECTION _lpCrit)
{
    Init(_lpCrit);
}

CCritSection::CCritSection()
{
    Init(NULL);
}

void
CCritSection::Init(LPCRITICAL_SECTION _lpCrit)
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




