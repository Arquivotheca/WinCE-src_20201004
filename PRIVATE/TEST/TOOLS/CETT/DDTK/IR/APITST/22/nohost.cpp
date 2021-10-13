//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <oscfg.h>
#include <katoex.h>
#include <tux.h>
#include <stdlib.h>
#include <stdio.h>
#undef AF_IRDA
#include <af_irda.h>
#include <irsup.h>
#include "shellproc.h"
#include "nohost.h"


/*-----------------------------------------------------------------------------
  Function		:	CompareSetValueAgainstQuery
  Description	:	Test to see if a function call leaks memory. The function
					to be tested should be memory usage neutral, unless the goal
					is to find out how much memory a particular function uses.

  Parameters	:	IN	pTestFunction
						- Function to be tested.  If this function
						returns something other than TPR_PASS then
						the memory leak test will be aborted.
					IN	numInitIterations
						- Number of iterations of pTestFunction to be
						executed to achieve a steady state before
						looking for memory loss.
					IN	numTestIterations
						- Number of iterations of pTestFunction to
						execute to detect memory loss.
					OUT pAmountLeakedPerIteration
						- Place to store the amount of memory leaked
						if MemoryLeakTest returns TPR_FAIL or TPR_PASS.

  Returns		:	TPR_SKIP
						if pTestFunction returns something other than TPR_PASS
					TPR_FAIL
						if 1 or more bytes of memory are lost running pTestFunction
				    TPR_PASS
						if no memory is lost (or memory is recovered)
  Comments		:
------------------------------------------------------------------------------*/

//	If a function is not leaking at least this much per call, then it is probably
//	not leaking at all.

#define MINIMUM_REASONABLE_LEAK_PER_CALL	16

extern int
MemoryLeakTest(
	int (*pTestFunction)(void),
	int numInitIterations,
	int numTestIterations,
	int *pAmountLeakedPerIteration)
{
	int			 result, amountLeaked;
	MEMORYSTATUS msCheckPoint, msCurrent;

	// Prior to starting to look for memory leaks, execute the
	// test function a number of times to allow for any cache
	// or stack type storage usage to reach a steady state.

	while (numInitIterations--)
	{
		result = (*pTestFunction)();
		if (result != TPR_PASS)
		{
			// Unable to execute memory leak tests since test
			// function is not passing.
			return TPR_SKIP;
		}
	}

	// Establish memory checkpoint
	msCheckPoint.dwLength = sizeof(msCheckPoint);
	GlobalMemoryStatus(&msCheckPoint);


	// Execute the test function a number of times to cause it
	// to leak memory if it does so.

	for (int i = 0; i < numTestIterations; i++)
	{
		result = (*pTestFunction)();
		if (result != TPR_PASS)
		{
			// Unable to execute memory leak tests since test
			// function is not passing.
			return TPR_SKIP;
		}
	}

	// Determine how much, if any, memory leaked.
	// If the amount leaked is negative, then the
	// function is recovering memory and this is counted
	// as a pass.

	msCurrent.dwLength = sizeof(msCurrent);
	GlobalMemoryStatus(&msCurrent);

	OutStr(TEXT("Before AvailPhys = %#x\n"), msCheckPoint.dwAvailPhys);
	OutStr(TEXT("After  AvailPhys = %#x\n"), msCurrent.dwAvailPhys);

	amountLeaked = msCheckPoint.dwAvailPhys - msCurrent.dwAvailPhys;

	// Round up the amount leaked per iteration
	if (numTestIterations > 0)
		*pAmountLeakedPerIteration = (amountLeaked + numTestIterations - 1) / numTestIterations;
	else
		*pAmountLeakedPerIteration = 0;

	if (*pAmountLeakedPerIteration < MINIMUM_REASONABLE_LEAK_PER_CALL)
	{
		amountLeaked = 0;
		*pAmountLeakedPerIteration = 0;
	}

	return TPR_PASS;
}

/*-----------------------------------------------------------------------------
  Function		:	SocketIteration
  Description	:	Support routine to call a function multiple times with
					a fresh socket each time on an iterated value
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

int
SocketIteration(
	int (*pfunc)(SOCKET s, int i),
	int first,
	int last)
{
	SOCKET			sock1;
	SOCKADDR_IRDA	addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int				result = TPR_PASS;

	for (int i = first; i <= last && result != TPR_FAIL; i++)
	{
		sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
		if (sock1 == INVALID_SOCKET)
		{
			result = TPR_SKIP;
			break;
		}

		result = pfunc(sock1, i);

		if (closesocket(sock1) == SOCKET_ERROR)
		{
			OutStr(TEXT("closesocket FAILED\n"));
			if (result != TPR_FAIL)
				result = TPR_SKIP;
			break;
		}
	}

	return result;
}

//
// A support routine to close a socket and log problems
// if they occur.
//
int
CloseSocket(SOCKET sock)
{
	if (sock != INVALID_SOCKET && closesocket(sock) == SOCKET_ERROR)
	{
		OutStr(TEXT("NOTE: closesocket FAILED\n"));
		return SOCKET_ERROR;
	}
	return 0;
}

//------------------------- Tux Testing Functions -----------------------------

//
// Socket Create Test - Make sure that IR socket creation works for valid parameters
//

TEST_FUNCTION(SocketCreateTest)
{
	SOCKET sock;
	int result;

	TEST_ENTRY;

	// Test Setup - none

	// Test Execution
	// Create an IR socket with the only set of valid parameters

	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
			OutStr(TEXT("socket err %d\n"), WSAGetLastError());
		result = TPR_FAIL;
	}
	else
	{
		result = TPR_PASS;

		// Test Cleanup
		(void)CloseSocket(sock);
	}

	return result;
}

//
// SocketCreateInvalidTest - Make sure that IR socket creation fails for invalid parameters
//

TEST_FUNCTION(SocketCreateInvalidTest)
{
	SOCKET sock;
	int result;

	TEST_ENTRY;

	// Test Setup - none

	// Test Execution
	// Create an IR socket with invalid parameters
	sock = socket(AF_IRDA, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		result = TPR_PASS;
	}
	else
	{
		result = TPR_FAIL;

		// Test Cleanup - close the erroneously created socket
		(void)CloseSocket(sock);
	}
	return result;
}

//
// Socket Close Test - Make sure that IR socket deletion works for valid parameters
//
TEST_FUNCTION(SocketCloseTest)
{
	SOCKET sock;
	int result;

	TEST_ENTRY;

	// Test Setup - none
	// Create an IR socket with the only set of valid parameters

	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		result = TPR_SKIP;
	}
	else
	{
		// Test Execution - Try to close the socket
		if (closesocket(sock) == SOCKET_ERROR)
		{
			result = TPR_FAIL;
		}
		else
		{
			result = TPR_PASS;
		}
	}

	// Test Cleanup - none

	return result;
}

//
// Note that a closesocket test for invalid parameters is not IR specific and so
// does not belong here.
//

//
// This function creates then destroys a socket and should be memory usage neutral.
//
int SocketCreateAndDestroy(void)
{
	SOCKET sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
		return TPR_SKIP;

	if (closesocket(sock) == SOCKET_ERROR)
		return TPR_SKIP;

	return TPR_PASS;
}

// SocketMemoryLeakTest - Check that calling socket -- closesocket results in
// no loss of memory.
//
TEST_FUNCTION(SocketMemoryLeakTest)
{
	int amountLeakedPerIteration, result;

	TEST_ENTRY;

	result = MemoryLeakTest(SocketCreateAndDestroy, 5, 300, &amountLeakedPerIteration);
	if (result == TPR_FAIL)
	{
		OutStr(TEXT("Socket Create/Destroy leaking %d bytes per call\n"), amountLeakedPerIteration);
	}
	return result;
}

// BindTest -- Check that binding a valid address returns ok.
//
TEST_FUNCTION(BindTest)
{
	SOCKET sock;
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create a socket
	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock != INVALID_SOCKET)
	{
		// Test Execution - Bind a name to the socket
		result = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (result == 0)
			result = TPR_PASS;
		else
			result = TPR_FAIL;

		// Test Cleanup - Destroy the socket
		(void)CloseSocket(sock);
	}

   return result;
}

// BindInvalidTest -- Check that binding an invalid address family returns an error.
//
TEST_FUNCTION(BindInvalidFamilyTest)
{
	SOCKET sock;
	SOCKADDR_IRDA addr = {AF_INET, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create a socket
	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock != INVALID_SOCKET)
	{
		// Test Execution - Bind an invalid address family to the socket
		int bindResult = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (bindResult == SOCKET_ERROR)
		{
			// What should the WSAGetLastError() code be? WSAEAFNOSUPPORT
			if (WSAGetLastError() != WSAEAFNOSUPPORT)
				OutStr(TEXT("expected WSAEAFNOSUPPORT from bind, got %d\n"), WSAGetLastError());
			result = TPR_PASS;
		}
		else
			result = TPR_FAIL;

		// Test Cleanup - Destroy the socket
		(void)CloseSocket(sock);
	}

	return result;
}


// Try binding the same name twice to the same socket. The second
// bind should fail.
//
TEST_FUNCTION(BindTwiceTest)
{
	SOCKET sock1;
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create sockets, bind a name to one
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		int bindResult = bind(sock1, (struct sockaddr *)&addr, sizeof(addr));
		if (bindResult == 0)
		{
			// Test Execution - Bind same name to the socket again
			bindResult = bind(sock1, (struct sockaddr *)&addr, sizeof(addr));
			if (bindResult == SOCKET_ERROR)
			{
				// What should the WSAGetLastError() code be? WSAEINVAL
				if (WSAGetLastError() != WSAEINVAL)
					OutStr(TEXT("expected WSAEINVAL from bind, got %d\n"), WSAGetLastError());
				result = TPR_PASS;
			}
			else
				result = TPR_FAIL;
		}
	}

	// Test Cleanup - Destroy the socket
	(void)CloseSocket(sock1);

   return result;
}

//  Bind a name to a socket, then close the socket and bind the name
//  again to confirm that the name is once again available for use.
//
TEST_FUNCTION(BindCloseBindTest)
{
	SOCKET sock;
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create a socket, bind a name to it, then close it
	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock != INVALID_SOCKET)
	{
		result = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (result == 0)
		{
			if (CloseSocket(sock) == 0)
			{
				sock = socket(AF_IRDA, SOCK_STREAM, 0);
				if (sock != INVALID_SOCKET)
				{
					// Test Execution - Try to bind the name again
					result = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
					if (result == SOCKET_ERROR)
					{
						OutStr(TEXT("FAILED: Second bind with same name failed, error=%d\n"), WSAGetLastError());
						result = TPR_FAIL;
					}
					else
					{
						result = TPR_PASS;
					}
				}
			}
		}
		else
			OutStr(TEXT("SKIPPED: Unable to do initial bind, error=%d\n"), WSAGetLastError());

		// Test Cleanup - Destroy the socket
		(void)CloseSocket(sock);
	}

   return result;
}

// Try binding the same name twice to different sockets. The second
// bind should fail.
//
TEST_FUNCTION(BindSameNameTo2SocketsTest)
{
	SOCKET sock1, sock2;
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create sockets, bind a name to one
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	sock2 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET && sock2 != INVALID_SOCKET)
	{
		int bindResult = bind(sock1, (struct sockaddr *)&addr, sizeof(addr));
		if (bindResult == 0)
		{
			// Test Execution - Bind same name to the second socket
			bindResult = bind(sock2, (struct sockaddr *)&addr, sizeof(addr));
			if (bindResult == SOCKET_ERROR)
			{
				// What should the WSAGetLastError() code be? WSAEADDRINUSE
				if (WSAGetLastError() != WSAEADDRINUSE)
					OutStr(TEXT("expected WSAEADDRINUSE from bind, got %d\n"), WSAGetLastError());
				result = TPR_PASS;
			}
			else
			{
				OutStr(TEXT("FAILED: bind() allowed the same name on a second socket"));
				result = TPR_FAIL;
			}
		}
	}

	// Test Cleanup - Destroy the socket
	(void)CloseSocket(sock1);
	(void)CloseSocket(sock2);

   return result;
}

// Try binding to a NULL socket.  The bind should fail.

TEST_FUNCTION(BindNullSocketTest)
{
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - none

	SOCKET sock = NULL;		

	// Test Execution - Bind to NULL socket.

	int bindResult = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (bindResult == SOCKET_ERROR)
	{
		// What should the WSAGetLastError() code be? WSAENOTSOCK
		if (WSAGetLastError() != WSAENOTSOCK)
			OutStr(TEXT("expected WSAENOTSOCK from bind, got %d\n"), WSAGetLastError());
		result = TPR_PASS;
	}
	else
		result = TPR_FAIL;

	// Test Cleanup - none

   return result;
}

// Try binding a NULL addr.
// The bind should fail.
//
TEST_FUNCTION(BindNullAddrTest)
{
	SOCKET sock1;
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create socket
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		// Test Execution - Bind NULL addr to the socket

		int bindResult = bind(sock1, NULL, sizeof(SOCKADDR_IRDA));
		if (bindResult == SOCKET_ERROR)
		{
			// What should the WSAGetLastError() code be? WSAEFAULT
			if (WSAGetLastError() != WSAEFAULT)
				OutStr(TEXT("expected WSAEFAULT from bind, got %d\n"), WSAGetLastError());
			result = TPR_PASS;
		}
		else
			result = TPR_FAIL;
	}

	// Test Cleanup - Destroy the socket
	(void)CloseSocket(sock1);

	return result;
}

// Try bind using various invalid address lengths, some too
// small and some too big.

int InvalidAddrLengths[] =
{
	-1,
	0,
	1,
	offsetof(SOCKADDR_IRDA, irdaServiceName[0]), // presumably must have at least one byte of name
	INT_MIN
};

int

BindInvalidAddressLength(
	SOCKET s,
	int i)
{
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, "MyServer"};

	// Test Execution - Bind same name to the socket again
	int bindResult = bind(s, (struct sockaddr *)&addr, InvalidAddrLengths[i]);

	if (bindResult == SOCKET_ERROR)
	{
		// What should the WSAGetLastError() code be? WSAEFAULT
		if (WSAGetLastError() != WSAEFAULT)
			OutStr(TEXT("expected WSAEFAULT from bind for addr length %d, got %d\n"), InvalidAddrLengths[i], WSAGetLastError());
		return TPR_PASS;
	}
	else
	{
		OutStr(TEXT("FAILED: bind should NOT have allowed namelen %d\n"), InvalidAddrLengths[i]);
		return TPR_FAIL;
	}
}

TEST_FUNCTION(BindHugeAddrLengthTest)
{
	TEST_ENTRY;

    return SocketIteration(BindInvalidAddressLength, 0, countof(InvalidAddrLengths) - 1);
}

void
GenerateName(
	char *nameBuffer,
	int   length)
{
	for (int i = 0; i < length; i++)
		*nameBuffer++ = '0' + i % 10;
	*nameBuffer = '\0';
}

#define MIN_IRDA_SERVICE_NAME_LENGTH 0
#define MAX_IRDA_SERVICE_NAME_LENGTH (sizeof(((SOCKADDR_IRDA *)NULL)->irdaServiceName) - 1)

int
BindServiceNameLength(
	SOCKET s,
	int length)
{
	BOOL shouldWork;
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, ""};

	// Note that a bind of a length greater than MAX_IRDA_SERVICE_NAME_LENGTH
	// should work, the OS should automatically zero terminate it transparently
	// for the user.  This is a 'feature'.
	shouldWork = MIN_IRDA_SERVICE_NAME_LENGTH <= length;

	// Test Execution - Generate a null terminated (except at maxlen)
	// name of the current length, bind to the socket
	memset(&addr.irdaServiceName[0], 0, sizeof(addr.irdaServiceName));
	memset(&addr.irdaServiceName[0], '0' + length, length);

	int bindResult = bind(s, (struct sockaddr *)&addr, sizeof(addr));
	if (shouldWork && bindResult != 0)
	{
		OutStr(TEXT("FAILED: Bind with irdaServiceName length %d got error %d\n"), length, WSAGetLastError());
		return TPR_FAIL;
	}
	else if (!shouldWork && bindResult != SOCKET_ERROR)
	{
		OutStr(TEXT("FAILED: Bind with irdaServiceName length %d should not have worked\n"), length);
		return TPR_FAIL;
	}
	else
	{
		return TPR_PASS;
	}
}

// Try binding various valid addresses.

TEST_FUNCTION(BindVariousAddrTest)
{
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, ""};

	TEST_ENTRY;

	return SocketIteration(BindServiceNameLength, 0, MAX_IRDA_SERVICE_NAME_LENGTH + 1);
}

// GetBoundSockNameTest -- Check that getsockname works on bound name
//
int
BindAndGetServiceNameLength(
	SOCKET sock,
	int length)
{
	SOCKADDR_IRDA	addrGet;
	int				result = TPR_SKIP, size, error;
	char			name[MAX_IRDA_SERVICE_NAME_LENGTH + 2];

	ASSERT((length + 1) <= sizeof(name));

	GenerateName(&name[0], length);

	// Test Setup - Bind a name to the socket
	IRBind(sock, &name[0], &result, &error);
	if (result == 0)
	{
		// Test Execution - Call getsockname
		size = sizeof(addrGet);
		result = getsockname(sock, (struct sockaddr *)&addrGet, &size);
		if (result == 0)
		{
			// The returned name should be identical to the bound name,
			// except for a non-'\0' terminated name in which case the
			// last char of the returned name should have been replaced
			// by '\0'.
			if (length <= MAX_IRDA_SERVICE_NAME_LENGTH)
			{
				if (strcmp(&name[0], &addrGet.irdaServiceName[0]) != 0)
				{
					OutStr(TEXT("FAILED: bound %hs != getsockname %hs\n"),
						&name[0], &addrGet.irdaServiceName[0]);
					result = TPR_FAIL;
				}
				else
					result = TPR_PASS;
			}
			else
			{
				// First MAX_IRDA_SERVICE_NAME_LENGTH chars should match
				if (strncmp(&name[0], &addrGet.irdaServiceName[0], MAX_IRDA_SERVICE_NAME_LENGTH) != 0)
				{
					OutStr(TEXT("FAILED: bound %hs != getsockname %hs\n"),
						&name[0], &addrGet.irdaServiceName[0]);
					result = TPR_FAIL;
				}

				// Last char of returned addr should be '\0'
				else if (addrGet.irdaServiceName[MAX_IRDA_SERVICE_NAME_LENGTH] != '\0')
				{
					OutStr(TEXT("FAILED: getsockname for length %d returned non-null[%c] terminated string\n"),
						length, addrGet.irdaServiceName[MAX_IRDA_SERVICE_NAME_LENGTH]);
					result = TPR_FAIL;
				}
				else
					result = TPR_PASS;
			}
		}
		else
		{
			OutStr(TEXT("FAILED: getsockname returned error %d\n"), WSAGetLastError());
			result = TPR_FAIL;
		}

	}

	return result;
}

// Try binding various valid addresses and retrieving their bound values.

TEST_FUNCTION(GetBoundSockNameTest)
{
	SOCKADDR_IRDA addr = {AF_IRDA, 0, 0, 0, 0, ""};

	TEST_ENTRY;

	return SocketIteration(BindAndGetServiceNameLength, 1, MAX_IRDA_SERVICE_NAME_LENGTH + 1);
}

//  Bind memory leak tests

//
// This function runs a variety of bind functions
//
int
BindAll(void)
{
	int result;

	result = SocketIteration(BindServiceNameLength, 0, MAX_IRDA_SERVICE_NAME_LENGTH + 1);
	if (result != TPR_PASS)
		return result;

	return result;
}

// BindMemoryLeakTest - Check that calling various binds results in
// no loss of memory.
//
TEST_FUNCTION(BindMemoryLeakTest)
{
	int    amountLeakedPerIteration, result;

	TEST_ENTRY;

	// Create a socket to hold the COM port open for SIR so that time is not wasted opening and closing it.
	SOCKET s = socket(AF_IRDA, SOCK_STREAM, 0);
	result = MemoryLeakTest(BindAll, 5, 300, &amountLeakedPerIteration);
	if (result == TPR_FAIL)
	{
		OutStr(TEXT("BindAll leaking %d bytes per call\n"), amountLeakedPerIteration);
	}
	CloseSocket(s);
	return result;
}

//----------------------------- IRLMP_IAS_SET Tests ---------------------------


int
SetIASInt(
	SOCKET	s,
	int		integerValue)
{
	IAS_SET value;

	strcpy(&value.irdaClassName[0],  "test");
	strcpy(&value.irdaAttribName[0], "test");

	// Set integer attribute value

	value.irdaAttribType = IAS_ATTRIB_INT;
	value.irdaAttribute.irdaAttribInt = integerValue;
	int setResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)&value, sizeof(value));
	if (setResult != 0)
	{
		OutStr(TEXT("IRLMP_IAS_SET gave error %d\n"), WSAGetLastError());
		return TPR_FAIL;
	}
	else
		return TPR_PASS;
}

// Simple Tests

//
//  This is a simple test function that does a basic IRLMP_IAS_SET.
//
TEST_FUNCTION(SetIASTest)
{
	SOCKET  sock1;
	int		result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create socket
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		// Test Execution - Do a setsockopt for an integer
		result = SetIASInt(sock1, 0);
	}

	// Test Cleanup - Destroy the socket
	(void)CloseSocket(sock1);

	return result;
}

//
//  A simple test trying an invalid attribute type for an
//  IRLMP_IAS_SET.
//
TEST_FUNCTION(SetIASInvalidTest)
{
	SOCKET  sock1;
	IAS_SET value;
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create socket
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		// Test Execution - Do a setsockopt for an attribute
		strcpy(&value.irdaClassName[0],  "test");
		strcpy(&value.irdaAttribName[0], "test");
		value.irdaAttribType = ~0; // invalid type
		value.irdaAttribute.irdaAttribInt = 0;

		int setResult = setsockopt(sock1, SOL_IRLMP, IRLMP_IAS_SET, (char *)&value, sizeof(value));
		if (setResult == SOCKET_ERROR)
		{
			// WSAGetLastError should be WSAEINVAL
			if (WSAGetLastError() != WSAEINVAL)
				OutStr(TEXT("expected WSAEINVAL from IRLMP_IAS_SET, got %d\n"), WSAGetLastError());
			result = TPR_PASS;
		}
		else
		{
			result = TPR_FAIL;
		}
	}

	// Test Cleanup - Destroy the socket
	(void)CloseSocket(sock1);

	return result;
}

//	Integer Tests

int ValidIntValues[] =
{
	0,
	1,
	-1,
	127,
	128,
	255,
	256,
	65535,
	65536,
	INT_MAX,
	INT_MIN
};

int
SetIASValidInt(
	SOCKET	s,
	int		i)
{
	// Test Execution - Do a setsockopt for a variety of attributes

	return SetIASInt(s, ValidIntValues[i]);
}

TEST_FUNCTION(SetIASVariousIntAttribTest)
{
	TEST_ENTRY;

	return SocketIteration(SetIASValidInt, 0, countof(ValidIntValues) - 1);
}


// Octet Sequence Tests

#define MIN_OCTETSEQ_LENGTH 0
#define MAX_OCTETSEQ_LENGTH 1024

int
SetIASOctetSeq(
	SOCKET	s,
	int		length,
	BOOL	bShouldWork)
{
	BYTE    buffer[sizeof(IAS_SET) + MAX_OCTETSEQ_LENGTH];
	IAS_SET *pvalue = (IAS_SET *)&buffer[0];

	// Test Execution - Do a setsockopt
	strcpy(&pvalue->irdaClassName[0],  "test");
	strcpy(&pvalue->irdaAttribName[0], "test");

	pvalue->irdaAttribType = IAS_ATTRIB_OCTETSEQ;

	pvalue->irdaAttribute.irdaAttribOctetSeq.Len = length;
	if (MIN_OCTETSEQ_LENGTH <= length && length <= MAX_OCTETSEQ_LENGTH)
		memset(&pvalue->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0], (BYTE)length, length);
	int setResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)pvalue, sizeof(IAS_SET) + length);
	if (bShouldWork && setResult != 0)
	{
		OutStr(TEXT("FAILED: set IAS_ATTRIB_OCTETSEQ of length %d error %d\n"), length, WSAGetLastError());
		return TPR_FAIL;
	}
	else if (!bShouldWork && setResult == 0)
	{
		OutStr(TEXT("FAILED: set IAS_ATTRIB_OCTETSEQ of length %d should NOT work\n"), length);
		return TPR_FAIL;
	}
	else
		return TPR_PASS;
}

int ValidOctetSeqLengths[] =
{
	MIN_OCTETSEQ_LENGTH,
	MIN_OCTETSEQ_LENGTH + 1,
	MIN_OCTETSEQ_LENGTH + (MAX_OCTETSEQ_LENGTH - MIN_OCTETSEQ_LENGTH) / 2,
	MAX_OCTETSEQ_LENGTH - 1,
	MAX_OCTETSEQ_LENGTH,
};

int
SetIASValidOctetSeq(
	SOCKET	s,
	int		index)
{
	return SetIASOctetSeq(s, ValidOctetSeqLengths[index], TRUE);
}

int InvalidOctetSeqLengths[] =
{
	MIN_OCTETSEQ_LENGTH - 1,
	MAX_OCTETSEQ_LENGTH + 1,
	INT_MAX,
	INT_MIN
};

int
SetIASInvalidOctetSeq(
	SOCKET s,
	int	   i)
{
	return SetIASOctetSeq(s, InvalidOctetSeqLengths[i], FALSE);
}

TEST_FUNCTION(SetIASVariousOctetSeqAttribTest)
{
	int result;

	TEST_ENTRY;

	// Test all valid octet sequence lengths
	result = SocketIteration(SetIASValidOctetSeq, 0, countof(ValidOctetSeqLengths) - 1);

	if (result == TPR_PASS)
		// Test various invalid octet sequence lengths
		result = SocketIteration(SetIASInvalidOctetSeq, 0, countof(InvalidOctetSeqLengths) - 1);

	return result;
}

//	User String Tests

#define MIN_USRSTR_LENGTH 0
#define MAX_USRSTR_LENGTH 255
#define CHARSET_ASCII     0

int
SetIASUsrStr(
	SOCKET  s,
	int		length,
	BOOL	bShouldWork)
{
	BYTE    buffer[sizeof(IAS_SET) + MAX_USRSTR_LENGTH];
	IAS_SET *pvalue = (IAS_SET *)&buffer[0];

	// Test Execution - Do a setsockopt

	strcpy(&pvalue->irdaClassName[0],  "test");
	strcpy(&pvalue->irdaAttribName[0], "test");

	pvalue->irdaAttribType = IAS_ATTRIB_STR;
	pvalue->irdaAttribute.irdaAttribUsrStr.Len = length;
	pvalue->irdaAttribute.irdaAttribUsrStr.CharSet = CHARSET_ASCII;
	if (MIN_USRSTR_LENGTH <= length && length <= MAX_USRSTR_LENGTH)
		memset(&pvalue->irdaAttribute.irdaAttribUsrStr.UsrStr[0], 'a', length);
	int setResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)pvalue, sizeof(IAS_SET) + length);
	if (bShouldWork && setResult != 0)
	{
		OutStr(TEXT("FAILED: set IAS_ATTRIB_STR of length %d error %d\n"), length, WSAGetLastError());
		return TPR_FAIL;
	}
	else if (!bShouldWork && setResult == 0)
	{
		OutStr(TEXT("FAILED: set IAS_ATTRIB_STR of length %d should NOT work\n"), length);
		return TPR_FAIL;
	}
	return TPR_PASS;
}

int ValidUsrStrLengths[] =
{
	MIN_USRSTR_LENGTH,
	MIN_USRSTR_LENGTH + 1,
	MIN_USRSTR_LENGTH + (MAX_USRSTR_LENGTH - MIN_USRSTR_LENGTH) / 2,
	MAX_USRSTR_LENGTH - 1,
	MAX_USRSTR_LENGTH
};

int
SetIASValidUsrStr(
	SOCKET	s,
	int		index)
{
	return SetIASUsrStr(s, ValidUsrStrLengths[index], TRUE);
}

int InvalidUsrStrLengths[] =
{
	MIN_USRSTR_LENGTH - 1,
	MAX_USRSTR_LENGTH + 1,
	INT_MAX,
	INT_MIN
};

int
SetIASInvalidUsrStr(
	SOCKET	s,
	int		i)
{
	return SetIASUsrStr(s, InvalidUsrStrLengths[i], FALSE);
}

TEST_FUNCTION(SetIASVariousUsrStrAttribTest)
{
	TEST_ENTRY;

	// Test all valid user string lengths
	int result = SocketIteration(SetIASValidUsrStr, 0, countof(ValidUsrStrLengths) - 1);

	// Test various invalid user string lengths
	if (result == TPR_PASS)
		result = SocketIteration(SetIASInvalidUsrStr, 0, countof(InvalidUsrStrLengths) - 1);

	return result;
}

// ClassName tests

#define MIN_VALID_CLASSNAME_LENGTH 1
#define MAX_VALID_CLASSNAME_LENGTH 60

int ClassNameTestLength[] =
{
	0,
	1,
	2,
	MAX_VALID_CLASSNAME_LENGTH / 2,
	MAX_VALID_CLASSNAME_LENGTH - 1,
	MAX_VALID_CLASSNAME_LENGTH,
	sizeof(((IAS_SET *)(0))->irdaClassName)
};

int
SetIASClassName(
	SOCKET	s,
	int		index)
{
	int		length = ClassNameTestLength[index];
	IAS_SET	value;
	BOOL	bShouldWork;
	int		memsetLength;

	// Apparently the OS is supposed to truncate a classname
	// longer than 60 without telling the programmer (i.e. it
	// does not return an error).  So lengths that are too large
	// should work (insofar as not reporting an error).

	bShouldWork = (MIN_VALID_CLASSNAME_LENGTH <= length);

	memsetLength = length;
	if (memsetLength > sizeof(value.irdaClassName))
		memsetLength = sizeof(value.irdaClassName);

	memset(&value.irdaClassName[0],  'a', memsetLength);

	// Null terminate the string if possible
	if (length < sizeof(value.irdaClassName))
		value.irdaClassName[length] = '\0';

	strcpy(&value.irdaAttribName[0], "test");

	value.irdaAttribType = IAS_ATTRIB_INT;
	value.irdaAttribute.irdaAttribInt = 0;
	int setResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)&value, sizeof(value));
	if (bShouldWork && setResult != 0)
	{
		OutStr(TEXT("FAILED: set ClassName of length %d error %d\n"), length, WSAGetLastError());
		return TPR_FAIL;
	}
	else if (!bShouldWork && setResult == 0)
	{
		OutStr(TEXT("FAILED: set ClassName of length %d should NOT work\n"), length);
		return TPR_FAIL;
	}
	return TPR_PASS;
}

TEST_FUNCTION(SetIASClassNameLengthTest)
{
	TEST_ENTRY;

	// Test various class name lengths
	return SocketIteration(SetIASClassName, 0, countof(ClassNameTestLength) - 1);
}


// AttribName tests

#define MIN_VALID_ATTRIBNAME_LENGTH 0
#define MAX_VALID_ATTRIBNAME_LENGTH 60

int AttribNameTestLength[] =
{
	0,
	1,
	2,
	MAX_VALID_CLASSNAME_LENGTH / 2,
	MAX_VALID_CLASSNAME_LENGTH - 1,
	MAX_VALID_CLASSNAME_LENGTH,
	sizeof(((IAS_SET *)(0))->irdaClassName)
};

int
SetIASAttribName(
	SOCKET	s,
	int		index)
{
	int		length = AttribNameTestLength[index];
	IAS_SET	value;
	BOOL	bShouldWork;
	int		memsetLength;

	bShouldWork = (MIN_VALID_ATTRIBNAME_LENGTH <= length) &&
				  (length <= sizeof(value.irdaAttribName));

	strcpy(&value.irdaClassName[0], "test");

	memsetLength = length;
	if (memsetLength > sizeof(value.irdaAttribName))
		memsetLength = sizeof(value.irdaAttribName);

	memset(&value.irdaAttribName[0],  'a', memsetLength);

	// Null terminate the string if possible
	if (length < sizeof(value.irdaAttribName))
		value.irdaAttribName[length] = '\0';

	//OutStr(TEXT("Set IAS Attrib Name Len=%d, name=%hs\n"), length, &value.irdaAttribName[0]);

	value.irdaAttribType = IAS_ATTRIB_INT;
	value.irdaAttribute.irdaAttribInt = 0;
	int setResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)&value, sizeof(value));
	if (bShouldWork && setResult != 0)
	{
		OutStr(TEXT("FAILED: set AttribName of length %d error %d\n"), length, WSAGetLastError());
		return TPR_FAIL;
	}
	else if (!bShouldWork && setResult == 0)
	{
		OutStr(TEXT("FAILED: set AttribName of length %d should NOT work\n"), length);
		return TPR_FAIL;
	}
	return TPR_PASS;
}

TEST_FUNCTION(SetIASAttribNameLengthTest)
{
	TEST_ENTRY;

	// Test various attribute name lengths
	int result = SocketIteration(SetIASAttribName, 0, countof(AttribNameTestLength) - 1);

	return result;
}

//
// The IrLMP protocol spec states that an object can have a maximum
// of 256 attributes.  This test ensures that up to 256 are allowed,
// but no more.
//

#define MAX_NUMBER_OF_ATTRIBUTES_PER_OBJECT 256

TEST_FUNCTION(SetIASMaxAttributeCountTest)
{
	SOCKET  sock1;
	IAS_SET value;
	int		result = TPR_SKIP;

	TEST_ENTRY;

	value.irdaAttribType = IAS_ATTRIB_INT;
	value.irdaAttribute.irdaAttribInt = 0;
	strcpy(&value.irdaClassName[0],  "test");

	// Test Setup - Create socket
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		// Test one more than the max number of attributes

		for (int i = 0;
			 i <= MAX_NUMBER_OF_ATTRIBUTES_PER_OBJECT && result != TPR_FAIL;
			 i++)
		{
			sprintf(&value.irdaAttribName[0], "%d", i);

			int setResult = setsockopt(sock1, SOL_IRLMP, IRLMP_IAS_SET, (char *)&value, sizeof(value));
			if (i < MAX_NUMBER_OF_ATTRIBUTES_PER_OBJECT && setResult != 0)
			{
				OutStr(TEXT("FAILED: IRLMP_IAS_SET gave error %d for attribute #%d\n"), WSAGetLastError(), i);
				result = TPR_FAIL;
			}
			else if (i >= MAX_NUMBER_OF_ATTRIBUTES_PER_OBJECT && (setResult == 0))
			{
				OutStr(TEXT("FAILED: IRLMP_IAS_SET should not have worked for attribute #%d\n"), i);
				result = TPR_FAIL;
			}
		}

		if (result == TPR_SKIP)
			result = TPR_PASS;

		CloseSocket(sock1);
	}

	return result;
}

//  Set IAS memory leak tests

//
// This function runs a variety of IAS_SET functions
//
int
SetIASAll(void)
{
	int result;

	result = SocketIteration(SetIASValidInt, 0, countof(ValidIntValues) - 1);
	if (result != TPR_PASS)
		return result;

	result = SocketIteration(SetIASValidOctetSeq, 0, countof(ValidOctetSeqLengths) - 1);
	if (result != TPR_PASS)
		return result;

	result = SocketIteration(SetIASValidUsrStr, 0, countof(ValidUsrStrLengths) - 1);
	if (result != TPR_PASS)
		return result;

	result = SocketIteration(SetIASClassName, 0, countof(ClassNameTestLength) - 1);
	if (result != TPR_PASS)
		return result;

	result = SocketIteration(SetIASAttribName, 0, countof(AttribNameTestLength) - 1);
	if (result != TPR_PASS)
		return result;

	return result;
}

// SetIASMemoryLeakTest - Check that calling various IAS_SETs results in
// no loss of memory.
//
TEST_FUNCTION(SetIASMemoryLeakTest)
{
	int    amountLeakedPerIteration, result;

	TEST_ENTRY;

	// Create a socket to hold the COM port open for SIR so that time is not wasted opening and closing it.
	SOCKET s = socket(AF_IRDA, SOCK_STREAM, 0);
	result = MemoryLeakTest(SetIASAll, 5, 300, &amountLeakedPerIteration);
	if (result == TPR_FAIL)
	{
		OutStr(TEXT("SetIASAll leaking %d bytes per call\n"), amountLeakedPerIteration);
	}
	CloseSocket(s);
	return result;
}

//----------------------------- LSAP-SEL Tests --------------------------------

#define MIN_LSAP_SEL		0
#define MAX_LSAP_SEL		127
#define IAS_LSAP_SEL		0		// LSAP-SEL reserved for use by the IAS
#define IAS_QUERY_LSAP_SEL	3		// reserved for doing IAS queries of the peer

#define LSAP_SEL_RESERVED(n) \
		((n) == IAS_LSAP_SEL || (n) == IAS_QUERY_LSAP_SEL)

#define LSAP_SEL_TXT	 "LSAP-SEL"
#define LSAP_SEL_TXT_LEN 8


//	Hard coded LSAP-SEL tests

int LsapSelValues[] =
{
	0,
	1,
	2,
	3,
	63,
	64,
	127,
	128,
	255,
	256,
	1000,		// interesting because it encodes to 4 ascii chars
	INT_MAX,
	-1
};


int
BindHardCodedLsapSel(
	SOCKET	sock,
	int		i)
{
	int				result = TPR_SKIP, lsapSel, size, error;
	char			nameBind[MAX_IRDA_SERVICE_NAME_LENGTH+1];
	SOCKADDR_IRDA	addrGet;
	BOOL			bShouldWork;

	lsapSel = LsapSelValues[i];
	bShouldWork = (MIN_LSAP_SEL <= lsapSel) && (lsapSel <= MAX_LSAP_SEL) && (!LSAP_SEL_RESERVED(lsapSel));

	sprintf(&nameBind[0], "LSAP-SEL%d", lsapSel);

	// Test Setup - Bind a name to the socket
	IRBind(sock, &nameBind[0], &result, &error);
	if (result == 0)
	{
		if (!bShouldWork)
		{
			OutStr(TEXT("FAILED: IRBind allowed the name '%hs'\n"), &nameBind[0]);
			result = TPR_FAIL;
		}
		else
		{
			// Call getsockname
			size = sizeof(addrGet);
			result = getsockname(sock, (struct sockaddr *)&addrGet, &size);
			if (result == 0)
			{
				// The returned name should be identical to the bound name,
				if (strcmp(&nameBind[0], &addrGet.irdaServiceName[0]) != 0)
				{
					OutStr(TEXT("FAILED: bound %hs != getsockname %hs\n"),
							&nameBind[0], &addrGet.irdaServiceName[0]);
					result = TPR_FAIL;
				}
				else
				{
					OutStr(TEXT("WORKED: bound %hs == getsockname %hs\n"),
							&nameBind[0], &addrGet.irdaServiceName[0]);
					result = TPR_PASS;
				}
			}
			else
			{
				OutStr(TEXT("FAILED: getsockname returned error %d\n"), WSAGetLastError());
				result = TPR_FAIL;
			}
		}
	}
	else
	{
		if (!bShouldWork)
		{
			OutStr(TEXT("PASSED: IRBind returned error %d for the name '%hs'\n"), WSAGetLastError(), &nameBind[0]);
			result = TPR_PASS;
		}
		else
		{
			OutStr(TEXT("FAILED: IRBind returned error %d for name '%hs'\n"), error, &nameBind[0]);
			result = TPR_FAIL;
		}
	}

	return result;
}

//
//	Bind a variety of LSAP-SEL values, and make sure they work/fail appropriately.
//
TEST_FUNCTION(BindHardCodedLsapSelTest)
{
	TEST_ENTRY;

	return SocketIteration(BindHardCodedLsapSel, 0, countof(LsapSelValues) - 1);
}

//
//	Bind same hard coded lsap-sel to 2 sockets, and verify the WSAEADDRINUSE gets returned.
//
TEST_FUNCTION(BindSameHardCodedLsapSelTest)
{
	SOCKET sock1, sock2;
	SOCKADDR_IRDA addr1 = {AF_IRDA, 0, 0, 0, 0, "LSAP-SEL127"};
	int result = TPR_SKIP;

	TEST_ENTRY;

	// Test Setup - Create sockets, bind a name to one
	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	sock2 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET && sock2 != INVALID_SOCKET)
	{
		int bindResult = bind(sock1, (struct sockaddr *)&addr1, sizeof(addr1));
		if (bindResult == 0)
		{
			// Test Execution - Bind same name to the second socket
			bindResult = bind(sock2, (struct sockaddr *)&addr1, sizeof(addr1));
			if (bindResult == SOCKET_ERROR)
			{
				// What should the WSAGetLastError() code be? WSAEADDRINUSE
				if (WSAGetLastError() != WSAEADDRINUSE)
					OutStr(TEXT("expected WSAEADDRINUSE from bind, got %d\n"), WSAGetLastError());
				result = TPR_PASS;
			}
			else
			{
				OutStr(TEXT("FAILED: bind() allowed the same hard coded lsap-sel name on a second socket"));
				result = TPR_FAIL;
			}
		}
		else
		{
			OutStr(TEXT("SKIPPED: Unable to bind '%hs' to socket, error=%d\n"), &addr1.irdaServiceName[0], WSAGetLastError());
		}
	}
	else
	{
		OutStr(TEXT("SKIPPED: Unable to create 2 sockets, error=%d\n"), WSAGetLastError());
	}

	// Test Cleanup - Destroy the sockets
	(void)CloseSocket(sock1);
	(void)CloseSocket(sock2);

	return result;
}

// quick hack of a function for base 10 only
unsigned long
strtoul(
	char *str,
	char **pEnd,
	int	  base)
{
	unsigned long result;
	int			  digit;

	result = 0;
	while (1)
	{
		digit = *str - '0';
		if (digit < 0 || digit > 9)
			break;

		result *= base;
		result += digit;

		str++;
	}
	*pEnd = str;

	return result;
}

//
// Get the LSAP-SEL value used by an IR socket.
// Return -1 if no valid LSAP-SEL can be determined.
//
int
GetSockLsapSel(
	SOCKET sock)
{
	unsigned long	ulLsapSel;
	int				result = TPR_SKIP, lsapSel, size;
	char			*pEnd;
	SOCKADDR_IRDA	addrGet;

	lsapSel = -1;

	size = sizeof(addrGet);
	result = getsockname(sock, (struct sockaddr *)&addrGet, &size);
	if (result == 0)
	{
		OutStr(TEXT("name is %hs\n"), &addrGet.irdaServiceName[0]);
		if (memcmp(&addrGet.irdaServiceName[0], "LSAP-SEL", LSAP_SEL_TXT_LEN) == 0)
		{
			ulLsapSel = strtoul(&addrGet.irdaServiceName[LSAP_SEL_TXT_LEN], &pEnd, 10);
			OutStr(TEXT("ulLsapSel is %u\n"), ulLsapSel);

			if ((pEnd > &addrGet.irdaServiceName[LSAP_SEL_TXT_LEN])
			&&  (*pEnd == '\0')
			&&  (ulLsapSel <= MAX_LSAP_SEL))
			{
				lsapSel = ulLsapSel;
			}
			else
			{
				OutStr(TEXT("FAILED: getsockname returned invalid name '%hs'\n"), &addrGet.irdaServiceName[0]);
			}
		}
		else
		{
			OutStr(TEXT("FAILED: getsockname returned invalid name '%hs'\n"), &addrGet.irdaServiceName[0]);
		}
	}
	else
	{
		OutStr(TEXT("FAILED: getsockname failed, error = %d\n"), WSAGetLastError());
	}

	return lsapSel;
}

// 0 and 3 are reserved, so the OS will only allocate 1,2,4-127 when a bind of a "" irdaServiceName is done

// #define MAX_USABLE_LSAP_SEL	125

#define MAX_USABLE_LSAP_SEL	126

// maximum number of sockets CE can handle in one time
#define MAX_SOCKET 70 

//
//	Bind null names to sockets, which causues the OS to assign a random LSAP-SEL.
//

TEST_FUNCTION(BindRandomLsapSelTest)
{
	int				result = TPR_SKIP, lsapSel, error, i, freeLsapSel;
	BOOLEAN			vbUsed[MAX_LSAP_SEL+2];
	SOCKET			vSocket[MAX_LSAP_SEL+2], sFixed;

	TEST_ENTRY;

	freeLsapSel = MAX_LSAP_SEL - MIN_LSAP_SEL + 1;
	for (i = 0; i < countof(vbUsed); i++)
	{
		vbUsed[i] = FALSE;
		vSocket[i] = INVALID_SOCKET;
	}

	vbUsed[IAS_LSAP_SEL] = TRUE;
	freeLsapSel--;

	vbUsed[IAS_QUERY_LSAP_SEL] = TRUE;
	freeLsapSel--;

	// Allocate one fixed LSAP-SEL to make sure that the OS skips it
	sFixed = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sFixed == INVALID_SOCKET)
	{
		OutStr(TEXT("Socket create failed\n"));
		return result;
	}
	IRBind(sFixed, "LSAP-SEL42", &result, &error);
	if (result != 0)
	{
		OutStr(TEXT("Bind of LSAP-SEL42 failed\n"));
		CloseSocket(sFixed);
		return result;
	}
	listen(sFixed, 1);
	vbUsed[42] = TRUE;
	freeLsapSel--;

	// We deliberately try to allocate one more than is possible to
	// ensure that the OS fails gracefully when all are in use.

	for (i = 0; i < freeLsapSel + 1; i++)
	{
		if (i > MAX_SOCKET)
			break;
	
		vSocket[i] = socket(AF_IRDA, SOCK_STREAM, 0);
		if (vSocket[i] == INVALID_SOCKET)
		{
			OutStr(TEXT("FAILED: Unable to allocate %d sockets\n"), i + 1);
			result = TPR_FAIL;
			break;
		}

		// Note that this should fail on the last iteration,
		// since all 127 available will be in use.

		IRBind(vSocket[i], "", &result, &error);
		if (result != 0)
		{
			if (i < MAX_USABLE_LSAP_SEL - 1)
			{
				OutStr(TEXT("FAILED: Bound %d names, but %d should be possible\n"), i + 1, MAX_USABLE_LSAP_SEL - 1);
				result = TPR_FAIL;
			}
			break;
		}

		// The OS should not have allowed this
		if (i == MAX_USABLE_LSAP_SEL - 1)
		{
			OutStr(TEXT("FAILED: Bound %d names, but only %d should be possible\n"), i + 1, MAX_USABLE_LSAP_SEL - 1);
			i++;
			result = TPR_FAIL;
			break;
		}

		lsapSel = GetSockLsapSel(vSocket[i]);
		if (lsapSel == -1)
		{
			result = TPR_FAIL;
			break;
		}
		if (vbUsed[lsapSel])
		{
			OutStr(TEXT("FAILED: lsapSel %d is already in use\n"), lsapSel);
			result = TPR_FAIL;
			break;
		}
	}

	if ((i == freeLsapSel) || (i == MAX_SOCKET + 1))
	{
		result = TPR_PASS;
	}

	// Test cleanup - close the sockets
	CloseSocket(sFixed);
	for (i = 0; i < countof(vbUsed); i++)
	{
		if (i > MAX_SOCKET)
			break;

		CloseSocket(vSocket[i]);
	}

	return result;
}

#ifdef UNDER_CE
#define	COM_NAME_FORMAT	"COM%d:"
#else
#define	COM_NAME_FORMAT	"COM%d"
#endif

//
// Determine if the specified COM port can be opened.
// Return TRUE if it can, FALSE otherwise.
//
extern BOOL
COMPortAvailable(
	int portNum,
	int *pError)
{
    HANDLE			hCommPort;

	hCommPort = COMPortOpen(portNum, pError);

    if (hCommPort == INVALID_HANDLE_VALUE)
		return FALSE;

	CloseHandle(hCommPort);
	return TRUE;
}

//
//	COM Port Usage Test
//  When no IR sockets are in use, the SIR driver should not be using
//  any COM ports so we should be able to open them.
//  When an IR socket exists, opening any COM port that is attached
//  to an SIR miniport should fail.
//

TEST_FUNCTION(COMPortUsageTest)
{
	SOCKET  sock1;
	int		result = TPR_SKIP;
	int		error;
	DWORD	portNum;

	TEST_ENTRY;

	portNum = GetSIRCOMPort();
	if (portNum == -1)
	{
		OutStr(TEXT("SKIPPED: Unable to determine Serial IR port from registry\n"));
		return result;
	}

	// Try to open the COM port with no IR sockets in use

	for (int attempt = 1; ; attempt++)
	{
			if (COMPortAvailable(portNum, &error))
			break;

		if (attempt > 2)
		{
			OutStr(TEXT("FAILED: Unable to open COM%d with no IR sockets in use, error=%d\n"), portNum, error);
			return TPR_FAIL;
		}

		// Allow more chances after waiting for any pending close to complete.

		Sleep(5000);
	}

	// Try to open the COM port with an IR socket in use

	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 == INVALID_SOCKET)
		return result;

	if (COMPortAvailable(portNum, &error))
	{
		OutStr(TEXT("FAILED: Able to open COM%d with IR sockets in use\n"), portNum);
		result = TPR_FAIL;
	}

	CloseSocket(sock1);

	// The close of the COM port is asynchronous (that is, when closesocket
	// returns the COM port may still be open), so allow some time
	// for the COM port to get closed.

	Sleep(3000);

	// Retry opening the COM port with no IR sockets in use

	if (!COMPortAvailable(portNum, &error))
	{
		OutStr(TEXT("FAILED: Unable to open COM%d with no IR sockets in use (after closesocket), error=%d\n"), portNum, error);
		result = TPR_FAIL;
	}

	if (result == TPR_SKIP)
		result = TPR_PASS;

	return result;
}

//
//	IrSIR open fail test.
//  Try to create an IR socket while the Serial IR COM port is open.
//  The socket create should fail, and the error should be WSAEBADF.
//  

TEST_FUNCTION(IrSIROpenFailTest)
{
	SOCKET  sock1;
	int		result = TPR_SKIP;
	int		error, retries;
	DWORD	portNum;
	HANDLE	hCommPort;

	TEST_ENTRY;

	// Test setup - open the COM port so that IrSIR open will fail

	portNum = GetSIRCOMPort();
	if (portNum == -1)
	{
		OutStr(TEXT("SKIPPED: Unable to determine Serial IR port from registry\n"));
		return result;
	}

	for (retries = 0; ; retries++)
	{
		hCommPort = COMPortOpen(portNum, &error);

		if (hCommPort != INVALID_HANDLE_VALUE)
			break;

		if (retries >= 2)
		{
			OutStr(TEXT("SKIPPED: Unable open Serial IR port, error=%d\n"), error);
			return result;
		}
		Sleep(5000);
	}

	// Test execution - try to create an IR socket, check the error code

	sock1 = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock1 != INVALID_SOCKET)
	{
		OutStr(TEXT("FAILED: Able to create IR socket while COM port open\n"));
		closesocket(sock1);
		result = TPR_FAIL;
	}
	else
	{
		error = WSAGetLastError();
		if (error != WSAEBADF)
		{
			OutStr(TEXT("FAILED: Error code creating socket is %d, expected WSAEBADF\n"), error);
			result = TPR_FAIL;
		}
		else
		{
			//OutStr(TEXT("PASSED: Error code creating socket is %d == WSAEBADF\n"), error);
			result = TPR_PASS;
		}
	}

	// Test cleanup - close the COM port

	CloseHandle(hCommPort);

	return result;
}