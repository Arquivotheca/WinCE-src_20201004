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
//      CritSect.cpp
//
// Contents:
//
//      CCritSect class implemenation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#include <windows.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CCritSect::CCritSect()
//
//  parameters:
//          
//  description:
//          Constructor - sets critical section structure to zeros
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CCritSect::CCritSect()
{
    ::memset(&m_cs, 0, sizeof(CRITICAL_SECTION));

    DBG_CODE(m_bInitialized = false);
    DBG_CODE(m_nLockCount   = 0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCritSect::Initialize()
//
//  parameters:
//          
//  description:
//          Initializes critical section
//  returns:
//          S_OK if succeeded, E_OUTOFMEMORY or E_FAIL if failed
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCritSect::Initialize()
{
    ASSERT(m_bInitialized == false);

    __try
    {
        ::InitializeCriticalSection(& m_cs);
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        if ( _exception_code() == STATUS_NO_MEMORY )
        {
            return E_OUTOFMEMORY;
        }
        else
        {
            return E_FAIL;
        }
    }

    DBG_CODE(m_bInitialized = true);
    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCritSect::Delete()
//
//  parameters:
//          
//  description:
//          Deletes critical section
//  returns:
//          S_OK
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCritSect::Delete()
{
#ifndef UNDER_CE
    ASSERT(m_bInitialized == true);
#else
    ASSERT(m_bInitialized == TRUE);
#endif

    ASSERT(m_nLockCount   == 0);

    ::DeleteCriticalSection(& m_cs);

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCritSect::Enter()
//
//  parameters:
//          
//  description:
//          Gets ownership over the critical section
//  returns:
//          S_OK on success, E_OUTOFMEMORY or E_FAIL on failure
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCritSect::Enter()
{
#ifndef UNDER_CE
    ASSERT(m_bInitialized == true);
#else
    ASSERT(m_bInitialized == TRUE);
#endif


    __try
    {
        ::EnterCriticalSection(& m_cs);
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        switch ( _exception_code() )
        {
        case STATUS_INVALID_HANDLE:
        case STATUS_NO_MEMORY:
            return E_OUTOFMEMORY;
        default:
            return E_FAIL;
        }
    }

    DBG_CODE(m_nLockCount ++);

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCritSect::TryEnter()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCritSect::TryEnter()
{
#ifndef UNDER_CE
    ASSERT(m_bInitialized == true);
#else
    ASSERT(m_bInitialized == TRUE);
#endif


    __try
    {
        return ::TryEnterCriticalSection(& m_cs) ? DBG_CODE(m_nLockCount ++), S_OK : S_FALSE;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        switch ( _exception_code() )
        {
        case STATUS_INVALID_HANDLE:
        case STATUS_NO_MEMORY:
            return E_OUTOFMEMORY;
        default:
            return E_FAIL;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCritSect::Leave()
//
//  parameters:
//          
//  description:
//          Releases ownership over the critical section
//  returns:
//          S_OK
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCritSect::Leave()
{
#ifndef UNDER_CE
    ASSERT(m_bInitialized == true);
#else
    ASSERT(m_bInitialized == TRUE);
#endif

    DBG_CODE(m_nLockCount --);

    ::LeaveCriticalSection(& m_cs);

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


CCritSectWrapper::CCritSectWrapper() : 
    pCritSect(NULL), 
    fEntered(FALSE),
    fDelete(FALSE)
{
};

CCritSectWrapper::CCritSectWrapper(CCritSect* crit, BOOL fDeleteFlag) : 
    pCritSect(NULL), 
    fEntered(FALSE),
    fDelete(FALSE)
{
    pCritSect = crit;
    fEntered = FALSE;
    fDelete = fDeleteFlag;
};

CCritSectWrapper::~CCritSectWrapper()
{
    Leave();
}


HRESULT CCritSectWrapper::init(CCritSect* crit, BOOL fDeleteFlag)
{
    HRESULT hr=S_OK;

    if (!crit)
        return (S_OK);
    
    CHK_BOOL(pCritSect == NULL, E_FAIL);
    
    pCritSect = crit;
    fDelete = fDeleteFlag;

Cleanup:
    return hr;
}



HRESULT CCritSectWrapper::Enter(void)
{
    HRESULT hr=S_OK;

    CHK_BOOL(pCritSect, E_FAIL);
    CHK_BOOL(fEntered == FALSE, E_FAIL);    // for the wrapper class we only allow one enter 

    CHK(pCritSect->Enter());
    fEntered = TRUE; 
    
Cleanup:
    return hr;
 }


HRESULT CCritSectWrapper::Leave(void)
{
    HRESULT hr=S_OK;

    if (!pCritSect)
        return S_OK;

    if (fEntered)
    {
        CHK(pCritSect->Leave());
        fEntered = FALSE;
    }

    if (fDelete)
    {
        CHK(pCritSect->Delete());
        fDelete = FALSE;
        pCritSect = NULL;
    }
    
Cleanup:
    return hr;
}


