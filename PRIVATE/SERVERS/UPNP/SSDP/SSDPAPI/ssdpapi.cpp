//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>
#pragma hdrstop
#include "ssdpfuncc.h"


extern HANDLE g_hUPNPSVC;		// handle to SSDP service

extern CRITICAL_SECTION g_csUPNPAPI;
static DWORD cbBytesReturned;

extern int ClientStop();
extern int ClientStart();

HANDLE WINAPI RegisterNotification(DWORD nt, const wchar_t* pwszUSN, const wchar_t* pwszQueryString, const wchar_t* pwszMsgQueue)
{
	HANDLE hNotify = NULL;
	
    if (g_hUPNPSVC == INVALID_HANDLE_VALUE)
    {
        ClientStart();
    }

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

	RegisterNotificationParams parms;
	
	parms.phNotify			= &hNotify;
	parms.nt				= nt;
	parms.pwszUSN			= pwszUSN;
	parms.pwszQueryString	= pwszQueryString;
	parms.pwszMsgQueue		= pwszMsgQueue;

	DeviceIoControl(
				g_hUPNPSVC,
				SSDP_IOCTL_REGISTERNOTIFICATION,
				&parms, sizeof(parms),
				NULL, 0,
				&cbBytesReturned, NULL);

	return hNotify;
}


BOOL WINAPI DeregisterNotification(HANDLE hNotification)
{

    BOOL    fRet = FALSE;
	struct  DeregisterNotificationParams parms = {hNotification};
	
    if (g_hUPNPSVC == INVALID_HANDLE_VALUE)
    {
        ClientStart();
    }

	return	DeviceIoControl(
				g_hUPNPSVC,
				SSDP_IOCTL_DEREGISTERNOTIFICATION,
				&parms, sizeof(parms),
				NULL, 0,
				&cbBytesReturned, NULL);
    
}


/*BOOL WINAPI UpdateCache(PSSDP_MESSAGE pSsdpMessage)
{
	struct UpdateCacheParams parms = {pSsdpMessage};
    
    if (g_hUPNPSVC == INVALID_HANDLE_VALUE)
    {
        ClientStart();
    }
    
    return DeviceIoControl(
				g_hUPNPSVC,
				SSDP_IOCTL_UPDATECACHE,
				&parms, sizeof(parms),
				NULL, 0,
				&cbBytesReturned, NULL);
}

BOOL WINAPI
LookupCache(PSTR szType,
	SERVICE_CALLBACK_FUNC pfSearchCallback, LPVOID pvContext)
{
	LookupCacheParams parms = {szType, pfSearchCallback, pvContext};
    
    if (g_hUPNPSVC == INVALID_HANDLE_VALUE)
    {
        ClientStart();
    }
    
    return DeviceIoControl(
				g_hUPNPSVC,
				SSDP_IOCTL_LOOKUPCACHE,
				&parms, sizeof(parms),
				NULL, 0,
				&cbBytesReturned, NULL);

}

BOOL WINAPI 
CleanupCache(void)
{
    if (g_hUPNPSVC == INVALID_HANDLE_VALUE)
    {
        ClientStart();
    }

    return DeviceIoControl(
				g_hUPNPSVC,
				SSDP_IOCTL_CLEANUPCACHE,
				NULL, 0,
				NULL, 0,
				&cbBytesReturned, NULL);
}*/
