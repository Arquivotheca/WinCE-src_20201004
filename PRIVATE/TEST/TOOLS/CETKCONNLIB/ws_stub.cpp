#include <windows.h>
#include <winsock.h>

HMODULE ghWinsockDll = NULL;

typedef int (*PFN_WSAStartup) (WORD wVersionRequested, LPWSADATA lpWSAData);
typedef int  (*PFN_WSACleanup) (void);
typedef int (WINSOCKAPI *PFN_connect) (SOCKET s, const struct sockaddr *name, int namelen);
typedef struct hostent FAR* (*PFN_gethostbyname) (const char FAR* name);
typedef unsigned long (*PFN_inet_addr) (const char FAR* cp);
typedef u_short (*PFN_htons) (u_short hostshort);
typedef SOCKET (*PFN_socket) (int af, int type, int protocol);
typedef int (*PFN_closesocket) (SOCKET s);
typedef int (*PFN_send) (SOCKET s, const char FAR* buf, int len, int flags);
typedef int (*PFN_recv) (SOCKET s, char FAR* buf, int len, int flags);

typedef char * (WINSOCKAPI *PFN_inet_ntoa) (struct in_addr in);
typedef int (WINSOCKAPI *PFN_listen) (SOCKET s, int backlog);
typedef struct hostent * (WINSOCKAPI *PFN_gethostbyaddr)(const char * addr, int len, int type);
typedef SOCKET (WINSOCKAPI *PFN_accept) (SOCKET s, struct sockaddr *addr, int *addrlen);
typedef int (WINSOCKAPI *PFN_bind) (SOCKET s, const struct sockaddr *addr, int namelen);


PFN_WSAStartup pfn_WSAStartup;
PFN_WSACleanup pfn_WSACleanup;
PFN_connect pfn_connect;
PFN_gethostbyname pfn_gethostbyname;
PFN_inet_addr pfn_inet_addr;
PFN_htons pfn_htons;
PFN_socket pfn_socket;
PFN_closesocket pfn_closesocket;
PFN_send pfn_send;
PFN_recv pfn_recv;

PFN_inet_ntoa pfn_inet_ntoa;
PFN_gethostbyaddr pfn_gethostbyaddr;
PFN_accept pfn_accept;
PFN_listen pfn_listen;
PFN_bind pfn_bind;

int WSAStartup (WORD wVersionRequested, LPWSADATA lpWSAData)
{
    ghWinsockDll = LoadLibrary (_T("winsock.dll"));

    if (!ghWinsockDll) {
        NKDbgPrintfW (_T("Unable to load winsock.dll (GLE: %u)\r\n"), GetLastError ());
        return WSASYSNOTREADY;
    }

    // Get function pointers.
    pfn_WSAStartup = (PFN_WSAStartup) GetProcAddress (ghWinsockDll, _T("WSAStartup"));
    pfn_WSACleanup = (PFN_WSACleanup) GetProcAddress (ghWinsockDll, _T("WSACleanup"));
    pfn_connect = (PFN_connect) GetProcAddress (ghWinsockDll, _T("connect"));
    pfn_gethostbyname = (PFN_gethostbyname) GetProcAddress (ghWinsockDll, _T("gethostbyname"));
    pfn_inet_addr = (PFN_inet_addr) GetProcAddress (ghWinsockDll, _T("inet_addr"));
    pfn_htons = (PFN_htons) GetProcAddress (ghWinsockDll, _T("htons"));
    pfn_socket = (PFN_socket) GetProcAddress (ghWinsockDll, _T("socket"));
    pfn_closesocket = (PFN_closesocket) GetProcAddress (ghWinsockDll, _T("closesocket"));
    pfn_send = (PFN_send) GetProcAddress (ghWinsockDll, _T("send"));
    pfn_recv = (PFN_recv) GetProcAddress (ghWinsockDll, _T("recv"));

	pfn_inet_ntoa = (PFN_inet_ntoa) GetProcAddress (ghWinsockDll, _T("inet_ntoa"));
	pfn_gethostbyaddr = (PFN_gethostbyaddr) GetProcAddress (ghWinsockDll, _T("gethostbyaddr"));
	pfn_accept = (PFN_accept) GetProcAddress (ghWinsockDll, _T("accept"));
	pfn_listen = (PFN_listen) GetProcAddress (ghWinsockDll, _T("listen"));
	pfn_bind = (PFN_bind) GetProcAddress (ghWinsockDll, _T("bind"));

    if (
        pfn_WSAStartup &&
        pfn_WSACleanup &&
        pfn_connect &&
        pfn_gethostbyname &&
        pfn_inet_addr &&
        pfn_htons &&
        pfn_socket &&
        pfn_closesocket &&
        pfn_send &&
        pfn_recv &&
        
		pfn_inet_ntoa &&
		pfn_gethostbyaddr &&
		pfn_accept &&
		pfn_listen &&
		pfn_bind
        ) {
        return pfn_WSAStartup (wVersionRequested, lpWSAData);
    }
    else {
        NKDbgPrintfW (_T("Failed to resolve imports to winsock.dll\r\n"));

        FreeLibrary (ghWinsockDll);
        ghWinsockDll = NULL;

        return WSASYSNOTREADY;
    }
}

int  WSACleanup (void)
{
    int nRet;

    if (!ghWinsockDll) {
        return WSANOTINITIALISED;
    }

    nRet = pfn_WSACleanup ();

    if (0 == nRet) {
        // Unload winsock.dll
        FreeLibrary (ghWinsockDll);
        ghWinsockDll = NULL;
    }

    return nRet;
}

int WINSOCKAPI connect (SOCKET s, const struct sockaddr *name, int namelen)
{
    return pfn_connect (s, name, namelen);
}

struct hostent FAR* gethostbyname (const char FAR* name)
{
    return pfn_gethostbyname (name);
}

unsigned long inet_addr (const char FAR* cp)
{
    return pfn_inet_addr (cp);
}

u_short htons (u_short hostshort)
{
    return pfn_htons (hostshort);
}

SOCKET socket (int af, int type, int protocol)
{
    return pfn_socket (af, type, protocol);
}

int closesocket (SOCKET s)
{
    return pfn_closesocket (s);
}

int send (SOCKET s, const char FAR* buf, int len, int flags)
{
    return pfn_send (s, buf, len, flags);
}

int recv (SOCKET s, char FAR* buf, int len, int flags)
{
    return pfn_recv (s, buf, len, flags);
}


char * WINSOCKAPI inet_ntoa (struct in_addr in)
{
	return pfn_inet_ntoa(in);
}

int WINSOCKAPI listen (SOCKET s, int backlog)
{
	return pfn_listen(s, backlog);
}

struct hostent * WINSOCKAPI gethostbyaddr(const char * addr, int len, int type)
{
	return pfn_gethostbyaddr(addr, len, type);
}

SOCKET WINSOCKAPI accept (SOCKET s, struct sockaddr *addr, int *addrlen)
{
	return pfn_accept(s, addr, addrlen);
}

int WINSOCKAPI bind (SOCKET s, const struct sockaddr *addr, int namelen)
{
	return pfn_bind(s, addr, namelen);
}
