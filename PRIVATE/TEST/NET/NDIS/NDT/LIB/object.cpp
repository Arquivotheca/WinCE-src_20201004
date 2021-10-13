//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Object.h"

//------------------------------------------------------------------------------

CObject::CObject()
{
   m_dwMagic = NDT_MAGIC_OBJECT;
   m_nRefCount = 0;
   AddRef();
}

//------------------------------------------------------------------------------

CObject::~CObject()
{
   ASSERT(m_nRefCount <= 0);
}

//------------------------------------------------------------------------------

LONG CObject::AddRef()
{
   return InterlockedIncrement(&m_nRefCount);
}

//------------------------------------------------------------------------------

LONG CObject::Release()
{
   LONG nRefCount = InterlockedDecrement(&m_nRefCount);
   if (nRefCount == 0) delete this;
   return nRefCount;
}

//------------------------------------------------------------------------------
