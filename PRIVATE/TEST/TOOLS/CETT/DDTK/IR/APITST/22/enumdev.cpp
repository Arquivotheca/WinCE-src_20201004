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
#include "shellproc.h"
#include "enumdev.h"

#undef AF_IRDA
#include <af_irda.h>

#include <irsrv.h>
#include <msg.h>
#include <irsup.h>
#include <request.h>


//
//	Test that enum devices returns the same list while
//  connected as while disconnected.
//
TEST_FUNCTION(EnumDeviceConnectedTest)
{
	Connection	connServer;
	int			result;
	DEVICELIST *devlDisconnected, *devlConnected;

	TEST_ENTRY;

	// While disconnected, get a list of visible devices

	result = TPR_PASS;

	for (int iteration = 3; iteration-- && result == TPR_PASS; )
	{
		devlDisconnected = IREnumDeviceList(10);
		if (devlDisconnected == NULL)
		{
			OutStr(TEXT("SKIPPED: Unable to get disconnected device list\n"));
			result = TPR_SKIP;
		}
		else
		{
			// Get into connected state
			if (ConnectionOpen(g_af, g_szServerName, &connServer))
			{
				// Get a device list in the connected state
				devlConnected = IREnumDeviceList(10);
				if (devlConnected == NULL)
				{
					OutStr(TEXT("SKIPPED: Unable to get connected device list\n"));
					result = TPR_SKIP;
				}
				else
				{

					// Execute the test - compare the two device lists
					if (devlDisconnected->numDevice != devlConnected->numDevice)
					{


					#if 0 
						OutStr(TEXT("FAILED: Disconnected found %d devices, Connected found %d\n"),
							devlDisconnected->numDevice, devlConnected->numDevice);
						result = TPR_FAIL;
						IRPrintDeviceList(devlDisconnected);
						IRPrintDeviceList(devlConnected);
					#endif 

					}
					else
					{
						for (unsigned int i = 0; i < devlDisconnected->numDevice; i++)
						{
							if (memcmp(&devlDisconnected->Device[i].irdaDeviceID[0],
									   &devlConnected->Device[i].irdaDeviceID[0],
									   sizeof(devlConnected->Device[0].irdaDeviceID)) != 0)
							{
								OutStr(TEXT("FAILED: Device lists differ at %d\n"), i);
								result = TPR_FAIL;
								IRPrintDeviceList(devlDisconnected);
								IRPrintDeviceList(devlConnected);
							}
						}
					}
				}
				ConnectionClose(&connServer);
			}
			free(devlDisconnected);
		}
		Sleep(1000);
	}
	return result;
}

//
//	Test that after a remote discovery operation followed by a connect, a local
//  ENUM_DEVICES returns the remote address.
//
//  This test is intended to test the cache of deviceIDs maintained by IrLAP.
//  Unfortunately, there is no way to clear this cache programmatically.
//  Thus, this test can be run at most once on a clean system, after that the
//  cache will (if the test passes) have been filled making subsequent runs useless.
//
TEST_FUNCTION(DiscoveryWhileConnectedTest)
{
	Connection	connServer;
	int			result = TPR_SKIP, remoteResult, remoteError, localResult, localError;
	BYTE		deviceID[4];

	TEST_ENTRY;

	if (g_af == AF_IRDA)
	{
		OutStr(TEXT("SKIPPED: This test cannot be run when the control connection uses an IR link\n"));
		return TPR_SKIP;
	}

	if (ConnectionOpen(g_af, g_szServerName, &connServer))
	{
		// Have the remote do a discovery sequence
		//if (SendDiscoverRequest(&connServer, &remoteResult, &remoteError))
		{
			// Create an IR socket
			SOCKET sockAccept = socket(AF_IRDA, SOCK_STREAM, 0);
			if (sockAccept != INVALID_SOCKET)
			{
#define TEST_NAME "dscvtest"
				// Allow time for Irsir to start
				Sleep(100);
				IRBind(sockAccept, TEST_NAME, &localResult, &localError);
				if (localResult == 0)
				{
					// Have the remote connect to our socket
					if (SendConnectRequest(&connServer, TEST_NAME, &remoteResult, &remoteError) &&
						remoteResult == 0)
					{
						// Accept the connection
						OutStr(TEXT("accepting connection...\n"));
						SOCKET sockData = accept(sockAccept, 0, 0);
						OutStr(TEXT("accepting connection DONE\n"));

						if (sockData != INVALID_SOCKET)
						{
							// Do a discovery in the connected state

							// Verify that the device ID the remote is found,
							// and that this continues for any number of discoveries.

							for (int i = 10; result != TPR_FAIL && i--; )
							{
								if (IRFindNewestDevice(&deviceID[0], 1) == -1)
								{
									OutStr(TEXT("FAILED: Found no devices while connected.\n"));
									// Found no devices
									result = TPR_FAIL;
								}
								else
								{
									// Found 1 or more devices
									OutStr(TEXT("WORKED: Found %02X%02X%02X%02X while connected.\n"),
											deviceID[0], deviceID[1], deviceID[2], deviceID[3]);
								}
							}
							if (result != TPR_FAIL)
								result = TPR_PASS;

							// Close the data socket
							closesocket(sockData);
						}
						else
						{
							OutStr(TEXT("SKIPPED: Unabled to accept connection\n"));
						}

						// Have the remote close its connection
						(void)SendUnSetRequest(&connServer, &remoteResult, &remoteError);
					}
					else
					{
						OutStr(TEXT("SKIPPED: Unabled to connect\n"));
					}
				}
				else
				{
					OutStr(TEXT("SKIPPED: Unabled to bind\n"));
				}
				// Close the accept socket
				closesocket(sockAccept);
			}
		}
		ConnectionClose(&connServer);
	}

	return result;
}
