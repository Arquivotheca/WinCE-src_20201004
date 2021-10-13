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
   NDIS_SPIN_LOCK m_spinLock;

public:   
   UINT m_uiItems;

public:
   CObjectList();
   ~CObjectList();

   void AddTail(CObject *pObject);
   void AddHead(CObject *pObject);
   CObject *GetHead();
   CObject *GetNext(CObject *pObject);
   void Remove(CObject *pObject);

   void AcquireSpinLock();
   void ReleaseSpinLock(); 
};

//------------------------------------------------------------------------------

#endif
