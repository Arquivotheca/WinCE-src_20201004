//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>

static ce::psl_proxy<> proxy(L"UPP1:", SSDP_IOCTL_INVOKE, NULL);

HANDLE WINAPI RegisterNotification(DWORD nt, const wchar_t* pwszUSN, const wchar_t* pwszQueryString, const wchar_t* pwszMsgQueue)
{
	HANDLE hNotify = NULL;
	
	if(!(nt & (NOTIFY_ALIVE | NOTIFY_PROP_CHANGE)))
	{
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

	if((nt & NOTIFY_ALIVE) && pwszUSN == NULL)
	{
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

	if((nt & NOTIFY_PROP_CHANGE) && pwszQueryString == NULL)
	{
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (pwszMsgQueue == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

	proxy.call(SSDP_IOCTL_REGISTERNOTIFICATION, nt, pwszUSN, pwszQueryString, pwszMsgQueue, &hNotify);
	
	return hNotify;
}


BOOL WINAPI DeregisterNotification(HANDLE hNotify)
{
    return (ERROR_SUCCESS == proxy.call(SSDP_IOCTL_DEREGISTERNOTIFICATION, reinterpret_cast<ce::PSL_HANDLE>(hNotify)));
}


HANDLE WINAPI RegisterService(SSDP_MESSAGE_XP* pMessage, DWORD dwFlags)
{
	SSDP_MESSAGE    msg;
	HANDLE          hService = NULL;
	
    memset(&msg, 0, sizeof(msg));
    *static_cast<SSDP_MESSAGE_XP*>(&msg) = *pMessage;
    
	proxy.call(SSDP_IOCTL_REGISTER_SERVICE, &msg, dwFlags, &hService);
				
	return hService;
}


BOOL WINAPI DeregisterService(HANDLE hService, BOOL bByebye)
{
	return (ERROR_SUCCESS == proxy.call(SSDP_IOCTL_DEREGISTER_SERVICE, reinterpret_cast<ce::PSL_HANDLE>(hService), bByebye));
}
