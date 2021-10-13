//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <windows.h>
#include <ws2bth.h>
#include <stdio.h>
#include <malloc.h>
#include <tux.h>
#include "util.h"

//
//TestId        : 1.1
//Title         : Socket Variations Test
//Description   : Test for socket() call with various legal and illegal parameters
//
TESTPROCAPI a1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0;//Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	bool bTestFail = false;

	TEST_ENTRY;

	OutStr(TEXT("Socket() call Variations Test\n"));

	//Variation 1 : Legal parameter Test
	OutStr(TEXT("Variation 1: Legal Paramater Variation\n"));

	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
		retcode =WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		bTestFail = true;
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 1: [Test Passed]\n"));
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 2 : SOCK_DGRAM : Illegal parameter Test
	OutStr(TEXT("Variation 2: SOCK_DGRAM: Illegal Paramater Variation\n"));
	sock = socket(AF_BTH, SOCK_DGRAM, BTHPROTO_RFCOMM);

	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode =WSAGetLastError();
	    OutStr(TEXT("Failed\n"));
	    OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 2: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 2: [Test Failed]\n"));
 		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 3 : AF_INET:SOCK_STREAM : Illegal parameter Test
	OutStr(TEXT("Variation 3: AF_INET: SOCK_STREAM : Illegal Paramater Variation\n"));
	sock = socket(AF_INET, SOCK_STREAM, BTHPROTO_RFCOMM);
	
	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode =WSAGetLastError();
	    OutStr(TEXT("Failed\n"));
	    OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 3: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 3: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

    //Variation 4 : AF_INET:SOCK_DGRAM : Illegal parameter Test
	OutStr(TEXT("Variation 4: AF_INET:SOCK_DGRAM : Illegal Paramater Variation\n"));
	sock = socket(AF_INET, SOCK_DGRAM, BTHPROTO_RFCOMM);

	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("Failed\n"));
	    OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 4: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 4: [Test Failed]\n"));
		bTestFail = true;
	}
	
	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

    //Variation 5 : Protocol field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 5: Protocol Field NULL : Illegal Paramater Variation\n"));
	sock = socket(AF_BTH, SOCK_STREAM, 0);

	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode =WSAGetLastError();
	    OutStr(TEXT("Failed\n"));
	    OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 5: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 5: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 6 : Type field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 6: Type Field NULL : Illegal Paramater Variation\n"));
	sock = socket(AF_BTH, 0, BTHPROTO_RFCOMM);

	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode =WSAGetLastError();
	    OutStr(TEXT("Failed\n"));
	    OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 6: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 6: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

    //Variation 7 : Family field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 7:  Family Field NULL : Illegal Paramater Variation\n"));
	sock = socket(0, SOCK_STREAM, BTHPROTO_RFCOMM);

	OutStr(TEXT("Socket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 7: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...Socket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 7: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

    //Check if the test passed
	if (!bTestFail)
	{
		return TPR_PASS;
	} 
	else
	{
		return TPR_FAIL;
	}


return SUCCESS;
}

//
//TestId        : 1.2
//Title         : WSASocket Variations Test
//Description   : Test for socket() call with various legal and illegal parameters
//
TESTPROCAPI a1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	bool bTestFail = false;
	SOCKET sock = 0;//Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;

	TEST_ENTRY;

	OutStr(TEXT("WSASocket() call Variations Test\n"));

	//Variation 1 : Legal parameter Test
	OutStr(TEXT("Variation 1: Legal Paramater Variation\n"));
	sock = WSASocket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSASocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"), retcode);					
		OutStr(TEXT("... Variation 1: [Test Failed]\n"));
		bTestFail = true;
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
	    OutStr(TEXT("... Variation 1: [Test Passed]\n"));
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 2 : SOCK_DGRAM : Illegal parameter Test
	OutStr(TEXT("Variation 2: SOCK_DGRAM : Illegal parameter Test\n"));
	sock = WSASocket(AF_BTH, SOCK_DGRAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 2: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 2: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 3 : AF_INET:SOCK_STREAM : Illegal parameter Test
	OutStr(TEXT("Variation 3: AF_INET: SOCK_STREAM : Illegal Paramater Variation\n"));
	sock = WSASocket(AF_INET, SOCK_STREAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 3: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 3: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 4 : AF_INET:SOCK_DGRAM : Illegal parameter Test
	OutStr(TEXT("Variation 4: AF_INET:SOCK_DGRAM : Illegal Paramater Variation\n"));
	sock = WSASocket(AF_INET, SOCK_DGRAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 4: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 4: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 5 : Protocol field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 5: Protocol Field NULL : Illegal Paramater Variation\n"));
	sock = WSASocket(AF_BTH, SOCK_STREAM, 0, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 5: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 5: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 6 : Type field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 6: Type Field NULL : Illegal Paramater Variation\n"));
	sock = WSASocket(AF_BTH, 0, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 6: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 6: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

	//Variation 7 : Family field NULL : Illegal parameter Test
	OutStr(TEXT("Variation 7:  Family Field NULL : Illegal Paramater Variation\n"));
	sock = WSASocket(0, SOCK_STREAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);

	OutStr(TEXT("WSASocket() call ...\n"));
	if (sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
		OutStr(TEXT("Failed\n"));
		OutStr(TEXT("...WSAsocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);					
		OutStr(TEXT("... Variation 7: [Test Passed]\n"));
	}
	else
	{
		OutStr(TEXT("Success\n"));
		OutStr(TEXT("...WSASocket() Call Succeeded : Sock = [%d]\n"), sock);
		OutStr(TEXT("... Variation 7: [Test Failed]\n"));
		bTestFail = true;
	}

	if (sock != 0)
	{
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	}

    //Check if the test passed
	if (!bTestFail)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 1.3
//Title         : Bind Variations Test
//Description   : Test for bind() call with various legal and illegal parameters
//
TESTPROCAPI a1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0;//Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	SOCKADDR_BTH local;
	bool bTestFail = false;

	TEST_ENTRY;

	OutStr(TEXT("Bind() call Variations Test\n"));

	//Variation 1 : Legal Paramater test : Explicit channel specification
	OutStr(TEXT("Variation 1:  Legal parameter test : Explicit Channel specification\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = 20;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 1: [Test Failed]\n"));
 			bTestFail = true;
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 1: [Test Passed]\n"));
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 1\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

	//Variation 2 : Bind to channel 0
	OutStr(TEXT("Variation 2:  Bind to channel 0\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = 0;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 2: [Test Failed]\n"));
 			bTestFail = true;
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 2: [Test Passed]\n"));
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 2\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

	//Variation 3 : Bind to channel -1
	OutStr(TEXT("Variation 3:  Legal parameter test : Implicit Channel specification\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = -1;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 3: [Test Passed]\n"));
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 3: [Test Failed]\n"));
 			bTestFail = true;
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 3\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

	//Variation 4 : Bind to an invalid channel - upper bound
	OutStr(TEXT("Variation 4:  Bind to invalid channel - upper bound\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = 31;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 4: [Test Passed]\n"));
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 4: [Test Failed]\n"));
 			bTestFail = true;
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 4\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

	//Variation 5 : Bind to odd value channel 
	OutStr(TEXT("Variation 5:  Bind to odd value channel\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = 9;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 5: [Test Failed]\n"));
 			bTestFail = true;
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 5: [Test Passed]\n"));
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 5\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

	//Variation 6 : Bind to even value channel 
	OutStr(TEXT("Variation 5:  Bind to even value channel\n"));
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	local.port = 30;

	if (sock != INVALID_SOCKET)
	{ 
		//Socket call passed
		local.addressFamily = AF_BTH;

		OutStr(TEXT("bind() call...\n"));

		retcode = bind(sock, (struct sockaddr *)&local,  sizeof(SOCKADDR_BTH));
		if (retcode == SOCKET_ERROR)
		{
			//Retrieve the error code immediately
			retcode = WSAGetLastError();
			OutStr(TEXT("Failed\n"));
			OutStr(TEXT("...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), local.port);
			OutStr(TEXT("WSAGetLastError  Code = %d\n"),retcode);
			OutStr(TEXT("Variation 6: [Test Failed]\n"));
 			bTestFail = true;
		}
		else 
		{
			OutStr(TEXT("Success\n"));
	   		OutStr(TEXT("...bind() Call Succeeded to port: %d   retcode = [%d]\n"),local.port,retcode);
			OutStr(TEXT("Variation 6: [Test Passed]\n"));
		}	

		//Close the socket
		shutdown(sock, 0);
		closesocket(sock);
		sock = 0;
	} 
	else 
	{ 
		//Socket call failed 
		retcode = WSAGetLastError();
		OutStr(TEXT("socket call failed ... Error code : [%d]...Skipping Varation 6\n"), retcode);

		//Reinitialize sock to 0
		sock = 0;
	}

    //Check if the test passed
	if (!bTestFail)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL;
	}
}

