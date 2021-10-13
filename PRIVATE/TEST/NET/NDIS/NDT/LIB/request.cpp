//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Request.h"
#include "ndterror.h"
#include "Driver.h"
#include "Request.h"
#include "Marshal.h"

//------------------------------------------------------------------------------

CRequest::CRequest(CDriver* pDriver)
{
   m_dwMagic = NDT_MAGIC_REQUEST;
   memset(m_baInpBuffer, 0, sizeof(m_baInpBuffer));
   m_cbInpBuffer = sizeof(m_baInpBuffer);
   memset(m_baOutBuffer, 0, sizeof(m_baOutBuffer));
   m_cbOutBuffer = sizeof(m_baOutBuffer);
   m_pvOverlapped = NULL;
   m_pDriver = pDriver; 
   pDriver->AddRef();
}

//------------------------------------------------------------------------------

CRequest::~CRequest()
{
   //ASSERT(m_pvOverlapped == NULL);
   m_pDriver->Release();
}

//------------------------------------------------------------------------------

HRESULT CRequest::MarshalInpParameters(LPCTSTR szFormat, ...)
{
   HRESULT hr = S_OK;
   va_list pArgs;
   DWORD cbBuffer = m_cbInpBuffer;
   PVOID pvBuffer = (PVOID)m_baInpBuffer;

   va_start(pArgs, szFormat);
   hr = MarshalParametersV(&pvBuffer, &cbBuffer, szFormat, pArgs);
   va_end(pArgs);

   m_cbInpBuffer -= cbBuffer;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRequest::UnmarshalOutParameters(LPCTSTR szFormat, ...)
{
   HRESULT hr = S_OK;
   va_list pArgs;
   DWORD cbBuffer = m_cbOutBuffer;
   PVOID pvBuffer = (PVOID)m_baOutBuffer;

   va_start(pArgs, szFormat);
   hr = UnmarshalParametersV(&pvBuffer, &cbBuffer, szFormat, pArgs);
   va_end(pArgs);

   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRequest::Execute(DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest)
{
   HRESULT hr = S_OK;
   
   // Call device with request
   hr = m_pDriver->StartDriverIoControl(
      dwTimeout, eRequest, m_baInpBuffer, m_cbInpBuffer, m_baOutBuffer, 
      &m_cbOutBuffer, &m_pvOverlapped
   );

   // Fix up situation when we timout
   if (dwTimeout != 0 && hr == NDT_STATUS_PENDING) {
      Stop();
      hr = NDT_STATUS_SYNC_REQUEST_TIMEOUT;
      goto cleanUp;
   }

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRequest::WaitForComplete(DWORD dwTimeout)
{
   HRESULT hr = S_OK;
   
   if (m_pvOverlapped == NULL) goto cleanUp;

   // We should set receiving buffer size 
   m_cbOutBuffer = sizeof(m_baOutBuffer);
   // And execute   
   hr = m_pDriver->WaitForDriverIoControl(
      &m_pvOverlapped, dwTimeout, m_baOutBuffer, &m_cbOutBuffer
   );
   
cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRequest::Stop()
{
   HRESULT hr = S_OK;
   
   if (m_pvOverlapped == NULL) goto cleanUp;
   hr = m_pDriver->StopDriverIoControl(&m_pvOverlapped);
   
cleanUp:
   return hr;
}   

//------------------------------------------------------------------------------
