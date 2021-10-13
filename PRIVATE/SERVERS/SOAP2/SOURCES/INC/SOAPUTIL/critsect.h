//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      CritSect.h
//
// Contents:
//
//      CCritSect class declaration - CRITICAL_SECTION wrapper class
//
//-----------------------------------------------------------------------------


#ifndef __CRITSECT_H_INCLUDED__
#define __CRITSECT_H_INCLUDED__


    class CCritSect
    {
    public:
        CCritSect();

        HRESULT Initialize();
        HRESULT Delete();
        HRESULT Enter();
        HRESULT Leave();

        HRESULT TryEnter();

    private:
        //
        // Critical section member variable
        //
        CRITICAL_SECTION    m_cs;

        //
        // Debugging support
        //
     //   #if defined(_DEBUG) && !defined(UNDER_CE)
     //       bool                m_bInitialized;
     //       int                 m_nLockCount;
     //   #endif

     //   #if defined(_DEBUG) && defined(UNDER_CE)
        	BOOL 			    m_bInitialized;
        	int                 m_nLockCount;
     //   #endif
        
    };

    class CCritSectWrapper
    {
    public:
        CCritSectWrapper();
        CCritSectWrapper(CCritSect* crit, BOOL bDelete = FALSE);
        ~CCritSectWrapper();
        HRESULT init(CCritSect* crit, BOOL bDelete = FALSE);
        HRESULT Enter(void);
        HRESULT Leave();

    private:
        CCritSect * pCritSect;
        BOOL fEntered;
        BOOL fDelete;
    };


#endif //__CRITSECT_H_INCLUDED__
