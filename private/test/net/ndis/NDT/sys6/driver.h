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
#ifndef __DEVICE_H
#define __DEVICE_H

//------------------------------------------------------------------------------

#include "NDT_Request6.h"
#include "Object.h"

//------------------------------------------------------------------------------

class CProtocol;
class CBinding;

//------------------------------------------------------------------------------

class CDriver : public CObject
{
friend class CRequestBind;

public:
   CDriver();
   virtual ~CDriver();
   
   BOOL Init(DWORD dwContext);
   BOOL Deinit();

   NDIS_STATUS IOControl(
      NDT_ENUM_REQUEST_TYPE eRequest, PVOID pvBufferInp, DWORD cbBufferInp, 
      PVOID pvBufferOut, DWORD cbBufferOut, DWORD *pcbActualOut, 
      NDIS_HANDLE hComplete
   );

};

//------------------------------------------------------------------------------

#endif
