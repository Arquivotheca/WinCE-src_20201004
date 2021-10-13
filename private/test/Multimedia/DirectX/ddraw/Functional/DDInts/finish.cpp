////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: finish.cpp
//          Clean-up management functions.
//
//  Revision History:
//      07/18/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// CFinishManager::CFinishManager
//  Constructor.
//
// Parameters:
//  None.

CFinishManager::CFinishManager(void)
{
    m_pHead = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// CFinishManager::~CFinishManager
//  Destructor.

CFinishManager::~CFinishManager()
{
    FMNODE  *pNode,
            *pNext;

    for(pNode = m_pHead; pNode; pNode = pNext)
    {
        pNext = pNode->m_pNext;
        delete pNode;
    }

    m_pHead = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// CFinishManager::AddFunction
//  Adds a FinishXXX function to the chain.
//
// Parameters:
//  pfnFinish       Function to add. If NULL, nothing is added and the function
//                  return true.
//
// Return value:
//  true if successful, false if out of memory. An error is logged if it occurs.

bool CFinishManager::AddFunction(FNFINISH pfnFinish)
{
    FMNODE  *pNode;

    if(!pfnFinish)
        return true;

    // Allocate a new node
    pNode = new FMNODE;

    if(!pNode)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Out of memory while allocating FMNODE"));
        return false;
    }

    // Store the data
    pNode->m_pfnFinish = pfnFinish;
    pNode->m_pNext = m_pHead;
    m_pHead = pNode;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// CFinishManager::Finish
//  Calls the chain of finish functions and releases the chain.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void CFinishManager::Finish(void)
{
    FMNODE  *pNode,
            *pNext;

    for(pNode = m_pHead; pNode; pNode = pNext)
    {
        (pNode->m_pfnFinish)();
        pNext = pNode->m_pNext;
        delete pNode;
    }

    m_pHead = NULL;
}

////////////////////////////////////////////////////////////////////////////////
