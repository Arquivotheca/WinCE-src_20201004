//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __OBJECT_H
#define __OBJECT_H

//------------------------------------------------------------------------------

#define NDT_MAGIC_OBJECT            0x25116400
#define NDT_MAGIC_DRIVER_LOCAL      0x25116401
#define NDT_MAGIC_DRIVER_REMOTE     0x25116402
#define NDT_MAGIC_ADAPTER           0x25116403
#define NDT_MAGIC_REQUEST           0x25116404

//------------------------------------------------------------------------------

class CObject
{
   friend class CObjectList;
   
private:
   LONG m_nRefCount;                      // Reference counter

protected:   
   CObject* m_pFLink;                     // Forward link for a list
   CObject* m_pBLink;                     // Backward link for a list
   
public:
   DWORD m_dwMagic;                       // Magic value

public:
   CObject();
   virtual ~CObject();
   
   LONG AddRef();
   LONG Release();
};

//------------------------------------------------------------------------------

#endif
