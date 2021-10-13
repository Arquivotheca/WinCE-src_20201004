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

#undef AF_IRDA
#include <af_irda.h>

#include <irsup.h>
#include <msg.h>
#include <request.h>
#include "irsrv.h"

//******************************************************************************

//
//	This program receives requests to perform IR operations from a client,
//  executes them, and returns the results.
//
int
main(
	 int   argc, 
	 TCHAR *argv[])
{
	SOCKET			sckServer;
	Connection      connClient;
	int				 type = AF_IRDA;
	char			*szServerName = IRTEST_SERVERNAME;
	char			 bufServerName[64];

	if (IRSockInit() != 0)
		return -1;

	if (argc == 2)
	{
		type = AF_INET;
		szServerName = &bufServerName[0];

		const TCHAR *psrc = argv[1];
		char  *pdst = szServerName;

		while (1)
		{
			*pdst = (char)*psrc;
			if (*pdst == '\0')
				 break;
			pdst++, psrc++;
		}
	}

	if (type == AF_IRDA)
		OutStr(TEXT("Creating AF_IRDA endpoint\n"));
	else if (type == AF_INET)
		OutStr(TEXT("Creating AF_INET endpoint\n"));
	else
		OutStr(TEXT("Creating %d??? endpoint\n"), type);

	// Create a socket to wait for a connection from the test engine
	if (!ConnectionCreateEndpoint(type, szServerName, &sckServer))
	{
		OutStr(TEXT("Unable to create server socket\n"));
		return -1;
	}

	while (1)
	{
		OutStr(TEXT("IRSRV waiting for connect...\n"));

		// Wait for the test engine to connect
		if (!ConnectionAccept(&sckServer, &connClient))
		{
			OutStr(TEXT("Unable to accept connection\n"));
			return -1;
		}
		OutStr(TEXT("IRSRV  connected...\n"));

		// Process requests from the test engine
		while (1)
		{
			OutStr(TEXT("IRSRV waiting for request\n"));
			if (ReceiveRequest(&connClient) <= 0)
				break;
		}
		ConnectionClose(&connClient);
	}

	// Exit
	return 0;
}