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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


 Module Name:    wshirda.c

 Abstract:       Contains winsock helper functions.

 Contents:

--*/

#include "irdatdi.h"

//
// Globals.
//
LIST_ENTRY       g_SocketList;
CRITICAL_SECTION g_csWshIrda;
INT              g_IrdaLinkState;
HANDLE           g_hEvWaitToRelLink;
CTEEvent         g_evReleaseLink;


///
// IrdaLinkStates
//
#define IRDALINKSTATE_CLOSED  1
#define IRDALINKSTATE_CLOSING 2
#define IRDALINKSTATE_OPENED  3
#define IRDALINKSTATE_OPENING 4
#define IRLMP_OPENSOCKET_MAX_WAIT 10000

// 
// Prototypes.
//

static INT
TdiEnumDevices(
    PDEVICELIST pDeviceList,
    DWORD       cDevices
    );

static INT
TdiIasQuery(
    PIAS_QUERY pIasQuery,
    DWORD      cbAttrib
    );

static INT
TdiReleaseLink(
    struct CTEEvent *pEvent,
    VOID *pvNULL
    );

static VOID
DeleteSocketAttribs(
    PIRLMP_SOCKET_CONTEXT pSocket
    );

#ifdef DEBUG

#define DUMP_WSHOBJECTS(zone) if (zone) DumpWshObjects()

VOID
DumpWshObjects()
{
    PLIST_ENTRY             pEntry;
    PIRLMP_SOCKET_CONTEXT   pContext;
    PWSHIRDA_IAS_ATTRIB     pAttrib;

    EnterCriticalSection(&g_csWshIrda);

    DEBUGMSG(1,
        (TEXT("*** WSHIRDA Objects : %hs\r\n"), 
         IsListEmpty(&g_SocketList) ? "EMPTY" : ""));
    
    for (pEntry = g_SocketList.Flink;
         pEntry != &g_SocketList;
         pEntry = pEntry->Flink)
    {
        pContext = (PIRLMP_SOCKET_CONTEXT)pEntry;

        DEBUGMSG(1,
            (TEXT("SockContext:%#x, Excl:%hs LPT:%hs, 9Wire:%hs, Sharp:%hs, Next:%#x\r\n"),
             pContext, 
             pContext->UseExclusiveMode == TRUE ? "T" : "F",
             pContext->UseIrLPTMode     == TRUE ? "T" : "F",
             pContext->Use9WireMode     == TRUE ? "T" : "F",
             pContext->UseSharpMode     == TRUE ? "T" : "F",
             pEntry->Flink != &g_SocketList ? pEntry->Flink : NULL));
        
        for (pAttrib = pContext->pIasAttribs;
             pAttrib != NULL;
             pAttrib = pAttrib->pNext)
        {
            DEBUGMSG(1, (TEXT("    Socket attribute handle:%#x, Next:%#x\r\n"), 
                 pAttrib->AttributeHandle,
                 pAttrib->pNext));
        }
    }

    LeaveCriticalSection(&g_csWshIrda);
}

#else // DEBUG

#define DUMP_WSHOBJECTS(zone) ((void)0)

#endif // !DEBUG

/*++

 Function:       WshIrdaInit

 Description:    Initializes the WinSock Helper code.

 Arguments:

    None.

 Returns:

    STATUS_SUCCESS.

 Comments:

--*/

DWORD WshIrdaInit()
{
    DEBUGMSG(ZONE_WSHIRDA | ZONE_INIT, (TEXT("WshIrdaInit\r\n")));
    
    g_hEvWaitToRelLink = CreateEvent(
        NULL,  // security attribs.
        TRUE,  // manual reset event.
        TRUE,  // initial state is signalled.
        NULL); // no name.

    if (g_hEvWaitToRelLink == NULL)
    {
        return (STATUS_INSUFFICIENT_RESOURCES);
    }

    InitializeCriticalSection(&g_csWshIrda);
    InitializeListHead(&g_SocketList);

    CTEInitEvent(&g_evReleaseLink, TdiReleaseLink);

    // WinCE miniports start without resources acquired.
    g_IrdaLinkState  = IRDALINKSTATE_CLOSED;

    return (STATUS_SUCCESS);
}

/*++

 Function:       WshIrdaDeinit

 Description:    Deinitalizes the WinSock Helper code.

 Arguments:

    None.

 Returns:

    STATUS_SUCCESS.

 Comments:

--*/

DWORD WshIrdaDeinit()
{
    PIRLMP_SOCKET_CONTEXT pSocket;
    
    DEBUGMSG(ZONE_WSHIRDA | ZONE_INIT, (TEXT("WshIrdaDeinit\r\n")));

    DUMP_WSHOBJECTS(ZONE_WSHIRDA);

    EnterCriticalSection(&g_csWshIrda);

    for (pSocket = (PIRLMP_SOCKET_CONTEXT) g_SocketList.Flink;
         pSocket != (PIRLMP_SOCKET_CONTEXT) &g_SocketList;
         pSocket = (PIRLMP_SOCKET_CONTEXT) pSocket->Linkage.Flink)
    {
        DeleteSocketAttribs(pSocket);
    }     

    CloseHandle(g_hEvWaitToRelLink);
    g_hEvWaitToRelLink = NULL;
    
    LeaveCriticalSection(&g_csWshIrda);

    DeleteCriticalSection(&g_csWshIrda);

    return (STATUS_SUCCESS);
}


/*****************************************************************************
*
*   @func   INT |   IRLMP_GetSockaddrType |
*           Called after bind(). Returns address information. 
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode WSAEAFNOSUPPORT  | AddressFamily != AF_IRDA.
*           @ecode WSAEFAULT        | SockaddrLen != sizeof(SOCKADDR_IRDA).
*
*   @parm   PSOCKADDR       | pSockaddr     |
*           Pointer to input SOCKADDR (winsock.h) struct.
*   @parm   DWORD           | SockaddrLen   |
*           Length of input SOCKADDR struct.
*   @parm   PSOCKADDR_INFO  | pSockaddrInfo |
*           Pointer to output SOCKADDR_INFO (tdice.h) struct.
*           <nl>pSockaddrInfo.AddressInfo can return
*           <nl>    SockaddrAddressInfoNormal
*           <nl>    SockaddrAddressInfoWildcard
*           <nl>    SockaddrAddressInfoBroadcast
*           <nl>    SockaddrAddressInfoLoopback
*           <nl>pSockaddrInfo.EndPointInfo can return
*           <nl>    SockaddrEndpointInfoNormal
*           <nl>    SockaddrEndpointInfoWildcard
*           <nl>    SockaddrEndpointInfoReserved
*           <nl>IrLmp will return SockaddrAddressInfoNormal and
*           SockaddrEndpointInfoNormal.
*/

INT 
IRLMP_GetSockaddrType(IN  PSOCKADDR      pSockaddr,
                      IN  DWORD          SockaddrLen,
                      OUT PSOCKADDR_INFO pSockaddrInfo)
{
	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_GetSockaddrType(0x%X,0x%X,0x%X)\r\n"),
        pSockaddr, SockaddrLen, pSockaddrInfo));

    if (SockaddrLen < sizeof(SOCKADDR_IRDA))
        return(WSAEFAULT);

    if (AF_IRDA != pSockaddr->sa_family) {
        return WSAEAFNOSUPPORT;
    }

    if (((SOCKADDR_IRDA *) pSockaddr)->irdaServiceName[0] == 0)
    {
        pSockaddrInfo->AddressInfo  = SockaddrAddressInfoWildcard;
        pSockaddrInfo->EndpointInfo = SockaddrEndpointInfoWildcard;
    }
    else
    {
        pSockaddrInfo->AddressInfo  = SockaddrAddressInfoNormal;
        pSockaddrInfo->EndpointInfo = SockaddrEndpointInfoNormal;
    }
    
    return(NO_ERROR);
}

/*****************************************************************************
*
*   @func   INT |   IRLMP_GetWildCardSockaddr           |
*           Called after connect() without previous bind(). Returns a 
*           SOCKADDR_IRDA suitable for an implicit bind().
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode WSAEFAULT    | SockaddrLen != sizeof(SOCKADDR_IRDA).
*
*   @parm   PVOID       |   pHelperDllSocketContext     |
*           Pointer to the IRLMP_SOCKET_CONTEXT returned by IRLMP_OpenSocket().
*   @parm   PSOCKADDR   | pSockaddr                     |
*           Pointer to output SOCKADDR (winsock.h) struct.
*   @parm   PINT        | pSockaddrLen                  |
*           Pointer to input and output PSOCKADDR struct length.
*/

INT
IRLMP_GetWildcardSockaddr(IN  PVOID     pHelperDllSocketContext,
                          OUT PSOCKADDR pSockaddr,
                          OUT PINT      pSockaddrLen)
{
	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("IRLMP_GetWildcardSockaddr(0x%X,0x%X,0x%X)\r\n"),
        pHelperDllSocketContext, pSockaddr, pSockaddrLen));

    if (*pSockaddrLen < (int)sizeof(SOCKADDR_IRDA))
    {
		DEBUGMSG(ZONE_ERROR,
            (TEXT("IRLMP_GetWildcardSockaddr *pSockaddrLen < sizeof(SOCKADDR_IRDA) %d\r\n")));

        return(WSAEFAULT);
    }

    *pSockaddrLen = sizeof(SOCKADDR_IRDA);

    memset(pSockaddr, sizeof(SOCKADDR_IRDA), 0);
    ((SOCKADDR_IRDA *) pSockaddr)->irdaAddressFamily = AF_IRDA;

    return(NO_ERROR);
}

/*****************************************************************************
*
*   @func   INT |   IRLMP_GetSocketInformation          |
*           Called by the Winsock DLL when a level/option name combination
*           is passed to getsockopt() that the winsock DLL does not understand.
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode WSAEINVAL        | Unsupported level.
*           @ecode WSAEFAULT        | pOptionLen in invalid.
*           @ecode WSAENOPROTOOPT   | Unsupported option.  
*
*   @parm   PVOID   | pHelperDllSocketContext   |
*           Pointer to the IRLMP_SOCKET_CONTEXT returned by IRLMP_OpenSocket().
*   @parm   SOCKET  | SocketHandle              |
*           Handle of the socket for which we're getting information.
*   @parm   HANDLE  | TdiAddressObjectHandle    |
*           TDI address object of the socket, or NULL.
*   @parm   HANDLE  | TdiConnectionObjectHandle |
*           TDI connection object of the socket, or NULL.
*   @parm   INT     | Level                     |
*           level, from getsockopt().           
*   @parm   INT     | OptionName                |
*           optname, from getsockopt().
*   @parm   PCHAR   | pOptionValue              |
*           optval, from getsockopt().
*   @parm   PINT    | pOptionLen                |
*           optlen, from getsockopt().
*/

INT
IRLMP_GetSocketInformation(IN  PVOID  pHelperDllSocketContext,
                           IN  SOCKET SocketHandle,
                           IN  HANDLE TdiAddressObjectHandle,
                           IN  HANDLE TdiConnectionObjectHandle,
                           IN  INT    Level,
                           IN  INT    OptionName,
                           OUT PCHAR  pOptionValue,
                           OUT PINT   pOptionLen)
{
    int                   Status = WSAENOPROTOOPT;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_GetSocketInformation(0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"),
        pHelperDllSocketContext, 
        (unsigned) SocketHandle, 
        (unsigned) TdiAddressObjectHandle, 
        (unsigned) TdiConnectionObjectHandle,
        Level, 
        OptionName, 
        pOptionValue, 
        pOptionLen));

    if (Level == SOL_INTERNAL && OptionName == SO_CONTEXT)
    {
        // Copy pHelperDllSocketContext to pOptionValue
        if ((*pOptionLen = sizeof(IRLMP_SOCKET_CONTEXT)) < 
            (int)sizeof(IRLMP_SOCKET_CONTEXT))
        {
            Status = WSAEFAULT;
            goto done;
        }
        
        if (pOptionValue != NULL)
        {
            memcpy(pOptionValue, pHelperDllSocketContext, 
                   sizeof(IRLMP_SOCKET_CONTEXT));
            ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->Linkage.Flink = NULL;
            ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->Linkage.Blink = NULL;
            ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->pIasAttribs   = NULL;
        }

        Status = NO_ERROR;
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_SEND_PDU_LEN)
    {
        PIRLMP_CONNECTION     pConn;
        
        if (*pOptionLen < (int)sizeof(DWORD))
        {
            Status = WSAEFAULT;
            goto done;
        }

        pConn = GETCONN(TdiConnectionObjectHandle);
    
        if (pConn == NULL)
        {
            Status = WSAEFAULT;
            goto done;
        }

        VALIDCONN(pConn);
    
        GET_CONN_LOCK(pConn);

        if (pConn->Use9WireMode)
            *(int *) pOptionValue = pConn->SendMaxPDU - 1;
        else
            *(int *) pOptionValue = pConn->SendMaxPDU;

        FREE_CONN_LOCK(pConn);

        REFDELCONN(pConn);
        Status = NO_ERROR;
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_ENUMDEVICES)
    {
        if (*pOptionLen < (int)sizeof(DWORD))
        {
            Status = WSAEFAULT;
            goto done;
        }
    
        Status = TdiEnumDevices(
            (PDEVICELIST)pOptionValue,
            (*pOptionLen - sizeof(DWORD))/sizeof(IRDA_DEVICE_INFO)
            );
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_IAS_QUERY)
    {
        if (*pOptionLen < (int)(sizeof(IAS_QUERY) - 4))
        {
            Status = WSAEFAULT;
            goto done;
        }

        Status = TdiIasQuery(
            (PIAS_QUERY)pOptionValue,
            *pOptionLen);
    }

done:


    return(Status);
}

/*****************************************************************************
*
*   @func   INT |   IRLMP_SetSocketInformation          |
*           Called by the Winsock DLL when a level/option name combination
*           is passed to setsockopt() that the winsock DLL does not understand.
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode WSAEINVAL        | Unsupported level.
*           @ecode WSAEFAULT        | pOptionLen in invalid.
*           @ecode WSAENOBUFS       | No memory.
*           @ecode WSAENOPROTOOPT   | Unsupported option.  
*
*   @parm   PVOID   | pHelperDllSocketContext   |
*           Pointer to the IRLMP_SOCKET_CONTEXT returned by IRLMP_OpenSocket().
*   @parm   SOCKET  | SocketHandle              |
*           Handle of the socket for which we're getting information.
*   @parm   HANDLE  | TdiAddressObjectHandle    |
*           TDI address object of the socket, or NULL.
*   @parm   HANDLE  | TdiConnectionObjectHandle |
*           TDI connection object of the socket, or NULL.
*   @parm   INT     | Level                     |
*           level, from setsockopt().           
*   @parm   INT     | OptionName                |
*           optname, from setsockopt().
*   @parm   PCHAR   | pOptionValue              |
*           optval, from setsockopt().
*   @parm   INT     | OptionLen                 |
*           optlen, from setsockopt().
*/
	
INT
IRLMP_SetSocketInformation(IN PVOID  pHelperDllSocketContext,
                           IN SOCKET SocketHandle,
                           IN HANDLE TdiAddressObjectHandle,
                           IN HANDLE TdiConnectionObjectHandle,
                           IN INT    Level,
                           IN INT    OptionName,
                           IN PCHAR  pOptionValue,
                           IN INT    OptionLen)
{
    PIRLMP_SOCKET_CONTEXT pContext = 
        (PIRLMP_SOCKET_CONTEXT) pHelperDllSocketContext;
    PIAS_SET              pIASSet  = (PIAS_SET) pOptionValue;
    IRDA_MSG              IrDAMsg;
    int                   rc;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_SetSocketInformation(0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"),
        pHelperDllSocketContext, 
        (unsigned) SocketHandle, 
        (unsigned) TdiAddressObjectHandle,
        (unsigned) TdiConnectionObjectHandle, 
        Level, 
        OptionName, 
        pOptionValue,
        OptionLen));
    
    DUMP_WSHOBJECTS(ZONE_WSHIRDA);

    if (Level == SOL_INTERNAL && OptionName == SO_CONTEXT)
    {
        PIRLMP_SOCKET_CONTEXT pSocketContext = pHelperDllSocketContext;

        if (OptionLen < (int)sizeof(IRLMP_SOCKET_CONTEXT))
            return(WSAEINVAL);
        
        if (pHelperDllSocketContext == NULL)
        {

            // Socket was inherited or duped, create a new context.
            pSocketContext = IrdaAlloc(sizeof(IRLMP_SOCKET_CONTEXT), 
                                       MT_WSH_SOCKET_CTXT);
            if (pSocketContext == NULL)
            {
                DEBUGMSG(ZONE_ERROR,
                    (TEXT("IRLMP_SetSocketInformation() CTEAllocMem() failed\r\n")));
                return(WSAENOBUFS);
            }

            // Copy the parent's context into the child's context.
            memcpy(pSocketContext, pOptionValue, sizeof(IRLMP_SOCKET_CONTEXT));
            pSocketContext->pIasAttribs = NULL;

            // Return the address of the new context in pOptionVal.
            *(PIRLMP_SOCKET_CONTEXT *) pOptionValue = pSocketContext;
            
            EnterCriticalSection(&g_csWshIrda);
            InsertHeadList(&g_SocketList, &pSocketContext->Linkage);
            LeaveCriticalSection(&g_csWshIrda);
        }
        else 
        {
            // The socket was accept()'ed and it needs to have the same 
            // properties as it's parent.  The OptionValue buffer
            // contains the context information of this socket's parent.  
            
            //memcpy(pSocketContext, pOptionValue, 
            //       sizeof(IRLMP_SOCKET_CONTEXT));

            // sh - don't memcpy - there is a link list entry in there!
            //      Also, we don't copy the attributes.

            pSocketContext->UseExclusiveMode = ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->UseExclusiveMode;
            pSocketContext->UseIrLPTMode     = ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->UseIrLPTMode;
            pSocketContext->Use9WireMode     = ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->Use9WireMode;
            pSocketContext->UseSharpMode     = ((PIRLMP_SOCKET_CONTEXT)pOptionValue)->UseSharpMode;
        }
        
        DUMP_WSHOBJECTS(ZONE_WSHIRDA);

        return(NO_ERROR);
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_EXCLUSIVE_MODE)
    {
        if (OptionLen < (int)sizeof(int))
            return(WSAEINVAL);

        pContext->UseExclusiveMode = *(int *) pOptionValue;

        return(NO_ERROR);
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_IRLPT_MODE)
    {
        if (OptionLen < (int)sizeof(int))
            return(WSAEINVAL);

        pContext->UseIrLPTMode = *(int *) pOptionValue;

        return(NO_ERROR);
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_9WIRE_MODE)
    {
        if (OptionLen < (int)sizeof(int))
            return(WSAEINVAL);

        pContext->Use9WireMode = *(int *) pOptionValue;

        return(NO_ERROR);
    }

    if (Level == SOL_IRLMP && OptionName == IRLMP_SHARP_MODE)
    {
        if (OptionLen < (int)sizeof(int))
            return(WSAEINVAL);

        pContext->UseSharpMode = *(int *) pOptionValue;

        return(NO_ERROR);
    }

    /* wmz clear the IAS sometime maybe */

    if (Level == SOL_IRLMP && OptionName == IRLMP_IAS_SET)
    {
        PVOID               AttribHandle;
        PWSHIRDA_IAS_ATTRIB pIasAttrib;
        
        if (OptionLen < (int)(sizeof(IAS_SET) - 4))
            return(WSAEFAULT);
        
        if (pIASSet->irdaClassName[0] == 0)
        {
            return (WSAEINVAL);
        }

        pIASSet->irdaClassName[60]  = 0;
        pIASSet->irdaAttribName[60] = 0;
        
        switch(pIASSet->irdaAttribType)
        {
          case IAS_ATTRIB_INT:
            break;
            
          case IAS_ATTRIB_OCTETSEQ:
            if (OptionLen - 
                offsetof(IAS_SET, irdaAttribute.irdaAttribOctetSeq.OctetSeq) <
                (unsigned) pIASSet->irdaAttribute.irdaAttribOctetSeq.Len)
            {
                return(WSAEFAULT);
            }                
            if (pIASSet->irdaAttribute.irdaAttribOctetSeq.Len > 1024 ||
                pIASSet->irdaAttribute.irdaAttribOctetSeq.Len < 0)
            {
                return (WSAEINVAL);
            }
            break;

          case IAS_ATTRIB_STR:
            if (OptionLen - 
                offsetof(IAS_SET, irdaAttribute.irdaAttribUsrStr.UsrStr) <
                (unsigned) pIASSet->irdaAttribute.irdaAttribUsrStr.Len)
            {
                return(WSAEFAULT);
            }
            if (pIASSet->irdaAttribute.irdaAttribUsrStr.Len > 255 ||
                pIASSet->irdaAttribute.irdaAttribUsrStr.Len < 0)
            {
                return (WSAEINVAL);
            }

            pIASSet->irdaAttribute.irdaAttribUsrStr.UsrStr[
                pIASSet->irdaAttribute.irdaAttribUsrStr.Len] = 0;
            break;
            
          default:
            return(WSAEFAULT);
            break;
        }

        pIasAttrib = IrdaAlloc(sizeof(WSHIRDA_IAS_ATTRIB), 0);

        if (pIasAttrib == NULL)
        {
            return (WSAENOBUFS);
        }

        IrDAMsg.Prim                   = IRLMP_ADDATTRIBUTE_REQ;
        IrDAMsg.IRDA_MSG_pIasSet       = pIASSet;
        IrDAMsg.IRDA_MSG_pAttribHandle = &AttribHandle;

        rc = IrlmpDown(NULL, &IrDAMsg);

        if (rc == IRLMP_IAS_ATTRIB_ALREADY_EXISTS ||
            rc == IRLMP_IAS_MAX_ATTRIBS_REACHED)
        {
            IrdaFree(pIasAttrib);
            return (WSAEINVAL);
        }
        else if (rc != SUCCESS)
        {
            IrdaFree(pIasAttrib);
            return (WSAENOBUFS);
        }

        ASSERT(AttribHandle != NULL);
        pIasAttrib->AttributeHandle = AttribHandle;

        EnterCriticalSection(&g_csWshIrda);

        DEBUGMSG(ZONE_WSHIRDA, (TEXT("Added attrib %x to socket %x\r\n"),
            AttribHandle, pContext));
                    
        pIasAttrib->pNext     = pContext->pIasAttribs;
        pContext->pIasAttribs = pIasAttrib;

        LeaveCriticalSection(&g_csWshIrda);

        return(NO_ERROR);
    }

    return(WSAENOPROTOOPT);
}

/*****************************************************************************
*
*   @func   INT |   IRLMP_Notify |
*           Called after the socket event(s) specified in IRLMP_OpenSocket()
*           occur.
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode  WSAEINVAL   |   Invalid NotifyEvent.
*
*   @parm   PVOID   |   pHelperDllSocketContext     |
*           Pointer to the IRLMP_SOCKET_CONTEXT returned by IRLMP_OpenSocket().
*   @parm   SOCKET  |   SocketHandle                |   Socket handle.
*   @parm   HANDLE  |   TdiAddressObjectHandle      |
*           TDI Address Object handle if the socket is bound, otherwise NULL.
*   @parm   HANDLE  |   TdiConnectionObjectHandle   |
*           TDI Connection Object handle if socket is connected, 
*           otherwise NULL.
*   @parm   DWORD   |   NotifyEvent                 |
*           (tdice.h) One of
*		    <nl>    WSH_NOTIFY_BIND
*		    <nl>    WSH_NOTIFY_LISTEN
*		    <nl>    WSH_NOTIFY_CONNECT
*		    <nl>    WSH_NOTIFY_ACCEPT
*		    <nl>    WSH_NOTIFY_SHUTDOWN_RECEIVE
*		    <nl>    WSH_NOTIFY_SHUTDOWN_SEND
*		    <nl>    WSH_NOTIFY_SHUTDOWN_ALL
*		    <nl>    WSH_NOTIFY_CLOSE
*		    <nl>    WSH_NOTIFY_CONNECT_ERROR
*
*   @xref   <f IRLMP_OpenSocket>
*/

INT 
IRLMP_Notify(IN PVOID  pHelperDllSocketContext,
             IN SOCKET SocketHandle,
             IN HANDLE TdiAddressObjectHandle,
             IN HANDLE TdiConnectionObjectHandle,
             IN DWORD  NotifyEvent)
{
    PIRLMP_SOCKET_CONTEXT pContext = 
        (PIRLMP_SOCKET_CONTEXT) pHelperDllSocketContext;
    IRDA_MSG          IrDAMsg;
    int               rc;
    IAS_SET           IASSet;
    int               i;

	DEBUGMSG(ZONE_FUNCTION || ZONE_WSHIRDA,
        (TEXT("IRLMP_Notify(0x%X,0x%X,0x%X,0x%X,%s - 0x%x)\r\n"),
        pHelperDllSocketContext, 
        (unsigned) SocketHandle,
        (unsigned) TdiAddressObjectHandle,     
        (unsigned) TdiConnectionObjectHandle, 
        NotifyEvent == WSH_NOTIFY_BIND    ? TEXT("WSH_NOTIFY_BIND") :
        NotifyEvent == WSH_NOTIFY_LISTEN  ? TEXT("WSH_NOTIFY_LISTEN") :
        NotifyEvent == WSH_NOTIFY_CLOSE   ? TEXT("WSH_NOTIFY_CLOSE") :
            TEXT("unexpected WSH_NOTIFY"), 
        NotifyEvent));

    DUMP_WSHOBJECTS(ZONE_WSHIRDA);

    if (NotifyEvent == WSH_NOTIFY_BIND)
    {
        PIRLMP_ADDR_OBJ pAddrObj;

        pAddrObj = GETADDR(TdiAddressObjectHandle);

        if (pAddrObj == NULL)
        {
            return (WSAEFAULT);
        }

        pAddrObj->UseExclusiveMode = pContext->UseExclusiveMode;
        pAddrObj->UseIrLPTMode     = pContext->UseIrLPTMode;
        pAddrObj->Use9WireMode     = pContext->Use9WireMode;
        pAddrObj->UseSharpMode     = pContext->UseSharpMode;
    
        REFDELADDR(pAddrObj);
    }

    if (NotifyEvent == WSH_NOTIFY_LISTEN)
    {
        PIRLMP_ADDR_OBJ pAddrObj;

        pAddrObj = GETADDR(TdiAddressObjectHandle);

        if (pAddrObj == NULL)
        {
            return (WSAEFAULT);
        }

        pAddrObj->IsServer  = TRUE;

        IrDAMsg.Prim                  = IRLMP_REGISTERLSAP_REQ;
        IrDAMsg.IRDA_MSG_LocalLsapSel = pAddrObj->LSAPSelLocal;

        if (pAddrObj->UseIrLPTMode || pAddrObj->UseExclusiveMode)
            IrDAMsg.IRDA_MSG_UseTtp   = FALSE;
        else
            IrDAMsg.IRDA_MSG_UseTtp   = TRUE;

        rc = IrlmpDown(NULL, &IrDAMsg);
        ASSERT(rc == 0);

        // BUGBUG sh: probably shouldn't add the service name to the
        //            IAS database if it is a hardcoded LSAP. i.e.
        //            LSAP-SELXxx.
            
        i = 0;
        // irdaServiceName is 25 chars, but will truncate with 24 + 1 NULL.
        while (pAddrObj->SockAddrLocal.irdaServiceName[i] && i < 25)
        {
            IASSet.irdaClassName[i] = 
                pAddrObj->SockAddrLocal.irdaServiceName[i];
            i++;
        }
        IASSet.irdaClassName[i] = 0;

        i = 0;
        while (IasAttribName_TTPLsapSel[i])
        {
            IASSet.irdaAttribName[i] = IasAttribName_TTPLsapSel[i];
            i++;
        }
        IASSet.irdaAttribName[i] = 0;

        IASSet.irdaAttribType               = IAS_ATTRIB_INT;
        IASSet.irdaAttribute.irdaAttribInt  = pAddrObj->LSAPSelLocal;

        IrDAMsg.Prim                        = IRLMP_ADDATTRIBUTE_REQ;
        IrDAMsg.IRDA_MSG_pIasSet            = &IASSet;
        IrDAMsg.IRDA_MSG_pAttribHandle      = &pAddrObj->IasAttribHandle;

        rc = IrlmpDown(NULL, &IrDAMsg);
        ASSERT(rc == 0);

        REFDELADDR(pAddrObj);
    }

    if (NotifyEvent == WSH_NOTIFY_CLOSE)
    {
        PIRLMP_SOCKET_CONTEXT pSocket = (PIRLMP_SOCKET_CONTEXT)pHelperDllSocketContext;

        EnterCriticalSection(&g_csWshIrda);
        DeleteSocketAttribs(pSocket);
        RemoveEntryList(&pSocket->Linkage);
        LeaveCriticalSection(&g_csWshIrda);

        IrdaFree(pSocket);

        EnterCriticalSection(&g_csWshIrda);

        OpenSocketCnt--;
        DEBUGMSG(ZONE_SOCKETCNT, (TEXT("IrDA: Open sockets = %d -> %d.\r\n"), 
            OpenSocketCnt + 1, OpenSocketCnt));
        ASSERT((int)OpenSocketCnt >= 0);

        if (OpenSocketCnt == 0) {
            if (g_IrdaLinkState == IRDALINKSTATE_OPENED) {
                g_IrdaLinkState = IRDALINKSTATE_CLOSING;
                ResetEvent(g_hEvWaitToRelLink);
                if (CTEScheduleEvent(&g_evReleaseLink, NULL) == 0)
                {
                    ASSERT(FALSE);
                    LeaveCriticalSection(&g_csWshIrda);
                    // Fail to release link resources.
                    return (WSAEBADF);
                }
            }
        }

        LeaveCriticalSection(&g_csWshIrda);
    }

    return(NO_ERROR);
}

/*****************************************************************************
*
*   @func   DWORD   |   IRLMP_GetWinsockMapping |
*           Called at stack load time. Returns a WINSOCK_MAPPING (tdice.h) 
*           struct containing the AddressFamily, SocketType and Protocol 
*           triple socket() parms supported by this stack.
*
*	@rdesc
*           Required WINSOCK_MAPPING struct size. The function is called again 
*           with a larger buffer if needed.
*
*   @parm   PWINSOCK_MAPPING    | pMapping          |
*           Pointer to output WINSOCK_MAPPING struct. IrLMP will copy
*           IRLMP_Mapping to this address.
*   @parm   DWORD               |   MappingLength   |
*           sizeof() output WINSOCK_MAPPING structure. If this value is too
*           small, GetWinsockMapping() will return the required buffer size.
*/

const DWORD IRLMP_Mapping[] = { 1, 3, AF_IRDA, SOCK_STREAM, 0};

DWORD
IRLMP_GetWinsockMapping(OUT PWINSOCK_MAPPING pMapping,
                        IN  DWORD            MappingLength)
{
    DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("IRLMP_GetWinsockMapping(0x%X,0x%X)\r\n"),
        pMapping, MappingLength));

    if (MappingLength >= sizeof(IRLMP_Mapping))
        memcpy(pMapping, &IRLMP_Mapping, sizeof(IRLMP_Mapping));

    return(sizeof(IRLMP_Mapping));
}   

/*****************************************************************************
*
*   @func   INT | IRLMP_EnumProtocols   | Called at stack load time.
*/

INT
IRLMP_EnumProtocols(IN     LPINT   lpiProtocols,
                    IN     LPTSTR  lpTransportKeyName,
                    IN OUT LPVOID  lpProtocolBuffer,
                    IN OUT LPDWORD lpdwBufferLength)
{
    int            i;
    BOOL           useIR = FALSE;
    PPROTOCOL_INFO pProtocolInfo;
    DWORD          bytesRequired;
    LPWSTR         pProtName;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_EnumProtocols(0x%X,0x%X,0x%X,0x%X)\r\n"),
        lpiProtocols, 
        lpTransportKeyName,
        lpProtocolBuffer, 
        lpdwBufferLength));

    // Should we check for correct procotol in lpiProtocols????
    if (lpiProtocols != NULL) 
    {
        for (i = 0; lpiProtocols[i] != 0; i++) 
        {
            if (AF_IRDA == lpiProtocols[i]) {
                useIR = TRUE;
            }
        }
    }
    else 
    {
        // If no proto's specified then Set true.
        useIR = TRUE;
    }    
    
    // Did they specify a valid protocol?
    if (!useIR) 
    {
        *lpdwBufferLength = 0;
        return 0;
    }
    
    // This only supports UNICODE.....
    bytesRequired = sizeof(PROTOCOL_INFO) + (wcslen (IRLMP_TRANSPORT_NAME) + 1) *sizeof(WCHAR);
    
    // Did they give us enough room?
    if (bytesRequired > *lpdwBufferLength) 
    {
        *lpdwBufferLength = bytesRequired;
        return -1;
    }
    
    pProtocolInfo = lpProtocolBuffer;
    pProtName     = (LPWSTR)((DWORD)lpProtocolBuffer + (sizeof(PROTOCOL_INFO)));

    pProtocolInfo->dwServiceFlags = 
        XP_GUARANTEED_DELIVERY |
        XP_GUARANTEED_ORDER;
    pProtocolInfo->iAddressFamily = AF_IRDA;
    pProtocolInfo->iMaxSockAddr   = sizeof(SOCKADDR_IRDA);
    pProtocolInfo->iMinSockAddr   = sizeof(SOCKADDR_IRDA);
    pProtocolInfo->iSocketType    = SOCK_STREAM;
    pProtocolInfo->iProtocol      = AF_IRDA;
    pProtocolInfo->dwMessageSize  = 0;
    pProtocolInfo->lpProtocol     = pProtName;

    // Copy the name
    wcscpy (pProtName, IRLMP_TRANSPORT_NAME);

    // Set return buffer length
    *lpdwBufferLength = bytesRequired;
    
    // Indicate 1 protocol info structures returned.
    return 1;
}

/*****************************************************************************
*
*   @func   INT |   IRLMP_OpenSocket |
*           Called after socket(). Validates socket() parms and allocates a 
*           new IRLMP_SOCKET_CONTEXT.
*
*	@rdesc
*           NO_ERROR (winerror.h) on success, or (winsock.h)
*   @errors
*           @ecode WSAEINVAL    |   Invalid socket() parameters.
*           @ecode WSAENOBUFS   |   No memory to allocate socket context.
*
*   @parm   PINT    |   pAddressFamily              |   
*           From socket(AF_IRDA, , ). IrLMP will return this value unchanged.
*   @parm   PINT    |   pSocketType                 | 
*           From socket( , SOCK_SEQPACKET, ). IrLMP will return this value 
*           unchanged.
*   @parm   PINT    |   pProtocol                   | 
*           From socket( , , 0). IrLMP will return this value unchanged.
*   @parm   PWSTR   |   pTransportDeviceName        |
*           IrLMP will copy IRLMP_TRANSPORT_NAME to this address.
*   @parm   PVOID * |   ppHelperDllSocketContext    |
*           Returns the address of a new IRLMP_SOCKET_CONTEXT. This context
*           will be returned by IRLMP_Notify(), IRLMP_OpenConnection(), 
*           IRLMP_GetWildcardSockaddr(), and IRLMP_SetSocketInformation(),
*           and IRLMP_GetSocketInformation(). 
*   @parm   PDWORD  |   pNotificationEvents         |
*           (tdice.h) Bitmask indicating which Winsock events should result
*           in a call to IRLMP_Notify(). IrLMP will set this value to 0.
*		    <nl>    WSH_NOTIFY_BIND
*		    <nl>    WSH_NOTIFY_LISTEN
*		    <nl>    WSH_NOTIFY_CONNECT
*		    <nl>    WSH_NOTIFY_ACCEPT
*		    <nl>    WSH_NOTIFY_SHUTDOWN_RECEIVE
*		    <nl>    WSH_NOTIFY_SHUTDOWN_SEND
*		    <nl>    WSH_NOTIFY_SHUTDOWN_ALL
*		    <nl>    WSH_NOTIFY_CLOSE
*		    <nl>    WSH_NOTIFY_CONNECT_ERROR
*
*   @xref   <f IRLMP_Notify>               <f IRLMP_OpenConnection>
*           <f IRLMP_GetWildcardSockaddr>  <f IRLMP_SetSocketInformation>
*/

INT
IRLMP_OpenSocket(IN  OUT PINT    pAddressFamily,
                 IN  OUT PINT    pSocketType,
                 IN  OUT PINT    pProtocol,
                 OUT     PWSTR   pTransportDeviceName,
                 OUT     PVOID * ppHelperDllSocketContext,
                 OUT     PDWORD  pNotificationEvents)
{
    PIRLMP_SOCKET_CONTEXT pSocket;
    UINT rc;

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_OpenSocket(0x%X,0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"),
        pAddressFamily, 
        pSocketType, 
        pProtocol, 
        pTransportDeviceName,
        ppHelperDllSocketContext, 
        pNotificationEvents));

    DUMP_WSHOBJECTS(ZONE_WSHIRDA);

    if (AF_IRDA != *pAddressFamily) {
        return WSAEINVAL;
    }

    if (*pSocketType != SOCK_STREAM)
        return(WSAEINVAL);

    switch (*pProtocol) {
    case 0:
    case AF_IRDA:
        break;

    default:
        return WSAEINVAL;
    }

	wcscpy(pTransportDeviceName, IRLMP_TRANSPORT_NAME);

    pSocket = IrdaAlloc(sizeof(IRLMP_SOCKET_CONTEXT), MT_WSH_SOCKET_CTXT);

    if (pSocket == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("IRLMP_OpenSocket() CTEAllocMem() failed\r\n")));

        return(WSAENOBUFS);
    }

    EnterCriticalSection(&g_csWshIrda);

    while (g_IrdaLinkState == IRDALINKSTATE_CLOSING)
    {
        LeaveCriticalSection(&g_csWshIrda);

        DEBUGMSG(ZONE_WARN, (TEXT("IRLMP_OpenSocket(0x%x): Wait for IrDA to release link before open...\r\n"), pSocket));
        // If we are in the midst of releasing the link, wait until done - then
        // we will need to reacquire the link.
        rc = WaitForSingleObject(g_hEvWaitToRelLink, IRLMP_OPENSOCKET_MAX_WAIT);
        if (rc != WAIT_OBJECT_0) {
            //
            // Either IRDA has been deinit'd or the process this thread belongs to has been terminated
            //
            DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_OpenSocket:WaitForSingleObject returned error %d\r\n"), rc));
            IrdaFree(pSocket);
            return (WSASYSNOTREADY);
        }
        EnterCriticalSection(&g_csWshIrda);        
    }


    *ppHelperDllSocketContext = pSocket;

    OpenSocketCnt++;
    DEBUGMSG(ZONE_SOCKETCNT, (TEXT("IrDA: Open sockets = %d -> %d.\r\n"), 
        OpenSocketCnt - 1, OpenSocketCnt));

    pSocket->UseExclusiveMode = FALSE;
    pSocket->UseIrLPTMode     = FALSE;
    pSocket->Use9WireMode     = FALSE;
    pSocket->UseSharpMode     = FALSE;
    pSocket->pIasAttribs      = NULL;

    InsertHeadList(&g_SocketList, &pSocket->Linkage);
	
    *pNotificationEvents = 
        WSH_NOTIFY_CLOSE    | 
        WSH_NOTIFY_BIND     | 
        WSH_NOTIFY_LISTEN;

    if (g_IrdaLinkState == IRDALINKSTATE_CLOSED)
    {
        IRDA_MSG IMsg;

        g_IrdaLinkState = IRDALINKSTATE_OPENING;

        DEBUGMSG(ZONE_SOCKETCNT, (TEXT("IrDA: Acquire link resources.\r\n")));
        IMsg.Prim = IRLMP_LINKCONTROL_REQ;
        IMsg.IRDA_MSG_AcquireResources = TRUE;
        rc = IrlmpDown(NULL, &IMsg);

        if (rc == SUCCESS)
        {
            g_IrdaLinkState = IRDALINKSTATE_OPENED;
        }
        else if (rc == IRMAC_ALREADY_INIT)
        {
            // This happens if a previous process got terminated in the middle of
            // calling IRLMP_OpenSocket
            DEBUGMSG(ZONE_WARN, (TEXT("IRDASTK:IRLMP_OpenSocket Link already up!\r\n")));
            g_IrdaLinkState = IRDALINKSTATE_OPENED;
            rc = SUCCESS;
        }
        else
        {
            g_IrdaLinkState  = IRDALINKSTATE_CLOSED;
            // Need to dec. socket count since new AFD will not call 
            // Notify CLOSE.
            OpenSocketCnt--;

            DEBUGMSG(ZONE_SOCKETCNT, 
               (TEXT("IrDA: Acquire resource failure - ")
                TEXT("Open sockets = %d -> %d.\r\n"), 
                OpenSocketCnt + 1, OpenSocketCnt));
        }

        if (rc == IRMAC_OPEN_PORT_FAILED)
        {
            DEBUGMSG(ZONE_WARN, (TEXT("IrTdi: Could not open serial port!\r\n")));
            rc = WSAEBADF;
        }
        else if (rc == IRMAC_MALLOC_FAILED)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("IrTdi: WSAENOBUFS!\r\n")));
            rc = WSAENOBUFS;
        }
        else if (rc != SUCCESS)
        {
            ASSERT(FALSE);
            DEBUGMSG(ZONE_ERROR, (TEXT("IrTdi: got unexpected error.\r\n")));
            rc = WSAEBADF;
        }

        if (rc != SUCCESS) {
            RemoveEntryList(&pSocket->Linkage);
            LeaveCriticalSection(&g_csWshIrda);
            IrdaFree(pSocket);
            return (INT) rc;
        }

        LeaveCriticalSection(&g_csWshIrda);
    }
    else
    {
        ASSERT(g_IrdaLinkState == IRDALINKSTATE_OPENED);
        LeaveCriticalSection(&g_csWshIrda);
    }

    return(NO_ERROR);
}

INT
TdiIasQuery(
    PIAS_QUERY pIasQuery,
    DWORD      cbIasQuery
    )
{
    INT        Status = NO_ERROR;
    INT        rc;
    IRDA_MSG   IrDAMsg;
    PIAS_QUERY pLocalIasQuery = NULL;

    EnterCriticalSection(&csIasQuery);

    if (IASQueryInProgress)
    {
        Status = WSAEINPROGRESS;
        goto done;
    }

    if (cbIasQuery > sizeof(IAS_QUERY) * 1000) {    // arbitrarily large size to make Prefast happy
        Status = WSAENOBUFS;
        goto done;
    }

    if (cbIasQuery < sizeof(IAS_QUERY)) {
        Status = WSAEFAULT;
        goto done;
    }

    pIASQuery = pIasQuery;

    // Need to get around this thing for proc permissions.
    pLocalIasQuery = IrdaAlloc(cbIasQuery, 0);

    if (pLocalIasQuery == NULL)
    {
        Status = WSAENOBUFS;
        goto done;
    }
    else 
    {
        memcpy(pLocalIasQuery, pIasQuery, cbIasQuery);
    }

    IrDAMsg.Prim                   = IRLMP_GETVALUEBYCLASS_REQ;
    IrDAMsg.IRDA_MSG_pIasQuery     = pLocalIasQuery;
    IrDAMsg.IRDA_MSG_AttribLen     = 
        cbIasQuery - offsetof(IAS_QUERY, irdaAttribute.irdaAttribUsrStr.UsrStr);

    if (IrDAMsg.IRDA_MSG_AttribLen <= 0)
    {
        Status = WSAEFAULT;
        goto done;
    }
    
    IASQueryInProgress = TRUE;

    LeaveCriticalSection(&csIasQuery);

    rc = IrlmpDown(NULL, &IrDAMsg);

    EnterCriticalSection(&csIasQuery);

    if (rc != 0)
    {
        if (rc == IRLMP_LINK_IN_USE)
        {
            Status = WSAEADDRINUSE;
            IASQueryInProgress = FALSE;
            goto done;
        }
        else if (rc == IRLMP_IN_EXCLUSIVE_MODE)
        {
            Status = WSAEISCONN;
            IASQueryInProgress = FALSE;
            goto done;
        }
        else 
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("!IRLMP_GetSocketInfo: Error from IRLMP_DOWN %d\r\n"), rc));
            Status = WSAEFAULT;
            IASQueryInProgress = FALSE;
            goto done;
        }
    }

    LeaveCriticalSection(&csIasQuery);

    WaitForSingleObject(IASQueryConfEvent, INFINITE);

    EnterCriticalSection(&csIasQuery);

    IASQueryInProgress = FALSE;

    switch(IASQueryStatus)
    {
    case IRLMP_IAS_SUCCESS:
        Status = NO_ERROR;
        break;

    case IRLMP_MAC_MEDIA_BUSY:
        /* wmz retry the query */
        DEBUGMSG (1, (TEXT("TDI:IRLMP_GetSocketinformation, Got IRLMP_MAC_MEDIA_BUSY, returning WSAENETDOWN\r\n")));
        Status = WSAENETDOWN;
        break;

    case IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS:
        /* wmz retry the query after DISCOVERY_IND */
        DEBUGMSG (1, (TEXT("GOT IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS in TDI:IRLMP_GetSocketinformation\r\n")));
        Status = WSAENETDOWN;
        break;
    
    case IRLMP_DISC_LSAP:
        Status = WSAECONNREFUSED;
        break;

    case IRLMP_NO_RESPONSE_LSAP:
        Status = WSAETIMEDOUT;
        break;

    case IRLMP_IAS_NO_SUCH_OBJECT:
        pLocalIasQuery->irdaAttribType = IAS_ATTRIB_NO_CLASS;
        Status = NO_ERROR;
        break;
    
    case IRLMP_IAS_NO_SUCH_ATTRIB:
        pLocalIasQuery->irdaAttribType = IAS_ATTRIB_NO_ATTRIB;
        Status = NO_ERROR;
        break;
    
    default:
        Status = WSAECONNABORTED;
        break;
    }

done:

    if (pLocalIasQuery != NULL)
    {
        memcpy(pIasQuery, pLocalIasQuery, cbIasQuery);
        IrdaFree(pLocalIasQuery);
    }

    LeaveCriticalSection(&csIasQuery);

    return (Status);
}

#define MAX_ENUM_WAIT_TIME 20000
#define MAX_ENUM_DEVICES    200

//
// TdiEnumDevices - initiate the device discovery process
//
// NOTE: The discovery confirmation packet handler, IrlmpDiscoveryConf, copies results into
// pDscvDevList in the context of packet reception when DscvInProgress==TRUE.
//
TdiEnumDevices(
    PDEVICELIST pDeviceList,
    DWORD       cDevices
    )
{
    INT      Status = NO_ERROR;
    INT      rc;
    IRDA_MSG IrDAMsg;
    DWORD    UserBufSize;

    EnterCriticalSection(&csDscv);

    if (DscvInProgress) {
        Status = WSAEINPROGRESS;
        goto done;
    }

    if (cDevices < 1) {
        Status = WSAEFAULT;
        goto done;
    }

    if (cDevices > MAX_ENUM_DEVICES) {
        cDevices = MAX_ENUM_DEVICES;
    }

    //
    // Allocate a kernel buffer for discovery results
    //
    UserBufSize = sizeof(DEVICELIST) + sizeof(IRDA_DEVICE_INFO) * (cDevices-1);

    pDscvDevList = IrdaAlloc(UserBufSize, MT_NOT_SPECIFIED);
    if (NULL == pDscvDevList) {
        DEBUGMSG (ZONE_ERROR, (TEXT("IRDASTK:TdiEnumDevices - IrdaAlloc failed\r\n")));
        Status = WSAENOBUFS;
        goto done;
    }

    ASSERT(pDscvDevList);

    DscvNumDevices          = cDevices;
    DscvRepeats             = 0;
    pDscvDevList->numDevice = 0;
        
    IrDAMsg.Prim                = IRLMP_DISCOVERY_REQ;
    IrDAMsg.IRDA_MSG_SenseMedia = TRUE;

    DscvInProgress = TRUE;

    LeaveCriticalSection(&csDscv);

    rc = IrlmpDown(NULL, &IrDAMsg);
    ASSERT(rc == 0);
    
    rc = WaitForSingleObject(DscvConfEvent, MAX_ENUM_WAIT_TIME);

    EnterCriticalSection(&csDscv);

    if (WAIT_TIMEOUT == rc) {
        Status = WSAETIMEDOUT;
    }

    DscvInProgress = FALSE;

    //
    // Copy discovery results to caller's buffer
    //
    if (pDscvDevList->numDevice) {
        if (!CeSafeCopyMemory(pDeviceList, pDscvDevList, UserBufSize)) {
            Status = WSAEFAULT;
        }
    } else {
        pDeviceList->numDevice = 0;
    }

    IrdaFree(pDscvDevList);
    pDscvDevList = NULL;

done:

    LeaveCriticalSection(&csDscv);
    
    return (Status);
}

/*++

 Function:       DeleteSocketAttribs

 Description:    Deletes all IAS attributes associated with this socket context.

 Arguments:

    pSocket - Socket context.

 Returns:

    None.

 Comments:

    Assumes g_csWshIrda is held.

--*/

VOID
DeleteSocketAttribs(PIRLMP_SOCKET_CONTEXT pSocket)
{
    IRDA_MSG            IMsg;
    PWSHIRDA_IAS_ATTRIB pIasAttrib;

#ifdef DEBUG
    DWORD cAttribs = 0;
#endif 

    DEBUGMSG(ZONE_WSHIRDA, (TEXT("+DeleteSocketAttributes(%x)\r\n"), pSocket));
    
    while (pSocket->pIasAttribs)
    {
    #ifdef DEBUG
        cAttribs++;
    #endif 

        pIasAttrib           = pSocket->pIasAttribs;
        pSocket->pIasAttribs = pIasAttrib->pNext;
                
        DEBUGMSG(ZONE_WSHIRDA, (TEXT("Delete attrib %x socket %x\r\n"), 
                 pIasAttrib->AttributeHandle, pSocket));
                
        IMsg.Prim                  = IRLMP_DELATTRIBUTE_REQ;
        IMsg.IRDA_MSG_AttribHandle = pIasAttrib->AttributeHandle;
        IrlmpDown(NULL, &IMsg);

        IrdaFree(pIasAttrib);
    }

    DEBUGMSG(ZONE_WSHIRDA, (TEXT("-DeleteSocketAttributes ")
        TEXT("[%d IAS attributes deleted]\r\n"), cAttribs));
}

/*++

 Function:       TdiReleaseLink

 Description:    Release the link. Called by g_evReleaseLink.

 Arguments:

 Returns:

 Comments:

    If we really did correct book-keeping, we know that this function
    is thread safe.
    1) all sockets are closed, that is why we are releasing the link
    2) we won't create any more sockets until g_hEvWaitToRelLink is signalled.

--*/

INT
TdiReleaseLink(
    struct CTEEvent *pEvent,
    VOID *pvNULL
    )
{
    IRDA_MSG IMsg;
    INT      rc;

    DEBUGMSG(ZONE_FUNCTION|ZONE_WARN, (TEXT("IRDA:+TdiReleaseLink\r\n")));
    EnterCriticalSection(&g_csWshIrda);

    if (g_IrdaLinkState == IRDALINKSTATE_CLOSING) {
        DEBUGMSG(ZONE_SOCKETCNT, (TEXT("IRDA:TdiReleaseLink releasing link resources\r\n")));
        IMsg.Prim = IRLMP_LINKCONTROL_REQ;
        IMsg.IRDA_MSG_AcquireResources = FALSE;
    
        LeaveCriticalSection(&g_csWshIrda);
        rc = IrlmpDown(NULL, &IMsg);
        EnterCriticalSection(&g_csWshIrda);
        switch (rc) {
        case SUCCESS:
        case IRMAC_NOT_INITIALIZED:
            g_IrdaLinkState  = IRDALINKSTATE_CLOSED;
            SetEvent(g_hEvWaitToRelLink);
            break;

        case IRMAC_MALLOC_FAILED:
            // Try again later.
            CTEStartTimer(&g_evReleaseLink,
                          1000,
                          TdiReleaseLink,
                          NULL);
            break;

        default:
            ASSERT(FALSE);
        }
    } else {
        ASSERT(FALSE);  // Should be at IRDALINKSTATE_CLOSING
    }
    
    LeaveCriticalSection(&g_csWshIrda);

    DEBUGMSG(ZONE_FUNCTION|ZONE_WARN, (TEXT("IRDA:-TdiReleaseLink\r\n")));
    return 0;
}


