//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

CObjectList::CObjectList()
{
   m_pHead = m_pTail = NULL;
   InitializeCriticalSection(&m_cs);
}

//------------------------------------------------------------------------------

CObjectList::~CObjectList()
{
//   ASSERT((m_pHead == NULL) && (m_pTail == NULL));
   DeleteCriticalSection(&m_cs);
}

//------------------------------------------------------------------------------

void CObjectList::AddTail(CObject *pObject)
{
   if (m_pTail == NULL && m_pHead == NULL) {
      m_pHead = m_pTail = pObject;
      pObject->m_pBLink = pObject->m_pFLink = NULL;
   } else {
      pObject->m_pBLink = m_pTail;
      pObject->m_pFLink = NULL;
      m_pTail->m_pFLink = pObject;
      m_pTail = pObject;
   }
}

//------------------------------------------------------------------------------

void CObjectList::AddHead(CObject *pObject)
{
   if (m_pTail == NULL && m_pHead == NULL) {
      m_pHead = m_pTail = pObject;
      pObject->m_pBLink = pObject->m_pFLink = NULL;
   } else {
      pObject->m_pBLink = NULL;
      pObject->m_pFLink = m_pHead;
      m_pHead->m_pBLink = pObject;
      m_pHead = pObject;
   }
}

//------------------------------------------------------------------------------

CObject* CObjectList::GetHead()
{
   return m_pHead;
}

//------------------------------------------------------------------------------

CObject* CObjectList::GetNext(CObject *pObject)
{
   return pObject->m_pFLink;
}

//------------------------------------------------------------------------------

void CObjectList::Remove(CObject *pObject)
{
   ASSERT(m_pHead != NULL && m_pTail != NULL && pObject != NULL);

   if (pObject->m_pBLink != NULL) {
      pObject->m_pBLink->m_pFLink = pObject->m_pFLink;
   } else {
      m_pHead = pObject->m_pFLink;
   }
   if (pObject->m_pFLink != NULL) {
      pObject->m_pFLink->m_pBLink = pObject->m_pBLink;
   } else {
      m_pTail = pObject->m_pBLink;
   }
   pObject->m_pBLink = pObject->m_pFLink = NULL;
}

//------------------------------------------------------------------------------

void CObjectList::EnterCriticalSection()
{
   ::EnterCriticalSection(&m_cs);
}

//------------------------------------------------------------------------------

void CObjectList::LeaveCriticalSection()
{
   ::LeaveCriticalSection(&m_cs);
}

//------------------------------------------------------------------------------
