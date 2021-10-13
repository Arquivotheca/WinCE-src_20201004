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
#include <kitl.h>
#include <kitlprot.h>
#include "kernel.h"
#include "kitlp.h"

static BOOL gfResetDevice, gfCleanBoot, gfDebugBreak;

static DWORD KITLRebootThread (DWORD dummy);

extern DWORD g_dwKITLThreadPriority;


#pragma warning(push)
#pragma warning(disable:4995)
// Ideally we want to declare a max KITL pack on stack. However, as KITL can be running on kernel 
// stack where the size is very limited (<6k), we don't want to grow the stack unless nessary.
// the use of _alloca here is to reduce stack usage.

BOOL SendConfig (KITL_CLIENT *pClient, BOOL fIsResp)
{
    UCHAR *FmtBuf = SendFmtBuf;
    KITL_HDR *pHdr;

    KITL_SVC_CONFIG_DATA *pConfigData;

    if (!InSysCall ()) {
        FmtBuf = (UCHAR *) _alloca (Kitl.FrmHdrSize   // for transport header
            + sizeof(KITL_HDR)                        // for protocol header
            + sizeof(KITL_SVC_CONFIG_DATA)            // for data
            + Kitl.FrmTlrSize);                       // for transport tailer
        if (!FmtBuf) {
            KITLOutputDebugString ("!SendConfig: Stack overflow\r\n");
            return FALSE;
        }
    }
    
    pHdr = (KITL_HDR *)(FmtBuf + Kitl.FrmHdrSize);   // pointer to the  protocol header
    pConfigData = (KITL_SVC_CONFIG_DATA *) KITLDATA(pHdr);   // pointer to the data

    // Format protocol header
    pHdr->Id = KITL_ID;
    pHdr->Service = KITL_SVC_ADMIN;
    pHdr->Flags = KITL_FL_FROM_DEV;
    if (fIsResp)
        pHdr->Flags |= KITL_FL_ADMIN_RESP;
    pHdr->SeqNum = 0;
    pHdr->Cmd = KITL_CMD_SVC_CONFIG;

    // Format service config message
    pConfigData->ProtocolVersion = CURRENT_KITL_VERSION;
    pConfigData->ServiceId = pClient->ServiceId;
    pConfigData->WindowSize = pClient->WindowSize;
    pConfigData->Flags = pClient->CfgFlags;

    if (IS_SVC_MULTIINST (pClient->CfgFlags)) {
        if(strnlen(pClient->ServiceName, _countof(pClient->ServiceName)) >=
                _countof(pConfigData->Multi.ServiceName)) {
            return FALSE;
        }
        strncpy_s(pConfigData->Multi.ServiceName, _countof(pConfigData->Multi.ServiceName), 
                    pClient->ServiceName, _TRUNCATE);
        pConfigData->Multi.ServiceInstanceId = pClient->ServiceInstanceId;
    } else {
        if(strnlen(pClient->ServiceName, _countof(pClient->ServiceName)) >=
                _countof(pConfigData->Single.ServiceName)) {
            return FALSE;
        }
        strncpy_s(pConfigData->Single.ServiceName, _countof(pConfigData->Single.ServiceName), 
                    pClient->ServiceName, _TRUNCATE);
    }

    if (KITLDebugZone & ZONE_FRAMEDUMP) {
        KITLFrameDump(">>SendConfig", pHdr,
                sizeof(KITL_HDR) + sizeof(KITL_SVC_CONFIG_DATA));
    }
    if (!KitlSendFrame (FmtBuf, sizeof(KITL_HDR) + sizeof(KITL_SVC_CONFIG_DATA))) {
        KITLOutputDebugString("!SendConfig: Error in KitlSendFrame\n");
        return FALSE;
    }
    return TRUE;
}
#pragma warning(pop)


static BOOL SendCapabilities ()
{
    BOOL fRetSuccess = FALSE;

    LPBYTE pFormatBuffer = SendFmtBuf;
    const size_t cbFrame = Kitl.FrmHdrSize
        + sizeof (KITL_HDR)
        + sizeof (KITL_CAPABILITIES_DATA)
        + Kitl.FrmTlrSize;

    if ( !InSysCall() )
    {
        pFormatBuffer = (LPBYTE) _alloca(cbFrame);
    }

    if (pFormatBuffer == NULL)
    {
        KITL_RETAILMSG (ZONE_ERROR, ("!SendCapabilities: Unable to stack allocate %u bytes for capabilities frame.\n", cbFrame));
    }
    else
    {
        const PKITL_HDR pHdr = (PKITL_HDR) (pFormatBuffer + Kitl.FrmHdrSize);
        const PKITL_CAPABILITIES_DATA pCapabilitiesData = (PKITL_CAPABILITIES_DATA) KITLDATA(pHdr);
        const WORD cbKitlPacket = sizeof(*pHdr) + sizeof(*pCapabilitiesData);

        pHdr->Id = KITL_ID;
        pHdr->Service = KITL_SVC_ADMIN;
        pHdr->Flags = KITL_FL_FROM_DEV | KITL_FL_ADMIN_RESP;
        pHdr->SeqNum = 0;
        pHdr->Cmd = KITL_CMD_GET_CAPABILITIES;

        pCapabilitiesData->kcSupported = (kcStandard | kcSupportsMultiInstanceServices);


        if (KITLDebugZone & ZONE_FRAMEDUMP)
        {
            KITLFrameDump (">>SendCapabilities", pHdr, cbKitlPacket);
        }

        fRetSuccess = KitlSendFrame (pFormatBuffer, cbKitlPacket);

        if ( !fRetSuccess )
        {
            KITL_RETAILMSG (ZONE_ERROR, ("!SendCapabilities: KitlSendFrame failed.\n"));
        }
    }

    return fRetSuccess;
}


/* ExchangeConfig
 *
 *   If we have peer address (e.g. for default services, we get this info from eshell at boot),
 *   send config message to peer, indicating the service ID and window size to use.  Otherwise,
 *   do passive connect (desktop app will get device address from eshell, then send us a config
 *   message).
 */
BOOL ExchangeConfig (KITL_CLIENT *pClient)
{
    BOOL fBlock = UseSysCall (pClient) && (KITLGlobalState & KITL_ST_INT_ENABLED); 

    if (pClient->State & KITL_WAIT_CFG) {
    
        DWORD Timeout = MIN_POLL_TIME;

        KITL_DEBUGMSG(ZONE_INIT,("Waiting for service '%s' to connect..., fBlock = %d\n", pClient->ServiceName, fBlock));

        do {

            if (!SendConfig (pClient, FALSE)) {
                break;
            }

            if (fBlock) { 
            
                if (WAIT_OBJECT_0 == DoWaitForObjects (1, &pClient->phdCfgEvt, Timeout)) {
                    break;
                }
                if (Timeout < MAX_POLL_TIME) {
                    Timeout <<= 1;
                }
                
            } else {
                // use Tx buffer for receive buffer, as there cannot be any client
                // message before the client is registered.
                while ((pClient->State & KITL_WAIT_CFG) 
                    && !HandleRecvInterrupt (pClient->pTxBufferPool)) {
                    // empty body
                }
                
            }
        } while (pClient->State & KITL_WAIT_CFG);
    }
    return 0 != (pClient->State & KITL_CLIENT_REGISTERED);
}

void ProcessSvcConfig (PKITL_HDR pHdr, WORD wMsgLen)
{
    if (wMsgLen == (sizeof(KITL_HDR) + sizeof(KITL_SVC_CONFIG_DATA)))
    {
        PKITL_CLIENT            pClient         = NULL;
        KITL_SVC_CONFIG_DATA    *pCfg           = (KITL_SVC_CONFIG_DATA *) KITLDATA (pHdr);

        const LPCSTR            szServiceName   = IS_SVC_MULTIINST (pCfg->Flags)
                                                    ? pCfg->Multi.ServiceName
                                                    : pCfg->Single.ServiceName;

        int                     idClient    = ChkDfltSvc (szServiceName);

        // Find client struct
        if (idClient < 0)
        {
            int  idStart = HASH(szServiceName[0]);

            idClient = idStart;

            while (KITLClients[idClient])
            {
                if ( (!IS_UNUSED (KITLClients[idClient]->State))
                    && (strcmp(KITLClients[idClient]->ServiceName,szServiceName) == 0))
                {
                    if (IS_SVC_MULTIINST (KITLClients[idClient]->CfgFlags) != IS_SVC_MULTIINST (pCfg->Flags))
                    {
                        KITL_DEBUGMSG(ZONE_WARNING, ("!ProcessSvcConfig: Cannot process config with differing multi-instance property.  ServiceName: '%s'\n"
                                                            "    KITLClients[idClient]->CfgFlags:   0x%B\n"
                                                            "    pCfg->Flags:                       0x%B\n",
                                                        KITLClients[idClient]->ServiceName,
                                                        KITLClients[idClient]->CfgFlags,
                                                        pCfg->Flags));
                        break;
                    }
                    else if (IS_SVC_MULTIINST (pCfg->Flags))
                    {
                        if (pCfg->Multi.ServiceInstanceId == KITLClients[idClient]->ServiceInstanceId)
                        {
                            // found multi-instance client with same name and instance ID.
                            pClient = KITLClients[idClient];
                            break;
                        }
                        else
                        {
                            // found multi-instance client with same name, but different
                            // instance ID... continue searching
                        }
                    }
                    else
                    {
                        // found single-instance client of the same name
                        pClient = KITLClients[idClient];
                        break;
                    }
                }

                if (MAX_KITL_CLIENTS == ++ idClient)
                    idClient = NUM_DFLT_KITL_SERVICES;

                if (idStart == idClient)
                    break;  // couldn't find a client
            }
        }
        else
        {
            pClient = KITLClients[idClient];
        }

        if (!pClient || !(pClient->State & (KITL_CLIENT_REGISTERING|KITL_CLIENT_REGISTERED))) {
            // Use a different debugzone here, for serail port keeps printing KDBG service if debugger not connected...
            KITL_DEBUGMSG(ZONE_CMD,("!Received config for unrecognized service %s\n",
                                        szServiceName));
        } else {
            BOOL fUseSysCalls = UseSysCall (pClient);
            
            if (fUseSysCalls) {
                EnterCriticalSection (&pClient->ClientCS);
            }
            
            // Send config to peer, unless this was a response to our cmd
            if (!(pHdr->Flags & KITL_FL_ADMIN_RESP)) {
                // ack this config message
                SendConfig(pClient, TRUE);

                // TODO: put a cookie into the service config packet so kitl
                // TODO: can recognize whether the config packet has been seen
                // TODO: or not.
                if (pClient->State & KITL_SYNCED) {
                    // Reset the kitl stream state if the stream was previously
                    // active and the desktop just sent a service configure
                    // packet.
                    ResetClientState (pClient);
                }
            }

            //
            // we got the response from desktop, connecting the client
            //
            KITL_DEBUGMSG(ZONE_INIT, ("ProcessAdminMsg: Receive Config message for service %s\n", pClient->ServiceName));
            pClient->State &= ~(KITL_WAIT_CFG|KITL_CLIENT_REGISTERING);
            pClient->State |= KITL_CLIENT_REGISTERED;
            
            // Set our event in case anyone is waiting for config info
            if (fUseSysCalls) {
                KITLSetEvent (pClient->phdCfgEvt);
                LeaveCriticalSection (&pClient->ClientCS);
            }
                    }
    }
}

BOOL ProcessAdminPacket (PKITL_HDR pHdr, WORD wMsgLen)
{
    BOOL fRetransmit = FALSE;
    
    switch (pHdr->Cmd)
    {
        case KITL_CMD_SVC_CONFIG:
            ProcessSvcConfig (pHdr, wMsgLen);
            break;

        case KITL_CMD_RESET:
            gfResetDevice = TRUE;
            gfCleanBoot   = ((KITL_RESET_DATA *) KITLDATA (pHdr))->Flags & KITL_RESET_CLEAN;
            break;

        case KITL_CMD_DEBUGBREAK:
            gfDebugBreak = TRUE;
            break;

        case KITL_CMD_TRAN_CONFIG:
            // ignore, transport config is handled in KITLConnectToDesktop
            break;

        case KITL_CMD_RETRASMIT:
            fRetransmit = TRUE;
            break;

        case KITL_CMD_GET_CAPABILITIES:
            SendCapabilities ();
            break;

        default:
            KITL_DEBUGMSG(ZONE_WARNING,("!ProcessAdminMsg: Unhandled command 0x%X\n",pHdr->Cmd));
            return FALSE;
    }
    return fRetransmit;
}

//
// KITLConnectToDesktop: Exchanging transport information with desktop
//
//      This function calls the transport function to retrieve device transport info,
//      send it to desktop, getting the transport infor from the desktop, and tell
//      the device transport about the desktop transport info.
//
//
// This is the only case where we cannot rely on desktop timer to poll data,
// as there is no KITL connection yet.
//
// No synchronization is required, as there cannot be any other KITL activities
//
BOOL KITLConnectToDesktop (void)
{
    // we'll use the SendFmtBuf for send buffer since ppfs can't be started yet at this stage
    //
    PKITL_HDR           pHdr    = (PKITL_HDR) (SendFmtBuf + Kitl.FrmHdrSize);
    PKITL_DEV_TRANSCFG  pCfg    = (PKITL_DEV_TRANSCFG) KITLDATA(pHdr);
    USHORT              cbData  = KITL_MTU - Kitl.FrmHdrSize - sizeof (KITL_HDR) - sizeof (KITL_DEV_TRANSCFG);
    

    KITLOutputDebugString ("Connecting to Desktop\r\n");
    
    if (Kitl.pfnGetDevCfg ((LPBYTE) (pCfg+1), &cbData)) {

        KITL_DEBUGMSG(ZONE_INIT, (" KITLConnectToDesktop: Including %u bytes of custom transport config data.\n", cbData));

        memset (pHdr, 0, sizeof (KITL_HDR));
        pHdr->Id = KITL_ID;
        pHdr->Service = KITL_SVC_ADMIN;
        pHdr->Flags = KITL_FL_FROM_DEV;
        pHdr->Cmd = KITL_CMD_TRAN_CONFIG;
        cbData += sizeof (KITL_DEV_TRANSCFG);
        pCfg->wLen = cbData;
        pCfg->wCpuId = KITL_CPUID;
        memcpy (pCfg->szDevName, Kitl.szName, KITL_MAX_DEV_NAMELEN);
        cbData += sizeof (KITL_HDR);

        KITL_RETAILMSG(ZONE_INIT, (" KITLConnectToDesktop: Encoding packet\n"));
        if (Kitl.pfnEncode (SendFmtBuf, cbData)) {
            DWORD dwLoopCnt     = 0;
            DWORD dwLoopMax     = MIN_POLL_ITER;
            DWORD dwStartTime   = CurMSec;
            int   nTimeMax      = MIN_POLL_TIME;  // start with 1 sec
            BOOL  fUseTick      = FALSE;
            BOOL  fRetransmit;
            WORD  wMsgLen;

            cbData += Kitl.FrmHdrSize + Kitl.FrmTlrSize;
            
            do {
                KITL_RETAILMSG(ZONE_INIT, (" KITLConnectToDesktop: Sending to CFG to desktop\n"));
                if (!KitlSendRawData (SendFmtBuf, cbData)) {
                    KITLOutputDebugString ("!ERROR: KITLConnectToDesktop failed\n");
                    break;
                }
                
                fRetransmit = FALSE;

                do {
                    wMsgLen = KITL_MTU;

                    pHdr = KitlRecvFrame(PollRecvBuf, &wMsgLen);
                    if(pHdr == NULL){
                        goto skip_process_frame;
                    }

                    // only care about admin transport config packet
                    if ((KITL_SVC_ADMIN == pHdr->Service)
                        && (KITL_CMD_TRAN_CONFIG == pHdr->Cmd)) {
                            
                        PKITL_HOST_TRANSCFG pCfg = (PKITL_HOST_TRANSCFG) KITLDATA(pHdr);
                        wMsgLen -= sizeof(KITL_HDR);
                        
                        if (pCfg->dwLen == wMsgLen) {
                            wMsgLen -= sizeof (KITL_HOST_TRANSCFG);
                            
                            if (Kitl.pfnSetHostCfg ((LPBYTE) (pCfg+1), wMsgLen)) {
                                Kitl.dwBootFlags = pCfg->dwFlags;
                                
                                if (pCfg->dwKeySig == HOST_TRANSCFG_KEYSIG) {
                                    for (dwLoopCnt = 0; dwLoopCnt < HOST_TRANSCFG_NUM_REGKEYS; dwLoopCnt ++) {
                                        // copy zone if host doesn't support flagging which zones are valid
                                        //    (old versions of host kitl)
                                        // or if the zone value is flagged valid.
                                        if (!(pCfg->dwFlags & KITL_FL_HAVEZONESET) ||
                                                (pCfg->dwFlags & KITL_FL_ZONESET(dwLoopCnt))) {
                                            g_kpriv.pdwKeys[dwLoopCnt] = pCfg->dwKeys[dwLoopCnt];
                                            KITL_DEBUGMSG (ZONE_INIT, (" KeyIndex %d = %d \n", dwLoopCnt, g_kpriv.pdwKeys[dwLoopCnt]));
                                        } else {
                                            KITL_DEBUGMSG(ZONE_INIT,(" KeyIndex %d not set on host\n",dwLoopCnt));
                                        }
                                    }
                                }    
                                KITLGlobalState |= KITL_ST_DESKTOP_CONNECTED;
                                KITLOutputDebugString ("KITL connected with Desktop\r\n");
                                break;
                            }
                        }
                    } else {
                        KITL_RETAILMSG(ZONE_INIT, (" KITLConnectToDesktop: Received non-admin, non-config packet\n"));
                    }
skip_process_frame:
                    if (fUseTick) {
                        // use tick count to determine if we need to retransmit
                        fRetransmit = ((int) (CurMSec - dwStartTime) > nTimeMax);
                        if (fRetransmit) {
                            dwStartTime = CurMSec;
                            if (nTimeMax < MAX_POLL_TIME) {
                                nTimeMax <<= 1;
                            }
                        }
                    } else {
                        // use iteration count to determine if we need to re-transmit
                        if (dwStartTime != CurMSec) {
                            // tick moving, use tick
                            fUseTick = TRUE;
                        } else {
                            fRetransmit = (dwLoopCnt ++ > dwLoopMax);
                            
                            if (fRetransmit) {
                                if (dwLoopMax < MAX_POLL_ITER) {
                                    dwLoopMax <<= 1;
                                }
                                dwLoopCnt = 0;
                            }
                        }
                    }                    
                } while (!fRetransmit);

            } while (!(KITLGlobalState & KITL_ST_DESKTOP_CONNECTED));
        }
    }

#ifndef SHIP_BUILD
    KITLDebugZone = DEFAULT_SETTINGS;
#endif
    return (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED);
}


void HandleResetOrDebugBreak (void)
{
    if (!InSysCall ()) {
        if (InterlockedExchange ((LONG *)&gfResetDevice, 0)) {
            HANDLE hTh;
            KITLOutputDebugString("KITL: Got RESET command\n");
        
            // Since NKReboot() blocks its caller it is not a good idea
            // to call it synchronously from KITL IST (interrupt mode) or 
            // a Shell thread (polling mode).
            // The code below creates a KITL reboot thread and lets it
            // call NKReboot() asynchronous to the original caller.

            // Start reboot thread
            KITL_DEBUGMSG(ZONE_INIT,("KITL Creating Reboot thread\n"));
            hTh = CreateKernelThread((LPTHREAD_START_ROUTINE)KITLRebootThread,
                            NULL, (WORD)g_dwKITLThreadPriority, 0);
            if (hTh == NULL) {
                KITLOutputDebugString("Error creating reboot thread\n");
                return;
            }
            HNDLCloseHandle (g_pprcNK, hTh);
        } else if (InterlockedExchange ((LONG *)&gfDebugBreak, FALSE)) {
            DebugBreak ();
        }
    }
}

static 
DWORD KITLRebootThread (DWORD dummy)
{
    pCurThread->bDbgCnt = 1;   // no entry messages - must be set before any debug message

    KITL_DEBUGMSG(ZONE_INIT,("KITL reboot thread started (hTh: 0x%X, pTh: 0x%X)\n",
                             dwCurThId, pCurThread));

    // Set for clean boot if requested
    g_pNKGlobal->pfnNKReboot (gfCleanBoot);

    // NKReboot function should never return
    KITLOutputDebugString("KITL: IOCTL_HAL_REBOOT not supported on this platform\n");
    return 0;
}

