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
/*****************************************************************************
* 
*
*   @doc EX_RAS
*   ras.c   Remote Access Server
*
*   Date: 6-22-95
*
*/


//  Include Files

#include "windows.h"

#include "winsock2.h"
#include "ws2tcpip.h"

#include "cxport.h"
#include "crypt.h"
#include "memory.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ipv6cp.h"
#include "ncp.h"
#include "mac.h"
#include "raserror.h"
#include "pppserver.h"
#include "auth.h"
#include "netui_kernel.h"

const WCHAR sc_szRAS[] = L"RAS";
const WCHAR sc_szHangUp[] = L"HANG_UP";

DWORD 
ShowUsernamePasswordDialog(pppSession_t *s_p)
{
    NETUI_USERPWD   UserPwd;
    DWORD           dwRetCode = NO_ERROR;
    HWND            hParent = NULL;
    BOOL            bGotUserInfo;
    RASDIALPARAMS   RasDialParams;
    BOOL            bPasswordSaved;

    if (s_p->notifierType == RAS_NOTIFIERTYPE_WINDOW_MSGQ) 
    {
        hParent = (HWND) s_p->notifier;
    }
    
    // Set up the UserPwd structure
    memset ((char *)&UserPwd, 0, sizeof(UserPwd));

    StringCchCopy(UserPwd.szUserName, _countof(UserPwd.szUserName), s_p->rasDialParams.szUserName);
    StringCchCopy(UserPwd.szPassword, _countof(UserPwd.szPassword), s_p->rasDialParams.szPassword);
    StringCchCopy(UserPwd.szDomain,   _countof(UserPwd.szDomain),   s_p->rasDialParams.szDomain);

    //
    // If a password is currently saved in the registry for the connectoid,
    // check the save password box.
    //
    RasDialParams.dwSize = sizeof(RasDialParams);
    wcscpy(RasDialParams.szEntryName, s_p->rasDialParams.szEntryName);
    if (RasGetEntryDialParams(NULL, &RasDialParams, &bPasswordSaved) == NO_ERROR)
    {
        if (bPasswordSaved)
            UserPwd.dwFlags |= NETUI_USERPWD_SAVEPWD;
    }

    // Show the save password box.
    UserPwd.dwFlags |= NETUI_USERPWD_SHOW_SAVEPWD;

    DEBUGMSG( ZONE_PPP, (TEXT("GetUsernamePassword:About to call CallKGetUsernamePassword()\r\n")));

    bGotUserInfo = CallKGetUsernamePasswordEx(hParent, &UserPwd, &s_p->hPasswordDialog);

    s_p->hPasswordDialog = NULL;

    pppLock(s_p);
    if (bGotUserInfo)
    {
        // Copy the data back.
        _tcscpy (s_p->rasDialParams.szUserName, UserPwd.szUserName);
        _tcscpy (s_p->rasDialParams.szPassword, UserPwd.szPassword);
        _tcscpy (s_p->rasDialParams.szDomain, UserPwd.szDomain);

        RasSetEntryDialParams( NULL, &(s_p->rasDialParams),
                              !(UserPwd.dwFlags & NETUI_USERPWD_SAVEPWD) );
    }
    else
    {
        // Presumably user cancelled the dialog, they do not want to connect.
        dwRetCode = ERROR_USER_DISCONNECTION;
    }
    pppUnLock(s_p);
    DEBUGMSG( ZONE_PPP, (TEXT("GetUserThread:GetUsernamePassword() dwRetCode=%d\r\n"), dwRetCode));
    return dwRetCode;
}


DWORD
_AfdValidatePeerAddress( 
   RASENTRY * _pRasEntry
   )
{
    DWORD           Sts = ERROR_SUCCESS;
    SOCKADDR_INET   AddrVal = {0};
    INT             AddrSize = 0;
    ADDRESS_FAMILY  AddrFamily = 0;

    do
    {
        if ( !_pRasEntry )
        {
            ASSERT( 0 );
            Sts = ERROR_INVALID_PARAMETER;

            break;
        }

        // Try IPv4 first...
        memset( &AddrVal, 0, sizeof( SOCKADDR_INET ) );
        AddrFamily = AF_INET;
        AddrVal.si_family = AddrFamily;
        AddrSize = sizeof( SOCKADDR_IN );
        Sts = WSAStringToAddress( _pRasEntry->szLocalPhoneNumber,
                                  AddrFamily,
                                  NULL,
                                  (LPSOCKADDR) &AddrVal,
                                  &AddrSize );
        if ( !Sts )
        {
            // A valid IPv4 Address... we're done
            break;
        }

        // Not an IPv4 address, so try IPv6...
        memset( &AddrVal, 0, sizeof( SOCKADDR_IN6 ) );
        AddrFamily = AF_INET6;
        AddrVal.si_family = AddrFamily;
        AddrSize = sizeof( SOCKADDR_IN6 );
        Sts = WSAStringToAddress( _pRasEntry->szLocalPhoneNumber,
                                  AddrFamily,
                                  NULL,
                                  (LPSOCKADDR) &AddrVal,
                                  &AddrSize );
        if ( !Sts )
        {
            Sts = WSAGetLastError();
        }
        
    }   while ( 0 );

    return( Sts );
}



/*****************************************************************************
* 
*   @func   DWORD | RasDial | RAS Dial Entry point
*
*   @rdesc  0 success, otherwise one of the errors from raserror.h
*   @ecode  ERROR_NOT_ENOUGH_MEMORY | Unable to allocate memory.
*   @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY | Invalid RasEntry name specified
*   @ecode  ERROR_EVENT_INVALID | PPP Internal error
*   @ecode  ERROR_PORT_ALREADY_OPEN | Port already open
*   @ecode  ERROR_PPP_MAC | Error initializing MAC layer
*   @ecode  ERROR_PPP_LCP | Error initializing LCP layer
*   @ecode  ERROR_PPP_AUTH | Error initializing AUTH layer
*   @ecode  ERROR_PPP_NCP | Error initializing NCP layer
*   
*   @parm    LPRASDIALEXTENSIONS |     dialExtensions | The dial extensions
*   @parm    LPTSTR | phoneBookPath | The phonebook
*   @parm    LPRASDIALPARAMS    | rasDialParam | Dial Params
*   @parm    DWORD  | NotifierType | Notifier type
*   @parm    LPVOID | notifier | Pointer to Notifier (or window handle)
*   @parm    LPHRASCONN | pRasConn | Returned pointer to Ras connection
*               
*
*/

DWORD APIENTRY

AfdRasDial
    ( 
    LPRASDIALEXTENSIONS     dialExtensions,
    DWORD                   cbDialExtensions,
    LPCTSTR                 phoneBookPath,
    LPRASDIALPARAMS         rasDialParam,
    DWORD                   cbRasDialParam,
    DWORD                   NotifierType,
    DWORD                   notifier,
    LPHRASCONN              pRasConn 
    )
{
    pppStart_t  Start;
    RASENTRY    *RasEntry_p;
    DWORD       dwSize;
    BOOL        rc;
    DWORD       RetVal;
    pppSession_t *pSession;
    
    PPP_LOG_FN_ENTER();
        
    if (!g_bPPPInitialized)
    {
        RetVal = ERROR_SERVICE_NOT_ACTIVE;
        PPP_LOG_FN_LEAVE( RetVal );

        return ( RetVal );
    }

    //  Parameter Notes:
    //
    //  1.  dialExtensions: TBD
    //  2.  phoneBookPath:  specifies the full pathname of the phone book
    //                      path. If null then the default phone book is 
    //                      used.
    //  3.  rasDialParam:   Name, phone, password etc - depending on dialer
    //                      this needs work.
    //  4.  NotifierType:   type of rasdial event handler - used
    //  5.  notifier:       window or call back function for events.
    //  6.  pRasConn:       connection handle - used 

    DEBUGMSG( ZONE_RAS | ZONE_FUNCTION,
              ( TEXT( "PPP: (%hs) - %08X, %08X, %08X, %08X, %08X, %08X )\r\n" ),
                __FUNCTION__,
                dialExtensions, phoneBookPath, rasDialParam, NotifierType,
                notifier, pRasConn ) );

    if (NULL == pRasConn)
    {
        RetVal = ERROR_INVALID_PARAMETER;
        PPP_LOG_FN_LEAVE( RetVal );

        return ( RetVal );
    }

    //
    //  Validate the notifier and NotifierType parameters
    //
    if (notifier != (DWORD)NULL)
    {
        switch(NotifierType)
        {
            case RAS_NOTIFIERTYPE_WINDOW_MSGQ:
                if ((g_pfnPostMessageW == NULL))
                {
                    //
                    // Window manager support not present
                    //
                    DEBUGMSG( ZONE_ERROR,
                              ( TEXT( "PPP: (%hs) - * WARNING * " )
                                TEXT( "PostMessage support not available\r\n" ),
                                __FUNCTION__ ) );

                    RetVal = ERROR_INVALID_PARAMETER;
                    PPP_LOG_FN_LEAVE( RetVal );

                    return ( RetVal );
                }
                break;

            case RAS_NOTIFIERTYPE_CE_MSGQ:
                break;

            case RAS_NOTIFIERTYPE_RASDIALFUNC:
            case RAS_NOTIFIERTYPE_RASDIALFUNC1:
            default:
            {
                DEBUGMSG( ZONE_ERROR,
                          ( TEXT( "PPP: (%hs) - * WARNING * " )
                            TEXT( "Unsupported Notifier Type %x\r\n" ),
                                  __FUNCTION__,
                                  NotifierType ) );

                RetVal = ERROR_INVALID_PARAMETER;
                PPP_LOG_FN_LEAVE( RetVal );

                return ( RetVal );
            }
        }
    }

    // Allocate room for the ras entry.

    RasEntry_p = (RASENTRY *)LocalAlloc( LMEM_FIXED, sizeof( RASENTRY ) );
    if( NULL == RasEntry_p )
    {
        DEBUGMSG( ZONE_ERROR,
                  ( TEXT( "PPP: (%hs) - *** ERROR *** LocalAlloc FAILED\r\n" ),
                    __FUNCTION__ ) );

                RetVal = ERROR_NOT_ENOUGH_MEMORY;
                PPP_LOG_FN_LEAVE( RetVal );

                return ( RetVal );
    }

    memset( RasEntry_p, 0, sizeof( RASENTRY ) );

    // Read the RasEntry
    RasEntry_p->dwSize = sizeof( RASENTRY );
    dwSize = sizeof( RASENTRY );
    Start.dwDevConfigSize = 0;
    Start.lpbDevConfig = 0;
    RetVal = AfdRasGetEntryProperties( phoneBookPath, 
                       rasDialParam->szEntryName,
                       (LPBYTE)RasEntry_p, 
                       sizeof( RASENTRY ),
                       &dwSize,
                       NULL, 
                       0,
                       &Start.dwDevConfigSize);
    // We will get a ERROR_BUFFER_TOO_SMALL error
    // if there is a devconfig associated with this connection
    
    if (RetVal && ((RetVal != ERROR_BUFFER_TOO_SMALL) &&
        (Start.dwDevConfigSize))) {
        DEBUGMSG( ZONE_RAS,
                  (  TEXT("PPP: (%hs) - *** ERROR *** " )
                     TEXT( "Unable to open RAS Entry %s\r\n" ),
                     __FUNCTION__,
                     rasDialParam->szEntryName ) );
        LocalFree( RasEntry_p );

        RetVal = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
        PPP_LOG_FN_LEAVE( RetVal );

        return ( RetVal );
    }

    if (Start.dwDevConfigSize) {
        Start.lpbDevConfig = LocalAlloc (LPTR, Start.dwDevConfigSize);

        // Read the structure in again.
        if( AfdRasGetEntryProperties( phoneBookPath, 
                          rasDialParam->szEntryName,
                          (LPBYTE)RasEntry_p, 
                          sizeof( RASENTRY ),
                          &dwSize, 
                          Start.lpbDevConfig, 
                          Start.dwDevConfigSize,
                          &Start.dwDevConfigSize )) 
        {
            DEBUGMSG( ZONE_RAS | ZONE_ERROR, ( 
                TEXT("Unable to open RAS Entry %s\r\n"),
                rasDialParam->szEntryName ));
            LocalFree( RasEntry_p );
            LocalFree (Start.lpbDevConfig);
            return( ERROR_CANNOT_FIND_PHONEBOOK_ENTRY );
        }
    }

    *pRasConn = (HRASCONN )NULL;                // init callers handle

    // Create the PPP Session - Fill in the start structure:
    // DevConfig already filled in.

    Start.session       = NULL;                 // clear session pointer
    Start.notifierType  = NotifierType;         // type of notifier
    Start.notifier      = (PVOID)notifier;      // notifier function
    Start.rasDialParams = rasDialParam;         // pntr to dialer parameters
    Start.rasEntry      = RasEntry_p;           // Pntr to the RASENTRY struct.
    Start.bIsServer = FALSE;        // We're a client, connecting to a server

    //
    //  Note that if pppSessionNew is succesful then the new
    //  session will be exposed to applications via the RasEnumConnections
    //  API, and thus could be closed via a call to RasHangup at any time.
    //

    rc = pppSessionNew( &Start );

    pSession = Start.session;
    
    LocalFree(RasEntry_p );                    // free ras entry memory
    LocalFree(Start.lpbDevConfig);

    if( SUCCESS != rc || NULL == pSession)
    {
        DEBUGMSG( ZONE_RAS | ZONE_ERROR, (TEXT( "PPP: AfdRasDial:SESSION_NEW failed\r\n" )));
        return rc;
    }

    // This will be used in RasHangUp() for the security policy
    pSession->pidCallerProcess = GetDirectCallerProcessId();

    *pRasConn = (HRASCONN )pSession;       // set callers handle

    //
    //  Since we just told the app about the session with the above assign,
    //  the app could call RasHangup at any time.  Get a ref to the session
    //  to make sure that while we are starting things up it does not go
    //  away unexpectedly...
    //

    if (PPPADDREF(pSession, REF_RASDIAL))
    {
        // Start the Session

        if(pSession->rasEntry.dwfOptions & RASEO_PreviewUserPw)
        {
            // User could cancel this dialog, resulting in non-zero rc, aborting the connection
            rc = ShowUsernamePasswordDialog(pSession);
        }

        if (rc == NO_ERROR)
        {
            rc = pppSessionRun(pSession);
            DEBUGMSG(rc && ZONE_ERROR, (TEXT("PPP: ERROR - RasDial: pppSessionRun returned %u\r\n"), rc));
        }

        PPPDELREF(pSession, REF_RASDIAL);

        DEBUGMSG( ZONE_RAS, ( TEXT("<-AfdRasDial:SESSION_RUN OK\r\n" )));
    }
    return rc;
}

static void
pppSessionCloseComplete(
    PVOID   pData)
{
    HANDLE          hCloseCompleteEvent = (HANDLE)pData;

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:+pppSessionCloseComplete\n" ) ));

    SetEvent(hCloseCompleteEvent);

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:-pppSessionCloseComplete\n" ) ));
}

/*****************************************************************************
* 
*   @func   DWORD | RasHangup | RAS Hangup Entry point
*
*   @rdesc  0 success, otherwise one of the following errors:
*   @ecode  ERROR_INVALID_PARAMETER | Invalid hRasConn parameter.
*   
*   @parm   HRASCONN    | hRasConn    | Handle to connection to close
*               
*
*/

DWORD APIENTRY
AfdRasHangUp( HRASCONN RasConn )
{
    DWORD           rc = ERROR_SUCCESS;
    pppSession_t    *s_p = (pppSession_t *)RasConn;
    HANDLE          hCloseCompleteEvent;
        
    DEBUGMSG( ZONE_RAS, ( TEXT( "PPP:+AfdRasHangUp( %08X )\r\n" ), RasConn ));
        
    if (PPPADDREF(s_p, REF_RASHANGUP))
    {
        if(ERROR_SUCCESS == rc)
        {
            hCloseCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

            if (hCloseCompleteEvent != NULL)
            {
                // Tell everyone that we're closing
                s_p->OpenFlags &= ~PPP_FLAG_PPP_OPEN;

                // Close any password dialog that is open
                if (s_p->hPasswordDialog)
                {
                    CallKCloseUsernamePasswordDialog(s_p->hPasswordDialog);
                    s_p->hPasswordDialog = NULL;
                }

                // Request Session stop
                rc = pppSessionStop(s_p, pppSessionCloseComplete, hCloseCompleteEvent);

                if (ERROR_SUCCESS == rc)
                    WaitForSingleObject(hCloseCompleteEvent, INFINITE);

              CloseHandle(hCloseCompleteEvent);
            }
            else
            {
                rc = ERROR_OUTOFMEMORY;
            }
        }
        PPPDELREF(s_p, REF_RASHANGUP);
    }
    else
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP:!AfdRasHangup: Invalid RasConn 0x%X\r\n"),
                               RasConn));
        rc = ERROR_INVALID_PARAMETER;
    }
    

    DEBUGMSG( ZONE_RAS, (TEXT("PPP:- AfdRasHangUp: Returning %d\r\n"), rc));
    return rc;
}

/*****************************************************************************
* 
*   @func   DWORD | RasEnumConnections |  Lists all active RAS connections. 
*                                        It returns each connection's 
*                                        handle and phone book entry name.
*
*   @rdesc  DWORD   0 success, !0 failure code defined in raserror.h
*   @ecode  ERROR_INVALID_SIZE | lpRasConn->dwSize is invalid.
*   @ecode  ERROR_BUFFER_TOO_SMALL | Buffer size is too small.
*
*   @parm   LPRASCONN   | lpRasConn      | buffer to receive connections data
*   @parm   LPDWORD     | lpcb           | buffer size in bytes
*   @parm   LPDWORD     | lpcConnections | number of connections written 
*                                          to buffer
*               
*
*/

DWORD APIENTRY
AfdRasEnumConnections( 
        OUT LPRASCONN   lpRasConn, 
    IN      DWORD       cbRasConn,
    IN  OUT LPDWORD     lpcb,  
        OUT LPDWORD     lpcConnections)
{
    DWORD            RetVal = NO_ERROR;
    DWORD            NumRequested;
    PPPP_SESSION     pSession;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +AfdRasEnumConnections\n")));

    if (NULL == lpRasConn || NULL == lpcb || NULL == lpcConnections)
        return ERROR_INVALID_PARAMETER;

    // Validate size of structure

    if( sizeof( RASCONN ) != lpRasConn->dwSize )
    {
        // Passed in size is incorrect.  The dwSize must 
        // be equal to the sizeof the RASCONN structure.

        return ERROR_INVALID_SIZE;
    }

    // Compute number of connections requested from buffer size

    NumRequested = cbRasConn / sizeof( RASCONN );
    
    // Initialize return parameters

    *lpcb = 0;                                  // size in bytes of return
    *lpcConnections = 0;                        // # written to buffer

    // Loop through the contexts

    EnterCriticalSection (&v_ListCS);

    __try
    {
        for(pSession = g_PPPSessionList; 
            pSession; 
            pSession = pSession->Next )
        {
            //
            //  Do not count server sessions
            //
            if( pSession->bIsServer)
            {
                continue;
            }

            DEBUGMSG(ZONE_RAS, (L"RAS: RasEnumConnections Found Entry Name: '%s'\n", pSession->rasDialParams.szEntryName ));

            // Copy this one into the provided buffer

            if( 0 == NumRequested )
            {
                // have run out of buffer space
                RetVal = ERROR_BUFFER_TOO_SMALL;
            }
            else
            {
                // Fill in request

                (*lpcConnections)++;

                // Fill in the request
                lpRasConn->dwSize   = sizeof( RASCONN );
                lpRasConn->hrasconn = (HRASCONN )pSession;
                _tcscpy(lpRasConn->szEntryName, pSession->rasDialParams.szEntryName );

                lpRasConn++;
                NumRequested--;
            }

            (*lpcb) += sizeof( RASCONN );       // we always update the required size so callers can tell how much to allocate
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        RetVal = ERROR_INVALID_PARAMETER;
    }

    LeaveCriticalSection (&v_ListCS);

    DEBUGMSG(ZONE_RAS && RetVal == ERROR_BUFFER_TOO_SMALL, (TEXT("RAS: RasEnumConnections - Caller BUFFER_TOO_SMALL.\n")));

    DEBUGMSG( ZONE_RAS | ZONE_FUNCTION, (TEXT( "PPP: -RasEnumConnections() numConn=%u\n" ), *lpcConnections));

    return RetVal;
}

/*****************************************************************************
* 
*   @func   DWORD | RasGetConnectStatus | Get connection status
*
*   @rdesc  If the function succeeds the value is zero. Else an error
*           from raserror.h is returned.
*   @ecode  ERROR_INVALID_PARAMETER | Invalid hRasConn or pRasConnStatus
*           parameter.
*   
*   @parm   HRASCONN        | hRasConn   |
*               Handle of Ras Connection
*   @parm   LPRASCONNSTATE  | pRasConnStatus |
*               Pointer to struct to return inf.
*               
*
*/

DWORD APIENTRY
AfdRasGetConnectStatus(
    IN  HRASCONN        hrasconn,
    OUT LPRASCONNSTATUS lprasconnstatus,
    IN  DWORD           cbRasConnStatus)
{
    pppSession_t    *s_p = (pppSession_t *)hrasconn;
    DWORD            rc = SUCCESS;

    DEBUGMSG(ZONE_RAS, (L"PPP:+AfdRasGetConnectStatus(%p, %p)\r\n", hrasconn, lprasconnstatus ) );

    if (PPPADDREF(s_p, REF_GETCONNECTSTATUS))
    {
        __try
        {
            lprasconnstatus->rasconnstate = s_p->RasConnState;
            lprasconnstatus->dwError      = s_p->RasError;
            _tcscpy( lprasconnstatus->szDeviceType, s_p->rasEntry.szDeviceType );
            _tcscpy( lprasconnstatus->szDeviceName, s_p->rasEntry.szDeviceName );
        }
        __except(EXCEPTION_EXECUTE_HANDLER) 
        {
            rc = ERROR_INVALID_PARAMETER;
        }
        
        PPPDELREF(s_p, REF_GETCONNECTSTATUS);
    }
    else
    {
        rc = ERROR_INVALID_HANDLE;
    }

    DEBUGMSG(ZONE_RAS, (L"PPP:-AfdRasGetConnectStatus: Returning %d\r\n", rc));
    return rc;
}

DWORD APIENTRY
AfdRasGetProjectionInfo(
    IN  HRASCONN      hrasconn,
    IN  RASPROJECTION rasprojection,
    OUT LPVOID        lpprojection,
    OUT LPDWORD       lpcb)
{
    pppSession_t    *s_p = (pppSession_t *)hrasconn;
    DWORD            rc = SUCCESS;
    RASPPPIP        *r_p;
    ncpCntxt_t      *ncp_p;
    ipcpCntxt_t     *ipcp_p;
    DWORD            LocalIP,
                     PeerIP;

    DEBUGMSG(ZONE_RAS, (L"PPP:+AfdRasGetProjectionInfo (%08X, %08X)\n", hrasconn, lpprojection));

    if (NULL == lpcb)
    {
        rc = ERROR_INVALID_PARAMETER;
    } 
    else
    {
        if (PPPADDREF (s_p, REF_GETPROJECTIONINFO)) 
        {
            __try
            {
                switch (rasprojection) 
                {
                    case RASP_PppIp:
                        r_p = (RASPPPIP *)lpprojection;
                        ncp_p  = (ncpCntxt_t  *)s_p->ncpCntxt;
                        ipcp_p = (ipcpCntxt_t *)ncp_p->protocol[ NCP_IPCP ].context;
                        LocalIP =  ipcp_p->local.ipAddress;
                        PeerIP =   ipcp_p->peer.ipAddress;
                                            
                        // Check state (CE never enters projected state)
                        if (ipcp_p->pFsm->state < PFS_Opened)
                        {
                            rc = ERROR_PROJECTION_NOT_COMPLETE;
                            break;
                        }

                        //
                        //  Additional fields have been added to the RASPPIP
                        //  structure in various releases.  Fill in all the
                        //  fields for which the user has supplied space.
                        //
                        if (r_p->dwSize < offsetof(RASPPPIP, szServerIpAddress)) {
                            rc = ERROR_BUFFER_TOO_SMALL;
                            break;
                        }

                        //
                        //  Space is available for the dwError and szIpAddress
                        //
                        r_p->dwError = s_p->RasError;
                        StringCchPrintfW(r_p->szIpAddress, _countof(r_p->szIpAddress),
                            L"%u.%u.%u.%u",
                            (LocalIP>>24)&0xFF,(LocalIP>>16)&0xFF,
                            (LocalIP>>8)&0xFF,LocalIP&0xFF);

                        //
                        //  Check for space for the serverIpAddress field
                        //
                        if (r_p->dwSize < offsetof(RASPPPIP, dwOptions))
                            break;
        
                        r_p->szServerIpAddress[0] = TEXT('\0');
                        if (PeerIP)
                        {
                            StringCchPrintfW(r_p->szServerIpAddress, _countof(r_p->szServerIpAddress), 
                                    L"%u.%u.%u.%u",
                                    (PeerIP>>24)&0xFF,(PeerIP>>16)&0xFF,
                                    (PeerIP>>8)&0xFF,PeerIP&0xFF);
                        }

                        //
                        //  Check for space for the dwOptions
                        //
                        if (r_p->dwSize < offsetof(RASPPPIP, dwServerOptions))
                            break;

                        r_p->dwOptions = ipcp_p->local.VJCompressionEnabled ? RASIPO_VJ : 0;


                        //
                        //  Check for space for the dwServerOptions
                        //
                        if (r_p->dwSize < sizeof(RASPPPIP))
                            break;

                        r_p->dwServerOptions = ipcp_p->peer.VJCompressionEnabled ? RASIPO_VJ : 0;

                        break;

                    case RASP_PppIpV6:
                    {
                        LPRASPPPIPV6 pIPV6Projection;
                        PIPV6Context pIPV6Context;

                        pIPV6Projection = (LPRASPPPIPV6)lpprojection;
                        ncp_p  = (ncpCntxt_t  *)s_p->ncpCntxt;
                        pIPV6Context = (PIPV6Context)(ncp_p->protocol[NCP_IPV6CP].context);

                        if (pIPV6Projection->dwSize < sizeof(RASPPPIPV6))
                        {
                            rc = ERROR_BUFFER_TOO_SMALL;
                            break;
                        }

                        if (ncp_p->protocol[NCP_IPV6CP].enabled == FALSE)
                        {
                            rc = ERROR_PROTOCOL_NOT_CONFIGURED;
                            break;
                        }

                        if (pIPV6Context->pFsm->state < PFS_Opened)
                        {
                            rc = ERROR_PROJECTION_NOT_COMPLETE;
                            break;
                        }

                        pIPV6Projection->dwError = NO_ERROR;
                        memcpy(&pIPV6Projection->LocalInterfaceIdentifier[0], &pIPV6Context->LocalInterfaceIdentifier[0], IPV6_IFID_LENGTH);
                        memcpy(&pIPV6Projection->PeerInterfaceIdentifier[0], &pIPV6Context->PeerInterfaceIdentifier[0], IPV6_IFID_LENGTH);
                        memcpy(&pIPV6Projection->LocalCompressionProtocol[0], &pIPV6Context->LocalCompressionProtocol[0], 2);
                        memcpy(&pIPV6Projection->PeerCompressionProtocol[0], &pIPV6Context->PeerCompressionProtocol[0], 2);

                        break;
                    }

                    default:
                        rc = ERROR_PROTOCOL_NOT_CONFIGURED;
                        break;
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER) 
            {
                rc = ERROR_INVALID_PARAMETER;
            }

            PPPDELREF (s_p, REF_GETPROJECTIONINFO);
        } 
        else 
        {
            rc = ERROR_INVALID_HANDLE;
        }
    }

    DEBUGMSG (ZONE_RAS, (TEXT("PPP:-AfdRasGetProjectionInfo: Returning %d\r\n"), rc));
    return rc;
}


/*****************************************************************************
* 
*   @func   DWORD | pppGetRasStats | Get connection statistics
*
*   @rdesc  If the function succeeds the value is zero. Else an error
*           from raserror.h is returned.
*   @ecode  ERROR_INVALID_PARAMETER | Invalid hRasConn or pRasStat
*           parameter.
*   
*   @parm   HRASCONN        | hRasConn   |
*               Handle of Ras Connection
*   @parm   PRAS_STATS      | pRasStat |
*               Pointer to struct to return inf.
*               
*
*/

DWORD
pppGetRasStats(
    HRASCONN   hRasConn,
    PRAS_STATS pRasStats)
{
    DWORD                   dwResult = ERROR_SUCCESS;
    pppSession_t            *s_p = (pppSession_t *)hRasConn;
    NDIS_STATUS             Status;
    NDIS_WAN_GET_STATS_INFO MacStats;

    DEBUGMSG(ZONE_RAS, (TEXT("PPP: +pppGetRasStats (0x%08X, 0x%08X)\r\n"), hRasConn, pRasStats));

    // Check size?
    if (pRasStats->dwSize != sizeof(RAS_STATS))
    {
        DEBUGMSG (ZONE_ERROR|ZONE_RAS, (TEXT("ppp:-pppGetRasStats: pRasStats->dwSize not correct size\r\n")));
        return ERROR_INVALID_PARAMETER;
    }

    if (PPPADDREF (s_p, REF_GETRASSTATS))
    {
        __try
        {
            // Clear buffer
            CTEMemSet( (char *)pRasStats, 0, sizeof(RAS_STATS) );
            pRasStats->dwSize = sizeof(RAS_STATS);

            // Use MAC layer stats if available
            Status = pppMacGetWanStats(s_p->macCntxt, &MacStats);
            if (Status == NDIS_STATUS_SUCCESS)
            {
                // Use stats provided by miniport
                
                pRasStats->dwBytesXmited = MacStats.BytesSent;
                pRasStats->dwBytesRcved = MacStats.BytesRcvd;
                pRasStats->dwFramesXmited = MacStats.FramesSent;
                pRasStats->dwFramesRcved = MacStats.FramesRcvd;

                pRasStats->dwCrcErr = MacStats.CRCErrors;
                pRasStats->dwTimeoutErr = MacStats.TimeoutErrors;
                pRasStats->dwAlignmentErr = MacStats.AlignmentErrors;
                pRasStats->dwHardwareOverrunErr = MacStats.SerialOverrunErrors;
                pRasStats->dwFramingErr = MacStats.FramingErrors;
                pRasStats->dwBufferOverrunErr = MacStats.BufferOverrunErrors;
            }
            else // No MAC stats available
            {
                // Use PPP stats for frames and bytes.
                pRasStats->dwBytesXmited = s_p->Stats.BytesSent;
                pRasStats->dwBytesRcved = s_p->Stats.BytesRcvd;
                pRasStats->dwFramesXmited = s_p->Stats.FramesSent;
                pRasStats->dwFramesRcved = s_p->Stats.FramesRcvd;
            }

            //
            //  Compression Ratios specify a percentage that indicates the degree
            //  to which data received on this connection is compressed.
            //  The ratio is the size of the compressed data divided by the size
            //  of the same data in an uncompressed state. 
            //
            pRasStats->dwCompressionRatioIn = 100;
            if (s_p->Stats.BytesReceivedUncompressed)
            {
                pRasStats->dwCompressionRatioIn = (s_p->Stats.BytesReceivedCompressed * 100) /
                                                s_p->Stats.BytesReceivedUncompressed;
            }
            pRasStats->dwCompressionRatioOut = 100;
            if (s_p->Stats.BytesTransmittedUncompressed)
            {
                pRasStats->dwCompressionRatioOut = (s_p->Stats.BytesTransmittedCompressed * 100) /
                                                s_p->Stats.BytesTransmittedUncompressed;
            }
            pppMac_GetCallSpeed(s_p->macCntxt, &pRasStats->dwBps);
            pRasStats->dwConnectDuration = GetTickCount() - s_p->dwStartTickCount;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dwResult = ERROR_INVALID_PARAMETER;
        }

        PPPDELREF(s_p, REF_GETRASSTATS);
    }
    else
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("ppp:pppGetRasStats: Invalid hRasConn passed in\r\n")));
        dwResult = ERROR_INVALID_HANDLE;
    }

    DEBUGMSG (ZONE_RAS, (TEXT("PPP: -pppGetRasStats : Result = %d \r\n"), dwResult));
    return dwResult;
}   

DWORD
PPPDevConfigDialogEdit(
    IN      PTCHAR      szDeviceName,
    IN      PTCHAR      szDeviceType,
    IN      HWND        hWndOwner,
    IN      PBYTE       pDeviceConfigIn,
    IN      DWORD       dwDeviceConfigInSize,
    IN  OUT LPVARSTRING pDeviceConfigOut)
{
    DWORD   RetVal;

    PNDISWAN_ADAPTER        pAdapter;
    DWORD                   dwDeviceID;

    RetVal = FindAdapter(szDeviceName, szDeviceType, &pAdapter, &dwDeviceID);
    if (RetVal != SUCCESS)
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("ppp: PPPDevConfigDialogEdit: Can't find device '%s'\n"), szDeviceName));
    }
    else
    {
        RetVal = NdisTapiLineConfigDialogEdit(pAdapter, dwDeviceID, hWndOwner,
                    NULL, pDeviceConfigIn, dwDeviceConfigInSize, pDeviceConfigOut);

        AdapterDelRef (pAdapter);
    }

    return RetVal;
}

DWORD
pppRasCntlGetDispPhone(
    PBYTE   pBufIn,
    DWORD   dwLenIn, 
    PBYTE   pBufOut, 
    DWORD   dwLenOut,
    PDWORD  pdwActualOut)
//
//  Handle RASCNTL_GETDISPPHONE
//
{
    RASENTRY RasEntry;
    DWORD   dwSize = sizeof(RasEntry);
    DWORD   dwDeviceID;
    PNDISWAN_ADAPTER    pAdapter;
    DWORD   dwFlag;
    LPTSTR  szDialStr;
    DWORD   RetVal;

    do
    {
        if ((0 == dwLenOut) || (NULL == pBufOut) || (NULL == pBufIn))
        {
            RetVal = ERROR_INVALID_PARAMETER;
            DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR - pppRasCntlGetDispPhone : Invalid Parameter\r\n")));
            break;
        }

        // First get the RasEntry Info
        RasEntry.dwSize = sizeof(RasEntry);
        RetVal = AfdRasGetEntryProperties ( NULL, (LPTSTR)pBufIn, (LPBYTE)&RasEntry, sizeof(RasEntry), &dwSize, NULL, 0, NULL);
        if (RetVal)
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppRasCntlGetDispPhone: Error %d from AfdRasGetEntryProperties\r\n"), RetVal));
            break;
        }

        // Shortcut for 
        if (0 == _tcscmp (RasEntry.szDeviceType, RASDT_Modem))
        {
            // Now find the associated adapter
            if (SUCCESS != FindAdapter (RasEntry.szDeviceName, RasEntry.szDeviceType, &pAdapter, &dwDeviceID))
            {
                DEBUGMSG (ZONE_ERROR, (TEXT("ppp: pppRasCntlGetDispPhone: Can't find device '%s'\r\n"), RasEntry.szDeviceName));
                RetVal = ERROR_DEVICE_DOES_NOT_EXIST;
                break;
            }

            dwFlag = 0;
            if (RasEntry.dwfOptions & RASEO_UseCountryAndAreaCodes)
                dwFlag = LINETRANSLATEOPTION_FORCELD;
            else if (RasEntry.dwfOptions & RASEO_DialAsLocalCall)
                dwFlag = LINETRANSLATEOPTION_FORCELOCAL;
            
            // Now attempt to translate the number
            szDialStr = NdisTapiLineTranslateAddress (pAdapter, dwDeviceID,
                RasEntry.dwCountryCode,
                RasEntry.szAreaCode,
                RasEntry.szLocalPhoneNumber,
                FALSE, dwFlag);

            AdapterDelRef (pAdapter);
                
            if (szDialStr)
            {
                __try
                {
                    // Now copy the result back
                    _tcsncpy ((LPTSTR)pBufOut, szDialStr, dwLenOut/sizeof(TCHAR));
                }
                __except(EXCEPTION_EXECUTE_HANDLER) 
                {
                    RetVal = ERROR_INVALID_PARAMETER;
                }
                LocalFree (szDialStr);
            }
            else
            {
                RetVal = ERROR_CANNOT_LOAD_STRING;
            }
        }
        else if (0 == _tcscmp (RasEntry.szDeviceType, RASDT_Vpn))
        {
            __try
            {
                PREFAST_SUPPRESS(419, "The buffer is the user supplied one");
                _tcsncpy ((LPTSTR)pBufOut, RasEntry.szLocalPhoneNumber, dwLenOut / sizeof(TCHAR));
            }
            __except(EXCEPTION_EXECUTE_HANDLER) 
            {
                RetVal = ERROR_INVALID_PARAMETER;
            }
        }
        else
        {
            // For other types just return the null string
            *(LPTSTR)pBufOut = TEXT('\0');
        }
    } while (FALSE); // end do

    return RetVal;
}

PPROTOCOL_CONTEXT
pppFindProtocolContext(
    pppSession_t   *pSession,
    DWORD           ProtocolType)
//
//  Find the PROTOCOL_CONTEXT for the specified ProtocolType in the session.
//
{
    PPROTOCOL_CONTEXT pContext = NULL,
                      pEndContext;

    if (ProtocolType != 0)
    {
        pEndContext = &pSession->pRegisteredProtocolTable[pSession->numRegisteredProtocols];
        for (pContext = &pSession->pRegisteredProtocolTable[0]; TRUE; pContext++)
        {
            if (pContext >= pEndContext)
            {
                pContext = NULL;
                break;
            }
            if (pContext->Type == ProtocolType)
            {
                // Matched, return pContext.
                break;
            }
        }
    }
    return pContext;
}

DWORD
pppLayerOpen(
    PPROTOCOL_CONTEXT pContext)
{
    DWORD RetVal = ERROR_INVALID_PARAMETER;

    if (pContext->pDescriptor->Open)
        RetVal = pContext->pDescriptor->Open(pContext->Context);

    return RetVal;
}

DWORD
pppLayerClose(
    PPROTOCOL_CONTEXT pContext)
{
    DWORD RetVal = ERROR_INVALID_PARAMETER;

    if (pContext->pDescriptor->Close)
        RetVal = pContext->pDescriptor->Close(pContext->Context);

    return RetVal;
}

DWORD
pppLayerRenegotiate(
    PPROTOCOL_CONTEXT pContext)
{
    DWORD RetVal = ERROR_INVALID_PARAMETER;

    if (pContext->pDescriptor->Renegotiate)
        RetVal = pContext->pDescriptor->Renegotiate(pContext->Context);

    return RetVal;
}

DWORD
pppLayerCommand(
    LPVOID  hRasConn,
    PBYTE   pBufIn,
    DWORD   dwLenIn,
    DWORD  (*pfnCommand)(PPROTOCOL_CONTEXT pContext))
//
//  For the specified connection, execute the specified layer command.
//
{
    DWORD             RetVal     = ERROR_INVALID_PARAMETER;
    DWORD             ProtocolType;
    pppSession_t     *s_p = (pppSession_t *)hRasConn;
    PPROTOCOL_CONTEXT pContext;

    if ((dwLenIn < sizeof(DWORD)) || (NULL == pBufIn) || (NULL == hRasConn))
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppLayerCommand : Invalid Parameter\r\n")));
    }
    else if (PPPADDREF (s_p, REF_GETRASSTATS))
    {
        pppLock(s_p);
        __try
        {
            ProtocolType = *(DWORD *)pBufIn;
            pContext = pppFindProtocolContext(s_p, ProtocolType);
            if (pContext)
            {
                RetVal = pfnCommand(pContext);
            }
            else
            {
                DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppLayerCommand : Unsupported protocol %x\r\n"), ProtocolType));
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            RetVal = ERROR_INVALID_PARAMETER;
        }

        pppUnLock(s_p);
        PPPDELREF(s_p, REF_GETRASSTATS);
    }

    return RetVal;
}


DWORD
pppLayerParameterGetOrSetCommand(
    LPVOID  hRasConn,
    PBYTE   pBufIn,
    DWORD   dwLenIn,
    PBYTE   pBufOut, 
    DWORD   dwLenOut,
    PDWORD  pdwActualOut,
    BOOL    bDoGet)
{
    DWORD                     RetVal     = ERROR_INVALID_PARAMETER;
    pppSession_t             *s_p = (pppSession_t *)hRasConn;
    PRASCNTL_LAYER_PARAMETER  pParmIn,
                              pParmOut;
    PPROTOCOL_CONTEXT         pContext;

    if ((dwLenIn < sizeof(RASCNTL_LAYER_PARAMETER)) || (NULL == pBufIn) || (NULL == hRasConn))
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppLayerCommand : Invalid Parameter\r\n")));
    }
    else if (PPPADDREF (s_p, REF_GETRASPARAMETER))
    {
        pppLock(s_p);

        __try
        {
            pParmIn  = (PRASCNTL_LAYER_PARAMETER)pBufIn;
            pParmOut = (PRASCNTL_LAYER_PARAMETER)pBufOut;
            pContext = pppFindProtocolContext(s_p, pParmIn->dwProtocolType);
            if (pContext)
            {
                if (bDoGet)
                {
                    if (pContext->pDescriptor->GetParameter)
                    {
                        pParmOut->dwProtocolType = pParmIn->dwProtocolType;
                        pParmOut->dwParameterId = pParmIn->dwParameterId;
                        *pdwActualOut = dwLenOut;
                        RetVal = pContext->pDescriptor->GetParameter(pContext->Context, pParmOut, pdwActualOut);
                    }
                }
                else
                {
                    if (pContext->pDescriptor->SetParameter)
                    {
                        RetVal = pContext->pDescriptor->SetParameter(pContext->Context, pParmIn, dwLenIn);
                    }
                }
            }
            else
            {
                DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppLayerCommand : Unsupported protocol %x\r\n"), pParmIn->dwProtocolType));
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            RetVal = ERROR_INVALID_PARAMETER;
        }

        pppUnLock(s_p);
        PPPDELREF(s_p, REF_GETRASPARAMETER);
    }

    return RetVal;
}

DWORD
ConvertHResultToErrorCode(
    IN HRESULT hResult)
{
    DWORD ErrorCode;

    switch (hResult)
    {
        case E_OUTOFMEMORY: ErrorCode = ERROR_OUTOFMEMORY;       break;
        default:            ErrorCode = ERROR_INVALID_PARAMETER; break;
    }

    return ErrorCode;
}

// @doc INTERNAL

/*****************************************************************************
* 
*   @func   DWORD | RasIOControl | RAS IO Control Entry point
*
*   @rdesc  Returns SUCCESS if successful, non-zero error code otherwise
*   
*   @parm DWORD | dwOpenData | value returned from ttt_Open call
*   @parm DWORD | dwCode | io control code to be performed
*   @parm PBYTE | pBufIn | input data to the device
*   @parm DWORD | dwLenIn | number of bytes being passed in
*   @parm PBYTE | pBufOut | output data from the device
*   @parm DWORD | dwLenOut |maximum number of bytes to receive from device
*   @parm PDWORD | pdwActualOut | actual number of bytes received from device
*               
*/

DWORD APIENTRY
AfdRasIOControl( 
    LPVOID  hRasConn,                           // handle - if available
    DWORD   dwCode,                             // request code
    PBYTE   pCallerBufIn,
    DWORD   dwLenIn, 
    PBYTE   pCallerBufOut, 
    DWORD   dwLenOut,
    PDWORD  pdwCallerActualOut
    )
{
    DWORD   RetVal = ERROR_INVALID_PARAMETER;
    PBYTE   pBufIn  = NULL,
            pBufOut = NULL;
    DWORD   dwActualOut = dwLenOut;
    PDWORD  pdwActualOut = &dwActualOut;
    HRESULT hResult;
    DWORD   BufInType = ARG_IO_PTR;
    RASPROJECTION ProjectionType;

    DEBUGMSG (ZONE_RAS | ZONE_FUNCTION,
              (TEXT("ppp:+AfdRasIOControl( 0x%X, %d, 0x%X, %d, 0x%X, %d, 0x%X )\r\n"),
               hRasConn, dwCode, pCallerBufIn, dwLenIn, pCallerBufOut, dwLenOut, 
               pdwActualOut));
    
    if (!g_bPPPInitialized)
    {
        // PPPInitialize has not been called yet, the system
        // is still starting up.
        RetVal = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    //
    //  Many RAS IOCTLs use the input buffer as a read only source of data. Some
    //  RAS IOCTLs use the input buffer as an output buffer as well. This switch
    //  statement handles the differences in the usage of the parameters by the
    //  different IOCTL types.
    // 
    switch ((RasCntlEnum_t )dwCode)
    {
    case RASCNTL_GETDISPPHONE:
    case RASCNTL_EAP_GET_USER_DATA:
    case RASCNTL_EAP_SET_USER_DATA:
    case RASCNTL_EAP_GET_CONNECTION_DATA:
    case RASCNTL_EAP_SET_CONNECTION_DATA:
    case RASCNTL_SERVER_SET_PARAMETERS:
    case RASCNTL_SERVER_SET_IPV6_NET_PREFIX:
    case RASCNTL_SERVER_LINE_ADD:
    case RASCNTL_SERVER_LINE_REMOVE:
    case RASCNTL_SERVER_LINE_ENABLE:
    case RASCNTL_SERVER_LINE_DISABLE:
    case RASCNTL_SERVER_LINE_GET_PARAMETERS:
    case RASCNTL_SERVER_LINE_SET_PARAMETERS:
    case RASCNTL_SERVER_LINE_GET_CONNECTION_INFO:
    case RASCNTL_SERVER_USER_SET_CREDENTIALS:
    case RASCNTL_SERVER_USER_DELETE_CREDENTIALS:
    case RASCNTL_LAYER_PARAMETER_GET:
    case RASCNTL_LAYER_PARAMETER_SET:
    case RASCNTL_LAYER_OPEN:
    case RASCNTL_LAYER_CLOSE:
    case RASCNTL_LAYER_RENEGOTIATE:
        // All these IOCTLs use the input buffer for input data only, not for output.
        BufInType = ARG_I_PTR;
        break;
    
    case RASCNTL_ENUMDEV:
        // pCallerBufOut is actually a pointer to a DWORD containing the input buffer length
        if (NULL == pCallerBufOut)
            goto done;

        dwLenIn = *(PDWORD)pCallerBufOut;
        dwLenOut = sizeof(DWORD);
        break;
    
    case RASCNTL_DEVCONFIGDIALOGEDIT:
        // pCallerBufOut should point to a VARSTRING
        if (NULL == pCallerBufOut)
            goto done;

        dwLenOut = ((LPVARSTRING)pCallerBufOut)->dwTotalSize;
        BufInType = ARG_I_PTR;
        break;
        
    case RASCNTL_GETPROJINFO:
        // dwLenIn actually contains an enumerated type, RASPROJECTION.
        // There is no real input buffer.
        ProjectionType = (RASPROJECTION)dwLenIn;
        dwLenIn = 0;
        
        // pCallerBufIn is actually the output buffer.
        if (NULL == pCallerBufIn)
            goto done;
        pCallerBufOut = pCallerBufIn;
        pCallerBufIn = NULL;
        
        // *pdwCallerActualOut contains the size of this output buffer.
        if (NULL == pdwCallerActualOut)
            goto done;
        dwActualOut = dwLenOut = *pdwCallerActualOut;
        break;
    }

    // Make a safe copy of the input buffer
    if (pCallerBufIn && dwLenIn)
    {
        hResult = CeAllocDuplicateBuffer(&pBufIn, pCallerBufIn, dwLenIn, BufInType);
        if (hResult != S_OK)
        {
            RetVal = ConvertHResultToErrorCode(hResult);
            goto done;
        }
    }

    // Make a safe copy of the output buffer
    if (pCallerBufOut && dwLenOut)
    {
        hResult = CeAllocDuplicateBuffer(&pBufOut, pCallerBufOut, dwLenOut, ARG_IO_PTR);
        if (hResult != S_OK)
        {
            RetVal = ConvertHResultToErrorCode(hResult);
            goto done;
        }
    }

    switch( (RasCntlEnum_t )dwCode ) 
    {
    case RASCNTL_STATISTICS:
        if ((dwLenOut < sizeof(RAS_STATS)) || (NULL == pBufOut) || (NULL == hRasConn))
        {
            RetVal = ERROR_INVALID_PARAMETER;
            DEBUGMSG (ZONE_ERROR, (TEXT("ppp: AfdRasIOControl : Invalid Parameter\r\n")));
            break;
        }
        RetVal = pppGetRasStats (hRasConn, (PRAS_STATS)pBufOut);
        *pdwActualOut = sizeof(RAS_STATS);
        break;
        
    case RASCNTL_SET_DEBUG:
    case RASCNTL_LOCK_STATUS:
    case RASCNTL_PRINT_CS:
        // deprecated debug ioctls
        RetVal = ERROR_INVALID_PARAMETER;
        break;

    case RASCNTL_ENUMDEV :
        // Serious abuse of parameters to the function.
        DEBUGMSG (ZONE_WARN, (TEXT("RasEnumDevices(0x%X, 0x%X, 0x%X)\r\n"),
                              pBufIn, pBufOut, pdwActualOut));

        RetVal = pppMac_EnumDevices((LPRASDEVINFOW)pBufIn, FALSE, (LPDWORD)pBufOut, (LPDWORD)pdwActualOut);
        break;

    case RASCNTL_GETPROJINFO:
        RetVal = AfdRasGetProjectionInfo((HRASCONN)hRasConn, ProjectionType, pBufOut, pdwActualOut);
        break;

    case RASCNTL_GETDISPPHONE :
        RetVal = pppRasCntlGetDispPhone(pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut);
        break;
    
    case RASCNTL_DEVCONFIGDIALOGEDIT :
        if ((dwLenIn < sizeof(RASCNTL_DEVCFGDLGED)) || (NULL == pBufIn) ||
            (NULL == pBufOut))
        {
            RetVal = ERROR_INVALID_PARAMETER;
            DEBUGMSG (ZONE_ERROR, (TEXT("ppp: AfdRasIOControl : Invalid Parameter\r\n")));
        }
        else
        {
            PRASCNTL_DEVCFGDLGED pDevCfgDlgEdit = (PRASCNTL_DEVCFGDLGED)pBufIn;

            RetVal = PPPDevConfigDialogEdit(
                        pDevCfgDlgEdit->szDeviceName,
                        pDevCfgDlgEdit->szDeviceType,
                        pDevCfgDlgEdit->hWndOwner,
                        pDevCfgDlgEdit->dwSize ? pDevCfgDlgEdit->DataBuf : NULL,
                        pDevCfgDlgEdit->dwSize,
                        (LPVARSTRING)pBufOut);
        }
        break;

    case RASCNTL_SERVER_GET_STATUS :
        RetVal = PPPServerGetStatus((PRASCNTL_SERVERSTATUS)pBufOut, dwLenOut, pdwActualOut);
        break;

    case RASCNTL_SERVER_ENABLE :
    case RASCNTL_SERVER_DISABLE :
        RetVal = PPPServerSetEnableState(dwCode == RASCNTL_SERVER_ENABLE);
        break;

    case RASCNTL_SERVER_GET_PARAMETERS :
        RetVal = PPPServerGetParameters((PRASCNTL_SERVERSTATUS)pBufOut, dwLenOut, pdwActualOut);
        break;

    case RASCNTL_SERVER_SET_PARAMETERS :
        RetVal = PPPServerSetParameters((PRASCNTL_SERVERSTATUS)pBufIn, dwLenIn);
        break;

    case RASCNTL_SERVER_GET_IPV6_NET_PREFIX :
        RetVal = PPPServerGetIPV6NetPrefix((PRASCNTL_SERVER_IPV6_NET_PREFIX)pBufOut, dwLenOut, pdwActualOut);
        break;
        
    case RASCNTL_SERVER_SET_IPV6_NET_PREFIX :
        RetVal = PPPServerSetIPV6NetPrefix((PRASCNTL_SERVER_IPV6_NET_PREFIX)pBufIn, dwLenIn);
        break;

    case RASCNTL_SERVER_LINE_ADD :
        RetVal = PPPServerLineAdd((PRASCNTL_SERVERLINE)pBufIn, dwLenIn);
        break;

    case RASCNTL_SERVER_LINE_REMOVE :
    case RASCNTL_SERVER_LINE_ENABLE :
    case RASCNTL_SERVER_LINE_DISABLE :
    case RASCNTL_SERVER_LINE_GET_PARAMETERS :
    case RASCNTL_SERVER_LINE_SET_PARAMETERS :
    case RASCNTL_SERVER_LINE_GET_CONNECTION_INFO :
        RetVal = PPPServerLineIoCtl(dwCode, (PRASCNTL_SERVERLINE)pBufIn, dwLenIn, (PRASCNTL_SERVERLINE)pBufOut, dwLenOut, pdwActualOut);
        break;

    case RASCNTL_SERVER_USER_SET_CREDENTIALS :
        RetVal = PPPServerUserSetCredentials((PRASCNTL_SERVERUSERCREDENTIALS)pBufIn, dwLenIn);
        break;

    case RASCNTL_SERVER_USER_DELETE_CREDENTIALS :
        RetVal = PPPServerUserDeleteCredentials((PRASCNTL_SERVERUSERCREDENTIALS)pBufIn, dwLenIn);
        break;

    case RASCNTL_EAP_GET_USER_DATA:
        RetVal = rasGetEapUserData(NULL, NULL, (PWSTR)pBufIn, pBufOut, pdwActualOut);
        break;

    case RASCNTL_EAP_SET_USER_DATA:
        RetVal = rasSetEapUserData(NULL, NULL, (PWSTR)pBufIn, pBufOut, dwLenOut);
        break;

    case RASCNTL_EAP_GET_CONNECTION_DATA:
        RetVal = rasGetEapConnectionData(NULL, (PWSTR)pBufIn, pBufOut, pdwActualOut);
        break;

    case RASCNTL_EAP_SET_CONNECTION_DATA:
        RetVal = rasSetEapConnectionData(NULL, (PWSTR)pBufIn, pBufOut, dwLenOut);
        break;

    case RASCNTL_LAYER_PARAMETER_GET:
        RetVal = pppLayerParameterGetOrSetCommand(hRasConn, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut, TRUE);
        break;

    case RASCNTL_LAYER_PARAMETER_SET:
        RetVal = pppLayerParameterGetOrSetCommand(hRasConn, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut, FALSE);
        break;

    case RASCNTL_LAYER_OPEN:
        RetVal = pppLayerCommand(hRasConn, pBufIn, dwLenIn, pppLayerOpen);
        break;

    case RASCNTL_LAYER_CLOSE:
        RetVal = pppLayerCommand(hRasConn, pBufIn, dwLenIn, pppLayerClose);
        break;

    case RASCNTL_LAYER_RENEGOTIATE:
        RetVal = pppLayerCommand(hRasConn, pBufIn, dwLenIn, pppLayerRenegotiate);
        break;

    case RASCNTL_ENABLE_LOGGING:  // OBSOLETE
    case RASCNTL_DISABLE_LOGGING: // OBSOLETE
    default:
        RetVal = ERROR_INVALID_PARAMETER;
        break;
    }

done:
    //
    // Free the copy of the input buffer if we made one. Note that if the input buffer data
    // has been changed then this also copies the results back to the caller input buffer 
    // because some IOCTLs use the input buffer to return data back to the caller.
    //

    //
    // If for some reason the caller's buffer has become invalid during the call, then
    // CeFreeDuplicateBuffer will catch the exception and return E_ACCESS_DENIED. We ignore
    // this error.
    //
    (void)CeFreeDuplicateBuffer(pBufIn, pCallerBufIn, dwLenIn, BufInType); 

    // Copy the result back from our output buffer to the caller's output buffer.
    hResult = CeFreeDuplicateBuffer(pBufOut, pCallerBufOut, dwLenOut, ARG_IO_PTR);
    if (E_ACCESSDENIED == hResult)
        RetVal = ERROR_INVALID_PARAMETER;

    if (pdwCallerActualOut)
        *pdwCallerActualOut = dwActualOut;

    DEBUGMSG (ZONE_RAS, (TEXT("ppp: -AfdRasIOControl : RetVal=%d\r\n"), RetVal));
    return RetVal;
}


