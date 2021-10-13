//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Protocol.h"
#include "NDTError.h"
#include "Driver.h"
#include "Request.h"
#include "Utils.h"
#include "Marshal.h"

//------------------------------------------------------------------------------

LPTSTR lstrdup(LPCTSTR sz)
{
   DWORD cb = lstrlen(sz);
   LPTSTR szDup = new TCHAR[cb + 1];
   memcpy(szDup, sz, (cb + 1) * sizeof(TCHAR));
   return szDup;
}

//------------------------------------------------------------------------------

LPTSTR lstrchr(LPCTSTR sz, TCHAR c)
{
   LPCTSTR szPos = sz;
   while (*szPos != _T('\0')) {
      if (*szPos == c) break;
      szPos++;
   }
   if (*szPos == _T('\0')) szPos = NULL;
   return (LPTSTR)szPos;
}

//------------------------------------------------------------------------------

INT lstrcmpX(LPCTSTR sz1, LPCTSTR sz2)
{
   if (sz1 == NULL) {
      if (sz2 == NULL) return 0;
      return -1;
   }
   if (sz2 == NULL) return 1;

   while (*sz1 != _T('\0') && *sz2 != _T('\0'))  {
      if (*sz1 < *sz2) return -1;
      if (*sz1 > *sz2) return 1;
      sz1++;
      sz2++;
   }
   return 0;
}

//------------------------------------------------------------------------------

HRESULT OpenX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;

   hr = OpenDriver(NULL);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CloseX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;

   hr = CloseDriver(NULL);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT LoadAdapterX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_LOAD_ADAPTER, &szMiniport
   );
   if (FAILED(hr)) goto cleanUp;

   hr = LoadAdapter(szMiniport);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnloadAdapterX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_UNLOAD_ADAPTER, &szMiniport
   );
   if (FAILED(hr)) goto cleanUp;

   hr = UnloadAdapter(szMiniport);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   delete szMiniport;
   return hr;
}


//------------------------------------------------------------------------------

HRESULT QueryAdaptersX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   TCHAR mszAdapters[1024];
   DWORD dwSize = sizeof(mszAdapters);

   hr = ::QueryAdapters(mszAdapters, dwSize);
   if (FAILED(hr)) goto cleanUp;
   
   hr = MarshalParameters(
      ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_QUERY_ADAPTERS, mszAdapters
   );
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT QueryProtocolsX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   TCHAR mszProtocols[1024];
   DWORD dwSize = sizeof(mszProtocols);

   hr = ::QueryProtocols(mszProtocols, dwSize);
   if (FAILED(hr)) goto cleanUp;
   
   hr = MarshalParameters(
      ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_QUERY_PROTOCOLS, mszProtocols
   );
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT QueryBindingsX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;
   TCHAR mszProtocols[1024];
   DWORD dwSize = sizeof(mszProtocols);

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_QUERY_BINDING, &szMiniport
   );
   if (FAILED(hr)) goto cleanUp;

   hr = ::QueryBindings(szMiniport, mszProtocols, dwSize);
   if (FAILED(hr)) goto cleanUp;
   
   hr = MarshalParameters(
      ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_QUERY_BINDING, mszProtocols
   );
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT BindProtocolX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;
   LPTSTR szProtocol = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_BIND_PROTOCOL, &szMiniport,
      &szProtocol
   );
   if (FAILED(hr)) goto cleanUp;

   hr = BindProtocol(szMiniport, szProtocol);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   delete szProtocol;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnbindProtocolX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;
   LPTSTR szProtocol = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_UNBIND_PROTOCOL, &szMiniport,
      &szProtocol
   );
   if (FAILED(hr)) goto cleanUp;

   hr = UnbindProtocol(szMiniport, szProtocol);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   delete szProtocol;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT WriteVerifyFlagX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;
   DWORD dwFlag = 0;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_WRITE_VERIFY_FLAG, &szMiniport,
      &dwFlag
   );
   if (FAILED(hr)) goto cleanUp;

   hr = WriteVerifyFlag(szMiniport, dwFlag);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT DeleteVerifyFlagX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   LPTSTR szMiniport = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_DELETE_VERIFY_FLAG, &szMiniport
   );
   if (FAILED(hr)) goto cleanUp;

   hr = DeleteVerifyFlag(szMiniport);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   delete szMiniport;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT StartIoControlX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   HRESULT hrExecute = S_OK;
   CDriver* pDriver = NULL;
   CRequest* pRequest = NULL;
   NDT_ENUM_REQUEST_TYPE eRequest = NDT_REQUEST_UNKNOWN;
   DWORD dwTimeout = 0;

   
   // Find a local driver
   pDriver = FindDriver(NULL);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }
   
   // Create a request
   pRequest = new CRequest(pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Release a driver reference
   pDriver->Release();
   
   // Get parameters
   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_START_IOCONTROL, &dwTimeout,
      &eRequest, &pRequest->m_cbInpBuffer, pRequest->m_baInpBuffer
   );
   if (FAILED(hr)) goto cleanUp;
   
   // Execute a request
   hrExecute = pRequest->Execute(dwTimeout, eRequest);
   if (FAILED(hrExecute)) {
      hr = hrExecute;
      goto cleanUp;
   }

   if (hrExecute != NDT_STATUS_PENDING) {
      // Marshal output parameters
      hr = MarshalParameters(
         ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_START_IOCONTROL, NULL,
         pRequest->m_cbOutBuffer, pRequest->m_baOutBuffer
      );
      if (FAILED(hr)) goto cleanUp;
   } else {
      hr = MarshalParameters(
         ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_START_IOCONTROL, pRequest,
         0, NULL
      );
      if (FAILED(hr)) goto cleanUp;
   }
   hr = hrExecute;

cleanUp:
   if (hr != NDT_STATUS_PENDING && pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT StopIoControlX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   CRequest* pRequest = NULL;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_STOP_IOCONTROL, &pRequest
   );
   if (FAILED(hr)) goto cleanUp;

   hr = pRequest->Stop();
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT WaitForControlX(
   PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, PVOID* ppvOutBuffer, 
   DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   HRESULT hrExecute = S_OK;
   CRequest* pRequest = NULL;
   DWORD dwTimeout = 0;

   hr = UnmarshalParameters(
      ppvInpBuffer, pcbInpBuffer, NDT_CMD_INP_WAIT_IOCONTROL, &pRequest,
      &dwTimeout
   );
   if (FAILED(hr)) goto cleanUp;

   hrExecute = pRequest->WaitForComplete(dwTimeout);
   if (FAILED(hrExecute)) goto cleanUp;

   if (hrExecute != NDT_STATUS_PENDING) {
      // Marshal output parameters
      hr = MarshalParameters(
         ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_WAIT_IOCONTROL, 
         pRequest->m_cbOutBuffer, pRequest->m_baOutBuffer
      );
      if (FAILED(hr)) goto cleanUp;
   } else {
      hr = MarshalParameters(
         ppvOutBuffer, pcbOutBuffer, NDT_CMD_OUT_WAIT_IOCONTROL, 0, NULL
      );
      if (FAILED(hr)) goto cleanUp;
   }
   hr = hrExecute;

cleanUp:
   if (hr != NDT_STATUS_PENDING) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT DispatchCommand(
   ULONG ulCommand, PVOID* ppvInp, DWORD* pcbInp, PVOID* ppvOut, DWORD* pcbOut
)
{
   HRESULT hr = S_OK;

   switch (ulCommand) {
   case NDT_CMD_OPEN:
      hr = OpenX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_CLOSE:
      hr = CloseX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_LOAD_ADAPTER:
      hr = LoadAdapterX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_UNLOAD_ADAPTER:
      hr = UnloadAdapterX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_QUERY_ADAPTERS:
      hr = QueryAdaptersX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_QUERY_PROTOCOLS:
      hr = QueryProtocolsX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_QUERY_BINDING:
      hr = QueryBindingsX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_BIND_PROTOCOL:
      hr = BindProtocolX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_UNBIND_PROTOCOL:
      hr = UnbindProtocolX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_WRITE_VERIFY_FLAG:
      hr = WriteVerifyFlagX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_DELETE_VERIFY_FLAG:
      hr = DeleteVerifyFlagX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_START_IOCONTROL:
      hr = StartIoControlX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_STOP_IOCONTROL:
      hr = StopIoControlX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   case NDT_CMD_WAIT_IOCONTROL:
      hr = WaitForControlX(ppvInp, pcbInp, ppvOut, pcbOut);
      break;
   default:
      hr = E_FAIL;
   }
   
   return hr;
}

//------------------------------------------------------------------------------
