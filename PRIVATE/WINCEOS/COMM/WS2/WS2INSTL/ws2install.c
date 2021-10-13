//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

ws2install.c

installs the protocols working under pm with Winsock 2


FILE HISTORY:
	OmarM     15-Sep-2000
	

*/

/*
Each Protocols needs to have one of these filled out to be submitted to WS2_32

    DWORD dwServiceFlags1;
    DWORD dwServiceFlags2;
    DWORD dwServiceFlags3;
    DWORD dwServiceFlags4;
    DWORD dwProviderFlags;
    GUID ProviderId;
    DWORD dwCatalogEntryId;
    WSAPROTOCOLCHAIN ProtocolChain;
    int iVersion;
    int iAddressFamily;
    int iMaxSockAddr;
    int iMinSockAddr;
    int iSocketType;
    int iProtocol;
    int iProtocolMaxOffset;
    int iNetworkByteOrder;
    int iSecurityScheme;
    DWORD dwMessageSize;
    DWORD dwProviderReserved;
    WCHAR  szProtocol[WSAPROTOCOL_LEN+1];
*/

#include <winsock2.h>
#include <ws2spi.h>
#include <types.h>
#include <ws2bth.h>
#include <winreg.h>
#include <svsutil.hxx>

extern void InstallThirdPartyLSP();
//
// Function to look in registry and verify with LoadLibrary that a stack dll
// is loaded and available.
//
// lpszStack == name of protocol stack to check
// bDefault == TRUE => may not be listed in Comm\\AFD:Stacks list because it is one of the default stacks
//             FALSE => will be listed in Comm\\AFD:Stacks list. 
//
// Return 1 if stack is present, 0 if not.
//
DWORD
IsStackInstalled(
    LPTSTR lpszStack,
    BOOL   bDefault
    )
{
    HKEY hStacksKey;
    DWORD rc;
    WCHAR szStacks[128];
    HMODULE hStack;
    DWORD ret;
    DWORD dwType;
    DWORD dwSize;
    // Try to load default stacks, and ones configured in registry
    BOOL  bTryLoad = bDefault;

    ret = 0;

    rc = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            L"Comm\\AFD",
            0,
            KEY_READ | KEY_QUERY_VALUE,
            &hStacksKey
            );
    if (ERROR_SUCCESS == rc) {
        dwSize = sizeof(szStacks);
        rc = RegQueryValueEx(hStacksKey, L"Stacks", NULL, &dwType, (UCHAR *)szStacks, &dwSize);
        if (ERROR_SUCCESS == rc) {
            WCHAR *szStack = szStacks;
            // If stacks value is present, only load those specified
            bTryLoad = FALSE;
            // Walk multi_sz string
            if (dwType == REG_MULTI_SZ) {
                while (*szStack) {
                    if (! wcsicmp(szStack,lpszStack)) {
                        bTryLoad = TRUE;
                        break;
                    }
                    while (*szStack++)
                        ;
                }
            }
            
            if (!bTryLoad) {
                DEBUGMSG(1, (L"Ws2Instl:IsStackInstalled(%s) - Not in stacks list\n", lpszStack));
            }
        } else {
            DEBUGMSG(1, (L"Ws2Instl:IsStackInstalled(%s) - RegQueryValueEx(Stacks) failed %d\n", lpszStack, rc));
        }
        
        RegCloseKey(hStacksKey);
    } else {
        if (!bDefault) {
	        DEBUGMSG(1, (L"Ws2Instl:IsStackInstalled(%s) - RegOpenKeyEx(Comm\\AFD) failed %d\n", lpszStack, rc));
        }
    }
    
    if (bTryLoad) {
        hStack = LoadLibrary(lpszStack);
        if (hStack) {
            FreeLibrary(hStack);
            //DEBUGMSG(1, (L"Ws2Instl:IsStackInstalled(%s) - Yes\n", lpszStack));
            ret = 1;
        } else {
            DEBUGMSG(1, (L"Ws2Instl:IsStackInstalled - LoadLibrary(%s) failed %d\n", lpszStack, GetLastError()));
        }
    }
    return ret;

}   // IsStackInstalled



int Ws2Instl(DWORD Context, DWORD OpCode, PBYTE pVTable, DWORD cVTable, 
	PBYTE pMTable, DWORD cMTable, PDWORD pIndex) 
{

	WCHAR		ProviderPath[MAX_PATH];
	DWORD		cEntries;
    DWORD       cIrdastk;
    DWORD       cBluetooth;
    DWORD		cIPv6;
	int			Err, Status;
	WSAPROTOCOL_INFOW	*pProtInfo, *p;
	
	GUID		Id = { 0xaafc3764, 0x763b, 0x427b, {0xb1, 0x5b, 0x97, 0xa6,
						0xe5, 0xc9, 0x83, 0xce}};

	ASSERT(1 == OpCode);
	*pIndex = 0;

    cIrdastk = IsStackInstalled(TEXT("irdastk"), TRUE);
    cBluetooth = IsStackInstalled(TEXT("btd"), TRUE);
    cIPv6 = IsStackInstalled(TEXT("tcpip6"), TRUE);
#ifdef RAW_SOCKETS
	cEntries = 3 + cIrdastk + cBluetooth + (cIPv6 * 2);
#else
	cEntries = 2 + cIrdastk + cBluetooth + (cIPv6 * 2);
#endif
	p = pProtInfo = LocalAlloc(LPTR, sizeof(*pProtInfo) * cEntries);

	// setup TCP/IP prot info
	pProtInfo->dwServiceFlags1 = (XP1_GUARANTEED_DELIVERY|XP1_GUARANTEED_ORDER|
			XP1_GRACEFUL_CLOSE);

	pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
	memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
	// pProtInfo->dwProviderFlags <- assigned by ws2_32
	pProtInfo->ProtocolChain.ChainLen = 1;
	pProtInfo->iVersion = 2;
	pProtInfo->iAddressFamily = AF_INET;
	pProtInfo->iMaxSockAddr = 16;
	pProtInfo->iMinSockAddr = 16;
	pProtInfo->iSocketType = SOCK_STREAM;
	pProtInfo->iProtocol = IPPROTO_TCP;
//	pProtInfo->iProtocolMaxOffset = 0;
//	pProtInfo->iNetworkByteOrder = 0;
//	pProtInfo->iSecurityScheme = 0;
//	pProtInfo->dwMessageSize = 0;
//	pProtInfo->dwProviderReserved = 0;
	wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Tcpip [TCP/IP]")));

	pProtInfo++;
	
	// setup UDP/IP prot info
	pProtInfo->dwServiceFlags1 = (XP1_CONNECTIONLESS|XP1_MESSAGE_ORIENTED|
			XP1_SUPPORT_BROADCAST|XP1_SUPPORT_MULTIPOINT);

	pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
	memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
	// pProtInfo->dwProviderFlags <- assigned by ws2_32
	pProtInfo->ProtocolChain.ChainLen = 1;
	pProtInfo->iVersion = 2;
	pProtInfo->iAddressFamily = AF_INET;
	pProtInfo->iMaxSockAddr = 16;
	pProtInfo->iMinSockAddr = 16;
	pProtInfo->iSocketType = SOCK_DGRAM;
	pProtInfo->iProtocol = IPPROTO_UDP;
//	pProtInfo->iProtocolMaxOffset = 0;
//	pProtInfo->iNetworkByteOrder = 0;
//	pProtInfo->iSecurityScheme = 0;
	pProtInfo->dwMessageSize = 65467;
//	pProtInfo->dwProviderReserved = 0;
	wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Tcpip [UDP/IP]")));

#ifdef RAW_SOCKETS
	pProtInfo++;
	
	// setup RAW/IP prot info
	pProtInfo->dwServiceFlags1 = (XP1_CONNECTIONLESS|XP1_MESSAGE_ORIENTED|
			XP1_SUPPORT_BROADCAST|XP1_SUPPORT_MULTIPOINT);

	pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
	memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
	// pProtInfo->dwProviderFlags <- assigned by ws2_32
	pProtInfo->ProtocolChain.ChainLen = 1;
	pProtInfo->iVersion = 2;
	pProtInfo->iAddressFamily = AF_INET;
	pProtInfo->iMaxSockAddr = 16;
	pProtInfo->iMinSockAddr = 16;
	pProtInfo->iSocketType = SOCK_RAW;
	pProtInfo->iProtocol = 0;
	pProtInfo->iProtocolMaxOffset = 255;
//	pProtInfo->iNetworkByteOrder = 0;
//	pProtInfo->iSecurityScheme = 0;
	pProtInfo->dwMessageSize = 65467;
//	pProtInfo->dwProviderReserved = 0;
	wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Tcpip [RAW/IP]")));
#endif

    // setup IPv6 protocol info
    if (cIPv6) {

		pProtInfo++;
		
		// setup TCP/IPv6 prot info
    	
		pProtInfo->dwServiceFlags1 = (XP1_GUARANTEED_DELIVERY|
			XP1_GUARANTEED_ORDER|XP1_GRACEFUL_CLOSE);

		pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
		memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
		// pProtInfo->dwProviderFlags <- assigned by ws2_32
		pProtInfo->ProtocolChain.ChainLen = 1;
		pProtInfo->iVersion = 2;
		pProtInfo->iAddressFamily = AF_INET6;
		pProtInfo->iMaxSockAddr = 28;
		pProtInfo->iMinSockAddr = 28;
		pProtInfo->iSocketType = SOCK_STREAM;
		pProtInfo->iProtocol = IPPROTO_TCP;
	//	pProtInfo->iProtocolMaxOffset = 0;
	//	pProtInfo->iNetworkByteOrder = 0;
	//	pProtInfo->iSecurityScheme = 0;
	//	pProtInfo->dwMessageSize = 0;
	//	pProtInfo->dwProviderReserved = 0;
		wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Tcpip [TCP/IPv6]")));

		pProtInfo++;
		
		// setup UDP/IPv6 prot info
		pProtInfo->dwServiceFlags1 = (XP1_CONNECTIONLESS|XP1_MESSAGE_ORIENTED|
				XP1_SUPPORT_BROADCAST|XP1_SUPPORT_MULTIPOINT);

		pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
		memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
		// pProtInfo->dwProviderFlags <- assigned by ws2_32
		pProtInfo->ProtocolChain.ChainLen = 1;
		pProtInfo->iVersion = 2;
		pProtInfo->iAddressFamily = AF_INET6;
		pProtInfo->iMaxSockAddr = 28;
		pProtInfo->iMinSockAddr = 28;
		pProtInfo->iSocketType = SOCK_DGRAM;
		pProtInfo->iProtocol = IPPROTO_UDP;
	//	pProtInfo->iProtocolMaxOffset = 0;
	//	pProtInfo->iNetworkByteOrder = 0;
	//	pProtInfo->iSecurityScheme = 0;
		pProtInfo->dwMessageSize = 65527;
	//	pProtInfo->dwProviderReserved = 0;
		wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Tcpip [UDP/IPv6]")));
	
    }


    // setup IrDA protocol info
    if (cIrdastk) {
        pProtInfo++;
        pProtInfo->dwServiceFlags1 = (XP1_GUARANTEED_DELIVERY|XP1_GUARANTEED_ORDER);

        pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
        memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
        // pProtInfo->dwProviderFlags <- assigned by ws2_32
        pProtInfo->ProtocolChain.ChainLen = 1;
        pProtInfo->iVersion = 2;
        pProtInfo->iAddressFamily = AF_IRDA;
        pProtInfo->iMaxSockAddr = 31;
        pProtInfo->iMinSockAddr = 31;
        pProtInfo->iSocketType = SOCK_STREAM;
        pProtInfo->iProtocol = 0;
    //	pProtInfo->iProtocolMaxOffset = 0;
    //	pProtInfo->iNetworkByteOrder = 0;
    //	pProtInfo->iSecurityScheme = 0;
        pProtInfo->dwMessageSize = 2048;
    //	pProtInfo->dwProviderReserved = 0;
        wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS IrDA")));
    }
	
    // setup Bluetooth protocol info
    if (cBluetooth) {
        pProtInfo++;
        pProtInfo->dwServiceFlags1 = (XP1_GUARANTEED_DELIVERY|XP1_GUARANTEED_ORDER);

        pProtInfo->dwProviderFlags = PFL_MATCHES_PROTOCOL_ZERO;
        memcpy(&pProtInfo->ProviderId, &Id, sizeof(Id));
        // pProtInfo->dwProviderFlags <- assigned by ws2_32
        pProtInfo->ProtocolChain.ChainLen = 1;
        pProtInfo->iVersion = 2;
        pProtInfo->iAddressFamily = AF_BTH;
        pProtInfo->iMaxSockAddr = sizeof(SOCKADDR_BTH);
        pProtInfo->iMinSockAddr = sizeof(SOCKADDR_BTH);
        pProtInfo->iSocketType = SOCK_STREAM;
        pProtInfo->iProtocol = BTHPROTO_RFCOMM;
    //	pProtInfo->iProtocolMaxOffset = 0;
    //	pProtInfo->iNetworkByteOrder = 0;
    //	pProtInfo->iSecurityScheme = 0;
    //  pProtInfo->dwMessageSize = 0;
    //	pProtInfo->dwProviderReserved = 0;
        wcscpy(pProtInfo->szProtocol, (TEXT("Windows CE MS Bluetooth")));
    }

	
	wcscpy(ProviderPath, (TEXT("wspm.dll")));
	Status = WSCInstallProvider(&Id, ProviderPath, p, cEntries, &Err);

	LocalFree(p);

	if (SOCKET_ERROR == Status) {
		DEBUGMSG(1, 
			(TEXT("Ws2Install: couldn't install the providers: Err %X\r\n"),
			Err));
			
		ASSERT(0);
	} else {
		ASSERT(! Status);
	}


	// Install the users LSP 
	InstallThirdPartyLSP();

	return 1;
	
}	// Ws2Install()


HRESULT InstallNSPM()
{
	// now install the name spaces...
	GUID		NsId = { 0x3c8441d3, 0xb4f9, 0x4ea0, {0x89, 0x74, 0xf4, 0xc3,
						0x49, 0x66, 0x51, 0x91}};
	WCHAR		ProviderPath[]=L"nspm.dll";
    int Status;
	
	Status = WSCInstallNameSpace((TEXT("Windows CE DNS/WINS Name Resolver")), 
		ProviderPath, NS_DNS, 0, &NsId);
		
	if (SOCKET_ERROR == Status) {
		DEBUGMSG(1, 
			(TEXT("Ws2Install: couldn't install name space: Err 0x%X\r\n"),
			WSAGetLastError()));
			
        return WSAGetLastError();
	} 

    return STATUS_SUCCESS;
}


BOOL DllEntry (HANDLE hinstDLL, DWORD Op, PVOID lpvReserved) 
{
	BOOL Status = TRUE;

	switch (Op) {
	case DLL_PROCESS_ATTACH:
		DEBUGMSG (1, (TEXT("Ws2Instl dllentry() %d\r\n"), hinstDLL));
        DisableThreadLibraryCalls ((HMODULE)hinstDLL);
        svsutil_Initialize();
		break;

	case DLL_PROCESS_DETACH:
        svsutil_DeInitialize();

	default :
		break;
	}
	return Status;
}	// DllEntry()


