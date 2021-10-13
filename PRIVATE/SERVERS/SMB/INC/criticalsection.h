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
#ifndef CCRITSECTION_H
#define CCRITSECTION_H

#include "SMB_Globals.h"

// This class is used to help abstract critical sections (and make their use 
//  a bit easier -- by using this you dont have to remember to unlock
//  the CS (you can, but its not required)
class CCritSection
{
    public:
        CCritSection(LPCRITICAL_SECTION _lpCrit);       
        CCritSection();
        
        void Init(LPCRITICAL_SECTION _lpCrit);
        void Lock();
        void UnLock();
        ~CCritSection();
        BOOL IsLocked() { return fLocked; }
        
    private:
        BOOL fLocked;
        LPCRITICAL_SECTION lpCrit;
};


#endif
