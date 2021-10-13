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
//------------------------------------------------------------------------------
//
//      Bluetooth Driver
//
//
// Module Name:
//
//      btd.cxx
//
// Abstract:
//
//      This file implements Bluetooth System Driver
//
//
//------------------------------------------------------------------------------
#include <windows.h>
#include <wincrypt.h>

#define PERSIST_LINK_KEYS       1
#define PERSIST_PINS            1

#if defined (SDK_BUILD)
#undef CP_UTF8
#endif

#include <svsutil.hxx>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_hcip.h>
#include <bt_ddi.h>

#include <bt_sdp.h>
#include <bthsdpdef.h>

#if defined (UNDER_CE)
#include <service.h>
#include <winsock2.h>
#endif

#include <bt_api.h>

#define BTD_EVENT_BASEBAND_CHANGED      0
#define BTD_EVENT_CONNECTIONS_CHANGED   1
#define BTD_EVENT_SECURITY_CHANGED      2
#define BTD_EVENT_CONNECTIVITY_CHANGED  3
#define BTD_EVENT_DEVICEID_CHANGED      4
#define BTD_EVENT_HARDWARE_CHANGED      5
#define BTD_EVENT_PAIRING_CHANGED       6
#define BTD_EVENT_NUMBER                7

#define BTD_AUTH_RETRY                  3
#define BTD_AUTH_SLEEP                  1000

#define BTD_POWER_RECHECK               30000
#define BTD_POWER_DELAY                 5000

#define BTD_POST_PAIRING_SLEEP_DEFAULT  1000
#define BTD_DEFAULT_ESCO_VOICE_SETTING  0x0060  // Linear Input Coding, 2's complement, 16-bit, CVSD air coding format

#define BTD_ESCO_PACKET_TYPE            (BT_SYNC_PACKET_TYPE_HV1|BT_SYNC_PACKET_TYPE_HV2|BT_SYNC_PACKET_TYPE_HV3|   \
                                         BT_SYNC_PACKET_TYPE_EV3|BT_SYNC_PACKET_TYPE_EV4|BT_SYNC_PACKET_TYPE_EV5)

#define BTD_DEFAULT_COD                 (BTH_COD_MAJOR_DEVICE_CLASS_COMPUTER | BTH_COD_MAJOR_SERVICE_CLASS_NETWORK |    \
                                         BTH_COD_MAJOR_SERVICE_CLASS_INFORMATION | BTH_COD_MAJOR_SERVICE_CLASS_OBEX |   \
                                         BTH_COD_MAJOR_SERVICE_CLASS_CAPTURE)

#define CALL_HCI_READ_BD_ADDR           0x01
#define CALL_HCI_CHANGE_LOCAL_NAME      0x02
#define CALL_HCI_INQUIRY                0x03
#define CALL_HCI_QUERY_REMOTE_NAME      0x04
#define CALL_HCI_WRITE_SCAN_ENABLE      0x05
#define CALL_HCI_WRITE_PAGE_TIMEOUT     0x06
#define CALL_HCI_AUTHENTICATE           0x07
#define CALL_HCI_SET_ENCRYPTION         0x08
#define CALL_HCI_READ_SCAN_ENABLE       0x09
#define CALL_HCI_READ_PAGE_TIMEOUT      0x0a
#define CALL_HCI_WRITE_CLASS_OF_DEVICE  0x0b
#define CALL_HCI_READ_CLASS_OF_DEVICE   0x0c
#define CALL_HCI_READ_LOCAL_VERSION     0x0d
#define CALL_HCI_READ_LOCAL_LMP         0x0e
#define CALL_HCI_READ_REMOTE_VERSION    0x0f
#define CALL_HCI_READ_REMOTE_LMP        0x10
#define CALL_HCI_WRITE_AUTHN_ENABLE     0x11
#define CALL_HCI_READ_AUTHN_ENABLE      0x12
#define CALL_HCI_WRITE_LINK_POLICY      0x13
#define CALL_HCI_READ_LINK_POLICY       0x14
#define CALL_HCI_ENTER_HOLD_MODE        0x15
#define CALL_HCI_ENTER_SNIFF_MODE       0x16
#define CALL_HCI_EXIT_SNIFF_MODE        0x17
#define CALL_HCI_ENTER_PARK_MODE        0x18
#define CALL_HCI_EXIT_PARK_MODE         0x19
#define CALL_HCI_CANCEL_INQUIRY         0x1a

#define CALL_L2CAP_CLEAN_UP_L2CAP       0x1b

#define CALL_HCI_EVENTFILTER            0x20
#define CALL_HCI_ACL_CONNECT            0x21
#define CALL_HCI_SCO_CONNECT            0x22
#define CALL_HCI_BASEBAND_DISCONNECT    0x23
#define CALL_HCI_SCO_ACCEPT_REJECT      0x24
#define CALL_HCI_SWITCH_ROLE            0x25
#define CALL_HCI_GET_ROLE               0x26
#define CALL_HCI_READ_RSSI              0x27
#define CALL_HCI_READ_INQUIRY_SCAN_ACTIVITY     0x28
#define CALL_HCI_WRITE_INQUIRY_SCAN_ACTIVITY    0x29
#define CALL_HCI_READ_PAGE_SCAN_ACTIVITY        0x2a
#define CALL_HCI_WRITE_PAGE_SCAN_ACTIVITY       0x2b
#define CALL_HCI_READ_INQUIRY_SCAN_TYPE         0x2c
#define CALL_HCI_WRITE_INQUIRY_SCAN_TYPE        0x2d
#define CALL_HCI_READ_PAGE_SCAN_TYPE            0x2e
#define CALL_HCI_WRITE_PAGE_SCAN_TYPE           0x2f

#define COMMAND_PERIODIC_TIMEOUT        5000

#define MAX_PENDING_PIN_REQUESTS        16

struct VersionInformation {
    unsigned char       hci_version;    // Local only
    unsigned short      hci_revision;   // Local only
    unsigned char       lmp_version;
    unsigned short      manufacturer_name;
    unsigned short      lmp_subversion;
};

struct ActivityResult {
    unsigned short usScanInterval;
    unsigned short usScanWindow;
}; 


struct SCall {
    SCall           *pNext;

    HANDLE          hProcOwner;

    HANDLE          hEvent;

    int             iResult;

    unsigned int    fWhat           : 8;
    unsigned int    fComplete       : 1;
    unsigned int    fRetry          : 1;

    union {
        VersionInformation  viResult;
        BD_ADDR             baResult;
        DWORD               dwResult;
        unsigned short      usResult;
        unsigned char       ucResult;
        unsigned char       ftmResult[8];
        ActivityResult      activityResult;
        
        struct {
            void            *pBuffer;
            DWORD           cbBuffer;
            DWORD           dwPerms;
            BOOL            bKMode;
        } ptrResult;
        
        
    };
};

struct Link {
    Link            *pNext;
    BD_ADDR         b;
    unsigned short  h;
    unsigned char   type;

    HANDLE          hProcOwner;
};

struct Security {
    Security        *pNext;
    BD_ADDR         b;
    int             csize;  // 0 for links
    unsigned char   ucdata[16];
};

struct PINRequest {
    PINRequest      *pNext;
    DWORD           dwTimeOutCookie;

    unsigned int    fConnectionLocked : 1;

    BD_ADDR         ba;
};

struct InquiryBuffer : public SVSRefObj {
    BT_ADDR         ba[256];
    unsigned int    cod[256];
    unsigned char   psm[256];
    unsigned char   pspm[256];
    unsigned char   psrm[256];
    unsigned short  co[256];

    int     cb;

    HANDLE  hWakeUpAll;

    int     iRes;

    InquiryBuffer (void) {
        cb = 0;
        iRes = ERROR_SUCCESS;
        hWakeUpAll = CreateEvent (NULL, TRUE, FALSE, NULL);
    }

    ~InquiryBuffer (void) {
        CloseHandle (hWakeUpAll);
    }
};

struct NotifyCaller {
    NotifyCaller* pNext;

    HANDLE id;      // Unique identifier for the caller context
    HANDLE hMsgQ;   // Message queue handle the caller has registered
    HANDLE hProc;   // Handle to the caller process
    DWORD dwClass;  // Class of notification events the caller wants

    NotifyCaller () {
        memset (this, 0, sizeof(*this));
    }

    ~NotifyCaller () {
        if (id)
            btutil_CloseHandle ((SVSHandle)id);
        if (hMsgQ)
            CloseMsgQueue (hMsgQ);
    }

    void *operator new (size_t iSize);
    void operator delete(void *ptr);
};

static int btshell_CloseDriverInstance (void);

static int btshell_InquiryComplete (void *pUserContext, void *pCallContext, unsigned char status);
static int btshell_InquiryResult (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned char page_scan_repetition_mode_list, unsigned char page_scan_period_mode, unsigned char page_scan_mode, unsigned int class_of_device, unsigned short clock_offset);
static int btshell_RemoteNameDone (void *pUserContext, void *pCallContext, unsigned char status, BD_ADDR *pba, unsigned char utf_name[BD_MAXNAME]);
static int btshell_PINCodeRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba);
static int btshell_LinkKeyRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba);
static int btshell_LinkKeyNotificationEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned char link_key[16], unsigned char key_type);
static int btshell_AuthenticationCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle);
static int btshell_EncryptionChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char encryption_enable);
static int btshell_ReadRemoteSupportedFeaturesCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char lmp_features[8]);
static int btshell_ReadRemoteVersionInformationCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char lmp_version, unsigned short manufacturers_name, unsigned short lmp_subversion);
static int btshell_ModeChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char current_more, unsigned short interval);
static int btshell_ConnectionCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, BD_ADDR *pba, unsigned char link_type, unsigned char encryption_mode);
static int btshell_DisconnectionCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char reason);
static int btshell_ConnectionRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned int class_of_device, unsigned char link_type);
static int btshell_RoleChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, BD_ADDR *pba, unsigned char new_role);
static int btshell_CustomCodeEvent (void *pUserContext, void *pCallContext, unsigned char cEventCode, unsigned char cEventLength, unsigned char *pEvent);
static int btshell_SynchronousConnectionCompleteEvent (void *pUserContext, void *pCallContect, unsigned char status, unsigned short connection_handle, BD_ADDR *pba, unsigned char link_type, unsigned char transmission_interval, unsigned char retransmit_window, unsigned short rx_packet_length, unsigned short tx_packet_length, unsigned char air_mode);

static int btshell_hStackEvent (void *pUserContext, int iEvent, void *pEventContext);

static int btshell_CommonCallbackHandler (void *pCallContext, unsigned char status);
static int btshell_CommonCallbackAbortHandler (void *pCallContext, unsigned char status);
static int btshell_BDADDR_Out (void *pCallContext, unsigned char status, BD_ADDR *pba);
static int btshell_ReadScan_Out (void *pCallContext, unsigned char status, unsigned char mask);
static int btshell_ReadPageTimeout_Out (void *pCallContext, unsigned char status, unsigned short page);
static int btshell_ReadClassOfDevice_Out (void *pCallContext, unsigned char status, unsigned int cod);
static int btshell_ReadAuthenticationEnable_Out (void *pCallContext, unsigned char status, unsigned char ae);
static int btshell_WriteLinkPolicySettings_Out (void *pCallContext, unsigned char status, unsigned short connection_handle);
static int btshell_ReadLinkPolicySettings_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned short link_policy_settings);
static int btshell_CommonAcceptConnectionRequest_Out (void *pCallContext, unsigned char status);
static int btshell_SwitchRole_Out (void *pCallContext, unsigned char status);
static int btshell_RoleDiscovery_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char current_role);
static int btshell_ReadRSSI_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, char RSSI);
static int btshell_ReadPageScanActivity_Out (void *pCallContext, unsigned char status, unsigned short page_scan_interval, unsigned short page_scan_window);
static int btshell_ReadInquiryScanActivity_Out (void *pCallContext, unsigned char status, unsigned short inquiry_scan_interval, unsigned short inquiry_scan_window);
static int btshell_ReadPageScanType_Out (void* pCallContext, unsigned char status, unsigned char page_scan_type);
static int btshell_ReadInquiryScanType_Out (void* pCallContext, unsigned char status, unsigned char inquiry_scan_type);

static int btshell_AuthenticationRequested_out (void *pCallContext, unsigned char status);
static int btshell_ReadLocalVersionInformation_Out (void *pCallContext, unsigned char status, unsigned char hci_version, unsigned short hci_revision, unsigned char lmp_version, unsigned short manufacturer_name, unsigned short lmp_subversion);
static int btshell_ReadLocalSupportedFeatures_Out (void *pCallContext, unsigned char status, unsigned char features_mask[8]);
static int btshell_AcceptSynchronousConnectionRequest_Out (void *pCallContext, unsigned char status);

static int btshell_CallAborted (void *pCallContext, int iError);

static int GetHandleForBA (BD_ADDR *pba, unsigned short *phandle);
static int LockL2CAPConnection (BD_ADDR *pba, int fLock);

static BOOL RouteSCOControl(void);

void SdpRemoveRecordsOnProcessExit(HANDLE hDyingProc);

//
//  L2CAP link is only to express-abort idle connections...
//
static int btshell_lStackEvent (void *pUserContext, int iEvent, void *pEventContext);

class BTSHELL : public SVSSynch, public SVSRefObj {
public:
    SCall           *pCalls;
    Security        *pSecurity;
    Link            *pLinks;
    NotifyCaller    *pNotifyCallers;

    unsigned int    fIsRunning : 1;
    unsigned int    fConnected : 1;
    unsigned int    fNoSuspend : 1;

    HANDLE          hL2CAP;
    L2CAP_INTERFACE l2cap_if;

    HANDLE          hHCI;
    HCI_INTERFACE   hci_if;

    FixedMemDescr   *pfmdCalls;
    FixedMemDescr   *pfmdSecurity;
    FixedMemDescr   *pfmdLinks;
    FixedMemDescr   *pfmdNotifyCaller;

    BD_ADDR         b;

    HANDLE          hNamedEvents[BTD_EVENT_NUMBER];

    PINRequest      pin_reqs[MAX_PENDING_PIN_REQUESTS];
    PINRequest      *pRequested;
    PINRequest      *pInService;
    PINRequest      *pFree;

    HANDLE          hAddrEvent;
    HANDLE          hSecurityUIEvent;
    HANDLE          hSecurityUIProc;
    DWORD           dwSecurityProcPermissions;

    DWORD           dwPINRequestStoreTimeout;
    DWORD           dwPINRequestProcessTimeout;

    SVSCookie       ckPowerMonitor;
    int             cntPowerMonitor;

    unsigned int    cntAcceptSCO;
    unsigned int    cntAcceptSync;
    
    DWORD           dwPairSleep;
    
    unsigned short  ESCOVoiceSetting;
    unsigned short  ESCOPacketType;

    void ClearPinRequests (void) {
        pFree = pRequested = pInService = NULL;

        memset (pin_reqs, 0, sizeof(pin_reqs));

        for (int i = 0 ; i < MAX_PENDING_PIN_REQUESTS ; ++i) {
            pin_reqs[i].pNext = pFree;
            pFree = &pin_reqs[i];
        }
    }

    BTSHELL (void) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: new BTSHELL\n"));
        pCalls = NULL;
        pSecurity = NULL;
        pLinks = NULL;
        pNotifyCallers = NULL;

        fIsRunning = FALSE;
        fConnected = FALSE;
        fNoSuspend = FALSE;

        ckPowerMonitor = 0;
        cntPowerMonitor = 0;

        cntAcceptSCO = 0;
        cntAcceptSync = 0;
        
        ClearPinRequests ();

        hSecurityUIEvent = NULL;
        hSecurityUIProc  = NULL;
        hAddrEvent = NULL;
        dwSecurityProcPermissions = 0;
        dwPINRequestStoreTimeout = 0;
        dwPINRequestProcessTimeout = 0;
        dwPairSleep = BTD_POST_PAIRING_SLEEP_DEFAULT;
        
        ESCOVoiceSetting = BTD_DEFAULT_ESCO_VOICE_SETTING;
        ESCOPacketType = BTD_ESCO_PACKET_TYPE;

        hL2CAP = NULL;

        memset (&l2cap_if, 0, sizeof(l2cap_if));

        hHCI = NULL;

        memset (&hci_if, 0, sizeof(hci_if));

        pfmdCalls = pfmdSecurity = pfmdLinks = NULL;

        if (! (pfmdCalls = svsutil_AllocFixedMemDescr (sizeof(SCall), 10)))
            return;

        if (! (pfmdSecurity = svsutil_AllocFixedMemDescr (sizeof(Security), 10)))
            return;

        if (! (pfmdLinks = svsutil_AllocFixedMemDescr (sizeof(Link), 10)))
            return;

        if (! (pfmdNotifyCaller = svsutil_AllocFixedMemDescr (sizeof(NotifyCaller), 10)))
            return;

        hNamedEvents[BTD_EVENT_BASEBAND_CHANGED]     = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_BASEBAND_CHANGED);
        hNamedEvents[BTD_EVENT_CONNECTIONS_CHANGED]  = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_CONNECTIONS_CHANGED);
        hNamedEvents[BTD_EVENT_SECURITY_CHANGED]     = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_SECURITY_CHANGED);
        hNamedEvents[BTD_EVENT_CONNECTIVITY_CHANGED] = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_CONNECTIVITY_CHANGED);
        hNamedEvents[BTD_EVENT_DEVICEID_CHANGED]     = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_DEVICEID_CHANGED);
        hNamedEvents[BTD_EVENT_HARDWARE_CHANGED]     = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_HARDWARE_CHANGED);
        hNamedEvents[BTD_EVENT_PAIRING_CHANGED]      = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_PAIRING_CHANGED);

        hAddrEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        L2CAP_EVENT_INDICATION lei;
        memset (&lei, 0, sizeof(lei));

        lei.l2ca_StackEvent = btshell_lStackEvent;

        L2CAP_CALLBACKS lc;
        memset (&lc, 0, sizeof(lc));

        lc.l2ca_CallAborted = btshell_CallAborted;

        int cL2CAPHeaders = 0;
        int cL2CAPTrailers = 0;

        if (ERROR_SUCCESS != L2CAP_EstablishDeviceContext (this, L2CAP_PSM_MULTIPLE, &lei, &lc, &l2cap_if, &cL2CAPHeaders, &cL2CAPTrailers, &hL2CAP))
            return;

        HCI_EVENT_INDICATION hei;
        memset (&hei, 0, sizeof (hei));

        hei.hci_InquiryCompleteEvent = btshell_InquiryComplete;
        hei.hci_InquiryResultEvent = btshell_InquiryResult;
        hei.hci_RemoteNameRequestCompleteEvent = btshell_RemoteNameDone;
        hei.hci_LinkKeyRequestEvent = btshell_LinkKeyRequestEvent;
        hei.hci_PINCodeRequestEvent = btshell_PINCodeRequestEvent;
        hei.hci_LinkKeyNotificationEvent = btshell_LinkKeyNotificationEvent;
        hei.hci_AuthenticationCompleteEvent = btshell_AuthenticationCompleteEvent;
        hei.hci_EncryptionChangeEvent = btshell_EncryptionChangeEvent;
        hei.hci_ReadRemoteSupportedFeaturesCompleteEvent = btshell_ReadRemoteSupportedFeaturesCompleteEvent;
        hei.hci_ReadRemoteVersionInformationCompleteEvent = btshell_ReadRemoteVersionInformationCompleteEvent;
        hei.hci_ModeChangeEvent = btshell_ModeChangeEvent;
        hei.hci_ConnectionCompleteEvent = btshell_ConnectionCompleteEvent;
        hei.hci_DisconnectionCompleteEvent = btshell_DisconnectionCompleteEvent;
        hei.hci_ConnectionRequestEvent = btshell_ConnectionRequestEvent;
        hei.hci_RoleChangeEvent = btshell_RoleChangeEvent;
        hei.hci_CustomCodeEvent = btshell_CustomCodeEvent;
        hei.hci_SynchronousConnectionCompleteEvent = btshell_SynchronousConnectionCompleteEvent;

        hei.hci_StackEvent = btshell_hStackEvent;

        HCI_CALLBACKS hc;
        memset (&hc, 0, sizeof (hc));

        hc.hci_ChangeLocalName_Out = btshell_CommonCallbackHandler;
        hc.hci_Inquiry_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_InquiryCancel_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadBDADDR_Out = btshell_BDADDR_Out;
        hc.hci_RemoteNameRequest_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_WriteScanEnable_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadScanEnable_Out = btshell_ReadScan_Out;
        hc.hci_WritePageTimeout_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadPageTimeout_Out = btshell_ReadPageTimeout_Out;
        hc.hci_WriteClassOfDevice_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadClassOfDevice_Out = btshell_ReadClassOfDevice_Out;
        hc.hci_WriteAuthenticationEnable_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadAuthenticationEnable_Out = btshell_ReadAuthenticationEnable_Out;
        hc.hci_WriteLinkPolicySettings_Out = btshell_WriteLinkPolicySettings_Out;
        hc.hci_ReadLinkPolicySettings_Out = btshell_ReadLinkPolicySettings_Out;
        hc.hci_AuthenticationRequested_Out = btshell_AuthenticationRequested_out;
        hc.hci_SetConnectionEncryption_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ReadRemoteSupportedFeatures_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ReadRemoteVersionInformation_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ReadLocalSupportedFeatures_Out = btshell_ReadLocalSupportedFeatures_Out;
        hc.hci_ReadLocalVersionInformation_Out = btshell_ReadLocalVersionInformation_Out;
        hc.hci_HoldMode_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_SniffMode_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ParkMode_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ExitSniffMode_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_ExitParkMode_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_CreateConnection_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_Disconnect_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_SetEventFilter_Out = btshell_CommonCallbackHandler;
        hc.hci_AddSCOConnection_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_AcceptConnectionRequest_Out = btshell_CommonAcceptConnectionRequest_Out;
        hc.hci_SwitchRole_Out = btshell_SwitchRole_Out;
        hc.hci_RoleDiscovery_Out = btshell_RoleDiscovery_Out;
        hc.hci_ReadRSSI_Out = btshell_ReadRSSI_Out;
        hc.hci_WritePageScanActivity_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadPageScanActivity_Out = btshell_ReadPageScanActivity_Out;
        hc.hci_WriteInquiryScanActivity_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadInquiryScanActivity_Out = btshell_ReadInquiryScanActivity_Out;
        hc.hci_WritePageScanType_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadPageScanType_Out = btshell_ReadPageScanType_Out;
        hc.hci_WriteInquiryScanType_Out = btshell_CommonCallbackHandler;
        hc.hci_ReadInquiryScanType_Out = btshell_ReadInquiryScanType_Out;
        hc.hci_SetupSynchronousConnection_Out = btshell_CommonCallbackAbortHandler;
        hc.hci_AcceptSynchronousConnectionRequest_Out = btshell_CommonAcceptConnectionRequest_Out;
        
        hc.hci_CallAborted = btshell_CallAborted;

        int cHciHeaders = 0;
        int cHciTrailers = 0;
        int cLinkType = 0;
        int cControl = BTH_CONTROL_DEVICEONLY | BTH_CONTROL_ROUTE_SECURITY | BTH_CONTROL_ROUTE_HARDWARE;

        if (RouteSCOControl()) {
            // If SCO audio routing is supported in hardware, we need
            // to get ConnectionRequest messages for SCO connections
            cControl |= BTH_CONTROL_ROUTE_BY_LINKTYPE;
            cLinkType = BTH_LINK_TYPE_SCO;
        }

        if (ERROR_SUCCESS != HCI_EstablishDeviceContextEx (this, cControl, NULL, 0, cLinkType, &hei, sizeof(hei), &hc, sizeof(hc), &hci_if, sizeof(hci_if), &cHciHeaders, &cHciTrailers, &hHCI))
            return;

        fIsRunning = TRUE;

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: new BTSHELL successful\n"));
    }

    ~BTSHELL (void) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: delete BTSHELL\n"));
        if (hL2CAP)
            L2CAP_CloseDeviceContext (hL2CAP);

        if (hHCI)
            HCI_CloseDeviceContext (hHCI);

        if (pfmdCalls)
            svsutil_ReleaseFixedNonEmpty (pfmdCalls);

        if (pfmdSecurity)
            svsutil_ReleaseFixedNonEmpty (pfmdSecurity);

        if (pfmdLinks)
            svsutil_ReleaseFixedNonEmpty (pfmdLinks);

        if (pfmdNotifyCaller)
            svsutil_ReleaseFixedNonEmpty (pfmdNotifyCaller);

        if (hAddrEvent)
            CloseHandle(hAddrEvent);

        for (int i = 0 ; i < BTD_EVENT_NUMBER ; ++i)
            CloseHandle (hNamedEvents[i]);
    }
};

typedef void (*PFN_SYSTEM_TIMER_RESET) (void);

static BTSHELL *gpState = NULL;
static HANDLE ghBthApiEvent = NULL;
static PFN_SYSTEM_TIMER_RESET gfnSystemTimerReset = NULL;

//
//  Auxiliary code
//
static PINRequest *GetNewPINRequest (void) {
    PINRequest *pResult = gpState->pFree;
    if (pResult)
        gpState->pFree = pResult->pNext;

    return pResult;
}

static void FreeRequest (PINRequest *pPinReq) { // This unlocks...
#if defined (DEBUG) || defined (_DEBUG)
    for (int i = 0 ; i < MAX_PENDING_PIN_REQUESTS ; ++i) {
        if (pPinReq == &gpState->pin_reqs[i])
            break;
    }

    SVSUTIL_ASSERT (i != MAX_PENDING_PIN_REQUESTS);

    PINRequest *p = gpState->pFree;
    while (p) {
        SVSUTIL_ASSERT (p != pPinReq);
        p = p->pNext;
    }

    p = gpState->pRequested;
    while (p) {
        SVSUTIL_ASSERT (p != pPinReq);
        p = p->pNext;
    }

    p = gpState->pInService;
    while (p) {
        SVSUTIL_ASSERT (p != pPinReq);
        p = p->pNext;
    }
#endif

    pPinReq->pNext = gpState->pFree;
    gpState->pFree = pPinReq;

    if (pPinReq->fConnectionLocked)
        LockL2CAPConnection (&pPinReq->ba, FALSE);  // This unlocks the state
}

static void CleanUpSecurityUIHandler (void) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: security handler does not exist any more - cleaning up!\n"));

    gpState->hSecurityUIEvent = NULL;
    gpState->hSecurityUIProc = NULL;
    gpState->dwSecurityProcPermissions = 0;

    gpState->dwPINRequestStoreTimeout = 0;
    gpState->dwPINRequestProcessTimeout = 0;
}

static void FinishTimeout (LPVOID pArg, PINRequest **pChain) {
    PINRequest *pParent = NULL;
    PINRequest *pThis = *pChain;

    while (pThis && pThis != pArg)
        pThis = pThis->pNext;


    if (! pThis) {
        gpState->Unlock ();
        return;
    }

    if (pParent)
        pParent->pNext = pThis->pNext;
    else
        *pChain = pThis->pNext;

    BD_ADDR ba = pThis->ba;

    HANDLE h = gpState->hHCI;

    HCI_PINCodeRequestNegativeReply_In pCallbackNo = gpState->hci_if.hci_PINCodeRequestNegativeReply_In;

    FreeRequest (pThis);

    gpState->Unlock ();

    __try {
        pCallbackNo (h, NULL, &ba);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_PINCodeRequest[Negative]Reply\n"));
    }
}

static DWORD WINAPI StorePINTimeout (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"StorePINTimeout\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    FinishTimeout (pArg, &gpState->pRequested); // This unlocks

    return 0;
}

static DWORD WINAPI ServicePINTimeout (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"ServicePINTimeout\n"));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    FinishTimeout (pArg, &gpState->pInService); // This unlocks

    return 0;
}

static int AddNewPINRequest (BD_ADDR *pba) {
    if (! gpState->hSecurityUIEvent) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell AddNewPINRequest: security handler not installed!\n"));
        return FALSE;
    }

    DWORD dwPerms = SetProcPermissions (gpState->dwSecurityProcPermissions);
    BOOL bRes = SetEvent (gpState->hSecurityUIEvent);
    SetProcPermissions (dwPerms);

    if (! bRes) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell AddNewPINRequest: security handler failed\n"));

        CleanUpSecurityUIHandler ();
        return FALSE;
    }

    PINRequest *pPRE = gpState->pRequested;
    while (pPRE) {
        if (pPRE->ba == *pba)
            break;

        pPRE = pPRE->pNext;
    }

    if (! pPRE) {
        pPRE = gpState->pInService;
        while (pPRE) {
            if (pPRE->ba == *pba)
                break;

            pPRE = pPRE->pNext;
        }
    }

    if (pPRE) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell AddNewPINRequest: %04x%08x already in process\n", pba->NAP, pba->SAP));
        return FALSE;
    }

    pPRE = GetNewPINRequest ();

    if (! pPRE) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell AddNewPINRequest: - no free slots for PIN requests!\n"));
        return FALSE;
    }

    memset (pPRE, 0, sizeof(*pPRE));

    pPRE->ba = *pba;
    pPRE->pNext = gpState->pRequested;
    gpState->pRequested = pPRE;
    pPRE->dwTimeOutCookie = btutil_ScheduleEvent (StorePINTimeout, pPRE, gpState->dwPINRequestStoreTimeout);

    pPRE->fConnectionLocked = LockL2CAPConnection (pba, TRUE);

    return TRUE;
}

static void UnscheduleAllPINReqests (void) {
    PINRequest *p = gpState->pRequested;
    while (p) {
        btutil_UnScheduleEvent (p->dwTimeOutCookie);
        p->dwTimeOutCookie = 0;
        p = p->pNext;
    }

    p = gpState->pInService;
    while (p) {
        btutil_UnScheduleEvent (p->dwTimeOutCookie);
        p->dwTimeOutCookie = 0;
        p = p->pNext;
    }
}

static void PersistKeyOrPin (BD_ADDR* pba, unsigned char *pBuffer, unsigned int cbBuffer, BOOL fLinkKey) {
    HKEY hk;
    DWORD dwDisp;

    if (ERROR_SUCCESS == RegCreateKeyEx (HKEY_BASE, L"comm\\security\\bluetooth", 0, NULL, 0, KEY_WRITE, NULL, &hk, &dwDisp)) {
        DATA_BLOB dataIn;
        DATA_BLOB dataOut = {0,NULL};
        
        WCHAR szValueName[3 + 4 + 8 + 1];
        if (fLinkKey)
            _snwprintf (szValueName, SVSUTIL_ARRLEN(szValueName), L"key%04x%08x", pba->NAP, pba->SAP);
        else
            _snwprintf (szValueName, SVSUTIL_ARRLEN(szValueName), L"pin%04x%08x", pba->NAP, pba->SAP);

        szValueName[SVSUTIL_ARRLEN(szValueName)-1] = 0;
        
        dataIn.cbData = cbBuffer;
        dataIn.pbData = (PBYTE)pBuffer;

        if (! CryptProtectData(&dataIn, NULL, NULL, NULL, NULL, CRYPTPROTECT_SYSTEM, &dataOut)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"PersistKeyOrPin: Error encrypting key or pin - not persisted.\n"));
        } else {
            RegSetValueEx (hk, szValueName, 0, REG_BINARY, dataOut.pbData, dataOut.cbData);
            LocalFree (dataOut.pbData);
        }

        RegCloseKey (hk);
        RegFlushKey (HKEY_LOCAL_MACHINE);
    }
}

#if defined (PERSIST_LINK_KEYS)
static void PersistLinkKey (BD_ADDR* pba, unsigned char link_key[16]) {
    PersistKeyOrPin (pba, link_key, 16, TRUE);
}
#endif

#if defined (PERSIST_PINS)
static void PersistPIN (BD_ADDR* pba, unsigned char* ppin, int cbPin) {
    PersistKeyOrPin (pba, ppin, cbPin, FALSE);
}
#endif

static BTSHELL *CreateNewState (void) {
    return new BTSHELL;
}

static InquiryBuffer *CreateNewInquiryBuffer (void) {
    return new InquiryBuffer;
}

static SCall *AllocCall (int fWhat) {
    SCall *pCall = (SCall *)svsutil_GetFixed (gpState->pfmdCalls);
    if (! pCall)
        return NULL;

    memset (pCall, 0, sizeof(*pCall));

    pCall->hProcOwner = GetOwnerProcess ();
    pCall->fWhat = fWhat;

    pCall->hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (NULL == pCall->hEvent) {
        svsutil_FreeFixed (pCall, gpState->pfmdCalls);
        return NULL;
    }

    pCall->pNext = gpState->pCalls;
    gpState->pCalls = pCall;

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Allocated call 0x%08x what = 0x%02x\n", pCall, fWhat));

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
            IFDBG(DebugOut (DEBUG_ERROR, L"Shell: call not in list in DeleteCall!\n"));

            return;
        }

        pParent->pNext = pCall->pNext;
    }

    CloseHandle (pCall->hEvent);

    svsutil_FreeFixed (pCall, gpState->pfmdCalls);
}

static SCall *VerifyCall (SCall *pCall) {
    SCall *p = gpState->pCalls;
    while (p && (p != pCall))
        p = p->pNext;

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
    if (! p)
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: call verify failed!\n"));
#endif

    return p;
}

static SCall *FindCall (unsigned int fOp) {
    SCall *p = gpState->pCalls;
    while (p && (p->fWhat != fOp))
        p = p->pNext;

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
    if (! p)
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: call find failed!\n"));
#endif

    return p;
}

static void NotifyEvent (PBTEVENT pbtEvent, DWORD dwEventClass) {
    SVSUTIL_ASSERT (gpState->IsLocked ());

    NotifyCaller* pCurr = gpState->pNotifyCallers;
    while (pCurr) {
        if (dwEventClass & pCurr->dwClass) {
            BOOL fRet = WriteMsgQueue (pCurr->hMsgQ, pbtEvent, sizeof(BTEVENT), 0, 0);

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
            if (! fRet) {
                IFDBG(DebugOut (DEBUG_ERROR, L"Shell: NotifyEvent could not write message queue entry [q=%d]!\n", pCurr->hMsgQ));
                SVSUTIL_ASSERT (0);
            }
#endif
        }

        pCurr = pCurr->pNext;
    }
}

static DWORD WINAPI StackDisconnect (LPVOID lpVoid) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: DisconnectStack\n"));

    btshell_CloseDriverInstance ();

    return ERROR_SUCCESS;
}

static DWORD WINAPI StackDown (LPVOID lpVoid) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if ((! gpState->fIsRunning) || (! gpState->fConnected)) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fConnected = FALSE;

    memset (&gpState->b, 0, sizeof(gpState->b));

    UnscheduleAllPINReqests ();

    gpState->ClearPinRequests ();

    SCall *pC = gpState->pCalls;
    while (pC) {
        if (! pC->fComplete) {
            pC->fComplete = TRUE;
            pC->iResult = ERROR_CONNECTION_UNAVAIL;
            SetEvent (pC->hEvent);
        } else {
            if (pC->iResult == ERROR_SUCCESS)
                pC->iResult = ERROR_CONNECTION_UNAVAIL;
        }

        pC = pC->pNext;
    }

    while (gpState->pLinks) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"StackDown : Deleting link 0x%04x\n", gpState->pLinks->h));

        Link *pNext = gpState->pLinks->pNext;
        svsutil_FreeFixed (gpState->pLinks, gpState->pfmdLinks);
        gpState->pLinks = pNext;
    }

    SetEvent (gpState->hNamedEvents[BTD_EVENT_HARDWARE_CHANGED]);
    ResetEvent (gpState->hNamedEvents[BTD_EVENT_HARDWARE_CHANGED]);

    BTEVENT bte;
    memset (&bte, 0, sizeof(BTEVENT));
    bte.dwEventId = BTE_STACK_DOWN;

    NotifyEvent (&bte, BTE_CLASS_STACK);

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static DWORD WINAPI StackUp (LPVOID lpVoid) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if ((! gpState->fIsRunning) || gpState->fConnected) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fConnected = TRUE;
    ResetEvent (gpState->hAddrEvent);

    SCall *pCall = AllocCall (CALL_HCI_READ_BD_ADDR);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pCall->hProcOwner = NULL;

    HANDLE hEvent = pCall->hEvent;
    HCI_ReadBDADDR_In pCallback = gpState->hci_if.hci_ReadBDADDR_In;
    HANDLE hHCI = gpState->hHCI;
    gpState->AddRef ();
    gpState->Unlock ();
    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    SetEvent (gpState->hAddrEvent);

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_CANCELLED;
    }

    if (pCall == VerifyCall (pCall)) {
        if (iRes == ERROR_SUCCESS) {
            if (pCall->fComplete)
                iRes = pCall->iResult;
            else
                iRes = ERROR_INTERNAL_ERROR;
        }

        if (iRes == ERROR_SUCCESS)
            gpState->b = pCall->baResult;

        DeleteCall (pCall);
    } else if (iRes == ERROR_SUCCESS)
        iRes = ERROR_INTERNAL_ERROR;

    if (iRes != ERROR_SUCCESS) {
        gpState->Unlock ();
        return iRes;
    }

    WCHAR szComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
    unsigned char aComputerName[BD_MAXNAME];
    memset (aComputerName, 0, sizeof(aComputerName));

    if (gpState->fIsRunning && gpState->fConnected && GetComputerName (szComputerName, &dwSize) &&
        (WideCharToMultiByte (CP_UTF8, 0, szComputerName, -1, (char *)aComputerName, sizeof(aComputerName), NULL, NULL) ||
        WideCharToMultiByte (CP_ACP, 0, szComputerName, -1, (char *)aComputerName, sizeof(aComputerName), NULL, NULL))) {

        HCI_ChangeLocalName_In pNameCallback = gpState->hci_if.hci_ChangeLocalName_In;
        pCall = AllocCall (CALL_HCI_CHANGE_LOCAL_NAME);
        if (! pCall) {
            gpState->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        pCall->hProcOwner = NULL;

        hEvent = pCall->hEvent;

        gpState->AddRef ();
        gpState->Unlock ();
        iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pNameCallback (hHCI, pCall, aComputerName);
        } __except (1) {
        }

        if (iRes == ERROR_SUCCESS)
            WaitForSingleObject (hEvent, 5000);

        gpState->Lock ();
        gpState->DelRef ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            return ERROR_CANCELLED;
        }

        if (pCall == VerifyCall (pCall)) {
            if (iRes == ERROR_SUCCESS) {
                if (pCall->fComplete)
                    iRes = pCall->iResult;
                else
                    iRes = ERROR_INTERNAL_ERROR;
            }

            DeleteCall (pCall);
        } else if (iRes == ERROR_SUCCESS)
            iRes = ERROR_INTERNAL_ERROR;

        if (iRes != ERROR_SUCCESS) {
            gpState->Unlock ();
            return iRes;
        }
    }

    HANDLE hSignal = gpState->hNamedEvents[BTD_EVENT_HARDWARE_CHANGED];
    
    unsigned int cod = BTD_DEFAULT_COD;

    gpState->fNoSuspend = FALSE;    

    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, L"software\\microsoft\\bluetooth\\sys", 0, KEY_READ, &hk)) {
        DWORD dwType;

        DWORD dw = 0;
        dwSize = sizeof(cod);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"COD", NULL, &dwType, (BYTE *)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)))
            cod = dw;

        dw = 0;
        dwSize = sizeof(dw);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"DisableAutoSuspend", NULL, &dwType, (BYTE *)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && dw)
            gpState->fNoSuspend = TRUE;

        dw = 0;
        dwSize = sizeof(dw);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"PostPairSleep", NULL, &dwType, (BYTE *)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)))
            gpState->dwPairSleep = dw;
            
        dw = 0;
        dwSize = sizeof(dw);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"AcceptESCOVoiceSetting", NULL, &dwType, (BYTE *)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)))
            gpState->ESCOVoiceSetting = (unsigned short) dw;
            
        dw = 0;
        dwSize = sizeof(dw);
            
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ESCOPacketType", NULL, &dwType, (BYTE *)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)))
            gpState->ESCOPacketType = (unsigned short) dw;

        RegCloseKey (hk);
    }

    gpState->AddRef ();
    gpState->Unlock ();
    
    BthWriteCOD (cod);

    gpState->Lock ();
    gpState->DelRef ();

    SetEvent (hSignal);
    ResetEvent (hSignal);

    BTEVENT bte;
    memset (&bte, 0, sizeof(BTEVENT));
    bte.dwEventId = BTE_STACK_UP;

    NotifyEvent (&bte, BTE_CLASS_STACK);

    gpState->Unlock();

    return ERROR_SUCCESS;
}

static void ProcessExited (HANDLE hProc) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: ProcessExited 0x%08x\n", hProc));

    if (! gpState)
        return;

    gpState->Lock ();

    if (gpState->hSecurityUIProc == hProc)
        CleanUpSecurityUIHandler ();

    //
    // Clean up notification contexts
    //

    NotifyCaller* pPrev = NULL;
    NotifyCaller* pCurr = gpState->pNotifyCallers;

    while (pCurr) {
        if (pCurr->hProc == hProc) {
            NotifyCaller* pDel = pCurr;

            if (pPrev)
                pPrev->pNext = pCurr->pNext;
            else
                gpState->pNotifyCallers = pCurr->pNext;

            pCurr = pCurr->pNext;

            delete pDel; // destructor cleans up data
        } else {
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }
    }

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return;
    }

    //
    // Clean up calls & links
    //

    SCall *pCall = gpState->pCalls;
    while (pCall) {
        if ((pCall->hProcOwner == hProc) && (! pCall->fComplete)) {
            pCall->fComplete = TRUE;
            pCall->iResult = ERROR_SHUTDOWN_IN_PROGRESS;
            SetEvent (pCall->hEvent);
        }

        pCall = pCall->pNext;
    }

    Link *pLink = gpState->pLinks;
    Link *pParent = NULL;

    while (pLink) {
        if (pLink->hProcOwner == hProc) {
            if (pParent)
                pParent->pNext = pLink->pNext;
            else
                gpState->pLinks = pLink->pNext;

            unsigned short h = pLink->h;
            svsutil_FreeFixed (pLink, gpState->pfmdLinks);

            HCI_Disconnect_In pCallback = gpState->hci_if.hci_Disconnect_In;
            HANDLE hHCI = gpState->hHCI;

            gpState->AddRef ();
            gpState->Unlock ();

            __try {
                pCallback (hHCI, NULL, h, 0x13);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BTSHELL :: Process exited :: exception in hci_Disconnect_In\n"));
            }

            gpState->Lock ();
            gpState->DelRef ();

            pLink = gpState->pLinks;
            pParent = NULL;
        } else {
            pParent = pLink;
            pLink = pLink->pNext;
        }
    }

    gpState->Unlock ();

    SdpRemoveRecordsOnProcessExit(hProc);
}

static int CleanUpL2CAP (SCall *pCall) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : ensuring that skies are clear\n"));

    BT_LAYER_IO_CONTROL pCallback = gpState->l2cap_if.l2ca_ioctl;
    int iRes = ERROR_INTERNAL_ERROR;

    int fWait = FALSE;
    int cRet = 0;

    HANDLE hL2CAP = gpState->hL2CAP;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    __try {
        iRes = pCallback (hL2CAP, BTH_L2CAP_IOCTL_DROP_IDLE, sizeof(pCall), (char *)&pCall, sizeof(fWait), (char *)&fWait, &cRet);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: CleanUpL2CAP : excepted in l2ca_ioctl\n"));
    }

    if ((iRes == ERROR_SUCCESS) && fWait) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : have connection to clean up, waiting...\n"));
        WaitForSingleObject (hEvent, 15000);
    }

    gpState->Lock ();
    gpState->DelRef ();

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : %d\n", iRes));
        return iRes;
    }

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : ERROR_INTERNAL_ERROR\n"));
        return ERROR_INTERNAL_ERROR;
    }

    iRes = pCall->iResult;

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : aborted, %d\n", iRes));
        return iRes;
    }

    pCall->fComplete = FALSE;

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: CleanUpL2CAP : ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}

static int GetHandleForBA
(
BD_ADDR *pba,
unsigned short  *phandle
) {
    // First, get the connection handle
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"GetHandleForBA : %04x%08x : getting connection handle\n", pba->NAP, pba->SAP));

    union {
        BD_ADDR b;
        unsigned char data[sizeof(BD_ADDR) + 1];
    } in_buff;

    memset (&in_buff, 0, sizeof(in_buff));
    in_buff.b = *pba;

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pCallbackIOCTL = gpState->hci_if.hci_ioctl;
    gpState->AddRef ();
    gpState->Unlock ();
    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        int dwData = 0;
        iRes = pCallbackIOCTL (hHCI, BTH_HCI_IOCTL_GET_HANDLE_FOR_BD, sizeof(in_buff), (char *)&in_buff, sizeof(*phandle), (char *)phandle, &dwData);
        if ((iRes == ERROR_SUCCESS) && (dwData != sizeof(*phandle))) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] GetHandleForBA : %04x%08x : incorrect return buffer in hci_ioctl\n", pba->NAP, pba->SAP));
            iRes = ERROR_INTERNAL_ERROR;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] GetHandleForBA : %04x%08x : exception in hci_ioctl\n", pba->NAP, pba->SAP));
    }
    gpState->Lock ();
    gpState->DelRef ();

    if ((iRes == ERROR_SUCCESS) && (! gpState->fConnected)) {
        iRes = ERROR_SERVICE_NOT_ACTIVE;
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] GetHandleForBA : %04x%08x (rare event!) disconnected while in ioctl\n", pba->NAP, pba->SAP));
    }

    return iRes;
}

static BOOL IsLinkEncrypted
(
unsigned short handle
) {    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"IsLinkEncrypted : handle=%d\n", handle));

    unsigned int fEncrypted = FALSE;

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pCallbackIOCTL = gpState->hci_if.hci_ioctl;
    gpState->AddRef ();
    gpState->Unlock ();
    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        int dwData = 0;
        iRes = pCallbackIOCTL (hHCI, BTH_HCI_IOCTL_HANDLE_ENCRYPTED, sizeof(handle), (char *)&handle, sizeof(fEncrypted), (char *)&fEncrypted, &dwData);
        if ((iRes == ERROR_SUCCESS) && (dwData != sizeof(fEncrypted))) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] IsLinkEncrypted : %d : incorrect return buffer in hci_ioctl\n", handle));
            iRes = ERROR_INTERNAL_ERROR;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] IsLinkEncrypted : %d : exception in hci_ioctl\n", handle));
    }
    gpState->Lock ();
    gpState->DelRef ();

    if ((iRes == ERROR_SUCCESS) && (! gpState->fConnected)) {
        iRes = ERROR_SERVICE_NOT_ACTIVE;
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] IsLinkEncrypted : %d (rare event!) disconnected while in ioctl\n", handle));
    }

    return fEncrypted;
}

static int LockL2CAPConnection
(
BD_ADDR *pba,
int fLock
) {
    // First, get the connection handle
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"LockL2CAPConnection : %04x%08x : %s l2cap connection\n", pba->NAP, pba->SAP, fLock ? L"locking" : L"unlocking"));

    HANDLE hL2CAP = gpState->hL2CAP;
    BT_LAYER_IO_CONTROL pCallbackIOCTL = gpState->l2cap_if.l2ca_ioctl;
    gpState->AddRef ();
    gpState->Unlock ();
    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        int dwData = 0;
        iRes = pCallbackIOCTL (hL2CAP, fLock ? BTH_L2CAP_IOCTL_LOCK_BASEBAND : BTH_L2CAP_IOCTL_UNLOCK_BASEBAND, sizeof(*pba), (char *)pba, 0, NULL, &dwData);
        if ((iRes == ERROR_SUCCESS) && (dwData != 0)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] LockL2CAPConnection : %04x%08x : incorrect return buffer in l2cap_ioctl\n", pba->NAP, pba->SAP));
            iRes = ERROR_INTERNAL_ERROR;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] LockL2CAPConnection : %04x%08x : exception in l2cap_ioctl\n", pba->NAP, pba->SAP));
    }
    gpState->Lock ();
    gpState->DelRef ();

    if ((iRes == ERROR_SUCCESS) && (! gpState->fConnected)) {
        iRes = ERROR_SERVICE_NOT_ACTIVE;
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] LockL2CAPConnection : %04x%08x (rare event!) disconnected while in ioctl\n", pba->NAP, pba->SAP));
    }

    return iRes == ERROR_SUCCESS ? TRUE : FALSE;
}

static int WaitForCommandCompletion (HANDLE hEvent, BD_ADDR *pba, unsigned short h) {
    for ( ; ; ) {
        int iRes = WaitForSingleObject (hEvent, COMMAND_PERIODIC_TIMEOUT);
        if (iRes == WAIT_OBJECT_0)
            return ERROR_SUCCESS;

        if (iRes == WAIT_TIMEOUT) {
            unsigned short h2 = 0;

            gpState->Lock ();
            iRes = GetHandleForBA (pba, &h2);
            gpState->Unlock ();

            if (iRes != ERROR_SUCCESS)
                return iRes;

            if (h2 != h)
                return ERROR_CONNECTION_INVALID;

            continue;
        }

        return ERROR_OPERATION_ABORTED;
    }
}

static BOOL RouteSCOControl (void) {
    BOOL fSCOControl = TRUE;
    HKEY hk;

    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Bluetooth\\AudioGateway", 0, 0, &hk)) {
        DWORD dwData;
        DWORD cbAudioRoute = sizeof(DWORD);
        if ((ERROR_SUCCESS == RegQueryValueEx(hk, _T("MapAudioToPcmMode"), 0, NULL, (PBYTE)&dwData, &cbAudioRoute)) && (0 == dwData)) {
            fSCOControl = FALSE;
        }
    }

    return fSCOControl;
}


//
//  Callbacks
//
static int btshell_CallAborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: SCall aborted for call 0x%08x error %d\n", pCallContext, iError));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = iError;
    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_CommonCallbackHandler (void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_CommonCallbackHandler call=0x%08X status=0x%08X\n", pCallContext, status));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: btshell_CommonCallbackHandler: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: btshell_CommonCallbackHandler: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    SetEvent (pCall->hEvent);

    gpState->Unlock ();
   
    return ERROR_SUCCESS;
}

static int btshell_CommonCallbackAbortHandler (void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_CommonCallbackAbortHandler call=0x%08X status=0x%08X\n", pCallContext, status));

    if (status != 0)
        return btshell_CallAborted (pCallContext, StatusToError (status, ERROR_INTERNAL_ERROR));

    return ERROR_SUCCESS;
}

static int btshell_BDADDR_Out (void *pCallContext, unsigned char status, BD_ADDR *pba) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: BD READ out status %d, address %04x%08x\n", status, pba->NAP, pba->SAP));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    memcpy (&pCall->baResult, pba, sizeof(pCall->baResult));

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_AuthenticationRequested_out (void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: encryption/authentication status %d\n", status));

    if (status != 0) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Auth request not accepted for call 0x%08x status %d\n", pCallContext, status));

        if (! gpState)
            return ERROR_SERVICE_NOT_ACTIVE;

        gpState->Lock ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        SCall *pCall = VerifyCall ((SCall *)pCallContext);
        if (! pCall) {
            gpState->Unlock ();
            return ERROR_NOT_FOUND;
        }

        pCall->fComplete = TRUE;
        pCall->iResult   = StatusToError (status, ERROR_INTERNAL_ERROR);

        if ((status == 0x0c) || (status == 0x22) || (status == 0x23))
            pCall->fRetry    = TRUE;

        SetEvent (pCall->hEvent);

        gpState->Unlock ();
    }

    return ERROR_SUCCESS;
}

static int btshell_ReadLocalVersionInformation_Out (void *pCallContext, unsigned char status, unsigned char hci_version,
                                unsigned short hci_revision, unsigned char lmp_version, unsigned short manufacturer_name,
                                unsigned short lmp_subversion) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLocalVersionInformation_Out: status %d\n", status));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalVersionInformation_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalVersionInformation_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalVersionInformation_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_LOCAL_VERSION);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0) {
        VersionInformation *pvi = &pCall->viResult;
        pvi->hci_revision = hci_revision;
        pvi->hci_version = hci_version;
        pvi->lmp_subversion = lmp_subversion;
        pvi->lmp_version = lmp_version;
        pvi->manufacturer_name = manufacturer_name;
    }

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLocalVersionInformation_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadLocalSupportedFeatures_Out (void *pCallContext, unsigned char status, unsigned char features_mask[8]) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLocalSupportedFeatures_Out: status %d\n", status));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalSupportedFeatures_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalSupportedFeatures_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLocalSupportedFeatures_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_LOCAL_LMP);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        memcpy (pCall->ftmResult, features_mask, 8);

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLocalSupportedFeatures_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadScan_Out (void *pCallContext, unsigned char status, unsigned char mask) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadScan_Out: status %d, mask 0x%02x\n", status, mask));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScan_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScan_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScan_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_SCAN_ENABLE);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        pCall->ucResult = mask;

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadScan_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadPageTimeout_Out (void *pCallContext, unsigned char status, unsigned short page) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadPageTimeout_Out: status %d, page %d\n", status, page));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadPageTimeout_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadPageTimeout_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadPageTimeout_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_PAGE_TIMEOUT);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        pCall->usResult = page;

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadPageTimeout_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadClassOfDevice_Out (void *pCallContext, unsigned char status, unsigned int cod) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadClassOfDevice_Out: status %d, cod 0x%06x\n", status, cod));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadClassOfDevice_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadClassOfDevice_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadClassOfDevice_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_CLASS_OF_DEVICE);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        pCall->dwResult = cod;

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadClassOfDevice_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadAuthenticationEnable_Out (void *pCallContext, unsigned char status, unsigned char authnetication_enable) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadAuthenticationEnable_Out: status %d, authnetication_enable 0x%02x\n", status, authnetication_enable));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadAuthenticationEnable_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadAuthenticationEnable_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadAuthenticationEnable_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_AUTHN_ENABLE);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        pCall->ucResult = authnetication_enable;

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadAuthenticationEnable_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_WriteLinkPolicySettings_Out (void *pCallContext, unsigned char status, unsigned short connection_handle) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_WriteLinkPolicySettings_Out: status %d connection_handle 0x%04x\n", status, connection_handle));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_WriteLinkPolicySettings_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_WriteLinkPolicySettings_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_WriteLinkPolicySettings_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_WRITE_LINK_POLICY);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_WriteLinkPolicySettings_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadLinkPolicySettings_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned short link_policy_settings) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLinkPolicySettings_Out: status %d, connection_handle 0x%04x link_policy_settings 0x%04x\n", status, connection_handle, link_policy_settings));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLinkPolicySettings_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLinkPolicySettings_Out: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadLinkPolicySettings_Out: ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_LINK_POLICY);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0)
        pCall->usResult = link_policy_settings;

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadLinkPolicySettings_Out: ERROR_SUCCESS\n"));
    gpState->Unlock ();

    return ERROR_SUCCESS;
}
          

static int btshell_CommonAcceptConnectionRequest_Out (void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_CommonAcceptConnectionRequest_Out: status %d\n", status));

    if (status != 0) {
        if (! gpState)
            return ERROR_SERVICE_NOT_ACTIVE;

        gpState->Lock ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        SCall *pCall = VerifyCall ((SCall *)pCallContext);
        if (! pCall) {
            gpState->Unlock ();
            return ERROR_NOT_FOUND;
        }

        DeleteCall(pCall);

        gpState->Unlock ();
    }

    return ERROR_SUCCESS;
}

static int btshell_InquiryComplete (void *pUserContext, void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Inquiry complete status %d\n", status));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_InquiryResult (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned char page_scan_repetition_mode, unsigned char page_scan_period_mode, unsigned char page_scan_mode, unsigned int class_of_device, unsigned short clock_offset) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Inquiry result %04x%08x rep %d period %d mode %d cod 0x%06x offset 0x%04x\n", pba->NAP, pba->SAP, page_scan_repetition_mode, page_scan_period_mode, page_scan_mode, class_of_device, clock_offset));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    InquiryBuffer *pBuff = (InquiryBuffer *)pCall->ptrResult.pBuffer;
    if (pBuff->cb < SVSUTIL_ARRLEN(pBuff->ba)) {
        pBuff->cod[pBuff->cb]  = class_of_device;
        pBuff->ba[pBuff->cb]   = SET_NAP_SAP(pba->NAP, pba->SAP);;
        pBuff->psm[pBuff->cb]  = page_scan_mode;
        pBuff->pspm[pBuff->cb] = page_scan_period_mode;
        pBuff->psrm[pBuff->cb] = page_scan_repetition_mode;
        pBuff->co[pBuff->cb]   = clock_offset;
        pBuff->cb++;
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_RemoteNameDone (void *pUserContext, void *pCallContext, unsigned char status, BD_ADDR *pba, unsigned char utf_name[BD_MAXNAME]) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: RemoteNameDone status %d addr %04x%08x\n", status, pba->NAP, pba->SAP));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    if (status == 0) {
        DWORD dwCurrentPerms = pCall->ptrResult.dwPerms ? SetProcPermissions (pCall->ptrResult.dwPerms) : 0;

        BOOL bKMode = FALSE;

        if (pCall->ptrResult.bKMode)
            bKMode = SetKMode (TRUE);

        StringCchCopyNA((char *)pCall->ptrResult.pBuffer, pCall->ptrResult.cbBuffer, (char *)utf_name, BD_MAXNAME);

        if (pCall->ptrResult.bKMode)
            SetKMode (bKMode);

        if (dwCurrentPerms)
            SetProcPermissions (dwCurrentPerms);
    }

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}


static int btshell_ReadRemoteSupportedFeaturesCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char lmp_features[8]) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadRemoteSupportedFeaturesCompleteEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteSupportedFeaturesCompleteEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteSupportedFeaturesCompleteEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteSupportedFeaturesCompleteEvent ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    if (status == 0)
        memcpy (pCall->ftmResult, lmp_features, 8);

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadRemoteVersionInformationCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char lmp_version, unsigned short manufacturers_name, unsigned short lmp_subversion) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadRemoteVersionInformationCompleteEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteVersionInformationCompleteEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteVersionInformationCompleteEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRemoteVersionInformationCompleteEvent ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    if (status == 0) {
        VersionInformation *pvi = &pCall->viResult;
        pvi->lmp_subversion = lmp_subversion;
        pvi->lmp_version = lmp_version;
        pvi->manufacturer_name = manufacturers_name;
    }

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ModeChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char current_mode, unsigned short interval) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ModeChangeEvent status %d connection 0x%04x mode %d interval 0x%04x\n", status, connection_handle, current_mode, interval));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ModeChangeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ModeChangeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ModeChangeEvent ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT ((pCall->fWhat == CALL_HCI_ENTER_HOLD_MODE) || (pCall->fWhat == CALL_HCI_ENTER_SNIFF_MODE) ||
        (pCall->fWhat == CALL_HCI_EXIT_SNIFF_MODE) || (pCall->fWhat == CALL_HCI_ENTER_PARK_MODE) ||
        (pCall->fWhat == CALL_HCI_EXIT_PARK_MODE));

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    if (status == 0) {
        if ((pCall->fWhat == CALL_HCI_ENTER_HOLD_MODE) || (pCall->fWhat == CALL_HCI_ENTER_SNIFF_MODE) ||
                                            (pCall->fWhat == CALL_HCI_ENTER_PARK_MODE))
            pCall->usResult = interval;

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ModeChangeEvent : new mode is %s, requested mode was %s\n",
            (current_mode == 0 ? L"ACTIVE" : (current_mode == 1 ? L"HOLD" : (current_mode == 2 ? L"SNIFF" :
            (current_mode == 3 ? L"PARK" : L"UNKNOWN")))),
            (pCall->fWhat == CALL_HCI_ENTER_HOLD_MODE ? L"HOLD" : (pCall->fWhat == CALL_HCI_ENTER_SNIFF_MODE ? L"SNIFF":
            (pCall->fWhat == CALL_HCI_ENTER_PARK_MODE ? L"PARK" : L"ACTIVE")))));
    }

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_SwitchRole_Out (void *pCallContext, unsigned char status) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_SwitchRole_Out: status %d\n", status));

    if (status != 0)
        return btshell_CallAborted (pCallContext, StatusToError (status, ERROR_INTERNAL_ERROR));

    return ERROR_SUCCESS;
}

static int btshell_RoleChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, BD_ADDR *pba, unsigned char new_role) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_RoleChangeEvent status %d addr %04x%08x role %d\n", status, pba->NAP, pba->SAP, new_role));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleChangeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleChangeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (! pCallContext) {
        // HCI is notifying us but we did not send the Switch_Role request.  Ignoring...
        gpState->Unlock ();
        return ERROR_SUCCESS;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleChangeEvent ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_SWITCH_ROLE);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_CustomCodeEvent (void *pUserContext, void *pCallContext, unsigned char cEventCode, unsigned char cEventLength, unsigned char *pEvent) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_CustomCodeEvent\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_CustomCodeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_CustomCodeEvent ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    // We want to check for a failed command status event since it can mean one
    // of our API calls failed and we want to abort immediately.
    if ((cEventCode == HCI_Command_Status_Event) && (cEventLength == 4)) {
        unsigned char status = pEvent[0];
        if (status) {
            SCall *pCall = VerifyCall ((SCall *)pCallContext);
            if (! pCall) {
                IFDBG(DebugOut (DEBUG_WARN, L"btshell_CustomCodeEvent ERROR_NOT_FOUND\n"));
                gpState->Unlock ();
                return ERROR_NOT_FOUND;
            }
            
            pCall->fComplete = TRUE;
            pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
            SetEvent (pCall->hEvent);            
        }        
    }
    
    gpState->Unlock ();
    
    return ERROR_SUCCESS;
}


static int btshell_RoleDiscovery_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char current_role) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_RoleDiscovery_Out: status %d connection 0x%04x role %d\n", status, connection_handle, current_role));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleDiscovery_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleDiscovery_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_RoleDiscovery_Out ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_GET_ROLE);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0) {
        pCall->ucResult = current_role;
    }

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadRSSI_Out (void *pCallContext, unsigned char status, unsigned short connection_handle, char RSSI) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadRSSI_Out: status %d connection 0x%04x RSSI %d\n", status, connection_handle, RSSI));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRSSI_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRSSI_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadRSSI_Out ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (pCall->fWhat == CALL_HCI_READ_RSSI);

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    if (status == 0) {
        pCall->ucResult = RSSI;
    }

    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadScanActivity_Out (void *pCallContext, unsigned char status, unsigned short scan_interval, unsigned short scan_window) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadScanActivity_Out: status %d\n", status));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanActivity_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanActivity_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanActivity_Out ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT ((pCall->fWhat == CALL_HCI_READ_PAGE_SCAN_ACTIVITY) ||
                    (pCall->fWhat == CALL_HCI_READ_INQUIRY_SCAN_ACTIVITY));
    
    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    
    if (status == 0) {
        pCall->activityResult.usScanInterval = scan_interval;
        pCall->activityResult.usScanWindow = scan_window;
    }
    
    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadPageScanActivity_Out (void *pCallContext, unsigned char status, unsigned short page_scan_interval, unsigned short page_scan_window) {
    return btshell_ReadScanActivity_Out (pCallContext, status, page_scan_interval, page_scan_window);
}

static int btshell_ReadInquiryScanActivity_Out (void *pCallContext, unsigned char status, unsigned short inquiry_scan_interval, unsigned short inquiry_scan_window) {
    return btshell_ReadScanActivity_Out (pCallContext, status, inquiry_scan_interval, inquiry_scan_window);    
}

static int btshell_ReadScanType_Out (void* pCallContext, unsigned char status, unsigned char scan_type) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_ReadScanType_Out: status %d\n", status));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanType_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanType_Out ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_WARN, L"btshell_ReadScanType_Out ERROR_NOT_FOUND\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT ((pCall->fWhat == CALL_HCI_READ_PAGE_SCAN_TYPE) ||
                    (pCall->fWhat == CALL_HCI_READ_INQUIRY_SCAN_TYPE));
    
    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);    
    
    if (status == 0) {
        pCall->ucResult = scan_type;
    }
    
    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ReadPageScanType_Out (void* pCallContext, unsigned char status, unsigned char page_scan_type) {
    return btshell_ReadScanType_Out (pCallContext, status, page_scan_type);
}

static int btshell_ReadInquiryScanType_Out (void* pCallContext, unsigned char status, unsigned char inquiry_scan_type) {
    return btshell_ReadScanType_Out (pCallContext, status, inquiry_scan_type);
}


//
//  Power manager
//
static DWORD WINAPI PowerMonitorEvent (LPVOID lpVoid) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"PowerMonitorEvent +++\n"));

    int cHandles = 0;
    unsigned short usHandles[256];

    int fReschedule = ((ERROR_SUCCESS == BthGetBasebandHandles (sizeof(usHandles)/sizeof(usHandles[0]), usHandles, &cHandles)) && (cHandles > 0));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Completely desynchronized\n"));
        return 0;
    }

    gpState->Lock ();

    if ((! gpState->fNoSuspend) && gfnSystemTimerReset && gpState->fIsRunning) {
        fReschedule = fReschedule || gpState->cntPowerMonitor;

        gpState->ckPowerMonitor = 0;
        gpState->cntPowerMonitor = 0;

        gfnSystemTimerReset ();

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"PowerMonitorEvent : found %d baseband handles; reset system timer\n", cHandles));

        if (fReschedule) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"PowerMonitorEvent : rescheduling in %d\n", BTD_POWER_RECHECK));
            gpState->ckPowerMonitor = btutil_ScheduleEvent (PowerMonitorEvent, NULL, BTD_POWER_RECHECK);
        }
    }

    gpState->Unlock ();

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"PowerMonitorEvent ---\n"));

    return 0;
}

//
//  Security manager section
//
static int btshell_PINCodeRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_PinCodeRequestEvent : addr %04x%08x\n", pba->NAP, pba->SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Completely desynchronized\n"));
        return ERROR_INTERNAL_ERROR;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"btshell_PinCodeRequestEvent: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE h = gpState->hHCI;

    HCI_PINCodeRequestReply_In pCallbackYes = gpState->hci_if.hci_PINCodeRequestReply_In;
    HCI_PINCodeRequestNegativeReply_In pCallbackNo = gpState->hci_if.hci_PINCodeRequestNegativeReply_In;

    Security *pSecurity = gpState->pSecurity;
    while (pSecurity && ((pSecurity->b != *pba) || (pSecurity->csize == 0)))
        pSecurity = pSecurity->pNext;

    unsigned char pin[16];
    int cPinLen = 0;

    if (pSecurity) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_PinCodeRequestEvent addr %04x%08x : found PIN\n", pba->NAP, pba->SAP));
        cPinLen = pSecurity->csize;
        memcpy (pin, pSecurity->ucdata, cPinLen);
    } else {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_PinCodeRequestEvent addr %04x%08x : PIN not found\n", pba->NAP, pba->SAP));
        if (AddNewPINRequest (pba)) {
            gpState->Unlock ();
            return ERROR_SUCCESS;
        }

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_PinCodeRequestEvent addr %04x%08x : out of PIN requests, failing...\n", pba->NAP, pba->SAP));
    }

    gpState->Unlock ();

    __try {
        if (cPinLen)
            pCallbackYes (h, NULL, pba, cPinLen, pin);
        else
            pCallbackNo (h, NULL, pba);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_PINCodeRequest[Negative]Reply\n"));
    }

    return ERROR_SUCCESS;
}

static int btshell_LinkKeyRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_LinkKeyRequestEvent : addr %04x%08x\n", pba->NAP, pba->SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Completely desynchronized\n"));
        return ERROR_INTERNAL_ERROR;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"btshell_LinkKeyRequestEvent: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE h = gpState->hHCI;

    HCI_LinkKeyRequestReply_In pCallbackYes = gpState->hci_if.hci_LinkKeyRequestReply_In;
    HCI_LinkKeyRequestNegativeReply_In pCallbackNo = gpState->hci_if.hci_LinkKeyRequestNegativeReply_In;

    Security *pSecurity = gpState->pSecurity;
    while (pSecurity && ((pSecurity->b != *pba) || (pSecurity->csize != 0)))
        pSecurity = pSecurity->pNext;

    unsigned char key[16];
    int cPinLen = 0;

    if (pSecurity) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_LinkKeyRequestEvent : addr %04x%08x : found key\n", pba->NAP, pba->SAP));
        memcpy (key, pSecurity->ucdata, sizeof(key));
    } else
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_LinkKeyRequestEvent : addr %04x%08x : key not found\n", pba->NAP, pba->SAP));

    gpState->Unlock ();

    __try {
        if (pSecurity)
            pCallbackYes (h, NULL, pba, key);
        else
            pCallbackNo (h, NULL, pba);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_LinkKeyRequest[Negative]Reply\n"));
    }

    return ERROR_SUCCESS;
}

static int btshell_LinkKeyNotificationEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned char link_key[16], unsigned char key_type) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_LinkKeyNotificationEvent : addr %04x%08x\n", pba->NAP, pba->SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Completely desynchronized\n"));
        return ERROR_INTERNAL_ERROR;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"btshell_LinkKeyNotificationEvent: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;

    while (pSecurity && ((pSecurity->b != *pba) || (pSecurity->csize != 0)))
        pSecurity = pSecurity->pNext;

    if (! pSecurity) {
        pSecurity = (Security *)svsutil_GetFixed (gpState->pfmdSecurity);
        if (pSecurity) {
            pSecurity->pNext = gpState->pSecurity;
            gpState->pSecurity = pSecurity;
        }
    }

    if (pSecurity) {
        pSecurity->b = *pba;
        pSecurity->csize = 0;
        memcpy (pSecurity->ucdata, link_key, 16);

#if defined (PERSIST_LINK_KEYS)
        PersistLinkKey (pba, link_key);
#endif
        SetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);
        ResetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);

        BTEVENT bte;
        memset (&bte, 0, sizeof(BTEVENT));
        bte.dwEventId = BTE_KEY_NOTIFY;

        BT_LINK_KEY_EVENT *plke = (BT_LINK_KEY_EVENT*) bte.baEventData;
        plke->dwSize = sizeof(BT_LINK_KEY_EVENT);
        plke->bta = SET_NAP_SAP (pba->NAP, pba->SAP);
        plke->key_type = key_type;
        memcpy (plke->link_key, link_key, 16);

        NotifyEvent (&bte, BTE_CLASS_PAIRING);

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"btshell_LinkKeyNotificationEvent : %04x%08x key successfully set\n", pba->NAP, pba->SAP));
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] btshell_LinkKeyNotificationEvent : %04x%08x out of memory\n", pba->NAP, pba->SAP));

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

static int btshell_AuthenticationCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: AuthenticationCompleteEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: AuthenticationCompleteEvent returns ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: AuthenticationCompleteEvent returns ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: AuthenticationCompleteEvent : call not originated here (ERROR_NOT_FOUND)\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: AuthenticationCompleteEvent : call successfully completed\n"));

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_EncryptionChangeEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char encryption_enable) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: EncryptionChangeEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: EncryptionChangeEvent returns ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"Shell: EncryptionChangeEvent returns ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: EncryptionChangeEvent : call not originated here (ERROR_NOT_FOUND)\n"));
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);

    SetEvent (pCall->hEvent);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: EncryptionChangeEvent : call successfully completed\n"));

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ConnectionCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, BD_ADDR *pba, unsigned char link_type, unsigned char encryption_mode) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_ConnectionCompleteEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    if (status == 0) {
        pCall->usResult = connection_handle;

        Link *pLink = (Link*)svsutil_GetFixed (gpState->pfmdLinks);
        if (pLink) {
            pLink->b = *pba;
            pLink->h = connection_handle;
            pLink->type = link_type;
            pLink->hProcOwner = pCall->hProcOwner;
            pLink->pNext = gpState->pLinks;
            gpState->pLinks = pLink;
        }
    }

    if (pCall->fWhat == CALL_HCI_SCO_ACCEPT_REJECT) {
        // Call to accept/reject does not wait
        DeleteCall (pCall);
    }
    else {
        pCall->fComplete = TRUE;
        pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
        SetEvent (pCall->hEvent);
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_SynchronousConnectionCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, 
    BD_ADDR *pba, unsigned char link_type, unsigned char transmission_interval, unsigned char retransmit_window, unsigned short rx_packet_length, 
    unsigned short tx_packet_length, unsigned char air_mode) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_SynchronousConnectionCompleteEvent status %d connection 0x%04x\n", status, connection_handle));
    
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    if (status == 0) {
        pCall->usResult = connection_handle;

        Link *pLink = (Link*)svsutil_GetFixed (gpState->pfmdLinks);
        if (pLink) {
            pLink->b = *pba;
            pLink->h = connection_handle;
            pLink->type = link_type;
            pLink->hProcOwner = pCall->hProcOwner;
            pLink->pNext = gpState->pLinks;
            gpState->pLinks = pLink;
        }
    }

    if (pCall->fWhat == CALL_HCI_SCO_ACCEPT_REJECT) {
        // Call to accept/reject does not wait
        DeleteCall (pCall);
    }
    else {
        pCall->fComplete = TRUE;
        pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
        SetEvent (pCall->hEvent);
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_DisconnectionCompleteEvent (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char reason) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_ConnectionCompleteEvent status %d connection 0x%04x\n", status, connection_handle));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (status == 0) {
        Link *pLink = gpState->pLinks;
        Link *pParent = NULL;

        while (pLink && (pLink->h != connection_handle)) {
            pParent = pLink;
            pLink = pLink->pNext;
        }

        if (pLink) {
            if (pParent)
                pParent->pNext = pLink->pNext;
            else
                gpState->pLinks = pLink->pNext;

            svsutil_FreeFixed (pLink, gpState->pfmdLinks);
        }
    }

    SCall *pCall = VerifyCall ((SCall *)pCallContext);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    pCall->fComplete = TRUE;
    pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
    SetEvent (pCall->hEvent);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

static int btshell_ConnectionRequestEvent (void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned int class_of_device, unsigned char link_type) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: btshell_ConnectionRequestEvent addr %04x%08x connection 0x%04x COD %d link %d\n", pba->NAP, pba->SAP, class_of_device, link_type));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if ((link_type != BTH_LINK_TYPE_SCO) && (link_type != BTH_LINK_TYPE_ESCO)) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_ERROR, L"btshell_ConnectionRequestEvent : No action for non-[e]SCO link type.\n"));
        return ERROR_PROTOCOL_UNREACHABLE;
    }

    int iRes = ERROR_SUCCESS;

    SCall *pCall = AllocCall (CALL_HCI_SCO_ACCEPT_REJECT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;
    unsigned char reject_reason = 0;    

    iRes = ERROR_INTERNAL_ERROR;

    if (((link_type == BTH_LINK_TYPE_ESCO) && (gpState->cntAcceptSync == 0)) ||
        ((link_type == BTH_LINK_TYPE_SCO) && ((gpState->cntAcceptSCO == 0) && (gpState->cntAcceptSync == 0)))) {
        reject_reason = BT_ERROR_UNSUPPORTED_FEATURE_OR_PARAMETER;            
    }
        
    if (reject_reason == 0) {        
        HCI_AcceptConnectionRequest_In pCallback = gpState->hci_if.hci_AcceptConnectionRequest_In;
        HCI_AcceptSynchronousConnectionRequest_In pCallbackSync = gpState->hci_if.hci_AcceptSynchronousConnectionRequest_In;

        gpState->AddRef ();
        gpState->Unlock ();

        __try {
            if (link_type == BTH_LINK_TYPE_SCO) {
                iRes = pCallback (hHCI, pCall, pba, 0);
            } else {
                unsigned int txBandwidth = 0xFFFFFFFF;  // don't care
                unsigned int rxBandwidth = 0xFFFFFFFF;  // don't care
                unsigned short maxLatency = 0xFFFF;     // don't care
                unsigned char retransmission = 0xFF;    // don't care
                
                iRes = pCallbackSync (hHCI, pCall, pba, txBandwidth, rxBandwidth, maxLatency, gpState->ESCOVoiceSetting, retransmission, gpState->ESCOPacketType);                    
            }
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_Accept[Synchronous]ConnectionRequest_In\n"));
        }

        gpState->Lock ();
        gpState->DelRef ();
    } else {
        HCI_RejectConnectionRequest_In pCallback = gpState->hci_if.hci_RejectConnectionRequest_In;
        HCI_RejectSynchronousConnectionRequest_In pCallbackSync = gpState->hci_if.hci_RejectSynchronousConnectionRequest_In;

        gpState->AddRef ();
        gpState->Unlock ();

        __try {
            if (link_type == BTH_LINK_TYPE_SCO) {
                iRes = pCallback (hHCI, pCall, pba, reject_reason);
            } else {
                iRes = pCallbackSync (hHCI, pCall, pba, reject_reason);
            }
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_Reject[Synchronous]ConnectionRequest_In\n"));
        }

        gpState->Lock ();
        gpState->DelRef ();
    }
        
    if (iRes) {
        if (pCall != VerifyCall (pCall)) {
            IFDBG(DebugOut (DEBUG_WARN, L"btshell_ConnectionRequestEvent : ERROR_INTERNAL_ERROR\n", iRes));
            gpState->Unlock ();
            return ERROR_INTERNAL_ERROR;
        }

        DeleteCall (pCall);
    }

    gpState->Unlock ();

    return iRes;
}

static int InterestingHciEvent (unsigned char *p, int c) {
    if ((p[0] == HCI_Role_Change_Event) ||
        (p[0] == HCI_Mode_Change_Event))
        return BTD_EVENT_BASEBAND_CHANGED;

    if ((p[0] == HCI_Connection_Complete_Event) ||
        (p[0] == HCI_Disconnection_Complete_Event))
        return BTD_EVENT_CONNECTIONS_CHANGED;

    if (p[0] != HCI_Command_Complete_Event)
        return -1;

    unsigned short usOpCode = p[3] | (p[4] << 8);
    switch (usOpCode) {
    case HCI_Write_PIN_Type:
    case HCI_Create_New_Unit_Key:
    case HCI_Write_Stored_Link_Key:
    case HCI_Delete_Stored_Link_Key:
    case HCI_Write_Authentication_Enable:
    case HCI_Write_Encryption_Mode:
        return BTD_EVENT_SECURITY_CHANGED;

    case HCI_Write_Connection_Accept_Timeout:
    case HCI_Write_Page_Timeout:
    case HCI_Write_Hold_Mode_Activity:
    case HCI_Write_Scan_Enable:
    case HCI_Write_Page_Scan_Period_Mode:
    case HCI_Write_Page_Scan_Mode:
    case HCI_Write_PageScan_Activity:
    case HCI_Write_InquiryScan_Activity:
    case HCI_Write_Current_IAC_LAP:
        return BTD_EVENT_CONNECTIVITY_CHANGED;

    case HCI_Change_Local_Name:
    case HCI_Write_Class_Of_Device:
        return BTD_EVENT_DEVICEID_CHANGED;
    }

    return -1;
}

static int btshell_hStackEvent (void *pUserContext, int iEvent, void *pEventContextVoid) {
    HCIEventContext *pEventContext = (HCIEventContext *)pEventContextVoid;

    if ((iEvent == BTH_STACK_HCI_HARDWARE_EVENT) && pEventContext && (pEventContext->Event.cSize > 0)) {
        //
        // Signal named events and do power handling
        //
        int cEvent = InterestingHciEvent (pEventContext->Event.pData, pEventContext->Event.cSize);
        if (cEvent >= 0) {
            SVSUTIL_ASSERT (cEvent < BTD_EVENT_NUMBER);

            if (gpState) {
                gpState->Lock ();

                if (gpState->fIsRunning) {
                    SetEvent (gpState->hNamedEvents[cEvent]);
                    ResetEvent (gpState->hNamedEvents[cEvent]);
                }

                if ((cEvent == BTD_EVENT_CONNECTIONS_CHANGED) && gfnSystemTimerReset && (! gpState->fNoSuspend)) {
                    if (! gpState->ckPowerMonitor)
                        gpState->ckPowerMonitor = btutil_ScheduleEvent (PowerMonitorEvent, NULL, BTD_POWER_DELAY);
                    else
                        ++gpState->cntPowerMonitor;
                }

                gpState->Unlock ();
            }
        }

        //
        // Signal BT notification system
        //
        // Note: Some of the parsing code below is (partially) copied from hci.cxx but
        // there is no better way to do this now since hStackEvent params just contains
        // the whole recv buffer.
        //

        if (gpState) {
            gpState->Lock ();

            // Parse out event, length, and param buffer
            unsigned char ucEvent = pEventContext->Event.pData[0];
            int cLength = pEventContext->Event.pData[1];
            unsigned char *pParms = pEventContext->Event.pData + 2;
            int fLengthFault = (cLength + 2) != pEventContext->Event.cSize;

            if (! fLengthFault) {
                switch (ucEvent) {
                case HCI_Connection_Complete_Event:
                {
                    if ((cLength == 11) && (pParms[0] == 0)) {
                        // Length and status are acceptable, let's notify up...
                        BTEVENT bte;
                        memset (&bte, 0, sizeof(BTEVENT));
                        bte.dwEventId = BTE_CONNECTION;

                        BT_CONNECT_EVENT *ped = (BT_CONNECT_EVENT*) bte.baEventData;
                        ped->dwSize = sizeof(BT_CONNECT_EVENT);
                        ped->hConnection = pParms[1] | (pParms[2] << 8);
                        ped->ucLinkType = pParms[9];
                        ped->ucEncryptMode = pParms[10];

                        BD_ADDR* pba = (BD_ADDR*) (pParms + 3);
                        ped->bta = SET_NAP_SAP (pba->NAP, pba->SAP);

                        NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                    }
                    else if (cLength != 11) {
                        SVSUTIL_ASSERT (0);
                    }
                }
                break;
                
                case HCI_Synchronous_Connection_Complete_Event:
                {
                    if ((cLength == 17) && (pParms[0] == 0)) {
                        // Length and status are acceptable, let's notify up...
                        BTEVENT bte;
                        memset (&bte, 0, sizeof(BTEVENT));
                        bte.dwEventId = BTE_CONNECTION;

                        BT_CONNECT_EVENT *ped = (BT_CONNECT_EVENT*) bte.baEventData;
                        ped->dwSize = sizeof(BT_CONNECT_EVENT);
                        ped->hConnection = pParms[1] | (pParms[2] << 8);
                        ped->ucLinkType = pParms[9];
                        ped->ucEncryptMode = 0;

                        BD_ADDR* pba = (BD_ADDR*) (pParms + 3);
                        ped->bta = SET_NAP_SAP (pba->NAP, pba->SAP);

                        NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                    }
                }
                break;

                case HCI_Disconnection_Complete_Event:
                {
                    if ((cLength == 4) && (pParms[0] == 0)) {
                        // Length and status are acceptable, let's notify up...
                        BTEVENT bte;
                        memset (&bte, 0, sizeof(BTEVENT));
                        bte.dwEventId = BTE_DISCONNECTION;

                        BT_DISCONNECT_EVENT *ped = (BT_DISCONNECT_EVENT*) bte.baEventData;
                        ped->dwSize = sizeof(BT_DISCONNECT_EVENT);
                        ped->hConnection = pParms[1] | (pParms[2] << 8);
                        ped->ucReason = pParms[3];

                        NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                    }
                    else if (cLength != 4) {
                        SVSUTIL_ASSERT (0);
                    }
                }
                break;
                
                case HCI_Mode_Change_Event:
                {
                    if ((cLength == 6) && (pParms[0] == 0)) {
                        // Length and status are acceptable, let's notify up...
                        BTEVENT bte;
                        memset (&bte, 0, sizeof(BTEVENT));
                        bte.dwEventId = BTE_MODE_CHANGE;

                        BT_MODE_CHANGE_EVENT *ped = (BT_MODE_CHANGE_EVENT*) bte.baEventData;
                        ped->dwSize = sizeof(BT_MODE_CHANGE_EVENT);
                        ped->hConnection = pParms[1] | (pParms[2] << 8);
                        ped->bMode = pParms[3];
                        ped->usInterval = pParms[4] | (pParms[5] << 8);

                        BthGetAddress (ped->hConnection, &ped->bta);

                        NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                    }
                    else if (cLength != 6) {
                        SVSUTIL_ASSERT (0);
                    }
                }
                break;

                case HCI_Role_Change_Event:
                {
                    if ((cLength == 8) && (pParms[0] == 0)) {
                        // Length and status are acceptable, let's notify up...
                        BTEVENT bte;
                        memset (&bte, 0, sizeof(BTEVENT));
                        bte.dwEventId = BTE_ROLE_SWITCH;

                        BT_ROLE_SWITCH_EVENT *ped = (BT_ROLE_SWITCH_EVENT*) bte.baEventData;
                        ped->dwSize = sizeof(BT_ROLE_SWITCH_EVENT);
                        ped->fRole = pParms[7];

                        BD_ADDR* pba = (BD_ADDR*) (pParms + 1);
                        ped->bta = SET_NAP_SAP (pba->NAP, pba->SAP);

                        NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                    }
                    else if (cLength != 8) {
                        SVSUTIL_ASSERT (0);
                    }
                }
                break;

                case HCI_Command_Complete_Event:
                {
                    if (cLength >= 3) {
                        unsigned short usOpCode = pParms[1] | (pParms[2] << 8);

                        switch (usOpCode) {
                            case HCI_Change_Local_Name:
                            case HCI_Write_Class_Of_Device: {
                                BTEVENT bte;
                                memset (&bte, 0, sizeof(BTEVENT));
                                bte.dwEventId = (usOpCode == HCI_Change_Local_Name) ? BTE_LOCAL_NAME : BTE_COD;

                                NotifyEvent (&bte, BTE_CLASS_DEVICE);
                            }
                            break;

                            case HCI_Write_Page_Timeout: {
                                BTEVENT bte;
                                memset (&bte, 0, sizeof(BTEVENT));
                                bte.dwEventId = BTE_PAGE_TIMEOUT;

                                NotifyEvent (&bte, BTE_CLASS_CONNECTIONS);
                            }
                            break;
                        }
                    }
                    else if (cLength < 3) {
                        SVSUTIL_ASSERT (0);
                    }
                }
                break;
                }
            }

            gpState->Unlock ();
        }
    } else {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Stack event (HCI) %d -- ignored\n", iEvent));
    }

    return ERROR_SUCCESS;
}

static int btshell_lStackEvent (void *pUserContext, int iEvent, void *pEventContext) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: Stack event (L2CAP) %d\n", iEvent));

    if (iEvent == BTH_STACK_DISCONNECT)
        btutil_ScheduleEvent (StackDisconnect, NULL);
    else if (iEvent == BTH_STACK_DOWN)
        btutil_ScheduleEvent (StackDown, NULL);
    else if (iEvent == BTH_STACK_UP)
        btutil_ScheduleEvent (StackUp, NULL);
    else if (iEvent == BTH_STACK_L2CAP_DROP_COMPLETE) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Shell: BTH_STACK_L2CAP_DROP_COMPLETE\n"));

        if (! gpState)
            return ERROR_SUCCESS;

        gpState->Lock ();

        if (! gpState->fIsRunning) {
            gpState->Unlock ();
            return ERROR_SUCCESS;
        }

        SCall *pCall = VerifyCall ((SCall *)pEventContext);

        if (pCall) {
            pCall->iResult = ERROR_SUCCESS;
            pCall->fComplete = TRUE;
            SetEvent (pCall->hEvent);
        } else
            IFDBG(DebugOut (DEBUG_ERROR, L"Shell: BTH_STACK_L2CAP_DROP_COMPLETE : ghost call context\n"));

        gpState->Unlock ();
    }

    return ERROR_SUCCESS;
}

//
//  Init stuff
//
static int GetBD (WCHAR *pString, BD_ADDR *pba) {
    pba->NAP = 0;

    for (int i = 0 ; i < 4 ; ++i, ++pString) {
        if (! iswxdigit (*pString))
            return FALSE;

        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->NAP = pba->NAP * 16 + c;
    }

    pba->SAP = 0;

    for (i = 0 ; i < 8 ; ++i, ++pString) {
        if (! iswxdigit (*pString))
            return FALSE;

        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->SAP = pba->SAP * 16 + c;
    }

    if (*pString != '\0')
        return FALSE;

    return TRUE;
}

static int btshell_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"+btshell_CreateDriverInstance\n"));

    if (gpState) {
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CreateDriverInstance : ERROR_ALREADY_INITIALIZED\n"));
        return ERROR_ALREADY_INITIALIZED;
    }

    gpState = CreateNewState ();

    if ((! gpState) || (! gpState->fIsRunning)) {
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CreateDriverInstance : ERROR_OUTOFMEMORY\n"));
        if (gpState)
            delete gpState;
        gpState = NULL;
        return ERROR_OUTOFMEMORY;
    }

#if defined (PERSIST_LINK_KEYS)
    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, L"comm\\security\\bluetooth", 0, KEY_READ, &hk)) {
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"btshell_CreateDriverInstance : initializing keys database...\n"));
        DWORD cbMaxValLen = 0;

        unsigned char* pLinkOrPin = NULL;
        if (ERROR_SUCCESS == RegQueryInfoKey (hk, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &cbMaxValLen, NULL, NULL)) {
            pLinkOrPin = new BYTE[cbMaxValLen];
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"btshell_CreateDriverInstance : error querying reg key info for keys.\n"));
        }
        
        if (pLinkOrPin) {
            DWORD dwIndex = 0;
            for ( ; ; ) {
                WCHAR szValName[3 + 4 + 8 + 1];
                DWORD cValName = SVSUTIL_ARRLEN(szValName);
                DWORD cLinkOrPin = cbMaxValLen;
                DWORD dwValType = 0;

                int iRes = RegEnumValue (hk, dwIndex, szValName, &cValName, NULL, &dwValType, pLinkOrPin, &cLinkOrPin);
                if (iRes == ERROR_NO_MORE_ITEMS)
                    break;

                DATA_BLOB dataIn;
                DATA_BLOB dataOut = {0,NULL};
                
                dataIn.cbData = cLinkOrPin;
                dataIn.pbData = pLinkOrPin;

                if (! CryptUnprotectData (&dataIn, NULL, NULL, NULL, NULL, CRYPTPROTECT_SYSTEM, &dataOut)) {
                    //
                    // We should never be here unless data is corrupted or master key on the system was
                    // not persisted, etc.                            
                    //                            
                    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"btshell_CreateDriverInstance : Error decrypting key or pin... index=%d\n", dwIndex));
                    SVSUTIL_ASSERT (0);
                    ++dwIndex;
                    continue; // Keep trying to decrypt keys and pins
                }
                
                BD_ADDR b;
                int fLink = FALSE;
                if ((iRes == ERROR_SUCCESS) && (dwValType == REG_BINARY) && (cValName > 3) && GetBD(szValName + 3, &b) &&
                    (fLink = (wcsnicmp (szValName, L"key", 3) == 0))
#if defined (PERSIST_PINS)
                    || ((cLinkOrPin > 0) && (wcsnicmp (szValName, L"pin", 3) == 0))
#endif
                    ) {
                    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"btshell_CreateDriverInstance : initializing keys database : found key or pin for %04x%08x at index %d...\n", b.NAP, b.SAP, dwIndex));
                    Security *pNew = (Security *)svsutil_GetFixed (gpState->pfmdSecurity);
                    if (pNew) {
                        // Insert into list
                        pNew->pNext = gpState->pSecurity;
                        gpState->pSecurity = pNew;

                        // Copy security properties
                        pNew->b = b;
                        pNew->csize = fLink ? 0 : dataOut.cbData;
                        memcpy (pNew->ucdata, dataOut.pbData, 16);
                    } else {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[shell] btshell_CreateDriverInstance : initializing keys database : OOM @ index %d...\n", dwIndex));
                    }
                } else {
                    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"btshell_CreateDriverInstance : initializing keys database : skipping incorrect data at index %d...\n", dwIndex));
                }

                LocalFree (dataOut.pbData);
                
                ++dwIndex;
            }

            delete[] pLinkOrPin;
        }

        RegCloseKey (hk);
    } else
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"btshell_CreateDriverInstance : no link keys/pins found...\n"));
#endif

    SetEvent (ghBthApiEvent);

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CreateDriverInstance : ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}


static int btshell_CloseDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"+btshell_CloseDriverInstance\n"));

    ResetEvent (ghBthApiEvent);

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CloseDriverInstance : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CloseDriverInstance : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->fIsRunning = FALSE;

    while (gpState->GetRefCount () > 1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Waiting for ref count in btshell_CloseDriverInstance\n"));
        gpState->Unlock ();
        Sleep (200);
        gpState->Lock ();
    }

    while (gpState->pCalls) {
        SetEvent (gpState->pCalls->hEvent);
        gpState->pCalls->iResult = ERROR_CANCELLED;
        gpState->pCalls = gpState->pCalls->pNext;
    }

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"-btshell_CloseDriverInstance : ERROR_SUCCESS\n"));

    gpState->Unlock ();
    delete gpState;
    gpState = NULL;

    return ERROR_SUCCESS;
}

int avrcp_CreateDriverInstance (void);
int avrcp_CloseDriverInstance (void);

static int StartupBTServices(int fHardwareOnly) {
    if (! fHardwareOnly) {
        int iRes = bth_CreateDriverInstance ();
        if (iRes != ERROR_SUCCESS)
            return iRes;

        iRes = bthns_CreateDriverInstance();
        if (iRes != ERROR_SUCCESS)
            return iRes;

        iRes = btshell_CreateDriverInstance();
        if (iRes != ERROR_SUCCESS)
            return iRes;

        iRes = pan_CreateDriverInstance();
        if (iRes != ERROR_SUCCESS)
            return iRes;

        return avrcp_CreateDriverInstance();
    }

    return HCI_StartHardware () ? ERROR_SUCCESS : ERROR_INTERNAL_ERROR;
}

static int ShutdownBTServices(int fHardwareOnly) {
    if (! fHardwareOnly) {
        avrcp_CloseDriverInstance ();
        btshell_CloseDriverInstance ();
        bthns_CloseDriverInstance ();
        bth_CloseDriverInstance ();

        return ERROR_SUCCESS;
    }

    return HCI_StopHardware () ? ERROR_SUCCESS : ERROR_INTERNAL_ERROR;
}

//
//  Main API section
//
int BthWriteScanEnableMask
(
unsigned char   mask
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanEnableMask : %02x\n", mask));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanEnableMask : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanEnableMask : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_WRITE_SCAN_ENABLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_WriteScanEnable_In pCallback = gpState->hci_if.hci_WriteScanEnable_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, mask);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanEnableMask : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanEnableMask : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanEnableMask : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthReadScanEnableMask
(
unsigned char   *pmask
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanEnableMask\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanEnableMask : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanEnableMask : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_SCAN_ENABLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadScanEnable_In pCallback = gpState->hci_if.hci_ReadScanEnable_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanEnableMask : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanEnableMask : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pmask = pCall->ucResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanEnableMask : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthWriteAuthenticationEnable
(
unsigned char   ae
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteAuthenticationEnable : %02x\n", ae));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteAuthenticationEnable : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteAuthenticationEnable : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_WRITE_AUTHN_ENABLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_WriteAuthenticationEnable_In pCallback = gpState->hci_if.hci_WriteAuthenticationEnable_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, ae);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteAuthenticationEnable : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteAuthenticationEnable : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteAuthenticationEnable : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthReadAuthenticationEnable
(
unsigned char   *pae
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadAuthenticationEnable\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadAuthenticationEnable : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadAuthenticationEnable : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_AUTHN_ENABLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadAuthenticationEnable_In pCallback = gpState->hci_if.hci_ReadAuthenticationEnable_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadAuthenticationEnable : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadAuthenticationEnable : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pae = pCall->ucResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadAuthenticationEnable : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthGetCurrentMode
(
BT_ADDR         *pbt,
unsigned char   *pmode
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetCurrentMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetCurrentMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetCurrentMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetCurrentMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetCurrentMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    gpState->Unlock ();

    iRes = ERROR_INTERNAL_ERROR;

    __try {
        unsigned char cMode;
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_HANDLE_MODE, sizeof(h), (char *)&h,
                        sizeof(cMode), (char *)&cMode, &cSizeRet);

        if (iRes == ERROR_SUCCESS) {
            iRes = ERROR_INTERNAL_ERROR;
            if (cSizeRet == sizeof(cMode)) {
                *pmode = cMode;
            iRes = ERROR_SUCCESS;
            }
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetCurrentMode : exception!\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetCurrentMode : result = %d\n", iRes));
    return iRes;
}


int BthGetBasebandHandles
(
int             cHandles,
unsigned short  *pHandles,
int             *pcHandlesReturned
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetBasebandHandles\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetBasebandHandles : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (! pHandles) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetBasebandHandles : ERROR_INVALID_PARAMTER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetBasebandHandles : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_BASEBAND_HANDLES, 0, NULL,
                        sizeof(unsigned short) * cHandles, (char *)pHandles, &cSizeRet);

        *pcHandlesReturned = cSizeRet / sizeof(unsigned short);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetBasebandHandles : exception!\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetBasebandHandles : result = %d\n", iRes));
    return iRes;
}

int BthGetBasebandConnections
(
int                         cConnections,
BASEBAND_CONNECTION_DATA    *pConnections,
int                         *pcConnectionsReturned
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetBasebandConnections\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetBasebandConnections : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (! pConnections) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetBasebandConnections : ERROR_INVALID_PARAMTER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetBasebandConnections : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_BASEBAND_CONNECTIONS, 0, NULL,
                        sizeof(BASEBAND_CONNECTION_DATA) * cConnections, (char *)pConnections, &cSizeRet);

        *pcConnectionsReturned = cSizeRet / sizeof(BASEBAND_CONNECTION_DATA);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetBasebandConnections : exception in hci_ioctl BTH_HCI_IOCTL_GET_BASEBAND_CONNECTIONS\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetBasebandConnections : result = %d\n", iRes));
    return iRes;
}

int BthGetQueuedHCIPacketCount
(
BT_ADDR* pbt,
int* pPacketCount
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetQueuedHCIPacketCount\n"));

    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthEnterHoldMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetQueuedHCIPacketCount : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetQueuedHCIPacketCount : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);
    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetQueuedHCIPacketCount : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    gpState->Unlock ();

    iRes = ERROR_INTERNAL_ERROR;

    __try {
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_NUM_QUEUED_PACKETS, sizeof(h), (char*)&h, sizeof(int), (char *)pPacketCount, &cSizeRet);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetQueuedHCIPacketCount : exception in hci_ioctl BTH_HCI_IOCTL_GET_NUM_QUEUED_PACKETS\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetQueuedHCIPacketCount : result = %d\n", iRes));
    return iRes;
}

int BthEnterHoldMode
(
BT_ADDR *pbt,
unsigned short  hold_mode_max,
unsigned short  hold_mode_min,
unsigned short  *pinterval
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthEnterHoldMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterHoldMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterHoldMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterHoldMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterHoldMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_ENTER_HOLD_MODE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_HoldMode_In pCallback = gpState->hci_if.hci_HoldMode_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h, hold_mode_max, hold_mode_min);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterHoldMode : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterHoldMode : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pinterval = pCall->usResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterHoldMode : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthEnterSniffMode
(
BT_ADDR *pbt,
unsigned short  sniff_mode_max,
unsigned short  sniff_mode_min,
unsigned short  sniff_attempt,
unsigned short  sniff_timeout,
unsigned short  *pinterval
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthEnterSniffMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterSniffMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterSniffMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterSniffMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterSniffMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_ENTER_SNIFF_MODE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_SniffMode_In pCallback = gpState->hci_if.hci_SniffMode_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h, sniff_mode_max, sniff_mode_min, sniff_attempt, sniff_timeout);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterSniffMode : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterSniffMode : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pinterval = pCall->usResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterSniffMode : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthExitSniffMode
(
BT_ADDR *pbt
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthExitSniffMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitSniffMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitSniffMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitSniffMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitSniffMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_EXIT_SNIFF_MODE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ExitSniffMode_In pCallback = gpState->hci_if.hci_ExitSniffMode_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitSniffMode : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitSniffMode : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitSniffMode : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthEnterParkMode
(
BT_ADDR *pbt,
unsigned short  beacon_max,
unsigned short  beacon_min,
unsigned short  *pinterval
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthEnterParkMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterParkMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterParkMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterParkMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterParkMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_ENTER_PARK_MODE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ParkMode_In pCallback = gpState->hci_if.hci_ParkMode_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h, beacon_max, beacon_min);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterParkMode : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthEnterParkMode : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pinterval = pCall->usResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthEnterParkMode : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthExitParkMode
(
BT_ADDR *pbt
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthExitParkMode : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitParkMode : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitParkMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitParkMode : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitParkMode : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_EXIT_PARK_MODE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ExitParkMode_In pCallback = gpState->hci_if.hci_ExitParkMode_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitParkMode : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthExitParkMode : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthExitParkMode : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthWriteLinkPolicySettings
(
BT_ADDR *pbt,
unsigned short  lps
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthWriteLinkPolicySettings : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteLinkPolicySettings : %04x%08x lps = 0x%04x\n", b.NAP, b.SAP, lps));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteLinkPolicySettings : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteLinkPolicySettings : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteLinkPolicySettings : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_WRITE_LINK_POLICY);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_WriteLinkPolicySettings_In pCallback = gpState->hci_if.hci_WriteLinkPolicySettings_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h, lps);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteLinkPolicySettings : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteLinkPolicySettings : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteLinkPolicySettings : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthReadLinkPolicySettings
(
BT_ADDR         *pbt,
unsigned short  *plps
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthReadLinkPolicySettings : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLinkPolicySettings : %04x%08x\n", b.NAP, b.SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLinkPolicySettings : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLinkPolicySettings : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLinkPolicySettings : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLinkPolicySettings : %04x%08x : connection handle 0x%04x. Requesting version info!\n", pba->NAP, pba->SAP, h));

    SCall *pCall = AllocCall (CALL_HCI_READ_LINK_POLICY);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadLinkPolicySettings_In pCallback = gpState->hci_if.hci_ReadLinkPolicySettings_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLinkPolicySettings : exception in HCI callback\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLinkPolicySettings : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *plps = pCall->usResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLinkPolicySettings : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}
int BthWritePageTimeout
(
unsigned short timeout
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageTimeout : %d\n", timeout));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWritePageTimeout : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWritePageTimeout : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_WRITE_PAGE_TIMEOUT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_WritePageTimeout_In pCallback = gpState->hci_if.hci_WritePageTimeout_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, timeout);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWritePageTimeout : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWritePageTimeout : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageTimeout : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthReadPageTimeout
(
unsigned short *ptimeout
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageTimeout\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadPageTimeout : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadPageTimeout : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_PAGE_TIMEOUT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadPageTimeout_In pCallback = gpState->hci_if.hci_ReadPageTimeout_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadPageTimeout : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadPageTimeout : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *ptimeout = pCall->usResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageTimeout : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthWriteCOD
(
unsigned int cod
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteCOD : %06x\n", cod));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_WRITE_CLASS_OF_DEVICE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_WriteClassOfDevice_In pCallback = gpState->hci_if.hci_WriteClassOfDevice_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, cod);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteCOD : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteCOD : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteCOD : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthReadCOD
(
unsigned int *pcod
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadCOD\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_CLASS_OF_DEVICE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadClassOfDevice_In pCallback = gpState->hci_if.hci_ReadClassOfDevice_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadCOD : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadCOD : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    if (iRes == ERROR_SUCCESS)
        *pcod = pCall->dwResult;

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadCOD : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthGetRemoteCOD
(
BT_ADDR* pbt,
unsigned int* pcod
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteCOD\n"));

    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthReadRemoteCOD : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteCOD : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_REMOTE_COD, sizeof(BD_ADDR), (char *)&b,
                        sizeof(unsigned int), (char *)pcod, &cSizeRet);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthReadRemoteCOD : exception!\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteCOD : result = %d\n", iRes));
    return iRes;
}

int BthReadLocalVersion
(
unsigned char   *phci_version,
unsigned short  *phci_revision,
unsigned char   *plmp_version,
unsigned short  *plmp_subversion,
unsigned short  *pmanufacturer,
unsigned char   *plmp_features
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLocalVersion\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_LOCAL_VERSION);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadLocalVersionInformation_In pCallback = gpState->hci_if.hci_ReadLocalVersionInformation_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    VersionInformation vi;

    if (iRes == ERROR_SUCCESS)
        vi = pCall->viResult;

    DeleteCall (pCall);

    if (iRes == ERROR_SUCCESS) {
        pCall = AllocCall (CALL_HCI_READ_LOCAL_LMP);
        if (! pCall) {
            gpState->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        hEvent = pCall->hEvent;

        HCI_ReadLocalSupportedFeatures_In pCallback2 = gpState->hci_if.hci_ReadLocalSupportedFeatures_In;
        iRes = ERROR_INTERNAL_ERROR;

        gpState->AddRef ();

        gpState->Unlock ();
        __try {
            iRes = pCallback2 (hHCI, pCall);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : excepted in HCI callback!\n"));
        }

        if (iRes == ERROR_SUCCESS)
            WaitForSingleObject (hEvent, 5000);

        gpState->Lock ();
        gpState->DelRef ();

        if (pCall != VerifyCall (pCall)) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthReadLocalVersion : ERROR_INTERNAL_ERROR\n"));
            gpState->Unlock ();
            return ERROR_INTERNAL_ERROR;
        }

        if (iRes == ERROR_SUCCESS) {
            if (pCall->fComplete)
                iRes = pCall->iResult;
            else
                iRes = ERROR_TIMEOUT;
        }

        if (iRes == ERROR_SUCCESS)
            memcpy (plmp_features, pCall->ftmResult, sizeof(pCall->ftmResult));

        DeleteCall (pCall);
    }

    if (iRes == ERROR_SUCCESS) {
        *phci_version = vi.hci_version;
        *phci_revision = vi.hci_revision;
        *plmp_version = vi.lmp_version;
        *plmp_subversion = vi.lmp_subversion;
        *pmanufacturer = vi.manufacturer_name;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadLocalVersion : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthReadRemoteVersion
(
BT_ADDR         *pbt,
unsigned char   *plmp_version,
unsigned short  *plmp_subversion,
unsigned short  *pmanufacturer,
unsigned char   *plmp_features
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthReadRemoteVersion : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteVersion : %04x%08x\n", pba->NAP, pba->SAP));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteVersion : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteVersion : %04x%08x : connection handle 0x%04x. Requesting version info!\n", pba->NAP, pba->SAP, h));

    SCall *pCall = AllocCall (CALL_HCI_READ_REMOTE_VERSION);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_ReadRemoteVersionInformation_In pCallback = gpState->hci_if.hci_ReadRemoteVersionInformation_In;
    iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall, h);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    VersionInformation vi;

    if (iRes == ERROR_SUCCESS)
        vi = pCall->viResult;

    DeleteCall (pCall);

    if (iRes == ERROR_SUCCESS) {
        pCall = AllocCall (CALL_HCI_READ_REMOTE_LMP);
        if (! pCall) {
            gpState->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        hEvent = pCall->hEvent;

        HCI_ReadRemoteSupportedFeatures_In pCallback2 = gpState->hci_if.hci_ReadRemoteSupportedFeatures_In;
        iRes = ERROR_INTERNAL_ERROR;

        gpState->AddRef ();

        gpState->Unlock ();
        __try {
            iRes = pCallback2 (hHCI, pCall, h);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : excepted in HCI callback!\n"));
        }

        if (iRes == ERROR_SUCCESS)
            WaitForSingleObject (hEvent, 5000);

        gpState->Lock ();
        gpState->DelRef ();

        if (pCall != VerifyCall (pCall)) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthReadRemoteVersion : ERROR_INTERNAL_ERROR\n"));
            gpState->Unlock ();
            return ERROR_INTERNAL_ERROR;
        }

        if (iRes == ERROR_SUCCESS) {
            if (pCall->fComplete)
                iRes = pCall->iResult;
            else
                iRes = ERROR_TIMEOUT;
        }

        if (iRes == ERROR_SUCCESS)
            memcpy (plmp_features, pCall->ftmResult, sizeof(pCall->ftmResult));

        DeleteCall (pCall);
    }

    if (iRes == ERROR_SUCCESS) {
        *plmp_version = vi.lmp_version;
        *plmp_subversion = vi.lmp_subversion;
        *pmanufacturer = vi.manufacturer_name;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRemoteVersion : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthCancelInquiry
(
void
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCancelInquiry\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCancelInquiry : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCancelInquiry : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_CANCEL_INQUIRY);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_InquiryCancel_In pCallback = gpState->hci_if.hci_InquiryCancel_In;
    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        iRes = pCallback (hHCI, pCall);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCancelInquiry : excepted in HCI callback!\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCancelInquiry : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCancelInquiry : result = %d\n", iRes));
    gpState->Unlock ();

    return iRes;
}

int BthPerformInquiry
(
unsigned int        LAP,
unsigned char       length,
unsigned char       num_responses,
unsigned int        cBuffer,
unsigned int        *pcDiscoveredDevices,
BthInquiryResult    *inquiry_list
) {
    *pcDiscoveredDevices = 0;

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    if ((! pcDiscoveredDevices) || (! inquiry_list)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthPerformInquiry : ERROR_INVALID_PARAMTER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = FindCall (CALL_HCI_INQUIRY);
    HANDLE          hEvent;
    InquiryBuffer   *pInquiry;
    int             fCallOwner;

    if (! pCall) {
        pInquiry = CreateNewInquiryBuffer();
        if (! pInquiry) {
            gpState->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        pCall = AllocCall (CALL_HCI_INQUIRY);
        if (! pCall) {
            delete pInquiry;
            gpState->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        pCall->ptrResult.pBuffer = pInquiry;
        hEvent = pCall->hEvent;
        fCallOwner = TRUE;
    } else {
        pInquiry = (InquiryBuffer *)pCall->ptrResult.pBuffer;
        pInquiry->AddRef ();
        hEvent = pInquiry->hWakeUpAll;
        fCallOwner = FALSE;
    }

    int iRes;

    if (fCallOwner) {
        HANDLE hHCI = gpState->hHCI;
        HCI_Inquiry_In pCallback = gpState->hci_if.hci_Inquiry_In;

        iRes = CleanUpL2CAP (pCall);

        gpState->Unlock ();

        if (iRes == ERROR_SUCCESS) {
            iRes = ERROR_INTERNAL_ERROR;

            __try {
                iRes = pCallback (hHCI, pCall, LAP, length, num_responses);
            } __except (1) {
            }
        }
    } else {
        iRes = ERROR_SUCCESS;
        gpState->Unlock ();
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, INFINITE);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (fCallOwner) {
        if (pCall != VerifyCall (pCall))
            iRes = ERROR_INTERNAL_ERROR;
        else {
            if (iRes == ERROR_SUCCESS) {
                if (pCall->fComplete)
                    iRes = pCall->iResult;
                else
                    iRes = ERROR_TIMEOUT;
            }

            DeleteCall (pCall);
        }

        pInquiry->iRes = iRes;
        SetEvent (pInquiry->hWakeUpAll);
    }

    if (pInquiry->iRes == ERROR_SUCCESS) {
        __try {
            int k=0;
            for (int i=0; i<pInquiry->cb; i++) {
                for (int j=0; j<k; j++) {
                    if (pInquiry->ba[i] == inquiry_list[j].ba)
                        break;
                }
                if (j == k) {
                    if (k < (int )cBuffer) {
                        inquiry_list[k].ba                        = pInquiry->ba[i];
                        inquiry_list[k].cod                       = pInquiry->cod[i];
                        inquiry_list[k].clock_offset              = pInquiry->co[i];
                        inquiry_list[k].page_scan_mode            = pInquiry->psm[i];
                        inquiry_list[k].page_scan_period_mode     = pInquiry->pspm[i];
                        inquiry_list[k].page_scan_repetition_mode = pInquiry->psrm[i];
                    }

                    ++k;
                }
            }
            *pcDiscoveredDevices = k;
        } __except (1) {
            iRes = ERROR_INVALID_USER_BUFFER;
        }
    } else
        iRes = pInquiry->iRes;

    if (pInquiry->GetRefCount () == 1)
        delete pInquiry;
    else
        pInquiry->DelRef ();

    gpState->Unlock ();
    return iRes;
}

int BthRemoteNameQuery (BT_ADDR *pbt, unsigned int cBuffer, unsigned int *pcRequired, WCHAR *szString) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    *pcRequired = 0;
    *szString = L'\0';

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    if ((! pcRequired) || (! szString)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthRemoteNameQuery : ERROR_INVALID_PARAMTER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned char utf_name[BD_MAXNAME + 1];
    DWORD cbName = sizeof(utf_name);

    memset (utf_name, 0, sizeof(utf_name));

    SCall *pCall = AllocCall (CALL_HCI_QUERY_REMOTE_NAME);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pCall->ptrResult.pBuffer = utf_name;
    pCall->ptrResult.cbBuffer = cbName;
    pCall->ptrResult.dwPerms = GetCurrentPermissions ();
    pCall->ptrResult.bKMode  = IsSecureVa(utf_name);

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_RemoteNameRequest_In pCallback = gpState->hci_if.hci_RemoteNameRequest_In;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    int iRes = CleanUpL2CAP (pCall);

    if (iRes == ERROR_SUCCESS) {
        iRes = ERROR_INTERNAL_ERROR;
        gpState->Unlock ();
        __try {
            InquiryResultBuffer irb;
            btutil_GetLastIrb(hHCI, pIoctl, pba, &irb);

            iRes = pCallback (hHCI, pCall, pba, irb.page_scan_repetition_mode, irb.page_scan_mode, irb.clock_offset);
        } __except (1) {
        }

        if (iRes == ERROR_SUCCESS)
            WaitForSingleObject (hEvent, INFINITE);

        if (! gpState)
            return ERROR_SERVICE_NOT_ACTIVE;

        gpState->Lock ();
    }

    if (pCall != VerifyCall (pCall)) {
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    if (iRes == ERROR_SUCCESS) {
        __try {
            *pcRequired = MultiByteToWideChar (CP_UTF8, 0, (char *)utf_name, -1, szString, cBuffer);
            if (! *pcRequired)
                *pcRequired = MultiByteToWideChar (CP_ACP, 0, (char *)utf_name, -1, szString, cBuffer);
        } __except (1) {
            iRes = ERROR_INVALID_USER_BUFFER;
        }
    }

    gpState->Unlock ();
    return iRes;
}

int BthReadLocalAddr
(
BT_ADDR *pba
) {
    memset (pba, 0, sizeof (*pba));

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fConnected) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (WaitForSingleObject (gpState->hAddrEvent, 0) != WAIT_OBJECT_0) {
        gpState->AddRef ();
        gpState->Unlock ();
        WaitForSingleObject (gpState->hAddrEvent, 5000);
        gpState->Lock ();
        gpState->DelRef ();

        if (! gpState->fConnected) {
            gpState->Unlock ();

            return ERROR_SERVICE_NOT_ACTIVE;
        }
    }

    *pba = SET_NAP_SAP(gpState->b.NAP, gpState->b.SAP);;

    gpState->Unlock ();
    return ERROR_SUCCESS;
}

int BthGetHardwareStatus
(
int *pStatus
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetHardwareStatus\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetHardwareStatus : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetHardwareStatus : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE hHCI = gpState->hHCI;
    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;

    int iRes = ERROR_INTERNAL_ERROR;
    gpState->AddRef ();
    gpState->Unlock ();
    __try {
        int cSizeRet = 0;
        iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_HARDWARE_STATUS, 0, NULL,
                        sizeof(int), (char *)pStatus, &cSizeRet);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetHardwareStatus : exception!\n"));
    }

    gpState->Lock ();
    gpState->DelRef ();

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetHardwareStatus : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}


//
//  Security manager APIs
//
int BthSetPIN
(
BT_ADDR             *pbt,
int                 cPinLength,
unsigned char       *ppin
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetPIN : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;
    unsigned char pin[16];

    if ((cPinLength > sizeof(pin)) || (cPinLength <= 0)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetPIN : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    __try {
        b = *pba;
        memcpy (pin, ppin, cPinLength);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetPIN : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetPIN : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetPIN : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;

    while (pSecurity && ((pSecurity->b != b) || (pSecurity->csize == 0)))
        pSecurity = pSecurity->pNext;

    if (! pSecurity) {
        pSecurity = (Security *)svsutil_GetFixed (gpState->pfmdSecurity);
        if (pSecurity) {
            pSecurity->pNext = gpState->pSecurity;
            gpState->pSecurity = pSecurity;
        }
    }

    if (! pSecurity) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetPIN : %04x%08x returns ERROR_OUTOFMEMORY\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_OUTOFMEMORY;
    }

    pSecurity->b = b;
    pSecurity->csize = cPinLength;
    memcpy (pSecurity->ucdata, pin, cPinLength);

#if defined (PERSIST_PINS)
    PersistPIN (&b, pin, cPinLength);
#endif

    SetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);
    ResetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);

    PINRequest *pPReq = gpState->pInService;
    PINRequest *pParent = NULL;

    while (pPReq && (b != pPReq->ba))
        pPReq = pPReq->pNext;

    if (pPReq) {
        if (pParent)
            pParent->pNext = pPReq->pNext;
        else
            gpState->pInService = pPReq->pNext;
    } else {
        pPReq = gpState->pRequested;
        pParent  = NULL;
        while (pPReq && (b != pPReq->ba))
            pPReq = pPReq->pNext;

        if (pPReq) {
            if (pParent)
                pParent->pNext = pPReq->pNext;
            else
                gpState->pRequested = pPReq->pNext;
        }
    }

    if (pPReq) {
        FreeRequest (pPReq);
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Going into hci_PINCodeRequestReply\n"));
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetPIN : %04x%08x returns ERROR_SUCCESS\n", pba->NAP, pba->SAP));

    HCI_PINCodeRequestReply_In pCallbackYes = gpState->hci_if.hci_PINCodeRequestReply_In;
    HANDLE h = gpState->hHCI;

    gpState->Unlock ();

    if (pPReq && h && pCallbackYes) {
        __try {
            pCallbackYes (h, NULL, pba, cPinLength, pin);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_PINCodeRequest[Negative]Reply\n"));
        }
    }

    return ERROR_SUCCESS;
}

int BthSetLinkKey
(
BT_ADDR             *pbt,
unsigned char       *plink
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetLinkKey : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;
    unsigned char link[16];

    __try {
        b = *pba;
        memcpy (link, plink, sizeof(link));
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetLinkKey : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;

    while (pSecurity && ((pSecurity->b != b) || (pSecurity->csize != 0)))
        pSecurity = pSecurity->pNext;

    if (! pSecurity) {
        pSecurity = (Security *)svsutil_GetFixed (gpState->pfmdSecurity);
        if (pSecurity) {
            pSecurity->pNext = gpState->pSecurity;
            gpState->pSecurity = pSecurity;
        }
    }

    if (! pSecurity) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetLinkKey : %04x%08x returns ERROR_OUTOFMEMORY\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_OUTOFMEMORY;
    }

    pSecurity->b = b;
    pSecurity->csize = 0;
    memcpy (pSecurity->ucdata, link, sizeof(link));

#if defined (PERSIST_LINK_KEYS)
    PersistLinkKey (&b, plink);
#endif

    SetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);
    ResetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetLinkKey : %04x%08x returns ERROR_SUCCESS\n", pba->NAP, pba->SAP));

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

int BthRevokePIN
(
BT_ADDR             *pbt
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokePIN : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokePIN : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokePIN : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokePIN : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;
    Security *pParent = NULL;

    while (pSecurity && ((pSecurity->b != b) || (pSecurity->csize == 0))) {
        pParent = pSecurity;
        pSecurity = pSecurity->pNext;
    }

    int iRes = ERROR_NOT_FOUND;
    if (pSecurity) {
        iRes = ERROR_SUCCESS;
        if (pParent)
            pParent->pNext = pSecurity->pNext;
        else
            gpState->pSecurity = pSecurity->pNext;

        svsutil_FreeFixed (pSecurity, gpState->pfmdSecurity);

#if defined (PERSIST_PINS)
        HKEY hk;
        DWORD dwDisp;
        if (ERROR_SUCCESS == RegCreateKeyEx (HKEY_BASE, L"comm\\security\\bluetooth", 0, NULL, 0, KEY_WRITE, NULL, &hk, &dwDisp)) {
            WCHAR szValueName[3 + 4 + 8 + 1];
            _snwprintf (szValueName, SVSUTIL_ARRLEN(szValueName), L"pin%04x%08x", b.NAP, b.SAP);
            szValueName[SVSUTIL_ARRLEN(szValueName)-1] = 0;
            RegDeleteValue (hk, szValueName);
            RegCloseKey (hk);
        }
#endif

        SetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);
        ResetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokePIN : %04x%08x returns ERROR_SUCCESS\n", pba->NAP, pba->SAP));
    } else
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokePIN : %04x%08x returns ERROR_NOT_FOUND\n", pba->NAP, pba->SAP));

    gpState->Unlock ();

    return iRes;
}

int BthRevokeLinkKey
(
BT_ADDR             *pbt
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokeLinkKey : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokeLinkKey : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokeLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRevokeLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;
    Security *pParent = NULL;

    while (pSecurity && ((pSecurity->b != b) || (pSecurity->csize != 0))) {
        pParent = pSecurity;
        pSecurity = pSecurity->pNext;
    }

    int iRes = ERROR_NOT_FOUND;
    if (pSecurity) {
        iRes = ERROR_SUCCESS;
        if (pParent)
            pParent->pNext = pSecurity->pNext;
        else
            gpState->pSecurity = pSecurity->pNext;

        svsutil_FreeFixed (pSecurity, gpState->pfmdSecurity);

#if defined (PERSIST_LINK_KEYS)
        HKEY hk;
        DWORD dwDisp;
        if (ERROR_SUCCESS == RegCreateKeyEx (HKEY_BASE, L"comm\\security\\bluetooth", 0, NULL, 0, KEY_WRITE, NULL, &hk, &dwDisp)) {
            WCHAR szValueName[3 + 4 + 8 + 1];
            _snwprintf (szValueName, SVSUTIL_ARRLEN(szValueName), L"key%04x%08x", b.NAP, b.SAP);
            szValueName[SVSUTIL_ARRLEN(szValueName)-1] = 0;
            RegDeleteValue (hk, szValueName);
            RegCloseKey (hk);
        }
#endif

        SetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);
        ResetEvent (gpState->hNamedEvents[BTD_EVENT_PAIRING_CHANGED]);

        BTEVENT bte;
        memset (&bte, 0, sizeof(BTEVENT));
        bte.dwEventId = BTE_KEY_REVOKED;

        BT_LINK_KEY_EVENT *plke = (BT_LINK_KEY_EVENT*) bte.baEventData;
        plke->dwSize = sizeof(BT_LINK_KEY_EVENT);
        plke->bta = SET_NAP_SAP (pba->NAP, pba->SAP);

        NotifyEvent (&bte, BTE_CLASS_PAIRING);

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokeLinkKey : %04x%08x returns ERROR_SUCCESS\n", pba->NAP, pba->SAP));
    } else
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthRevokeLinkKey : %04x%08x returns ERROR_NOT_FOUND\n", pba->NAP, pba->SAP));

    gpState->Unlock ();

    return iRes;
}

int BthGetLinkKey
(
BT_ADDR             *pbt,
unsigned char       plink[16]
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetLinkKey : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;

    __try {
        b = *pba;
        memset (plink, 0, 16);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetLinkKey : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetLinkKey : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Security *pSecurity = gpState->pSecurity;

    while (pSecurity && ((pSecurity->b != b) || (pSecurity->csize != 0)))
        pSecurity = pSecurity->pNext;

    int iRes = ERROR_NOT_FOUND;
    if (pSecurity) {
        iRes = ERROR_SUCCESS;
        memcpy (plink, pSecurity->ucdata, 16);
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetLinkKey : %04x%08x returns ERROR_SUCCESS\n", pba->NAP, pba->SAP));
    } else
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetLinkKey : %04x%08x returns ERROR_NOT_FOUND\n", pba->NAP, pba->SAP));

    gpState->Unlock ();

    return iRes;
}

int BthAuthenticate
(
BT_ADDR             *pbt
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthAuthenticate : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthAuthenticate : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fConnected) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthAuthenticate : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    if (IsLinkEncrypted (h)) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x : Link is already encrypted\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();
        
        return ERROR_SUCCESS;
    }

    int fRetry = TRUE;

    for (int i = 0 ; fRetry && (i < BTD_AUTH_RETRY) ; ++i) {
        fRetry = FALSE;

        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"[SHELL] BthAuthenticate : %04x%08x : connection handle 0x%04x. Requesting authentication now!\n", pba->NAP, pba->SAP, h));

        SCall *pCall = AllocCall (CALL_HCI_AUTHENTICATE);
        if (! pCall) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x returns ERROR_OUTOFMEMORY\n", pba->NAP, pba->SAP));
            gpState->Unlock ();

            return ERROR_OUTOFMEMORY;
        }

        HANDLE hEvent = pCall->hEvent;
        HANDLE hHCI = gpState->hHCI;
        HCI_AuthenticationRequested_In pCallback = gpState->hci_if.hci_AuthenticationRequested_In;

        gpState->AddRef ();
        gpState->Unlock ();
        iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pCallback (hHCI, pCall, h);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] BthAuthenticate : %04x%08x : exception in hci_AuthenticationRequested_In\n", pba->NAP, pba->SAP));
        }

        if (iRes == ERROR_SUCCESS)
            iRes = WaitForCommandCompletion (hEvent, &b, h);

        gpState->Lock ();
        gpState->DelRef ();

        if (! gpState->fIsRunning) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x returns ERROR_CANCELLED\n", pba->NAP, pba->SAP));
            gpState->Unlock ();
            return ERROR_CANCELLED;
        }

        if (pCall == VerifyCall (pCall)) {
            if (iRes == ERROR_SUCCESS) {
                if (pCall->fComplete) {
                    iRes = pCall->iResult;
                    if (iRes != ERROR_SUCCESS)
                        fRetry = pCall->fRetry;
                    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x call completed result = %d\n", pba->NAP, pba->SAP, iRes));
                } else {
                    iRes = ERROR_INTERNAL_ERROR;
                    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x call has not completed (ERROR_INTERNAL_ERROR)\n", pba->NAP, pba->SAP));
                }
            }

            DeleteCall (pCall);
        } else if (iRes == ERROR_SUCCESS) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthAuthenticate : %04x%08x call lost (ERROR_INTERNAL_ERROR)\n", pba->NAP, pba->SAP));
            iRes = ERROR_INTERNAL_ERROR;
        }

        if (fRetry)
            Sleep (BTD_AUTH_SLEEP);
    }

    gpState->Unlock ();

    return iRes;
}


int BthSetEncryption
(
BT_ADDR             *pbt,
int                 fOn
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x\n", pba->NAP, pba->SAP));
    BD_ADDR b;

    __try {
        b = *pba;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetEncryption : %04x%08x returns ERROR_INVALID_PARAMETER\n", pba->NAP, pba->SAP));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetEncryption : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fConnected) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetEncryption : %04x%08x returns ERROR_SERVICE_NOT_ACTIVE\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x returns %d\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    if (IsLinkEncrypted (h)) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x : Link is already encrypted\n", pba->NAP, pba->SAP, iRes));
        gpState->Unlock ();
        
        return ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"[SHELL] BthSetEncryption : %04x%08x : connection handle 0x%04x. Setting encryption now!\n", pba->NAP, pba->SAP, h));

    SCall *pCall = AllocCall (CALL_HCI_SET_ENCRYPTION);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x returns ERROR_OUTOFMEMORY\n", pba->NAP, pba->SAP));
        gpState->Unlock ();

        return ERROR_OUTOFMEMORY;
    }

    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;
    HCI_SetConnectionEncryption_In pCallback = gpState->hci_if.hci_SetConnectionEncryption_In;

    gpState->AddRef ();
    gpState->Unlock ();
    iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (hHCI, pCall, h, fOn ? 1 : 0);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] BthSetEncryption : %04x%08x : exception in hci_SetConnectionEncryption_In\n", pba->NAP, pba->SAP));
    }

    if (iRes == ERROR_SUCCESS)
        iRes = WaitForCommandCompletion (hEvent, &b, h);

    gpState->Lock ();
    gpState->DelRef ();

    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x returns ERROR_CANCELLED\n", pba->NAP, pba->SAP));
        gpState->Unlock ();
        return ERROR_CANCELLED;
    }

    if (pCall == VerifyCall (pCall)) {
        if (iRes == ERROR_SUCCESS) {
            if (pCall->fComplete) {
                iRes = pCall->iResult;
                IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x call completed result = %d\n", pba->NAP, pba->SAP, iRes));
            } else {
                iRes = ERROR_INTERNAL_ERROR;
                IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x call has not completed (ERROR_INTERNAL_ERROR)\n", pba->NAP, pba->SAP));
            }
        }

        DeleteCall (pCall);
    } else if (iRes == ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetEncryption : %04x%08x call lost (ERROR_INTERNAL_ERROR)\n", pba->NAP, pba->SAP));
        iRes = ERROR_INTERNAL_ERROR;
    }

    gpState->Unlock ();

    return iRes;
}


int BthSetSecurityUI
(
HANDLE      hEvent,
DWORD       dwStoreTimeout,
DWORD       dwProcTimeout
) {
    if (hEvent) {
        if ((dwStoreTimeout < 1000) || (dwStoreTimeout > 600000)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetSecurityUI returns ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }

        if ((dwProcTimeout < 1000) || (dwProcTimeout > 600000)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetSecurityUI returns ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetSecurityUI returns ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthSetSecurityUI returns ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    int iRes = ERROR_ALREADY_ASSIGNED;

    if ((! hEvent) && gpState->hSecurityUIEvent) {
        if (GetOwnerProcess() != gpState->hSecurityUIProc) {
            iRes = ERROR_ACCESS_DENIED;
        } else {
            CleanUpSecurityUIHandler ();
            iRes = ERROR_SUCCESS;
        }
    }

    if (hEvent && (! gpState->hSecurityUIEvent)) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSetSecurityUI - setting security handler\n"));

        gpState->hSecurityUIEvent = hEvent;
        gpState->hSecurityUIProc = GetOwnerProcess ();
        gpState->dwSecurityProcPermissions = GetCurrentPermissions ();
        gpState->dwPINRequestStoreTimeout = dwStoreTimeout;
        gpState->dwPINRequestProcessTimeout = dwProcTimeout;

        iRes = ERROR_SUCCESS;
    }

    gpState->Unlock ();

    return iRes;
}

int BthGetPINRequest
(
BT_ADDR *pbt
) {
    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetPINRequest returns ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthGetPINRequest returns ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    int iRes = ERROR_NOT_FOUND;

    PINRequest *pThis = gpState->pRequested;

    if (pThis) {
        gpState->pRequested = pThis->pNext;
        pThis->pNext = gpState->pInService;
        gpState->pInService = pThis;

        btutil_UnScheduleEvent (pThis->dwTimeOutCookie);
        pThis->dwTimeOutCookie = btutil_ScheduleEvent (ServicePINTimeout, pThis, gpState->dwPINRequestProcessTimeout);

        *pbt = SET_NAP_SAP (pThis->ba.NAP, pThis->ba.SAP);

        iRes = ERROR_SUCCESS;
    }
    gpState->Unlock ();

    return iRes;
}

int BthRefusePINRequest
(
BT_ADDR *pbt
) {
    BD_ADDR b;
    __try {
        b.NAP = GET_NAP (*pbt);
        b.SAP = GET_SAP (*pbt);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRefusePINRequest -- excpetion getting address\n"));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRefusePINRequest returns ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthRefusePINRequest returns ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    int iRes = ERROR_NOT_FOUND;

    PINRequest *pPReq = gpState->pInService;
    PINRequest *pParent = NULL;

    while (pPReq && (b != pPReq->ba))
        pPReq = pPReq->pNext;

    if (pPReq) {
        if (pParent)
            pParent->pNext = pPReq->pNext;
        else
            gpState->pInService = pPReq->pNext;
    } else {
        pPReq = gpState->pRequested;
        pParent  = NULL;
        while (pPReq && (b != pPReq->ba))
            pPReq = pPReq->pNext;

        if (pPReq) {
            if (pParent)
                pParent->pNext = pPReq->pNext;
            else
                gpState->pRequested = pPReq->pNext;
        }
    }

    if (pPReq) {
        FreeRequest (pPReq);
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"Going into hci_PINCodeRequestNegativeReply\n"));
        iRes = ERROR_SUCCESS;
    }

    HCI_PINCodeRequestNegativeReply_In pCallbackNo = gpState->hci_if.hci_PINCodeRequestNegativeReply_In;
    HANDLE h = gpState->hHCI;

    gpState->Unlock ();

    if (pPReq && h && pCallbackNo) {
        __try {
            pCallbackNo (h, NULL, &b);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[SHELL] Exception in hci_PINCodeRequest[Negative]Reply\n"));
        }
    }

    return iRes;
}

int BthAnswerPairRequest
(
BT_ADDR *pba,
int cPinLength,
unsigned char *ppin
) {
    int iRes = ERROR_SUCCESS;

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    HANDLE hPairEvent = OpenEvent (EVENT_ALL_ACCESS, FALSE, BTH_NAMEDEVENT_PAIRING_CHANGED);
    if (! hPairEvent) {
        IFDBG(DebugOut (DEBUG_WARN, L"[SHELL] BthAnswerBonding -- Can't open named event.\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    iRes = BthSetPIN (pba, cPinLength, ppin);
    if (iRes != ERROR_SUCCESS) {
        CloseHandle (hPairEvent);
        return iRes;
    }

    BOOL fSuccess = FALSE;

    // Wait for link key notification (up to 6 seconds).
    for (int i = 0; i < 6; i++) {
        DWORD dwWait = WaitForSingleObject (hPairEvent, 1000);
        if ( dwWait == WAIT_OBJECT_0 ) {
            unsigned char key[16];
            if (ERROR_SUCCESS == BthGetLinkKey (pba, key)) {
                fSuccess = TRUE;
                break;
            }
        }
    }

    if (! fSuccess) {
        iRes = ERROR_NOT_AUTHENTICATED;
    }

    CloseHandle (hPairEvent);

    // Some devices (e.g. Sony Ericsson headsets) will not
    // behave well after an HCI disconnect immediately
    // followed by an SDP query.  To work around this we
    // need to sleep after closing the pairing connection.
    Sleep(gpState->dwPairSleep);

    return iRes;
}

int BthPairRequest
(
BT_ADDR *pba,
int cPinLength,
unsigned char *ppin
) {
    int iRes = ERROR_SUCCESS;

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    iRes = BthSetPIN (pba, cPinLength, ppin);
    if (iRes != ERROR_SUCCESS) {
        return iRes;
    }

    unsigned short usConn = 0;

    iRes = BthCreateACLConnection (pba, &usConn);
    if (iRes != ERROR_SUCCESS) {
        return iRes;
    }

    // Result iRes is returned by this function
    iRes = BthAuthenticate (pba);

    BthCloseConnection (usConn);

    // Some devices (e.g. Sony Ericsson headsets) will not
    // behave well if we send an HCI disconnect immediately
    // followed by an SDP query.  To work around this we
    // need to sleep after closing the pairing connection.
    Sleep(gpState->dwPairSleep);

    return iRes;
}

//
//  Misc...
//

int BthTerminateIdleConnections (void) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_L2CAP_CLEAN_UP_L2CAP);

    int iRes = ERROR_OUTOFMEMORY;

    if (pCall) {
        iRes = CleanUpL2CAP (pCall);
        DeleteCall (pCall);
    }

    gpState->Unlock ();

    return iRes;
}

int BthGetAddress
(
unsigned short  handle,
BT_ADDR         *pba
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetAddress : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    BD_ADDR b;

    BT_LAYER_IO_CONTROL pCallbackIOCTL = gpState->hci_if.hci_ioctl;
    HANDLE hHCI = gpState->hHCI;

    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        int dwData = 0;
        iRes = pCallbackIOCTL (hHCI, BTH_HCI_IOCTL_GET_BD_FOR_HANDLE, sizeof(handle), (char *)&handle, sizeof(b), (char *)&b, &dwData);
        if ((iRes == ERROR_SUCCESS) && (dwData != sizeof(BD_ADDR))) {
            IFDBG(DebugOut (DEBUG_ERROR, L"BthGetAddress : 0x%04x : incorrect return buffer in hci_ioctl\n", handle));
            iRes = ERROR_INTERNAL_ERROR;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthGetAddress : 0x%04x : exception in hci_ioctl\n", handle));
    }

    if (iRes == ERROR_SUCCESS) {
        __try {
            *pba = SET_NAP_SAP (b.NAP, b.SAP);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthGetAddress : ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetAddress : result = %d\n", iRes));
    return iRes;
}

int SetEventFilter
(
BYTE bFilterType, 
BYTE bFilterConditionType, 
unsigned char condition[7]
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"SetEventFilter : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_EVENTFILTER);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"SetEventFilter : created call 0x%08x\n", pCall));

    HCI_SetEventFilter_In pCallback = gpState->hci_if.hci_SetEventFilter_In;
    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, bFilterType, bFilterConditionType, condition);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"SetEventFilter : exception in hci_SetEventFilter_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"SetEventFilter : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"SetEventFilter : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;    
}

int BthSetInquiryFilter
(
BT_ADDR         *pba
) {
    unsigned char condition[7];
    
    __try {
        memcpy(condition, pba, sizeof(BD_ADDR));
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthSetInquiryFilter : ERROR_INVALID_PARAMETER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    return SetEventFilter(HCI_EVENT_FILTER_INQUIRY, HCI_EVENT_FILTER_CONDITION_BDADDR, condition);
}

int BthSetCODInquiryFilter
(
unsigned int cod,
unsigned int codMask
) {
    unsigned char condition[7];
    
    __try {
        memcpy(condition, (unsigned char *)&cod, 3);
        memcpy(&condition[3], (unsigned char *)&codMask, 3);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthSetCODInquiryFilter : ERROR_INVALID_PARAMETER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    return SetEventFilter(HCI_EVENT_FILTER_INQUIRY, HCI_EVENT_FILTER_CONDITION_COD, condition);
}

int BthClearInquiryFilter
(
void
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthClearInquiryFilter : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_EVENTFILTER);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthClearInquiryFilter : created call 0x%08x\n", pCall));

    HCI_SetEventFilter_In pCallback = gpState->hci_if.hci_SetEventFilter_In;
    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;

    unsigned char condition[7];
    memset (condition, 0, sizeof(condition));

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, HCI_EVENT_FILTER_CLEAR, 0, condition);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthClearInquiryFilter : exception in hci_SetEventFilter_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthClearInquiryFilter : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthClearInquiryFilter : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthSwitchRole
(
BT_ADDR* pbt,
USHORT usRole
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba; // Convert to BD_ADDR
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthSwitchRole : pbt=%08x returns ERROR_INVALID_PARAMETER\n", pbt));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) { // ensure stack is running
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthSwitchRole : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_SWITCH_ROLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSwitchRole : created call 0x%08x\n", pCall));

    HCI_SwitchRole_In pCallback = gpState->hci_if.hci_SwitchRole_In;
    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, &b, (UCHAR)usRole);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSwitchRole : exception in hci_SwitchRole_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000); // Wait for async call to complete

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        // Async call is not in list anymore
        IFDBG(DebugOut (DEBUG_WARN, L"BthSwitchRole : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT; // Async call was not completed
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthSwitchRole : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthGetRole
(
BT_ADDR* pbt,
USHORT* pusRole
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba; // Convert to BD_ADDR
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetRole : pbt=%08x returns ERROR_INVALID_PARAMETER\n", pbt));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) { // ensure stack is running
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetRole : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetRole : %04x%08x returns %d\n", b.NAP, b.SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_GET_ROLE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetRole : created call 0x%08x\n", pCall));

    HCI_RoleDiscovery_In pCallback = gpState->hci_if.hci_RoleDiscovery_In;
    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;

    gpState->AddRef ();
    gpState->Unlock ();

    iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, h); // Call into HCI
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetRole : exception in hci_SwitchRole_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000); // Wait for async call to complete

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        // Async call is not in list anymore
        IFDBG(DebugOut (DEBUG_WARN, L"BthGetRole : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT; // Async call was not completed
    }

    if (iRes == ERROR_SUCCESS) {
        __try {
            *pusRole = pCall->ucResult;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetRole : ERROR_INVALID_PARAMETER\n"));
        }
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthGetRole : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthReadRSSI
(
BT_ADDR* pbt,
BYTE* pbRSSI
) {
    BD_ADDR *pba = (BD_ADDR *)pbt;
    BD_ADDR b;

    __try {
        b = *pba; // Convert to BD_ADDR
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRSSI : pbt=%08x returns ERROR_INVALID_PARAMETER\n", pbt));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) { // ensure stack is running
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRSSI : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    unsigned short h = 0;
    int iRes = GetHandleForBA (&b, &h);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRSSI : %04x%08x returns %d\n", b.NAP, b.SAP, iRes));
        gpState->Unlock ();

        return iRes;
    }

    SCall *pCall = AllocCall (CALL_HCI_READ_RSSI);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRSSI : created call 0x%08x\n", pCall));

    HCI_ReadRSSI_In pCallback = gpState->hci_if.hci_ReadRSSI_In;
    HANDLE hEvent = pCall->hEvent;
    HANDLE hHCI = gpState->hHCI;

    gpState->AddRef ();
    gpState->Unlock ();

    iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, h); // Call into HCI
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRSSI : exception in hci_SwitchRole_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000); // Wait for async call to complete

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        // Async call is not in list anymore
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadRSSI : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT; // Async call was not completed
    }

    if (iRes == ERROR_SUCCESS) {
        __try {
            *pbRSSI = pCall->ucResult;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRSSI : ERROR_INVALID_PARAMETER\n"));
            iRes = ERROR_INVALID_PARAMETER;
        }
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadRSSI : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthCreateACLConnection
(
BT_ADDR         *pbt,
unsigned short  *phandle
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    BD_ADDR b;
    __try {
        b = *(BD_ADDR*)pbt;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateACLConnection : ERROR_INVALID_PARAMETER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateACLConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_ACL_CONNECT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    BT_LAYER_IO_CONTROL pIoctl = gpState->hci_if.hci_ioctl;
    HCI_CreateConnection_In pCallback = gpState->hci_if.hci_CreateConnection_In;
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        InquiryResultBuffer irb;
        btutil_GetLastIrb(hHCI, pIoctl, &b, &irb);
        
        iRes = pCallback (hHCI, pCall, &b, BT_PACKET_TYPE_DM1 | BT_PACKET_TYPE_DM3 | BT_PACKET_TYPE_DM5, 
            irb.page_scan_repetition_mode, irb.page_scan_mode, irb.clock_offset, 1);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateACLConnection : exception in hci_CreateConnection_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, INFINITE);

    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateACLConnection : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    unsigned short usResult = pCall->usResult;
    DeleteCall (pCall);

    gpState->Unlock ();

    if (iRes == ERROR_SUCCESS) {
        __try {
            *phandle = usResult;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthCreateACLConnection : ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateACLConnection : result = %d\n", iRes));
    return iRes;
}

int BthCloseConnection
(
unsigned short  handle
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthCloseConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_BASEBAND_DISCONNECT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HCI_Disconnect_In pCallback = gpState->hci_if.hci_Disconnect_In;
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, handle, 0x13);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCloseConnection : exception in hci_Disconnect_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCloseConnection : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    DeleteCall (pCall);

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCloseConnection : result = %d\n", iRes));
    gpState->Unlock ();
    return iRes;
}

int BthCreateSCOConnection
(
BT_ADDR         *pbt,
unsigned short  *phandle
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    BD_ADDR b;
    __try {
        b = *(BD_ADDR*)pbt;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSCOConnection : ERROR_INVALID_PARAMETER\n"));
        return ERROR_INVALID_PARAMETER;
    }
    
    gpState->Lock ();

    unsigned short hAclConnection = 0;
    int iRes = GetHandleForBA(&b, &hAclConnection);
    
    gpState->AddRef ();
    gpState->Unlock ();
    
    if (ERROR_SUCCESS != iRes) {
        // No ACL link found
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateSCOConnection : ACL connection to target address %04x%08x not found", b.NAP, b.SAP));                        
        gpState->DelRef ();
        return ERROR_CONNECTION_INVALID;
    }
    
    unsigned char mode = 0;
    if ((ERROR_SUCCESS == BthGetCurrentMode(pbt, &mode)) &&
        (mode == 0x03)) {
        // Need to unpark before creating a synchronous connection
        BthExitParkMode (pbt);
    }
    
    gpState->Lock ();
    gpState->DelRef ();
    
    if (! (gpState->fIsRunning && gpState->fConnected)) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSCOConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_SCO_CONNECT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HCI_AddSCOConnection_In pCallback = gpState->hci_if.hci_AddSCOConnection_In;
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, hAclConnection, BT_PACKET_TYPE_HV1|BT_PACKET_TYPE_HV2|BT_PACKET_TYPE_HV3);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthCreateSCOConnection : exception in hci_AddSCOConnection_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSCOConnection : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    unsigned short usResult = pCall->usResult;
    DeleteCall (pCall);

    gpState->Unlock ();

    if (iRes == ERROR_SUCCESS) {
        __try {
            *phandle = usResult;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSCOConnection : ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateSCOConnection : result = %d\n", iRes));
    return iRes;
}

int BthAcceptSCOConnections
(
BOOL fAccept
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    // Either inc or dec ref count for outstanding clients
    // who have enabled SCO-accept mode

    if (fAccept) {
        gpState->cntAcceptSCO++;
    }
    else if (gpState->cntAcceptSCO > 0) {
        gpState->cntAcceptSCO--;
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

int BthCreateSynchronousConnection
(
BT_ADDR         *pbt,
unsigned short  *pHandle,
unsigned int    txBandwidth,
unsigned int    rxBandwidth,
unsigned short  maxLatency,
unsigned short  voiceSetting,
unsigned char   retransmit
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    BD_ADDR b;
    __try {
        b = *(BD_ADDR*)pbt;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSynchronousConnection : ERROR_INVALID_PARAMETER\n"));
        return ERROR_INVALID_PARAMETER;
    }

    gpState->Lock ();

    unsigned short hAclConnection = 0;    
    int iRes = GetHandleForBA(&b, &hAclConnection);

    gpState->AddRef ();
    gpState->Unlock ();
    
    if (ERROR_SUCCESS != iRes) {
        // No ACL link found
        IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateSynchronousConnection : ACL connection to target address %04x%08x not found", b.NAP, b.SAP));
        gpState->DelRef ();
        return ERROR_CONNECTION_INVALID;
    }
    
    unsigned char mode = 0;
    if ((ERROR_SUCCESS == BthGetCurrentMode(pbt, &mode)) &&
        (mode == 0x03)) {
        // Need to unpark before creating a synchronous connection
        BthExitParkMode (pbt);
    }
    
    gpState->Lock ();
    gpState->DelRef ();
    
    if (! (gpState->fIsRunning && gpState->fConnected)) {
        gpState->Unlock ();
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSynchronousConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall (CALL_HCI_SCO_CONNECT);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HCI_SetupSynchronousConnection_In pCallback = gpState->hci_if.hci_SetupSynchronousConnection_In;
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;
    
    gpState->AddRef ();
    gpState->Unlock ();
    
    iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback (hHCI, pCall, hAclConnection, txBandwidth, rxBandwidth, maxLatency, voiceSetting, retransmit, gpState->ESCOPacketType);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"BthCreateSynchronousConnection : exception in hci_AddSCOConnection_In\n"));
    }

    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);

    gpState->Lock ();
    gpState->DelRef ();

    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSynchronousConnection : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }

    unsigned short usResult = pCall->usResult;
    DeleteCall (pCall);

    gpState->Unlock ();

    if (iRes == ERROR_SUCCESS) {
        __try {
            *pHandle = usResult;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_WARN, L"BthCreateSynchronousConnection : ERROR_INVALID_PARAMETER\n"));
            return ERROR_INVALID_PARAMETER;
        }
    }

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthCreateSynchronousConnection : result = %d\n", iRes));
    return iRes;
}

int BthAcceptSynchronousConnections
(
BOOL fAccept
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    // Either inc or dec ref count for outstanding clients
    // who have enabled sync-accept mode

    if (fAccept) {
        gpState->cntAcceptSync++;
    }
    else if (gpState->cntAcceptSync > 0) {
        gpState->cntAcceptSync--;
    }

    gpState->Unlock ();

    return ERROR_SUCCESS;
}

typedef enum _ScanMethod {
    SCAN_INQUIRY,
    SCAN_PAGE
} ScanMethod;

int BthWriteScanActivity
(
unsigned short scanInterval,
unsigned short scanWindow,
ScanMethod method
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanActivity\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanActivity : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanActivity : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    SCall *pCall = AllocCall ((method == SCAN_INQUIRY) ? CALL_HCI_WRITE_INQUIRY_SCAN_ACTIVITY : CALL_HCI_WRITE_PAGE_SCAN_ACTIVITY);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }
    
    HCI_WritePageScanActivity_In pCallbackPage = gpState->hci_if.hci_WritePageScanActivity_In;
    HCI_WriteInquiryScanActivity_In pCallbackInquiry = gpState->hci_if.hci_WriteInquiryScanActivity_In;
    
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        if (method == SCAN_INQUIRY) {
            iRes = pCallbackInquiry (hHCI, pCall, scanInterval, scanWindow);
        } else {
            iRes = pCallbackPage (hHCI, pCall, scanInterval, scanWindow);
        }
    } __except (1) {
        if (method == SCAN_INQUIRY) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanActivity : exception in hci_WriteInquiryScanActivity_In\n"));
        } else {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanActivity : exception in hci_WritePageScanActivity_In\n"));
        }        
    }
    
    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);
        
    gpState->Lock ();
    gpState->DelRef ();
        
    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanActivity : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }
    
    DeleteCall (pCall);
    
    gpState->Unlock();
    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanActivity : result = %d\n", iRes));    
    return iRes;    
}


int BthWritePageScanActivity
(
unsigned short pageScanInterval,
unsigned short pageScanWindow
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageScanActivity\n"));
    int iRes = BthWriteScanActivity(pageScanInterval, pageScanWindow, SCAN_PAGE);    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageScanActivity : result = %d\n", iRes));        
    return iRes;    
}

int BthWriteInquiryScanActivity
(
unsigned short inquiryScanInterval,
unsigned short inquiryScanWindow
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteInquiryScanActivity\n"));
    int iRes = BthWriteScanActivity(inquiryScanInterval, inquiryScanWindow, SCAN_INQUIRY);    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteInquiryScanActivity : result = %d\n", iRes));
    return iRes;    
}

int BthReadScanActivity
(
unsigned short* pScanInterval,
unsigned short* pScanWindow,
ScanMethod method
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanActivity\n"));
    
    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanActivity : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanActivity : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall ((method == SCAN_INQUIRY) ? CALL_HCI_READ_INQUIRY_SCAN_ACTIVITY : CALL_HCI_READ_PAGE_SCAN_ACTIVITY);    
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }
    
    HCI_ReadPageScanActivity_In pCallbackPage = gpState->hci_if.hci_ReadPageScanActivity_In;
    HCI_ReadInquiryScanActivity_In pCallbackInquiry = gpState->hci_if.hci_ReadInquiryScanActivity_In;
    
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        if (method == SCAN_INQUIRY) {
            iRes = pCallbackInquiry (hHCI, pCall);
        } else {
            iRes = pCallbackPage (hHCI, pCall);
        }
    } __except (1) {
        if (method == SCAN_INQUIRY) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanActivity : exception in hci_ReadInquiryScanActivity_In\n"));
        } else {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanActivity : exception in hci_ReadPageScanActivity_In\n"));
        }        
    }
        
    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);
        
    gpState->Lock ();
    gpState->DelRef ();
        
    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanActivity : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }
    
    if (iRes == ERROR_SUCCESS)
    {
        __try {
            *pScanInterval = pCall->activityResult.usScanInterval;
            *pScanWindow = pCall->activityResult.usScanWindow;
        } __except (1) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanActivity : ERROR_INVALID_PARAMETER\n"));
            iRes = ERROR_INVALID_PARAMETER;
        }
    }
    
    DeleteCall (pCall);
    
    gpState->Unlock();
    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanActivity : result = %d\n", iRes));    
    return iRes;       
}

int BthReadPageScanActivity
(
unsigned short* pPageScanInterval,
unsigned short* pPageScanWindow
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageScanActivity\n"));
    int iRes = BthReadScanActivity(pPageScanInterval, pPageScanWindow, SCAN_PAGE);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageScanActivity : result = %d\n", iRes));
    return iRes;   
}

int BthReadInquiryScanActivity
(
unsigned short* pInquiryScanInterval,
unsigned short* pInquiryScanWindow
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadInquiryScanActivity\n"));
    int iRes = BthReadScanActivity(pInquiryScanInterval, pInquiryScanWindow, SCAN_INQUIRY);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadInquiryScanActivity : result = %d\n", iRes));
    return iRes;   
}

int BthWriteScanType
(
unsigned char scanType,
ScanMethod method
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanType\n"));

    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanType : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanType : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    SCall *pCall = AllocCall ((method == SCAN_INQUIRY) ? CALL_HCI_WRITE_INQUIRY_SCAN_TYPE : CALL_HCI_WRITE_PAGE_SCAN_TYPE);
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }
    
    HCI_WritePageScanType_In pCallbackPage = gpState->hci_if.hci_WritePageScanType_In;
    HCI_WriteInquiryScanType_In pCallbackInquiry = gpState->hci_if.hci_WriteInquiryScanType_In;
    
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        if (method == SCAN_INQUIRY) {
            iRes = pCallbackInquiry (hHCI, pCall, scanType);
        } else {
            iRes = pCallbackPage (hHCI, pCall, scanType);
        }
    } __except (1) {
        if (method == SCAN_INQUIRY) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanType : exception in hci_WriteInquiryScanType_In\n"));
        } else {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanType : exception in hci_WritePageScanType_In\n"));
        }        
    }
    
    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);
        
    gpState->Lock ();
    gpState->DelRef ();
        
    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthWriteScanType : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }
    
    DeleteCall (pCall);
    
    gpState->Unlock();
    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteScanType : result = %d\n", iRes));    
    return iRes; 
}


int BthWritePageScanType
(
unsigned char pageScanType
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageScanType\n"));
    int iRes = BthWriteScanType(pageScanType, SCAN_PAGE);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWritePageScanType : result = %d\n", iRes));
    return iRes;   
}

int BthWriteInquiryScanType
(
unsigned char inquiryScanType
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteInquiryScanType\n"));
    int iRes = BthWriteScanType(inquiryScanType, SCAN_INQUIRY);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthWriteInquiryScanType : result = %d\n", iRes));
    return iRes;
}

int BthReadScanType
(
unsigned char* pScanType,
ScanMethod method
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanType\n"));
    
    if (! gpState) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanType : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpState->Lock ();
    if (! gpState->fIsRunning) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanType : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    SCall *pCall = AllocCall ((method == SCAN_INQUIRY) ? CALL_HCI_READ_INQUIRY_SCAN_TYPE : CALL_HCI_READ_PAGE_SCAN_TYPE);    
    if (! pCall) {
        gpState->Unlock ();
        return ERROR_OUTOFMEMORY;
    }
    
    HCI_ReadPageScanType_In pCallbackPage = gpState->hci_if.hci_ReadPageScanType_In;
    HCI_ReadInquiryScanType_In pCallbackInquiry = gpState->hci_if.hci_ReadInquiryScanType_In;
    
    HANDLE hHCI = gpState->hHCI;
    HANDLE hEvent = pCall->hEvent;

    gpState->AddRef ();
    gpState->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        if (method == SCAN_INQUIRY) {
            iRes = pCallbackInquiry (hHCI, pCall);
        } else {
            iRes = pCallbackPage (hHCI, pCall);
        }
    } __except (1) {
        if (method == SCAN_INQUIRY) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanType : exception in hci_ReadInquiryScanType_In\n"));
        } else {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanType : exception in hci_ReadPageScanType_In\n"));
        }        
    }
        
    if (iRes == ERROR_SUCCESS)
        WaitForSingleObject (hEvent, 5000);
        
    gpState->Lock ();
    gpState->DelRef ();
        
    if (pCall != VerifyCall (pCall)) {
        IFDBG(DebugOut (DEBUG_WARN, L"BthReadScanType : ERROR_INTERNAL_ERROR\n"));
        gpState->Unlock ();
        return ERROR_INTERNAL_ERROR;
    }

    if (iRes == ERROR_SUCCESS) {
        if (pCall->fComplete)
            iRes = pCall->iResult;
        else
            iRes = ERROR_TIMEOUT;
    }
    
    if (iRes == ERROR_SUCCESS)
    {
        __try {
            *pScanType = pCall->ucResult;            
        } __except (1) {
            IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanType : ERROR_INVALID_PARAMETER\n"));
            iRes = ERROR_INVALID_PARAMETER;
        }
    }
    
    DeleteCall (pCall);
    
    gpState->Unlock();
    
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadScanType : result = %d\n", iRes));    
    return iRes;   
}

int BthReadPageScanType
(
unsigned char* pPageScanType
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageScanType\n"));
    int iRes = BthReadScanType(pPageScanType, SCAN_PAGE);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadPageScanType : result = %d\n", iRes));
    return iRes;
}

int BthReadInquiryScanType
(
unsigned char* pInquiryScanType
) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadInquiryScanType\n"));
    int iRes = BthReadScanType(pInquiryScanType, SCAN_INQUIRY);
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"BthReadInquiryScanType : result = %d\n", iRes));
    return iRes;
}


//
// BT Notification APIs
//

HANDLE RequestBluetoothNotifications
(
DWORD dwClass,
HANDLE hMsgQ
) {
    HANDLE hRet = NULL;

    if (! gpState) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return NULL;
    }

    gpState->Lock ();

    MSGQUEUEOPTIONS mqOptions;
    memset (&mqOptions, 0, sizeof(mqOptions));
    mqOptions.dwSize = sizeof(mqOptions);
    mqOptions.bReadAccess = FALSE;
    mqOptions.dwMaxMessages = 10;
    mqOptions.cbMaxMessage = sizeof(BTEVENT);
    mqOptions.dwFlags = 0;

    HANDLE hProc = GetCallerProcess();

    HANDLE hMsgQWrite = OpenMsgQueue (hProc, hMsgQ, &mqOptions);
    if (hMsgQWrite) {
        NotifyCaller* pCaller = new NotifyCaller;
        if (pCaller) {
            pCaller->id = (HANDLE) btutil_AllocHandle (pCaller);
            if (SVSUTIL_HANDLE_INVALID != pCaller->id) {
                pCaller->dwClass = dwClass;
                pCaller->hMsgQ = hMsgQWrite;
                pCaller->hProc = hProc;

                // Add to the front of the list
                pCaller->pNext = gpState->pNotifyCallers;
                gpState->pNotifyCallers = pCaller;

                hRet = pCaller->id; // Set return value
            } else {
                IFDBG(DebugOut (DEBUG_ERROR, L"RequestBluetoothNotifications : Error creating unique handle.\n"));

                CloseMsgQueue (hMsgQWrite);
                delete pCaller;

                SetLastError (ERROR_OUTOFMEMORY);
            }
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"RequestBluetoothNotifications : Error allocating NotifyCaller struct.\n"));

            CloseMsgQueue (hMsgQWrite);

            SetLastError (ERROR_OUTOFMEMORY);
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"RequestBluetoothNotifications : Error opening message queue.\n"));
        SetLastError (ERROR_OUTOFMEMORY);
    }

    gpState->Unlock ();

    return hRet;
}

BOOL StopBluetoothNotifications
(
HANDLE h
) {
    BOOL fRet = FALSE;

    if (! gpState) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    gpState->Lock ();

    NotifyCaller* pPrev = NULL;
    NotifyCaller* pCurr = gpState->pNotifyCallers;

    while (pCurr) {
        if (pCurr->id == h) {
            if (pPrev)
                pPrev->pNext = pCurr->pNext;
            else
                gpState->pNotifyCallers = pCurr->pNext;

            delete pCurr; // destructor will clean up data

            fRet = TRUE;
            break;
        }

        pPrev = pCurr;
        pCurr = pCurr->pNext;
    }

    gpState->Unlock ();

    if (! fRet)
        SetLastError (ERROR_NOT_FOUND);

    return fRet;
}

int BthNotifyEvent 
(
PBTEVENT pbtEvent, 
DWORD dwEventClass
) {
    if (! gpState)
        return ERROR_SERVICE_NOT_ACTIVE;

    gpState->Lock ();

    NotifyEvent(pbtEvent, dwEventClass);

    gpState->Unlock ();

    return ERROR_SUCCESS;
}


//
//  RPC, server startup & debug stuff
//

#if defined (USE_RPC)
extern "C" RPC_IF_HANDLE bluetooth_v1_0_s_ifspec;

extern "C" void __RPC_FAR * __RPC_API midl_user_allocate (size_t len) {
    return malloc(len);
}

extern "C" void __RPC_API midl_user_free (void * __RPC_FAR ptr) {
    free (ptr);
}

//extern "C" void server_ShutDown (void) {
//  IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"server_Shutdown\n"));
//  RpcMgmtStopServerListening (NULL);
//}

static void do_rpc (void) {
    WCHAR   *pszProtocolSequence = L"ncalrpc";
    WCHAR   *pszSecurity         = NULL;
    WCHAR   *pszEndpoint         = L"bt_server";
    UINT    cMinCalls            = 1;
    UINT    cMaxCalls            = 20;

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Starting Bluetooth RPC server\n"));

    RpcServerUseProtseqEp (pszProtocolSequence, cMaxCalls, pszEndpoint, pszSecurity);

    RpcServerRegisterIf (bluetooth_v1_0_s_ifspec, NULL, NULL);
    RpcServerListen (cMinCalls, cMaxCalls, TRUE);

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Blocking for calls now...\n"));

    RpcMgmtWaitServerListen ();

    RpcServerUnregisterIf (NULL, NULL, FALSE);

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Unblocking and exiting...\n"));
}

//
//  L2CAP APIs must be statically linked.
//
int l2capdev_CreateDriverInstance (void);
int l2capdev_CloseDriverInstance (void);

extern "C" int wmain (int argc, WCHAR **argv) {
    bth_InitializeOnce ();

    if (argc > 2) {
        DWORD dwBaud = _wtoi(argv[2]);
        if ((dwBaud == 0) || (dwBaud > 1000000)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"Incorrect baud rate %d\n", dwBaud));
            return 1;
        }

        if ((wcslen (argv[1]) != 5) || (wcsnicmp (argv[1], L"COM", 3) != 0) ||
            (argv[1][4] != ':') || (! iswdigit (argv[1][3]))) {
            IFDBG(DebugOut (DEBUG_ERROR, L"Device name must be COMx: where x is 0-9 (got %s)\n", argv[1]));
            return 1;
        }

        HKEY hk;
        DWORD dwDisp;
        if (ERROR_SUCCESS == RegCreateKeyEx (HKEY_BASE, L"Software\\Microsoft\\Bluetooth\\hci", 0, NULL, 0, KEY_WRITE, NULL, &hk, &dwDisp)) {
            RegSetValueEx (hk, L"port", 0, REG_SZ, (BYTE *)argv[1], (wcslen (argv[1]) + 1) * sizeof(WCHAR));
            RegSetValueEx (hk, L"baud", 0, REG_DWORD, (BYTE *)&dwBaud, sizeof (dwBaud));
            RegCloseKey (hk);
        }
    }

    DebugSetMask (0xffffffff);
    DebugSetOutput (OUTPUT_MODE_CONSOLE);

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Starting Bluetooth server\n"));

    if (StartupBTServices (FALSE) == ERROR_SUCCESS) {
        bth_StartConsole ();
        l2capdev_CreateDriverInstance ();
        do_rpc ();
        l2capdev_CloseDriverInstance ();
        bth_EndConsole ();
    }

    ShutdownBTServices (FALSE);
    bth_UninitializeOnce ();

    return 0;
}

#else

#if ! defined (UNDER_CE)
#error Device driver interface only works under Windows CE
#endif

#include <bt_ioctl.h>

static DWORD WINAPI BthDriverStart (LPVOID lpNull) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Starting Bluetooth server\n"));

    int iErr = StartupBTServices (FALSE);

    if (iErr != ERROR_SUCCESS)
        ShutdownBTServices (FALSE);

    return iErr;
}

static DWORD WINAPI BthDriverReset (LPVOID lpNull) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Resetting Bluetooth server\n"));

    ShutdownBTServices (TRUE);

    Sleep (5000);

    int iErr = StartupBTServices (TRUE);

    if (iErr != ERROR_SUCCESS)
        ShutdownBTServices (TRUE);

    return iErr;
}

static DWORD WINAPI BthDriverStop (LPVOID lpNull) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Stopping Bluetooth server\n"));

    ShutdownBTServices (FALSE);

    return ERROR_SUCCESS;
}

struct CallerContext {
    HANDLE hCallerProc;
    CallerContext* pNext;
    CallerContext* pPrev;
};

CallerContext* g_pCallers;


//
//  Driver service funcs...
//
extern "C" BOOL WINAPI DllMain( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) {
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls ((HMODULE)hInstDll);
            DEBUGREGISTER((HINSTANCE)hInstDll);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      EXECUTION THREAD: Client-application!
//          These functions are only executed on the caller's thread
//          i.e. the thread belongs to the client application
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  @func PVOID | BTD_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc  Returns a DWORD which will be passed to Open & Deinit or NULL if
//          unable to initialize the device.
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice
extern "C" DWORD BTD_Init (DWORD Index) {
#if defined (SDK_BUILD)
    int CheckTime (void);

    if (! CheckTime ())
        return 0;
#endif

    bth_InitializeOnce ();

    HINSTANCE hCoreDll = LoadLibrary (L"coredll.dll");
    gfnSystemTimerReset = (PFN_SYSTEM_TIMER_RESET)GetProcAddress (hCoreDll, L"SystemIdleTimerReset");

    ghBthApiEvent = CreateEvent (NULL, TRUE, FALSE, BTH_NAMEDEVENT_STACK_INITED);

    CloseHandle (CreateThread (NULL, 0, BthDriverStart, NULL, 0, NULL));

    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"-BTD_Init\n"));
    return (DWORD)1;
}

//  @func PVOID | BTD_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from CON_Init call
//  @rdesc  Returns TRUE for success, FALSE for failure.
//  @remark Routine exported by a device driver.  "PRF" is the string
//          passed in as lpszType in RegisterDevice
extern "C" BOOL BTD_Deinit(DWORD dwData) {
    IFDBG(DebugOut (DEBUG_SHELL_TRACE, L"+BTD_Deinit\n"));
    bth_EndConsole ();

    HANDLE hThread = CreateThread (NULL, 0, BthDriverStop, NULL, 0, NULL);
    if (hThread) {
        WaitForSingleObject (hThread, INFINITE);
        CloseHandle (hThread);
    }

    CloseHandle (ghBthApiEvent);

    bth_UninitializeOnce ();
    return TRUE;
}

//  @func PVOID | BTD_Open      | Device open routine
//  @parm DWORD | dwData        | value returned from CON_Init call
//  @parm DWORD | dwAccess      | requested access (combination of GENERIC_READ
//                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc  Returns a DWORD which will be passed to Read, Write, etc or NULL if
//          unable to open device.
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice
extern "C" DWORD BTD_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
    CallerContext* pCaller = (CallerContext*)g_funcAlloc (sizeof(CallerContext), g_pvAllocData);

    // Add caller to the list

    if (pCaller) {
        pCaller->hCallerProc = 0;
        pCaller->pNext = g_pCallers;
        pCaller->pPrev = NULL;
        if (pCaller->pNext)
            pCaller->pNext->pPrev = pCaller;
        g_pCallers = pCaller;
    }
    
    return (DWORD)pCaller;
}

//  @func BOOL | BTD_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from BTD_Open call
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice
extern "C" BOOL BTD_Close (DWORD dwData)  {
    // dwData points to context allocated in BTD_Open.  This pointer is valid.
    CallerContext* pCaller = (CallerContext*)dwData;

    if (pCaller) {
        BOOL fCleanup = TRUE;

        // Loop through the list of callers and see if any other caller has the
        // same process.  If it is the same process, we should not clean up yet.
        // Need to wait until all handles for a process are gone.
        
        CallerContext* pIter = g_pCallers;
        while (pIter) {
            if ((pIter != pCaller) && (pIter->hCallerProc == pCaller->hCallerProc)) {
                fCleanup = FALSE; // Do not clean up yet
                break;
            }
            pIter = pIter->pNext;
        }

        if (fCleanup && pCaller->hCallerProc)
            ProcessExited (pCaller->hCallerProc);

        // Remove the caller from the list
        if (pCaller->pNext) {
            pCaller->pNext->pPrev = pCaller->pPrev;
        }
        if (pCaller->pPrev) {
            pCaller->pPrev->pNext = pCaller->pNext;
        }
        if (pCaller == g_pCallers) {
            g_pCallers = pCaller->pNext;
        }

        g_funcFree (pCaller, g_pvFreeData);
    }

    return TRUE;
}

//  @func DWORD | BTD_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc  Returns -1 for error, otherwise the number of bytes written.  The
//          length returned is guaranteed to be the length requested unless an
//          error condition occurs.
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice
//
extern "C" DWORD BTD_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
    return -1;
}

//  @func DWORD | BTD_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc  Returns 0 for end of file, -1 for error, otherwise the number of
//          bytes read.  The length returned is guaranteed to be the length
//          requested unless end of file or an error condition occurs.
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice
//
extern "C" DWORD BTD_Read (DWORD dwData, LPVOID pBuf, DWORD Len) {
    return -1;
}

//  @func DWORD | BTD_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc  Returns current position relative to start of file, or -1 on error
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//       in as lpszType in RegisterDevice

extern "C" DWORD BTD_Seek (DWORD dwData, long pos, DWORD type) {
    return (DWORD)-1;
}

//  @func void | BTD_PowerUp | Device powerup routine
//  @comm   Called to restore device from suspend mode.  You cannot call any
//          routines aside from those in your dll in this call.
extern "C" void BTD_PowerUp (void) {
    return;
}
//  @func void | BTD_PowerDown | Device powerdown routine
//  @comm   Called to suspend device.  You cannot call any routines aside from
//          those in your dll in this call.
extern "C" void BTD_PowerDown (void) {
    return;
}

//  @func BOOL | BTD_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//      in as lpszType in RegisterDevice
extern "C" BOOL BTD_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
#if ! defined (SDK_BUILD)
    if (dwCode == IOCTL_PSL_NOTIFY) {
        HANDLE hProc = NULL;
        PDEVICE_PSL_NOTIFY pPslPacket = (PDEVICE_PSL_NOTIFY)pBufIn;
        __try {
            if ((pPslPacket->dwSize == sizeof(DEVICE_PSL_NOTIFY)) && (pPslPacket->dwFlags == DLL_PROCESS_EXITING)){
                SVSUTIL_ASSERT (*(HANDLE *)dwData == pPslPacket->hProc);
                hProc = (HANDLE)pPslPacket->hProc;
            }
        } __except (1) {
            SetLastError (ERROR_INVALID_USER_BUFFER);
            return FALSE;
        }

        if (hProc)
            ProcessExited (hProc);

        return TRUE;
    }
#endif

    // dwData points to context allocated in BTD_Open.  This pointer is valid.
    CallerContext* pCaller = (CallerContext *)dwData;
    SVSUTIL_ASSERT (pCaller);

    if (! pCaller->hCallerProc)
        pCaller->hCallerProc = GetCallerProcess ();

    SVSUTIL_ASSERT (pCaller->hCallerProc == GetCallerProcess());

    int iError = ERROR_SUCCESS;

    if (dwCode == IOCTL_SERVICE_STOP) {
        int fHardwareOnly = FALSE;
        if (dwLenIn != 0) {
            if ((dwLenIn != sizeof(L"card")) || (wcsicmp ((WCHAR *)pBufIn, L"card") != 0))
                return ERROR_INVALID_PARAMETER;
            fHardwareOnly = TRUE;
        }

        iError = ShutdownBTServices (fHardwareOnly);
        if (iError != ERROR_SUCCESS) {
            SetLastError(iError);
            return FALSE;
        }
        return TRUE;
    }

    if (dwCode == IOCTL_SERVICE_START) {
        int fHardwareOnly = FALSE;
        if (dwLenIn != 0) {
            if ((dwLenIn != sizeof(L"card")) || (wcsicmp ((WCHAR *)pBufIn, L"card") != 0))
                return ERROR_INVALID_PARAMETER;
            fHardwareOnly = TRUE;
        }

        iError = StartupBTServices (fHardwareOnly);
        if (iError != ERROR_SUCCESS) {
            SetLastError(iError);
            return FALSE;
        }
        return TRUE;
    }

    if (dwCode == IOCTL_SERVICE_REFRESH) {
        iError = BthDriverReset (NULL);
        if (iError != ERROR_SUCCESS) {
            SetLastError(iError);
            return FALSE;
        }
        return TRUE;
    }

    if (dwCode == IOCTL_SERVICE_CONSOLE) {
        int fConsole = TRUE;
        if (dwLenIn != 0) {
            if ((dwLenIn == sizeof(L"on")) && (wcsicmp ((WCHAR *)pBufIn, L"on") == 0))
                fConsole = TRUE;
            else if ((dwLenIn == sizeof(L"off")) && (wcsicmp ((WCHAR *)pBufIn, L"off") == 0))
                fConsole = FALSE;
            else {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }

        if (fConsole)
            bth_StartConsole ();
        else
            bth_EndConsole ();

        return TRUE;
    }

    if (dwCode == IOCTL_SERVICE_DEBUG) {
        if ((dwLenIn == sizeof(L"console")) && (wcsicmp ((WCHAR *)pBufIn, L"console") == 0))
            DebugSetOutput (OUTPUT_MODE_CONSOLE);
        else if ((dwLenIn == sizeof(L"serial")) && (wcsicmp ((WCHAR *)pBufIn, L"serial") == 0))
            DebugSetOutput (OUTPUT_MODE_DEBUG);
        else if ((dwLenIn == sizeof(L"file")) && (wcsicmp ((WCHAR *)pBufIn, L"file") == 0))
            DebugSetOutput (OUTPUT_MODE_FILE);
        else if ((dwLenIn == sizeof(L"reopen")) && (wcsicmp ((WCHAR *)pBufIn, L"reopen") == 0))
            DebugSetOutput (OUTPUT_MODE_REOPEN);
        else if ((dwLenIn == sizeof(L"celog")) && (wcsicmp ((WCHAR *)pBufIn, L"celog") == 0))
            DebugSetOutput (OUTPUT_MODE_CELOG);
        else if ((dwLenIn == sizeof(L"memtotal")) && (wcsicmp ((WCHAR *)pBufIn, L"memtotal") == 0))
            svsutil_TotalAlloc ();
        else if ((dwLenIn == sizeof(L"memdump")) && (wcsicmp ((WCHAR *)pBufIn, L"memdump") == 0))
            svsutil_LogCallStack ();
        else if (dwLenIn == sizeof(DWORD))
            DebugSetMask (*(unsigned int *)pBufIn);
        else {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        return TRUE;
    }

    BTAPICALL *pbapi = (BTAPICALL *)pBufIn;
    if (dwLenIn != sizeof(BTAPICALL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    BTAPICALL bApiTmp;
    if (! CeSafeCopyMemory(&bApiTmp, pbapi, sizeof(bApiTmp))) {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    BOOL fSetLastErr = TRUE;

    switch (dwCode) {
    case BT_IOCTL_BthWriteScanEnableMask:
        // This needs to be trusted so OEMs can enforce that their device cannot
        // be made visible (by untrusted apps)
        if (CeGetCallerTrust() != OEM_CERTIFY_TRUST) {
            iError = ERROR_ACCESS_DENIED;
        } else {        
            iError = BthWriteScanEnableMask (bApiTmp.BthScanEnableMask_p.mask);
        }
        break;

    case BT_IOCTL_BthReadScanEnableMask:
        iError = BthReadScanEnableMask (&bApiTmp.BthScanEnableMask_p.mask);
        break;

    case BT_IOCTL_BthWriteAuthenticationEnable:
        iError = BthWriteAuthenticationEnable (bApiTmp.BthAuthenticationEnable_p.ae);
        break;

    case BT_IOCTL_BthReadAuthenticationEnable:
        iError = BthReadAuthenticationEnable (&bApiTmp.BthAuthenticationEnable_p.ae);
        break;

    case BT_IOCTL_BthWriteLinkPolicySettings:
        iError = BthWriteLinkPolicySettings (&bApiTmp.BthLinkPolicySettings_p.bt, bApiTmp.BthLinkPolicySettings_p.lps);
        break;

    case BT_IOCTL_BthReadLinkPolicySettings:
        iError = BthReadLinkPolicySettings (&bApiTmp.BthLinkPolicySettings_p.bt, &bApiTmp.BthLinkPolicySettings_p.lps);
        break;

    case BT_IOCTL_BthEnterHoldMode:
        iError = BthEnterHoldMode (&bApiTmp.BthHold_p.ba, bApiTmp.BthHold_p.hold_mode_max, bApiTmp.BthHold_p.hold_mode_min, &bApiTmp.BthHold_p.interval);
        break;

    case BT_IOCTL_BthEnterSniffMode:
        iError = BthEnterSniffMode (&bApiTmp.BthSniff_p.ba, bApiTmp.BthSniff_p.sniff_mode_max, bApiTmp.BthSniff_p.sniff_mode_min,
            bApiTmp.BthSniff_p.sniff_attempt, bApiTmp.BthSniff_p.sniff_timeout, &bApiTmp.BthSniff_p.interval);
        break;

    case BT_IOCTL_BthExitSniffMode:
        iError = BthExitSniffMode (&bApiTmp.BthSniff_p.ba);
        break;

    case BT_IOCTL_BthEnterParkMode:
        iError = BthEnterParkMode (&bApiTmp.BthPark_p.ba, bApiTmp.BthPark_p.beacon_max, bApiTmp.BthPark_p.beacon_min, &bApiTmp.BthPark_p.interval);
        break;

    case BT_IOCTL_BthExitParkMode:
        iError = BthExitParkMode (&bApiTmp.BthPark_p.ba);
        break;

    case BT_IOCTL_BthGetBasebandHandles:
        iError = BthGetBasebandHandles (bApiTmp.BthBasebandHandles_p.cMaxHandles,
                                (unsigned short *)MapCallerPtr (bApiTmp.BthBasebandHandles_p.pHandles, bApiTmp.BthBasebandHandles_p.cMaxHandles * sizeof(unsigned short)),
                                &bApiTmp.BthBasebandHandles_p.cHandlesReturned);
        break;

    case BT_IOCTL_BthGetBasebandConnections:
        iError = BthGetBasebandConnections (bApiTmp.BthBasebandConnections_p.cMaxConnections,
                                (PBASEBAND_CONNECTION_DATA)MapCallerPtr (bApiTmp.BthBasebandConnections_p.pConnections, bApiTmp.BthBasebandConnections_p.cMaxConnections * sizeof(BASEBAND_CONNECTION_DATA)),
                                &bApiTmp.BthBasebandConnections_p.cConnectionsReturned);
        break;

    case BT_IOCTL_BthGetCurrentMode:
        iError = BthGetCurrentMode (&bApiTmp.BthCurrentMode_p.bt, &bApiTmp.BthCurrentMode_p.mode);
        break;

    case BT_IOCTL_BthWriteCOD:
        iError = BthWriteCOD (bApiTmp.BthCOD_p.cod);
        break;

    case BT_IOCTL_BthReadCOD:
        iError = BthReadCOD (&bApiTmp.BthCOD_p.cod);
        break;

    case BT_IOCTL_BthGetRemoteCOD:
        iError = BthGetRemoteCOD (&bApiTmp.BthRemoteCOD_p.ba, &bApiTmp.BthRemoteCOD_p.cod);
        break;

    case BT_IOCTL_BthPerformInquiry:
        iError = BthPerformInquiry (bApiTmp.BthPerformInquiry_p.LAP, bApiTmp.BthPerformInquiry_p.length,
                    bApiTmp.BthPerformInquiry_p.num_responses, bApiTmp.BthPerformInquiry_p.cBuffer,
                    (unsigned int *)MapCallerPtr (bApiTmp.BthPerformInquiry_p.pcDiscoveredDevices, sizeof(unsigned int)),
                    (BthInquiryResult *)MapCallerPtr (bApiTmp.BthPerformInquiry_p.inquiry_list, bApiTmp.BthPerformInquiry_p.cBuffer));
        break;

    case BT_IOCTL_BthCancelInquiry:
        iError = BthCancelInquiry ();
        break;

    case BT_IOCTL_BthRemoteNameQuery:
        iError = BthRemoteNameQuery (&bApiTmp.BthRemoteNameQuery_p.ba, bApiTmp.BthRemoteNameQuery_p.cBuffer,
                    (unsigned int *)MapCallerPtr (bApiTmp.BthRemoteNameQuery_p.pcRequired, sizeof(unsigned int)),
                    (WCHAR *)MapCallerPtr (bApiTmp.BthRemoteNameQuery_p.pszString, bApiTmp.BthRemoteNameQuery_p.cBuffer));
        break;

    case BT_IOCTL_BthReadLocalAddr:
        iError = BthReadLocalAddr (&bApiTmp.BthReadLocalAddr_p.ba);
        break;

    case BT_IOCTL_BthWritePageTimeout:
        iError = BthWritePageTimeout (bApiTmp.BthPageTimeout_p.timeout);
        break;

    case BT_IOCTL_BthReadPageTimeout:
        iError = BthReadPageTimeout (&bApiTmp.BthPageTimeout_p.timeout);
        break;

    case BT_IOCTL_BthReadLocalVersion:
        iError = BthReadLocalVersion (&bApiTmp.BthReadVersion_p.hci_version, &bApiTmp.BthReadVersion_p.hci_revision,
            &bApiTmp.BthReadVersion_p.lmp_version, &bApiTmp.BthReadVersion_p.lmp_subversion, &bApiTmp.BthReadVersion_p.manufacturer_name,
            bApiTmp.BthReadVersion_p.lmp_features);
        break;

    case BT_IOCTL_BthReadRemoteVersion:
        iError = BthReadRemoteVersion (&bApiTmp.BthReadVersion_p.bt,
            &bApiTmp.BthReadVersion_p.lmp_version, &bApiTmp.BthReadVersion_p.lmp_subversion, &bApiTmp.BthReadVersion_p.manufacturer_name,
            bApiTmp.BthReadVersion_p.lmp_features);
        break;

    case BT_IOCTL_BthGetHardwareStatus:
        iError = BthGetHardwareStatus (&bApiTmp.BthGetHardwareStatus_p.iHwStatus);
        break;

    case BT_IOCTL_BthTerminateIdleConnections:
        iError = BthTerminateIdleConnections ();
        break;

    case BT_IOCTL_BthGetAddress:
        iError = BthGetAddress (bApiTmp.BthGetAddress_p.handle, &bApiTmp.BthGetAddress_p.ba);
        break;

    case BT_IOCTL_BthSetInquiryFilter:
        iError = BthSetInquiryFilter (&bApiTmp.BthGetAddress_p.ba);
        break;

    case BT_IOCTL_BthSetCODInquiryFilter:
        iError = BthSetCODInquiryFilter (bApiTmp.BthSetCODInquiry_p.cod, bApiTmp.BthSetCODInquiry_p.codMask);
        break;

    case BT_IOCTL_BthClearInquiryFilter:
        iError = BthClearInquiryFilter ();
        break;

    case BT_IOCTL_BthSwitchRole:
        iError = BthSwitchRole (&bApiTmp.BthSwitchRole_p.ba, bApiTmp.BthSwitchRole_p.usRole);
        break;

    case BT_IOCTL_BthGetRole:
        iError = BthGetRole (&bApiTmp.BthGetRole_p.ba, &bApiTmp.BthGetRole_p.usRole);
        break;

    case BT_IOCTL_BthReadRSSI:
        iError = BthReadRSSI (&bApiTmp.BthReadRSSI_p.ba, &bApiTmp.BthReadRSSI_p.iRSSI);
        break;

    case BT_IOCTL_BthCreateACLConnection:
        iError = BthCreateACLConnection (&bApiTmp.BthCreateConnection_p.ba, &bApiTmp.BthCreateConnection_p.handle);
        break;

    case BT_IOCTL_BthCreateSCOConnection:
        iError = BthCreateSCOConnection (&bApiTmp.BthCreateConnection_p.ba, &bApiTmp.BthCreateConnection_p.handle);
        break;
    
    case BT_IOCTL_BthCreateSynchronousConnection:
        iError = BthCreateSynchronousConnection (&bApiTmp.BthCreateSynchronousConnection_p.ba, &bApiTmp.BthCreateSynchronousConnection_p.handle, 
            bApiTmp.BthCreateSynchronousConnection_p.txBandwidth, bApiTmp.BthCreateSynchronousConnection_p.rxBandwidth,
            bApiTmp.BthCreateSynchronousConnection_p.maxLatency, bApiTmp.BthCreateSynchronousConnection_p.voiceSetting,
            bApiTmp.BthCreateSynchronousConnection_p.retransmit);
        break;
        
    case BT_IOCTL_BthCloseConnection:
        iError = BthCloseConnection (bApiTmp.BthCloseConnection_p.handle);
        break;

    case BT_IOCTL_BthAcceptSCOConnections:
        iError = BthAcceptSCOConnections (bApiTmp.BthAcceptSCOConnections_p.fAccept);
        break;
        
    case BT_IOCTL_BthAcceptSynchronousConnections:
        iError = BthAcceptSynchronousConnections (bApiTmp.BthAcceptSynchronousConnections_p.fAccept);
        break;
    
    case BT_IOCTL_BthWritePageScanActivity:
        iError = BthWritePageScanActivity (bApiTmp.BthScanActivity_p.scanInterval, bApiTmp.BthScanActivity_p.scanWindow);
        break;
    
    case BT_IOCTL_BthWriteInquiryScanActivity:
        iError = BthWriteInquiryScanActivity (bApiTmp.BthScanActivity_p.scanInterval, bApiTmp.BthScanActivity_p.scanWindow);
        break;
    
    case BT_IOCTL_BthReadPageScanActivity:
        iError = BthReadPageScanActivity (&bApiTmp.BthScanActivity_p.scanInterval, &bApiTmp.BthScanActivity_p.scanWindow);
        break;
    
    case BT_IOCTL_BthReadInquiryScanActivity:
        iError = BthReadInquiryScanActivity (&bApiTmp.BthScanActivity_p.scanInterval, &bApiTmp.BthScanActivity_p.scanWindow);
        break;
    
    case BT_IOCTL_BthWritePageScanType:
        iError = BthWritePageScanType (bApiTmp.BthScanType_p.scanType);
        break;
        
    case BT_IOCTL_BthWriteInquiryScanType:
        iError = BthWriteInquiryScanType (bApiTmp.BthScanType_p.scanType);
        break;
        
    case BT_IOCTL_BthReadPageScanType:
        iError = BthReadPageScanType (&bApiTmp.BthScanType_p.scanType);
        break;
        
    case BT_IOCTL_BthReadInquiryScanType:
        iError = BthReadInquiryScanType (&bApiTmp.BthScanType_p.scanType);
        break;

        //
        //  Security manager
        //
    case BT_IOCTL_BthSetPIN:
        iError = BthSetPIN (&bApiTmp.BthSecurity_p.ba, bApiTmp.BthSecurity_p.cDataLength, bApiTmp.BthSecurity_p.data);
        break;

    case BT_IOCTL_BthRevokePIN:
        iError = BthRevokePIN (&bApiTmp.BthSecurity_p.ba);
        break;

    case BT_IOCTL_BthSetLinkKey:
        iError = BthSetLinkKey (&bApiTmp.BthSecurity_p.ba, bApiTmp.BthSecurity_p.data);
        break;

    case BT_IOCTL_BthGetLinkKey:
        iError = BthGetLinkKey (&bApiTmp.BthSecurity_p.ba, bApiTmp.BthSecurity_p.data);
        break;

    case BT_IOCTL_BthRevokeLinkKey:
        iError = BthRevokeLinkKey (&bApiTmp.BthSecurity_p.ba);
        break;

    case BT_IOCTL_BthAuthenticate:
        iError = BthAuthenticate (&bApiTmp.BthAuthenticate_p.ba);
        break;

    case BT_IOCTL_BthSetEncryption:
        iError = BthSetEncryption (&bApiTmp.BthSetEncryption_p.ba, bApiTmp.BthSetEncryption_p.fOn);
        break;

    case BT_IOCTL_BthSetSecurityUI:
        iError = BthSetSecurityUI (bApiTmp.BthSetSecurityUI_p.hEvent, bApiTmp.BthSetSecurityUI_p.dwStoreTimeout, bApiTmp.BthSetSecurityUI_p.dwProcessTimeout);
        break;

    case BT_IOCTL_BthGetPINRequest:
        iError = BthGetPINRequest (&bApiTmp.BthGetPINRequest_p.ba);
        break;

    case BT_IOCTL_BthRefusePINRequest:
        iError = BthRefusePINRequest (&bApiTmp.BthRefusePINRequest_p.ba);
        break;

    case BT_IOCTL_BthAnswerPairRequest:
        iError = BthAnswerPairRequest (&bApiTmp.BthSecurity_p.ba, bApiTmp.BthSecurity_p.cDataLength, bApiTmp.BthSecurity_p.data);
        break;

    case BT_IOCTL_BthPairRequest:
        iError = BthPairRequest (&bApiTmp.BthSecurity_p.ba, bApiTmp.BthSecurity_p.cDataLength, bApiTmp.BthSecurity_p.data);
        break;

    case BT_IOCTL_BthGetQueuedHCIPacketCount:
        iError = BthGetQueuedHCIPacketCount (&bApiTmp.BthGetQueuedHCIPacketCount_p.ba, &bApiTmp.BthGetQueuedHCIPacketCount_p.cPackets);
        break;

        //
        //  SDP
        //
    case BT_IOCTL_BthNsSetService: {
        LPWSAQUERYSET pWSA = (LPWSAQUERYSET) MapCallerPtr(bApiTmp.BthNsSetService_p.pSet, sizeof(WSAQUERYSET));
        WSAQUERYSET wsa;

        if (! pWSA) {
            iError = ERROR_INVALID_PARAMETER;
            break;
        }

        __try {
            memcpy(&wsa,pWSA,sizeof(wsa));
            wsa.lpBlob = (LPBLOB) MapCallerPtr(pWSA->lpBlob, sizeof(BLOB));
            if (wsa.lpBlob) {
                wsa.lpBlob->pBlobData  = (unsigned char*) MapCallerPtr(wsa.lpBlob->pBlobData, wsa.lpBlob->cbSize);
                BTHNS_SETBLOB *pSetBlob = (BTHNS_SETBLOB *) wsa.lpBlob->pBlobData;
                pSetBlob->pRecordHandle = (ULONG*) MapCallerPtr(pSetBlob->pRecordHandle, sizeof(ULONG));
                pSetBlob->pSdpVersion   = (ULONG*) MapCallerPtr(pSetBlob->pSdpVersion, sizeof(ULONG));
            }
        } __except (1) {
            iError = ERROR_INVALID_USER_BUFFER;
            break;
        }

        iError = BthNsSetService(&wsa,bApiTmp.BthNsSetService_p.op,bApiTmp.BthNsSetService_p.dwFlags);
        fSetLastErr = FALSE;
        }
        break;

    case BT_IOCTL_BthNsLookupServiceBegin: {
        LPWSAQUERYSET pWSA = (LPWSAQUERYSET) MapCallerPtr(bApiTmp.BthNsLookupServiceBegin_p.pQuerySet, sizeof(WSAQUERYSET));
        WSAQUERYSET wsa;

        if (! pWSA) {
            iError = ERROR_INVALID_PARAMETER;
            break;
        }

        __try {
            memcpy(&wsa,pWSA,sizeof(wsa));
            wsa.lpBlob = (LPBLOB) MapCallerPtr(pWSA->lpBlob, sizeof(BLOB));
            if (wsa.lpBlob)
                wsa.lpBlob->pBlobData  = (unsigned char*) MapCallerPtr(wsa.lpBlob->pBlobData, wsa.lpBlob->cbSize);

            wsa.lpServiceClassId = (LPGUID)        MapCallerPtr(pWSA->lpServiceClassId,sizeof(GUID));
            wsa.lpcsaBuffer      = (LPCSADDR_INFO) MapCallerPtr(pWSA->lpcsaBuffer,sizeof(CSADDR_INFO));
            wsa.lpszContext      = (LPWSTR)        MapCallerPtr(pWSA->lpszContext,sizeof(WCHAR));
            if (wsa.lpcsaBuffer)
                wsa.lpcsaBuffer->RemoteAddr.lpSockaddr = (LPSOCKADDR) MapCallerPtr(wsa.lpcsaBuffer->RemoteAddr.lpSockaddr,sizeof(SOCKADDR));
        } __except (1) {
            iError = ERROR_INVALID_USER_BUFFER;
            break;
        }

        iError = BthNsLookupServiceBegin(&wsa,bApiTmp.BthNsLookupServiceBegin_p.dwFlags,&bApiTmp.BthNsLookupServiceBegin_p.hLookup);
        fSetLastErr = FALSE;
        }
        break;

    case BT_IOCTL_BthNsLookupServiceNext:
        iError = BthNsLookupServiceNext(bApiTmp.BthNsLookupServiceNext_p.hLookup,
                                      bApiTmp.BthNsLookupServiceNext_p.dwFlags,
                                      &bApiTmp.BthNsLookupServiceNext_p.dwBufferLength,
                                      (LPWSAQUERYSET) MapCallerPtr(bApiTmp.BthNsLookupServiceNext_p.pResults, sizeof(WSAQUERYSET)));
        fSetLastErr = FALSE;
        break;

    case BT_IOCTL_BthNsLookupServiceEnd:
        iError = BthNsLookupServiceEnd(bApiTmp.BthNsLookupServiceEnd_p.hLookup);
        fSetLastErr = FALSE;
        break;

    //
    // Notification System
    //
    case BT_IOCTL_RequestBluetoothNotifications:
        bApiTmp.RequestBluetoothNotifications_p.hOut =
            RequestBluetoothNotifications (bApiTmp.RequestBluetoothNotifications_p.dwClass,
                                        bApiTmp.RequestBluetoothNotifications_p.hMsgQ);

        if (bApiTmp.RequestBluetoothNotifications_p.hOut)
            iError = ERROR_SUCCESS;
        else
            iError = GetLastError ();
        
        fSetLastErr = FALSE;
        break;

    case BT_IOCTL_StopBluetoothNotifications:
        if (StopBluetoothNotifications (bApiTmp.StopBluetoothNotifications_p.h))
            iError = ERROR_SUCCESS;
        else
            iError = GetLastError ();

        fSetLastErr = FALSE;
        break;

    //
    // PAN
    //
    case BT_IOCTL_BthActivatePAN:
        iError = pan_Activate(bApiTmp.BthActivatePAN_p.fActivate);
        break;

    default:
        IFDBG(DebugOut (DEBUG_WARN, L"Unknown control code %d\n", dwCode));
        iError = ERROR_CALL_NOT_IMPLEMENTED;
    }

    if (! CeSafeCopyMemory(pbapi, &bApiTmp, sizeof(bApiTmp))) {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    if (iError != ERROR_SUCCESS) {
        if (fSetLastErr)
            SetLastError(iError);
        return FALSE;
    }

    return TRUE;
}

#if defined(UNDER_CE) && CE_MAJOR_VER < 0x0003
extern "C" int _isctype(int c, int mask) {
    if ((c < 0) || (c > 0xff))
        return 0;
    return iswctype((wchar_t)c,(wctype_t)mask);
}
#endif //defined(UNDER_CE) && CE_MAJOR_VER < 0x0003

#endif


//
//  Allocation
//
void *NotifyCaller::operator new (size_t iSize) {
    SVSUTIL_ASSERT (gpState->pfmdNotifyCaller);
    SVSUTIL_ASSERT (gpState->IsLocked ());
    SVSUTIL_ASSERT (iSize == sizeof(NotifyCaller));

    void *pRes = svsutil_GetFixed (gpState->pfmdNotifyCaller);

    SVSUTIL_ASSERT (pRes);

    return pRes;
}

void NotifyCaller::operator delete(void *ptr) {
    SVSUTIL_ASSERT (gpState->IsLocked ());
    SVSUTIL_ASSERT (ptr);

    svsutil_FreeFixed (ptr, gpState->pfmdNotifyCaller);
}

