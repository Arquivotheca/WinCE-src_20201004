//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    scping.cpp

Abstract:

  Small Client private ping client and server


--*/
#include <windows.h>
#include <winsock.h>
#include <nspapi.h>

#include <sc.hxx>
#include <scping.hxx>

#define PING_SIGNATURE       'UH'

//---------------------------------------------------------
//
//  IMPLEMENTATION
//
//---------------------------------------------------------
//---------------------------------------------------------
//
//  class CPingPacket.
//
//---------------------------------------------------------
struct CPingPacket : public SVSAllocClass {
private:
    union {
        USHORT m_wFlags;
        struct {
            USHORT m_bfIC : 1;
            USHORT m_bfRefuse : 1;
        };
    };
    USHORT  m_ulSignature;
    DWORD   m_dwCookie;
    GUID    m_myQMGuid ;

public:
    CPingPacket (void) {};

	CPingPacket(DWORD dwCookie, USHORT fIC, USHORT fRefuse, GUID *pQMGuid)
	{
		m_bfIC = fIC;
        m_bfRefuse = fRefuse;
        m_ulSignature = PING_SIGNATURE;
        m_dwCookie = dwCookie;
        m_myQMGuid = *pQMGuid;
	};

    GUID *pOtherGuid		(void)		 { return &m_myQMGuid;};
	DWORD Cookie			(void) const { return m_dwCookie; };
    BOOL IsOtherSideClient	(void) const { return m_bfIC;	  };
    BOOL IsRefuse			(void) const { return m_bfRefuse; };
    BOOL IsValidSignature	(void) const { return m_ulSignature == PING_SIGNATURE; };
};

//---------------------------------------------------------
//
//  Ping
//
//---------------------------------------------------------
static SOCKET gs_sock = INVALID_SOCKET;
static HANDLE gs_hServerThread = NULL;
static long gs_lCookie = 0;

DWORD WINAPI PingWorkerThread (LPVOID lpParam) {

	for ( ; ; ) {
		CPingPacket pkt;
		SOCKADDR_IN	sa_in;
		int sa_len = sizeof(sa_in);
		int ucb_cb = recvfrom (gs_sock, (char *)&pkt, sizeof(pkt), 0, (SOCKADDR *)&sa_in, &sa_len);

		if (ucb_cb == SOCKET_ERROR)
			break;

		if ((ucb_cb != sizeof(pkt)) || (! pkt.IsValidSignature ()))
			continue;

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"MQPing received from %d.%d.%d.%d GUID = " SC_GUID_FORMAT L"\n",
			sa_in.sin_addr.S_un.S_un_b.s_b1, sa_in.sin_addr.S_un.S_un_b.s_b2,
			sa_in.sin_addr.S_un.S_un_b.s_b3, sa_in.sin_addr.S_un.S_un_b.s_b4,
			SC_GUID_ELEMENTS((pkt.pOtherGuid ()))
			);
#endif

		CPingPacket pkt2( pkt.Cookie() , TRUE, FALSE, &gMachine->guid);

		sendto (gs_sock, (char*)&pkt2, sizeof(pkt2), 0, (SOCKADDR *)&sa_in, sizeof(SOCKADDR));
	}

	return 0;
}

//---------------------------------------------------------
//
//  StartPingServer(...)
//
//---------------------------------------------------------
BOOL StartPingServer (void) {
	if (! gMachine->uiPingPort)
		return TRUE;

	gs_sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (gs_sock == INVALID_SOCKET)
		return FALSE;

	SOCKADDR_IN sa;

	memset ((char *)&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(gMachine->uiPingPort);
	sa.sin_addr.S_un.S_addr = 0;

	if (bind(gs_sock, (SOCKADDR *)&sa, sizeof(sa)) == SOCKET_ERROR) {
		closesocket (gs_sock);
		gs_sock = INVALID_SOCKET;
		return FALSE;
	}

	DWORD dwTID = 0;

	gs_hServerThread = CreateThread (NULL, 0, PingWorkerThread, NULL, 0, &dwTID);

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"MQPing initialization %s...\n", gs_hServerThread ? L"successful" : L"failed" );
#endif

	if (! gs_hServerThread) {
		closesocket (gs_sock);
		gs_sock = INVALID_SOCKET;
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------
//
//  StopPingServer(...)
//
//---------------------------------------------------------
void StopPingServer (void) {
	if (gs_sock != INVALID_SOCKET)
		closesocket (gs_sock);

	gs_sock = INVALID_SOCKET;

	if (gs_hServerThread) {
		if (WaitForSingleObject (gs_hServerThread, 5000) == WAIT_TIMEOUT) {
			scerror_Inform (MSMQ_SC_ERRMSG_PINGKILLED);
			TerminateThread (gs_hServerThread, 0);
		}

		CloseHandle (gs_hServerThread);
	}

	gs_hServerThread = NULL;
}

//---------------------------------------------------------
//
//  ping(...)
//
//---------------------------------------------------------

BOOL ping(unsigned long ip) {
	if (! gMachine->uiPingPort)
		return FALSE;

    SOCKADDR_IN ping_addr;

	ping_addr.sin_family = AF_INET;
    ping_addr.sin_port = htons(gMachine->uiPingPort);
	ping_addr.sin_addr.S_un.S_addr = ip;

	SOCKET s = socket (PF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in sin;
	memset (&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port   = htons (gMachine->uiPingPort);
	sin.sin_addr.S_un.S_addr = ip;

    CPingPacket Pkt( InterlockedIncrement (&gs_lCookie) + 1, TRUE, FALSE, &gMachine->guid);

	if (sendto (s, (char *)&Pkt, sizeof(Pkt), 0, (struct sockaddr *)&sin, sizeof(sin)) != sizeof(Pkt)) {
#if defined (SC_VERBOSE)
		{
		SOCKADDR_IN	sa_in = ping_addr;

		scerror_DebugOut (VERBOSE_MASK_SESSION, L"MQPing to  %d.%d.%d.%d has failed\n",
			sa_in.sin_addr.S_un.S_un_b.s_b1, sa_in.sin_addr.S_un.S_un_b.s_b2,
			sa_in.sin_addr.S_un.S_un_b.s_b3, sa_in.sin_addr.S_un.S_un_b.s_b4);
		}
#endif
		closesocket (s);
		return FALSE;
	}

	fd_set f;
	FD_ZERO (&f);
	FD_SET (s, &f);

	timeval tv;
	tv.tv_sec  = gMachine->uiPingTimeout / 1000;
	tv.tv_usec = (gMachine->uiPingTimeout % 1000) * 1000;

	if (select (0, &f, NULL, NULL, &tv) <= 0) {
#if defined (SC_VERBOSE)
		{
		SOCKADDR_IN	sa_in = ping_addr;

		scerror_DebugOut (VERBOSE_MASK_SESSION, L"MQPing to  %d.%d.%d.%d has failed\n",
			sa_in.sin_addr.S_un.S_un_b.s_b1, sa_in.sin_addr.S_un.S_un_b.s_b2,
			sa_in.sin_addr.S_un.S_un_b.s_b3, sa_in.sin_addr.S_un.S_un_b.s_b4);
		}
#endif
		closesocket (s);
		return FALSE;
	}

	struct sockaddr_in sin2;
	int s2len = sizeof(sin2);
	memset (&sin2, 0, s2len);

	int iSize = recvfrom (s, (char *)&Pkt, sizeof(Pkt), 0, (struct sockaddr *)&sin2, &s2len);

	closesocket (s);

#if defined (SC_VERBOSE)
	if (! (iSize == sizeof(Pkt) && Pkt.IsValidSignature ())) {
		SOCKADDR_IN	sa_in = ping_addr;

		scerror_DebugOut (VERBOSE_MASK_SESSION, L"MQPing to  %d.%d.%d.%d has failed\n",
			sa_in.sin_addr.S_un.S_un_b.s_b1, sa_in.sin_addr.S_un.S_un_b.s_b2,
			sa_in.sin_addr.S_un.S_un_b.s_b3, sa_in.sin_addr.S_un.S_un_b.s_b4);
		}
	else {
		SOCKADDR_IN	sa_in = ping_addr;

		scerror_DebugOut (VERBOSE_MASK_SESSION, L"MQPing to  %d.%d.%d.%d has succeeded\n",
			sa_in.sin_addr.S_un.S_un_b.s_b1, sa_in.sin_addr.S_un.S_un_b.s_b2,
			sa_in.sin_addr.S_un.S_un_b.s_b3, sa_in.sin_addr.S_un.S_un_b.s_b4);
	}
#endif
    return iSize == sizeof(Pkt) && Pkt.IsValidSignature ();
}

