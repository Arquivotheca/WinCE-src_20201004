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
#ifndef __DRIVER_H
#define __DRIVER_H

//------------------------------------------------------------------------------

#include "NDT_Request.h"
#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

class CDriver : public CObject
{
   friend HRESULT OpenDriver(LPCTSTR szSystem);
   friend HRESULT CloseDriver(LPCTSTR szSystem);
   friend CDriver* FindDriver(LPCTSTR szSystem);
   
protected:
   static CObjectList s_listDriver;

protected:
   LPTSTR m_szSystem;

public:
   CDriver();
   virtual ~CDriver();

   virtual HRESULT Open() = 0;
   virtual HRESULT Close() = 0;

   virtual HRESULT StartDriverIoControl(
      DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest, PVOID pvInpBuffer, 
      DWORD cbInpBuffer, PVOID pvOutBuffer, DWORD* pcbOutBuffer, 
      PVOID* ppvOverlapped
   ) = 0;
   
   virtual HRESULT StopDriverIoControl(
      PVOID* ppvOverlapped
   ) = 0;
   
   virtual HRESULT WaitForDriverIoControl(
      PVOID *ppvOverlapped, DWORD dwTimeout, PVOID pvOutBuffer, 
      DWORD* pcbOutBuffer
   ) = 0;

   virtual HRESULT LoadAdapter(LPCTSTR szAdapter) = 0;
   virtual HRESULT UnloadAdapter(LPCTSTR szAdapter) = 0;

   virtual HRESULT QueryAdapters(LPTSTR mszAdapters, DWORD dwSize) = 0;
   virtual HRESULT QueryProtocols(LPTSTR mszProtocols, DWORD dwSize) = 0;
   virtual HRESULT QueryBindings(
      LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
   ) = 0;
   virtual HRESULT UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol) = 0;
   virtual HRESULT BindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol) = 0;

   virtual HRESULT WriteVerifyFlag(LPCTSTR szAdapter, DWORD dwFlag) = 0;
   virtual HRESULT DeleteVerifyFlag(LPCTSTR szAdapter) = 0;
};

//------------------------------------------------------------------------------

HRESULT OpenDriver(LPCTSTR szSystemName);
HRESULT CloseDriver(LPCTSTR szSystemName);
CDriver* FindDriver(LPCTSTR szSystemName);

//------------------------------------------------------------------------------

#endif
