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
#include "RemoteDriver.h"
#include "Marshal.h"
#include "Utils.h"

//------------------------------------------------------------------------------

CRemoteDriver::CRemoteDriver(LPCTSTR szSystemName) : CDriver()
{
   m_dwMagic = NDT_MAGIC_DRIVER_REMOTE;
   m_szSystem = lstrdup(szSystemName);
   m_usPort = NDT_CMD_PORT;
   memset(&m_saLocal, 0, sizeof(m_saLocal));
   memset(&m_saRemote, 0, sizeof(m_saRemote));
   m_pucInpBuffer = m_aucInpBuffer + sizeof(NDT_CMD_HEADER);
   m_cbInpBufferSize = sizeof(m_aucInpBuffer) - sizeof(NDT_CMD_HEADER);
   m_pucOutBuffer = m_aucOutBuffer + sizeof(NDT_CMD_HEADER);
   m_cbOutBufferSize = sizeof(m_aucOutBuffer) - sizeof(NDT_CMD_HEADER);
}

//------------------------------------------------------------------------------

CRemoteDriver::~CRemoteDriver()
{
   Close();
   delete m_szSystem;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::MarshalInpParameters(LPCTSTR szFormat, ...)
{
   HRESULT hr = S_OK;
   DWORD cbBuffer = m_cbInpBufferSize;
   PVOID pvBuffer = (PVOID)m_pucInpBuffer;
   va_list pArgs;

   va_start(pArgs, szFormat);
   hr = MarshalParametersV(&pvBuffer, &cbBuffer, szFormat, pArgs);
   va_end(pArgs);

   m_cbInpBuffer = m_cbInpBufferSize - cbBuffer;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::UnmarshalOutParameters(LPCTSTR szFormat, ...)
{
   HRESULT hr = S_OK;
   va_list pArgs;
   DWORD cbBuffer = m_cbOutBufferSize;
   PVOID pvBuffer = (PVOID)m_pucOutBuffer;

   va_start(pArgs, szFormat);
   hr = UnmarshalParametersV(&pvBuffer, &cbBuffer, szFormat, pArgs);
   va_end(pArgs);

   m_cbOutBuffer = m_cbOutBufferSize - cbBuffer;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::Execute(NDT_CMD eCommand)
{
   HRESULT hr = S_OK;
   INT rc = 0;
   NDT_CMD_HEADER* pInpHeader = (NDT_CMD_HEADER*)m_aucInpBuffer;
   NDT_CMD_HEADER* pOutHeader = (NDT_CMD_HEADER*)m_aucOutBuffer;
   DWORD cbInpBuffer = sizeof(NDT_CMD_HEADER) + m_cbInpBuffer;
   DWORD cbOutBuffer = 0;

   pInpHeader->ulPacketId = 0;
   pInpHeader->ulCommand = (ULONG)eCommand;
   pInpHeader->hr = S_OK;
   pInpHeader->ulLength = m_cbInpBuffer;

   // Send command
   rc = ::send(m_socket, (char*)m_aucInpBuffer, cbInpBuffer, 0);
   if (rc == SOCKET_ERROR) {
      rc = ::WSAGetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Wait to response header
   rc = ::recv(m_socket, (char*)m_aucOutBuffer, sizeof(NDT_CMD_HEADER), 0);
   if (rc == SOCKET_ERROR) {
      rc = ::WSAGetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   ASSERT(pOutHeader->ulLength <= m_cbOutBufferSize);

   // Get a packet body
   cbOutBuffer = 0;
   while (cbOutBuffer < pOutHeader->ulLength) {
      rc = ::recv(
         m_socket, (char*)(m_pucOutBuffer + cbOutBuffer), 
         pOutHeader->ulLength - cbOutBuffer,
         0
      );
      if (rc == SOCKET_ERROR) {
         rc = ::WSAGetLastError();
         hr = HRESULT_FROM_WIN32(rc);
         goto cleanUp;
      }
      cbOutBuffer += rc;
   }

   hr = pOutHeader->hr;
   
cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::Open()
{
   HRESULT hr = S_OK;
   INT rc = 0;
   WSADATA wsaData;
   CHAR szSystemName[128];

   rc = ::WideCharToMultiByte(
      CP_ACP, 0, m_szSystem, -1, szSystemName, sizeof(szSystemName), NULL, 
      NULL
   );
   if (rc == 0) {
      rc = GetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Initialize winsock
   rc = ::WSAStartup(MAKEWORD(1,1), &wsaData);
   if (rc != 0) {
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
         
   // Create socket
   m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
   if (m_socket == INVALID_SOCKET) {
      rc = ::WSAGetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   m_saRemote.sin_family = AF_INET;
   m_saRemote.sin_port = htons(m_usPort);
   m_saRemote.sin_addr.s_addr = inet_addr(szSystemName);

   // Connect
   rc = ::connect(m_socket, (SOCKADDR*)&m_saRemote, sizeof(m_saRemote));
   if (rc == SOCKET_ERROR) {
      rc = ::WSAGetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Open driver
   m_cbInpBuffer = 0;
   hr = Execute(NDT_CMD_OPEN);
   if (FAILED(hr)) goto cleanUp;

   return S_OK;
   
cleanUp:
   if (m_socket != INVALID_SOCKET) {
      ::closesocket(m_socket);
      m_socket = NULL;
   }
   ::WSACleanup();
   return hr;
}      

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::Close()
{
   HRESULT hr = S_OK;
   INT rc = 0;

   if (m_socket == NULL) goto cleanUp;

   // Close driver
   m_cbInpBuffer = 0;
   hr = Execute(NDT_CMD_CLOSE);
   
   rc = ::closesocket(m_socket);
   if (rc == INVALID_SOCKET) {
      rc = ::WSAGetLastError();
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

cleanUp:
   m_socket = NULL;
   ::WSACleanup();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::StartDriverIoControl(
    DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest, PVOID pvInpBuffer, 
    DWORD cbInpBuffer, PVOID pvOutBuffer, DWORD* pcbOutBuffer, 
    PVOID* ppvOverlapped
)
{
   HRESULT hr = S_OK;
   HRESULT hrExecute = S_OK;
   PVOID pvOutBufferWork = NULL;
   DWORD cbOutBuffer = *pcbOutBuffer;

   hr = MarshalInpParameters(
      NDT_CMD_INP_START_IOCONTROL, dwTimeout, eRequest, cbInpBuffer, pvInpBuffer
   );
   if (FAILED(hr)) goto cleanUp;

   hrExecute = Execute(NDT_CMD_START_IOCONTROL);
   if (FAILED(hrExecute)) goto cleanUp;

   hr = UnmarshalOutParameters(
      NDT_CMD_OUT_START_IOCONTROL, ppvOverlapped, pcbOutBuffer, pvOutBuffer
   );
   if (FAILED(hr)) goto cleanUp;

   hr = hrExecute;
   
cleanUp:  
   delete pvOutBufferWork;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::StopDriverIoControl(PVOID *ppvOverlapped)
{
   HRESULT hr = S_OK;
  
   hr = MarshalInpParameters(NDT_CMD_INP_STOP_IOCONTROL, *ppvOverlapped);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_STOP_IOCONTROL);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   *ppvOverlapped = NULL;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::WaitForDriverIoControl(
   PVOID* ppvOverlapped, DWORD dwTimeout, PVOID pvOutBuffer, DWORD* pcbOutBuffer
)
{
   HRESULT hr = S_OK;
   HRESULT hrExecute = S_OK;
  
   hr = MarshalInpParameters(
      NDT_CMD_INP_WAIT_IOCONTROL, *ppvOverlapped, dwTimeout
   );
   if (FAILED(hr)) goto cleanUp;

   hrExecute = Execute(NDT_CMD_WAIT_IOCONTROL);
   if (FAILED(hrExecute)) goto cleanUp;
   if (hrExecute != NDT_STATUS_PENDING) *ppvOverlapped = NULL;

   hr = UnmarshalOutParameters(
      NDT_CMD_OUT_WAIT_IOCONTROL, pcbOutBuffer, pvOutBuffer
   );
   if (FAILED(hr)) goto cleanUp;

   hr = hrExecute;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::LoadAdapter(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;

   hr = MarshalInpParameters(NDT_CMD_INP_LOAD_ADAPTER, szAdapter);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_LOAD_ADAPTER);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::UnloadAdapter(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;
  
   hr = MarshalInpParameters(NDT_CMD_INP_UNLOAD_ADAPTER, szAdapter);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_UNLOAD_ADAPTER);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::QueryAdapters(LPTSTR mszAdapters, DWORD dwSize)
{
   HRESULT hr = S_OK;
   LPTSTR mszWork = NULL, msz = NULL;
   DWORD dwCount = 0;
  
   hr = MarshalInpParameters(NDT_CMD_INP_QUERY_ADAPTERS);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_QUERY_ADAPTERS);
   if (FAILED(hr)) goto cleanUp;

   hr = UnmarshalOutParameters(NDT_CMD_OUT_QUERY_ADAPTERS, &mszWork);
   if (FAILED(hr)) goto cleanUp;

   msz = mszWork;
   while (*msz != _T('\0') && *(msz + 1) != _T('\0')) msz++;
   dwCount = msz - mszWork + 2;

   if (dwSize < dwCount) {
      hr = E_FAIL;
      goto cleanUp;
   }

   memcpy(mszAdapters, mszWork, dwCount);
   
cleanUp:
   delete mszWork;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::QueryProtocols(LPTSTR mszProtocols, DWORD dwSize)
{
   HRESULT hr = S_OK;
   LPTSTR mszWork = NULL, msz = NULL;
   DWORD dwCount = 0;
  
   hr = MarshalInpParameters(NDT_CMD_INP_QUERY_PROTOCOLS);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_QUERY_PROTOCOLS);
   if (FAILED(hr)) goto cleanUp;

   hr = UnmarshalOutParameters(NDT_CMD_OUT_QUERY_PROTOCOLS, &mszWork);
   if (FAILED(hr)) goto cleanUp;

   msz = mszWork;
   while (*msz != _T('\0') && *(msz + 1) != _T('\0')) msz++;
   dwCount = msz - mszWork + 2;

   if (dwSize < dwCount) {
      hr = E_FAIL;
      goto cleanUp;
   }

   memcpy(mszProtocols, mszWork, dwCount);
   
cleanUp:
   delete mszWork;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::QueryBindings(
   LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
)
{
   HRESULT hr = S_OK;
   LPTSTR mszWork = NULL, msz = NULL;
   DWORD dwCount = 0;
  
   hr = MarshalInpParameters(NDT_CMD_INP_QUERY_BINDING, szAdapter);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_QUERY_BINDING);
   if (FAILED(hr)) goto cleanUp;

   hr = UnmarshalOutParameters(NDT_CMD_OUT_QUERY_BINDING, &mszWork);
   if (FAILED(hr)) goto cleanUp;

   msz = mszWork;
   while (*msz != _T('\0') && *(msz + 1) != _T('\0')) msz++;
   dwCount = msz - mszWork + 2;

   if (dwSize < dwCount) {
      hr = E_FAIL;
      goto cleanUp;
   }

   memcpy(mszProtocols, mszWork, dwCount);
   
cleanUp:
   delete mszWork;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::BindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;
  
   hr = MarshalInpParameters(
      NDT_CMD_INP_BIND_PROTOCOL, szAdapter, szProtocol
   );
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_BIND_PROTOCOL);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;

   hr = MarshalInpParameters(
      NDT_CMD_INP_UNBIND_PROTOCOL, szAdapter, szProtocol
   );
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_UNBIND_PROTOCOL);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::WriteVerifyFlag(LPCTSTR szAdapter, DWORD dwFlag)
{
   HRESULT hr = S_OK;

   hr = MarshalInpParameters(NDT_CMD_INP_WRITE_VERIFY_FLAG, szAdapter, dwFlag);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_WRITE_VERIFY_FLAG);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CRemoteDriver::DeleteVerifyFlag(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;
  
   hr = MarshalInpParameters(NDT_CMD_INP_DELETE_VERIFY_FLAG, szAdapter);
   if (FAILED(hr)) goto cleanUp;

   hr = Execute(NDT_CMD_DELETE_VERIFY_FLAG);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------
