//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __OBJECT_LIST_H
#define __OBJECT_LIST_H

//------------------------------------------------------------------------------

#include "Object.h"

//------------------------------------------------------------------------------

class CObjectList
{
private:
   CObject* m_pHead;
   CObject* m_pTail;
   CRITICAL_SECTION m_cs;

public:
   CObjectList();
   ~CObjectList();

   void AddTail(CObject *pObject);
   void AddHead(CObject *pObject);
   CObject *GetHead();
   CObject *GetNext(CObject *pObject);
   void Remove(CObject *pObject);

   void EnterCriticalSection();
   void LeaveCriticalSection(); 
};

//------------------------------------------------------------------------------

#endif
