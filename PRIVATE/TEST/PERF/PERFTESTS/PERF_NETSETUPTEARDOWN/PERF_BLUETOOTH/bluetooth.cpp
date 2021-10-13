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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#include "perf_bluetooth.h"

// Macro to handle error after performing a send or recv operation

// If snrOp1 == BTSend, then nBytesTransferred has to be one of:
//		SOCKET_ERROR
//		nBufferSize
// If snrOp1 == BTRecv, then nBytesTransferred has to be one of:
//		SOCKET_ERROR
//		nBufferSize
//		0
//
// The other side closed connection, if it is client it is an error
// If it is server, assume client closed the connection at end of test */

#define PROCESS_SNROP1_ERROR(errorAction) \
if (SOCKET_ERROR == nBytesTransferred) \
{ \
	g_pKato->Log(LOG_FAIL, TEXT("%s() failed, WSA Error: %ld\n"), snrOp1Str, WSAGetLastError()); \
	errorAction; \
} \
else if (0 == nBytesTransferred) \
{ \
	if (snrOp1 == BTRecv) \
	{ \
 \
		if (!g_BTServer) \
		{ \
			g_pKato->Log(LOG_FAIL, TEXT("Connection closed by server, unexpected\n")); \
			errorAction; \
		} \
	} \
	else \
	{ \
 \
		g_pKato->Log(LOG_FAIL, TEXT("Should never be here for snrOp1==BTSend\n")); \
		errorAction; \
	} \
} \
else \
{ \
}

int BTSend(SOCKET sd, char *lpBuffer, int nBufferSize)
{
	int bytesTransferred = 0;
	int offset;

	for (offset=0; offset < nBufferSize; offset+=bytesTransferred) {
		bytesTransferred = send(sd, &lpBuffer[offset], nBufferSize-offset, 0);
        if (SOCKET_ERROR == bytesTransferred) 
			break;
    }
	return offset;
}

int BTRecv(SOCKET sd, char *lpBuffer, int nBufferSize)
{
	int bytesTransferred = 0;
	int offset;

	for (offset=0; offset < nBufferSize; offset+=bytesTransferred) {
		bytesTransferred = recv(sd, &lpBuffer[offset], nBufferSize-offset, 0);
        if ((SOCKET_ERROR == bytesTransferred) || (0 == bytesTransferred))
			break;
    }
	return offset;
}

int BTSignalGo (SOCKET s)
{
	int nBytesTransferred;
	SendRecvFunc snrOp1 = BTSend;
	TCHAR snrOp1Str [] = TEXT ("BTSend");
	char inStr [1] = {'G'};

	nBytesTransferred = snrOp1(s, inStr, sizeof(inStr));
	PROCESS_SNROP1_ERROR(return nBytesTransferred);

	return nBytesTransferred;
}

int BTSignalGoOk (SOCKET s)
{
	int nBytesTransferred;
	SendRecvFunc snrOp1 = BTSend;
	TCHAR snrOp1Str [] = TEXT ("BTSend");
	char inStr [1] = {'K'};

	nBytesTransferred = snrOp1(s, inStr, sizeof(inStr));
	PROCESS_SNROP1_ERROR(return nBytesTransferred);

	return nBytesTransferred;
}

int BTWaitForGo (SOCKET s)
{
	int nBytesTransferred;
	SendRecvFunc snrOp1 = BTRecv;
	TCHAR snrOp1Str [] = TEXT ("BTRecv");
	char inStr [1] = {0};

	nBytesTransferred = snrOp1(s, inStr, sizeof(inStr));
	PROCESS_SNROP1_ERROR(return nBytesTransferred);

	if (inStr[0] != 'G')
		return 0;

	return nBytesTransferred;
}

int BTWaitForGoOk (SOCKET s)
{
	int nBytesTransferred;
	SendRecvFunc snrOp1 = BTRecv;
	TCHAR snrOp1Str [] = TEXT ("BTRecv");
	char inStr [1] = {0};

	nBytesTransferred = snrOp1(s, inStr, sizeof(inStr));
	PROCESS_SNROP1_ERROR(return nBytesTransferred);

	if (inStr[0] != 'K')
		return 0;

	return nBytesTransferred;
}

TESTPROCAPI DoBluetoothTest ()
{
	WSADATA wsaData;
    DWORD dwRetVal = TPR_FAIL;
	int dwCount = 0;
	unsigned int success = 0;
	int namelen;
	SOCKET s = INVALID_SOCKET;
	SOCKET s2 = INVALID_SOCKET;
	size_t size;

	// Data transfer perf

	char* lpBuffer = NULL;
	DWORD dwBufferSize [] = {16, 512, 1024, 2048, BUFFER_SIZE_RANDOM, BUFFER_SIZE_FINAL};
	int i = 0;
	int j = 0;
	int k = 0;
	int nTotalBytesTransferred = 0;
	int nBytesTransferred = 0;
	int nNumBuffers = 0;
	int nBufferSize = 0;
	SendRecvFunc snrOp1;
	SignalWaitFunc msgOp2, msgOp3;
	TCHAR snrOp1Str [MAX_PATH];
	TCHAR msgOp2Str [MAX_PATH];
	TCHAR msgOp3Str [MAX_PATH];
	DWORD curBufferSize = 0;
	
	// Intialize Winsock
	if (WSAStartup (MAKEWORD (1, 1), &wsaData))
	{
		 g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** WSAStartup failed, error %d\n"), WSAGetLastError ());
		 goto Error;
	}

	// All BT data structures are defined in ws2bth.h

	SOCKADDR_BTH sa;
	SOCKADDR_BTH sa2;
	SOCKADDR_BTH sa3;

	//  Register performance markers:
    // 
    //  Registering markers associates readable strings with numbers in
    //  the performance logger. Typically, these numbers are defined
    //  in the test application and can range from 0 to MAX_MARKER_ID
    //
    
	if(!Perf_RegisterMark(MARK_BLUETOOTH_RFCOMM_CONNECT, TEXT("Bluetooth RFCOMM Connection Setup")))
    {
		LOG(TEXT("Perf_RegisterMark() failed"));
		goto Error;
	}

	if(!Perf_RegisterMark(MARK_BLUETOOTH_RFCOMM_TEARDOWN, TEXT("Bluetooth RFCOMM Connection Teardown")))
	{
	    LOG(TEXT("Perf_RegisterMark() failed"));
		goto Error;
	}

    //  Calibrate the perf logger
    for(dwCount = 0; dwCount < CALIBRATION_COUNT; dwCount++)
    {
       Perf_MarkBegin(MAX_MARKER_ID);
       Perf_MarkEnd(MAX_MARKER_ID);
    }

	s = socket (AF_BT, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (s == INVALID_SOCKET)
	{
		g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** socket failed, error %d\n"), WSAGetLastError ());
        goto Error;
	}

	// Set Bluetooth TDI MTU
	if (g_nBTMtu)
	{
		if (SOCKET_ERROR == setsockopt (s, SOL_RFCOMM, SO_BTH_SET_MTU, (char *)&g_nBTMtu, sizeof (g_nBTMtu)))
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** setsockopt(SO_BTH_SET_MTU, %d) failed, error %d\n"), WSAGetLastError (), g_nBTMtu);
			goto Error;
		}
		else
		{
				g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Bluetooth TDI MTU is now 0x%x\n"), g_nBTMtu);
		}
	}

	memset (&sa, 0, sizeof(sa));
	sa.addressFamily = AF_BT;
	sa.port = g_BTServerChannel & 0xff;

	if (!g_BTServer)
	{
		// This is the client part

		sa.btAddr = g_BTServerAddr;

		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Connecting to peer device...\n"));

		Perf_MarkBegin(MARK_BLUETOOTH_RFCOMM_CONNECT);
		if (connect (s, (SOCKADDR *)&sa, sizeof(sa)))
		{
			Perf_MarkEnd(MARK_BLUETOOTH_RFCOMM_CONNECT);
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Connect failed, error = %d\n"), WSAGetLastError ());
			goto Error;
		}
		Perf_MarkEnd(MARK_BLUETOOTH_RFCOMM_CONNECT);

		// Output some debug information

		SOCKADDR_BTH sa4;
		namelen = sizeof(sa4);

		if (0 == getsockname(s, (SOCKADDR *)&sa4, &namelen))	{
			g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Socket s:localname<%04x%08x> connecting on port %d(0x%x)...\n"), GET_NAP(sa4.btAddr), GET_SAP(sa4.btAddr), sa4.port, sa4.port);
		}

		namelen = sizeof(sa4);
		if (!getpeername(s, (SOCKADDR *)&sa4, &namelen))	{
			g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Socket s:peername<%04x%08x> connecting on port %d(0x%x)...\n"), GET_NAP(sa4.btAddr), GET_SAP(sa4.btAddr), sa4.port, sa4.port);
		}
		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Established connection with %04x%08x 0x%02x\n"), GET_NAP(g_BTServerAddr), GET_SAP(g_BTServerAddr), g_BTServerChannel & 0xff);

		// Use same socket descriptor after connection on both client and server
		s2 = s;
	}
	else
	{
		// This is the server part

		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** binding to 0x%02x\n"), g_BTServerChannel & 0xff);

        int retval = SOCKET_ERROR;
        int bind_retries = 5;

        do
        {
            // this may fail if we try to bind to a port that we just closed - timing issue.
            retval = bind (s, (SOCKADDR *)&sa, sizeof(sa));

            if (retval != INVALID_SOCKET || WSAGetLastError() != WSAEADDRINUSE)
                break;

            g_pKato->Log (LOG_COMMENT, TEXT("** RFCOMM ** bind failed, error %d, retrying...\n"), WSAGetLastError ()); 
        
            Sleep(500);
            bind_retries--;
        } while (bind_retries);

		if (retval == SOCKET_ERROR)
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Bind failed, error = %d\n"), WSAGetLastError ());
			goto Error;
		}

		// Output some debug information

		namelen = sizeof(sa);
		if (getsockname(s, (SOCKADDR *)&sa, &namelen))
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** getsockname failed, error = %d\n"), WSAGetLastError());
			goto Error;
		}

		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** localhost<%04x%08x> listening on port %d(0x%x)...\n"), GET_NAP(sa.btAddr), GET_SAP(sa.btAddr), sa.port, sa.port);
		if (listen (s, 5))
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Listen failed, error = %d\n"), WSAGetLastError ());
			goto Error;
		}

		// Accept connection from client

		size = sizeof(sa2);

		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Accepting...\n"));

		s2 = accept (s, (SOCKADDR *)&sa2, (int*)&size);

		if (s2 == INVALID_SOCKET)
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Accept failed, error = %d\n"), WSAGetLastError ());
			goto Error;
		}

		if (size != sizeof(sa2))
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Sockaddr size is %d, not %d which was expected!\n"), size, sizeof(sa2));
			goto Error;
		}

		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Connection accepted !\n"));

		// Output some debug information

		namelen = sizeof(sa3);

		if (!getsockname(s2, (SOCKADDR *)&sa3, &namelen))	{
			g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Socket s2:localname<%04x%08x> connecting on port %d(0x%x)...\n"), GET_NAP(sa3.btAddr), GET_SAP(sa3.btAddr), sa3.port, sa3.port);
		}

		namelen = sizeof(sa3);
		if (!getpeername(s2, (SOCKADDR *)&sa3, &namelen))	{
			g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Socket s2:peername<%04x%08x> connecting on port %d(0x%x)...\n"), GET_NAP(sa3.btAddr), GET_SAP(sa3.btAddr), sa3.port, sa3.port);
		}
		
		g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** Connection info: Family %d Address %04x%08x Channel 0x%02x\n"), sa2.addressFamily, GET_NAP(sa2.btAddr), GET_SAP(sa2.btAddr), sa2.port);
	}

	//	Data Xfer Perf test (both IrDA and Bluetooth are stream model connections) :
	//			Different buffer sizes: 16, 512, 1024, 2048 and RANDOM [1,2048] buffer sizes
	//			Total transfer size: configurable
	//			Single thread, send thruput, recv thruput
	//
	//				Client					Server
	//
	//							j == 0 (send throughput)
	//
	//		1.Send, send, send			Recv, recv, recv
	//		2.Wait for GO				Signal GO 
	//		3.Signal GO_OK				Wait for GO_OK 
	//
	//							j == 1 (recv throughput)
	//
	//		4.Recv, recv, recv			Send, send, send
	//		5.Signal GO					Wait for GO
	//		6.Wait for GO_OK			Signal GO_OK
	//
	//		7.Go to step 1, for next buffer size
	//
	// Use markers MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i for sending buffer of size dwBufferSize[i]
	// Use markers MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i+1 for recving buffer of size dwBufferSize[i] 

	// The socket to use depends on whether we are client or server

	for (i = 0; dwBufferSize[i] != BUFFER_SIZE_FINAL; i++)
	{
		curBufferSize = ((dwBufferSize[i] != BUFFER_SIZE_RANDOM) ? dwBufferSize[i] : MAX_RANDOM_BUFFER_SIZE);

		lpBuffer = new char[curBufferSize];
		if (lpBuffer == NULL)
		{
			g_pKato->Log (LOG_FAIL, TEXT("** RFCOMM ** Failed to allocate buffer to send/recv\n"));
			goto Error;
		}

		// Set the contents of the buffer to a constant (we don't check for contents though)

		memset (lpBuffer, 'I', curBufferSize);

		// j == 0, send throughput
		// j == 1, recv throughput

		for (j = 0; j < 2; j++)
		{
			nTotalBytesTransferred = 0;

			if (!g_BTServer)
			{
				// Client role
				if (j == 0)
				{
					// send throughput
					snrOp1 = BTSend;
					msgOp2 = BTWaitForGo;
					msgOp3 = BTSignalGoOk;
				}
				else
				{
					// recv throughput
					snrOp1 = BTRecv;
					msgOp2 = BTSignalGo;
					msgOp3 = BTWaitForGoOk;
				}

			}
			else
			{
				// Server role
				if (j == 0)
				{
					// send throughput
					snrOp1 = BTRecv;
					msgOp2 = BTSignalGo;
					msgOp3 = BTWaitForGoOk;
				}
				else
				{
					// recv throughput
					snrOp1 = BTSend;
					msgOp2 = BTWaitForGo;
					msgOp3 = BTSignalGoOk;
				}
			}

			_tcscpy (snrOp1Str, (snrOp1 == BTSend) ? TEXT("BTSend") : TEXT("BTRecv"));
			_tcscpy (msgOp2Str,
				(msgOp2 == BTWaitForGo) ? TEXT("BTWaitForGo") :
				(msgOp2 == BTWaitForGoOk) ? TEXT("BTWaitForGoOk") : 
				(msgOp2 == BTSignalGo) ? TEXT("BTSignalGo") : TEXT("BTSignalGoOk"));
			_tcscpy (msgOp3Str,
				(msgOp2 == BTWaitForGo) ? TEXT("BTWaitForGo") :
				(msgOp2 == BTWaitForGoOk) ? TEXT("BTWaitForGoOk") : 
				(msgOp2 == BTSignalGo) ? TEXT("BTSignalGo") : TEXT("BTSignalGoOk"));

			if (!g_BTServer)
			{
				BOOL fRet;

				if (dwBufferSize[i] == BUFFER_SIZE_RANDOM)
					fRet = Perf_RegisterMark(MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i+j,
						TEXT("BT Data Xfer: %s Throughput, Buffer Size = RANDOM bytes"),
						(j == 0) ? TEXT("Send") : TEXT("Receive"));
				else
					fRet = Perf_RegisterMark(MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i+j,
						TEXT("BT Data Xfer: %s Throughput, Buffer Size = %d bytes"),
						(j == 0) ? TEXT("Send") : TEXT("Receive"),
						dwBufferSize[i]);

				if (!fRet) 
				{
					LOG(TEXT("Perf_RegisterMark() failed"));
					goto Error;
				}

				if (dwBufferSize[i] == BUFFER_SIZE_RANDOM)
					g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** BT Data Xfer: %s Throughput, Buffer Size = RANDOM bytes"),
						(j == 0) ? TEXT("Send") : TEXT("Receive"));
				else
					g_pKato->Log (LOG_DETAIL, TEXT("** RFCOMM ** BT Data Xfer: %s Throughput, Buffer Size = %d bytes"),
						(j == 0) ? TEXT("Send") : TEXT("Receive"), dwBufferSize[i]);

				Perf_MarkBegin(MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i+j);
			}

			if (curBufferSize == BUFFER_SIZE_RANDOM)
			{
				// Random buffer case

				while (nTotalBytesTransferred < g_nTotalTransferSize)
				{
					// Select buffer size at random
				
					nBufferSize = MIN_RANDOM_BUFFER_SIZE +
						(rand() % (MAX_RANDOM_BUFFER_SIZE - MIN_RANDOM_BUFFER_SIZE));
					if (nTotalBytesTransferred + nBufferSize > g_nTotalTransferSize)
						nBufferSize = g_nTotalTransferSize - nTotalBytesTransferred;

					nBytesTransferred = snrOp1(s2, lpBuffer, nBufferSize);
					PROCESS_SNROP1_ERROR(goto Error);

					nTotalBytesTransferred += nBytesTransferred;
				}
			}
			else
			{
				// Constant buffer case
				
				nBufferSize = curBufferSize;
				nNumBuffers = g_nTotalTransferSize / nBufferSize; // assume no remainder

				for (k=0; k < nNumBuffers; k++)
				{
					nBytesTransferred = snrOp1(s2, lpBuffer, nBufferSize);
					PROCESS_SNROP1_ERROR(goto Error);
				}
			}

			if (!g_BTServer) Perf_MarkEnd(MARK_BLUETOOTH_RFCOMM_DATAXFER+2*i+j);

			if (!msgOp2 (s2))
			{
				g_pKato->Log(LOG_FAIL, TEXT("%s() failed, WSA Error: %ld\n"), msgOp2Str, WSAGetLastError());
				goto Error;
			}

			if (!msgOp3 (s2))
			{
				g_pKato->Log(LOG_FAIL, TEXT("%s() failed, WSA Error: %ld\n"), msgOp3Str, WSAGetLastError());
				goto Error;
			}

		} // send throughput, recv throughput

		delete [] lpBuffer;
		lpBuffer = NULL;
	} // for all buffer sizes

	if (!g_BTServer)
	{
		Perf_MarkBegin(MARK_BLUETOOTH_RFCOMM_TEARDOWN);
		closesocket (s2);
		Perf_MarkEnd(MARK_BLUETOOTH_RFCOMM_TEARDOWN);
		s = s2 = INVALID_SOCKET;
	}

	//
    //  test passed
    //
    dwRetVal = TPR_PASS;
    
Error:
	// cleanup here

	delete [] lpBuffer;
	lpBuffer = NULL;

	if (s != INVALID_SOCKET) closesocket (s);
	if (s2 != INVALID_SOCKET) closesocket (s2);

	WSACleanup ();

	return dwRetVal;
}

TESTPROCAPI BluetoothRFCOMM (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

	INT result;
	
	for (int i = 0; i < g_nIterations; i++)
	{
		g_pKato->Log(LOG_COMMENT, TEXT("Starting iteration: %d/%d\n"), i + 1, g_nIterations);
		result = DoBluetoothTest ();
		
		if (!g_BTServer)
			Sleep (2000); // minimize race conditions with server
	}

	return result;
}
