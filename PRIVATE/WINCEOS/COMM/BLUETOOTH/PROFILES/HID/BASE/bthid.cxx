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
/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Abstract:
    Windows CE Bluetooth stack layer sample

**/

#include <windows.h>
#include <intsafe.h>

#include <svsutil.hxx>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>

#if defined (UNDER_CE)
#include <pkfuncs.h>
#endif

#include <bt_tdbg.h>

#include <hid.h>

#include "bthidpriv.h"
#include "hidpkt.h"
#include "hidsdp.h"
#include "bthid.h"
#include "hidconf.h"

#define DEBUG_LAYER_TRACE           0x00000040

DECLARE_DEBUG_VARS();

#define BTH_PSM_CONTROL             0x11
#define BTH_PSM_INTERRUPT           0x13
#define BTH_MTU_MIN                 48
#define BTH_MTU_MAX                 672

#define BTH_MAX_FRAGMENTS           256

#define BTH_PARSER_TIMEOUT          3600000
#define BTH_CONNECTION_TIMEOUT      120000
#define BTH_WIRE_TIMEOUT            5000

#define CALL_L2CAP_LINK_SETUP       0x11

#define NONE                        0x00
#define CONNECTED                   0x01
#define CONFIG_REQ_DONE             0x02
#define CONFIG_IND_DONE             0x04
#define UP                          0x07
#define LINK_ERROR                  0x80

#define HID_RECONNECT_INTERVAL      20000


#ifdef DEBUG

// Need this for hidmdd module
DBGPARAM dpCurSettings = {
    _T("BTHHID"),
    {
        _T("Errors"), _T("Warnings"), _T("Init"), _T("Function"),
        _T("HID Data"), _T("Comments"), _T(""), _T(""),
        _T("Parsing"), _T(""), _T(""), _T(""),
        _T(""), _T(""), _T(""), _T("BTH Client")
    },
    0x3
};

#endif


struct Link {
    Link            *pNext;
    BD_ADDR         b;
    unsigned short  psm;
    unsigned short  cid;

    unsigned int    fStage;

    unsigned int    fIncoming : 1;

    unsigned short  mtu;
};

struct SCall {
    SCall           *pNext;
    Link            *pLink;

    HANDLE          hEvent;

    SVSHandle        hCallContext;    // Unique handle for async call lookup

    int             iResult;

    unsigned short  psm;

    unsigned int    fWhat            : 8;
    unsigned int    fComplete        : 1;
    unsigned int    fAutoClean       : 1;

    unsigned char   l2cap_id;

    BD_BUFFER       *pBuffer;
};

struct HidDevice : public IBTHHIDReportHandler {
    HidDevice        *pNext;
    BD_ADDR          b;

    union {
        struct {
            // BTH
            unsigned int    fEncrypt : 1;
            unsigned int    fAuthenticate : 1;
            unsigned int    fHaveControl : 1;
            unsigned int    fHaveInterrupt : 1;

            // HID
            unsigned int    fReconnectInitiate : 1;
            unsigned int    fNormallyConnectable : 1;
            unsigned int    fVirtualCable : 1;

            // Service
            unsigned int    fIncoming : 1;
            unsigned int    fAuthSpinned : 1;
            unsigned int    fTrans : 1;
        };

        unsigned int flags;
    };

    HANDLE          hevtTransFree;
    HANDLE          hevtTrans;
    int             cTransWait;
    BTHHIDPacket    *pktTransResp;

    BTHHIDPacket    *pktControl;
    BTHHIDPacket    *pktInterrupt;

    SVSCookie       ckDeviceTimeout;
    SVSCookie       ckConnectionTimeout;
    SVSCookie       ckAttachHidThread;

    PVOID           pvNotifyParameter;
    BLOB            blobReportDescriptor;

    HidDevice (BD_ADDR *pba) {
        pNext = 0;
        b = *pba;

        flags = 0;

        hevtTrans = CreateEvent (NULL, FALSE, FALSE, NULL);
        hevtTransFree = CreateEvent (NULL, TRUE, FALSE, NULL);

        cTransWait = 0;
        pktTransResp = pktControl = pktInterrupt = NULL;
        ckDeviceTimeout = 0;
        ckConnectionTimeout = 0;
        ckAttachHidThread = 0;
        pvNotifyParameter = NULL;

        memset(&blobReportDescriptor, 0, sizeof(BLOB));
    }

    ~HidDevice (void) {
        SetEvent (hevtTransFree);
        CloseHandle (hevtTransFree);
        SetEvent (hevtTrans);
        CloseHandle (hevtTrans);

        if (pvNotifyParameter)
            HidMdd_Notifications(HID_MDD_CLOSE_DEVICE, 0, pvNotifyParameter);

        if (pktTransResp) {
            pktTransResp->ReleasePayload ();
            delete pktTransResp;
        }

        if (pktControl) {
            pktControl->ReleasePayload ();
            delete pktControl;
        }

        if (pktInterrupt) {
            pktInterrupt->ReleasePayload ();
            delete pktInterrupt;
        }

        if (blobReportDescriptor.pBlobData)
            g_funcFree (blobReportDescriptor.pBlobData, g_pvFreeData);
    }

    void *operator new (size_t iSize);
    void operator delete(void *ptr);

    int FillPersistentParameters (int fIncoming);

    // IBTHHIDReportHandler overrides
    int  SetIdle(unsigned char bIdle);
    int  GetIdle(unsigned char* pbIdle);
    int  SetProtocol(E_BTHID_PROTOCOLS protocol);
    int  GetReport(int iReportID, E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, PDWORD pcbTransfered, int iTimeout);
    int  SetReport(E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, int iTimeout, BYTE bReportID);
};

int hiddev_CloseDriverInstance (void);

static int hiddev_ConfigInd (void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **pExtendedOptions);
static int hiddev_ConnectInd (void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm);
static int hiddev_DataUpInd (void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer);
static int hiddev_DisconnectInd (void *pUserContext, unsigned short cid, int iError);
static int hiddev_lStackEvent (void *pUserContext, int iEvent, void *pEventContext);

static int hiddev_lCallAborted (void *pCallContext, int iError);
static int hiddev_ConfigReq_Out (void *pCallContext, unsigned short usResult, unsigned short usInMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **pExtendedOptions);
static int hiddev_ConfigResponse_Out (void *pCallContext, unsigned short result);
static int hiddev_ConnectReq_Out (void *pCallContext, unsigned short cid, unsigned short result, unsigned short status);
static int hiddev_ConnectResponse_Out (void *pCallContext, unsigned short result);
static int hiddev_DataDown_Out (void *pCallContext, unsigned short result);
static int hiddev_Ping_Out (void *pCallContext, BD_ADDR *pba, unsigned char *pOutBuffer, unsigned short size);
static int hiddev_Disconnect_Out (void *pCallContext, unsigned short result);

#if defined (BTHHID_QUEUE)
static DWORD WINAPI HidReportHandler (LPVOID lpNull);
#endif

static int WritePacket (void *lpHidDevice, BTHHIDPacket *pSource, int iTimeout, BTHHIDPacket **ppRes, int iChannel, BOOL fDoWait);
static int HIDConnect_Int(BD_ADDR *pba, unsigned short usPSM, unsigned short *pusCid);
static int HIDCloseCID_Int (unsigned short usCID);

class HIDDEV : public SVSSynch, public SVSRefObj {
public:
    Link            *pLinks;
    SCall           *pCalls;
    HidDevice       *pHIDs;

    unsigned int    fIsRunning : 1;
    unsigned int    fConnected : 1;

    HANDLE          hL2CAP;
    L2CAP_INTERFACE l2cap_if;

    int             cHeaders;
    int             cTrailers;

    FixedMemDescr   *pfmdLinks;
    FixedMemDescr   *pfmdCalls;
    FixedMemDescr   *pfmdHIDs;

#if defined (BTHHID_QUEUE)
    HANDLE          hthReports;
    SVSQueue        qHidReports;
    HANDLE          hevtReports;
#endif

    HANDLE          hthReconnect;
    HANDLE          hReconnectEvent;

    SVSThreadPool   *pSchedule;
        SVSHandleSystem *pHandles;

    HIDDEV (void) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: new HIDDEV\n"));
        pLinks = NULL;
        pCalls = NULL;
        pHIDs  = NULL;

        cHeaders = 0;
        cTrailers = 0;

        fIsRunning = FALSE;
        fConnected = FALSE;

        hL2CAP = NULL;

        memset (&l2cap_if, 0, sizeof(l2cap_if));

        pfmdLinks = pfmdCalls = pfmdHIDs = NULL;
        pSchedule = NULL;
                pHandles = NULL;

        if (! (hReconnectEvent = CreateEvent (NULL, FALSE, FALSE, NULL)))
            return;

#if defined (BTHHID_QUEUE)
        hthReports  = NULL;
        hevtReports = NULL;

        if (! qHidReports.IsQueueInitialized())
            return;

        if (! (hevtReports = CreateEvent (NULL, FALSE, FALSE, NULL)))
            return;
#endif

        if (! (pSchedule = new SVSThreadPool))
            return;

                if (! (pHandles = new SVSHandleSystem))
                        return;

        if (! (pfmdLinks = svsutil_AllocFixedMemDescr (sizeof(Link), 10)))
            return;

        if (! (pfmdHIDs = svsutil_AllocFixedMemDescr (sizeof(HidDevice), 10)))
            return;

        if (! (pfmdCalls = svsutil_AllocFixedMemDescr (sizeof(SCall), 10)))
            return;

        L2CAP_EVENT_INDICATION lei;
        memset (&lei, 0, sizeof(lei));

        lei.l2ca_ConfigInd = hiddev_ConfigInd;
        lei.l2ca_ConnectInd = hiddev_ConnectInd;
        lei.l2ca_DataUpInd = hiddev_DataUpInd;
        lei.l2ca_DisconnectInd = hiddev_DisconnectInd;
        lei.l2ca_StackEvent = hiddev_lStackEvent;

        L2CAP_CALLBACKS lc;
        memset (&lc, 0, sizeof(lc));

        lc.l2ca_CallAborted = hiddev_lCallAborted;
        lc.l2ca_ConfigReq_Out = hiddev_ConfigReq_Out;
        lc.l2ca_ConfigResponse_Out = hiddev_ConfigResponse_Out;
        lc.l2ca_ConnectReq_Out = hiddev_ConnectReq_Out;
        lc.l2ca_ConnectResponse_Out = hiddev_ConnectResponse_Out;
        lc.l2ca_DataDown_Out = hiddev_DataDown_Out;
        lc.l2ca_Ping_Out = hiddev_Ping_Out;
        lc.l2ca_Disconnect_Out = hiddev_Disconnect_Out;

        if (ERROR_SUCCESS != L2CAP_EstablishDeviceContext (this, L2CAP_PSM_MULTIPLE, &lei, &lc, &l2cap_if, &cHeaders, &cTrailers, &hL2CAP))
            return;

        int iRes = ERROR_SUCCESS;

        __try {
            int iRet = 0;
            unsigned short usPSMin = BTH_PSM_CONTROL;

            iRes = l2cap_if.l2ca_ioctl (hL2CAP, BTH_STACK_IOCTL_RESERVE_PORT, sizeof(usPSMin), (char *)&usPSMin, 0, NULL, &iRet);
            if (iRes == ERROR_SUCCESS) {
                usPSMin = BTH_PSM_INTERRUPT;
                iRes = l2cap_if.l2ca_ioctl (hL2CAP, BTH_STACK_IOCTL_RESERVE_PORT, sizeof(usPSMin), (char *)&usPSMin, 0, NULL, &iRet);
            }
        } __except (1) {
            iRes = ERROR_EXCEPTION_IN_SERVICE;
        }

        if (iRes == ERROR_SUCCESS)
            fIsRunning = TRUE;

        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: new HIDDEV successful\n"));
    }

    ~HIDDEV (void) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: delete HIDDEV\n"));

        fIsRunning = FALSE;
        fConnected = FALSE;

#if defined (BTHHID_QUEUE)
        if (hevtReports) {
            HANDLE h = hevtReports;
            hevtReports = NULL;

            SetEvent (h);
            CloseHandle (h);
        }

        if (hthReports) {
            if (WAIT_OBJECT_0 != WaitForSingleObject (hthReports, 15000))
                TerminateThread (hthReports, 0);

            CloseHandle (hthReports);
        }
#endif

        CloseHandle (hReconnectEvent);

        if (pSchedule)
            delete pSchedule;

                if (pHandles)
                        delete pHandles;

        if (hL2CAP)
            L2CAP_CloseDeviceContext (hL2CAP);

        if (pfmdHIDs)
            svsutil_ReleaseFixedNonEmpty (pfmdHIDs);

        if (pfmdLinks)
            svsutil_ReleaseFixedNonEmpty (pfmdLinks);

        if (pfmdCalls)
            svsutil_ReleaseFixedNonEmpty (pfmdCalls);
    }
};


static HIDDEV *gpState = NULL;
static BYTE gbUnplugHeader = 0;

//
//    Auxiliary code
//
void *HidDevice::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(HidDevice));

    void *pRes = svsutil_GetFixed (gpState->pfmdHIDs);

    return pRes;
}

void HidDevice::operator delete(void *ptr) {
    svsutil_FreeFixed (ptr, gpState->pfmdHIDs);
}

static HIDDEV *CreateNewState (void) {
    return new HIDDEV;
}

static SCall *AllocCall (int fWhat, Link *pLink) {
    SCall *pCall = (SCall *)svsutil_GetFixed (gpState->pfmdCalls);
    if (! pCall)
        return NULL;

    memset (pCall, 0, sizeof(*pCall));

    pCall->pLink = pLink;
    pCall->fWhat = fWhat;

    pCall->hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (NULL == pCall->hEvent) {
        svsutil_FreeFixed (pCall, gpState->pfmdCalls);
        return NULL;
    }

    pCall->hCallContext = gpState->pHandles->AllocHandle ((LPVOID)pCall);
    if (SVSUTIL_HANDLE_INVALID == pCall->hCallContext) {
        CloseHandle (pCall->hEvent);
        svsutil_FreeFixed (pCall, gpState->pfmdCalls);
        return NULL;
    }

    if (! gpState->pCalls)
        gpState->pCalls = pCall;
    else {
        SCall *pLast = gpState->pCalls;
        while (pLast->pNext)
            pLast = pLast->pNext;

        pLast->pNext = pCall;
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Allocated call 0x%08x what = 0x%02x\n", pCall, fWhat));

    return pCall;
}

static void DeleteCall (SCall *pCall) {
    if (pCall == gpState->pCalls)
        gpState->pCalls = pCall->pNext;
    else {
        SCall *pParent = gpState->pCalls;
        while (pParent && (pParent->pNext != pCall))
            pParent = pParent->pNext;

        if (! pParent) {
            IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: call not in list in DeleteCall!\n"));

            return;
        }

        pParent->pNext = pCall->pNext;
    }

    gpState->pHandles->CloseHandle (pCall->hCallContext);
    CloseHandle (pCall->hEvent);

    svsutil_FreeFixed (pCall, gpState->pfmdCalls);
}

static SCall *VerifyCall (SCall *pCall) {
    SCall *p = gpState->pCalls;
    while (p && (p != pCall))
        p = p->pNext;

    return p;
}

static SCall *FindCall (unsigned int fOp) {
    SCall *p = gpState->pCalls;
    while (p && (p->fWhat != fOp))
        p = p->pNext;

    return p;
}

static SCall *FindCall (Link *pLink, unsigned int fOp) {
    SCall *p = gpState->pCalls;
    while (p && ((p->pLink != pLink) || (p->fWhat != fOp)))
        p = p->pNext;

    return p;
}

static HidDevice *VerifyDevice (HidDevice *pDev) {
    HidDevice *pD = gpState->pHIDs;
    while (pD && (pD != pDev))
        pD = pD->pNext;

    return pD;
}

static HidDevice *FindDevice (BD_ADDR *pba) {
    HidDevice *pD = gpState->pHIDs;
    while (pD && (pD->b != *pba))
        pD = pD->pNext;

    return pD;
}

static void DeleteDevice (HidDevice *pDev) {
    if (pDev == gpState->pHIDs)
        gpState->pHIDs = pDev->pNext;
    else {
        HidDevice *p = gpState->pHIDs;
        while (p->pNext != pDev)
            p = p->pNext;

        p->pNext = pDev->pNext;
    }

    delete pDev;

    // Signal Reconnect thread that a device has been deleted
    SetEvent(gpState->hReconnectEvent);
}

static Link *VerifyLink (Link *pLink) {
    Link *p = gpState->pLinks;
    while (p && (p != pLink))
        p = p->pNext;

    return p;
}

static Link *FindLink (BD_ADDR *pba) {
    Link *pL = gpState->pLinks;
    while (pL && (pL->b != *pba))
        pL = pL->pNext;

    return pL;
}

static Link *FindLink (BD_ADDR *pba, unsigned short psm) {
    Link *pL = gpState->pLinks;
    while (pL && ((pL->b != *pba) || (pL->psm != psm)))
        pL = pL->pNext;

    return pL;
}

static Link *FindLink (unsigned short cid) {
    Link *p = gpState->pLinks;
    while (p && (p->cid != cid))
        p = p->pNext;

    return p;
}

static void DisconnectL2CAPLocked(HidDevice *pDev)
{
    for ( ; ; ) {
        Link *pLink = FindLink (&pDev->b);
        if (! pLink)
            return;

        HIDCloseCID_Int (pLink->cid);
    }
}

static DWORD WINAPI DisconnectL2CAP(LPVOID lpArg)
{
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Timeout on hid device\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HidDevice *pDev = VerifyDevice ((HidDevice *)lpArg);
    if (pDev) {
        DisconnectL2CAPLocked(pDev);
        DeleteDevice(pDev);
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static void DisconnectDevice (HidDevice *pDev, BOOL fSendVCD) {
    if(fSendVCD && pDev->fVirtualCable)
    {
        // Send VIRTUAL_CABLE_DISCONNECT
        BTHHIDPacket packet;
        packet.SetHeader(gbUnplugHeader);
        (void) WritePacket (pDev, &packet, 0, NULL, BTH_PSM_CONTROL, TRUE);
        gpState->pSchedule->ScheduleEvent (DisconnectL2CAP, pDev, BTH_WIRE_TIMEOUT);
    }
    else
    {
        DisconnectL2CAPLocked(pDev);
        DeleteDevice(pDev);
    }    
}

static DWORD WINAPI HidReconnectThread (LPVOID lpArg) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Started reconnect thread\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    while (1) {
        DWORD dwTimeout;

        if (! gpState->fIsRunning) {
            break;
        }

        HANDLE h = gpState->hReconnectEvent;

        if (gpState->pHIDs)
            dwTimeout = HID_RECONNECT_INTERVAL;
        else
            dwTimeout = INFINITE;

        gpState->AddRef ();
        gpState->Unlock ();

        DWORD dwWait = WaitForSingleObject(h, dwTimeout);

        gpState->Lock ();
        gpState->DelRef ();

        if (WAIT_TIMEOUT == dwWait) {
            HidDevice *pD = gpState->pHIDs;
            while (pD) {
                if ((!pD->fHaveControl || !pD->fHaveInterrupt) && !pD->fReconnectInitiate && pD->fNormallyConnectable) {
                    int iRes = ERROR_SUCCESS;

                    unsigned short cidControl = 0;
                    if (!pD->fHaveControl) {
                        gpState->AddRef ();
                        gpState->Unlock ();
                        iRes = HIDConnect_Int (&pD->b, BTH_PSM_CONTROL, &cidControl);
                        gpState->Lock ();
                        gpState->DelRef ();
                    }

                    if (iRes == ERROR_SUCCESS) {
                        unsigned short cidInterrupt = 0;
                        if (!pD->fHaveInterrupt) {
                            gpState->AddRef ();
                            gpState->Unlock ();

                            iRes = HIDConnect_Int (&pD->b, BTH_PSM_INTERRUPT, &cidInterrupt);

                            if (iRes != ERROR_SUCCESS) {
                                if (cidControl)
                                    HIDCloseCID_Int (cidControl);
                            }

                            gpState->Lock ();
                            gpState->DelRef ();
                        }
                    }
                }

                pD = pD->pNext;
            }
        }


    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static DWORD WINAPI DeviceTimeout (LPVOID lpArg) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Timeout on hid device\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HidDevice *pDev = VerifyDevice ((HidDevice *)lpArg);
    if (pDev && pDev->ckDeviceTimeout && (! FindLink (&pDev->b))) {
        DeleteDevice(pDev);
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static DWORD WINAPI ConnectionTimeout (LPVOID lpArg) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Connection Timeout on hid device\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HidDevice *pDev = VerifyDevice ((HidDevice *)lpArg);
    if (pDev && pDev->ckConnectionTimeout && (! (pDev->fHaveControl && pDev->fHaveInterrupt))) {
        pDev->ckConnectionTimeout = 0;
        DisconnectDevice (pDev, TRUE);
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static void DeleteLink (Link *pLink) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: delete link for bd_addr %04x%08x cid 0x%04x\n", pLink->b.NAP, pLink->b.SAP, pLink->cid));

    if (pLink == gpState->pLinks)
        gpState->pLinks = pLink->pNext;
    else {
        Link *pParent = gpState->pLinks;
        while (pParent && (pParent->pNext != pLink))
            pParent = pParent->pNext;

        if (! pParent) {
            IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: link to be deleted not in list\n"));
            return;
        }

        pParent->pNext = pLink->pNext;
    }

    SCall *pC = gpState->pCalls;
    while (pC) {
        if (pC->pLink == pLink) {
            if (pC->fAutoClean) {
                DeleteCall (pC);
                pC = gpState->pCalls;
                continue;
            } else if (! pC->fComplete) {
                pC->pLink = NULL;
                pC->fComplete = TRUE;
                pC->iResult = ERROR_CONNECTION_UNAVAIL;
                SetEvent (pC->hEvent);
            } else {
                pC->pLink = NULL;
                if (pC->iResult == ERROR_SUCCESS)
                    pC->iResult = ERROR_CONNECTION_UNAVAIL;

            }
        }

        pC = pC->pNext;
    }

    HidDevice *pDev = FindDevice (&pLink->b);

    if (pDev) {
        if (pLink->psm == BTH_PSM_CONTROL)
            pDev->fHaveControl = FALSE;
        else
            pDev->fHaveInterrupt = FALSE;

        if (!pDev->fHaveControl && !pDev->fHaveInterrupt) {
            if (pDev->pvNotifyParameter) {
                HidMdd_Notifications(HID_MDD_CLOSE_DEVICE, 0, pDev->pvNotifyParameter);
                pDev->pvNotifyParameter = NULL;
            }
        }

        if (! pDev->ckDeviceTimeout)
            pDev->ckDeviceTimeout = gpState->pSchedule->ScheduleEvent (DeviceTimeout, pDev, BTH_PARSER_TIMEOUT);
    }

    svsutil_FreeFixed (pLink, gpState->pfmdLinks);
}

static inline void HID_BufferFree (BD_BUFFER *pBuf) {
    g_funcFree (pBuf, g_pvFreeData);
}

static void HID_BufferFreeArray (BD_BUFFER **ppArray, int carray) {
    for (int i = 0 ; i < carray ; ++i)
        HID_BufferFree (ppArray[i]);
}

static BD_BUFFER *HID_BufferAlloc (int cSize) {
    SVSUTIL_ASSERT (cSize > 0);

    BD_BUFFER *pRes = (BD_BUFFER *)g_funcAlloc (cSize + sizeof (BD_BUFFER), g_pvAllocData);
    pRes->cSize = cSize;

    pRes->cEnd = pRes->cSize;
    pRes->cStart = 0;

    pRes->fMustCopy = TRUE;
    pRes->pFree = NULL;
    pRes->pBuffer = (unsigned char *)(pRes + 1);

    return pRes;
}

static BD_BUFFER *HID_BufferCopy (BD_BUFFER *pBuffer) {
    BD_BUFFER *pRes = HID_BufferAlloc (pBuffer->cSize);
    pRes->cSize = pBuffer->cSize;
    pRes->cStart = pBuffer->cStart;
    pRes->cEnd = pBuffer->cEnd;
    pRes->fMustCopy = FALSE;
    pRes->pFree = HID_BufferFree;
    pRes->pBuffer = (unsigned char *)(pRes + 1);

    memcpy (pRes->pBuffer, pBuffer->pBuffer, pRes->cSize);

    return pRes;
}

static DWORD WINAPI StackDown (LPVOID lpVoid) {        // Attention - must increment ref count before calling this!
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Stack Down\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    gpState->DelRef ();

    if ((! gpState->fIsRunning) || (! gpState->fConnected)) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fConnected = FALSE;

    Link *pLink = gpState->pLinks;

    while (gpState->pLinks)
        DeleteLink (gpState->pLinks);

    SCall *pC = gpState->pCalls;
    while (pC) {
        if (pC->fAutoClean) {
            DeleteCall (pC);
            pC = gpState->pCalls;
            continue;
        } else if (! pC->fComplete) {
            pC->pLink = NULL;
            pC->fComplete = TRUE;
            pC->iResult = ERROR_CONNECTION_UNAVAIL;
            SetEvent (pC->hEvent);
        } else {
            pC->pLink = NULL;
            if (pC->iResult == ERROR_SUCCESS)
                pC->iResult = ERROR_CONNECTION_UNAVAIL;
        }
        pC = pC->pNext;
    }

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static DWORD WINAPI StackUp (LPVOID lpVoid) {    // Attention - must increment ref count before calling this!
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Stack Up\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    gpState->DelRef ();

    if ((! gpState->fIsRunning) || gpState->fConnected) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fConnected = TRUE;
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static HidDevice *MakeNewDevice (BD_ADDR *pba, int fIncoming) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: MakeNewDevice %04x%08x\n", pba->NAP, pba->SAP));

    HidDevice *pDev = new HidDevice (pba);

    if (! pDev)
        return NULL;

    if (! pDev->FillPersistentParameters (fIncoming)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: MakeNewDevice %04x%08x fails to retrieve pairing information\n", pba->NAP, pba->SAP));
        delete pDev;
        return NULL;
    }

    pDev->pNext     = gpState->pHIDs;
    gpState->pHIDs  = pDev;
    pDev->fIncoming = fIncoming;

    // Signal Reconnect thread that we added a device.
    SetEvent(gpState->hReconnectEvent);

    return pDev;
}

static DWORD WINAPI AuthenticateDevice (LPVOID lpArg) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: authentication request on hid device\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)lpArg);
    HidDevice *pDev = pCall ? FindDevice (&pCall->pLink->b) : NULL;

    if (! pDev) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: authentication request on nonexistent hid device\n"));
        gpState->Unlock ();

        return ERROR_SUCCESS;
    }

    int fAuth = pDev->fAuthenticate;
    int fEncr = pDev->fEncrypt;
    int fIncoming = pDev->fIncoming;

    unsigned char id = pCall->l2cap_id;
    unsigned short cid = pCall->pLink->cid;

    BD_ADDR b = pDev->b;
    BT_ADDR bt = SET_NAP_SAP (b.NAP, b.SAP);

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: authentication request on hid device %04x%08x cid 0x%04x auth: %d, encr: %d, incoming: %d\n", b.NAP, b.SAP, fAuth, fEncr, fIncoming));

    HANDLE hL2CAP = gpState->hL2CAP;
    SVSHandle hContext = pCall->hCallContext;
    L2CA_ConfigReq_In pCallbackConfig = gpState->l2cap_if.l2ca_ConfigReq_In;
    L2CA_ConnectResponse_In pCallbackConnect = gpState->l2cap_if.l2ca_ConnectResponse_In;

    gpState->Unlock ();

    if (fIncoming) {
        __try {
            pCallbackConnect (hL2CAP, NULL, &b, id, cid, 1, 1);
        } __except (1) {
        }
    }

    int iErr = ERROR_SUCCESS;
    if (fAuth)
        iErr = BthAuthenticate (&bt);

    if (fEncr && (iErr == ERROR_SUCCESS))
        iErr = BthSetEncryption (&bt, TRUE);

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    pDev = VerifyDevice (pDev);
    if (! pDev) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: authentication request : device disappeared enroute\n"));
        gpState->Unlock ();

        return ERROR_SUCCESS;
    }

    gpState->Unlock ();

    if (fIncoming) {
        unsigned short result = 0;

        if (iErr != ERROR_SUCCESS)
            result = 3;

        __try {
            pCallbackConnect (hL2CAP, NULL, &b, id, cid, result, 0);
        } __except (1) {
        }

        if (result == 0) {
            iErr = ERROR_INTERNAL_ERROR;
            __try {
                iErr = pCallbackConfig (hL2CAP, (LPVOID)hContext, cid, BTH_MTU_MAX, 0xffff, NULL, 0, NULL);
            } __except (1) {
            }
        }

        if (iErr != ERROR_SUCCESS)
            hiddev_lCallAborted ((LPVOID)pCall->hCallContext, iErr);
    } else {
        if (iErr == ERROR_SUCCESS) {
            iErr = ERROR_INTERNAL_ERROR;
            __try {
                iErr = pCallbackConfig (hL2CAP, (LPVOID)hContext, cid, BTH_MTU_MAX, 0xffff, NULL, 0, NULL);
            } __except (1) {
            }
        } else
            HIDCloseCID_Int (cid);

        if (iErr != ERROR_SUCCESS)
            hiddev_lCallAborted ((LPVOID)pCall->hCallContext, iErr);
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: authentication request completes with status %d\n", iErr));

    return ERROR_SUCCESS;
}

static int WritePacket (void *lpHidDevice, BTHHIDPacket *pSource, int iTimeout, BTHHIDPacket **ppRes, int iChannel, BOOL fDoWait) {
    int iRes = ERROR_SUCCESS;

    for ( ; ; ) {
        if (! gpState)
            return ERROR_SERVICE_NOT_ACTIVE;

        gpState->Lock ();
        if (! gpState->fIsRunning) {
            gpState->Unlock ();

            return ERROR_SERVICE_NOT_ACTIVE;
        }

        HidDevice *pDev = VerifyDevice ((HidDevice *)lpHidDevice);
        if (! pDev) {
            gpState->Unlock ();

            return ERROR_SERVICE_NOT_ACTIVE;
        }

        HANDLE hevtComplete = pDev->hevtTransFree;

        if (pDev->fTrans) {    // We don't have the transaction
            gpState->Unlock ();

            if (iTimeout == 0)
                return ERROR_TIMEOUT;

            DWORD dwTicks = GetTickCount ();
            WaitForSingleObject (hevtComplete, iTimeout);
            if (iTimeout != INFINITE) {
                iTimeout -= GetTickCount () - dwTicks;
                if (iTimeout < 0)
                    iTimeout = 0;
            }

            continue;
        }

        Link *pLink = FindLink (&pDev->b, iChannel);
        if (! pLink) {
            gpState->Unlock ();

            return ERROR_NOT_FOUND;
        }

        BD_BUFFER *frags[BTH_MAX_FRAGMENTS];
        int cfrags = 0;

        pSource->SetMTU(pLink->mtu);

        for (cfrags = 0 ; cfrags < BTH_MAX_FRAGMENTS ; ++cfrags) {
            int cChunk = 0;
            pSource->GetPayloadChunk (NULL, 0, &cChunk);

            unsigned int cSize;
            if (! SUCCEEDED(CeUIntAdd3(cChunk, gpState->cHeaders, gpState->cTrailers, &cSize))) {
                HID_BufferFreeArray (frags, cfrags);
                gpState->Unlock ();
                
                return ERROR_ARITHMETIC_OVERFLOW;
            }

            frags[cfrags] = HID_BufferAlloc (cSize);
            if (! frags[cfrags]) {
                HID_BufferFreeArray (frags, cfrags);
                gpState->Unlock ();

                return ERROR_OUTOFMEMORY;
            }

            frags[cfrags]->cStart = gpState->cHeaders;
            frags[cfrags]->cEnd = frags[cfrags]->cSize - gpState->cTrailers;

            if (! pSource->GetPayloadChunk(frags[cfrags]->pBuffer + frags[cfrags]->cStart, cChunk, &cChunk))
                break;
        }

        if (cfrags == BTH_MAX_FRAGMENTS) {
            HID_BufferFreeArray (frags, cfrags);
            gpState->Unlock ();

            return ERROR_OUTOFMEMORY;
        }

        ++cfrags;

        HANDLE hL2CAP = gpState->hL2CAP;
        L2CA_DataDown_In pCallback = gpState->l2cap_if.l2ca_DataDown_In;

        unsigned short usCID = pLink->cid;
        HANDLE hevt = pDev->hevtTrans;

        pDev->fTrans = TRUE;    // We own the transaction
        ResetEvent (hevtComplete);

        gpState->Unlock ();

        iRes = ERROR_SUCCESS;
        int iSent = 0;
        __try {
            for (iSent = 0 ; iSent < cfrags ; ++iSent) {
                if (ERROR_SUCCESS != (iRes = pCallback (hL2CAP, NULL, usCID, frags[iSent])))
                    break;
            }
        } __except (1) {
            iRes = ERROR_EXCEPTION_IN_SERVICE;
        }

        if (fDoWait && (iRes == ERROR_SUCCESS))
            WaitForSingleObject (hevt, iTimeout);

        if (! gpState) {
            SetEvent (hevtComplete);
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        gpState->Lock ();

        // Buffers were copied in HCI
        HID_BufferFreeArray (frags, cfrags);

        SetEvent (hevtComplete);

        if (! gpState->fIsRunning) {
            gpState->Unlock ();

            return ERROR_SERVICE_NOT_ACTIVE;
        }

        if (! VerifyDevice (pDev)) {
            gpState->Unlock ();

            return ERROR_SERVICE_NOT_ACTIVE;
        }

        pDev->fTrans = FALSE;    // Clear the transaction

        if (fDoWait) {
            BTHHIDPacket *pPacket = pDev->pktTransResp;
            pDev->pktTransResp = NULL;

            if (! pPacket) {
                gpState->Unlock ();

                return iRes == ERROR_SUCCESS ? ERROR_TIMEOUT : iRes;
            }

            if (ppRes)
                *ppRes = pPacket;
            else {
                pPacket->ReleasePayload ();
                delete pPacket;
            }
        }
        
        gpState->Unlock ();

        break;
    }

    return iRes;
}

DWORD WINAPI AttachHidDeviceThread(LPVOID lpArg)
{
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: ++AttachHidDeviceThread\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HidDevice *pDev = VerifyDevice ((HidDevice *)lpArg);
    if (pDev && pDev->ckAttachHidThread) {
        pDev->ckAttachHidThread = 0;

        // Attach HID device.  This call will cause HID MDD to call
        // back into HID driver to send various HID commands.
        if (NULL == pDev->pvNotifyParameter) {
            PVOID pvNotifyParam = NULL;
            BLOB blobReportDescriptor;
            DWORD cbMaxInputReport = 0;

            blobReportDescriptor.cbSize = pDev->blobReportDescriptor.cbSize;
            blobReportDescriptor.pBlobData = (unsigned char *)g_funcAlloc (blobReportDescriptor.cbSize, g_pvAllocData);
            if (! blobReportDescriptor.pBlobData) {
                IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: Out of memory\n"));
                gpState->Unlock();
                return ERROR_OUTOFMEMORY;
            }

            memcpy(blobReportDescriptor.pBlobData, pDev->blobReportDescriptor.pBlobData, blobReportDescriptor.cbSize);        
            
            gpState->AddRef ();
            gpState->Unlock ();

            BOOL fRet = HidMdd_Attach(
                    (HID_PDD_HANDLE) pDev,
                    blobReportDescriptor.pBlobData,
                    blobReportDescriptor.cbSize,
                    0, 0, 0, 0,
                    &pvNotifyParam,
                    &cbMaxInputReport);

            gpState->Lock ();
            gpState->DelRef ();

            g_funcFree (blobReportDescriptor.pBlobData, g_pvFreeData);

            if (! fRet) {
                IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: Failed to initialize HID Mdd\n"));
                gpState->Unlock();
                return ERROR_NOT_CONNECTED;
            }

            pDev->pvNotifyParameter = pvNotifyParam;            
        }
    }

    gpState->Unlock ();

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: --AttachHidDeviceThread\n"));    
    
    return ERROR_SUCCESS;
}

int HidConnectionUp(Link *pLink, SCall *pCall)
{
    int iRes = ERROR_SUCCESS;
    
    SVSUTIL_ASSERT(gpState->IsLocked());
    
    pCall->fComplete = TRUE;
    pCall->iResult = ERROR_SUCCESS;
    SetEvent (pCall->hEvent);

    if (pCall->fAutoClean)
        DeleteCall (pCall);

    HidDevice *pDev = FindDevice (&pLink->b);

    if (pDev) {
        if (pLink->psm == BTH_PSM_CONTROL)
            pDev->fHaveControl = TRUE;
        else
            pDev->fHaveInterrupt = TRUE;

        if (pDev->fHaveControl && pDev->fHaveInterrupt) {
            IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: device %04x%08x connected!\n", pDev->b.NAP, pDev->b.SAP));

            if (pDev->ckConnectionTimeout) {
                gpState->pSchedule->UnScheduleEvent (pDev->ckConnectionTimeout);
                pDev->ckConnectionTimeout = 0;
            }

            pDev->ckAttachHidThread = gpState->pSchedule->ScheduleEvent (AttachHidDeviceThread, pDev, 0);
        } else if (! pDev->ckConnectionTimeout)
            pDev->ckConnectionTimeout = gpState->pSchedule->ScheduleEvent (ConnectionTimeout, pDev, BTH_CONNECTION_TIMEOUT);
    } else {
        iRes = ERROR_CONNECTION_ABORTED;
    }

    return iRes;
}

int HidDevice::FillPersistentParameters (int fIncoming) {
    DWORD dwDeviceFlags = 0;
    unsigned char *psdp = NULL;
    unsigned int csdp = 0;

    if (! GetDeviceConfig (&b, &dwDeviceFlags, &psdp, &csdp)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: FillPersistentParameters %04x%08x - bonding does not exist\n", b.NAP, b.SAP));
        return FALSE;
    }

    if (fIncoming && ((dwDeviceFlags & HIDCONF_FLAGS_ACTIVE) == 0)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: FillPersistentParameters %04x%08x - device is not activated\n", b.NAP, b.SAP));
        g_funcFree (psdp, g_pvFreeData);
        return FALSE;
    }

    if (dwDeviceFlags & HIDCONF_FLAGS_AUTH)
        fAuthenticate = TRUE;

    if (dwDeviceFlags & HIDCONF_FLAGS_ENCRYPT)
        fEncrypt = TRUE;

    BTHHIDSdpParser sdpParser;

    int iErr = sdpParser.Start (psdp, csdp);

    BOOL bb = FALSE;

    if (iErr == ERROR_SUCCESS)
        iErr = sdpParser.GetHIDReconnectInitiate(&bb);

    if (iErr == ERROR_SUCCESS) {
        fReconnectInitiate = bb;
        iErr = sdpParser.GetHIDNormallyConnectable(&bb);
        if (iErr == ERROR_SUCCESS)
            fNormallyConnectable = bb;
        iErr = ERROR_SUCCESS;
    }

    if (iErr == ERROR_SUCCESS) {
        iErr = sdpParser.GetHIDVirtualCable(&bb);
    }

    if (iErr == ERROR_SUCCESS) {
        fVirtualCable = bb;
        iErr = sdpParser.GetHIDReportDescriptor(&blobReportDescriptor);
    }

    if (iErr == ERROR_SUCCESS) {
        // Now we have the size. Allocate and requery.
        SVSUTIL_ASSERT(blobReportDescriptor.cbSize > 0);
        blobReportDescriptor.pBlobData = (unsigned char *)g_funcAlloc (blobReportDescriptor.cbSize, g_pvAllocData);
        iErr = sdpParser.GetHIDReportDescriptor(&blobReportDescriptor);
    }

    sdpParser.End ();

    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_WARN, L"BTH HID: FillPersistentParameters %04x%08x - parsing SDP record failed\n", b.NAP, b.SAP));
    }

    g_funcFree (psdp, g_pvFreeData);

    return iErr == ERROR_SUCCESS;
}

static int ProcessHidPacket (BTHHIDPacket *pPacket, HidDevice *pDev) {
    if (pPacket->GetReportType () == BTHID_REPORT_INPUT) {
        BYTE* pPayload;
        int   cbPayload;

        if (pDev->pvNotifyParameter) {
            pPacket->GetPayload(&pPayload, &cbPayload);
            if (pPayload && (cbPayload > 0) &&
                (ERROR_SUCCESS == HidMdd_ProcessInterruptReport(pPayload, cbPayload, pDev->pvNotifyParameter))) {
                // success
            } else {
                // There was a problem with the report
                IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID ProcessHidPacket : Error processing interrupt report - ignoring error.\n"));
            }
        }
    } else {
        if (pPacket->GetHeader() == gbUnplugHeader) {
            IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID ReportHandler : virtual disconnect\n"));
            DisconnectDevice (pDev, FALSE);
        } else {
            if (! pDev->fTrans) {
                IFDBG(DebugOut (DEBUG_WARN, L"BTH Device: protocol error - control packet w/o transaction. Ignoring...\n"));                
            } else {
                if (pDev->pktTransResp) {                        // Transaction violated
                    IFDBG(DebugOut (DEBUG_ERROR, L"BTH Device: protocol error - multiple control packets for transaction. Aborting...\n"));
                    DisconnectDevice (pDev, TRUE);
                } else {
                    pDev->pktTransResp = pPacket;
                    SetEvent (pDev->hevtTrans);
                    return FALSE;    // Don't destroy the packet
                }
            }
        }
    }

    return TRUE;
}

//
//    L2CAP stuff
//
static int hiddev_DataUpInd (void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Data up on channel 0x%04x %d bytes\n", cid, BufferTotal (pBuffer)));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Link *pLink = FindLink (cid);
    HidDevice *pDev = pLink ? FindDevice (&pLink->b) : NULL;
    BTHHIDPacket *pPacket = pDev ? (pLink->psm == BTH_PSM_CONTROL ? pDev->pktControl : pDev->pktInterrupt) : NULL;

    if (pDev && (! pPacket)) {
        pPacket = new BTHHIDPacket;

        if (pPacket) {
            pPacket->SetMTU (pLink->mtu);
            pPacket->SetOwner (pDev);

            // Store this packet away.  In some cases, we will need to buffer up
            // several fragments before passing data up to HID layer.

            if (pLink->psm == BTH_PSM_CONTROL) {
                pPacket->SetReportType(BTHID_REPORT_FEATURE);
                pDev->pktControl = pPacket;
            } else {
                pPacket->SetReportType(BTHID_REPORT_INPUT);
                pDev->pktInterrupt = pPacket;
            }
        }
    }

#if defined (DEBUG) || defined (_DEBUG)
    if (pPacket) {
        SVSUTIL_ASSERT (pPacket->GetOwner() == pDev);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: Data up - no packet\n"));
    }
#endif

    if (pPacket) {
        // If this function returns 0 we can indicate the packet up to HID layer.  Otherwise, we have
        // to recv more data before indicating up.
        if (! pPacket->AddPayloadChunk(pBuffer->pBuffer + pBuffer->cStart, BufferTotal (pBuffer))) {
            if (pLink->psm == BTH_PSM_CONTROL)
                pDev->pktControl = NULL;
            else
                pDev->pktInterrupt = NULL;

#if defined (BTHHID_QUEUE)
            if (! gpState->qHidReports.Put (pPacket)) {
                pPacket->ReleasePayload();
                delete pPacket;
            }

            SetEvent (gpState->hevtReports);
#else
            int fRelease = ProcessHidPacket (pPacket, pDev);
            if (fRelease) {
                pPacket->ReleasePayload();
                delete pPacket;
            }
#endif
        }
    }


    if (pBuffer->pFree)
        pBuffer->pFree (pBuffer);

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static int hiddev_DisconnectInd (void *pUserContext, unsigned short cid, int iError) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Disconnect indicator on channel 0x%04x\n", cid));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Link *pLink = FindLink (cid);
    if (! pLink) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SCall *pC = gpState->pCalls;
    while (pC) {
        if (pC->pLink == pLink) {
            if (pC->fAutoClean) {
                DeleteCall (pC);
                pC = gpState->pCalls;
                continue;
            } else if (! pC->fComplete) {
                pC->fComplete = TRUE;
                pC->iResult = ERROR_CONNECTION_UNAVAIL;
                SetEvent (pC->hEvent);
            }
        }

        pC = pC->pNext;
    }
    
    DeleteLink (pLink);

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static int hiddev_lStackEvent (void *pUserContext, int iEvent, void *pEventContext) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Stack event (L2CAP) %d\n", iEvent));

    if (! gpState)
        return ERROR_SUCCESS;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SUCCESS;
    }

    LPTHREAD_START_ROUTINE p = NULL;

    if (iEvent == BTH_STACK_DISCONNECT)
        hiddev_CloseDriverInstance ();
    else if (iEvent == BTH_STACK_DOWN)
        p = StackDown;
    else if (iEvent == BTH_STACK_UP)
        p = StackUp;

    HANDLE h = p ? CreateThread (NULL, 0, p, NULL, 0, NULL) : NULL;

    if (h) {
        CloseHandle (h);
        gpState->AddRef ();
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int hiddev_lCallAborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Aborted call 0x%08x error %d\n", pCallContext, iError));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = (SCall *) gpState->pHandles->TranslateHandle ((SVSHandle)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: Aborted call not found!\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));
    SVSUTIL_ASSERT (! pCall->fComplete);
    SVSUTIL_ASSERT (! pCall->pBuffer);

    Link *pLink = pCall->pLink;

    if (pCall->fAutoClean)
        DeleteCall (pCall);
    else {
        pCall->iResult = iError;
        pCall->fComplete = TRUE;
        pCall->pLink = NULL;
        SetEvent (pCall->hEvent);
    }

    unsigned short disconnect_cid = 0;

    if (pLink) {
        if ((pLink->fStage & UP) != UP) {
            if (pLink->fStage & CONNECTED)
                disconnect_cid = pLink->cid;
            else
                DeleteLink (pLink);
        } else {
            disconnect_cid = pLink->cid;
            pLink->fStage |= LINK_ERROR;
        }
    }

    gpState->Unlock ();

    if (disconnect_cid)
        HIDCloseCID_Int (disconnect_cid);

    return ERROR_SUCCESS;
}

static int hiddev_ConfigInd (void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **pExtendedOptions) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Config request indicator on ch 0x%04x id %d MTU %d flush 0x%04x, flow: %s\n", cid, id, usOutMTU, usInFlushTO, pInFlow ? L"yes" : L"no"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Link *pLink = FindLink (cid);
    SCall *pCall = pLink ? FindCall (pLink, CALL_L2CAP_LINK_SETUP) : NULL;

    if ((! pCall) || pCall->fComplete)  {
        IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: config setup call not found!\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->l2cap_id = id;

    SVSUTIL_ASSERT (pCall->fWhat == CALL_L2CAP_LINK_SETUP);
    SVSUTIL_ASSERT (! pCall->fComplete);
    SVSUTIL_ASSERT (VerifyLink (pCall->pLink));
    SVSUTIL_ASSERT (! pCall->pBuffer);

    int fAccept = FALSE;

    pCall->iResult = ERROR_SUCCESS;
    pCall->pLink->fStage |= CONFIG_IND_DONE;
    pCall->pLink->mtu = usOutMTU ? usOutMTU : L2CAP_MTU;
    fAccept = TRUE;

    if (pLink->fStage == UP) {
        int iRes = HidConnectionUp(pLink, pCall);
        if (ERROR_SUCCESS != iRes) {
            fAccept = FALSE;
        }
    }

    HANDLE hL2CAP = gpState->hL2CAP;
    L2CA_ConfigResponse_In pCallback = gpState->l2cap_if.l2ca_ConfigResponse_In;
    gpState->Unlock ();

    __try {
        pCallback (hL2CAP, NULL, id, cid, fAccept ? 0 : 2, usOutMTU, 0xffff, NULL, 0, NULL);
    } __except (1) {
    }

    if (! fAccept)
        hiddev_lCallAborted ((LPVOID)pCall->hCallContext, ERROR_CONNECTION_ABORTED);

    return ERROR_SUCCESS;
}

static int hiddev_ConfigReq_Out (void *pCallContext, unsigned short usResult, unsigned short usOutMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **pExtendedOptions) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Config req out for call 0x%08x result 0x%04x mtu %d flush 0x%04x, flow %s\n", pCallContext, usResult, usOutMTU, usOutFlushTO, pOutFlow ? L"yes" : L"no" ));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = (SCall *) gpState->pHandles->TranslateHandle ((SVSHandle)pCallContext);
    if ((! pCall) || pCall->fComplete)  {
        IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: config setup call not found!\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));
    SVSUTIL_ASSERT (pCall->fWhat == CALL_L2CAP_LINK_SETUP);
    SVSUTIL_ASSERT (! pCall->fComplete);
    SVSUTIL_ASSERT (VerifyLink (pCall->pLink));
    SVSUTIL_ASSERT (! pCall->pBuffer);

    if (usResult == 0) {
        Link *pLink = pCall->pLink;
        SVSUTIL_ASSERT (! (pLink->fStage & CONFIG_REQ_DONE));
        SVSUTIL_ASSERT (pLink->fStage & CONNECTED);
        SVSUTIL_ASSERT (pLink->cid);
        SVSUTIL_ASSERT (pLink->psm);

        pLink->fStage |= CONFIG_REQ_DONE;
        if (usOutMTU)
            pLink->mtu = usOutMTU;

        if (pLink->fStage == UP) {
            int iRes = HidConnectionUp(pLink, pCall);
            if (ERROR_SUCCESS != iRes) {
                IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: could not find HID device - connection not accepted!\n"));
                gpState->Unlock ();
                hiddev_lCallAborted (pCallContext, ERROR_CONNECTION_ABORTED);
                return ERROR_SUCCESS;
            }
        }

        gpState->Unlock ();
        return ERROR_SUCCESS;
    }

    gpState->Unlock ();

    hiddev_lCallAborted (pCallContext, ERROR_CONNECTION_ABORTED);

    return ERROR_SUCCESS;
}

static int hiddev_ConnectInd (void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Connect indicator from %04x%08x ch 0x%04x id %d psm 0x%04x\n", pba->NAP, pba->SAP, cid, id, psm));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short    result = 0;
    unsigned short    status = 0;

    HidDevice *pDev = FindDevice (pba);

    if (! pDev) {
        pDev = MakeNewDevice (pba, TRUE);
    }

    if (! pDev) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BTH HID: connect indication: could not make device\n"));
        result = 4;
    } else if (pDev->ckDeviceTimeout) {
        gpState->pSchedule->UnScheduleEvent (pDev->ckDeviceTimeout);
        pDev->ckDeviceTimeout = 0;
    }

    SCall *pCall = NULL;

    if (pDev) {
        Link *pLink = (Link *)svsutil_GetFixed (gpState->pfmdLinks);
        pCall = pLink ? AllocCall (CALL_L2CAP_LINK_SETUP, pLink) : NULL;
        if (pCall) {
            pCall->fAutoClean = TRUE;
            pCall->l2cap_id = id;

            pLink->b = *pba;
            pLink->cid = cid;
            pLink->fStage = CONNECTED;
            pLink->mtu = 0;
            pLink->psm = psm;
            pLink->fIncoming = TRUE;

            pLink->pNext = gpState->pLinks;
            gpState->pLinks = pLink;

            result = 0;
        } else {
            if (pLink)
                svsutil_FreeFixed (pLink, gpState->pfmdLinks);
            result = 4;
        }
    } else {
        if (result == 0)
            result = 2;
    }

    if ((result == 0) && (pDev->fEncrypt || pDev->fAuthenticate) && (! pDev->fAuthSpinned)) {
        pDev->fAuthSpinned = TRUE;
        gpState->pSchedule->ScheduleEvent (AuthenticateDevice, pCall, 0);

        gpState->Unlock ();

        return ERROR_SUCCESS;
    }



    HANDLE hL2CAP = gpState->hL2CAP;
    L2CA_ConnectResponse_In pCallbackConnect = gpState->l2cap_if.l2ca_ConnectResponse_In;
    L2CA_ConfigReq_In pCallbackConfig = gpState->l2cap_if.l2ca_ConfigReq_In;

    SVSHandle hContext = NULL;
    if (pCall)
        hContext = pCall->hCallContext;

    gpState->Unlock ();

    __try {
        pCallbackConnect (hL2CAP, NULL, pba, id, cid, result, status);
    } __except (1) {
    }

    if (result == 0) {
        int iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pCallbackConfig (hL2CAP, (LPVOID)hContext, cid, BTH_MTU_MAX, 0xffff, NULL, 0, NULL);
        } __except (1) {
        }

        if (iRes != ERROR_SUCCESS)
            hiddev_lCallAborted ((LPVOID)hContext, iRes);
    }

    return ERROR_SUCCESS;
}

static int hiddev_ConnectReq_Out (void *pCallContext, unsigned short cid, unsigned short result, unsigned short status) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Connect out for call 0x%08x ch 0x%04x result = 0x%04x status 0x%04x\n", pCallContext, cid, result, status));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = (SCall *) gpState->pHandles->TranslateHandle ((SVSHandle)pCallContext);
    if ((! pCall) || pCall->fComplete) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));
    SVSUTIL_ASSERT (pCall->fWhat == CALL_L2CAP_LINK_SETUP);
    SVSUTIL_ASSERT (! pCall->fComplete);
    SVSUTIL_ASSERT (VerifyLink (pCall->pLink));
    SVSUTIL_ASSERT (! pCall->pBuffer);

    if (result) {
        if (result != 1) {
            pCall->fComplete = TRUE;
            pCall->iResult = ERROR_CONNECTION_REFUSED;
            SetEvent (pCall->hEvent);
        }
        gpState->Unlock ();
        return ERROR_SUCCESS;
    }

    HidDevice *pDev = FindDevice (&pCall->pLink->b);

    if (! pDev) {
        pCall->fComplete = TRUE;
        pCall->iResult = ERROR_CONNECTION_REFUSED;
        SetEvent (pCall->hEvent);
        gpState->Unlock ();

        HIDCloseCID_Int (cid);
        return ERROR_SUCCESS;
    }

    Link *pLink = pCall->pLink;

    SVSUTIL_ASSERT (pLink->fStage == NONE);
    SVSUTIL_ASSERT (! pLink->cid);
    SVSUTIL_ASSERT (pLink->psm);

    pLink->fStage = CONNECTED;
    pLink->cid = cid;

    if ((pDev->fEncrypt || pDev->fAuthenticate) && (! pDev->fAuthSpinned)) {
        pDev->fAuthSpinned = TRUE;
        gpState->pSchedule->ScheduleEvent (AuthenticateDevice, pCall, 0);

        gpState->Unlock ();

        return ERROR_SUCCESS;
    }

    HANDLE hL2CAP = gpState->hL2CAP;
    L2CA_ConfigReq_In pCallback = gpState->l2cap_if.l2ca_ConfigReq_In;
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (hL2CAP, pCallContext, cid, BTH_MTU_MAX, 0xffff, NULL, 0, NULL);
    } __except (1) {
    }

    if (iRes != ERROR_SUCCESS)
        hiddev_lCallAborted (pCallContext, iRes);

    return ERROR_SUCCESS;
}

static int hiddev_DataDown_Out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: Data down call 0x%08x result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}

static int hiddev_Ping_Out (void *pCallContext, BD_ADDR *pba, unsigned char *pOutBuffer, unsigned short size) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: hiddev_Ping_Out call 0x%08x result %d\n", pCallContext));
    return ERROR_SUCCESS;
}

// These are just stubs - they do nothing
static int hiddev_ConfigResponse_Out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: ConfigResponse out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}

static int hiddev_ConnectResponse_Out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: ConnectResponse out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}

static int hiddev_Disconnect_Out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: disconnect out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}

//
//    Init stuff
//
static int hiddev_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+hiddev_CreateDriverInstance\n"));

    if (gpState) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CreateDriverInstance : ERROR_ALREADY_INITIALIZED\n"));
        return ERROR_ALREADY_INITIALIZED;
    }

    if (! BthPktInitAllocator ())
        return ERROR_OUTOFMEMORY;

    BTHID_Header_Parameter  packetVirtualUnplug;
    packetVirtualUnplug.bRawHeader = 0;

    packetVirtualUnplug.control_p.bControlOp = BTHID_CONTROL_VIRTUAL_CABLE_UNPLUG;
    gbUnplugHeader = (BTHID_HID_CONTROL << 4) | packetVirtualUnplug.bRawHeader;

    gpState = CreateNewState ();

    if ((! gpState) || (! gpState->fIsRunning)) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CreateDriverInstance : ERROR_OUTOFMEMORY\n"));
        if (gpState)
            delete gpState;
        gpState = NULL;
        return ERROR_OUTOFMEMORY;
    }

#if defined (BTHHID_QUEUE)
    gpState->hthReports = CreateThread (NULL, 0, HidReportHandler, NULL, 0, NULL);
    if (gpState->hthReports)
        SetThreadPriority (gpState->hthReports, THREAD_PRIORITY_HIGHEST);
#endif

    gpState->hthReconnect = CreateThread (NULL, 0, HidReconnectThread, NULL, 0, NULL);
    if (! gpState->hthReconnect) {
        IFDBG(DebugOut (DEBUG_ERROR, L"-hiddev_CreateDriverInstance : Failed to create reconnect thread.\n"));
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CreateDriverInstance : ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}


static int hiddev_CloseDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+hiddev_CloseDriverInstance\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CloseDriverInstance : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CloseDriverInstance : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fIsRunning = FALSE;

    SetEvent(gpState->hReconnectEvent);

    while (gpState->GetRefCount () > 1) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Waiting for ref count in hiddev_CloseDriverInstance\n"));
        gpState->Unlock ();
        Sleep (200);
        gpState->Lock ();
    }

    while (gpState->pCalls) {
        SetEvent (gpState->pCalls->hEvent);
        gpState->pCalls->iResult = ERROR_CANCELLED;
        gpState->pCalls = gpState->pCalls->pNext;
    }

    while (gpState->pLinks) {
        unsigned short cid = gpState->pLinks->cid;

        gpState->Unlock ();
        gpState->l2cap_if.l2ca_Disconnect_In (gpState->hL2CAP, NULL, cid);
        gpState->Lock ();

        gpState->pLinks = gpState->pLinks->pNext;
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"-hiddev_CloseDriverInstance : ERROR_SUCCESS\n"));

    if (gpState->hthReconnect)
        CloseHandle (gpState->hthReconnect);

    gpState->Unlock ();
    delete gpState;
    gpState = NULL;

    BthPktFreeAllocator ();

    return ERROR_SUCCESS;
}

#if defined (BTHHID_QUEUE)
//
//    HID reports processing
//
static DWORD WINAPI HidReportHandler (LPVOID lpNull) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID ReportHandler : started\n"));

    for ( ; ; ) {
        if (! gpState)
            return 0;

        gpState->Lock ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            break;
        }

        HANDLE hEvent = gpState->hevtReports;

        gpState->Unlock ();

        WaitForSingleObject (hEvent, INFINITE);

        gpState->Lock ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            break;
        }

        while (! gpState->qHidReports.IsEmpty()) {
            BTHHIDPacket *pPacket = (BTHHIDPacket *)gpState->qHidReports.Get ();
            HidDevice *pDev = VerifyDevice ((HidDevice *)pPacket->GetOwner());
            int fRelease = TRUE;
            if (pDev)
                fRelease = ProcessHidPacket (pPacket, pDev);
            else {
                IFDBG(DebugOut (DEBUG_ERROR, L"BTH Device: Device not found for a packet...\n"));
            }

            if (fRelease) {
                pPacket->ReleasePayload();
                delete pPacket;
            }
        }
        gpState->Unlock ();
    }
    return 0;
}
#endif

//
//    Internal API section
//

static int HIDConnect_Int
(
BD_ADDR            *pba,
unsigned short  usPSM,
unsigned short  *pusCid
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Link *pLink = (Link *)svsutil_GetFixed (gpState->pfmdLinks);
    memset (pLink, 0, sizeof (*pLink));
    pLink->cid = 0;
    pLink->b = *pba;
    pLink->fStage = NONE;
    pLink->fIncoming = FALSE;
    pLink->psm = usPSM;

    pLink->pNext = gpState->pLinks;
    gpState->pLinks = pLink;

    SCall *pCall = AllocCall (CALL_L2CAP_LINK_SETUP, pLink);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hL2CAP = gpState->hL2CAP;
    SVSHandle hContext = pCall->hCallContext;
    L2CA_ConnectReq_In pCallbackConnect = gpState->l2cap_if.l2ca_ConnectReq_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallbackConnect (hL2CAP, (LPVOID)hContext, usPSM, pba);
    } __except (1) {
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, INFINITE);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (gpState->fIsRunning) {
        pCall = (SCall *) gpState->pHandles->TranslateHandle ((SVSHandle)hContext);
        pLink = VerifyLink (pLink);
    } else {
        pCall = NULL;
        pLink = NULL;
        iRes = ERROR_SERVICE_NOT_ACTIVE;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall && pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (pCall) {
        SVSUTIL_ASSERT (VerifyCall (pCall));
        DeleteCall (pCall);
    }
    else
        iRes = ERROR_INTERNAL_ERROR;

    if ((! pLink) || (pLink->fStage & LINK_ERROR) || (iRes != ERROR_SUCCESS) || (pLink->fStage != UP)) {
        unsigned short cid_disconnect = 0;

        if (pLink) {
            if ((iRes == ERROR_SUCCESS) && (pLink->fStage != UP))
                iRes = ERROR_INTERNAL_ERROR;

            cid_disconnect = pLink->cid;
        } else
            iRes = ERROR_INTERNAL_ERROR;

        gpState->Unlock ();

        if (cid_disconnect)
            HIDCloseCID_Int (cid_disconnect);

        return iRes;
    }

    *pusCid = pLink->cid;

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static int HIDCloseCID_Int (unsigned short usCID) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Link *pLink = FindLink (usCID);
    if (! pLink) {
        gpState->Unlock ();

        return ERROR_NOT_FOUND;
    }

    unsigned short cid = pLink->cid;
    DeleteLink (pLink);
    HANDLE hL2CAP = gpState->hL2CAP;
    L2CA_Disconnect_In pCallback = gpState->l2cap_if.l2ca_Disconnect_In;

    gpState->Unlock ();

    __try {
        pCallback (hL2CAP, NULL, cid);
    } __except (1) {
    }

    return ERROR_SUCCESS;
}

//
//    Hid parser interface
//
//
//    Parser interface
//
int HidDevice::SetIdle(unsigned char bIdle) {
    // Initialize the packets
    BTHHIDPacket requestPacket;
    requestPacket.SetHeader(BTHID_SET_IDLE << 4);
    requestPacket.SetPayload(&bIdle, sizeof(bIdle));

    return WritePacket (this, &requestPacket, BTH_WIRE_TIMEOUT, NULL, BTH_PSM_CONTROL, TRUE);
}

int HidDevice::GetIdle(unsigned char* pbIdle) {
    BTHHIDPacket requestPacket;
    requestPacket.SetHeader(BTHID_GET_IDLE << 4);

    BTHHIDPacket *pPacket = NULL;
    int iErr = WritePacket (this, &requestPacket, BTH_WIRE_TIMEOUT, &pPacket, BTH_PSM_CONTROL, TRUE);

    if (iErr == ERROR_SUCCESS) {
        // Check if the request succeeded
        BYTE bHeader = pPacket->GetHeader();
        if ((bHeader >> 4) == BTHID_DATA) {
            // Request succeeded, get the payload to the caller
            BYTE *pPacketPayload;
            int   cbPacketPayload;

            pPacket->GetPayload(&pPacketPayload, &cbPacketPayload);
            if (cbPacketPayload == 1) {
                *pbIdle = *pPacketPayload;
            } else
                iErr = ERROR_INVALID_DATA;
        } else
            iErr = ERROR_INVALID_DATA;

        pPacket->ReleasePayload ();
        delete pPacket;
    }

    return iErr;
}

int HidDevice::SetProtocol(E_BTHID_PROTOCOLS protocol) {
    // Initialize the packets
    BTHHIDPacket requestPacket;
    BTHID_Header_Parameter param;

    // Initialize the packets
    param.bRawHeader = 0;
    param.setprotocol_p.bProtocol = protocol;
    requestPacket.SetHeader((BTHID_SET_PROTOCOL << 4) | param.bRawHeader);

    return WritePacket (this, &requestPacket, BTH_WIRE_TIMEOUT, NULL, BTH_PSM_CONTROL, TRUE);
}
int HidDevice::SetReport(E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, int iTimeout, BYTE bReportID) {
    // Initialize the packets
    BTHHIDPacket requestPacket;
    BTHID_Header_Parameter SetReport_param;

    // Initialize the packets
    SetReport_param.bRawHeader = 0;
    SetReport_param.setreport_p.bReportType = type;

    // Start by sending BTHIDSetReport_Request down the pipe.
    requestPacket.SetHeader((BTHID_SET_REPORT << 4) | SetReport_param.bRawHeader);
    requestPacket.SetPayload((PBYTE) pBuffer, cbBuffer);
    requestPacket.SetReportID(bReportID);

    BOOL fDoWait = (type != BTHID_REPORT_OUTPUT);
    int channel = (type == BTHID_REPORT_FEATURE) ? BTH_PSM_CONTROL : BTH_PSM_INTERRUPT;

    return WritePacket (this, &requestPacket, iTimeout, NULL, channel, fDoWait);
}

int HidDevice::GetReport(int iReportID, E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, PDWORD pcbTransfered, int iTimeout) {
    // Initialize the packets
    BTHHIDPacket requestPacket;
    BTHID_Header_Parameter GetReport_param;

    // Initialize the packets
    GetReport_param.bRawHeader = 0;
    GetReport_param.getreport_p.bReportType = type;
    GetReport_param.getreport_p.bSize = 0;

    // Start by sending BTHIDGetReport_Request down the pipe.
    requestPacket.SetHeader((BTHID_GET_REPORT << 4) | GetReport_param.bRawHeader);

    int channel = (type == BTHID_REPORT_FEATURE) ? BTH_PSM_CONTROL : BTH_PSM_INTERRUPT;

    BTHHIDPacket *pPacket = NULL;
    int iErr = WritePacket (this, &requestPacket, iTimeout, &pPacket, channel, TRUE);

    if (iErr == ERROR_SUCCESS) {
        // Check if the request succeeded
        BYTE bHeader = pPacket->GetHeader();
        if ((bHeader >> 4) == BTHID_DATA) {
            // Request succeeded, get the payload to the caller
            BYTE *pPacketPayload;
            int   cbPacketPayload;

            pPacket->GetPayload(&pPacketPayload, &cbPacketPayload);
            if (cbBuffer >= cbPacketPayload && cbPacketPayload>=0) {
                memcpy(pBuffer, pPacketPayload, cbPacketPayload);
                *pcbTransfered = cbPacketPayload;
            } else
                iErr = ERROR_INSUFFICIENT_BUFFER;
        } else
            iErr = ERROR_INVALID_DATA;

        pPacket->ReleasePayload ();
        delete pPacket;
    }

    return iErr;
}

//
// Main API section
//
static int HIDConnect (BT_ADDR *pbt) {
    BD_ADDR *pba = (BD_ADDR *)pbt;

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HidDevice *pDev = FindDevice (pba);

    if (! pDev) {
        pDev = MakeNewDevice (pba, FALSE);
    }

    if (pDev && pDev->ckDeviceTimeout) {
        gpState->pSchedule->UnScheduleEvent (pDev->ckDeviceTimeout);
        pDev->ckDeviceTimeout = 0;
    }

    gpState->Unlock ();

    if (! pDev)
        return ERROR_CONNECTION_REFUSED;

    unsigned short cidControl = 0;
    int iRes = HIDConnect_Int (pba, BTH_PSM_CONTROL, &cidControl);
    if (iRes != ERROR_SUCCESS)
        return iRes;

    unsigned short cidInterrupt = 0;
    iRes = HIDConnect_Int (pba, BTH_PSM_INTERRUPT, &cidInterrupt);
    if (iRes != ERROR_SUCCESS) {
        HIDCloseCID_Int (cidControl);
        return iRes;
    }

    return ERROR_SUCCESS;
}

static int HIDVerify (unsigned char *ucSDP, unsigned int cSDP) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    BTHHIDSdpParser sdpParser;

    int iErr = sdpParser.Start (ucSDP, cSDP);

    BOOL bb = FALSE;

    if (iErr == ERROR_SUCCESS)
        iErr = sdpParser.GetHIDReconnectInitiate(&bb);

    if (iErr == ERROR_SUCCESS)
        iErr = sdpParser.GetHIDNormallyConnectable(&bb);

    if (iErr == ERROR_SUCCESS)
        iErr = sdpParser.GetHIDVirtualCable(&bb);

    // Pass in an empty structure to retrieve the size of the blob
    BLOB blobReportDescriptor = {0, NULL};

    if (iErr == ERROR_SUCCESS)
        iErr = sdpParser.GetHIDReportDescriptor(&blobReportDescriptor);

    if (iErr == ERROR_SUCCESS) {
        // Now we have the size. Allocate and requery.
        SVSUTIL_ASSERT(blobReportDescriptor.cbSize > 0);
        blobReportDescriptor.pBlobData = (unsigned char *)g_funcAlloc (blobReportDescriptor.cbSize, g_pvAllocData);
        iErr = sdpParser.GetHIDReportDescriptor(&blobReportDescriptor);

        if (iErr == ERROR_SUCCESS)
            g_funcFree (blobReportDescriptor.pBlobData, g_pvFreeData);
    }

    sdpParser.End ();

    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: HIDVerify - parsing SDP record failed\n"));
    } else {
        IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"BTH HID: HIDVerify - successfully parsed SDP record\n"));
    }

    gpState->Unlock ();

    return iErr;
}

//
//    Marshaling code
//
#define TASK_CONNECT    1
#define TASK_VERIFY        2

struct Task {
    unsigned int what;

    union {
        struct {
            BT_ADDR b;
        } Connect;

        struct {
            unsigned int cSDP;
            unsigned char ucSDP[1];
        } Verify;
    };
};

static DWORD WINAPI ExecThread (LPVOID lpTask) {
    Task *pTask = (Task *)lpTask;

    DWORD dwError = ERROR_CALL_NOT_IMPLEMENTED;

    if (pTask->what == TASK_CONNECT)
        dwError = HIDConnect (&pTask->Connect.b);
    else if (pTask->what == TASK_VERIFY)
        dwError = HIDVerify (pTask->Verify.ucSDP, pTask->Verify.cSDP);

    return dwError;
}

static int Exec (Task *pTask) {
    HANDLE hThread = CreateThread (NULL, 0, ExecThread, pTask, 0, NULL);
    if (! hThread)
        return GetLastError ();

    WaitForSingleObject (hThread, INFINITE);
    DWORD dwCode;

    if (! GetExitCodeThread (hThread, &dwCode))
        dwCode = GetLastError ();

    CloseHandle (hThread);
    return dwCode;
}

#if defined (UNDER_CE)
//
//    Driver service funcs...
//
extern "C" BOOL WINAPI DllMain( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) {
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls ((HMODULE)hInstDll);
            svsutil_Initialize ();
            break;

        case DLL_PROCESS_DETACH:
            svsutil_DeInitialize ();
            break;
    }
    return HidMdd_DllEntry(fdwReason);
}

#if defined (SDK_BUILD)
HANDLE OpenEvent (DWORD dwDesiredAccess, BOOL bInheritHandle, LPCTSTR lpName) {
    return CreateEvent (NULL, TRUE, FALSE, lpName);
}
#endif

static DWORD WINAPI StackInitialize (LPVOID lpUnused) {
    for (int i = 0 ; i < 100 ; ++i) {
        HANDLE hBthStackInited = OpenEvent (EVENT_ALL_ACCESS, FALSE, BTH_NAMEDEVENT_STACK_INITED);

        if (hBthStackInited) {
            DWORD dwRes = WaitForSingleObject (hBthStackInited, INFINITE);
            CloseHandle (hBthStackInited);
            if (WAIT_OBJECT_0 == dwRes)
                return hiddev_CreateDriverInstance ();
        }

        Sleep (1000);
    }

    return ERROR_TIMEOUT;
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        EXECUTION THREAD: Client-application!
//            These functions are only executed on the caller's thread
//             i.e. the thread belongs to the client application
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    @func PVOID | BHI_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc    Returns a DWORD which will be passed to Open & Deinit or NULL if
//            unable to initialize the device.
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice
extern "C" DWORD BHI_Init (LPCWSTR lpszDevKey) {
    DebugInit();

    HANDLE hBthStackInited = OpenEvent (EVENT_ALL_ACCESS, FALSE, BTH_NAMEDEVENT_STACK_INITED);
    if (hBthStackInited) {
        DWORD dwRes = WaitForSingleObject (hBthStackInited, 0);
        CloseHandle (hBthStackInited);
        if (WAIT_OBJECT_0 == dwRes)
            if (hiddev_CreateDriverInstance () == ERROR_SUCCESS)
                goto exit;
    }

    CloseHandle (CreateThread (NULL, 0, StackInitialize, NULL, 0, NULL));

exit:
    return HidMdd_Init(lpszDevKey);
}

//    @func PVOID | BHI_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from BHI_Init call
//  @rdesc    Returns TRUE for success, FALSE for failure.
//    @remark    Routine exported by a device driver.  "PRF" is the string
//            passed in as lpszType in RegisterDevice
extern "C" BOOL BHI_Deinit(DWORD dwData) {
    hiddev_CloseDriverInstance ();

    DebugDeInit();

    return HidMdd_Deinit(dwData);
}

//    @func PVOID | BHI_Open        | Device open routine
//  @parm DWORD | dwData        | value returned from BHI_Init call
//  @parm DWORD | dwAccess        | requested access (combination of GENERIC_READ
//                                  and GENERIC_WRITE)
//  @parm DWORD | dwShareMode    | requested share mode (combination of
//                                  FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc    Returns a DWORD which will be passed to Read, Write, etc or NULL if
//            unable to open device.
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice
extern "C" DWORD BHI_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
    return HidMdd_Open(dwData, dwAccess, dwShareMode);
}

//    @func BOOL | BHI_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from BHI_Open call
//  @rdesc    Returns TRUE for success, FALSE for failure
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice
extern "C" BOOL BHI_Close (DWORD dwData)  {
    return HidMdd_Close(dwData);
}

//    @func DWORD | BHI_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from BHI_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc    Returns -1 for error, otherwise the number of bytes written.  The
//            length returned is guaranteed to be the length requested unless an
//            error condition occurs.
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice
//
extern "C" DWORD BHI_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
    return -1;
}

//    @func DWORD | BHI_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from BHI_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc    Returns 0 for end of file, -1 for error, otherwise the number of
//            bytes read.  The length returned is guaranteed to be the length
//            requested unless end of file or an error condition occurs.
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice
//
extern "C" DWORD BHI_Read (DWORD dwData, LPVOID pBuf, DWORD Len) {
    return -1;
}

//    @func DWORD | BHI_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from BHI_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc    Returns current position relative to start of file, or -1 on error
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//         in as lpszType in RegisterDevice

extern "C" DWORD BHI_Seek (DWORD dwData, long pos, DWORD type) {
    return (DWORD)-1;
}

//    @func void | BHI_PowerUp | Device powerup routine
//    @comm    Called to restore device from suspend mode.  You cannot call any
//            routines aside from those in your dll in this call.
extern "C" void BHI_PowerUp (void) {
    return;
}
//    @func void | BHI_PowerDown | Device powerdown routine
//    @comm    Called to suspend device.  You cannot call any routines aside from
//            those in your dll in this call.
extern "C" void BHI_PowerDown (void) {
    return;
}


//    @func BOOL | BHI_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from BHI_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc    Returns TRUE for success, FALSE for failure
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//        in as lpszType in RegisterDevice
extern "C" BOOL BHI_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
    int iError = ERROR_SUCCESS;

    switch (dwCode) {
    case BTHHID_IOCTL_HIDConnect:
        if (dwLenIn == sizeof(BT_ADDR)) {
            Task *pTask = (Task *)LocalAlloc (LMEM_FIXED, sizeof(Task));
            if (pTask) {
                pTask->what = TASK_CONNECT;
                if (CeSafeCopyMemory (&pTask->Connect.b, pBufIn, sizeof(pTask->Connect.b))) {
                    iError = Exec(pTask);
                } else {
                    iError = ERROR_INVALID_USER_BUFFER;
                }
                LocalFree (pTask);
            } else
                iError = ERROR_OUTOFMEMORY;
        } else
            iError = ERROR_INVALID_PARAMETER;
        break;

    case BTHHID_IOCTL_HIDDisconnect:
        if (dwLenIn == sizeof(BT_ADDR)) {
            BD_ADDR bda;
            __try {
                bda = *(BD_ADDR*)pBufIn;
            } __except(1) {
                iError = ERROR_INVALID_USER_BUFFER;
            }
            if (ERROR_SUCCESS == iError) {
                gpState->Lock ();
                HidDevice *pDev = FindDevice (&bda);
                if (pDev) {
                    DisconnectDevice(pDev, TRUE);
                }
                gpState->Unlock ();
            }
        }
        else
            iError = ERROR_INVALID_PARAMETER;
        break;

    case BTHHID_IOCTL_HIDVerify:
        if (dwLenIn > 0) {
            Task *pTask = (Task *)LocalAlloc (LMEM_FIXED, sizeof(Task) + dwLenIn);
            if (pTask) {
                pTask->what = TASK_VERIFY;
                if (CeSafeCopyMemory (pTask->Verify.ucSDP, pBufIn, dwLenIn)) {
                    pTask->Verify.cSDP = dwLenIn;
                    iError = Exec(pTask);
                } else {
                    iError = ERROR_INVALID_USER_BUFFER;
                }
                LocalFree (pTask);
            } else
                iError = ERROR_OUTOFMEMORY;
        } else
            iError = ERROR_INVALID_PARAMETER;
        break;

    case BTHHID_IOCTL_HidSendControl:
        if (dwLenIn > sizeof(BT_ADDR)) {
            BD_ADDR bda;
            __try {
                bda = *(BD_ADDR*)pBufIn;
            } __except(1) {
                iError = ERROR_INVALID_USER_BUFFER;
            }
            if (ERROR_SUCCESS == iError) {
                gpState->Lock ();
                HidDevice* pDev = FindDevice(&bda);
                gpState->Unlock ();
                if (pDev) {
                    BTHHIDPacket requestPacket;
                    __try {
                        requestPacket.AddPayloadChunk(pBufIn + sizeof(BT_ADDR), dwLenIn - sizeof(BT_ADDR));
                    } __except(1) {
                        iError = ERROR_INVALID_USER_BUFFER;
                    }
                    if (ERROR_SUCCESS == iError) {
                        iError = WritePacket (pDev, &requestPacket, BTH_WIRE_TIMEOUT, NULL, BTH_PSM_CONTROL, TRUE);
                    }
                }
                else {
                    iError = ERROR_INVALID_PARAMETER;
                }
            }
        } else
            iError = ERROR_INVALID_PARAMETER;

        break;

    case BTHHID_IOCTL_HidSendInterrupt:
        if (dwLenIn > sizeof(BT_ADDR)) {
            BD_ADDR bda;
            __try {
                bda = *(BD_ADDR*)pBufIn;
            } __except(1) {
                iError = ERROR_INVALID_USER_BUFFER;
            }
            if (ERROR_SUCCESS == iError) {
                gpState->Lock ();
                HidDevice* pDev = FindDevice(&bda);
                gpState->Unlock ();
                if (pDev) {
                    BTHHIDPacket requestPacket;
                    __try {
                        requestPacket.AddPayloadChunk(pBufIn + sizeof(BT_ADDR), dwLenIn - sizeof(BT_ADDR));
                    } __except(1) {
                        iError = ERROR_INVALID_USER_BUFFER;
                    }
                    if (ERROR_SUCCESS == iError) {
                        iError = WritePacket (pDev, &requestPacket, BTH_WIRE_TIMEOUT, NULL, BTH_PSM_INTERRUPT, FALSE);
                    }
                }
                else {
                    iError = ERROR_INVALID_PARAMETER;
                }
            }
        } else
            iError = ERROR_INVALID_PARAMETER;

        break;

    default:
        IFDBG(DebugOut (DEBUG_WARN, L"Unknown control code %d\n", dwCode));
        iError = ERROR_CALL_NOT_IMPLEMENTED;
    }

    if (iError != ERROR_SUCCESS) {
        SetLastError(iError);
        return FALSE;
    }

    HidMdd_IOControl(dwData, dwCode, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut);

    return TRUE;
}

#if defined(UNDER_CE) && CE_MAJOR_VER < 0x0003
extern "C" int _isctype(int c, int mask) {
    if ((c < 0) || (c > 0xff))
        return 0;
    return iswctype((wchar_t)c,(wctype_t)mask);
}
#endif //defined(UNDER_CE) && CE_MAJOR_VER < 0x0003

#endif    // UNDER_CE



// Get an Input, Output, or Feature report from the device. This is not to be
// be used for recurring interrupt reports. The PDD calls into the MDD with
// HidMdd_ProcessReport for that.
DWORD
WINAPI
HidPdd_GetReport(
    HID_PDD_HANDLE   hPddDevice,
    HIDP_REPORT_TYPE type,
    PCHAR            pbBuffer,
    DWORD            cbBuffer,
    PDWORD           pcbTransferred,
    DWORD            dwTimeout,
    BYTE             bReportID
    )
{
    HidDevice* pDev = (HidDevice*) hPddDevice;
    PREFAST_DEBUGCHK(pDev);

    return pDev->GetReport(bReportID, (E_BTHID_REPORT_TYPES)type, pbBuffer, cbBuffer, pcbTransferred, dwTimeout);
}

// Set an Input, Output, or Feature report on the device.
DWORD
WINAPI
HidPdd_SetReport(
    HID_PDD_HANDLE   hPddDevice,
    HIDP_REPORT_TYPE type,
    PCHAR            pbBuffer,
    DWORD            cbBuffer,
    DWORD            dwTimeout,
    BYTE             bReportID
    )
{
    HidDevice* pDev = (HidDevice*) hPddDevice;
    PREFAST_DEBUGCHK(pDev);
    return pDev->SetReport((E_BTHID_REPORT_TYPES)type, pbBuffer, cbBuffer, dwTimeout, bReportID);
}

DWORD
WINAPI
HidPdd_GetString(
    HID_PDD_HANDLE  hPddDevice,
    HID_STRING_TYPE stringType,
    DWORD           dwIdx,     // Only used with stringType == HID_STRING_INDEXED
    LPWSTR          pszBuffer, // Set to NULL to get character count
    DWORD           cchBuffer, // Count of chars that will fit into pszBuffer
                               // including the NULL terminator.
    PDWORD          pcchActual // Count of chars in the string NOT including
                               // the NULL terminator
    )
{
    HidDevice* pDev = (HidDevice*) hPddDevice;
    DEBUGCHK(pDev);

    // TODO: Get HID strings
    if (pcchActual)
        *pcchActual = 0;

    return ERROR_SUCCESS;
}

// Get the idle rate for a specific report.
DWORD
WINAPI
HidPdd_GetIdle(
    HID_PDD_HANDLE hPddDevice,
    PDWORD         pdwIdle,
    DWORD          dwReportID
    )
{
    HidDevice* pDev = (HidDevice*) hPddDevice;
    PREFAST_DEBUGCHK(pDev);

    return pDev->GetIdle((unsigned char*)pdwIdle);
}

// Set the idle rate for a specific report. The idle rate is expressed in
// 4 ms increments, so to set idle rate of 20 ms, bDuration should be 5.
// bDuration of 0 means infinite. bReportID of 0 means to apply to all
// reports.
DWORD
WINAPI
HidPdd_SetIdle(
    HID_PDD_HANDLE hPddDevice,
    DWORD          dwDuration,
    DWORD          dwReportID
    )
{
    HidDevice* pDev = (HidDevice*) hPddDevice;
    PREFAST_DEBUGCHK(pDev);

    return pDev->SetIdle((unsigned char)dwDuration);
}

// Commands the PDD to initiate some activity or configuration.
DWORD
WINAPI
HidPdd_IssueCommand(
    HID_PDD_HANDLE hPddDevice,        // PDD function parameter
    DWORD          dwMsg,             // Notification message
    WPARAM         wParam             // Message parameter
    )
{
    // We do not support remote wake or any other command
    return ERROR_SUCCESS;
}




