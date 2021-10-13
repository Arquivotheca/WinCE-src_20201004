//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// util.h

#ifndef __UTIL_H__
#define __UTIL_H__

#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <ws2bth.h>
#include <tux.h>

#define DEFAULT_PORT 6789

// return code
#define SUCCESS	1 
#define EROR	-1
#define FAIL	0

// control packet
typedef struct _CONTROL_PACKET {
	DWORD dwCommand;
	char  Data[8];
} CONTROL_PACKET, *PCONTROL_PACKET;

// commands
#define EXCHANGE_BT_ADDR	1
#define BEGIN_TEST			2
#define END_TEST			3
#define TEST_DONE			4

/* Winsock Wrapper APIs */
int Socket(SOCKET *sock);
int Closesocket(SOCKET *sock);
int Bind(SOCKET *sock, int port);
int Listen(SOCKET *sock, int backlog);
int Accept(SOCKET *sock, SOCKET *csock, SOCKADDR_BTH *client);
int Connect(SOCKET *sock, SOCKADDR_BTH *server, int port);
int Write(SOCKET *sock, int tot_bytes, int chunks);
int Read(SOCKET *sock, int tot_bytes);
int Shutdown(SOCKET *sock, int how);
int LoopBackConnect(SOCKET *sock, SOCKADDR_BTH *server, int port);
int GetPeerName(SOCKET *sock);
int GetSockName(SOCKET *sock);
#ifndef UNDER_CE
int WSA_DuplicateSocket(SOCKET *csock);
#endif
int WSA_Socket(SOCKET *sock);
int Ioctlsocket(SOCKET *sock, long cmd, ULONG arg);
int WSA_Accept(SOCKET *sock, SOCKET *csock, SOCKADDR_BTH *client);
int DiscoverLocalRadio(BD_ADDR *pAddress);

int SendControlPacket(SOCKET sock, CONTROL_PACKET *pPacket);
int ReceiveControlPacket(SOCKET sock, CONTROL_PACKET *pPacket);

extern void OutStr(TCHAR *format, ...);

// Local Bluetooth device address.
extern BD_ADDR g_LocalRadioAddr;

// Remote Bluetooth device address.
extern BD_ADDR g_RemoteRadioAddr;

// Server address.
extern SOCKADDR_IN g_ServerAddr; 

// Role server/client.
extern BOOL g_bIsServer;

// Skip connection test.
extern BOOL g_bNoServer;

// TEST_ENTRY
#define TEST_ENTRY \
	if (uMsg == TPM_QUERY_THREAD_COUNT) \
	{ \
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData; \
		return SPR_HANDLED; \
	} \
	else if (uMsg != TPM_EXECUTE) \
	{ \
		return TPR_NOT_HANDLED; \
	}

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00000000

// Our function table that we pass to Tux
extern FUNCTION_TABLE_ENTRY g_lpFTE[];

// ------------------------ Test Functions ----------------------------
extern TESTPROCAPI t1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_11(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_12(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_13(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_14(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_15(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t1_16(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t2_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t3_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t3_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI t3_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

extern TESTPROCAPI a1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI a1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI a1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

extern TESTPROCAPI f1_1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_11(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_16(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_2_17(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f1_5_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f2_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_4_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI f3_4_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

extern TESTPROCAPI SDP_f1_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f1_1_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
extern TESTPROCAPI SDP_f2_1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

#endif //__UTIL_H__

