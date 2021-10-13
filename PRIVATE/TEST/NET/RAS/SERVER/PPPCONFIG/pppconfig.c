//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "windows.h"
#include <tapi.h>
#include <unimodem.h>
#include "cclib.h"
#include "ras.h"
#include "raserror.h"
#include "afdfunc.h"

#include <stdio.h>
#include <stdarg.h>

#ifndef ERROR
#define ERROR 0
#endif
#define DEVMINCFG_VERSION 0x0010
#define DIAL_MODIFIER_LEN 40

#define COUNTOF(a) (sizeof(a)/sizeof(*(a)))

BOOL	g_bOutputToConsole = TRUE;
BOOL	g_bVerbose = FALSE;

#ifdef UNDER_CE
typedef int (__cdecl *PFN_wprintf)(const wchar_t *, ...);
PFN_wprintf	v_pfn_wprintf;
HMODULE v_hCoreDLL;
#endif

void
Print(
	TCHAR *pFormat, 
	...)
{
	va_list ArgList;
	TCHAR	Buffer[256];

	va_start (ArgList, pFormat);

	(void)wvsprintf (Buffer, pFormat, ArgList);

	if (g_bOutputToConsole)
	{
		if (v_pfn_wprintf == NULL)
		{
			// Since not all configs contain the wprintf function we'll
			// try to find it.  If it's not there we'll default to using
			// OutputDebugString.
			v_hCoreDLL = LoadLibrary(TEXT("coredll.dll"));
			if (v_hCoreDLL) {
				v_pfn_wprintf = (PFN_wprintf)GetProcAddress(v_hCoreDLL, TEXT("wprintf"));
			}
			
		}
		if (v_pfn_wprintf != NULL) {
			(v_pfn_wprintf) (TEXT("%s"), Buffer);
		} else {
			// Couldn't find the entry point, revert to OutputDebugString()
			g_bOutputToConsole = FALSE;
		}
    }
	else
	{
		OutputDebugString(&Buffer[0]);
		OutputDebugString(L"\n");
	}

	va_end(ArgList);
}

void
Usage()
{
	Print(TEXT("Usage: pppconfig [-d] [-v]  [Parameters]\n"));
	Print(TEXT("Parameters:"));
	Print(TEXT("            enable"));
	Print(TEXT("            disable"));
	Print(TEXT("            status"));
	Print(TEXT("            user    add     <userName> [<domainName>] <password>"));
	Print(TEXT("            user    remove  <userName> [<domainName>]"));
	Print(TEXT("            ras     set     authmode   [STANDARD|BYPASS]"));
	Print(TEXT("            ras     set     authmask   <mask as defined in ras.h>"));
	Print(TEXT("            dhcp    [on|off]"));
	Print(TEXT("            dhcpif  <interface name>"));
	Print(TEXT("            autoip  [on | off]"));
	Print(TEXT("            autonet <subnet> <mask>"));
	Print(TEXT("            ipv6    <prefix>/<bitlen> <count>"));
	Print(TEXT("            line    enum"));
	Print(TEXT("            line    addallvpn"));
	Print(TEXT("            line    add     <lineName or #>"));
	Print(TEXT("            line    remove  <lineName or #>"));
	Print(TEXT("            line    enable  <lineName or #>"));
	Print(TEXT("            line    disable <lineName or #>"));
	Print(TEXT("            line    timeout <lineName or #> <timeout seconds>"));
	Print(TEXT("            line    status"));
	Print(TEXT("            line    conninfo <lineName or #>"));
	Print(TEXT("            line    config  <lineName or #>"));
	Print(TEXT("            line    config  <lineName or #> set baudrate <baudrate>"));
	Print(TEXT("            line    drop    <lineName or #>"));
	Print(TEXT("\n"));
}

typedef
void (*CommandHandler)(
	int		 argc,
	TCHAR	*argv[]);

typedef
void (*LineConfigCommandHandler)(
	int		 argc,
	TCHAR	*argv[],
	RASCNTL_SERVERLINE *pLine);

typedef struct
{
	TCHAR			*szName;
	CommandHandler	 pHandler;
	int				 minParms;
	int				 maxParms;
} CommandInfo;

typedef struct
{
	TCHAR			*szName;
	LineConfigCommandHandler	 pHandler;
	int				 minParms;
	int				 maxParms;
} LineConfigCommandInfo;

BOOL
CommandFindAndExecute(
	CommandInfo *pCommandTable,
	int			 argc,
	TCHAR		*argv[])
//
//	Search for a command matching argv[0], then execute it.
//	Return TRUE if a matching command is found and executed, FALSE otherwise.
//
{
	ASSERT(argc >= 1);

	for ( ; pCommandTable->szName != NULL; pCommandTable++)
	{
		// Check for a matching entry, same name and right number of parameters
		if (_tcscmp(argv[0], pCommandTable->szName) == 0
		&&  pCommandTable->minParms <= (argc - 1) && (argc - 1) <= pCommandTable->maxParms)
		{
			pCommandTable->pHandler(argc - 1, argv + 1);
			return TRUE;
		}
	}

	Usage();
	return FALSE;
}

#define IPADDR(x) ((x>>24) & 0x000000ff), ((x>>16) & 0x000000ff), ((x>>8) & 0x000000ff),  (x & 0x000000ff)

void
DisplayServerStatus()
//
//	Display current PPP server state
//
{
	RASCNTL_SERVERSTATUS *pStatus;
	RASCNTL_SERVERLINE   *pLine;
	DWORD				 cbStatus,
						 dwResult,
						 iLine;

	//
	//	First pass is to get the size
	//
	dwResult = RasIOControl(0, RASCNTL_SERVER_GET_STATUS, 0, 0, NULL, 0, &cbStatus);

	pStatus = malloc(cbStatus);
	if (pStatus)
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_GET_STATUS, 0, 0, (PUCHAR)pStatus, cbStatus, &cbStatus);

		if (dwResult == 0)
		{
			Print(TEXT("Ras Server Status:"));
			Print(TEXT("\tbEnable             = %d"), pStatus->bEnable);
			Print(TEXT("\tFlags               = %x"), pStatus->bmFlags);
			Print(TEXT("\tUseDhcpAddresses    = %d"),  pStatus->bUseDhcpAddresses);
			Print(TEXT("\tDhcpInterface       = %s"),  pStatus->wszDhcpInterface);
			Print(TEXT("\tUseAutoIPAddresses  = %d"),  pStatus->bUseAutoIpAddresses);
			Print(TEXT("\tAutoIPSubnet        = %u.%u.%u.%u mask=%u.%u.%u.%u"),  IPADDR(pStatus->dwAutoIpSubnet), IPADDR(pStatus->dwAutoIpSubnetMask));
			Print(TEXT("\tStaticIpAddrStart   = %u.%u.%u.%u"),  IPADDR(pStatus->dwStaticIpAddrStart));
			Print(TEXT("\tStaticIpAddrCount   = %u"),  pStatus->dwStaticIpAddrCount);
			Print(TEXT("\tAuthenticationMethods = %x"),  pStatus->bmAuthenticationMethods);
			Print(TEXT("\tNumLines = %u"), pStatus->dwNumLines);

			//
			// Per line info is in an array immediately following the global server status struct
			//
			pLine = (RASCNTL_SERVERLINE *)(&pStatus[1]);

			Print(TEXT("Line Devices:"));
			Print(TEXT("Type   Flags IdleTO Name"));
			Print(TEXT("------ ----- ------ ----"));
			for (iLine = 0; iLine < pStatus->dwNumLines; iLine++, pLine++)
			{
				Print(TEXT("%6s %5x %6d %s %s"), pLine->rasDevInfo.szDeviceType, pLine->bmFlags, pLine->DisconnectIdleSeconds, pLine->rasDevInfo.szDeviceName, pLine->bEnable?TEXT("Enabled"):TEXT("Disabled"));
			}
		}
		else
		{
			Print(TEXT("RasIOControl failed, error = %d"), dwResult);
		}
		
		free(pStatus);
	}
	else
	{
		Print(TEXT("Unable to allocate %d bytes of memory for status info"), cbStatus);
	}
}

DWORD
GetServerParameters(
	OUT	RASCNTL_SERVERSTATUS	*pParameters)
//
//	Get current server global parameter settings
//
{
	DWORD	dwResult,
			cbParameters;

	dwResult = RasIOControl(0, RASCNTL_SERVER_GET_PARAMETERS, 0, 0, (PUCHAR)pParameters, sizeof(*pParameters), &cbParameters);
	if (dwResult != 0)
	{
		Print(TEXT("Unable to Get Server Parameters, error=%d"), dwResult);
	}
	return dwResult;
}

DWORD
SetServerParameters(
	IN	RASCNTL_SERVERSTATUS	*pParameters)
//
//	Set current server global parameter settings
//
{
	DWORD	dwResult;

	dwResult = RasIOControl(0, RASCNTL_SERVER_SET_PARAMETERS, (PUCHAR)pParameters, sizeof(*pParameters), NULL, 0, NULL);
	if (dwResult != 0)
	{
		Print(TEXT("Unable to Set Server Parameters, error=%d"), dwResult);
	}
	return dwResult;
}

DWORD
SetDhcpEnable(
	IN	BOOLEAN	bEnable)
//
//	Enable/disable the use of DHCP addresses
//
{
	RASCNTL_SERVERSTATUS	Parameters;
	DWORD	dwResult;

	dwResult = GetServerParameters(&Parameters);
	if (dwResult == 0)
	{
		Parameters.bUseDhcpAddresses = bEnable;
		dwResult = SetServerParameters(&Parameters);
	}
	return dwResult;
}

void
DhcpCommand(
	int argc,
	TCHAR *argv[])
//
//	Enable/disable the use of DHCP addresses
//
{
	DWORD	dwResult;
	BOOLEAN	bEnable;

	bEnable = FALSE;
	if (_tcscmp(argv[0], TEXT("on")) == 0)
		bEnable = TRUE;

	dwResult = SetDhcpEnable(bEnable);

	if (dwResult == 0)
	{
		if (bEnable)
			Print(TEXT("DHCP is now ON"));
		else
			Print(TEXT("DHCP is now OFF"));
	}
	else
		Print(TEXT("Failed to enable/disable DHCP, error = %u"), dwResult);
}

void
DhcpIfCommand(
	int argc,
	TCHAR *argv[])
//
//	Set the DHCP Interface
//
{
	RASCNTL_SERVERSTATUS	Parameters;
	DWORD	dwResult;

	dwResult = GetServerParameters(&Parameters);
	if (dwResult == 0)
	{
		wcscpy(&Parameters.wszDhcpInterface[0], argv[0]);
		dwResult = SetServerParameters(&Parameters);
	}

	if (dwResult == 0)
	{
		Print(TEXT("DHCP interface is now argv[0]"));
	}
}

DWORD
SetAutoIPEnable(
	IN	BOOLEAN	bEnable)
//
//	Enable/disable the use of AutoIP addresses
//
{
	RASCNTL_SERVERSTATUS	Parameters;
	DWORD	dwResult;

	dwResult = GetServerParameters(&Parameters);
	if (dwResult == 0)
	{
		Parameters.bUseAutoIpAddresses = bEnable;
		dwResult = SetServerParameters(&Parameters);
	}

	return dwResult;
}

void
AutoIPCommand(
	int argc,
	TCHAR *argv[])
//
//	Set the subnet to assign autoip addresses from.
//
{
	DWORD	dwResult;
	BOOLEAN	bEnable;

	bEnable = FALSE;
	if (_tcscmp(argv[0], TEXT("on")) == 0)
		bEnable = TRUE;

	dwResult = SetAutoIPEnable(bEnable);
	if (dwResult == 0)
	{
		if (bEnable)
			Print(TEXT("AutoIP is now ON"));
		else
			Print(TEXT("AutoIP is now OFF"));
	}
}

void
IPV6Command(
	int argc,
	TCHAR *argv[])
//
//	Set the IPV6 network prefix pool to assign for IPV6 connections.
//
{
	DWORD	                       dwResult;
	RASCNTL_SERVER_IPV6_NET_PREFIX prefix;
	PWSTR                          pwszLength,
								   pwszPrefix;
	DWORD                          bitlength;
	int							   value;
	DWORD                          bitoffset;
	WCHAR                          wc, wbuf[64];

	if (argc == 0)
	{
		// Display current settings
		dwResult = RasIOControl(0, RASCNTL_SERVER_GET_IPV6_NET_PREFIX, NULL, 0, (PUCHAR)&prefix, sizeof(prefix), NULL);

		if (dwResult != 0)
		{
			Print(TEXT("RASCNTL_SERVER_GET_IPV6_NET_PREFIX IOCTL failed error=%d"), dwResult);
		}
		else
		{
			bitlength = prefix.IPV6NetPrefixBitLength;
			bitoffset = 0;
			while (bitoffset < bitlength)
			{
				swprintf(&wbuf[bitoffset / 4], L"%02X", prefix.IPV6NetPrefix[bitoffset / 8]);
				bitoffset += 8;
			}
			wbuf[bitoffset / 4] = L'\0';
			Print(TEXT("%s/%u count=%u"), wbuf, bitlength, prefix.IPV6NetPrefixCount);
		}
	}
	else
	{
		// Set new IPV6 network prefix pool
		prefix.IPV6NetPrefixCount = 1; // default
		if (argc == 2)
			(void)swscanf(argv[1], L"%u", &prefix.IPV6NetPrefixCount);

		prefix.IPV6NetPrefixBitLength = 64;
		pwszLength = wcschr(argv[0], L'/');
		if (pwszLength)
		{
			(void)swscanf(pwszLength + 1, L"%u", &prefix.IPV6NetPrefixBitLength);
		}

		bitoffset = 0;
		memset(prefix.IPV6NetPrefix, 0, sizeof(prefix.IPV6NetPrefix));
		pwszPrefix = argv[0];
		while (TRUE)
		{
			value = -1;
			wc = *pwszPrefix++;
			if (wc == 0 || wc == L'/')
				break;
			if (L'0' <= wc && wc <= '9')
				value = wc - L'0';
			else if (L'A' <= wc && wc <= 'F')
				value = 10 + wc - L'A';
			else if (L'a' <= wc && wc <= 'f')
				value = 10 + wc - L'a';

			if (value != -1)
			{
				prefix.IPV6NetPrefix[bitoffset / 8] |= value << ((bitoffset + 4) % 8);
				bitoffset += 4;
			}
		}

		dwResult = RasIOControl(0, RASCNTL_SERVER_SET_IPV6_NET_PREFIX, (PUCHAR)&prefix, sizeof(prefix), NULL, 0, NULL);

		if (dwResult != 0)
		{
			Print(TEXT("RASCNTL_SERVER_SET_IPV6_NET_PREFIX IOCTL failed error=%d"), dwResult);
		}
		else
		{
			Print(TEXT("Set IPv6 Net Prefix"));
		}
	}
}


DWORD
SetAutoNet(
	IN	DWORD	dwSubnet,
	IN	DWORD	dwSubnetMask)
//
//	
//
{
	RASCNTL_SERVERSTATUS	Parameters;
	DWORD	dwResult;

	dwResult = GetServerParameters(&Parameters);
	if (dwResult == 0)
	{
		Parameters.dwAutoIpSubnet = dwSubnet;
		Parameters.dwAutoIpSubnetMask = dwSubnetMask;
		dwResult = SetServerParameters(&Parameters);
	}

	if (dwResult == 0)
	{
		Print(TEXT("AutoIPSubnet set successfully"));
	}

	return dwResult;
}

void
AutoNetCommand(
	int argc,
	TCHAR *argv[])
//
//	Enable/disable the use of DHCP addresses
//
{
	DWORD	dwResult;
	DWORD	dwSubnet;
	DWORD	dwSubnetMask;

	(void)swscanf(argv[0], L"%x", &dwSubnet); 
	(void)swscanf(argv[1], L"%x", &dwSubnetMask); 
	dwResult = SetAutoNet(dwSubnet, dwSubnetMask);
}

DWORD
GetLineDevices(
	OUT	LPRASDEVINFOW	*ppRasDevInfo,
	OUT	PDWORD			 pcDevices)
//
//	Get an array of all the RAS line devices in the system
//
{
	DWORD			cbBuffer,
					dwResult;

	cbBuffer = 0;
	*pcDevices = 0;

	dwResult = RasEnumDevices(NULL, &cbBuffer, pcDevices);
	*ppRasDevInfo = malloc(cbBuffer);

	if (*ppRasDevInfo == NULL)
	{
		Print(TEXT("Unable to allocate %d bytes of memory to store device info"), cbBuffer);
		dwResult = ERROR;
	}
	else
	{
		(*ppRasDevInfo)->dwSize = sizeof(RASDEVINFO);
		dwResult = RasEnumDevices(*ppRasDevInfo, &cbBuffer, pcDevices);
	}

	return dwResult;
}

DWORD
GetDeviceInfoFromIndex(
	IN	DWORD		iDevice,
	OUT	RASDEVINFO *DeviceInfo)
{
	LPRASDEVINFOW	pRasDevInfo;
	DWORD			cDevices,
					dwResult;

	dwResult = GetLineDevices(&pRasDevInfo, &cDevices);

	if (dwResult == 0)
	{
		if (iDevice < cDevices)
		{
			*DeviceInfo = pRasDevInfo[iDevice];
		}
		else
		{
			Print(TEXT("Device index %u >= number of devices %d"), iDevice, cDevices);
			dwResult = ERROR;
		}
	}

	free(pRasDevInfo);

	return dwResult;
}

DWORD
GetDeviceInfoFromName(
	IN	PTCHAR		ptszDeviceName,
	OUT	RASDEVINFO *DeviceInfo)
{
	LPRASDEVINFOW	pRasDevInfo;
	DWORD			cDevices,
					dwResult,
					iDevice;

	dwResult = GetLineDevices(&pRasDevInfo, &cDevices);

	if (dwResult == 0)
	{
		dwResult = ERROR;
		for (iDevice = 0; iDevice < cDevices; iDevice++)
		{
			if (_tcscmp(pRasDevInfo[iDevice].szDeviceName, ptszDeviceName) == 0)
			{
				*DeviceInfo = pRasDevInfo[iDevice];
				dwResult = STATUS_SUCCESS;
				break;
			}
		}
		if (dwResult == ERROR)
		{
			Print(TEXT("Device '%s' not found"), ptszDeviceName);
		}
	}

	free(pRasDevInfo);

	return dwResult;
}

DWORD
GetDeviceInfoFromCommand(
	IN	PTCHAR		ptszCmdLine,
	OUT	RASDEVINFO *DeviceInfo)
//
//	Command line parameters identifying devices may be of the form:
//		<device name>
//		<device number>
//
//	This function handles all methods to get the device info.
//
{
	DWORD dwResult;

	if (TEXT('0' <= *ptszCmdLine && *ptszCmdLine <= '9'))
	{
		dwResult = GetDeviceInfoFromIndex(_ttoi(ptszCmdLine), DeviceInfo);
	}
	else
	{
		dwResult = GetDeviceInfoFromName(ptszCmdLine, DeviceInfo);
	}

	return dwResult;
}


void
LineEnumCommand(
	int argc,
	TCHAR *argv[])
//
//	List all the RAS capable line devices present on the system
//
{
	LPRASDEVINFOW	pRasDevInfo;
	DWORD			cDevices,
					dwResult,
					iDevice;

	dwResult = GetLineDevices(&pRasDevInfo, &cDevices);

	if (dwResult == 0)
	{
		Print(TEXT("Line Devices:"));
		Print(TEXT("##  Type     Name"));
		Print(TEXT("-- ------   ----"));
		for (iDevice = 0; iDevice < cDevices; iDevice++)
		{
			Print(TEXT("%2d %6s   %s"), iDevice, pRasDevInfo[iDevice].szDeviceType, pRasDevInfo[iDevice].szDeviceName);
		}
	}
	else
	{
		Print(TEXT("RasIOControl failed, error = %d"), dwResult);
	}
	free(pRasDevInfo);
}

DWORD
BasicLineIoctlCommand(
	int		argc,
	TCHAR	*argv[],
	DWORD	dwIoctl)
{
	RASCNTL_SERVERLINE	Line;
	DWORD				dwResult;

	dwResult = GetDeviceInfoFromCommand(argv[0], &Line.rasDevInfo);
	if (dwResult == 0)
	{
		dwResult = RasIOControl(0, dwIoctl, (PUCHAR)&Line, sizeof(Line), NULL, 0, NULL);
		if (dwResult != 0)
			Print(TEXT("RasIOControl %d failed, error = %d"), dwIoctl, dwResult);
	}

	return dwResult;
}

#define STRINGCASE(x) case x: return #x

PSTR
RasConnStateName(
	IN RASCONNSTATE state)
{
	switch(state)
	{
		STRINGCASE(RASCS_OpenPort);
		STRINGCASE(RASCS_PortOpened);
		STRINGCASE(RASCS_ConnectDevice);
		STRINGCASE(RASCS_DeviceConnected);
		STRINGCASE(RASCS_AllDevicesConnected);
		STRINGCASE(RASCS_Authenticate);
		STRINGCASE(RASCS_Authenticated);
		STRINGCASE(RASCS_Connected);
		STRINGCASE(RASCS_Disconnected);
		default: return "???";
	}
}

void
LineConnInfoCommand(
	int		argc,
	TCHAR	*argv[])
{
	RASCNTL_SERVERLINE	     Line;
	RASCNTL_SERVERCONNECTION Connection;
	RAS_STATS                stats;
	DWORD				     dwResult;

	dwResult = GetDeviceInfoFromCommand(argv[0], &Line.rasDevInfo);
	if (dwResult == 0)
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_LINE_GET_CONNECTION_INFO, (PUCHAR)&Line, sizeof(Line), (PBYTE)&Connection, sizeof(Connection), NULL);
		if (dwResult != 0)
		{
			Print(TEXT("RasIOControl RASCNTL_SERVER_LINE_GET_CONNECTION_INFO failed, error = %d"), dwResult);
		}
		else
		{
			Print(L"Line Name %s Type %s", Connection.rasDevInfo.szDeviceName, Connection.rasDevInfo.szDeviceType);
			Print(L"\tState=%hs", RasConnStateName(Connection.RasConnState));
			Print(L"\tIPAddr Server=%u.%u.%u.%u Client=%u.%u.%u.%u",  IPADDR(Connection.dwServerIpAddress), IPADDR(Connection.dwClientIpAddress));
			Print(L"\tUser=%s",  Connection.tszUserName);

			if (Connection.hrasconn)
			{
				stats.dwSize = sizeof(stats);
				dwResult = RasGetLinkStatistics(Connection.hrasconn, 0, &stats);
				if (dwResult != 0)
				{
					Print(TEXT("RasGetLinkStatistics %s failed, error = %d"), Connection.hrasconn, dwResult);
				}
				else
				{
					Print(L"\tFrames/Bytes Tx=%u/%u Rx=%u/%u  Bps=%u", 
						stats.dwFramesXmited, stats.dwBytesXmited, stats.dwFramesRcved, stats.dwBytesRcved, stats.dwBps);
				}
			}
		}
	}
}


void
LineStatusCommand(
	int argc,
	TCHAR *argv[])
//
// List All Server Line Status
//
{
	RASCNTL_SERVERSTATUS *pStatus;
	RASCNTL_SERVERLINE   *pLine;
	DWORD				 cbStatus,
						 dwResult,
						 iLine;

	//
	//	First pass is to get the size
	//
	dwResult = RasIOControl(0, RASCNTL_SERVER_GET_STATUS, 0, 0, NULL, 0, &cbStatus);

	pStatus = malloc(cbStatus);
	if (pStatus)
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_GET_STATUS, 0, 0, (PUCHAR)pStatus, cbStatus, &cbStatus);

		if (dwResult == 0)
		{
			//
			// Per line info is in an array immediately following the global server status struct
			//
			pLine = (RASCNTL_SERVERLINE *)(&pStatus[1]);

			Print(TEXT("Line Devices:"));
			Print(TEXT("Type   Flags IdleTO Name           Status"));
			Print(TEXT("------ ----- ------ -------------- ------"));
			for (iLine = 0; iLine < pStatus->dwNumLines; iLine++, pLine++)
			{
				Print(TEXT("%6s %5x %6d %s %s"), pLine->rasDevInfo.szDeviceType, pLine->bmFlags, pLine->DisconnectIdleSeconds, pLine->rasDevInfo.szDeviceName, pLine->bEnable?TEXT("Enabled"):TEXT("Disabled"));
			}
		}
		else
		{
			Print(TEXT("RasIOControl failed, error = %d"), dwResult);
		}
		
		free(pStatus);
	}
	else
	{
		Print(TEXT("Unable to allocate %d bytes of memory for status info"), cbStatus);
	}
}

void
LineAddCommand(
	int argc,
	TCHAR *argv[])
//
//	Add a new line to those capable of being managed by the ras server
//
{
	DWORD dwResult;

	dwResult = BasicLineIoctlCommand(argc, argv, RASCNTL_SERVER_LINE_ADD);
	if (dwResult == 0)
	{
		Print(TEXT("Line added successfully"));
	}
}

DWORD
AddEnableLine(
	RASDEVINFO RasDevInfo)
//
// Add/Enable RAS line
//
{
	RASCNTL_SERVERLINE	Line;
	DWORD				dwResult;

	memcpy(&Line.rasDevInfo, &RasDevInfo, sizeof(RASDEVINFO));
	
	dwResult = RasIOControl(0, RASCNTL_SERVER_LINE_ADD, (PUCHAR)&Line, sizeof(Line), NULL, 0, NULL);
	if (dwResult != 0)
	{
		Print(TEXT("RasIOControl (RASCNTL_SERVER_LINE_ADD) failed, error = %d"), dwResult);
		return dwResult;
	}

	dwResult = RasIOControl(0, RASCNTL_SERVER_LINE_ENABLE, (PUCHAR)&Line, sizeof(Line), NULL, 0, NULL);
	if (dwResult != 0)
	{
		Print(TEXT("RasIOControl (RASCNTL_SERVER_LINE_ENABLE) failed, error = %d"), dwResult);
		return dwResult;
	}

	return dwResult;
}

void
LineAddAllVPNCommand(
	int argc,
	TCHAR *argv[])
//
// Add/Enable all VPN lines to the VPN server
//
{
	LPRASDEVINFOW	pRasDevInfo;
	DWORD			cDevices,
					dwResult,
					iDevice;

	dwResult = GetLineDevices(&pRasDevInfo, &cDevices);

	if (dwResult == 0)
	{
		Print(TEXT("Added/Enabled the following lines:"));
		Print(TEXT("##  Type     Name"));
		Print(TEXT("-- ------   ----"));
		for (iDevice = 0; iDevice < cDevices; iDevice++)
		{
			if(!wcsncmp(pRasDevInfo[iDevice].szDeviceType, RASDT_Vpn, wcslen(RASDT_Vpn)))
			{
				if (AddEnableLine(pRasDevInfo[iDevice]))
				{
					Print(TEXT("Failed to enable line (%s)"), pRasDevInfo[iDevice].szDeviceName);
				}
				else 
				{
					Print(TEXT("%2d %6s   %s"), iDevice, pRasDevInfo[iDevice].szDeviceType, pRasDevInfo[iDevice].szDeviceName);
				}
			}
		}
	}
	else
	{
		Print(TEXT("RasIOControl failed, error = %d"), dwResult);
	}
	free(pRasDevInfo);
}

void
LineRemoveCommand(
	int argc,
	TCHAR *argv[])
//
//	Remove a line from those capable of being managed by the ras server
//
{
	DWORD dwResult;

	dwResult = BasicLineIoctlCommand(argc, argv, RASCNTL_SERVER_LINE_REMOVE);
	if (dwResult == 0)
	{
		Print(TEXT("Line removed successfully"));
	}

}

void
LineEnableCommand(
	int argc,
	TCHAR *argv[])
//
//	Enable a line, that is, start listening for connections
//
{
	DWORD dwResult;

	dwResult = BasicLineIoctlCommand(argc, argv, RASCNTL_SERVER_LINE_ENABLE);
	if (dwResult == 0)
	{
		Print(TEXT("Line enabled successfully"));
	}
}

void
LineDisableCommand(
	int argc,
	TCHAR *argv[])
//
//	Disable a line, stop listening for connections
//
{
	DWORD dwResult;

	dwResult = BasicLineIoctlCommand(argc, argv, RASCNTL_SERVER_LINE_DISABLE);
	if (dwResult == 0)
	{
		Print(TEXT("Line disabled successfully"));
	}
}

//
//	Device configuration support functions
//
DWORD
LineDeviceConfigure(
	IN		RASDEVINFO	*DeviceInfo,
	IN	OUT	PBYTE		*ppDevConfig,
	IN	OUT	PDWORD		pDevConfigSize)
{
	DWORD		dwNeededSize;
	LPVARSTRING	pVarString;
	HWND		hDlg = (HWND)NULL;
	DWORD		RetVal;
	PUCHAR		pNewDevConfig;
	
	dwNeededSize = sizeof(VARSTRING);
	do
	{
		pVarString = (LPVARSTRING)LocalAlloc (LPTR, dwNeededSize);
		pVarString->dwTotalSize = dwNeededSize;

		RetVal = RasDevConfigDialogEdit (DeviceInfo->szDeviceName,
										 DeviceInfo->szDeviceType,
										 hDlg, 
										 *ppDevConfig, 
										 *pDevConfigSize,
										 pVarString);
		if (STATUS_SUCCESS != RetVal)
		{
			Print(TEXT("ERROR 0x%X(%d) from RasDevConfigDialogEdit\n"), RetVal, RetVal);
		}
		else if (pVarString->dwNeededSize > pVarString->dwTotalSize)
		{
			// Structure not large enough.  Get new size and free original
			dwNeededSize = pVarString->dwNeededSize;
		}
		else // Sucessfully retrieved the dev config
		{
			//
			//	Only need to reallocate the device info storage space
			//  if the size of the new dev config is different than the old.
			//
			if (pVarString->dwStringSize != *pDevConfigSize)
			{
				pNewDevConfig = (PBYTE)LocalAlloc (LMEM_FIXED, pVarString->dwStringSize);
				if (pNewDevConfig)
				{
					// Free the original
					if (*ppDevConfig)
						LocalFree (*ppDevConfig);

					*ppDevConfig = pNewDevConfig;
					*pDevConfigSize = pVarString->dwStringSize;
				}
				else
				{
					Print (TEXT("ERROR - Unable to allocate %d bytes for Line devconfig\n"), pVarString->dwStringSize);
					RetVal = STATUS_NO_MEMORY;

					// Don't copy any bytes
					pVarString->dwStringSize = 0;
				}
			}

			//
			//	Copy new devconfig info
			//
			memcpy (*ppDevConfig, (LPBYTE)pVarString + pVarString->dwStringOffset, pVarString->dwStringSize);
			dwNeededSize = 0;
		}

		LocalFree (pVarString);

	} while (RetVal == STATUS_SUCCESS && dwNeededSize);

	return RetVal;
}

DWORD
LineGetParameters(
	IN	RASCNTL_SERVERLINE	*pLine,
	OUT RASCNTL_SERVERLINE  **ppOutBuf,
	OUT DWORD				*pdwOutBufSize)
{
	DWORD dwResult = 0;
	PRASCNTL_SERVERLINE	 pOutBuf = NULL;
	DWORD dwOutBufSize = 0,
		  dwNeededSize;

	do
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_LINE_GET_PARAMETERS, (PUCHAR)pLine, sizeof(*pLine), (PUCHAR)pOutBuf, dwOutBufSize, &dwNeededSize);
		if (dwResult != ERROR_BUFFER_TOO_SMALL)
			break;

		// Free old buffer
		if (pOutBuf)
			LocalFree(pOutBuf);

		// Allocate new buffer
		pOutBuf = (PRASCNTL_SERVERLINE)LocalAlloc (LPTR, dwNeededSize);
		if (pOutBuf == NULL)
		{
			dwResult = ERROR_OUTOFMEMORY;
			break;
		}
		dwOutBufSize = dwNeededSize;

	} while (TRUE);

	*ppOutBuf = pOutBuf;
	*pdwOutBufSize = dwOutBufSize;

	return dwResult;
}

DWORD
LineGetDevConfig(
	IN	RASCNTL_SERVERLINE	*pLine,
	OUT	PBYTE				*ppDevConfig,
	OUT	DWORD				*pdwDevConfigSize)
{
	DWORD					dwResult;
	PRASCNTL_SERVERLINE		pOutBuf;
	DWORD					dwOutBufSize;

	dwResult = LineGetParameters(pLine, &pOutBuf, &dwOutBufSize);

	if (dwResult == STATUS_SUCCESS)
	{
		*ppDevConfig = LocalAlloc (LPTR, pOutBuf->dwDevConfigSize);
		if (*ppDevConfig)
		{
			*pdwDevConfigSize = pOutBuf->dwDevConfigSize;
			memcpy (*ppDevConfig, &pOutBuf->DevConfig[0], pOutBuf->dwDevConfigSize);
		}
		else
			dwResult = ERROR_OUTOFMEMORY;
	}

	if (pOutBuf)
		LocalFree(pOutBuf);

	return dwResult;
}

DWORD
LineSetDevConfig(
	IN	RASCNTL_SERVERLINE	*pLine,
	IN	PBYTE				pDevConfig,
	IN	DWORD				dwDevConfigSize)
{
	DWORD					dwResult = 0;
	PRASCNTL_SERVERLINE		pInBuf,
							pOutBuf;
	DWORD					dwInBufSize,
							dwOutBufSize;

	//
	//	Get the current parameter settings
	//
	dwResult = LineGetParameters(pLine, &pOutBuf, &dwOutBufSize);

	if (dwResult == STATUS_SUCCESS)
	{
		//
		//	Allocate space to hold the new parameter settings
		//
		dwInBufSize = offsetof(RASCNTL_SERVERLINE, DevConfig) + dwDevConfigSize;
		pInBuf = LocalAlloc (LPTR, dwInBufSize);
		if (pInBuf)
		{
			//
			// Copy the current settings in
			//
			memcpy(pInBuf, pOutBuf, offsetof(RASCNTL_SERVERLINE, DevConfig));

			//
			//	Copy the new dev config
			//
			memcpy(&pInBuf->DevConfig[0], pDevConfig, dwDevConfigSize);
			pInBuf->dwDevConfigSize = dwDevConfigSize;
			dwResult = RasIOControl(0, RASCNTL_SERVER_LINE_SET_PARAMETERS, (PUCHAR)pInBuf, dwInBufSize, NULL, 0, NULL);

			LocalFree(pInBuf);
		}
		else
			dwResult = ERROR_OUTOFMEMORY;
	}

	if (pOutBuf)
		LocalFree(pOutBuf);

	return dwResult;
}

//
//Description   : Call back function used by TAPI.
//
void 
CALLBACK 
lineCallbackFunc(
	IN	DWORD	dwDevice, 
	IN	DWORD	dwMsg,
	IN	DWORD	dwCallbackInstance, 
	IN	DWORD	dwParam1, 
	IN	DWORD	dwParam2, 
	IN	DWORD	dwParam3
	)
{
	return;
}

void
LineConfigSetBaudrateCommand(
	int argc,
	TCHAR *argv[],
	RASCNTL_SERVERLINE *pLine)
{
	DWORD				dwResult;
	PBYTE				pDevConfig;
	DWORD				dwDevConfigSize;
	BYTE				RasDevInfoBuffer[1024];				/* buffer that receives device-specific configuration */ 
	HLINEAPP			hLineApp;													   /* Line application handle */
	HLINE				hLine;
	DWORD				dwNumofTAPIDevices = 0;
	DWORD				dwDeviceNumber = 0;
	LINEEXTENSIONID		ExtensionID;								   /* Dummy structure - needed for NT */
	DWORD				dwAPIVersion=0;
	BYTE				DevCapsBuffer[1024];										   /* Buffer for "lineGetDevCaps" */
	LPLINEDEVCAPS		lpDevCaps = (LPLINEDEVCAPS)&DevCapsBuffer[0];
	LPVARSTRING			lpDevConfig = (LPVARSTRING)&RasDevInfoBuffer[0];
    UNIMDM_CHG_DEVCFG	UCD;				// Unimodem configuration parameter structure
	LONG				lRet;

	// Get current device config
	dwResult = LineGetDevConfig(pLine, &pDevConfig, &dwDevConfigSize);

	if (dwResult == STATUS_SUCCESS)
	{
		Print(TEXT("Get device config info from TAPI"));

		//Get device config info.
		dwResult = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("PPPCONFIG"), &dwNumofTAPIDevices);
		if (dwResult != 0)
		{
			Print(TEXT("Error from \"lineInitialize\": %d"), dwResult);
			return;
		}

		for (dwDeviceNumber = 0; dwDeviceNumber < dwNumofTAPIDevices; dwDeviceNumber++)
		{
			if (dwResult = lineNegotiateAPIVersion(hLineApp, dwDeviceNumber, 0x00010000,
				0x00020001, &dwAPIVersion, &ExtensionID))
			{
				Print(TEXT("Error %d from \"lineNegotiateAPIVersion\" for device : %d"), 
					dwResult, dwDeviceNumber);
				continue;
			}
				
			lpDevCaps->dwTotalSize = sizeof (DevCapsBuffer);
			if (dwResult = lineGetDevCaps(hLineApp, dwDeviceNumber, dwAPIVersion, 0, lpDevCaps))
			{
				Print(TEXT("Error %d from \"lineGetDevCaps\" for device : %d"),
					dwResult, dwDeviceNumber);
				continue;
			}

			if (_tcscmp((LPTSTR)(DevCapsBuffer + lpDevCaps->dwLineNameOffset), pLine->rasDevInfo.szDeviceName) == 0)
			{
				//This is the device we want to set baudrate.
				if (_tcsicmp((LPTSTR)(DevCapsBuffer + lpDevCaps->dwProviderInfoOffset), TEXT("UNIMODEM")) != 0)
				{
					Print(TEXT("TAPI device %lu isn't a unimodem device, SetBaudrate ignored."),
						dwDeviceNumber);
					return;
				}

				lpDevConfig->dwTotalSize = sizeof (RasDevInfoBuffer);
				if (dwResult = lineGetDevConfig(dwDeviceNumber, lpDevConfig, TEXT("comm/datamodem")))
				{
						Print(TEXT("Error %d from \"lineGetDevConfig\" for device : %d"), 
							dwResult, dwDeviceNumber);
						return;
				}

				//Break since device already found.
				break;
			}
		}

		if (dwDeviceNumber == dwNumofTAPIDevices)
		{
			//No device found with name pLine->rasDevInfo.szDeviceName.
			Print(TEXT("No TAPI device found with name %s, SetBaudrate ingored."), 
				pLine->rasDevInfo.szDeviceName);
			return;
		}

		dwDevConfigSize = lpDevConfig->dwStringSize;
		pDevConfig = RasDevInfoBuffer + lpDevConfig->dwStringOffset;
		//memcpy(lpDevConfig, RasDevInfoBuffer + lpDevConfig->dwStringOffset, dwDevInfoSize);

		// modify baudrate
		if (lRet = lineOpen(hLineApp, dwDeviceNumber, &hLine, dwAPIVersion, 0, 0,
		  LINECALLPRIVILEGE_NONE, LINEMEDIAMODE_DATAMODEM, NULL))
		{
			Print(TEXT("Couldn't open TAPI device %lu: error %d"), dwDeviceNumber, lRet);
			lineShutdown(hLineApp);
			return;
		} 

		// Set up the lineDevSpecific data block:
		memset (&UCD, 0, sizeof (UCD));
		UCD.dwCommand = UNIMDM_CMD_CHG_DEVCFG;
		UCD.lpszDeviceClass = TEXT("comm/datamodem");
		UCD.lpDevConfig = lpDevConfig;
		dwResult = 0;

		// Set the baudrate:
		UCD.dwOption = UNIMDM_OPT_BAUDRATE;
		UCD.dwValue = _ttoi(argv[0]);

		// lineDevSpecific is an asynchronous function; however, there is no need to
		//   wait for the LINE_REPLY callback
		if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
		{
			Print(TEXT("Couldn't set baudrate to %d: error %d"), UCD.dwValue, lRet);
			lineClose(hLine);
			lineShutdown(hLineApp);
			return;
		}

		lineClose(hLine);
		lineShutdown(hLineApp);
		
		// Set new device config
		dwResult = LineSetDevConfig(pLine, pDevConfig, dwDevConfigSize);
		if (dwResult != 0)
		{
			Print(TEXT("LineSetDevConfig failed, error = %d"), dwResult);
		}
		else
		{
			Print(TEXT("Baudrate successfully set to %d "), _ttoi(argv[0]));
		}
	}
	else
	{
		Print(TEXT("LineGetDevConfig failed, error = %d"), dwResult);
	}

	return;
}

LineConfigCommandInfo g_LineConfigSetCommandTable[] =
{
	{ TEXT("baudrate"),		LineConfigSetBaudrateCommand,	1,	1},
	{ NULL,					NULL,							0,	0}
};

BOOL
LineConfigCommandFindAndExecute(
	LineConfigCommandInfo *pCommandTable,
	int			 argc,
	TCHAR		*argv[],
	RASCNTL_SERVERLINE *pLine)
//
//	Search for a command matching argv[0], then execute it.
//	Return TRUE if a matching command is found and executed, FALSE otherwise.
//
{
	ASSERT(argc >= 1);

	for ( ; pCommandTable->szName != NULL; pCommandTable++)
	{
		// Check for a matching entry, same name and right number of parameters
		if (_tcscmp(argv[0], pCommandTable->szName) == 0
		&&  pCommandTable->minParms <= (argc - 1) && (argc - 1) <= pCommandTable->maxParms)
		{
			pCommandTable->pHandler(argc - 1, argv + 1, pLine);
			return TRUE;
		}
	}

	Usage();
	return FALSE;
}

void
LineConfigSetCommand(
	int argc,
	TCHAR *argv[],
	RASCNTL_SERVERLINE *pLine)
{
	(void)LineConfigCommandFindAndExecute(&g_LineConfigSetCommandTable[0], argc, argv, pLine);
}

LineConfigCommandInfo g_LineConfigCommandTable[] =
{
	{ TEXT("set"),		LineConfigSetCommand,		2,	2},
	{ NULL,				NULL,						0,	0}
};

void
LineConfigCommand(
	int argc,
	TCHAR *argv[])
//
//	List all the RAS capable line devices present on the system
//
{
	RASCNTL_SERVERLINE	Line;
	DWORD				dwResult;
	PBYTE				pDevConfig;
	DWORD				dwDevConfigSize;

	dwResult = GetDeviceInfoFromCommand(argv[0], &Line.rasDevInfo);
	if (dwResult == 0)
	{
		// Get current device config
		dwResult = LineGetDevConfig(&Line, &pDevConfig, &dwDevConfigSize);

		if (dwResult == STATUS_SUCCESS)
		{
			if (argc == 1)
			{
				// Allow user to modify device config
				dwResult = LineDeviceConfigure(&Line.rasDevInfo, &pDevConfig, &dwDevConfigSize);
				if (dwResult == STATUS_SUCCESS)
				{
					// Set new device config
					dwResult = LineSetDevConfig(&Line, pDevConfig, dwDevConfigSize);
					if (dwResult != 0)
					{
						Print(TEXT("LineSetDevConfig failed, error = %d"), dwResult);
					}
				}
				else
				{
					Print(TEXT("LineDeviceConfigure failed, error = %d"), dwResult);
				}
			}
			else
			{
				argc--;
				argv++;
				// Search for a command and execute
				(void)LineConfigCommandFindAndExecute(&g_LineConfigCommandTable[0], argc, argv, &Line);
			}
		}
		else
		{
			Print(TEXT("LineGetDevConfig failed, error = %d"), dwResult);
		}
	}
}

CommandInfo g_LineCommandTable[] =
{
	{ TEXT("enum"),		LineEnumCommand,			0,	0},
	{ TEXT("add"),		LineAddCommand,				1,	1},
	{ TEXT("addallvpn"),	LineAddAllVPNCommand,				0,	0},
	{ TEXT("remove"),	LineRemoveCommand,			1,	1},
	{ TEXT("config"),	LineConfigCommand,			1,	4},
	{ TEXT("enable"),	LineEnableCommand,			1,	1},
	{ TEXT("disable"),	LineDisableCommand,			1,	1},
	{ TEXT("status"),	LineStatusCommand,			0,	0},
	{ TEXT("conninfo"),	LineConnInfoCommand,			1,	1},
	{ NULL,				NULL,						0,	0}
};

void
LineCommand(
	int argc,
	TCHAR *argv[])
{
	// Search for a command and execute
	(void)CommandFindAndExecute(&g_LineCommandTable[0], argc, argv);
}

void
UserAddCommand(
	int argc,
	TCHAR *argv[])
//
//	List all the RAS capable line devices present on the system
//
{
	RASCNTL_SERVERUSERCREDENTIALS	Credentials;
	DWORD							dwResult;
	int								iArg;

	memset(&Credentials, 0, sizeof(Credentials));

	// Get user name
	iArg = 0;
	_tcsncpy(Credentials.tszUserName, argv[iArg++], UNLEN+1);

	// Get optional domain name
	Credentials.tszDomainName[0] = TEXT('\0');
	if (argc == 3)
		_tcscpy(Credentials.tszDomainName, argv[iArg++]);

	// Get password
	Credentials.cbPassword = WideCharToMultiByte (CP_OEMCP, 0,
								argv[iArg], wcslen(argv[iArg]),
								Credentials.password, sizeof(Credentials.password), NULL, NULL );

	Print(TEXT("Adding: User:%s Domain:%s Password:%hs %d bytes"), Credentials.tszUserName, Credentials.tszDomainName, Credentials.password, Credentials.cbPassword);

	// Set the credentials
	dwResult = RasIOControl(0, RASCNTL_SERVER_USER_SET_CREDENTIALS, (PUCHAR)&Credentials, sizeof(Credentials), NULL, 0, NULL);
}

void
UserRemoveCommand(
	int argc,
	TCHAR *argv[])
//
//	Remove user from RAS server
//
{
	RASCNTL_SERVERUSERCREDENTIALS	Credentials;
	DWORD							dwResult;

	// Get user name
	_tcsncpy(Credentials.tszUserName, argv[0], UNLEN + 1);

	// Get optional domain name
	Credentials.tszDomainName[0] = TEXT('\0');
	if (argc == 2)
		_tcscpy(Credentials.tszDomainName, argv[1]);

	// Set the credentials
	dwResult = RasIOControl(0, RASCNTL_SERVER_USER_DELETE_CREDENTIALS, (PUCHAR)&Credentials, sizeof(Credentials), NULL, 0, NULL);
	if (dwResult == 0)
	{
		Print(TEXT("User %s successfully removed from domain %s"), Credentials.tszUserName, Credentials.tszDomainName);
	}
}

#if 0
void
UserEnumCommand(
	int argc,
	TCHAR *argv[])

//
//	Enum user from RAS server
//
{
	DWORD i;
	TCHAR pszUser[256]={0};
	DWORD cchUser = sizeof(pszUser)/sizeof(pszUser[0]);

	for ( i=0; NTLMEnumUser( i, pszUser, &cchUser); i++)
		
		{
			Print(TEXT("User Name %i : %s "),i , pszUser);
			cchUser = sizeof(pszUser)/sizeof(pszUser[i+1]);
		}
}
#endif

CommandInfo g_UserCommandTable[] =
{
	{ TEXT("add"),		UserAddCommand,				2,	3},
	{ TEXT("remove"),	UserRemoveCommand,			1,	2},
	{ NULL,				NULL,						0,	0}
};

void
UserCommand(
	int argc,
	TCHAR *argv[])
{
	// Search for a command and execute
	(void)CommandFindAndExecute(&g_UserCommandTable[0], argc, argv);
}

void
RasSetAuthmodeCommand(
	int argc,
	TCHAR *argv[])
{
	RASCNTL_SERVERSTATUS *pStatus;
	DWORD				 cbStatus,
						 dwResult;

	//
	//	First pass is to get the size
	//
	dwResult = RasIOControl(0, RASCNTL_SERVER_GET_PARAMETERS, 0, 0, NULL, 0, &cbStatus);

	pStatus = malloc(cbStatus);
	if (pStatus)
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_GET_PARAMETERS, 0, 0, (PUCHAR)pStatus, cbStatus, &cbStatus);

		if (dwResult == 0)
		{
			if (_tcsicmp(argv[0], TEXT("BYPASS")) == 0)
			{
				pStatus->bmFlags |= PPPSRV_FLAG_ALLOW_UNAUTHENTICATED_ACCESS;
			}
			else
			{
				if (_tcsicmp(argv[0], TEXT("STANDARD")) == 0)
				{
					pStatus->bmFlags &= ~PPPSRV_FLAG_ALLOW_UNAUTHENTICATED_ACCESS;
				}
				else
				{
					Print(TEXT("Unknown option - %s"), argv[0]);
					return;
				}
			}

			dwResult = RasIOControl(0, RASCNTL_SERVER_SET_PARAMETERS, (PUCHAR)pStatus, cbStatus, 0, 0, &cbStatus);

			if (dwResult != 0)
			{
				Print(TEXT("RasIOControl failed, error = %d"), dwResult);
			}
			else
			{
				if (pStatus->bmFlags & PPPSRV_FLAG_ALLOW_UNAUTHENTICATED_ACCESS)
				{
					Print(TEXT("Authentication mode is now BYPASS"));
				}
				else
				{
					Print(TEXT("Authentication mode is now STANDARD"));
				}
			}

		}
		else
		{
			Print(TEXT("RasIOControl failed, error = %d"), dwResult);
		}
	}
	else
	{
		Print(TEXT("Unable to allocate %d bytes of memory for status info"), cbStatus);
	}
}

void
RasSetAuthmaskCommand(
	int argc,
	TCHAR *argv[])
{
	RASCNTL_SERVERSTATUS *pStatus;
	DWORD				 cbStatus,
						 dwResult;

	//
	//	First pass is to get the size
	//
	dwResult = RasIOControl(0, RASCNTL_SERVER_GET_PARAMETERS, 0, 0, NULL, 0, &cbStatus);

	pStatus = malloc(cbStatus);
	if (pStatus)
	{
		dwResult = RasIOControl(0, RASCNTL_SERVER_GET_PARAMETERS, 0, 0, (PUCHAR)pStatus, cbStatus, &cbStatus);

		if (dwResult == 0)
		{
			pStatus -> bmAuthenticationMethods = _ttoi(argv[0]);
			dwResult = RasIOControl(0, RASCNTL_SERVER_SET_PARAMETERS, (PUCHAR)pStatus, cbStatus, 0, 0, &cbStatus);

			if (dwResult != 0)
			{
				Print(TEXT("RasIOControl failed, error = %d"), dwResult);
			}
			else
			{
				Print(TEXT("Authentication mask is now 0x%x"), pStatus -> bmAuthenticationMethods);
			}
		}
		else
		{
			Print(TEXT("RasIOControl failed, error = %d"), dwResult);
		}
	}
	else
	{
		Print(TEXT("Unable to allocate %d bytes of memory for status info"), cbStatus);
	}
}

CommandInfo g_RasSetCommandTable[] =
{
	{ TEXT("authmode"),		RasSetAuthmodeCommand,	1,	1},
	{ TEXT("authmask"),		RasSetAuthmaskCommand,	1,	1},
	{ NULL,					NULL,					0,	0}
};

void
RasSetCommand(
	int argc,
	TCHAR *argv[])
{
	// Search for a command and execute
	(void)CommandFindAndExecute(&g_RasSetCommandTable[0], argc, argv);
}

CommandInfo g_RasCommandTable[] =
{
	{ TEXT("set"),		RasSetCommand,				2,	2},
	{ NULL,				NULL,						0,	0}
};

void
RasCommand(
	int argc,
	TCHAR *argv[])
{
	// Search for a command and execute
	(void)CommandFindAndExecute(&g_RasCommandTable[0], argc, argv);
}

void
EnableServerCommand(
	int argc,
	TCHAR *argv[])
{
	DWORD dwResult;

	dwResult = RasIOControl(NULL, RASCNTL_SERVER_ENABLE, NULL, 0, NULL, 0, NULL);
	if (dwResult == 0)
	{
		Print(TEXT("Server enabled successfully"));
	}
}

void
DisableServerCommand(
	int argc,
	TCHAR *argv[])
{
	DWORD dwResult;

	dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, NULL);
	if (dwResult == 0)
	{
		Print(TEXT("Server disabled successfully"));
	}
}

void
RasGetEntryCommand(
	int argc,
	TCHAR *argv[])
{
	DWORD	dwResult,
			dwEntrySize,
			dwDevInfoSize;
	LPWSTR	szEntry;
	BYTE	devInfo[1024];

	if (argc == 0)
		szEntry = NULL;
	else
		szEntry = argv[0];

	dwEntrySize = 0;
	dwDevInfoSize = sizeof(devInfo);
	dwResult = RasGetEntryProperties(NULL, szEntry, NULL, &dwEntrySize, devInfo, &dwDevInfoSize);

	Print(TEXT("Result=%d dwEntrySize=%d dwDevInfoSize=%d"), dwResult, dwEntrySize, dwDevInfoSize);
}

CommandInfo g_CommandTable[] =
{
	{ TEXT("user"),			UserCommand,				0,	9},
	{ TEXT("ras"),			RasCommand,					3,  3},
	{ TEXT("line"),			LineCommand,				0,	9},
	{ TEXT("enable"),		EnableServerCommand,		0,	0},
	{ TEXT("disable"),		DisableServerCommand,		0,	0},
	{ TEXT("dhcp"),			DhcpCommand,				1,	1},
	{ TEXT("dhcpif"),		DhcpIfCommand,				1,	1},
	{ TEXT("ipv6"),		    IPV6Command,				0,	2},
	{ TEXT("autoip"),		AutoIPCommand,				1,	1},
	{ TEXT("autonet"),		AutoNetCommand,				2,	2},
	{ TEXT("rasgetentry"),	RasGetEntryCommand,			0,	1},
	{ NULL,					NULL,						0,	0}
};

void
main(
	int		argc,
	TCHAR  *argv[])
{
	BOOL	bFoundCommand = FALSE;

	// Skip the program name
	argc--;
	argv++;

	// Parse and remove options
	while (argc > 0 && argv[0][0] == TEXT('-'))
	{
		switch (argv[0][1])
		{
		case TEXT('d'):
			g_bOutputToConsole = FALSE;
			break;

		case TEXT('v'):
			g_bVerbose = TRUE;
			break;

		default:
			Usage();
			return;
		}
		argc--;
		argv++;
	}


	if (argc == 0)
	{
		// With no parameters, dump current PPP server state
		DisplayServerStatus();
	}
	else
	{
		// Search for a command and execute
		(void)CommandFindAndExecute(&g_CommandTable[0], argc, argv);
	}
}

int 
WINAPI WinMain(
	HINSTANCE	hInstance, 
	HINSTANCE	hPrevInstance, 
	LPTSTR		pCmdLine, 
	int			nCmdShow)
{
	TCHAR	*argv[20];
	int		argc;
	
	argc = CreateArgvArgc( TEXT( "pppconfig" ), argv, pCmdLine );

	main(argc, argv);

	return 0;
}
