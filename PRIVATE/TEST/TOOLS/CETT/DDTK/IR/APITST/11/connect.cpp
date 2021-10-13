//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock.h>
#include <oscfg.h>
#include <katoex.h>
#include <tux.h>
#include "shellproc.h"
#include "connect.h"
#undef AF_IRDA
#include <af_irda.h>

#include <irsrv.h>
#include <msg.h>
#include <irsup.h>
#include <request.h>

#define ACCEPTED_CONNECTION	(WM_USER+1)
#define ACCEPT_FAILED		(WM_USER+2)
#define RECEIVED_DATA		(WM_USER+3)
#define LAST_MESSAGE_TYPE	(WM_USER+3)

typedef struct
{
	SOCKET	sock;
	DWORD	threadID;
} ThreadInfo;

//
// A support routine to close a socket and log problems
// if they occur.
//
static int
CloseSocket(SOCKET sock)
{
	if (sock != INVALID_SOCKET && closesocket(sock) == SOCKET_ERROR)
	{
		OutStr(TEXT("NOTE: closesocket FAILED, error=%d\n"), WSAGetLastError());
		return SOCKET_ERROR;
	}
	return 0;
}

static SOCKET
OpenIRSocket()
{
	SOCKET sock;

	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
		OutStr(TEXT("SKIPPED: Unable to create IR socket, error=%d\n"), WSAGetLastError());

	return sock;
}

//
//	This thread receives bytes from a stream socket and posts messages
//  indicating how many bytes are received.
//

static DWORD WINAPI
RecvThread(
	void *pParm)
{
	ThreadInfo *pTI = (ThreadInfo *)pParm;
	SOCKET		recvSock = pTI->sock;
	DWORD		postMsgThreadID = pTI->threadID;
	char		buf[256];
	int			bytesReceived, result;

	while (1)
	{
OutStr(TEXT("recv Socket: %d\n"), pTI->sock );
		bytesReceived = recv(recvSock, &buf[0], sizeof(buf), 0);
OutStr(TEXT("recv data from server\n"));
		result = PostThreadMessage(postMsgThreadID, RECEIVED_DATA, 0, (long)bytesReceived);
		if (result == FALSE)
		{
			OutStr(TEXT("RecvThread: PostThreadMessage failed\n"));
			break;
		}
		if (bytesReceived == SOCKET_ERROR || bytesReceived == 0)
			break;
	}
	CloseSocket(recvSock);

	return 0;
}

//
//	This thread accepts connections on a socket, posts messages each time,
//  and spawns a receive thread for each.
//

static DWORD WINAPI
AcceptThread(
	void *pParm)
{
	ThreadInfo *pTI = (ThreadInfo *)pParm;
	SOCKET		sockAccept = pTI->sock;
	DWORD		postMsgThreadID = pTI->threadID;
	SOCKET		sockConn;
	UINT		message;
	LONG		lParam;
	DWORD		tidRecv;
	ThreadInfo	info;
	int			result;
	HANDLE		hThread;

	while (1)
	{
OutStr( TEXT("accept socket: %d\n"), sockAccept );
		sockConn = accept(sockAccept, NULL, NULL);
OutStr( TEXT("connect socket: %d\n"), sockConn );
		if (sockConn == INVALID_SOCKET)
		{
			message = ACCEPT_FAILED;
			lParam = WSAGetLastError();
		}
		else
		{
			message = ACCEPTED_CONNECTION;
			lParam = 0;
		}

		result = PostThreadMessage(postMsgThreadID, message, 0, lParam);
		if (result == FALSE)
		{
			OutStr(TEXT("AcceptThread: PostThreadMessage failed\n"));
			break;
		}

		if (sockConn == INVALID_SOCKET)
		{
			break;
		}

		info.sock = sockConn;
		info.threadID = postMsgThreadID;
		hThread = CreateThread(NULL, 0, RecvThread, &info, 0, &tidRecv);
		CloseHandle(hThread);
	}
	ExitThread(0);
	return 0;
}

void
CreateMessageQueue(void)
{
	MSG msg;

	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
}

void
ClearMessageQueue(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, WM_USER, LAST_MESSAGE_TYPE, PM_REMOVE))
		;
}

typedef HANDLE	TimerId;
#define INVALID_TIMER	INVALID_HANDLE_VALUE
typedef struct tagTimerThreadParam{
	UINT	timeoutMs;
	DWORD	dwMsgThreadId;
}TimerThreadParam;

static DWORD WINAPI
TimerThread(void *pParm)
{
	TimerThreadParam *pParam = (TimerThreadParam *)pParm;

	Sleep(pParam->timeoutMs);
	PostThreadMessage(pParam->dwMsgThreadId, WM_TIMER, 0, 0);

	return 0;
}

TimerId 
StartTimer(UINT timeoutMs)
{
	HANDLE  hThread;
	DWORD   dwThreadId;
	TimerThreadParam Param;

	Param.timeoutMs = timeoutMs;
	Param.dwMsgThreadId = GetCurrentThreadId();
	return hThread = CreateThread(NULL, 0, TimerThread, (LPVOID) &Param,  0,  &dwThreadId);

	//return SetTimer(NULL, 0, timeoutMs, NULL);
}

void
StopTimer(TimerId timerId)
{
	if (timerId != INVALID_TIMER)
		TerminateThread(timerId, -1);
		//KillTimer(NULL, timerId);
}

//
//	Bind a name to a socket, then have the remote peer connect to it.
//
int
RemoteConnectTest(
	char	*nameToBind,
	char	*nameToRemoteConnect)
{
	Connection	connServer;
	int			result = TPR_SKIP;
	MSG			msg;
	TimerId		timerId;
	SOCKET		sockAccept;
	int			bindResult, error, connectResult, sendResult;
	ThreadInfo	info;
	DWORD		tidAccept;
	HANDLE		hThread;
	BOOL		shouldConnect;

	// The connection attempt should work if and only if the
	// two names are the same.
	shouldConnect = strcmp(nameToBind, nameToRemoteConnect) == 0;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return result;

	CreateMessageQueue();

	// Clear the message queue
	ClearMessageQueue();

	// Create and bind a name to an IR socket
	sockAccept = OpenIRSocket();
	if (sockAccept != INVALID_SOCKET)
	{
		OutStr(TEXT("Bind %hs to socket %d\n"), nameToBind, sockAccept);
		IRBind(sockAccept, nameToBind, &bindResult, &error);
OutStr(TEXT("Bind OK\n"));
		if (bindResult != SOCKET_ERROR)
		{
			listen(sockAccept, 1);

			// Spawn a thread to accept connections on the socket
			info.sock = sockAccept;
			info.threadID = GetCurrentThreadId();
			hThread = CreateThread(NULL, 0, AcceptThread, &info, 0, &tidAccept);

			// Request that the remote side connect
			sendResult = SendConnectRequest(&connServer, nameToRemoteConnect, &connectResult, &error);
OutStr(TEXT("Send Request OK\n"));
			if (sendResult == FALSE)
			{
				OutStr(TEXT("SKIPPED: Unable to send connect request\n"));
			}
			else if (!shouldConnect)
			{
				if (connectResult == 0)
				{
					OutStr(TEXT("FAILED: Remote connected %hs to %hs, should not connect\n"), nameToRemoteConnect, nameToBind);
					result = TPR_FAIL;
					
					// Request that the remote side disconnect
					sendResult = SendUnSetRequest(&connServer, &connectResult, &error);
					if (sendResult == FALSE)
					{
						OutStr(TEXT("WARNING: Unable to send disconnect request\n"));
					}
					else if (connectResult != 0)
					{
						OutStr(TEXT("WARNING: Remote disconnect failed, error=%d\n"), error);
					}
				}
				else
				{
					// The attempt to connect failed, as expected.
					result = TPR_PASS;
				}
			}
			else if (connectResult != 0)
			{
				OutStr(TEXT("FAILED: Remote connect failed, error=%d\n"), error);
				result = TPR_FAIL;
			}
			else
			{
				// The connection was expected to be successful, and it was from the
				// remote point of view.  Now wait for the local AcceptThread to report
				// what it sees.

#define STATE_WAIT_FOR_CONNECT		1
#define STATE_WAIT_FOR_DISCONNECT	2
#define STATE_GOT_DISCONNECT		3

				int state;
				// Start a timer
				timerId = StartTimer(5000);

				state = STATE_WAIT_FOR_CONNECT;
				result = TPR_PASS;

OutStr(TEXT("Waiting for message\n"));

				// Process events
				while (result == TPR_PASS &&
					   state != STATE_GOT_DISCONNECT &&
					   GetMessage(&msg, NULL, 0, 0))
				{
					switch (msg.message)
					{
					case WM_TIMER:
						OutStr(TEXT("FAILED: Remote failed to %s before timeout\n"),
							state == STATE_WAIT_FOR_CONNECT ? TEXT("connect") : TEXT("disconnect"));
						result = TPR_FAIL;
						break;

					case ACCEPTED_CONNECTION:
						if (state == STATE_WAIT_FOR_CONNECT)
						{
							// Request that the remote side disconnect
							sendResult = SendUnSetRequest(&connServer, &connectResult, &error);
							if (sendResult == FALSE)
							{
								OutStr(TEXT("SKIPPED: Unable to send disconnect request\n"));
								result = TPR_SKIP;
							}
							else if (connectResult != 0)
							{
								OutStr(TEXT("FAILED: Remote disconnect failed, error=%d\n"), error);
								result = TPR_FAIL;
							}
							else
								state = STATE_WAIT_FOR_DISCONNECT;
						}
						else
						{
							OutStr(TEXT("FAILED: Remote connected twice!\n"));
							result = TPR_FAIL;
						}
						break;

					case ACCEPT_FAILED:
						if (state == STATE_WAIT_FOR_CONNECT)
						{
							OutStr(TEXT("FAILED: Remote failed to connect, accept error=%d\n"), msg.lParam);
							result = TPR_FAIL;
						}
						else
						{
							OutStr(TEXT("FAILED: Remote bad 2nd connect, accept error=%d\n"), msg.lParam);
							result = TPR_FAIL;
						}
						break;

					case RECEIVED_DATA:
						if (state == STATE_WAIT_FOR_CONNECT)
						{
							OutStr(TEXT("FAILED: Received %d bytes before connected\n"), msg.lParam);
							result = TPR_FAIL;
						}
						else if (msg.lParam != 0)
						{
							OutStr(TEXT("FAILED: Received %d bytes, should have been 0\n"), msg.lParam);
							result = TPR_FAIL;
						}
						else
						{
							state = STATE_GOT_DISCONNECT;
						}
						break;

					default:
						ASSERT(FALSE);
						break;
					}
				} // while GetMsg

				StopTimer(timerId);
			}
		}

		OutStr(TEXT("Close socket %d\n"), sockAccept);
		CloseSocket(sockAccept);

		// Wait for the accept thread to exit
		DWORD exitCode;

		GetExitCodeThread(hThread, &exitCode);

		// Delete the accept thread
		CloseHandle(hThread);

		// Clear the message queue
		ClearMessageQueue();
	}

	ConnectionClose(&connServer);

	return result;
}

//
//	Bind a name to a socket, then have the remote connect to that socket.
//
int
RemoteConnectOKTest(
	char	*nameToTest)
{
	return RemoteConnectTest(nameToTest, nameToTest);
}


//
//	Have the remote bind a name to a socket.  Then try to connect.
//  If the names are the same then a connection should be created,
//  otherwise the connection should fail.
//
int
LocalConnectTest(
	char	*nameToRemoteBind,
	char	*nameToConnect)
{
	Connection	connServer;
	int			result = TPR_SKIP;
	SOCKET		sockConn;
	int			bindResult, error, connectResult, sendResult;
	BOOL		shouldConnect;

	// The connection attempt should work if and only if the
	// two names are the same.
	shouldConnect = strcmp(nameToRemoteBind, nameToConnect) == 0;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return result;

	// Request that the remote side connect
	sendResult = SendBindRequest(&connServer, nameToRemoteBind, &bindResult, &error);
	if (sendResult == FALSE)
	{
		OutStr(TEXT("SKIPPED: Unable to send bind request\n"));
	}
	else if (bindResult != 0)
	{
		OutStr(TEXT("SKIPPED: Remote bind got error %d\n"), error);
	}
	else
	{
		sockConn = socket(AF_IRDA, SOCK_STREAM, 0);
		if (sockConn != INVALID_SOCKET)
		{
			IRConnect(sockConn, nameToConnect, &connectResult, &error);
			if (connectResult == 0)
			{
				if (shouldConnect)
				{
					// Connected as it should
					result = TPR_PASS;
				}
				else
				{
					// Connected but it should not have
					OutStr(TEXT("FAILED: Connected %hs to %hs but should not have\n"), nameToConnect, nameToRemoteBind);
					result = TPR_FAIL;
				}
			}
			else
			{
				if (shouldConnect)
				{
					// Did not connect but it should have
					OutStr(TEXT("FAILED: Unable to connect %hs to %hs\n"), nameToConnect, nameToRemoteBind);
					result = TPR_FAIL;
				}
				else
				{
					// Did not connect which is correct
					result = TPR_PASS;
				}
			}

			closesocket(sockConn);

			if (shouldConnect && result == TPR_PASS)
			{
				// Check to see that the remote side got the shutdown
			}
		}
	}

	ConnectionClose(&connServer);

	return result;
}

//
//	Remote bind a name to a socket, then connect to that socket.
//
int
LocalConnectOKTest(
	char	*nameToTest)
{
	return LocalConnectTest(nameToTest, nameToTest);
}


//------------------------- Tux Testing Functions ----------------------------- 

//
//	This function does a simple test to verify that a bound socket
//  can be connected to by a remote device.
//

#define CONNTEST	"ConnTest"

TEST_FUNCTION(BasicRemoteConnectTest)
{
	TEST_ENTRY;

	return RemoteConnectOKTest(CONNTEST);
}

TEST_FUNCTION(BasicLocalConnectTest)
{
	TEST_ENTRY;

	return LocalConnectOKTest(CONNTEST);
}

static void
GenerateName(
	char *nameBuffer,
	int   length)
{
	for (int i = 0; i < length; i++)
		*nameBuffer++ = '0' + i % 10;
	*nameBuffer = '\0';
}

//	A service name becomes a class name, hence it has a minimum length of 1
#define MIN_IRDA_SERVICE_NAME_LENGTH 1

// The service name must be '\0' terminated, hence it has a maximum length
// of 1 less than the size of the array
#define MAX_IRDA_SERVICE_NAME_LENGTH (sizeof(((SOCKADDR_IRDA *)NULL)->irdaServiceName) -1)

// Test lengths up to 1 greater than the max legal length
// That is, for the last iteration the '\0' termination rule
// is violated.  The OS should auto-terminate the string for
// us by always setting the last char in the bind to '\0',
// so a name of MAX_LENGTH or MAX_LENGTH+1 should wind up
// being identical.
int ivNameLengthsToTest[] =
{
	MIN_IRDA_SERVICE_NAME_LENGTH,
	MIN_IRDA_SERVICE_NAME_LENGTH + 1,
	10,
	10,
	20,
	MAX_IRDA_SERVICE_NAME_LENGTH - 1,
	MAX_IRDA_SERVICE_NAME_LENGTH,
	MAX_IRDA_SERVICE_NAME_LENGTH + 1
};

//
//	This function does tests that socket names of each legal length
//  can be connected to.
//

int
ConnectNameLengthTest(
	int (*pTestFunc)(char *name))
{
	int length, result, overall_result;
	char name[MAX_IRDA_SERVICE_NAME_LENGTH+2];

	overall_result = TPR_PASS;
	for (int i = 0; i < countof(ivNameLengthsToTest); i++)
	{
		length = ivNameLengthsToTest[i];
		GenerateName(&name[0], length);

		result = (*pTestFunc)(&name[0]);
		if (result != TPR_PASS)
		{
			if (result == TPR_SKIP)
			{
				if (overall_result != TPR_FAIL)
					overall_result = TPR_SKIP;
				break;
			}

			if (result == TPR_FAIL)
			{
				OutStr(TEXT("FAILED: Remote connect to service name length %d, name %hs failed.\n"),
					length, &name[0]);

				overall_result = TPR_FAIL;
			}
		}
	}
	return overall_result;
}

// Bind socket names of various lengths, and have the remote server try connecting

TEST_FUNCTION(RemoteConnectNameLengthTest)
{
	TEST_ENTRY;
	
	return ConnectNameLengthTest(RemoteConnectOKTest);
}

// Have the remote server bind socket names of various lengths, and then try connecting

TEST_FUNCTION(LocalConnectNameLengthTest)
{
	TEST_ENTRY;
	
	return ConnectNameLengthTest(LocalConnectOKTest);
}

//
//	Test all legal character values in a name, that is, character values of 1 to 255.
//  0 is not legal since it is the name termination char.
//

#define MIN_CHARACTER_VALUE 1
#define MAX_CHARACTER_VALUE 255

int
ConnectNameValueTest(
	int (*pTestFunc)(char *name))
{
	int	 result, i;
	char name[MAX_IRDA_SERVICE_NAME_LENGTH+1];
	int  c, startchar;

	for (c = MIN_CHARACTER_VALUE;
		 c <= MAX_CHARACTER_VALUE;
		 )
	{
		startchar = c;

		for (i = 0; i < (MAX_IRDA_SERVICE_NAME_LENGTH-1) && c <= MAX_CHARACTER_VALUE; i++)
			name[i] = (char)c++;
		name[i] = '\0';

		result = (*pTestFunc)(&name[0]);
		if (result != TPR_PASS)
		{
			if (result == TPR_FAIL)
				OutStr(TEXT("FAILED: Remote connect to service name with start char=%d '%hs' failed.\n"), startchar, &name[0]);
			break;
		}
	}
	return result;
}

TEST_FUNCTION(RemoteConnectNameValueTest)
{
	TEST_ENTRY;

	return ConnectNameValueTest(RemoteConnectOKTest);
}

TEST_FUNCTION(LocalConnectNameValueTest)
{
	TEST_ENTRY;

	return ConnectNameValueTest(LocalConnectOKTest);
}

int subAndSuperLengthsToTest[] =
{
	MIN_IRDA_SERVICE_NAME_LENGTH,
	MIN_IRDA_SERVICE_NAME_LENGTH + 1,
	MAX_IRDA_SERVICE_NAME_LENGTH / 2,
	MAX_IRDA_SERVICE_NAME_LENGTH - 1,
	MAX_IRDA_SERVICE_NAME_LENGTH,
};

//	Try connecting names which are substrings and superstrings of the bound name.
//  If any connections occur, this test fails.
//
int
ConnectSubAndSuperStringTest(
	int (*pTestFunc)(char *, char *))
{
	int bindLength, connectLength, result, numTest;
	char nameBind[MAX_IRDA_SERVICE_NAME_LENGTH+1];
	char nameConnect[MAX_IRDA_SERVICE_NAME_LENGTH+1];

	result = TPR_PASS;
	for (numTest = 0; numTest < countof(subAndSuperLengthsToTest); numTest++)
	{
		bindLength = subAndSuperLengthsToTest[numTest];
		GenerateName(&nameBind[0], bindLength);

		// Try to connect using names of length 1 shorter and 1 longer than the bind name
		for (connectLength = bindLength - 1; connectLength <= bindLength + 1; connectLength += 2)
		{
			// Only try names that are of a valid length
			if (MIN_IRDA_SERVICE_NAME_LENGTH <= connectLength &&
												connectLength <= MAX_IRDA_SERVICE_NAME_LENGTH)
			{
				GenerateName(&nameConnect[0], connectLength);
				result = (*pTestFunc)(&nameBind[0], &nameConnect[0]);
				if (result != TPR_PASS)
					break;
			}
		}
	}

	return result;
}

TEST_FUNCTION(RemoteConnectSubAndSuperStringTest)
{
	TEST_ENTRY;

	return ConnectSubAndSuperStringTest(RemoteConnectTest);
}

TEST_FUNCTION(LocalConnectSubAndSuperStringTest)
{
	TEST_ENTRY;

	return ConnectSubAndSuperStringTest(LocalConnectTest);
}

//
//	Bind a name to a socket, then destroy the socket.
//  Verify that the remote side cannot connect to the socket.
//
TEST_FUNCTION(RemoteConnectToUnboundNameTest)
{
	int	 result = TPR_SKIP;
	Connection	connServer;
	SOCKET		sockAccept;
	int			bindResult, error, connectResult, sendResult;

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return TPR_SKIP;

	// Create and bind a name to an IR socket
	sockAccept = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sockAccept != INVALID_SOCKET)
	{
		IRBind(sockAccept, CONNTEST, &bindResult, &error);
		if (bindResult != SOCKET_ERROR)
		{
			listen(sockAccept, 1);
			closesocket(sockAccept);

			// Request that the remote side connect
			sendResult = SendConnectRequest(&connServer, CONNTEST, &connectResult, &error);
			if (sendResult == FALSE)
			{
				OutStr(TEXT("SKIPPED: Unable to send connect request\n"));
			}
			else if (connectResult == 0)
			{
				OutStr(TEXT("FAILED: Remote connect to unbound name should have failed.\n"));
				result = TPR_FAIL;

				// Request that the remote side disconnect
				sendResult = SendUnSetRequest(&connServer, &connectResult, &error);
				if (sendResult == FALSE)
					OutStr(TEXT("Unable to send disconnect request\n"));
			}
			else
			{
				// Connect was supposed to fail, and it did
				result = TPR_PASS;
			}
		}
		else
		{
			OutStr(TEXT("SKIPPED: Unable to bind name to socket\n"));
			closesocket(sockAccept);
		}
	}
	ConnectionClose(&connServer);
	return result;
}

//	Try using hard coded lsap-sels for binds and connects.
//
int
ConnectHardCodedLsapSelTest(
	int (*pTestFunc)(char *, char *))
{
	int  result, numTest, lsapSel;
	char nameBind[MAX_IRDA_SERVICE_NAME_LENGTH+1];
	char nameConnect[MAX_IRDA_SERVICE_NAME_LENGTH+1];

	result = TPR_PASS;
	for (numTest = 0; numTest < 1; numTest++)
	{
		lsapSel = 42;

		sprintf(&nameBind[0], "LSAP-SEL%d", lsapSel);
		sprintf(&nameConnect[0], "LSAP-SEL%d", lsapSel);
		result = (*pTestFunc)(&nameBind[0], &nameConnect[0]);
		if (result != TPR_PASS)
			break;
	}

	return result;
}

TEST_FUNCTION(RemoteConnectHardCodedLsapSelTest)
{
	TEST_ENTRY;

	return ConnectHardCodedLsapSelTest(RemoteConnectTest);
}

TEST_FUNCTION(LocalConnectHardCodedLsapSelTest)
{
	TEST_ENTRY;

	return ConnectHardCodedLsapSelTest(LocalConnectTest);
}

// Connection memory leak detection functions

extern int
MemoryLeakTest(
	int (*pTestFunction)(void),
	int numInitIterations,
	int numTestIterations,
	int *pAmountLeakedPerIteration);

int
RemoteConnectAll(void)
{
	int result;

	result = RemoteConnectOKTest(CONNTEST);
	if (result != TPR_PASS)
		return result;

	return result;
}

int
LocalConnectAll(void)
{
	int result;

	result = LocalConnectOKTest(CONNTEST);
	if (result != TPR_PASS)
		return result;

	return result;
}

// RemoteConnectMemoryLeakTest - Check that calling various remote connects results in
// no loss of memory.
//
TEST_FUNCTION(RemoteConnectMemoryLeakTest)
{
	int    amountLeakedPerIteration, result;

	TEST_ENTRY;

	// Create a socket to hold the COM port open for SIR so that time is not wasted opening and closing it.
	SOCKET s = socket(AF_IRDA, SOCK_STREAM, 0);
	result = MemoryLeakTest(RemoteConnectAll, 5, 300, &amountLeakedPerIteration);
	if (result == TPR_FAIL)
	{
		OutStr(TEXT("RemoteConnectAll leaking %d bytes per call\n"), amountLeakedPerIteration);
	}
	CloseSocket(s);
	return result;
}

// LocalConnectMemoryLeakTest - Check that calling various local connects results in
// no loss of memory.
//
TEST_FUNCTION(LocalConnectMemoryLeakTest)
{
	int    amountLeakedPerIteration, result;

	TEST_ENTRY;

	// Create a socket to hold the COM port open for SIR so that time is not wasted opening and closing it.
	SOCKET s = socket(AF_IRDA, SOCK_STREAM, 0);
	result = MemoryLeakTest(LocalConnectAll, 5, 300, &amountLeakedPerIteration);
	if (result == TPR_FAIL)
	{
		OutStr(TEXT("LocalConnectAll leaking %d bytes per call\n"), amountLeakedPerIteration);
	}
	CloseSocket(s);
	return result;
}

// Test that doing connect--closesocket before an "accept" does not cause an exception
//
TEST_FUNCTION(RemoteConnectAndCloseBeforeAcceptTest)
{
	int	 result = TPR_PASS;
	Connection	connServer;
	SOCKET		sockAccept;
	int			bindResult, error, connectResult, sendResult;

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return TPR_SKIP;

	// Create and bind a name to an IR socket
	sockAccept = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sockAccept == INVALID_SOCKET)
		result = TPR_SKIP;
	else
	{
		IRBind(sockAccept, CONNTEST, &bindResult, &error);
		if (bindResult != SOCKET_ERROR)
		{
			listen(sockAccept, 1);

			// Request that the remote side connect
			sendResult = SendConnectRequest(&connServer, CONNTEST, &connectResult, &error);
			if (sendResult == FALSE)
			{
				OutStr(TEXT("SKIPPED: Unable to send connect request\n"));
				result = TPR_SKIP;
			}
			else if (connectResult != 0)
			{
				OutStr(TEXT("SKIPPED: Remote connect failed, error=%d\n"), error);
				result = TPR_SKIP;
			}
			else
			{
				// Request that the remote side disconnect
				sendResult = SendUnSetRequest(&connServer, &connectResult, &error);
				if (sendResult == FALSE)
				{
					OutStr(TEXT("SKIPPED: Unable to send disconnect request\n"));
					result = TPR_SKIP;
				}

			}
		}
		closesocket(sockAccept);
	}
	ConnectionClose(&connServer);
	return result;
}

//
//	Try to connect to a bad device id.
//  The connect should fail
//
TEST_FUNCTION(LocalConnectToUnknownDevice)
{
	Connection	connServer;
	int			result = TPR_SKIP;
	SOCKET		sockConn;
	int			bindResult, error, connectResult, sendResult;

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return result;

	// Request that the remote side bind
	sendResult = SendBindRequest(&connServer, CONNTEST, &bindResult, &error);
	if (sendResult == FALSE)
	{
		OutStr(TEXT("SKIPPED: Unable to send bind request\n"));
	}
	else if (bindResult != 0)
	{
		OutStr(TEXT("SKIPPED: Remote bind got error %d\n"), error);
	}
	else
	{
		sockConn = socket(AF_IRDA, SOCK_STREAM, 0);
		if (sockConn != INVALID_SOCKET)
		{
			SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, CONNTEST};

			if (IRFindNewestDevice(&addr.irdaDeviceID[0], 1) == 0)
			{
				// Corrupt the device id so that the connect should fail
				addr.irdaDeviceID[0] += 1;

				connectResult = connect(sockConn, (struct sockaddr *)&addr, sizeof(addr));

				if (connectResult == 0)
				{
					// Connected but it should not have
					OutStr(TEXT("FAILED: Connected to invalid deviceID but should not have\n"));
					result = TPR_FAIL;
				}
				else
				{
					// Did not connect which is correct
					result = TPR_PASS;
				}
			}

			closesocket(sockConn);
		}
	}

	ConnectionClose(&connServer);

	return result;
}



#ifdef UNDER_CE
#define	COM_NAME_FORMAT	"COM%d:"
#else
#define	COM_NAME_FORMAT	"COM%d"
#endif

//
// Open the specified COM port.
// Return the HANDLE.
//
extern HANDLE
COMPortOpen(
	int portNum,
	int *pError)
{
    HANDLE			hCommPort;
    TCHAR			SerialPortName[64];

    _stprintf(SerialPortName, TEXT(COM_NAME_FORMAT), portNum);
    hCommPort = CreateFile(SerialPortName,
               GENERIC_READ | GENERIC_WRITE,
               0, NULL, OPEN_EXISTING,
               0, NULL);

    if (hCommPort == INVALID_HANDLE_VALUE)
		*pError = WSAGetLastError();

	return hCommPort;
}

//
//	Determine the COM port to be used by the serial IR driver
//  from the registry.  Return the com port number if found,
//  otherwise return -1;
//
extern int
GetSIRCOMPort()
{
	DWORD	portNum;
	HKEY	hKey;
	DWORD	type;
	TCHAR	lpIRIntBinded[64];			// IR interface binded
	TCHAR	lpSubKey[64];				// subkey name to be queried when looking for SIR port
	DWORD	valueSize = sizeof(lpSubKey);

	// Example SIR Registry Entry:
	//
	// [HKEY_LOCAL_MACHINE\Comm\Irsir1\Parms]
	// "Port"=dword:1

	// Determine the IR port number from the registry
	// Determine which IR device is binded
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					TEXT("Comm\\IrDA\\Linkage"), 
					0,
					KEY_READ,
					&hKey)
		!= ERROR_SUCCESS)
	{
		return -1;
	}

	if (RegQueryValueEx(
				hKey,					// handle of key to query
				TEXT("Bind"),			// address of name of value to query
				NULL,					// reserved
				&type,					// address of buffer for value type
				(LPBYTE)lpIRIntBinded,	// address of data buffer
				&valueSize)				// address of data buffer size
		!= ERROR_SUCCESS)
	{
		return -1;
	}
	RegCloseKey(hKey);

	// Construct subkey name to be queried
	_tcscpy(lpSubKey, TEXT("Comm\\"));
	_tcscat(lpSubKey, lpIRIntBinded);
	_tcscat(lpSubKey, TEXT("\\Parms"));
	valueSize = sizeof(portNum);
	
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                   lpSubKey,
					//TEXT("Comm\\Irsir1\\Parms"),
                   0,
                   KEY_READ,
                   &hKey)
		!= ERROR_SUCCESS)
	{
		return -1;
	}

	if (RegQueryValueEx(
				hKey,				// handle of key to query
				TEXT("Port"),		// address of name of value to query
				NULL,				// reserved
				&type,				// address of buffer for value type
				(LPBYTE)&portNum,	// address of data buffer
				&valueSize)			// address of data buffer size
		!= ERROR_SUCCESS)
	{
		return -1;
	}

	RegCloseKey(hKey);

	return portNum;
}

//
//
//  This test performs the following:
//
//		1. Open the COM port to be used by IrSIR
//		2. Try to create an IR socket (this should fail due to COM port in use)
//		3. Close the COM port
//		4. Verify that an IR socket can be created and connected to a remote device
//		5. Verify that the COM port can be opened again (with no IR sockets in use).
//
//

TEST_FUNCTION(AfterIrSirCOMOpenFailedTest)
{
    HANDLE		hCommPort;
	int			result = TPR_SKIP, error, portNum;
	SOCKET		sock;

	TEST_ENTRY;

	portNum = GetSIRCOMPort();

	if (portNum == -1)
	{
		OutStr(TEXT("SKIPPED: Unable to determine COM port used by Serial IR driver\n"));
	}
	else
	{
		// Allow pending close to complete.
		Sleep(10000);

		hCommPort = COMPortOpen(portNum, &error);
		if (hCommPort == INVALID_HANDLE_VALUE)
		{
			OutStr(TEXT("SKIPPED: Unable to open COM port %d\n"), portNum);
		}
		else
		{
			// With the COM port in use, try to create an IR socket
			sock = socket(AF_IRDA, SOCK_STREAM, 0);

			// The COM port can now be closed
			CloseHandle(hCommPort);
			if (sock != INVALID_SOCKET)
			{
				OutStr(TEXT("SKIPPED: Should not have been able to create socket with COM port open\n"));
				closesocket(sock);
			}
			else // socket could not be created, as expected
			{
				// Run a test to ensure that we can do stuff with IR sockets
				result = LocalConnectOKTest(CONNTEST);
				if (result == TPR_PASS)
				{

					Sleep(10000);
					hCommPort = COMPortOpen(portNum, &error);
					if (hCommPort == INVALID_HANDLE_VALUE)
					{
						result = TPR_FAIL;
						OutStr(TEXT("FAILED: Unable to open COM port %d after closing IR sockets\n"), portNum);
					}
					else // we are golden!
					{
						CloseHandle(hCommPort);
					}
				}
			}
		}
	}

	return result;
}


//
// While a connection attempt is in progress to a remote system, try closing the socket and make
// sure that no exceptions occur.

typedef struct 
{
	int		delay_milliseconds;
	SOCKET	sock;
} CloseThreadInfo;

// Close socket after requested time interval

static DWORD WINAPI
CloseThread(
	void *pParm)
{
	CloseThreadInfo *pInfo = (CloseThreadInfo *)pParm;

	OutStr(TEXT("Sleep %d ms\n"), pInfo->delay_milliseconds);
	Sleep(pInfo->delay_milliseconds);

	OutStr(TEXT("Close socket\n"));
	closesocket(pInfo->sock);

	OutStr(TEXT("Exit\n"));
	return 0;
}

TEST_FUNCTION(CloseConnectInProgressTest)
{
	Connection	connServer;
	int			result = TPR_SKIP;
	SOCKET		sockConn;
	int			bindResult, error, connectResult, sendResult;
	DWORD		exitCode, tidClose;
	HANDLE		hThread;
	CloseThreadInfo info;

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return result;

	// Request that the remote side bind
	sendResult = SendBindRequest(&connServer, CONNTEST, &bindResult, &error);
	if (sendResult == FALSE)
	{
		OutStr(TEXT("SKIPPED: Unable to send bind request\n"));
	}
	else if (bindResult != 0)
	{
		OutStr(TEXT("SKIPPED: Remote bind got error %d\n"), error);
	}
	else
	{
		result = TPR_PASS;
		for (int delay_milliseconds = 0; delay_milliseconds <= 300; delay_milliseconds += 10)
		{
			Sleep(2000); 

			OutStr(TEXT("delay = %d\n"), delay_milliseconds);
			sockConn = socket(AF_IRDA, SOCK_STREAM, 0);
			if (sockConn == INVALID_SOCKET)
			{
				OutStr(TEXT("SKIPPED: socket failed, error = %d\n"), WSAGetLastError());
				result = TPR_SKIP;
				break;
			}

			// Spawn a thread to accept connections on the socket
			info.sock = sockConn;
			info.delay_milliseconds = delay_milliseconds;
			hThread = CreateThread(NULL, 0, CloseThread, &info, 0, &tidClose);

			if (hThread == NULL)
			{
				OutStr(TEXT("SKIPPED: CreateThread failed, error = %d\n"), WSAGetLastError());
				result = TPR_SKIP;
				break;
			}

			if (SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST) == FALSE)
			{
				OutStr(TEXT("SetThreadPriority FAILED, error = %d\n"), WSAGetLastError());
				result = TPR_SKIP;
				break;
			}

			// Start the connection attempt
			OutStr(TEXT("Do connect\n"));
			IRConnect(sockConn, CONNTEST, &connectResult, &error);
			OutStr(TEXT("connect result=%d error=%d\n"), connectResult, error);

			OutStr(TEXT("Wait for close thread to exit\n"));
			// Wait for the close thread to exit
			GetExitCodeThread(hThread, &exitCode);

			OutStr(TEXT("Close handle\n"));
			// Delete the close thread
			CloseHandle(hThread);
		}
	}

	ConnectionClose(&connServer);

	return result;
}

//
// Try to connect using a socket which has failed a prior connect attempt.


TEST_FUNCTION(ConnectAgainTest)
{
	Connection	connServer;
	int			result = TPR_SKIP;
	SOCKET		sockConn;
	int			bindResult, error, connectResult, sendResult;

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return result;

	// Connect to a non-existent socket

	sockConn = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sockConn == INVALID_SOCKET)
	{
		OutStr(TEXT("SKIPPED: socket failed, error = %d\n"), WSAGetLastError());
	}
	else
	{
		// Do the first connection attempt, which should fail
		OutStr(TEXT("Do connect\n"));
		IRConnect(sockConn, CONNTEST, &connectResult, &error);
		OutStr(TEXT("connect result=%d error=%d\n"), connectResult, error);

		if (connectResult == 0)
		{
			OutStr(TEXT("SKIPPED: Connect to non-existent service should have failed\n"));
		}
		else
		{
			// Now bind a service name on the remote so we can connect.
			sendResult = SendBindRequest(&connServer, CONNTEST, &bindResult, &error);
			if (sendResult == FALSE)
			{
				OutStr(TEXT("SKIPPED: Unable to send bind request\n"));
			}
			else if (bindResult != 0)
			{
				OutStr(TEXT("SKIPPED: Remote bind got error %d\n"), error);
			}
			else
			{
				// Do the second connection attempt, which should work
				OutStr(TEXT("Do connect\n"));
				IRConnect(sockConn, CONNTEST, &connectResult, &error);
				OutStr(TEXT("connect result=%d error=%d\n"), connectResult, error);
				if (connectResult != 0)
				{
					OutStr(TEXT("FAILED: Connect to %hs should have worked\n"), CONNTEST);
					result = TPR_FAIL;
				}
				else
				{
					result = TPR_PASS;
				}
			}
		}
	}

	CloseSocket(sockConn);

	ConnectionClose(&connServer);

	return result;
}