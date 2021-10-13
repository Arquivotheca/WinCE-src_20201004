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
#include "StdAfx.h"
#include "Utils.h"
#include "Driver.h"
#include "LocalDriver.h"
#include "RemoteDriver.h"

//------------------------------------------------------------------------------

CObjectList CDriver::s_listDriver;

//------------------------------------------------------------------------------

CDriver::CDriver()
{
   m_szSystem = NULL;
}

//------------------------------------------------------------------------------

CDriver::~CDriver()
{
   // Delete system name
   delete m_szSystem;
   m_szSystem = NULL;
}

//------------------------------------------------------------------------------

HRESULT OpenDriver(LPCTSTR szSystem)
{
   HRESULT hr = S_OK;
   LPTSTR sz = NULL;
   CDriver* pDriver = NULL;
   
   // Nobody else should play with a list
   CDriver::s_listDriver.EnterCriticalSection();
   
   // Walk list for request
   pDriver = (CDriver*)CDriver::s_listDriver.GetHead();
   while (pDriver != NULL) {
      if (pDriver->m_szSystem == NULL && szSystem == NULL) break;
      if (lstrcmpX(pDriver->m_szSystem, szSystem) == 0) break;
      pDriver = (CDriver*)CDriver::s_listDriver.GetNext(pDriver);
   }

   if (pDriver != NULL) {
      pDriver->AddRef();
   } else {
      if (szSystem == NULL) {
         pDriver = new CLocalDriver();
      } else {
         pDriver = new CRemoteDriver(szSystem);
      }

	  if (pDriver)
	  {
		  CDriver::s_listDriver.AddTail(pDriver);
		  hr = pDriver->Open();
	  }
	  else
		  hr = S_FALSE;
   }      
      
   // We are done (so we find it or not)
   CDriver::s_listDriver.LeaveCriticalSection();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CloseDriver(LPCTSTR szSystem)
{
   HRESULT hr = S_OK;
   LPTSTR sz = NULL;
   CDriver* pDriver = NULL;
   
   // Nobody else should play with a list
   CDriver::s_listDriver.EnterCriticalSection();
   
   // Walk list for request
   pDriver = (CDriver*)CDriver::s_listDriver.GetHead();
   while (pDriver != NULL) {
      if (pDriver->m_szSystem == NULL && szSystem == NULL) break;
      if (lstrcmpX(pDriver->m_szSystem, szSystem) == 0) break;
      pDriver = (CDriver*)CDriver::s_listDriver.GetNext(pDriver);
   }

   // We find it so remove it
   if (pDriver != NULL) {
      pDriver->AddRef();
      if (pDriver->Release() == 1) CDriver::s_listDriver.Remove(pDriver);
      pDriver->Release();
   }
      
   // We are done (so we find it or not)
   CDriver::s_listDriver.LeaveCriticalSection();
   return hr;
}

//------------------------------------------------------------------------------

CDriver* FindDriver(LPCTSTR szSystem)
{
   HRESULT hr = S_OK;
   LPTSTR sz = NULL;
   CDriver* pDriver = NULL;
   
   // Nobody else should play with a list
   CDriver::s_listDriver.EnterCriticalSection();
   
   // Walk list for request
   pDriver = (CDriver*)CDriver::s_listDriver.GetHead();
   while (pDriver != NULL) {
      if (pDriver->m_szSystem == NULL && szSystem == NULL) break;
      if (lstrcmpX(pDriver->m_szSystem, szSystem) == 0) break;
      pDriver = (CDriver*)CDriver::s_listDriver.GetNext(pDriver);
   }
   if (pDriver != NULL) pDriver->AddRef();
      
   // We are done (so we find it or not)
   CDriver::s_listDriver.LeaveCriticalSection();
   
   return pDriver;
}

//------------------------------------------------------------------------------
