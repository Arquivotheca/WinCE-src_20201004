//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDTError.h"
#include "Utils.h"
#include "LocalDriver.h"

//------------------------------------------------------------------------------

CLocalDriver::CLocalDriver() : CDriver()
{
   m_dwMagic = NDT_MAGIC_DRIVER_LOCAL;
}

//------------------------------------------------------------------------------

CLocalDriver::~CLocalDriver()
{
   Close();
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::Open()
{
   // Open device with a system dependand call
   return ::Open();
}      

//------------------------------------------------------------------------------

HRESULT CLocalDriver::Close()
{
   // Close device with a system dependand call
   return ::Close();
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::StartDriverIoControl(
    DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest, 
    PVOID pvInpBuffer, DWORD cbInpBuffer, PVOID pvOutBuffer, 
    DWORD* pcbOutBuffer, PVOID* ppvOverlapped
)
{
   HRESULT hr = S_OK;

   // Start device 
   hr = ::StartIoControl(
      (DWORD)eRequest, pvInpBuffer, cbInpBuffer, pvOutBuffer, *pcbOutBuffer,
      pcbOutBuffer, ppvOverlapped
   );   
   if (FAILED(hr)) goto cleanUp;

   if (hr == NDT_STATUS_PENDING && dwTimeout != 0) {
      hr = ::WaitForIoControl(ppvOverlapped, dwTimeout);
   }

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::StopDriverIoControl(PVOID *ppvOverlapped)
{
   // Stop request (close a request)
   return ::StopIoControl(ppvOverlapped);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::WaitForDriverIoControl(
   PVOID *ppvOverlapped, DWORD dwTimeout, PVOID pvOutBuffer, DWORD* pcbOutBuffer
)
{
   // Wait for request complete
   return ::WaitForIoControl(ppvOverlapped, dwTimeout);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::LoadAdapter(LPCTSTR szAdapter)
{
   return ::LoadAdapter(szAdapter);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::UnloadAdapter(LPCTSTR szAdapter)
{
   return ::UnloadAdapter(szAdapter);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::QueryAdapters(LPTSTR mszAdapters, DWORD dwSize)
{
   return ::QueryAdapters(mszAdapters, dwSize);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::QueryProtocols(LPTSTR mszProtocols, DWORD dwSize)
{
   return ::QueryProtocols(mszProtocols, dwSize);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::QueryBindings(
   LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
)
{
   return ::QueryBindings(szAdapter, mszProtocols, dwSize);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   return ::UnbindProtocol(szAdapter, szProtocol);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::BindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   return ::BindProtocol(szAdapter, szProtocol);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::WriteVerifyFlag(LPCTSTR szAdapter, DWORD dwFlag)
{
   return ::WriteVerifyFlag(szAdapter, dwFlag);
}

//------------------------------------------------------------------------------

HRESULT CLocalDriver::DeleteVerifyFlag(LPCTSTR szAdapterName)
{
   return ::DeleteVerifyFlag(szAdapterName);
}

//------------------------------------------------------------------------------
