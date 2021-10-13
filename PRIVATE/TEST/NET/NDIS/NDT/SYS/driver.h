//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __DEVICE_H
#define __DEVICE_H

//------------------------------------------------------------------------------

#include "NDT_Request.h"
#include "Object.h"

//------------------------------------------------------------------------------

class CProtocol;
class CBinding;

//------------------------------------------------------------------------------

class CDriver : public CObject
{
friend class CRequestBind;

private:
   CProtocol* m_pProtocol;
   CProtocol* m_pProtocol40;
   
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
