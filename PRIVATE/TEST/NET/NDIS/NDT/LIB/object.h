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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
