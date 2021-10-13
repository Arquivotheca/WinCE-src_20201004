//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
        void Lock();
        void UnLock();
        ~CCritSection();
        BOOL IsLocked() { return fLocked; }
        
    private:
        BOOL fLocked;
        LPCRITICAL_SECTION lpCrit;
};


#endif
