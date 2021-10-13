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
#include <katoex.h>
#include <malloc.h>
#include <ws2bth.h>
#include "util.h"

// Local Bluetooth device address.
BD_ADDR g_LocalRadioAddr;

// Remote Bluetooth device address.
BD_ADDR g_RemoteRadioAddr;

// Server address.
SOCKADDR_IN g_ServerAddr; 

// Role server/client.
BOOL g_bIsServer;

// Skip connection test.
BOOL g_bNoServer;

int Socket(SOCKET *sock)
{
	int retcode;
                
	*sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	OutStr(TEXT("\tSocket() call ...\n"));

	if (*sock == INVALID_SOCKET )
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...socket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);	

		return FAIL;
	}
	else
	{
	   OutStr(TEXT("\tSuccess\n"));
	   OutStr(TEXT("\t...Socket() Call Succeeded : Sock = [%d]\n"), *sock);

	   return SUCCESS;
	}
}

int WSA_Socket(SOCKET *sock)
{
	int retcode;

	*sock = WSASocket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM, NULL, 0,WSA_FLAG_OVERLAPPED);
    OutStr(TEXT("WSASocket() Call... \n"));

	if (*sock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...WSASocket() Call Failed     Expected : Valid Socket   Got : INVALID_SOCKET\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);	

		return FAIL;
	}
	else
	{
	   OutStr(TEXT("\tSuccess\n"));
	   OutStr(TEXT("\t...WSASocket() Call Succeeded : Sock = [%d]\n"), *sock);

	   return SUCCESS;
	}
}

int Ioctlsocket(SOCKET *sock, long cmd, ULONG arg)
{
	int retcode;

    OutStr(TEXT("\tioctlsocket() call...\n"));
	retcode = ioctlsocket(*sock , cmd, &arg);

	if (retcode  == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...ioctlsocket() Call Failed     Expected : Valid retcode   Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

		return FAIL;
	}
	else
	{
	   OutStr(TEXT("\tSuccess\n"));
	   OutStr(TEXT("\t...ioctlsocket() Call Succeeded : Sock = [%d]\n"), *sock);

	   return SUCCESS;
	}
}

int Closesocket(SOCKET *sock)
{
	int retcode;

	if (*sock > 0)
	{
		OutStr(TEXT("\tclosesocket() call ...\n"));
		shutdown(*sock, 0);
		retcode = closesocket(*sock);
		if (retcode == SOCKET_ERROR)
		{
			retcode = WSAGetLastError();
			OutStr(TEXT("\tFailed\n"));
			OutStr(TEXT("\t...closesocket() Call Failed for sock: [%d] :    Expected : retcode=0   Got : SOCKET_ERROR\n"), *sock);
			OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

			//Re-initialize socket to 0
			*sock = 0;

			return FAIL;
		}
		else 
		{
	        OutStr(TEXT("\tSuccess\n"));
	   		OutStr(TEXT("\t...closesocket() Call Succeeded for sock: [%d] : retcode = [%d]\n"), *sock, retcode);

			//Re-initialize socket to 0
			*sock = 0;

			return SUCCESS;
		}
	} 
	else 
	{
	  return SUCCESS;
	}
}

int Bind(SOCKET *sock,  int port)
{
	int retcode;
	SOCKADDR_BTH local;

    local.addressFamily = AF_BTH;
    local.port = port;
	memset(&local.btAddr, 0, sizeof(local.btAddr));

	OutStr(TEXT("\tbind() call...\n"));

	retcode = bind(*sock, (struct sockaddr *)&local, sizeof(local)); 
	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
		OutStr(TEXT("\t...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), port);
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

		return FAIL;
	}
	else 
	{
		OutStr(TEXT("\tSuccess\n"));
	   	OutStr(TEXT("\t...bind() Call Succeeded to port: %d   retcode = [%d]\n"),port,retcode);

		return SUCCESS;
	}
}

int Listen(SOCKET *sock, int backlog)
{
	int retcode;

	OutStr(TEXT("\tlisten() call...\n"));
	retcode = listen(*sock,backlog);
	if (retcode == SOCKET_ERROR)
	{
		retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...listen() Call Failed w/ backlog = %d    Expected : retcode=0   Got : SOCKET_ERROR\n"), backlog);
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
   }
   else 
   {
        OutStr(TEXT("\tSuccess\n"));
   		OutStr(TEXT("\t...listen() Call Succeeded w/ backlog = %u : retcode = [%d]\n"), backlog, retcode);

		return SUCCESS;
   }
}

int Accept(SOCKET *sock, SOCKET *csock, SOCKADDR_BTH *client)
{
	int addr_len;
	int retcode;

	addr_len = sizeof(SOCKADDR_BTH);

	OutStr(TEXT("\taccept() call ...\n"));

	*csock = accept(*sock, (struct sockaddr *) client, &addr_len);

	if (*csock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...accept() Call Failed Expected : retcode = Valid sock  Got : INVALID_SOCKET\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
   }
   else 
   {
        OutStr(TEXT("\tSuccess\n"));
   		OutStr(TEXT("\t...accept() Call Succeeded on sock [%d]\n"), *csock);

		return SUCCESS;
   }
}

int WSA_Accept(SOCKET *sock, SOCKET *csock, SOCKADDR_BTH *client)
{
	int addr_len;
	int retcode;

	addr_len = sizeof(SOCKADDR_BTH);

	OutStr(TEXT("accept() call ...\n"));

	*csock = WSAAccept(*sock, (struct sockaddr *) client, &addr_len, NULL, 0);

	if (*csock == INVALID_SOCKET)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	    OutStr(TEXT("\t...accept() Call Failed Expected : retcode = Valid sock  Got : INVALID_SOCKET\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

		return FAIL;
   }
   else 
   {
        OutStr(TEXT("\tSuccess\n"));
   		OutStr(TEXT("\t...accept() Call Succeeded retcode = [%d]\n"), *csock);

		return SUCCESS;
   }
}

int Connect(SOCKET *sock, SOCKADDR_BTH *server, int port)
{
	int retcode;
	SOCKADDR_BTH dest;
	int len = sizeof(SOCKADDR_BTH);

	dest.addressFamily = AF_BTH;
	memcpy((void *)&(dest.btAddr), (void *)&g_RemoteRadioAddr, sizeof(g_RemoteRadioAddr));
	dest.port = port;

	OutStr(TEXT("\tconnect() call...  on Port [%d] to Radio [0x%04x%08x]...\n"), dest.port, ((BD_ADDR *)&dest.btAddr)->NAP, ((BD_ADDR *)&dest.btAddr)->SAP);
   
	retcode = connect(*sock, (struct sockaddr *)&dest, len);

	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
		OutStr(TEXT("\t...connect() Call Failed     Expected : retcode=0   Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		Closesocket(sock);
		*sock = 0;

		//Try connect again
		OutStr(TEXT("\tSince connect() ... Failed ... Attempting Connect() again...\n"));
		OutStr(TEXT("\tNOTE:It is possible Slave rushed through connect() before Master did accept()...\n"));

		//Sleep for 0.5 second before doing connnect
		//This is to avoid the fact that connect() on Slave is too fast 
		//to return fail before the Master even is ready to do accept()
		Sleep(500);

		OutStr(TEXT("\tAttempting to connect() 2nd time ...\n"));
		Socket(sock);

		OutStr(TEXT("\tconnect() call...  on Port [%d] to Radio [0x%04x%08x]...\n"), dest.port, ((BD_ADDR *)&dest.btAddr)->NAP, ((BD_ADDR *)&dest.btAddr)->SAP);
		retcode = connect(*sock, (struct sockaddr *)&dest, len);
		if (retcode == SOCKET_ERROR)
		{
			retcode = WSAGetLastError();
			OutStr(TEXT("\tFailed\n"));
			OutStr(TEXT("\t...2nd attempt to connect() Call Failed     Expected : retcode=0   Got : SOCKET_ERROR\n"));
			OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

			Closesocket(sock);
			*sock = 0;

			return FAIL;
		} 
		else 
		{
		    OutStr(TEXT("\tSuccess\n"));
	   		OutStr(TEXT("\t...2nd attempt to connect() Call Succeeded : retcode = [%d]\n"), retcode);

			return SUCCESS;
		}

   }
   else 
   {
		OutStr(TEXT("\tSuccess\n"));
		OutStr(TEXT("\t...connect() Call Succeeded : retcode = [%d]\n"), retcode);

		return SUCCESS;
   }

   return SUCCESS;
}


int Write(SOCKET *sock, int tot_bytes, int chunks)
{
	char *buffer;
	int nleft=0;
	int idx=0;
	int attempt=0;
	int bytes;
	char *buffer2;
	DWORD i, Cnt;
	char lendata[16];
	int DataAmount = 16;
	int PacketSize = 27;

	int retcode;

	//Flag for controlling the display : reg. Warning for exceeding L2CAP MTU
	int logflag = true;

    buffer = (char *) malloc (tot_bytes);
	memset(buffer, 0, tot_bytes);

	/* Data check */
	if(tot_bytes > 9)
	{
		Cnt = (tot_bytes-9)/PacketSize;
		//The beginning and ending headers are 6 Bytes and 3 bytes [xxxx] and END the packet size is the Number of characters
		//Plus the size of the headers which are 6 and 5 (xxxx)(END)

		//Set the beginning of the the data buffer to after the beginning header
		buffer2 = buffer+6;
		//Copy the size of the buffer to the begining of the packet.
		sprintf(lendata, "[%04X]", tot_bytes);
		memcpy( buffer, lendata, 6);
		//Terminate the packet.
		memcpy( (buffer+(tot_bytes-3)), "END", 3);
		if (buffer2)
		{
			for (i=0; i < Cnt; i++)
			{
				memset(	(buffer2+(i*PacketSize))	, (i%254)+1 ,PacketSize);
				sprintf(lendata, "(%04X)", DataAmount);
				memcpy( (buffer2+(i*PacketSize)), lendata, 6);
				memcpy( (buffer2+(i*PacketSize)+(PacketSize-5)), "(END)", 5);					
			}
				
			memset(	(buffer2+(i*PacketSize)) , '*',(tot_bytes-9)-(PacketSize*Cnt)	);
		}
	}

	nleft = tot_bytes;

	OutStr(TEXT("\t...send() on sock: %d Total Bytes to Xfer : %d in chuncks of : %d\n"), *sock, tot_bytes, chunks);
	
	while(nleft > 0 || tot_bytes == 0)
	{
		++attempt;

		OutStr(TEXT("\tsend() call ...\n"));
		bytes = send(*sock, buffer+idx, chunks, 0);
		if (bytes == SOCKET_ERROR)
		{
			OutStr(TEXT("\tFailed\n"));
			retcode = WSAGetLastError();
		    OutStr(TEXT("\t...send() Call Failed     Expected : retcode=0   Got : SOCKET_ERROR\n"));
			OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

			return FAIL;	
		}

		OutStr(TEXT("\tSuccess\n"));
		OutStr(TEXT("\tAttempt %d : Bytes transferred : %d\n"), attempt, bytes);

		if (bytes == 0)
		{
			OutStr(TEXT("\tBytes recvd 0\n"));
			OutStr(TEXT("\t>> send() on sock: %d  Total Bytes Written : %d [Operation Complete]\n"), *sock, idx);
			break;
		}

		if (tot_bytes == 0)
		{
			//Dont loop forever - rather break
			break;
		}

		nleft -= bytes;

		if (nleft < chunks)
		{
			chunks = nleft;
		}

		idx+= bytes;
	}

	free(buffer);

	return SUCCESS;
}

int Read(SOCKET *sock, int tot_bytes)
{
	char *buffer;
	int nleft=0;
	int idx=0;
	int attempt=0;
	int bytes;
	char *buffer3;
	char *buffer4;
	DWORD i,Cnt;
	char  lendata[16];
	int DataAmount=16;
	int PacketSize=27;
	int buffer_size=0;
	int temp_bytes=0;

	int retcode;

	buffer = (char *) malloc (tot_bytes);
	memset(buffer, 0, tot_bytes);

	nleft = tot_bytes;

	OutStr(TEXT("\t...recv() on sock: %d Total Bytes to Recv : %d\n"), *sock, tot_bytes);

	if (tot_bytes == 0)
	{
		Sleep(1000 * 3);  //Sleep for 3 secs for those tests where I attempt to read 0 bytes
	}
	
	while(nleft > 0 || tot_bytes == 0)
	{
		++attempt;

		if (temp_bytes > 1000)
		{
			OutStr(TEXT("\trecv() call...Bytes Left to recv: %d...\n"), nleft);
			temp_bytes = 0;
		}
		bytes = recv(*sock, buffer+idx , nleft, 0);
	    if (bytes == SOCKET_ERROR)
		{
			OutStr(TEXT("\tFailed\n"));
			retcode = WSAGetLastError();
		    OutStr(TEXT("\t...recv() Call Failed     Expected : retcode=0   Got : SOCKET_ERROR\n"));
			OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);
			break;
		}

		//If tot_bytes == 0 break - dont keep looping forever
		if (tot_bytes == 0 || bytes == 0)
		{
			break;
		}

		nleft -= bytes;
		idx+= bytes;
		temp_bytes += bytes;
	}

	OutStr(TEXT("\t>> recv() on sock: %d Total Bytes Recd :%d  [Operation Complete]\n"), *sock, idx);
	if (tot_bytes != idx)
	{
		OutStr(TEXT("\t*Failed* : Total Bytes Expected : %d  Total Bytes Recd : %d\n"), tot_bytes, idx);
		OutStr(TEXT("\tTotal Bytes does not match Total Bytes Read\n"));
		free(buffer);

		return FAIL;
	}

	if(tot_bytes > 9 )
	{
		sscanf(buffer,"[%04X]",&buffer_size);
		OutStr(TEXT("Buffer size is %d\n"),buffer_size);
		buffer3 = (char *) malloc((buffer_size));				
		memset(buffer3,'~',tot_bytes);
		Cnt = (buffer_size-9)/PacketSize;
		//The beginning and ending headers are 6 Bytes and 3 bytes [xxxx] and END the packet size is the Number of characters
		//Plus the size of the headers which are 6 and 5 (xxxx)(END)

		//Set the beginning of the the data buffer to after the beginning header
		buffer4=buffer3+6;
		//Copy the size of the buffer to the begining of the packet.
		sprintf(lendata, "[%04X]", buffer_size);
		memcpy( buffer3, lendata, 6);
		//Terminate the packet.
		memcpy( (buffer3+(buffer_size-3)), "END", 3);
	
		for (i=0; i < Cnt; i++)
		{
			memset(	(buffer4+(i*PacketSize))	, (i%254)+1 ,PacketSize);
			sprintf(lendata, "(%04X)", DataAmount);
			memcpy( (buffer4+(i*PacketSize)), lendata, 6);
			memcpy( (buffer4+(i*PacketSize)+(PacketSize-5)), "(END)", 5);					
		}
				
		memset(	(buffer4+(i*PacketSize)) , '*',(buffer_size-9)-(PacketSize*Cnt)	);

		if(memcmp(buffer,buffer3,buffer_size)==0)
		{
			OutStr(TEXT("The Data is Valid\n"));
		}
		else
		{
			OutStr(TEXT("\t*Failed* : The Data is invalid\n"));
			free(buffer3);
			free(buffer);

			return FAIL;
		}
		free(buffer3);
	}				

	OutStr(TEXT("\t*Passed* : Total Bytes Expected : %d  Total Bytes Recd : %d\n"), tot_bytes, idx);

	if (tot_bytes > 0)
		free(buffer);

	return SUCCESS;
}

int Shutdown(SOCKET *sock, int how)
{
	int retcode;

   OutStr(TEXT("\tshutdown() call...\n"));
   retcode = shutdown(*sock, how);
   if (retcode == SOCKET_ERROR)
   {
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	   	OutStr(TEXT("\t...shutdown() Call Failed : how = [%d]  Expected retcode =0 Got : SOCKET_ERROR\n"), how);
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"), retcode);

		return FAIL;
   }
   else 
   {
		OutStr(TEXT("\tSuccess\n"));
	   	OutStr(TEXT("\t...shutdown() Call Succeeded : how = [%d] retcode = [%d]\n"), how, retcode);

		return SUCCESS;
   }
}

int LoopBackConnect(SOCKET *sock, SOCKADDR_BTH *server, int port)
{
	int retcode;

	server->addressFamily = AF_BTH;
	memcpy((void *)&(server->btAddr), (void *)&g_LocalRadioAddr, sizeof(g_LocalRadioAddr));
	server->port = port;

	OutStr(TEXT("\tconnect() call...\n"));
	retcode = connect(*sock, (struct sockaddr *)server,  sizeof(SOCKADDR_BTH));
	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
		OutStr(TEXT("\t...Loop Back connect() Call Failed     Expected : retcode=0   Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
	}
	else 
	{
	    OutStr(TEXT("\tSuccess\n"));
	    OutStr(TEXT("\t...Loop Back connect() Call Succeeded : retcode = [%d]\n"), retcode);

		return SUCCESS;
	}
}

int GetPeerName(SOCKET *sock)
{
	int retcode;
	int len;
	SOCKADDR_BTH name;

	len = sizeof(SOCKADDR_BTH);

	OutStr(TEXT("\tgetpeername() call...\n"));
	retcode = getpeername(*sock, (struct sockaddr *)&name, &len);
	if (retcode == SOCKET_ERROR)
	{
	   	retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	   	OutStr(TEXT("\t...getpeername() Call Failed : Expected retcode =0 Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
   }
   else 
   {
	   	OutStr(TEXT("\t...getpeername() Call Succeeded retcode = [%d]\n"), retcode);
		OutStr(TEXT("\taddressFamily = %d\n"), name.addressFamily);
		OutStr(TEXT("\tbtAddr        = [0x%04x%08x]\n"), GET_NAP(name.btAddr), GET_SAP(name.btAddr));
		OutStr(TEXT("\tport          = %d\n"), name.port);

	    if ((name.addressFamily == AF_BTH) &&
			(GET_NAP(name.btAddr) == g_RemoteRadioAddr.NAP) &&
			(GET_SAP(name.btAddr) == g_RemoteRadioAddr.SAP))
		{
			OutStr(TEXT("\tSuccess\n"));
			return SUCCESS;
		}
		else
		{	
			OutStr(TEXT("\tgetpeername result does not match remote address"));
			return FAIL;
		}
   }
}

int GetSockName(SOCKET *sock)
{
	int retcode;
	int len;
	SOCKADDR_BTH name;

	len = sizeof(SOCKADDR_BTH);

	OutStr(TEXT("\tgetsockname() call...\n"));
	retcode = getsockname(*sock, (struct sockaddr *)&name, &len);
	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	   	OutStr(TEXT("\t...getsockname() Call Failed : Expected retcode =0 Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
   }
   else 
   {
	   	OutStr(TEXT("\t...getsockname() Call Succeeded retcode = [%d]\n"), retcode);
		OutStr(TEXT("\taddressFamily = %d\n"), name.addressFamily);
		OutStr(TEXT("\tbtAddr        = [0x%04x%08x]\n"), GET_NAP(name.btAddr), GET_SAP(name.btAddr));
		OutStr(TEXT("\tport          = %d\n"), name.port);

	    if ((name.addressFamily == AF_BTH) &&
			(GET_NAP(name.btAddr) == g_LocalRadioAddr.NAP) &&
			(GET_SAP(name.btAddr) == g_LocalRadioAddr.SAP))
		{
			OutStr(TEXT("\tSuccess\n"));
			return SUCCESS;
		}
		else
		{	
			OutStr(TEXT("\tgetsockname result does not match remote address"));
			return FAIL;
		}
   }
}

// CE does not support WSADuplicateSocket
#ifndef UNDER_CE

int WSA_DuplicateSocket(SOCKET *csock)
{
	int retcode;
	int len;
	WSAPROTOCOL_INFO ProtocolInfo;
	DWORD dwProcessId;
	
	len = sizeof(struct sockaddr);
	dwProcessId = GetCurrentProcessId();

	OutStr(TEXT("WSADuplicateSocket() call...\n"));
	retcode = WSADuplicateSocket(*csock, dwProcessId, &ProtocolInfo);

	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
	   	OutStr(TEXT("\t...WSADuplicate() Call Failed : Expected retcode =0 Got : SOCKET_ERROR\n"));
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);

		return FAIL;
   }
   else 
   {
		OutStr(TEXT("\tSuccess\n"));
	   	OutStr(TEXT("\t...WSADuplicateSocket() Call Succeeded retcode = [%d]\n"), retcode);

		return SUCCESS;
   }
}

#endif

int DiscoverLocalRadio(BD_ADDR *pAddress)
{
	SOCKADDR_BTH locaddr;
	SOCKADDR_BTH addr;

	SOCKET sock = -1;
	int retcode = 0;
	int len = 0;
	int i = 0;

	OutStr(TEXT("Discovering Local Radio ...\n"));

	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (sock == SOCKET_ERROR)
	{
	   OutStr(TEXT("\tSocket Call failed : Protocol : RFCOMM :Error : %d ... Unable to Discover Radio...\n"), WSAGetLastError());
	   return FAIL;
	}

	//Bind local socket

	locaddr.addressFamily = AF_BTH;
	memset(&locaddr.btAddr, 0, sizeof(locaddr.btAddr));
	locaddr.port = 0;

	retcode = bind(sock, (const struct sockaddr *)&locaddr,  sizeof(SOCKADDR_BTH));
	if (retcode == SOCKET_ERROR)
	{
	    retcode = WSAGetLastError();
	    OutStr(TEXT("\tFailed\n"));
		OutStr(TEXT("\t...bind() Call Failed to port : %d     Expected : retcode=0   Got : SOCKET_ERROR\n"), locaddr.port);
		OutStr(TEXT("\tWSAGetLastError  Code = %d\n"),retcode);
		closesocket(sock);
		return FAIL;
	}

  	len  = sizeof(SOCKADDR_BTH);
	if (getsockname(sock, (struct sockaddr*)&addr, &len) == SOCKET_ERROR)
	{
		OutStr(TEXT("\tgetsockname() failed ... %d error code\n"), WSAGetLastError());
		closesocket(sock);
		return FAIL;
	}

	memcpy((void *)pAddress, (void *)&addr.btAddr, sizeof(BD_ADDR));
	OutStr(TEXT("\tThe local address is  .... [0x%04x%08x]\n"), pAddress->NAP, pAddress->SAP);

	closesocket(sock);

	return SUCCESS;
}

int SendControlPacket(SOCKET sock, CONTROL_PACKET *pPacket)
{
	int retcode;

	retcode = send(sock, (char *)pPacket, sizeof(*pPacket), 0);
	if (retcode == SOCKET_ERROR)
	{
		OutStr(TEXT("\tsend failed with error %d\n"), WSAGetLastError());

		return FAIL;
	}

	return SUCCESS;
}

int ReceiveControlPacket(SOCKET sock, CONTROL_PACKET *pPacket)
{
	int retcode;
	int total_bytes;
	int zero_count;

	total_bytes = 0;
	zero_count;
	memset((void *)pPacket, 0, sizeof(*pPacket));

	while(total_bytes < sizeof(*pPacket))
	{
		retcode = recv(sock, (char *)pPacket + total_bytes, sizeof(*pPacket) - total_bytes, 0);
		if (retcode == SOCKET_ERROR)
		{
			OutStr(TEXT("\trecv failed with error %d\n"), WSAGetLastError());

			return FAIL;
		}

		total_bytes += retcode;

		if (retcode == 0)
		{
			zero_count++;
		}
		else
		{
			zero_count = 0;
		}

		if (zero_count >= 100)
		{
			// Don't want it to spin
			OutStr(TEXT("\tHaven't received anything in 100 tries\n"));

			return FAIL;
		}
	}

	return SUCCESS;
}

// Global Kato Object.
static CKato *g_pUtilKato = NULL;

#define countof(a) (sizeof(a)/sizeof(*(a)))

//******************************************************************************

//
//  OutStr: A formatted output routine that outputs to the serial port under
//		    CE or the console otherwise.
//
extern void
OutStr(
	   TCHAR *format, 
	   ...)
{
	va_list		 pArgs;

	va_start(pArgs, format);

#ifdef UNDER_CE
	if (g_bIsServer)
	{
		TCHAR szBuffer[256];
		int	  cbWritten;

		cbWritten = _vsntprintf(&szBuffer[0], countof(szBuffer), format, pArgs);
		RETAILMSG(TRUE, (TEXT("%s"), &szBuffer[0]));
	}
	else
	{
		if (g_pUtilKato == NULL)
		{
			g_pUtilKato = (CKato*)KatoGetDefaultObject();
		}
		ASSERT (g_pUtilKato);
		if (g_pUtilKato)
		{
			g_pUtilKato->LogV (LOG_DETAIL, format, pArgs);
		}
	}
#else // UNDER_CE
	_vtprintf(format, pArgs);
#endif
	va_end(pArgs);
}

// --------------------- Function Table used by TUX ----------------------------

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
	TEXT("Bluetooth Winsock 2 BVT Tests:"							), 0, 0, 0,				NULL,
	TEXT(  "Open and Close Socket Test"								), 1, 1, BASE + 101,	t1_1,
	TEXT(  "Bind Call Test"											), 1, 1, BASE + 102,	t1_2,
	TEXT(  "Bind 2 Times Test"										), 1, 1, BASE + 103,	t1_3,
	TEXT(  "Listen Call Test (backlog = 0)"							), 1, 1, BASE + 104,	t1_4,
	TEXT(  "Listen Call Test (backlog = 1)"							), 1, 1, BASE + 105,	t1_5,
	TEXT(  "Listen Call Test (backlog < 8)"							), 1, 1, BASE + 106,	t1_6,
	TEXT(  "Listen Call Test (backlog = 8)"							), 1, 1, BASE + 107,	t1_7,
	TEXT(  "Listen Call Test (backlog = SOMAXCONN)"					), 1, 1, BASE + 108,	t1_8,
	TEXT(  "Listen Call Test (backlog = SOMAXCONN + 1)"				), 1, 1, BASE + 109,	t1_9,
	TEXT(  "Accept Call Test (CLIENT connect to SERVER)"			), 1, 1, BASE + 110,	t1_10,
	//TEXT(  "Connect 2 Times to same RFCOMM channel Test"			), 1, 1, BASE + 111,	t1_11,
	TEXT(  "Accept Call Test (SERVER connects to CLIENT)"			), 1, 1, BASE + 112,	t1_12,
	TEXT(  "Multiple connections Test(non - simultaneous)"			), 1, 1, BASE + 113,	t1_13,
	TEXT(  "Multiple connections Test(Simultaneous)"				), 1, 1, BASE + 114,	t1_14,
	TEXT(  "Disconnect Order Test"									), 1, 1, BASE + 115,	t1_15,
	TEXT(  "Data Xfer Test (10 bytes with 1 byte chunk)"			), 1, 1, BASE + 116,	t2_1,
	TEXT(  "Data Xfer Test (100 bytes with 10 byte chunk)"			), 1, 1, BASE + 117,	t2_2,
	TEXT(  "Data Xfer Test (1000 bytes with 10 byte chunk)"			), 1, 1, BASE + 118,	t2_3,
	TEXT(  "Data Xfer Test (10000 bytes with 1000 byte chunk)"		), 1, 1, BASE + 119,	t2_4,
	//TEXT(  "Data Xfer Test (30000 bytes with 10000 byte chunk)"		), 1, 1, BASE + 120,	t2_5,
	//TEXT(  "Data Xfer Test (30000 bytes with 30000 byte chunk)"		), 1, 1, BASE + 121,	t2_6,
	//TEXT(  "Data Xfer Test (65535 bytes with 30000 byte chunk)"		), 1, 1, BASE + 122,	t2_7,
	//TEXT(  "Data Xfer Test (65535 bytes with 65535 byte chunk)"		), 1, 1, BASE + 123,	t2_8,
	TEXT(  "Data Xfer Test (0 bytes payload Xfer)"					), 1, 1, BASE + 124,	t2_9,
	TEXT(  "Full duplex data transfer test"							), 1, 1, BASE + 125,	t2_10,
	TEXT(  "Shutdown Call Test (how = SD_RECEIVE)"					), 1, 1, BASE + 126,	t3_1,
	TEXT(  "Shutdown Call Test (how = SD_SEND)"						), 1, 1, BASE + 127,	t3_2,
	TEXT(  "Shutdown Call Test (how = SD_BOTH)"						), 1, 1, BASE + 128,	t3_3,
	TEXT("Bluetooth Winsock 2 API Tests:"							), 0, 0, 0,				NULL,
	TEXT(  "Socket Variations Test"									), 1, 1, BASE + 201,	a1_1,
	TEXT(  "WSASocket Variations Test"								), 1, 1, BASE + 202,	a1_2,
	TEXT(  "Bind Variations Test"									), 1, 1, BASE + 203,	a1_3,
	TEXT("Bluetooth Winsock 2 FUNCTION Tests:"						), 0, 0, 0,				NULL,
	TEXT(  "Connection Management Test (backlog = 0)"				), 1, 1, BASE + 301,	f1_2_8,
	TEXT(  "Connection Management Test (backlog = SOMAXCONN)"		), 1, 1, BASE + 302,	f1_2_9,
	TEXT(  "Connection Management Test (backlog = 8)"				), 1, 1, BASE + 303,	f1_2_10,
	TEXT(  "Connection Management Test (backlog = SOMAXCONN + 1)"	), 1, 1, BASE + 304,	f1_2_11,
	TEXT(  "Get Peer Name Test"										), 1, 1, BASE + 305,	f1_2_16,
	TEXT(  "Get Sock Name Test"										), 1, 1, BASE + 306,	f1_2_17,
	//TEXT(  "Remote Disconnect Notification Test"					), 1, 1, BASE + 307,	f1_5_6,
	//TEXT(  "Synchronous Select Test"								), 1, 1, BASE + 308,	f2_5,
	TEXT(  "SO_ACCEPTCONN Test on listening socket"					), 1, 1, BASE + 309,	f3_1_1,
	TEXT(  "SO_ACCEPTCONN Test on non-listening socket"				), 1, 1, BASE + 310,	f3_1_2,
	TEXT(  "SO_TYPE Test"											), 1, 1, BASE + 311,	f3_2,
	TEXT(  "SO_LINGER Test with getsockopt"							), 1, 1, BASE + 312,	f3_4_1,
	TEXT(  "SO_LINGER Test with setsockopt"							), 1, 1, BASE + 313,	f3_4_2,
	TEXT("Bluetooth Winsock 2 SDP Tests:"							), 0, 0, 0,				NULL,
	//TEXT(  "Enumerate Device Test"									), 1, 1, BASE + 401,	SDP_f1_1_2,
	TEXT(  "Enumerate Services on Local Device Test"				), 1, 1, BASE + 402,	SDP_f1_1_3,
	TEXT(  "Enumerate Services on Remote Device Test"				), 1, 1, BASE + 403,	SDP_f1_1_4,
	TEXT(  "Register Services and Lookup Local Test"				), 1, 1, BASE + 404,	SDP_f1_1_5,
	TEXT(  "Modify Existing SDP Record Test"						), 1, 1, BASE + 405,	SDP_f1_1_6,
	TEXT(  "Delete Service Test"									), 1, 1, BASE + 406,	SDP_f1_1_7,
	TEXT(  "Register Services and Lookup Remote Test"				), 1, 1, BASE + 407,	SDP_f1_1_8,
	TEXT(  "Service Attribute Search Test"							), 1, 1, BASE + 408,	SDP_f1_1_9,
	TEXT(  "SERVICE_MULTIPLE Flag Fail Test"						), 1, 1, BASE + 409,	SDP_f2_1_1,
	TEXT(  "Register Invalid SD Record Test"						), 1, 1, BASE + 410,	SDP_f2_1_2,
	TEXT(  "Update with Invalid SD Record Test"						), 1, 1, BASE + 411,	SDP_f2_1_3,
	TEXT(  "Delete Non-existing SD Record Test"						), 1, 1, BASE + 412,	SDP_f2_1_4,
	TEXT(  "Delete Existing SD Record Twice Test"					), 1, 1, BASE + 413,	SDP_f2_1_5,
	TEXT(  "Lookup Service Test (class ID)"							), 1, 1, BASE + 414,	SDP_f2_1_6,
	TEXT(  "Lookup Service Test (invaid address)"					), 1, 1, BASE + 415,	SDP_f2_1_7,

	NULL,															   0, 0, 0,				NULL  // marks end of list
};
 
