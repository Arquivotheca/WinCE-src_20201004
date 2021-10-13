//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Deamon.h"
#include "Log.h"

//------------------------------------------------------------------------------

#define THREAD_STOP_TIMEOUT     10000

UCHAR g_aucInpBuffer[2048];
UCHAR g_aucOutBuffer[2048];

//------------------------------------------------------------------------------

DWORD CDeamon::DeamonThread(PVOID pThis)
{
   return ((CDeamon *)pThis)->Thread();
}

//------------------------------------------------------------------------------

CDeamon::CDeamon(USHORT usPort)
{
   ZeroMemory(this, sizeof(*this));
   m_socket = INVALID_SOCKET;
   m_dwLastError = 0;
   m_hThread = NULL;
   m_usPort = usPort;
}

//------------------------------------------------------------------------------

CDeamon::~CDeamon()
{
   // Just make sure
   CloseSocket();
}

//------------------------------------------------------------------------------

DWORD CDeamon::Start(HANDLE hFinishEvent)
{
   DWORD rc = ERROR_SUCCESS;
   DWORD dwThreadId = 0;

   // Copy event to member variable (sorry, no DuplicateHandle on CE)
   m_hFinishEvent = hFinishEvent;

   // Start thread
   m_hThread = CreateThread(
      NULL, 0, DeamonThread, (PVOID)this, 0, &dwThreadId
   );
   if (m_hThread == NULL) rc = GetLastError();
   return rc;
}

//------------------------------------------------------------------------------

void CDeamon::Stop()
{
   DWORD rc = 0;
   
   // If there is no running thread we are done
   if (m_hThread == NULL) return;

   // Close socket -> this cause exit from read
   if (m_socket != INVALID_SOCKET) closesocket(m_socket);

   // Wait for thread end
   rc = WaitForSingleObject(m_hThread, THREAD_STOP_TIMEOUT);

   // If we wasn't succesfull kill it
   if (rc != WAIT_OBJECT_0) TerminateThread(m_hThread, 0);

   // We don't need it
   CloseHandle(m_hThread);
   m_hThread = NULL;
}

//------------------------------------------------------------------------------

BOOL CDeamon::OpenSocket()
{
   DWORD rc = 0;
   BOOL bOk = FALSE;
   SOCKET socket = INVALID_SOCKET;
   INT size = 0;
  
   // Create socket
   m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
   if (m_socket == INVALID_SOCKET) {
      m_dwLastError = ::WSAGetLastError();
      goto cleanUp;
   }

   // Bind the MessageCard Interface to our socket
   ZeroMemory(&m_saLocal, sizeof(m_saLocal));
   m_saLocal.sin_family = AF_INET;
   m_saLocal.sin_port = htons(m_usPort);
   m_saLocal.sin_addr.s_addr = htonl(INADDR_ANY);
   
   rc = ::bind(m_socket, (SOCKADDR *)&m_saLocal, sizeof(m_saLocal));
   if (rc == SOCKET_ERROR) {
      m_dwLastError = ::WSAGetLastError();
      goto cleanUp;
   }

   // Listen for connection
   rc = ::listen(m_socket, SOMAXCONN);
   if (rc == SOCKET_ERROR) {
      m_dwLastError = ::WSAGetLastError();
      goto cleanUp;
   }
   
   // Accept connection 
   size = sizeof(m_saRemote);
   socket = ::accept(m_socket, (SOCKADDR *)&m_saRemote, &size);
   if (socket == INVALID_SOCKET) {
      m_dwLastError = ::WSAGetLastError();
      goto cleanUp;
   }

   // Close listen socket and save connection one
   ::closesocket(m_socket);
   m_socket = socket;
   
   // We was succesfull
   return TRUE;

cleanUp:
   if (m_socket != INVALID_SOCKET) {
      ::closesocket(m_socket);
      m_socket = INVALID_SOCKET;
   }
   return FALSE;
}

//------------------------------------------------------------------------------

void CDeamon::CloseSocket()
{
   if (m_socket == INVALID_SOCKET) return;
   shutdown(m_socket, 0);
   closesocket(m_socket);
   m_socket = INVALID_SOCKET;
}

//------------------------------------------------------------------------------

BOOL CDeamon::DoDataReceive()
{
   BOOL   bOk = FALSE;
   DWORD  rc = 0;
   DWORD  cbInpBufferSize = sizeof(g_aucInpBuffer) - sizeof(NDT_CMD_HEADER);
   UCHAR* pucInpBuffer = g_aucInpBuffer + sizeof(NDT_CMD_HEADER);
   ULONG  cbInpBuffer = 0;
   PVOID  pvInpBuffer = NULL;
   DWORD  cbOutBufferSize = sizeof(g_aucOutBuffer) - sizeof(NDT_CMD_HEADER);
   UCHAR* pucOutBuffer = g_aucOutBuffer + sizeof(NDT_CMD_HEADER);
   ULONG  cbOutBuffer = 0;
   PVOID  pvOutBuffer = NULL;
   NDT_CMD_HEADER* pInpHeader = (NDT_CMD_HEADER*)g_aucInpBuffer;
   NDT_CMD_HEADER* pOutHeader = (NDT_CMD_HEADER*)g_aucOutBuffer;
   HRESULT hr = S_OK;


   do {
      
      // Get packet header
      rc = ::recv(m_socket, (char *)g_aucInpBuffer, sizeof(NDT_CMD_HEADER), 0);
      if (rc == 0) goto cleanUp;
      if (rc == SOCKET_ERROR) {
         m_dwLastError = ::WSAGetLastError();
         if (m_dwLastError == WSAECONNRESET) goto cleanUp;
         LogErr(
            _T("CDeamon::DoDataReceive: ")
            _T("Failed get expected data packet with code %d"), m_dwLastError
         );
         goto cleanUp;
      }
      
      // Dump packet header
      DumpPacketHeader(_T(">>>"), pInpHeader);

      // Check buffer size
      ASSERT(pInpHeader->ulLength <= cbInpBufferSize);

      // Get a packet body
      cbInpBuffer = 0;
      while (cbInpBuffer < pInpHeader->ulLength) {
         rc = ::recv(
            m_socket, (char*)(pucInpBuffer + cbInpBuffer), 
            pInpHeader->ulLength - cbInpBuffer, 0
         );
         if (rc == SOCKET_ERROR) {
            rc = ::WSAGetLastError();
            hr = HRESULT_FROM_WIN32(rc);
            goto cleanUp;
         }
         cbInpBuffer += rc;
      }
      
      // Dump packet body
      DumpPacketBody(_T(">>>"), cbInpBuffer, pucInpBuffer);

      pvInpBuffer = (PVOID)pucInpBuffer;
      pvOutBuffer = (PVOID)pucOutBuffer;
      cbOutBuffer = cbOutBufferSize;
      
      hr = DispatchCommand(
         pInpHeader->ulCommand, &pvInpBuffer, &cbInpBuffer, &pvOutBuffer, 
         &cbOutBuffer
      );

      cbOutBuffer = cbOutBufferSize - cbOutBuffer;

      pOutHeader->ulPacketId = pInpHeader->ulPacketId;
      pOutHeader->ulCommand = pInpHeader->ulCommand;
      pOutHeader->hr = hr;
      pOutHeader->ulLength = cbOutBuffer;

      // Dump packet header
      DumpPacketHeader(_T("<<<"), pOutHeader);

      // Dump packet body
      DumpPacketBody(_T("<<<"), cbOutBuffer, pucOutBuffer);

      // Get packet size
      cbOutBuffer += sizeof(NDT_CMD_HEADER);
      
      // Send it back
      rc = ::send(m_socket, (char *)g_aucOutBuffer, cbOutBuffer, 0);
      if (rc == SOCKET_ERROR) {
         m_dwLastError = ::WSAGetLastError();
         LogErr(
            _T("CDeamon::DoDataReceive: ")
            _T("Failed send command response with code %d"), m_dwLastError
         );
         goto cleanUp;
      }

   } while (TRUE);
   
cleanUp:
   DispatchCommand(NDT_CMD_CLOSE, NULL, 0, NULL, 0);
   return bOk;
}

//------------------------------------------------------------------------------

DWORD CDeamon::Thread()
{
   HRESULT hr = S_OK;
   BOOL bOk = FALSE;

   while (TRUE) {

      // Let know what we do
      LogDbg(_T("CDeamon::Thread: Start wait for a connection"));

      // Open socket and start listen for connection
      bOk = OpenSocket();

      // Exit loop when we are interrupted
      if (!bOk && (m_dwLastError == WSAEINTR)) goto cleanUp;

      // Something else wrong happen
      if (!bOk) {
         LogErr(
            _T("CDeamon::Thread: ")
            _T("Failed open socket with code %d"), m_dwLastError
         );
         goto cleanUp;
      }

      // Let know what we do
      LogDbg(_T("CDeamon::Thread: A connection accepted"));

      // Receive data until a connection is closed
      DoDataReceive();

      if (m_dwLastError != ERROR_SUCCESS) {
         LogErr(
            _T("CDeamon::Thread: ")
            _T("Failed get or process received data with code %d"),
            m_dwLastError
         );
         goto cleanUp;
      }

   }

cleanUp:
   CloseSocket();
   // Let know that we finished
   if (m_hFinishEvent != NULL) SetEvent(m_hFinishEvent);
   return ERROR_SUCCESS;
}

//------------------------------------------------------------------------------

void CDeamon::DumpPacketHeader(LPTSTR szPrefix, NDT_CMD_HEADER* pHeader)
{
   LogDbg(_T("%s ----------------------------------------------------------------"), szPrefix);
   LogDbg(_T("%s Packet ID     = %ld"), szPrefix, pHeader->ulPacketId);
   LogDbg(_T("%s Command       = %ld"), szPrefix, pHeader->ulCommand);
   LogDbg(_T("%s Result        = 0x%08x"), szPrefix, pHeader->hr);
   LogDbg(_T("%s Packet Length = %ld"), szPrefix, pHeader->ulLength);
}

//------------------------------------------------------------------------------

void CDeamon::DumpPacketBody(LPTSTR szPrefix, DWORD dwSize, PVOID pvBody)
{
   TCHAR szDump[3 * 16 + 1] = _T("");
   LPTSTR szWork = szDump;

   for (DWORD i = 0; i < dwSize; i++) {
      wsprintf(szWork, _T(" %02x"), ((UCHAR*)pvBody)[i]);
      szWork += 3;
      if ((i % 16) == 15) {
         LogVbs(_T("%s Data (%03x0)  = %s"), szPrefix, i/16, szDump);
         szWork = szDump;
         szDump[0] = _T('\0');
      }
   }
   if (szDump[0] != _T('\0')) {
      LogVbs(_T("%s Data (%03x0)  = %s"), szPrefix, (i - 1)/16, szDump);
   }
}

//------------------------------------------------------------------------------
