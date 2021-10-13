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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
