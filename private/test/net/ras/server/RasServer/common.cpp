//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <windows.h>
#include "common.h"

void
RasPrint(
    TCHAR *pFormat, 
    ...)
{
    va_list ArgList;
    TCHAR    Buffer[256];

    va_start (ArgList, pFormat);

    (void)wvsprintf (Buffer, pFormat, ArgList);

#ifndef UNDER_CE
    _putts(Buffer);
#else
    OutputDebugString(Buffer);
#endif

    va_end(ArgList);
}

//
//    Get the array of all the RAS line devices in the system
//    This function will allocate memory for the array
//    Return: 0 if success
//            Error code on failure
//
//
// *pdwTargetedLine==0 -> Default Device 0
// *pdwTargetedLine==1 -> PPTP Line
// *pdwTargetedLine==2 -> L2TP Line
//
//
//
DWORD GetLineDevices(OUT LPRASDEVINFOW *ppRasDevInfo, OUT PDWORD pcDevices, IN OUT PDWORD pdwTargetedLine)
{
    DWORD    cbNeeded,
            dwResult;
    DWORD dwCurrentDevice=0;

    dwResult = 0;
    //
    //    First pass is to get the size
    //
    cbNeeded = 0;
    *pcDevices = 0;
    (void)RasIOControl(0, RASCNTL_ENUMDEV, NULL, 0, (PUCHAR)&cbNeeded, 0, pcDevices);

    *ppRasDevInfo = (LPRASDEVINFOW)malloc(cbNeeded);
    if (*ppRasDevInfo == NULL)
    {
        RasPrint(TEXT("Unable to allocate %d bytes of memory to store device info"), cbNeeded);
        dwResult = GetLastError();
    }
    else
    {
        (*ppRasDevInfo)->dwSize = sizeof(RASDEVINFO);
        dwResult = RasIOControl(0, RASCNTL_ENUMDEV, (PUCHAR)*ppRasDevInfo, 0, (PUCHAR)&cbNeeded, 0, pcDevices);
    }

    while(dwCurrentDevice < *pcDevices)
    {
        switch (*pdwTargetedLine)
        {
            case 0:
                *pdwTargetedLine=0;
                goto Return;
                
            case 1:
                if(_tcsstr((*ppRasDevInfo)[dwCurrentDevice].szDeviceName, PPTP_LINE_NAME))
                {
                    *pdwTargetedLine=dwCurrentDevice;
                    goto Return;
                }
                break;

            case 2:
                if(_tcsstr((*ppRasDevInfo)[dwCurrentDevice].szDeviceName, L2TP_LINE_NAME))
                {
                    *pdwTargetedLine=dwCurrentDevice;
                    goto Return;
                }
                break;

            default:
                *pdwTargetedLine=0;
                goto Return;
        }

        dwCurrentDevice++;
    }

Return: 
    return dwResult;
}

//    
//    Remove all line devices added in the RAS server
//    Return:    0 if SUCCESS
//            Error code on Failure
//
DWORD RemoveAllLines()
{
    RASCNTL_SERVERSTATUS *pStatus    = NULL;
    RASCNTL_SERVERLINE   *pLine        = NULL;
    DWORD                 cbStatus    = 0,
                         dwResult    = 0,
                         iLine        = 0,
                         dwError    = 0;

    //
    //    First pass is to get the size
    //
    (void)RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, NULL, 0, &cbStatus);

    pStatus = (RASCNTL_SERVERSTATUS *)malloc(cbStatus);
    if (pStatus == NULL)
    {
        RasPrint(TEXT("Unable to allocate %d bytes of memory to store device info"), cbStatus);
        dwError = GetLastError();
    }
    else
    {
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, (PUCHAR)pStatus, cbStatus, &cbStatus);
        if (dwResult != 0)
        {
            RasPrint(TEXT("Unable to retrieve RAS server status. Error code = %i"), dwResult);
            dwError = dwResult;
        }
        else
        {
            pLine = (RASCNTL_SERVERLINE *)(&pStatus[1]);
            for (iLine = 0; iLine < pStatus->dwNumLines; iLine++, pLine++)
            {
                dwResult = RasIOControl(NULL, RASCNTL_SERVER_LINE_REMOVE, (PUCHAR)pLine, sizeof(RASCNTL_SERVERLINE), NULL, 0,&cbStatus);
                if (dwResult != 0)
                {
                    RasPrint(TEXT("Unable to remove line %s. Error code = %i"), (pLine->rasDevInfo).szDeviceName, dwResult);
                    dwError = dwResult;
                }    
            }
        }

    }

    if (pStatus != NULL)
    {
        free(pStatus);
    }
    
    return dwError;
}

//
// Disables the server, and also removes all added/enabled lines
//
VOID CleanupServer()
{
    DWORD dwResult=0, cbStatus=0;

    //
    // Remove all lines
    // 
    RemoveAllLines();

    //
    // Disable Server
    //
    dwResult = RasIOControl(
                    NULL, 
                    RASCNTL_SERVER_DISABLE, 
                    NULL, 0, 
                    NULL, 0, 
                    &cbStatus
                    );
}
