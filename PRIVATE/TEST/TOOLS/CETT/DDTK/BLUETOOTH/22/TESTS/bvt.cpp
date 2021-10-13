//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <winbase.h>
#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <ws2bth.h>
#include <tux.h>
#include "util.h"

#ifndef UNDER_CE
#include <time.h>
#else
extern time_t time( time_t *timer );   // CE does not have time.h
#endif

time_t timer;

// Test Functions
int DataChunkTst(int tot_bytes, int chunks, int port);
int ListenTst(int backlog);

int PORT1 = 21;
int PORT2 = 22;
int PORT3 = 23;
int PORT4 = 24;

/*----------------------------------------   Connection Mgmt Tests ----------------------------------------*/

//
//TestId        : 1.1
//Title         : Open and Close Socket Test
//Description   : Exercise socket() and closesocket() call
//
TESTPROCAPI t1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0;//Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	bool TstResult;

	TEST_ENTRY;

    //Initialize TstResult for each Test
    TstResult = true; 

	retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}
	
	retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//At this stage the test passed. Safe to turn TstResult to true.
	if (TstResult)
	{
		return TPR_PASS; //Test passed
	} 

FailExit:
	//Handling the failure case.
	return TPR_FAIL;
}

//
//TestId        : 1.2
//Title         : Bind Call Test
//Description   : Bind a socket to a valid port and make sure closesocket() succeeds
//
TESTPROCAPI t1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	bool TstResult;

	TEST_ENTRY;

    //Initialize TstResult for each Test
    TstResult = true; 

	retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	retcode = Bind(&sock, PORT1);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

		
	retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}
	
	if (TstResult)
	{
		return TPR_PASS; //Test passed
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
		Closesocket(&sock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.3
//Title         : Bind 2 Times Test
//Description   : Bind a socket to a valid port and bind again to make sure proper error is
//                returned back
//
TESTPROCAPI t1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	bool TstResult;

	TEST_ENTRY;

    //Initialize TstResult for each Test
    TstResult = true; 

	retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	retcode = Bind(&sock,PORT2);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//This call should fail - binding to an already bound channel
	retcode = Bind(&sock,PORT2);
    if (retcode == SUCCESS)
	{	   		
		TstResult = false;
		goto FailExit;        
	}

	retcode = Closesocket(&sock);
    if (retcode == FAIL)
	{	   		
		TstResult = false;
		goto FailExit;       	   
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;  
}

//
//TestId        : 1.4
//Title         : Listen Call Test
//Description   : Bind a socket to a valid port and listen with backlog = 0
//
TESTPROCAPI t1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Call ListenTst with backlog value = 0;
	//The ListenTst() function takes care of logging results - so
	//we dont have to take care of these here
    return ListenTst(0);  
}

//
//TestId        : 1.5
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog = 1
//
TESTPROCAPI t1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Call ListenTst with backlog value = 1;
	//The ListenTst() function takes care of logging results - so
	//we dont have to take care of these here
    return ListenTst(1);
}

//
//TestId        : 1.6
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog < 8
//
TESTPROCAPI t1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	 //Call ListenTst with backlog value < 8;
	 //The ListenTst() function takes care of logging results - so
	 //we dont have to take care of these here
    return ListenTst(5);
}

//
//TestId        : 1.7
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog = 8
//
TESTPROCAPI t1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Call ListenTst with backlog value = 8;
	//The ListenTst() function takes care of logging results - so
	//we dont have to take care of these here
	return ListenTst(8);
}

//
//TestId        : 1.8
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog = SOMAXCONN
//
TESTPROCAPI t1_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Call ListenTst with backlog value = SOMAXCONN;
	//The ListenTst() function takes care of logging results - so
	//we dont have to take care of these here
    return ListenTst(SOMAXCONN);
}

//
//TestId        : 1.9
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog = SOMAXCONN + 1
//
TESTPROCAPI t1_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Call ListenTst with backlog value = SOMAXCONN + 1;
	//The ListenTst() function takes care of logging results - so
	//we dont have to take care of these here
    return ListenTst((int) ((DWORD)SOMAXCONN + (DWORD)1));
}

//
//TestId        : 1.10
//Title         : Accept Call Test (CLIENT connects to SERVER)
//Description   : Accept a connection from the peer node
//                
//
TESTPROCAPI t1_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		// Wait for the client to disconnect first
		Sleep(2000);
		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}


//
//TestId        : 1.11
//Title         : Connect 2 Times to same RFCOMM channel Test
//Description   : RFCOMM does not allow 2 sessions simultaneously to be connected
//                over the *SAME* channel between *SAME* peers.
//				  However BTHTDI does
//
TESTPROCAPI t1_11(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		// Wait for the client to disconnect first	
		Sleep(2000);
			
		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
  }
  else 
  {
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	  	
		retcode = Socket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Now this call should fail for RFCOMM - if it does not - then fails spec - you will run into trouble
		retcode = Connect(&csock, &server,PORT4);
		if (retcode == SUCCESS)
		{
			TstResult = false;
			goto FailExit;
		}

		OutStr(TEXT(">> 2nd connect() call fails as Expected...\n"));

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.12
//Title         : Accept Call Test (SERVER connects to CLIENT)
//Description   : Now The Test Master tried to connect to the Test Slave
//                
//
TESTPROCAPI t1_12(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is SLAVE
	if(!g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		// Wait for the client to disconnect first	
		Sleep(2000);

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
  }
  else 
  {
		//Mode is Master

	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.13
//Title         : Multiple conections Test(non - simultaneous) 
//Description   : Accept connections from same peer over same RFCOMM channel after
//                disconneting channel.
//
TESTPROCAPI t1_13(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int i = 1;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT2);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		while(i <= 2)
		{
			retcode = Accept(&sock, &csock, &client);
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}

			retcode = Closesocket(&csock);
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}

			++i;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
  }
  else 
  { 
		// The Slave Node processing
        while(i <= 2)
		{
	  		retcode = Socket(&sock);
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}			
			
			Sleep(500);

			retcode = Connect(&sock, &server,PORT2);
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}
			retcode = Closesocket(&sock);
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}
			++i;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.14
//Title         : Multiple conections Test(Simultaneous) 
//Description   : Accept simultaneous connections from same peer over *DIFFERENT* RFCOMM channel without
//                disconneting channel.
//
TESTPROCAPI t1_14(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock[2]  = { 0, 0 };//Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock[2] = { 0, 0 };//Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server[2];
	SOCKADDR_BTH client[2];
	int retcode;
	int i;
	bool TstResult;

	TEST_ENTRY;

	//Initialize TstResult for each Test
	TstResult = true; 

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	if(g_bIsServer)
	{
		for (i=0; i<2; i++)
		{
			retcode = Socket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}

			retcode = Bind(&sock[i],PORT3+(2*i));
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}

			//Pass backlog
			retcode = Listen(&sock[i], 5);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}

		for (i=0; i<2; i++)
		{
			retcode = Accept(&sock[i], &csock[i], &client[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}
		
		for (i=0; i<2; i++)
		{
			retcode = Closesocket(&csock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
			retcode = Closesocket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}
	}
	else 
	{ 
		//Perform Slave node processing
		for (i=0; i<2; i++)
		{
			retcode = Socket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}

		//Sleep for 0.5 secs.

		Sleep(500);

        for (i=0; i<2; i++)
        {
            retcode = Connect(&sock[i], &server[i],PORT3+(2*i));
            if (retcode == FAIL)
            {
 				TstResult = false;
				goto FailExit;
            }
        }

        for (i=0; i<2; i++)
        {
            retcode = Closesocket(&sock[i]);
            if (retcode == FAIL)
            {
 				TstResult = false;
				goto FailExit;
            }
        }
	}

	//Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    for (i=0; i<2; i++)
    {
        if (sock[i])
        {
            Closesocket(&sock[i]);
        }
        if (csock[i])
        {
            Closesocket(&csock[i]);
        }
    }

	return TPR_FAIL;
}

//
//TestId        : 1.15
//Title         : Disconnect Order Test 
//Description   : Listener listens on a connection, accepts a connection and waits, the connector 
//                disconnets the channel, connects on another connection and waits, while the listener
//                disconnects the 2nd connection first.
TESTPROCAPI t1_15(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock[2]  = { 0, 0 };//Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock[2] = { 0, 0 };//Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server[2];
	SOCKADDR_BTH client[2];
	int retcode;
	int	i;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	if(g_bIsServer)
	{
		for (i=0; i<2; i++)
		{	
			retcode = Socket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}

			retcode = Bind(&sock[i],PORT3+(2*i));
			if (retcode == FAIL)
			{
				TstResult = false;
				goto FailExit;
			}

			//Pass backlog
			retcode = Listen(&sock[i], 5);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}

		//Accept  connections without disconnecting 
		for (i=0; i<2; i++)
		{
			retcode = Accept(&sock[i], &csock[i], &client[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}

		OutStr(TEXT("Sleep for 2 secs ...\n"));
		Sleep(2000);
		
		//Close the 2nd connection first and the first connection next
		for (i=1; i>= 0; --i)
		{
			retcode = Closesocket(&csock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
			retcode = Closesocket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}
	}
	else 
	{ 
		//Perform Slave node processing
		for (i=0; i<2; i++)
		{
			retcode = Socket(&sock[i]);
			if (retcode == FAIL)
			{
 				TstResult = false;
				goto FailExit;
			}
		}

		//Sleep for 0.5 secs.
		Sleep(500);

        for (i=0; i<2; i++)
        {
            retcode = Connect(&sock[i], &server[i],PORT3+(2*i));
            if (retcode == FAIL)
            {
 				TstResult = false;
				goto FailExit;
            }

			//On the first connect, issue a disconnect immediately after connect, while listener waits
			if (i == 0)
			{
				retcode = Closesocket(&sock[i]);
				if (retcode == FAIL)
				{
 					TstResult = false;
					goto FailExit;
				}
			}
        }

		//Sleep for 5 secs.
		OutStr(TEXT("Sleeping for 5 secs...\n"));
		Sleep(1000 * 5);

		//Now close the 2nd connection
        retcode = Closesocket(&sock[1]);
        if (retcode == FAIL)
        {
 			TstResult = false;
			goto FailExit;
        }
	}

	//Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    for (i=0; i<2; i++)
    {
        if (sock[i])
        {
            Closesocket(&sock[i]);
        }
        if (csock[i])
        {
            Closesocket(&csock[i]);
        }
    }

	return TPR_FAIL;
}

/*----------------------------------------   Data Transfer Tests ----------------------------------------*/

//
//TestId        : 2.1
//Title         : Data Xfer Test - 10 bytes with 1 byte chunk
//Description   : Send and Receive data of 10 bytes in 1 byte chunk
//                
//
TESTPROCAPI t2_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(10,1, PORT1);
}

//
//TestId        : 2.2
//Title         : Data Xfer Test - 100 bytes with 10 byte chunk
//Description   : Send and Receive data of 100 bytes in 10 byte chunk
//                
//
TESTPROCAPI t2_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(100,10, PORT2);
}

//
//TestId        : 2.3
//Title         : Data Xfer Test - 1000 bytes with 10 byte chunk
//Description   : Send and Receive data of 1000 bytes in 10 byte chunk
//                
//
TESTPROCAPI t2_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(1000,10, PORT4);
}

//
//TestId        : 2.4
//Title         : Data Xfer Test - 10000 bytes with 1000 byte chunk
//Description   : Send and Receive data of 10000 bytes in 1000 byte chunk
//                
//
TESTPROCAPI t2_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(10000,100, PORT1);
}

//
//TestId        : 2.5
//Title         : Data Xfer Test - 30000 bytes with 10000 byte chunk
//Description   : Send and Receive data of 10000 bytes in 10000 byte chunk
//                
//
TESTPROCAPI t2_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(30000,10000, PORT2);
}

//
//TestId        : 2.6
//Title         : Data Xfer Test - 30000 bytes with 30000 byte chunk
//Description   : Send and Receive data of 30000 bytes in 30000 byte chunk
//                
//
TESTPROCAPI t2_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(30000,30000, PORT4);
}

//
//TestId        : 2.7
//Title         : Data Xfer Test - 65535 bytes with 30000 byte chunk
//Description   : Send and Receive data of 65535 bytes in 30000 byte chunk
//                
//
TESTPROCAPI t2_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(65535,30000, PORT1);
}

//
//TestId        : 2.8
//Title         : Data Xfer Test - 65535 bytes with 65535 byte chunk
//Description   : Send and Receive data of 65535 bytes in 65535 byte chunk
//                
//
TESTPROCAPI t2_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(65535, 65535, PORT2);
}

//
//TestId        : 2.9
//Title         : Data Xfer Test - 0 bytes payload Xfer
//Description   : Send and Receive data of 0 bytes payload
//                
//
TESTPROCAPI t2_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//DataChunkTst handles all the logging and takes cares of the test statistics too..
	//So we dont want to do anything here
	return DataChunkTst(0,0, PORT4);
}

//
//TestId        : 2.10
//Title         : Full duplex data transfer test
//Description   : Send and Receive data in both directions on the *SAME* channel
//                
//
TESTPROCAPI t2_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	int tot_bytes = 1000;
	int chunks = 100;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Now Read on the same socket
		retcode = Read(&csock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&sock, tot_bytes, chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

/*----------------------------------------   Connection Termination  Tests ----------------------------------------*/

//
//TestId        : 3.1 
//Title         : Shutdown Call Test (how = SD_RECEIVE)
//Description   : Shutdown with how = SD_RECEIVE
//                
//
TESTPROCAPI t3_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	int tot_bytes = 1000;
	int chunks = 100;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT2);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Close the Receive channel
		retcode = Shutdown(&csock, SD_RECEIVE);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Now this call should fail
		retcode = Read(&csock, 1000);
		if (retcode == FAIL)
		{
			OutStr(TEXT(">> PASSED : recv() call fails after shutdown() on SD_RECEIVE\n"));
			TstResult = true;

		} else {
			OutStr(TEXT(">> FAILED : recv() call succeeds even after shutdown() on SD_RECEIVE\n"));
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave

	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT2);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		Sleep(2000);

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 3.2 
//Title         : Shutdown Call Test (how = SD_SEND)
//Description   : Shutdown with how = SD_SEND
//                
//
TESTPROCAPI t3_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	int tot_bytes = 1000;
	int chunks = 100;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT3);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Close the Receive channel
		retcode = Shutdown(&csock, SD_SEND);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Now this call should fail
		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			OutStr(TEXT(">> PASSED : send() call fails after shutdown() on SD_SEND\n"));
			TstResult = true;
		} else {
			OutStr(TEXT(">> FAILED : send() call succeeds even after shutdown() on SD_SEND\n"));
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT3);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		Sleep(2000);

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;


}

//
//TestId        : 3.3 
//Title         : Shutdown Call Test (how = SD_BOTH)
//Description   : Shutdown with how = SD_BOTH
//                
//
TESTPROCAPI t3_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	int tot_bytes = 1000;
	int chunks = 1000;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Close the Receive channel
		retcode = Shutdown(&csock, SD_BOTH);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Now this call should fail
		retcode = Write(&csock, tot_bytes , chunks);
		if (retcode == FAIL)
		{
			OutStr(TEXT(">> PASSED : send() call fails after shutdown() on SD_BOTH\n"));
			TstResult = true;
		} else {
			OutStr(TEXT(">> FAILED : send() call succeeds even after shutdown() on SD_BOTH\n"));
			TstResult = false;
			goto FailExit;
		}

		//Now this call should fail
		retcode = Read(&csock, tot_bytes);
		if (retcode == FAIL)
		{
			OutStr(TEXT(">> PASSED : read() call fails after shutdown() on SD_BOTH\n"));
			TstResult = true;
		} else {
			OutStr(TEXT(">> FAILED : read() call succeeds even after shutdown() on SD_BOTH\n"));
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
  }
  else 
  {
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Connect(&sock, &server,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		Sleep(500);

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

/*---------------------------------------- Private Test Functions ----------------------------------------*/

//
//Title         : Listen call Test
//Description   : Bind a socket to a valid port and listen with backlog
//
int ListenTst(int backlog)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	bool TstResult;

    //Initialize TstResult for each Test
    TstResult = true; 

	retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	retcode = Bind(&sock,PORT3);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//Pass backlog
	retcode = Listen(&sock, backlog);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}
		
	retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//At this point the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;
}

//
//Title         : Data Chunk Test 
//Description   : Send and receive data of tot_bytes, in chunks on a specified channel
//
int  DataChunkTst(int tot_bytes, int chunks, int port)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,port);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Write(&csock, tot_bytes, chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,port);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

/*----------------------------------------   E  N  D    ----------------------------------------*/
