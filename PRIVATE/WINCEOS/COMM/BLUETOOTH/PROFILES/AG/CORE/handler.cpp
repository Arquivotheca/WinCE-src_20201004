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
#include <pmpolicy.h>
#include <initguid.h>
#include <bt_sdp.h>

#include "btagpriv.h"
#include "btagnetwork.h"

#ifndef PREFAST_ASSERT
#define PREFAST_ASSERT(x) ASSERT(x)
#endif

#ifndef BOOLIFY
#define BOOLIFY(e)      (!!(e))
#endif


#define AT_OK       "\r\nOK\r\n"
#define AT_ERROR    "\r\nERROR\r\n"
#define AT_VGS      "\r\n+VGS=%d\r\n"
#define AT_VGM      "\r\n+VGM=%d\r\n"
#define AT_RING     "\r\nRING\r\n"

#define MAX_NETWORK_REQUEST_ITERATIONS  4

void PhoneExtServiceCallback(BOOL fHaveService);

BOOL IsHandsfreeUUID(const GUID* pGuid)
{
    ASSERT(pGuid);
    return !memcmp(pGuid, &HandsfreeServiceClass_UUID, sizeof(*pGuid));
}

void GetNetworkDropFlags(DWORD dwState, DWORD* pdwDropFlags)
{
    // We will drop one call based on state in the following priority: 
    // Outgoing, Active, Offering
    
    if (dwState & NETWORK_FLAGS_STATE_OUTGOING) {
        *pdwDropFlags = NETWORK_FLAGS_DROP_OUTGOING;
    }
    else if (dwState & NETWORK_FLAGS_STATE_ACTIVE) {
        *pdwDropFlags = NETWORK_FLAGS_DROP_ACTIVE;
    }
    else if (dwState & NETWORK_FLAGS_STATE_OFFERING) {
        *pdwDropFlags = NETWORK_FLAGS_DROP_OFFERING;
    }
    else {
        *pdwDropFlags = 0; // no current calls
    }
}

CAGEngine::CAGEngine(void)
{
}


// This method initializes the AG engine
DWORD CAGEngine::Init(CATParser* pParser)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: AG Engine is being initialized.\n"));

    Lock();

    ASSERT(pParser);
    
    m_pParser = pParser;
    m_pTP = NULL;
    m_fExpectHeadsetButton = FALSE;
    m_fAudioConnect = FALSE;
    m_hSco = 0;
    m_hUIThread = NULL;
    m_hHangupEvent = NULL;
    m_fPhoneExtInited = FALSE;
    m_fNetworkInited = FALSE;
    m_fA2DPSuspended = FALSE;
    m_szCLI[0] = 0;
    m_fCancelCloseConnection = FALSE;
    m_CloseCookie = NULL;
    m_SniffCookie = NULL;

    memset(&m_AGProps, 0, sizeof(AG_PROPS));

    m_AGProps.fPCMMode = TRUE;
    m_AGProps.usMicVolume = 8;
    m_AGProps.usSpeakerVolume = 8;
    m_AGProps.fAuth = TRUE;
    m_AGProps.fEncrypt = TRUE;
    m_AGProps.fUnattendedMode = TRUE;
    m_AGProps.fConnectHFOnCall = FALSE;
    m_AGProps.usHFCapability = DEFAULT_AG_CAPABILITY;
    m_AGProps.usPageTimeout = DEFAULT_PAGE_TIMEOUT;
    m_AGProps.ulSniffDelay = DEFAULT_SNIFF_DELAY;
    m_AGProps.usSniffMax = DEFAULT_SNIFF_MAX;
    m_AGProps.usSniffMin = DEFAULT_SNIFF_MIN;    
    m_AGProps.usSniffAttempt = DEFAULT_SNIFF_ATTEMPT;
    m_AGProps.usSniffTimeout = DEFAULT_SNIFF_TO;
    m_AGProps.fUseESCO = TRUE;
    m_AGProps.txBandwidth = DEFAULT_ESCO_TX_BANDWIDTH;
    m_AGProps.rxBandwidth = DEFAULT_ESCO_RX_BANDWIDTH;
    m_AGProps.maxLatency = DEFAULT_ESCO_MAX_LATENCY;
    m_AGProps.voiceSetting = DEFAULT_ESCO_VOICE_SETTING;
    m_AGProps.retransmission = DEFAULT_ESCO_RETRANSMISSION;

    m_pTP = new SVSThreadPool(2);
    if (! m_pTP) {
        Unlock();
        return ERROR_OUTOFMEMORY;
    }

    // This needs to be manual reset since we'll be calling SetEvent when the
    // parser thread is not waiting sometimes.
    m_hHangupEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (! m_hHangupEvent) {
        Unlock();
        return GetLastError();
    }

    DWORD dwErr = BthAGNetworkInit(g_hInstance);
    if (ERROR_SUCCESS != dwErr) {
        // This is not a critical failure, continue to load service.
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Error initializing network module: %d.\n", dwErr));        
    }
    else {
        m_fNetworkInited = TRUE;
    }    

    dwErr = BthAGPhoneExtInit();
    if (ERROR_SUCCESS != dwErr) {
        // This is not a critical failure, continue to load service.
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Error initializing PhoneExt module: %d.\n", dwErr));        
    }
    else {
        m_fPhoneExtInited = TRUE;
    }

    Unlock();
    
    if (ERROR_SUCCESS != BthAGSetServiceCallback(PhoneExtServiceCallback)) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Error setting service callback in PhoneExt.\n"));
    }

    return dwRetVal;
}


// This method deinitializes the AG engine
void CAGEngine::Deinit(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: AG Engine is being de-initialized.\n"));

    Lock();

    if (m_pParser) {
        CloseAudioChannel();
        CloseControlChannel();

        if (m_fPhoneExtInited) {
            BthAGPhoneExtDeinit();
        }

        if (m_fNetworkInited) {
            BthAGNetworkDeinit();
        }

        if (m_hHangupEvent) {
            CloseHandle(m_hHangupEvent);
            m_hHangupEvent = NULL;
        }

        if (m_pTP) {
            delete m_pTP;
            m_pTP = NULL;
        }

        memset(&m_AGProps, 0, sizeof(AG_PROPS));    
        m_pParser = NULL;

        while (GetRefCount() > 1) {
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Sleeping while AGEngine ref count is still active.\n"));
            Unlock();
            Sleep(500);
            Lock();
        }
    }
    
    Unlock();    
}

//
// This function gets the list of HF devices from the registry
//
DWORD GetBTAddrList(AG_DEVICE* pDevices, const USHORT usNumDevices)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    HKEY hk = NULL;
    int dwIdx = 0;
    WCHAR szName[MAX_PATH];
    DWORD cchName = ARRAY_SIZE(szName);

    ASSERT(pDevices && usNumDevices);

    // Clear the current list
    memset(pDevices, 0, usNumDevices*sizeof(pDevices[0]));

    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AUDIO_GATEWAY_DEVICES, 0, 0, &hk);
    if (ERROR_SUCCESS != dwRetVal) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not open registry key for BT Addr: %d.\n", dwRetVal));        
        goto exit;
    }

    // Enumerate devices in registry    
    while (ERROR_SUCCESS == RegEnumKeyEx(hk, dwIdx, szName, &cchName, NULL, NULL, NULL, NULL)) {
        HKEY hkAddr;
        BT_ADDR btAddr = 0;
        GUID service;
        DWORD cdwSize;
        
        DWORD dwErr = RegOpenKeyEx(hk, szName, 0, 0, &hkAddr);
        if (ERROR_SUCCESS != dwErr) {
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not open registry key for BT Addr: %d.\n", dwRetVal));        
            break;
        }
        
        cdwSize = sizeof(btAddr);
        dwErr = RegQueryValueEx(hkAddr, L"Address", NULL, NULL, (PBYTE)&btAddr, &cdwSize);
        if (dwErr != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not query registry value for BT Addr: %d.\n", dwErr));        
            RegCloseKey(hkAddr);
            break;
        }

        cdwSize = sizeof(service);
        dwErr = RegQueryValueEx(hkAddr, L"Service", NULL, NULL, (PBYTE)&service, &cdwSize);
        if (dwErr != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not query registry value for service: %d.\n", dwErr));        
            RegCloseKey(hkAddr);
            break;
        }        

        RegCloseKey(hkAddr);

        WCHAR* p;
        int iDeviceIdx = wcstol(szName, &p, 10);
        if (btAddr != 0 && ((iDeviceIdx >= 1) || (iDeviceIdx <= usNumDevices))) {
            DEBUGMSG(ZONE_OUTPUT, (L"BTAGSVC: Setting btAddr at index %d to %04x%08x.\n", iDeviceIdx, GET_NAP(btAddr), GET_SAP(btAddr))); 
            pDevices[iDeviceIdx-1].bta = btAddr;
            memcpy(&pDevices[iDeviceIdx-1].service, &service, sizeof(GUID));
        }

        cchName = ARRAY_SIZE(szName);
        dwIdx++;        
    }    

    RegCloseKey(hk);

exit:
    
    return dwRetVal;
}


BOOL CAGEngine::FindBTAddrInList(const BT_ADDR btaSearch)
{
    BOOL fRetVal = FALSE;

    Lock();

    GetBTAddrList(m_AGProps.DeviceList, ARRAY_SIZE(m_AGProps.DeviceList)); // Update from registry

    // Loop through list until we find the address
    int i = 0;
    while ((i < ARRAY_SIZE(m_AGProps.DeviceList)) && (btaSearch != m_AGProps.DeviceList[i].bta)) {
        i++;
    }

    if (i < ARRAY_SIZE(m_AGProps.DeviceList)) {
        fRetVal = TRUE;
    }

    Unlock();
    
    return fRetVal;
}



// This method writes the address list back to the registry
// sets the parameter btAddrDefault at the top of the list
// and as the active device.
DWORD CAGEngine::SetBTAddrList(BT_ADDR btAddrDefault, BOOL fHfSupport)
{
    DWORD       dwRetVal    = ERROR_SUCCESS;
    DWORD       dwDis       = 0;
    HKEY        hk          = NULL;

    Lock();

    GetBTAddrList(m_AGProps.DeviceList, ARRAY_SIZE(m_AGProps.DeviceList)); // Update from registry

    m_AGProps.btAddrClient = btAddrDefault;

    // Loop through list until we find the address
    int i = 0;
    while ((i < ARRAY_SIZE(m_AGProps.DeviceList)) && (btAddrDefault != m_AGProps.DeviceList[i].bta)) {
        i++;
    }
    
    if (i > 0) {
        GUID service;
        
        if (i == ARRAY_SIZE(m_AGProps.DeviceList)) {
            // This is a very unlikely scenario.  We get a connection request after pairing
            // with the device but before SDP is done.
            service = fHfSupport ? HandsfreeServiceClass_UUID : HeadsetServiceClass_UUID;
            i--; // decrement so we don't overrun DeviceList array below
        }
        else {
            service = m_AGProps.DeviceList[i].service;
        }

        memmove(&m_AGProps.DeviceList[1], m_AGProps.DeviceList, i*sizeof(AG_DEVICE));
        
        m_AGProps.DeviceList[0].bta = btAddrDefault;
        m_AGProps.DeviceList[0].service = service;
    }

    // Write the list back to the registry
    dwRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE, RK_AUDIO_GATEWAY_DEVICES, 0, NULL, 0, NULL, NULL, &hk, &dwDis);
    if (ERROR_SUCCESS == dwRetVal) {
        int dwIdx = 0;
        DWORD cbName = MAX_PATH;

        while ((dwIdx < ARRAY_SIZE(m_AGProps.DeviceList)) && m_AGProps.DeviceList[dwIdx].bta != 0) {
            HKEY hkAddr;
            WCHAR wszIdx[10];            
            _itow(dwIdx+1, wszIdx, 10);

            dwDis = 0;
            DWORD dwErr = RegCreateKeyEx(hk, wszIdx, 0, NULL, 0, NULL, NULL, &hkAddr, &dwDis);
            if (ERROR_SUCCESS != dwErr) {
                DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not open registry key for BT Addr: %d.\n", dwRetVal));        
                break;
            }
                
            dwErr = RegSetValueEx(hkAddr, L"Address", NULL, REG_BINARY, (PBYTE)&m_AGProps.DeviceList[dwIdx].bta, sizeof(BT_ADDR));
            if (dwErr != ERROR_SUCCESS) {
                RegCloseKey(hkAddr);
                DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not set registry value for BT Addr: %d.\n", dwErr));        
                break;
            }

            dwErr = RegSetValueEx(hkAddr, L"Service", NULL, REG_BINARY, (PBYTE)&m_AGProps.DeviceList[dwIdx].service, sizeof(GUID));
            if (dwErr != ERROR_SUCCESS) {
                RegCloseKey(hkAddr);
                DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Could not set registry value for service: %d.\n", dwErr));        
                break;
            }

            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Saving to registry btAddr %04x%08x at index %d.\n", GET_NAP(m_AGProps.DeviceList[dwIdx].bta), GET_SAP(m_AGProps.DeviceList[dwIdx].bta), dwIdx+1));

            RegCloseKey(hkAddr);
            
            dwIdx++;        
        }

        RegCloseKey (hk);
    }

    Unlock();

    return dwRetVal;
}

// This method notifies the AG Engine that a connection has been made with a peer
DWORD CAGEngine::NotifyConnect(SOCKET sockClient, BOOL fHFSupport)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    Lock();
    
    if (m_AGProps.state >= AG_STATE_CONNECTING) {
        dwRetVal = ERROR_ALREADY_INITIALIZED;
        goto exit;
    }

    m_AGProps.state = AG_STATE_CONNECTING;

    if (m_pParser) {
        dwRetVal = m_pParser->Start(this, sockClient);        
    }
    else {
        dwRetVal = ERROR_NOT_READY;
    }
    if (ERROR_SUCCESS != dwRetVal) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error starting parser module: %d\n", dwRetVal)); 
        goto exit;
    }

    ASSERT(INVALID_SOCKET != sockClient);
    
    m_sockClient = sockClient;

    AddRef();
    Unlock();
    
    BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_CTRL, 1, NULL);

    Lock();
    DelRef();   

    m_fAudioConnect = FALSE;

    m_AGProps.fHandsfree = fHFSupport;    
    
    if (!m_AGProps.fHandsfree) {
        // For headset devices we will expect to see the CKPD
        // command once.  If we are in a call, open SCO.
        m_fExpectHeadsetButton = TRUE;
        m_AGProps.state = AG_STATE_CONNECTED;

        if (m_AGProps.fInCall) {
            // If we are in a call, connect SCO
            OpenAudioChannel();
        }
    }
   
exit:
    if (ERROR_SUCCESS != dwRetVal) {
        CloseControlChannel();
    }
    
    Unlock();
    return dwRetVal;
}


// This method is called when a disconnection event occurs
void CAGEngine::DisconnectionEvent(unsigned short connection_handle)
{
    Lock();    
    if (m_hSco == connection_handle) {
        ClearSCO();
    }    
    Unlock();   
}


// This method notifies the AG Engine that a SCO connection has been made with a peer
DWORD CAGEngine::NotifySCOConnect(unsigned short handle)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: ++NotifySCOConnect.\n"));
    
    Lock();
    
    if (m_AGProps.state >= AG_STATE_AUDIO_UP) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: NotifySCOConnect - Audio connection already opened.\n"));
        m_AGProps.state = AG_STATE_AUDIO_UP;
        dwRetVal = ERROR_ALREADY_INITIALIZED;
        goto exit;
    }

    if (m_AGProps.state < AG_STATE_CONNECTED) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: NotifySCOConnect - Cannot open audio connection until service-level connection has been made.\n"));
        dwRetVal = ERROR_NOT_READY;
        goto exit;        
    }

    // TODO: See if we have a built-in audio driver and do switching.
    UINT numDevs = waveOutGetNumDevs();
    for (UINT i = 0; i < numDevs; i++) {
        waveOutMessage((HWAVEOUT)i, WODM_BT_SCO_AUDIO_CONTROL, 0, TRUE);
    }

    if (m_AGProps.fPCMMode) {
        m_hSco = handle;
    }

    EnsureActiveBaseband();

    m_AGProps.state = AG_STATE_AUDIO_UP;

    AddRef();
    Unlock();
    
    BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_AUDIO, 1, NULL);

    Lock();
    DelRef();

exit:
    Unlock();
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --NotifySCOConnect.\n"));
    
    return dwRetVal;
}


// This method informs the AG that no SCO connections to the device are present.  We need to
// make sure that audio state is no longer up.
void CAGEngine::ClearSCO(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: ++ClearSCO.\n"));

    BOOL fClearSCO = FALSE;
    
    ASSERT(IsLocked());
    
    if (AG_STATE_AUDIO_UP == m_AGProps.state) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: SCO connection was closed.\n"));
        m_AGProps.state = AG_STATE_CONNECTED;
        fClearSCO = TRUE;
    }
    else if (AG_STATE_RINGING_AUDIO_UP == m_AGProps.state) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: SCO connection was closed.\n"));
        m_AGProps.state = AG_STATE_RINGING;
        fClearSCO = TRUE;
    }

    if (fClearSCO) {
        UINT numDevs = waveOutGetNumDevs();
        for (UINT i = 0; i < numDevs; i++) {
            waveOutMessage((HWAVEOUT)i, WODM_BT_SCO_AUDIO_CONTROL, 0, FALSE);
        }

        // TODO: See if we have a built-in audio driver and do switching.
        
        m_hSco = 0;

        // Restart A2DP if it was previously suspended
        SendA2DPStart();

        AddRef();
        Unlock();

        BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_AUDIO, 0, NULL);

        Lock();
        DelRef();  

        StartSniffModeTimer();
    }
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --ClearSCO.\n"));
}


// This method instructs the AG Engine to open a connection
DWORD CAGEngine::OpenAGConnection(BOOL fOpenAudio, BOOL fFirstOnly)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Opening AG connection.\n"));

    Lock();

    m_fAudioConnect = fOpenAudio;
    dwRetVal = OpenControlChannel(fFirstOnly, FALSE);
    
    Unlock();
    
    return dwRetVal;
}


// This private method opens control connection to peer
DWORD CAGEngine::OpenControlChannel(BOOL fFirstOnly, BOOL fConnectHSOnly)
{
    DWORD           dwRetVal = ERROR_SUCCESS;
    SOCKADDR_BTH    sa;
    DWORD           dwDeviceIdx = 0;
    USHORT          usPageTimeout;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Opening control connection.\n"));

    ASSERT(IsLocked());
    
    if (m_AGProps.state >= AG_STATE_CONNECTING) {
        // Make sure we are not timing out the connection
        m_pTP->UnScheduleEvent(m_CloseCookie);
        m_CloseCookie = NULL;
        m_fCancelCloseConnection = FALSE;

        // If we are in connected state and m_fAudioConnect is set then
        // let's connect audio now.  If we are still in CONNECTING state
        // then do nothing now as it will get connected later.

        if (m_fAudioConnect && (m_AGProps.state >= AG_STATE_CONNECTED)) {
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Already connected, just opening audio connection.\n")); 
            m_fAudioConnect = FALSE;

            // Ignore error code here.  If SCO fails to open we should keep the
            // ACL link up.  Also, we must consider the scenario where the HF
            // opens the audio link at the same time as us.  Let's let this fail
            // gracefully and hope the HF is in the process of opening SCO.
            OpenAudioChannel();
        }
        else {
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: OpenControlConnection returning ERROR_ALREADY_INITIALIZED\n")); 
            dwRetVal = ERROR_ALREADY_INITIALIZED;
        }
        
        goto exit;
    }

    usPageTimeout = m_AGProps.usPageTimeout;

    (void) BthWritePageTimeout(usPageTimeout); // if this fails, we continue anyway    

    m_sockClient = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (INVALID_SOCKET == m_sockClient) {
        dwRetVal = GetLastError();
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error opening socket: %d\n", dwRetVal)); 
        goto exit;
    }

    // Set AG MTU to be RFCOMM default
    DWORD dwMTU = AG_MTU; 
    if (SOCKET_ERROR == setsockopt(m_sockClient, SOL_RFCOMM, SO_BTH_SET_MTU, (char *)&dwMTU, sizeof(dwMTU))) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Error setting MTU: %d\n", GetLastError()));
    }

    (void) GetBTAddrList(m_AGProps.DeviceList, ARRAY_SIZE(m_AGProps.DeviceList)); // if this fails, we continue anyway

    if (m_AGProps.DeviceList[0].bta == 0) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Tried to open a connection but no device has been paired yet.\n"));
        dwRetVal = ERROR_NOT_READY;
        goto exit;
    }

    while (dwDeviceIdx < ARRAY_SIZE(m_AGProps.DeviceList)) {
        if (m_AGProps.DeviceList[dwDeviceIdx].bta == 0) {
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Device with index %d does not exist, failed to connect.\n", dwDeviceIdx));
            dwRetVal = ERROR_NOT_READY;
            goto exit;
        }

        BOOL fHS = m_AGProps.fNoHandsfree || !IsHandsfreeUUID(&(m_AGProps.DeviceList[dwDeviceIdx].service));
        
        BOOL fAttemptConnect = FALSE;
        
        memset(&sa, 0, sizeof(sa));
        sa.addressFamily = AF_BTH;
        sa.btAddr = m_AGProps.DeviceList[dwDeviceIdx].bta;

        if (fHS) {
            // Try to connect to a headset
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: OpenControlConnection - Attempting to connect to headset device at index %d.\n", dwDeviceIdx));
            sa.serviceClassId = HeadsetServiceClass_UUID;
            fAttemptConnect = TRUE;
        }
        else if (! fConnectHSOnly) {
            // Try to connect to a hands-free
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: OpenControlConnection - Attempting to connect to HF device at index %d.\n", dwDeviceIdx));
            sa.serviceClassId = HandsfreeServiceClass_UUID;
            fAttemptConnect = TRUE;
        }

        int sockErr = SOCKET_ERROR;
        if (fAttemptConnect) {
            sockErr = connect(m_sockClient, (SOCKADDR *)&sa, sizeof(sa));
        }
        
        if (SOCKET_ERROR == sockErr) {
            if (fFirstOnly) {
                dwRetVal = ERROR_NOT_CONNECTED;
                goto exit;
            }
            else {
                dwDeviceIdx++;
                continue;
            }
        }

        if (fHS) {
            m_AGProps.fHandsfree = FALSE;
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Connected to a headset device.\n"));
        }
        else {
            m_AGProps.fHandsfree = TRUE;
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Connected to a hands-free device.\n"));
        }
  
        // If we get to here we are connected
        SetBTAddrList(m_AGProps.DeviceList[dwDeviceIdx].bta, m_AGProps.fHandsfree);
        break;
    }

    if (dwDeviceIdx == ARRAY_SIZE(m_AGProps.DeviceList)) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error: Could not connect to any devices.\n"));
        dwRetVal = ERROR_NOT_CONNECTED;
        goto exit;
    }

    if (m_AGProps.fHandsfree) {
        m_AGProps.state = AG_STATE_CONNECTING;
    }
    else {
        m_fExpectHeadsetButton = FALSE;
        m_AGProps.state = AG_STATE_CONNECTED;
    }


    if (m_pParser) {
        dwRetVal = m_pParser->Start(this, m_sockClient);
    }
    else {
        dwRetVal = ERROR_NOT_READY;
    }
    if (ERROR_SUCCESS != dwRetVal) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error starting parser module: %d\n", dwRetVal));
        goto exit;
    }

    AddRef();
    Unlock();

    BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_CTRL, 1, NULL);

    Lock();
    DelRef();

    if (m_fAudioConnect && !m_AGProps.fHandsfree) {
        // Open audio connection
        m_fAudioConnect = FALSE;

        // Ignore error code here.  If SCO fails to open we should keep the
        // ACL link up.  Also, we must consider the scenario where the HF
        // opens the audio link at the same time as us.  Let's let this fail
        // gracefully and hope the HF is in the process of opening SCO.
        OpenAudioChannel();
    }

exit:
    if ((ERROR_SUCCESS != dwRetVal) && (ERROR_ALREADY_INITIALIZED != dwRetVal)) {
        // We need to close this socket now since the parser component is not
        // yet aware of this socket handle.  In normal scenarios, the call to
        // CloseControlChannel will close the socket.
        closesocket(m_sockClient);
        m_sockClient = INVALID_SOCKET;        
        
        CloseControlChannel();

        m_fAudioConnect = FALSE;
    }
        
    return dwRetVal;
}


// This method closes the connection to the peer device
void CAGEngine::CloseAGConnection(BOOL fCloseControl)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Closing AG connection.\n"));
    
    Lock();
    
    if (fCloseControl || !m_AGProps.fHandsfree) {
        CloseControlChannel();
    }
    else {
        CloseAudioChannel();
    }
    
    Unlock();
}


// This private method closes the control connection to the peer
void CAGEngine::CloseControlChannel(void) 
{
    ASSERT(IsLocked());

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: ++CloseControlChannel\n"));

    // Make sure audio connection is closed and we are unparked
    CloseAudioChannel();
    EnsureActiveBaseband();

    // When control channel is closed we need make sure we are 
    // not in unattended mode by stopping the sniff mode timer.
    StopSniffModeTimer();

    AddRef();
    Unlock();

    if (m_pParser) {
        m_pParser->Stop();
    }
    
    Lock();
    DelRef();

    m_sockClient = INVALID_SOCKET;  // parser will close the socket

    m_AGProps.state = AG_STATE_DISCONNECTED;
    m_AGProps.usRemoteFeatures = 0;
    m_AGProps.fHandsfree = FALSE;
    m_AGProps.fNotifyCallWait = FALSE;
    m_AGProps.fNotifyCLI = FALSE;
    m_AGProps.fIndicatorUpdates = FALSE;
    m_AGProps.btAddrClient = 0;
    m_AGProps.fESCOSupport = FALSE;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --CloseControlChannel\n"));
}


void CAGEngine::ServiceConnectionUp()
{
    //
    // At this point, HF is initialized
    //


    ASSERT(IsLocked());

    if (AG_STATE_CONNECTING == m_AGProps.state) {        
        
        // If we are still in connecting state, we are now connected.
        m_AGProps.state = AG_STATE_CONNECTED;

        if (m_AGProps.fHaveService) {
            SendIndicatorEvent(SERVICE_EVENT, 1);
        }
        if (m_AGProps.fInCall) {
            SendIndicatorEvent(CALL_EVENT, 1);
        }
        
        SendIndicatorEvent(CALL_SETUP_EVENT, m_AGProps.usCallSetup);
        
        if (m_AGProps.fUseESCO) {
            // Determine if the local baseband supports eSCO
            unsigned short hci_subversion, lmp_subversion, manufacturer;
            unsigned char hci_version, lmp_version, lmp_features[8];
            int result = BthReadLocalVersion(&hci_version, &hci_subversion, &lmp_version, &lmp_subversion, &manufacturer, lmp_features);
            if (ERROR_SUCCESS == result) {
                if ((lmp_features[3] & 0x80) ||     // eSCO EV3
                    (lmp_features[4] & 0x01) ||     // eSCO EV4
                    (lmp_features[4] & 0x02)) {     // eSCO EV5
                    m_AGProps.fESCOSupport = TRUE;
                }            
            }
        }

        if (m_fAudioConnect) {
            OpenAudioChannel();
            m_fAudioConnect = FALSE;
        }

        // When the service level connection is up we need to inform the peer
        // if in-band ringing is supported.
        if (m_AGProps.usHFCapability & AG_CAP_INBAND_RING) {
            const char szCommand[] = "\r\n+BSIR: 1\r\n";
            SendATCommand(szCommand);
        }   
    }
}

// This method gets AG properties
void CAGEngine::GetAGProps(PAG_PROPS pAGProps)
{
    PREFAST_ASSERT(pAGProps);

    Lock();
    *pAGProps = m_AGProps;
    Unlock();
}


// This method sets AG properties
void CAGEngine::SetAGProps(PAG_PROPS pAGProps)
{
    ASSERT((pAGProps->usSpeakerVolume >= 0) && (pAGProps->usSpeakerVolume <= 15));
    ASSERT((pAGProps->usMicVolume >= 0) && (pAGProps->usMicVolume <= 15));

    Lock();
    m_AGProps = *pAGProps;
    Unlock();
}    


// This private method opens the audio connection to the peer
DWORD CAGEngine::OpenAudioChannel(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Opening audio connection.\n"));

    ASSERT(IsLocked());
    
    if (m_AGProps.state >= AG_STATE_AUDIO_UP) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Audio connection already opened.\n"));
        m_AGProps.state = AG_STATE_AUDIO_UP;
        dwRetVal = ERROR_ALREADY_INITIALIZED;
        goto exit;
    }

    if (m_AGProps.state < AG_STATE_CONNECTED) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Cannot open audio connection until service-level connection has been made.\n"));
        dwRetVal = ERROR_NOT_READY;
        goto exit;        
    }

    EnsureActiveBaseband();

    // Send an A2DP suspend if it has not already been sent
    SendA2DPSuspend();

    if (m_AGProps.fPCMMode) {
        ASSERT(m_hSco == 0);
        
        if (m_AGProps.fESCOSupport) {                        
            dwRetVal = BthCreateSynchronousConnection(&m_AGProps.btAddrClient, &m_hSco, m_AGProps.txBandwidth, 
                m_AGProps.rxBandwidth, m_AGProps.maxLatency, m_AGProps.voiceSetting, m_AGProps.retransmission);
        }
        
        if (!m_AGProps.fESCOSupport || (ERROR_SUCCESS != dwRetVal)) {
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Synchronous connect failed (%d).  Trying SCO.\n", dwRetVal));
        
            dwRetVal = BthCreateSCOConnection(&m_AGProps.btAddrClient, &m_hSco);
            if (ERROR_SUCCESS != dwRetVal) {
                DEBUGMSG(ZONE_ERROR, (_T("BTAGSVC: Error creating SCO connection: %d\n"), dwRetVal));            
                goto exit;
            }
        }
    }

    // TODO: See if we have a built-in audio driver and do switching.

    UINT numDevs = waveOutGetNumDevs();
    for (UINT i = 0; i < numDevs; i++) {
        waveOutMessage((HWAVEOUT)i, WODM_BT_SCO_AUDIO_CONTROL, 0, TRUE);
    }

    m_AGProps.state = AG_STATE_AUDIO_UP;

    AddRef();
    Unlock();

    BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_AUDIO, 1, NULL);

    Lock();
    DelRef();
    
exit:
    return dwRetVal;
}


// This private method closes the audio connection to the peer
DWORD CAGEngine::CloseAudioChannel(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: ++CloseAudioChannel\n"));

    ASSERT(IsLocked());
    
    if (m_AGProps.state >= AG_STATE_AUDIO_UP) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: CloseAudioChannel - audio is up, closing connection\n"));

        EnsureActiveBaseband();

        UINT numDevs = waveOutGetNumDevs();
        for (UINT i = 0; i < numDevs; i++) {
            waveOutMessage((HWAVEOUT)i, WODM_BT_SCO_AUDIO_CONTROL, 0, FALSE);
        }

        // TODO: See if we have a built-in audio driver and do switching.

        if (m_AGProps.fPCMMode) {
            DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: CloseAudioChannel - Need to close SCO connection in PCM mode\n"));
            
            dwRetVal = BthCloseConnection (m_hSco);
            if (dwRetVal != ERROR_SUCCESS) {
                DEBUGMSG(ZONE_WARN, (_T("BTAGSVC: Error closing SCO connection: %d\n"), dwRetVal));
            }

            m_hSco = 0; 
        }

        // Restart A2DP if it was previously suspended
        SendA2DPStart();
        
        AddRef();
        Unlock();
        
        BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_BT_AUDIO, 0, NULL);

        Lock();
        DelRef();           

        m_AGProps.state = AG_STATE_CONNECTED;

        StartSniffModeTimer();
    }

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --CloseAudioChannel\n"));
    
    return dwRetVal;
}


// This method sets the speaker volume for the peer device
void CAGEngine::SetSpeakerVolume(unsigned short usVolume)
{
    CHAR szBuf[MAX_SEND_BUF];
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Setting the speaker volume to %d\n", usVolume));

    ASSERT(usVolume <= 15);

    Lock();

    m_AGProps.usSpeakerVolume = usVolume;
    if (m_AGProps.state >= AG_STATE_CONNECTING) {
        if (S_OK == StringCchPrintfA(szBuf, MAX_SEND_BUF, AT_VGS, usVolume)) {
            SendATCommand(szBuf);
        }
    }
    
    Unlock();
}


// This method sets the mic volume for the peer device
void CAGEngine::SetMicVolume(unsigned short usVolume)
{
    CHAR szBuf[MAX_SEND_BUF];

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Setting the microphone volume to %d\n", usVolume));

    ASSERT(usVolume <= 15);

    Lock();

    m_AGProps.usMicVolume = usVolume;    
    if (m_AGProps.state >= AG_STATE_CONNECTING) {
        if (S_OK == StringCchPrintfA(szBuf, MAX_SEND_BUF, AT_VGM, usVolume)) {
            SendATCommand(szBuf);
        }
    }

    Unlock();
}


// This method is called when the headset button is pressed
void CAGEngine::OnHeadsetButton(LPSTR pszParams, int cchParam)
{
    DWORD dwErr = ERROR_SUCCESS;
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnHeadsetButton\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);

    SendATCommand(AT_OK);

    if ((AG_STATE_RINGING == m_AGProps.state) || (AG_STATE_RINGING_AUDIO_UP == m_AGProps.state)) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnHeadsetButton - In ringing state, answer call.\n"));

        m_AGProps.fUseHFAudio = TRUE;
        
        AddRef();
        Unlock();
        
        dwErr = BthAGNetworkAnswerCall();
        
        Lock();
        DelRef();

        m_fExpectHeadsetButton = FALSE;

        if (ERROR_SUCCESS != dwErr) {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error answering call: %d\n", dwErr));
            m_AGProps.fUseHFAudio = FALSE;
        }
    }
    else if (! m_fExpectHeadsetButton) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnHeadsetButton - Not expecting headset button, drop/swap call.\n"));

        AddRef();
        Unlock();

        DWORD dwState = 0;
        BthAGNetworkGetCallState(&dwState);

        if ((dwState & NETWORK_FLAGS_STATE_HOLD)    ||   // If bitmask indicates we have calls on hold, do a swap
            ((dwState & NETWORK_FLAGS_STATE_ACTIVE) && 
            (dwState & NETWORK_FLAGS_STATE_OFFERING))    // If we have active call and offering call, do a swap
            ) {
            m_AGProps.fUseHFAudio = TRUE;
            dwErr = BthAGNetworkSwapCall();
        }
        else {
            DWORD dwDropFlags = 0;
            GetNetworkDropFlags(dwState, &dwDropFlags);

            if (dwDropFlags) {
                dwErr = BthAGNetworkDropCall(dwDropFlags);
            }
        }
        
        Lock();
        DelRef();

        if (ERROR_SUCCESS != dwErr) {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error swapping call: %d\n", dwErr));
            m_AGProps.fUseHFAudio = FALSE;
        }
    }
    else {
        // After headset-initiated connection, the HeadsetButton command
        // is expected once.
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnHeadsetButton - Expecting headset button after connect, do nothing.\n"));
        m_fExpectHeadsetButton = FALSE;
    }

    Unlock();
}


// This method is called when the speaker volume on the device changes
void CAGEngine::OnSpeakerVol(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnSpkVol\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    LPSTR pTmp;    
    unsigned short usVolume = (USHORT) strtol(pszParams, &pTmp, 10);
    if (pTmp != (pszParams + strlen(pszParams))) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Speaker volume AT Command was poorly formatted.\n"));
        SendATCommand(AT_ERROR);
    }
    else if (usVolume > 15) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Received speaker volume indication of %d - returning ERROR (out of range).\n", usVolume));
        SendATCommand(AT_ERROR);
    }
    else {
        m_AGProps.usSpeakerVolume = usVolume;
        SendATCommand(AT_OK);
        
        AddRef();
        Unlock();        
        
        BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_SPEAKER_VOLUME, (DWORD)usVolume, NULL);        
        
        Lock();
        DelRef();
    }

    Unlock();
}


// This method is called when the mic volume on the device changes
void CAGEngine::OnMicVol(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnMicVol\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    LPSTR pTmp;    
    unsigned short usVolume = (USHORT) strtol(pszParams, &pTmp, 10);
    if (pTmp != (pszParams + strlen(pszParams))) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Mic volume AT Command was poorly formatted.\n"));
        SendATCommand(AT_ERROR);
    }
    else if (usVolume > 15) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Received microphone volume indication of %d - returning ERROR (out of range).\n", usVolume));
        SendATCommand(AT_ERROR);
    }
    else {
        m_AGProps.usMicVolume = usVolume;
        SendATCommand(AT_OK);
        
        AddRef();
        Unlock();
        
        BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_MIC_VOLUME, (DWORD)usVolume, NULL); 
        
        Lock();
        DelRef();        
    }

    Unlock();
}

// This method sets the flag to ensure HF audio is used with next call (for auto-answer)
void CAGEngine::SetUseHFAudio(BOOL fUseHFAudio)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Setting use handsfree audio mode to %s\n", fUseHFAudio ? L"TRUE" : L"FALSE"));

    Lock();

    m_AGProps.fUseHFAudio = fUseHFAudio;
    
    Unlock();
}

// This method modifies the AG capabilities to enable/disable in-band ring tones.
// It will also enable/disable the feature with a device that is currently connected.
void CAGEngine::SetUseInbandRing(BOOL fUseInbandRing)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Setting use inband ring to %s\n", fUseInbandRing ? L"TRUE" : L"FALSE"));

    Lock();

    if (fUseInbandRing) {
        m_AGProps.usHFCapability |= AG_CAP_INBAND_RING;
        if (m_AGProps.state >= AG_STATE_CONNECTED) {
            const char szCommand[] = "\r\n+BSIR: 1\r\n";
            SendATCommand(szCommand);
        }
    } 
    else {
        m_AGProps.usHFCapability &= ~AG_CAP_INBAND_RING; 
        if (m_AGProps.state >= AG_STATE_CONNECTED) {
            const char szCommand[] = "\r\n+BSIR: 0\r\n";
            SendATCommand(szCommand);
        }
    }

    Unlock();    
}

// This method is called when the peer device dials a number
void CAGEngine::OnDial(LPSTR pszParams, int cchParam)
{
    WCHAR wszNumber[MAX_PHONE_NUMBER];
    DWORD dwErr;

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnDial\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);
    
    SendATCommand(AT_OK);

    if (pszParams[cchParam-1] == ';') {
        pszParams[cchParam-1] = '\0';
    }

    SendIndicatorEvent(CALL_SETUP_EVENT, 2);

    m_AGProps.usCallSetup = 2;

    m_AGProps.fUseHFAudio = TRUE; // Indicate call is initiated from HF

    AddRef();
    Unlock();
    
    if (0 < MultiByteToWideChar(CP_ACP, 0, pszParams, -1, wszNumber, ARRAY_SIZE(wszNumber))) {
        dwErr = BthAGNetworkDialNumber(wszNumber);
    }
    else {
        ASSERT(0);
        dwErr = GetLastError();
    }
    
    Lock();
    DelRef();

    if (ERROR_SUCCESS != dwErr) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error dialing number: %d\n", dwErr));

        if (m_AGProps.usCallSetup) {
            SendIndicatorEvent(CALL_SETUP_EVENT, 0); // Call setup is done
        }
        m_AGProps.usCallSetup = 0;
        m_AGProps.fUseHFAudio = FALSE;
    }

    Unlock();
}


// This method is called when the peer device wants to dial the last dialed number
void CAGEngine::OnDialLast(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnDialLast\n"));
    WCHAR wszNumber[MAX_PHONE_NUMBER];

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);

    AddRef();
    Unlock();
    
    BOOL fDialed = BthAGGetLastDialed(wszNumber);
    
    Lock();
    DelRef();

    if (fDialed) {
        SendATCommand(AT_OK);
        
        SendIndicatorEvent(CALL_SETUP_EVENT, 2); // Call Setup is ongoing

        m_AGProps.usCallSetup = 2;

        m_AGProps.fUseHFAudio = TRUE; // Indicate call is initiated from HF

        AddRef();
        Unlock();        
        
        DWORD dwErr = BthAGNetworkDialNumber(wszNumber);
        
        Lock();
        DelRef();

        if (ERROR_SUCCESS != dwErr) {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error dialing number: %d\n", dwErr));
            if (m_AGProps.usCallSetup) {
                SendIndicatorEvent(CALL_SETUP_EVENT, 0); // call Setup is done.
            }
            m_AGProps.usCallSetup = 0;
            m_AGProps.fUseHFAudio = FALSE;
        }
    } else {
        SendATCommand(AT_ERROR);
    }

    Unlock();
}


// This method is called when the peer device wants to dial a number in memory
void CAGEngine::OnDialMemory(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnDialMemory\n"));
        
    if (pszParams[cchParam-1] == ';') {
        pszParams[cchParam-1] = '\0';
    }

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);

    LPSTR pTmp; 
    unsigned short usIndex = (USHORT) strtol(pszParams, &pTmp, 10);
    if (pTmp != (pszParams + strlen(pszParams))) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: ATD> Command was poorly formatted.\n"));
        SendATCommand(AT_ERROR);
    }
    else {
        WCHAR wszNumber[MAX_PHONE_NUMBER];
        
        AddRef();
        Unlock();

        BOOL fDialed = BthAGGetSpeedDial(usIndex, wszNumber);

        Lock();
        DelRef();
       
        if (fDialed) {
            SendATCommand(AT_OK);
            
            SendIndicatorEvent(CALL_SETUP_EVENT, 2); // Call Setup is ongoing

            m_AGProps.usCallSetup = 2;

            m_AGProps.fUseHFAudio = TRUE; // Indicate call is initiated from HF

            AddRef();
            Unlock();
            
            DWORD dwErr = BthAGNetworkDialNumber(wszNumber);            
            
            Lock();
            DelRef();

            if (ERROR_SUCCESS != dwErr) {                
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error dialing number: %d\n", dwErr));
                if (m_AGProps.usCallSetup) {
                    SendIndicatorEvent(CALL_SETUP_EVENT, 0); // call setup is done.                   
                }
                m_AGProps.usCallSetup = 0;
                m_AGProps.fUseHFAudio = FALSE;
            }
        } 
        else {
            SendATCommand(AT_ERROR);
        }
    }

    Unlock();
}


// This method is called when the peer device to answer an incoming call
void CAGEngine::OnAnswerCall(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnAnswerCall\n"));

    Lock();

    if (m_AGProps.usCallSetup == 1) { // incoming call
        m_AGProps.fMuteRings = TRUE;

        m_AGProps.fUseHFAudio = TRUE; // Indicate call is answered from HF

        AddRef();
        Unlock();
        
        DWORD dwErr = BthAGNetworkAnswerCall();
        
        Lock();
        DelRef();
      
        if (ERROR_SUCCESS == dwErr) {
            SendATCommand(AT_OK);
        }
        else {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error answering call: %d\n", dwErr));
            SendATCommand(AT_ERROR);
            m_AGProps.fUseHFAudio = FALSE;
        }
    }
    else {
        SendATCommand(AT_ERROR);
    }

    Unlock();
}


// This method is called by the peer device to hang-up or reject an incoming call
void CAGEngine::OnHangupCall(LPSTR pszParam, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnHangupCall\n"));

    Lock();
    
    if (((m_AGProps.state == AG_STATE_RINGING) || 
        (m_AGProps.state == AG_STATE_RINGING_AUDIO_UP)) && 
        (!(m_AGProps.usHFCapability & AG_CAP_REJECT_CALL)))
    {
        // Do not support rejecting incoming calls
        SendATCommand(AT_ERROR);
    }
    else {       
        DWORD dwErr = ERROR_SUCCESS;
        DWORD dwState = 0;
        DWORD dwDropFlags = 0;

        ResetEvent(m_hHangupEvent);
            
        AddRef();
        Unlock();

        BthAGNetworkGetCallState(&dwState);
        GetNetworkDropFlags(dwState, &dwDropFlags);

        if (dwDropFlags) {
            dwErr = BthAGNetworkDropCall(dwDropFlags);
        }
        
        Lock();
        DelRef();

        if (ERROR_SUCCESS == dwErr) {    
            SendATCommand(AT_OK);
            if (dwDropFlags) {
                AddRef();
                Unlock();
                
                // Wait for hang-up event when a call was dropped                
                WaitForSingleObject(m_hHangupEvent, AG_COMMAND_COMPLETE_TO);

                Lock();
                DelRef();
            }
        }
        else {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error dropping call: %d\n", dwErr));
            SendATCommand(AT_ERROR);            
        }
    }
    
    Unlock();
}


// This method is called by the peer to transmit DTMF codes
void CAGEngine::OnDTMF(LPSTR pszParams, int cchParam)
{
    WCHAR wszDTMF[MAX_SEND_BUF];
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnDTMF\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);

    if (m_AGProps.fInCall) {
        SendATCommand(AT_OK);

        AddRef();
        Unlock();
        
        if (0 < MultiByteToWideChar(CP_ACP, 0, pszParams, -1, wszDTMF, ARRAY_SIZE(wszDTMF))) {
            BthAGNetworkTransmitDTMF(wszDTMF);
        }
        else {
            ASSERT(0);
        }               
        
        Lock();
        DelRef();
    }
    else {
        SendATCommand(AT_ERROR);
    }

    Unlock();
}


// This method is called by the peer to place a call on hold or enable
// a multi-party call.
void CAGEngine::OnCallHold(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnCallHold\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    BOOL fSuccess = FALSE;
    BOOL fServiceConnectionUp = FALSE;
    BOOL fCallSetup = FALSE;
    DWORD dwState = 0;

    AddRef();
    Unlock();

    // Get current call state so we can determine how to handle
    // this command.
    
    DWORD dwErr = BthAGNetworkGetCallState(&dwState);

    Lock();
    DelRef();
    
    if (ERROR_SUCCESS != dwErr) {    
        // Fall thru to return error
        ASSERT(0);
    }
    else if (m_AGProps.usHFCapability & AG_CAP_3WAY_CALL) {
        if ((cchParam < 1) || (cchParam > 1)) {
            // Bad or unsupported parameters, do nothing to send error
        }
        else if (pszParams[0] == '?') {
            // Test command for supported call-waiting features            
            SendATCommand("\r\n+CHLD: (0,1,2,3)\r\n");
            SendATCommand(AT_OK);
            fServiceConnectionUp = TRUE;
            fSuccess = TRUE;
        }
        else if (pszParams[0] == '0') {
            // Release all held/waiting calls
            DWORD dwDropFlags = 0;

            if (dwState & NETWORK_FLAGS_STATE_HOLD) {
                // Release held calls
                dwDropFlags |= NETWORK_FLAGS_DROP_HOLD;
            }
            if (dwState & NETWORK_FLAGS_STATE_OFFERING) {
                // Release waiting calls
                dwDropFlags |= NETWORK_FLAGS_DROP_OFFERING;
            }

            ResetEvent(m_hHangupEvent);

            AddRef();
            Unlock();

            if (dwDropFlags) {
                dwErr = BthAGNetworkDropCall(dwDropFlags);
            }
            else {
                // if dwDropFlags is 0 we return "OK"
                dwErr = ERROR_SUCCESS;
            }

            Lock();
            DelRef();
            
            if (ERROR_SUCCESS == dwErr) {
                fSuccess = TRUE;
                SendATCommand(AT_OK);
                if (dwDropFlags) {
                    AddRef();
                    Unlock();
                    
                    // Wait for hang-up event when a call was dropped                
                    WaitForSingleObject(m_hHangupEvent, AG_COMMAND_COMPLETE_TO);

                    Lock();
                    DelRef();
                }
            }
        }
        else if (pszParams[0] == '1') {
            // Release active calls, transfer held/waiting calls
            m_AGProps.fUseHFAudio = TRUE;

            ResetEvent(m_hHangupEvent);

            AddRef();
            Unlock();

            dwErr = BthAGNetworkDropSwapCall();

            Lock();
            DelRef();
               
            if (ERROR_SUCCESS == dwErr) {
                fSuccess = TRUE;
                SendATCommand(AT_OK);
                if (dwState & NETWORK_FLAGS_STATE_ACTIVE) {
                    AddRef();
                    Unlock();
                    
                    // Wait for hang-up event if we have an active call
                    WaitForSingleObject(m_hHangupEvent, AG_COMMAND_COMPLETE_TO);

                    Lock();
                    DelRef();
                }
            }
        }
        else if (pszParams[0] == '2') {
            // Swap calls
            
            BOOL fUnHoldCall = FALSE;
            BOOL fHoldCall = FALSE;

            if ((dwState & NETWORK_FLAGS_STATE_ACTIVE) &&
                !(dwState & NETWORK_FLAGS_STATE_HOLD) &&
                !(dwState & NETWORK_FLAGS_STATE_OFFERING)) {
                // Just an active call, need to hold call
                fHoldCall = TRUE;
            }
            else if (!(dwState & NETWORK_FLAGS_STATE_ACTIVE) &&
                (dwState & NETWORK_FLAGS_STATE_HOLD) &&
                !(dwState & NETWORK_FLAGS_STATE_OFFERING)) {
                // Just a held call, need to unhold call
                fUnHoldCall = TRUE;
            }
            
            m_AGProps.fUseHFAudio = TRUE;

            SVSUTIL_ASSERT(!(fUnHoldCall && fHoldCall));

            AddRef();
            Unlock();

            if (fUnHoldCall) {
                dwErr = BthAGNetworkUnholdCall();
            }
            else if (fHoldCall) {
                dwErr = BthAGNetworkHoldCall();
            }
            else {            
                dwErr = BthAGNetworkSwapCall();
            }
            
            Lock();
            DelRef();
            
            if (ERROR_SUCCESS == dwErr) {
                fSuccess = TRUE;
                SendATCommand(AT_OK);
            }
        }
        else if (pszParams[0] == '3') {
            // add held call to conversation.
            if ((dwState & NETWORK_FLAGS_STATE_HOLD) &&
                TRUE == BthAGNetworkExtRequest(NETWORK_EVENT_SET_ADD_HELD_CALL_TO_CONF, 0, NULL)) {
                fSuccess = TRUE;
                SendATCommand(AT_OK);
            }
        }
            
        else if ((pszParams[0] == '4')) {
            // We don't support these parameters, do nothing to send error
        }
    }
    else {
        // We don't support 3-way, do nothing to send error
        // TODO: If we ever support AT+CMEE we can send a specific CME error instead of the generic ERROR response
    }

    if (! fSuccess) {
        SendATCommand(AT_ERROR);
        m_AGProps.fUseHFAudio = FALSE;
    }

    if (fServiceConnectionUp) {
        ServiceConnectionUp();
    }

    Unlock();
}


PFN_BthAGOnVoiceTag CAGEngine::GetVoiceTagHandler()
{
    return m_pParser ? m_pParser->GetVoiceTagHandler() : NULL;
}


// This method is called by the peer to start voice recognition
void CAGEngine::OnVoiceRecognition(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnVoiceRecognition\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTED);

    if (m_AGProps.usHFCapability & AG_CAP_VOICE_RECOG) {
        if (cchParam != 1) {
            SendATCommand(AT_ERROR);
        }
        else if (pszParams[0] == '0') {
            SendATCommand(AT_OK);
            
            AddRef();
            Unlock();
            
            BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_VOICE_RECOG, 0, (void*)GetVoiceTagHandler());

            Lock();
            DelRef();
        }
        else if (pszParams[0] == '1') {
            SendATCommand(AT_OK);
            DWORD dwErr = OpenAudioChannel();
            if ((ERROR_SUCCESS != dwErr) && (ERROR_ALREADY_INITIALIZED != dwErr)) {
                DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Call to open audio channel failed.  Can't do voice recognition.\n"));
            }        

            AddRef();
            Unlock();
            
            BthAGPhoneExtRequest(AG_PHONE_EVENT_SET_VOICE_RECOG, 1, (void*)GetVoiceTagHandler());

            Lock();
            DelRef();            
        }    
    }
    else {
        SendATCommand(AT_ERROR);        
    }
    
    Unlock();
}


// This method is called by the peer to query supported features of the AG
void CAGEngine::OnSupportedFeatures(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnSupportedFeatures\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    LPSTR pTmp;    
    m_AGProps.usRemoteFeatures = (USHORT) strtol(pszParams, &pTmp, 10);
    if (pTmp != (pszParams + strlen(pszParams))) {
        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: AT+BRSF Command was poorly formatted.\n"));
        SendATCommand(AT_ERROR);
    }
    else {
        CHAR pszCommand[MAX_SEND_BUF];
        
        m_AGProps.fHandsfree = TRUE;

        if (S_OK == StringCchPrintfA(pszCommand, MAX_SEND_BUF, "\r\n+BRSF: %d\r\n", m_AGProps.usHFCapability)) {
            SendATCommand(pszCommand);
            SendATCommand(AT_OK);
        }
        else {
            SendATCommand(AT_ERROR);
        }       
    }   

    Unlock();
}


// This method is called by the peer to get a list of supported indicators
void CAGEngine::OnTestIndicators(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnTestIndicators\n"));

    LPCSTR szCommand = "\r\n+CIND: "                \
                       "(\"service\",(0,1)),"       \
                       "(\"call\",(0,1)),"          \
                       "(\"callsetup\",(0-3)),"     \
                       "(\"callheld\",(0-2)),"      \
                       "(\"signal\",(0-5)),"        \
                       "(\"roam\",(0,1)),"          \
                       "(\"battchg\",(0-5))\r\n";
    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    m_AGProps.fHandsfree = TRUE;

    SendATCommand(szCommand);
    SendATCommand(AT_OK);

    Unlock();
}


// This method is called by the peer to read the supported indicators
void CAGEngine::OnReadIndicators(LPSTR pszParams, int cchParam)
{
    #define CALL_HELD_NONE                  0x00
    #define CALL_HELD_AND_ACTIVE_CALL       0x01
    #define CALL_HELD_AND_NO_ACTIVE_CALL    0x02    
    
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnReadIndicators\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    m_AGProps.fHandsfree = TRUE;

    //  Get newest signal strength, and battchg status.
    DWORD dwSignalStrength = 0;
    DWORD dwBattLevel = 0;
    BthAGPhoneExtRequest(AG_PHONE_EVENT_QUERY_SIGNAL_STRENGTH, 0x00, &dwSignalStrength);
    BthAGPhoneExtRequest(AG_PHONE_EVENT_QUERY_BATT_LEVEL, 0x00, &dwBattLevel);
    
    //  Determine held call status
    DWORD dwNetworkCallState = 0;
    BthAGNetworkGetCallState(&dwNetworkCallState);
    
    BOOL bActive = dwNetworkCallState & NETWORK_FLAGS_STATE_ACTIVE;
    BOOL bHeld   = dwNetworkCallState & NETWORK_FLAGS_STATE_HOLD;

    DWORD dwCallHeld;
    if (!bHeld) {
        dwCallHeld = CALL_HELD_NONE;
    } 
    else {
        if (bActive) {
            dwCallHeld = CALL_HELD_AND_ACTIVE_CALL;
        }
        else {
            dwCallHeld = CALL_HELD_AND_NO_ACTIVE_CALL;    
        }
    }
    
    //  Send status of all indicators   
    CHAR pszCommand[MAX_SEND_BUF];             
    HRESULT hResult = StringCchPrintfA(
                    pszCommand,
                    MAX_SEND_BUF,
                    "\r\n+CIND: %d,%d,%d,%d,%d,%d,%d\r\n",
                    m_AGProps.fHaveService?1:0,
                    m_AGProps.fInCall?1:0,
                    m_AGProps.usCallSetup,
                    dwCallHeld,
                    dwSignalStrength,
                    m_AGProps.fRoam?1:0,
                    dwBattLevel);
    
    if (S_OK == hResult) {
        SendATCommand(pszCommand);
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }

    Unlock();
}


// This method is called by the peer to register for indicator updates.
void CAGEngine::OnRegisterIndicatorUpdates(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnRegisterIndicatorUpdates\n"));

    //
    // Format of the string we are parsing: "a,b,c,d\n".
    // We care about a and d.
    //

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    DWORD cParam = 0;
    BOOL fSendOk = FALSE;

    while (*pszParams != '\0') {
        if (*pszParams == ',') {
            cParam++;
        }
        else if (cParam == 0 && (*pszParams != ' ')) {
            // If first param is not 3 we don't care about this command
            if (*pszParams != '3') {
                break;
            }
        }
        else if ((cParam == 3) && (*pszParams != ' ')) {
            // Fourth param is indicator update flag
            if (*pszParams == '1') {            
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Enabling Indicator Updates.\n"));
                m_AGProps.fIndicatorUpdates = TRUE;
            }
            else {            
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Disabling Indicator Updates.\n"));
                m_AGProps.fIndicatorUpdates = FALSE;
            }

            m_AGProps.fHandsfree = TRUE;

            fSendOk = TRUE;
            break;
        }

        pszParams++;
    }

    if (fSendOk) {
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }

    if ((m_AGProps.usHFCapability & AG_CAP_3WAY_CALL) && 
        (m_AGProps.usRemoteFeatures & HF_CAP_3WAY_CALL)) {
        // If we mutually support 3-way call, service level connection
        // is not up until after AT+CHLD=?
    }
    else {
        ServiceConnectionUp();
    }
    
    Unlock();

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --AGEngine::OnRegisterIndicatorUpdates\n"));
}


// This method is called by the peer to enable call-waiting
void CAGEngine::OnEnableCallWaiting(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnEnableCallWaiting\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    BOOL fSendOk = FALSE;

    while (*pszParams != '\0') {
        if (*pszParams != ' ') {
            if (*pszParams == '1') {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Enabling Call Waiting.\n"));
                m_AGProps.fNotifyCallWait = TRUE;
            }
            else {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Disabling Call Waiting.\n"));
                m_AGProps.fNotifyCallWait = FALSE;
            }

            fSendOk = TRUE;
            break;
        }
        pszParams++;
    }

    if (fSendOk) {
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }

    Unlock();

    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: --AGEngine::OnEnableCallWaiting\n"));
}


// This method is called by the peer to enable "call line identification".
void CAGEngine::OnEnableCLI(LPSTR pszParams, int cchParam)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnEnableCLI\n"));

    BOOL fSendOk = FALSE;

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);

    while (*pszParams != '\0') {
        if (*pszParams != ' ') {
            if (*pszParams == '1') {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Enabling CLI.\n"));
                m_AGProps.fNotifyCLI = TRUE;
            }
            else {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Disabling CLI.\n"));
                m_AGProps.fNotifyCLI = FALSE;
            }

            fSendOk = TRUE;
            break;
        }
        pszParams++;
    }

    if (fSendOk) {
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }

    Unlock();
}


// This method is called when the parser receives an unknown command
void CAGEngine::OnUnknownCommand(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnUnknownCommand\n"));

    Lock();

    ASSERT(m_AGProps.state >= AG_STATE_CONNECTING);
    
    if (m_AGProps.state >= AG_STATE_CONNECTING) {
        SendATCommand(AT_ERROR);
    }
    
    Unlock();
}


// This method is called when the peer device responds with OK
void CAGEngine::OnOK(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Got OK response from peer.\n"));
}


// This method is called when the peer devices responds with ERROR
void CAGEngine::OnError(void)
{
    DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Got ERROR response from peer.\n"));
}


// This method is called when service goes up or down
void CAGEngine::OnServiceCallback(BOOL fHaveService)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: OnServiceCallback: %d\n", fHaveService));

    if (BOOLIFY(m_AGProps.fHaveService) != BOOLIFY(fHaveService)) {
        Lock();       
        m_AGProps.fHaveService = fHaveService;
        SendIndicatorEvent(SERVICE_EVENT, fHaveService?1:0);            
        Unlock();
    }
}


// This method is called by phone extension for OOB notification.
void CAGEngine::OnPhoneEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam)
{   
    DEBUGMSG (ZONE_HANDLER, (TEXT("BTAGSVC:: Phone event received [0x%x].\r\n"),
        dwEvent));

    Lock();
    
    switch(dwEvent) {
        case AG_PHONE_NOTIF_CELLULAR_SIGNAL_STRENGTH:
            SendIndicatorEvent(SIGNAL_STRENGTH_EVENT, *(PDWORD)lpvParam, 0, 5);
            break;
   
        case AG_PHONE_NOTIF_ROAM:
            m_AGProps.fRoam = *(PDWORD)lpvParam?1:0;
            SendIndicatorEvent(ROAM_EVENT, m_AGProps.fRoam?1:0, 0, 1);
            break;

        case AG_PHONE_NOTIF_BATT_STRENGTH:
            SendIndicatorEvent(BATT_STRENGTH_EVENT, *(PDWORD)lpvParam, 0, 5);
            break;

        default:
            DEBUGMSG (ZONE_WARN, (L"BTAGSVC: unknown phone event [0x%x]\r\n",dwEvent));
            ASSERT(0);
            break;
    }

    Unlock();
    
}   


// This method is called by the Network when an event has occured
void CAGEngine::OnNetworkEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam)
{
    DEBUGMSG (ZONE_HANDLER, (TEXT("BTAGSVC:: Network event received [0x%x].\r\n"),
        dwEvent));
    
    switch (dwEvent) {
        case NETWORK_NOTIF_CALL_IN:
            OnNetworkCallIn((LPSTR)lpvParam);
            break;
        case NETWORK_NOTIF_CALL_OUT:
            OnNetworkCallOut();
            break;
        case NETWORK_NOTIF_CALL_CONNECT:
            OnNetworkAnswerCall();
            break;
        case NETWORK_NOTIF_CALL_BUSY:
        case NETWORK_NOTIF_CALL_DISCONNECT:
            if (sizeof(DWORD) > cbParam) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Received event from network component with invalid param size.\n"));
                ASSERT(0);                    
            }
            else {
                OnNetworkHangupCall((DWORD)lpvParam, (NETWORK_NOTIF_CALL_BUSY == dwEvent) ? HANGUP_DELAY_BUSY_MS : HANGUP_DELAY_MS);
            }
            break;
        case NETWORK_NOTIF_CALL_REJECT:        
            OnNetworkRejectCall();
            break;
        case NETWORK_NOTIF_RING:
            OnNetworkRing();
            break;
        case NETWORK_NOTIF_CALL_INFO:
            OnNetworkInfo((LPSTR)lpvParam);
            break;
        case NETWORK_NOTIF_CALL_HELD:
            if (sizeof(DWORD) > cbParam) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Received event from network component with invalid param size.\n"));
                ASSERT(0);                    
            }
            else {
                OnNetworkHeldCall(*(DWORD*)lpvParam);
            }            
            break;
        case NETWORK_NOTIF_FAILED:
            if (sizeof(NetworkCallFailedInfo) > cbParam) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Received error event from network component with invalid param size.\n"));
                ASSERT(0);                    
            }
            else {
                OnNetworkCallFailed(((NetworkCallFailedInfo*)lpvParam)->usCallType, ((NetworkCallFailedInfo*)lpvParam)->dwStatus);
            }            
            break;            
        default:
            DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Unknown network event: %d\n", dwEvent));
            ASSERT(0);
    }
}


// This method is called by the Network when a call is answered
void CAGEngine::OnNetworkAnswerCall(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkAnswerCall\n"));

    Lock();
    
    if ((m_AGProps.state >= AG_STATE_CONNECTED) && m_AGProps.fIndicatorUpdates) {
        if (! m_AGProps.fInCall) {
            SendIndicatorEvent(CALL_EVENT, 1); // Call is active
        }
        if (m_AGProps.usCallSetup) {
            SendIndicatorEvent(CALL_SETUP_EVENT, 0); // Call Setup is completed
        }
    }
    m_AGProps.usCallSetup = 0;
    m_AGProps.fInCall = TRUE;

    if ((m_AGProps.state >= AG_STATE_CONNECTED) && (m_AGProps.fUseHFAudio || m_AGProps.fUseHFAudioAlways)) {
        if (m_AGProps.usCallType == AG_CALL_TYPE_IN) {
            DWORD dwErr = OpenAudioChannel();
            if ((ERROR_SUCCESS != dwErr) && (ERROR_ALREADY_INITIALIZED != dwErr)) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error opening audio connection: %d\n", dwErr));
            }
        }
    }
    else if (m_AGProps.fPowerSave || !m_AGProps.fHandsfree) {
        // Call was answered in the handset close connection in power-save mode
        CloseControlChannel();
    }
    else if (AG_STATE_RINGING == m_AGProps.state) {
        // We need to at least reset the state.  We can
        // remain connected, but we shouldn't be ringing.
        m_AGProps.state = AG_STATE_CONNECTED;
    }
    else if (AG_STATE_RINGING_AUDIO_UP == m_AGProps.state) {
        // We need to at least reset the state.  We can
        // remain connected, but we shouldn't be ringing.
        m_AGProps.state = AG_STATE_AUDIO_UP;
    }    

    m_AGProps.fUseHFAudio = FALSE;

    Unlock();
}


// This method is called by the Network when a call is disconnected (or busy)
void CAGEngine::OnNetworkHangupCall(DWORD dwRemainingCalls, DWORD dwHangUpDelayMS)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkHangupCall\n"));

    Lock();

    SetEvent(m_hHangupEvent);

    // If we have no remaining calls (i.e. dwRemainingCalls==0)
    // then we can clean up the call state.
    //
    // Note:
    // If we have another call and it is a held or active call we clearly
    // should not clean up.  For offering or outgoing we also don't want
    // to clean up since it implies we are in a call waiting scenario in
    // which case the call indicator is not used.  The callsetup indicator
    // will be sent with CHLD=1 in OnNetworkAnswerCall.

    if (dwRemainingCalls > 0) {
        DWORD dwState = 0;
        BthAGNetworkGetCallState(&dwState);
        if (! (dwState & NETWORK_FLAGS_STATE_ACTIVE ||
            dwState & NETWORK_FLAGS_STATE_HOLD)) {
            dwRemainingCalls = 0;
        }
    }

    if (dwRemainingCalls > 0) {
        if (m_AGProps.usCallSetup) {
            SendIndicatorEvent(CALL_SETUP_EVENT, 0);
            m_AGProps.usCallSetup = 0;
        }        
    }
    else if (dwRemainingCalls == 0) {
        if (m_AGProps.state >= AG_STATE_CONNECTED) {
            if (m_AGProps.fInCall) {
                // For hangup call, send call indicator as inactive
                SendIndicatorEvent(CALL_EVENT, 0);
            }
            else if (m_AGProps.usCallSetup) {
                // For rejected call, send callsetup indicator as inactive
                SendIndicatorEvent(CALL_SETUP_EVENT, 0);
                m_AGProps.usCallSetup = 0;
            }

            m_fCancelCloseConnection = FALSE;
            m_CloseCookie = m_pTP->ScheduleEvent(TimeoutConnection, (LPVOID)this, dwHangUpDelayMS);
        }
        
        m_szCLI[0] = 0;
        m_AGProps.fInCall = FALSE;
        m_AGProps.fUseHFAudio = FALSE;
    }

    Unlock();
}


// This method is called by the Network when a call is rejected
void CAGEngine::OnNetworkRejectCall(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkRejectCall\n"));

    Lock();

    SetEvent(m_hHangupEvent);

    if (m_AGProps.state >= AG_STATE_CONNECTED) {
        if (m_AGProps.usCallSetup) {
            SendIndicatorEvent(CALL_SETUP_EVENT,0);
        }

        CloseAudioChannel();
        if (m_AGProps.fPowerSave || !m_AGProps.fHandsfree) {
            CloseControlChannel();
        }
        else if (AG_STATE_RINGING == m_AGProps.state) {
            // We need to at least reset the state.  We can
            // remain connected, but we shouldn't be ringing.
            m_AGProps.state = AG_STATE_CONNECTED;
        }        
    }

    m_szCLI[0] = 0;    
    m_AGProps.usCallSetup = 0;
    m_AGProps.fInCall = FALSE;
    m_AGProps.fUseHFAudio = FALSE;
    
    Unlock();
}

void CAGEngine::OnNetworkHeldCall(DWORD dwState)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkHeldCall\n"));

    Lock();    
    SendIndicatorEvent(CALL_HELD_EVENT, dwState);    
    Unlock();    
}


// This method is called by the Network when an incoming call is placed
void CAGEngine::OnNetworkCallIn(LPSTR pszNumber)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkCallIn\n"));

    Lock();

    if (m_AGProps.fInCall) {
        // Already in a call, send call-wait signal but do not accept call.
        if (m_AGProps.fNotifyCallWait) {
            CHAR szCommand[MAX_SEND_BUF];
            int cchCommand = 0;
            
            if (pszNumber[0] == 0) {
                if (S_OK == StringCchPrintfA(szCommand, MAX_SEND_BUF, "\r\n+CCWA: \"0\",128,1\r\n")) {
                    cchCommand = strlen(szCommand);
                }
            }
            else {
                if (S_OK == StringCchPrintfA(szCommand, MAX_SEND_BUF, "\r\n+CCWA: \"%s\",128,1\r\n", pszNumber)) {
                    cchCommand = strlen(szCommand);
                }
            }

            if (cchCommand > 0) {
                SendATCommand(szCommand);
            }
        }
        
        SendIndicatorEvent(CALL_SETUP_EVENT,1);
        m_AGProps.usCallSetup = 1;
    }
    else {
        m_AGProps.usCallType = AG_CALL_TYPE_IN;
        m_AGProps.usCallSetup = 1;
        m_AGProps.fMuteRings = FALSE;

        if (m_AGProps.usHFCapability & AG_CAP_INBAND_RING) {
            // If inband ring tones is enabled, connect audio channel
            m_fAudioConnect = TRUE;
        }

        StringCchCopyA(m_szCLI, MAX_PHONE_NUMBER, pszNumber);            

        BOOL fConnectHSOnly = !m_AGProps.fConnectHFOnCall;
        DWORD dwErr = OpenControlChannel(FALSE, fConnectHSOnly);
        
        if ((ERROR_SUCCESS != dwErr) && (ERROR_ALREADY_INITIALIZED != dwErr)) {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error opening AG Connection or no connection: %d.\n", dwErr));
        }
        else {
            if (! m_hUIThread) {
                m_hUIThread = CreateThread(NULL, 0, AGUIThread, this, 0, NULL);
            }

            if (m_hUIThread) {
                if (m_AGProps.fHandsfree && (m_AGProps.state >= AG_STATE_CONNECTED)) {
                    if (m_AGProps.usHFCapability & AG_CAP_INBAND_RING) {
                        DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Incoming call is using in-band ring tone.\n"));
                        const char szCommand[] = "\r\n+BSIR: 1\r\n";
                        SendATCommand(szCommand);
                    }
    
                    SendIndicatorEvent(CALL_SETUP_EVENT,1); // Call Setup is ongoing
                }
            }
            else {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Out of resources.\n"));
            }
        }
    }
    
    Unlock();
}


// This method is called by the Network when an outgoing call is placed
void CAGEngine::OnNetworkCallOut(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkCallOut\n"));

    Lock();

    m_AGProps.usCallType = AG_CALL_TYPE_OUT;
    m_AGProps.fUseHFAudio = TRUE; // By default outgoing calls will connect HF audio

    m_fAudioConnect = TRUE;
    BOOL fConnectHSOnly = !m_AGProps.fConnectHFOnCall;
    DWORD dwErr = OpenControlChannel(FALSE, fConnectHSOnly);
    
    if ((ERROR_SUCCESS != dwErr) && (ERROR_ALREADY_INITIALIZED != dwErr)) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error opened AG Connection or no connection: %d.\n", dwErr));
    }
    else {
        if (! m_hUIThread) {
            m_hUIThread = CreateThread(NULL, 0, AGUIThread, this, 0, NULL);
        }
        
        if (m_hUIThread) {
            if (m_AGProps.fHandsfree && (m_AGProps.state >= AG_STATE_CONNECTED)) {
                if (m_AGProps.usCallSetup != 2) {
                    // For outgoing calls, some car kits need to see 3,2 at some point
                    SendIndicatorEvent(CALL_SETUP_EVENT,2);
                }
                SendIndicatorEvent(CALL_SETUP_EVENT,3);
            }
        }
        else {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Out of resources.\n"));
        }                
    }

    m_AGProps.usCallSetup = 3;

    Unlock();
}


// This method is called by the Network when a caller id notification is delayed
void CAGEngine::OnNetworkInfo(LPSTR pszNumber)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkInfo number:%hs\n", pszNumber));
    
    Lock();

    // If caller id is present and different, send CLIP
    if (pszNumber[0] != 0 && strcmp(m_szCLI, pszNumber)) {
        StringCchCopyA(m_szCLI, MAX_PHONE_NUMBER, pszNumber);
        if (m_AGProps.fNotifyCLI) {
            // If we are already ringing the HF, send the CLIP
            if ((m_AGProps.state == AG_STATE_RINGING) || 
                (m_AGProps.state == AG_STATE_RINGING_AUDIO_UP)) {
                CHAR szCommand[MAX_SEND_BUF];
                if (S_OK == StringCchPrintfA(szCommand, MAX_SEND_BUF, "\r\n+CLIP: \"%s\",128\r\n", pszNumber)) {
                    SendATCommand(szCommand);
                }
            }
        }
    }
    

    
    Unlock();
}


// This method is called by the Network when a RING is received
void CAGEngine::OnNetworkRing(void)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkRing\n"));

    Lock();
    
    if (m_AGProps.state >= AG_STATE_CONNECTED) {
        if (AG_STATE_AUDIO_UP == m_AGProps.state) {
            m_AGProps.state = AG_STATE_RINGING_AUDIO_UP;
        }
        else if (AG_STATE_CONNECTED == m_AGProps.state) {
            m_AGProps.state = AG_STATE_RINGING;
        }

        if (!m_AGProps.fMuteRings) {
            SendATCommand(AT_RING);

            if (m_AGProps.fNotifyCLI && (*m_szCLI != 0)) {                
                CHAR szCommand[MAX_SEND_BUF];
                if (S_OK == StringCchPrintfA(szCommand, MAX_SEND_BUF, "\r\n+CLIP: \"%s\",128\r\n", m_szCLI)) {
                    SendATCommand(szCommand);
                }
            }
        }
    }

    Unlock();
}

void CAGEngine::OnNetworkCallFailed(USHORT usCallType, DWORD dwStatus)
{
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Calling AGEngine::OnNetworkCallFailed error:0x%X\n", dwStatus));

    Lock();

    switch (usCallType) {
        case CALL_TYPE_ANSWER:
        {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error answering call: 0x%X\n", dwStatus));
            m_AGProps.fUseHFAudio = FALSE;
            
        }
        break;

        case CALL_TYPE_DROP:
        {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error dopping call: 0x%X\n", dwStatus));
        }
        break;
        
        case CALL_TYPE_DIAL:
        {
            if (m_AGProps.usCallSetup) {
                SendIndicatorEvent(CALL_SETUP_EVENT,0); // Call Setup is ongoing
            }
            m_AGProps.usCallSetup = 0;
            m_AGProps.fUseHFAudio = FALSE;
        }
        break;

        case CALL_TYPE_HOLD:
        {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error putting call on hold: 0x%X\n", dwStatus));
        }
        break;

        case CALL_TYPE_SWAP:
        {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error swapping call: 0x%X\n", dwStatus));
            m_AGProps.fUseHFAudio = FALSE;
        }
        break;

        case CALL_TYPE_UNHOLD:
        {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Error taking call off hold: 0x%X\n", dwStatus));
            m_AGProps.fUseHFAudio = FALSE;
        }
        break;
    }
    
    Unlock();
}


//  This method handles offering call put on hold.
void CAGEngine::OnRespondAndHold(LPSTR pszParams, int cchParam)
{
    // Not supported.
    OnUnknownCommand();
}


//  This method returns the subscriber number information.
void CAGEngine::OnSubscriberNumberInformation(LPSTR pszParams, int cchParam)
{
    SUBSCRIBER_NUMBER_LIST  sSubscriberNumberList, 
                            *psSubscriberNumberList = &sSubscriberNumberList;
    CHAR                    Buffer[512];
    BOOL                    bSubscriberNumberObtained = FALSE; 
    HRESULT                 hResult;
        
    Lock();    
    
    psSubscriberNumberList->dwBufferSize = sizeof(Buffer);
    psSubscriberNumberList->pcBuffer     = Buffer;

    DWORD cIterations = 0;
    while (cIterations < MAX_NETWORK_REQUEST_ITERATIONS) {
        DWORD cbBuffer = psSubscriberNumberList->dwBufferSize;
        if (!BthAGNetworkExtRequest(NETWORK_EVENT_QUERY_SUBSCRIBER_NO, 0, psSubscriberNumberList)) {
            if (psSubscriberNumberList->dwBufferSize <= cbBuffer) {
                DEBUGMSG (ZONE_ERROR, (L"BTAGSVC: Error query subscriber no.\r\n"));
                break;
            }
            if (psSubscriberNumberList->pcBuffer != Buffer)
                delete[]psSubscriberNumberList->pcBuffer;
            psSubscriberNumberList->pcBuffer = new CHAR[psSubscriberNumberList->dwBufferSize];
            if (psSubscriberNumberList->pcBuffer == NULL) {
                DEBUGMSG (ZONE_ERROR, (L"BTAGSVC:: OnSubscriberNumberInformation(): OOM.\r\n"));
                break;
            }
        } 
        else {
            bSubscriberNumberObtained = TRUE;
            break;
        }
        
        cIterations++;        
    }    

    if (bSubscriberNumberObtained) {
        // return format: ,<number>,<type>,,<service>
        // with type hardcoded to 128 (national or international format)
        // service hardcoded to 4 (voice)
        // pcBuffer points to multi_sz style phone number to be returned.
        
        CHAR szCommand[MAX_SEND_BUF];
        CHAR *pOneSubscriberNumber = psSubscriberNumberList->pcBuffer;
        
        while (*pOneSubscriberNumber) {            
            hResult = StringCchPrintfA(
                            szCommand, 
                            MAX_SEND_BUF, 
                            "\r\n+CNUM: ,\"%s\",128,,4\r\n", 
                            pOneSubscriberNumber);
            if (S_OK == hResult) {                
                SendATCommand(szCommand);
            }
            pOneSubscriberNumber += (strlen(pOneSubscriberNumber) + 1);
        }        
        
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }
        
    if (psSubscriberNumberList->pcBuffer != Buffer)
        delete[]psSubscriberNumberList->pcBuffer;  

    Unlock();

}

DWORD CAGEngine::MapNetworkCallStateToCallStatus(DWORD dwNetworkCallState)
{
    DWORD dwCallStatus = 0;
    
    switch (dwNetworkCallState) {
        case NETWORK_FLAGS_STATE_ACTIVE:
            dwCallStatus = 0;
            break;
        case NETWORK_FLAGS_STATE_HOLD:
            dwCallStatus = 1;
            break;                    
        case NETWORK_FLAGS_STATE_OFFERING:
            dwCallStatus = 4;
            break;                    
        case NETWORK_FLAGS_STATE_OUTGOING:
            dwCallStatus = 3;
            break;
        case NETWORK_FLAGS_STATE_WAITING:
            dwCallStatus = 5;
        default:
            //  Should we skip this call?
            ASSERT(0);            
            break;
    }
    
    return dwCallStatus;
}


// This methods gets list of current calls.
void CAGEngine::OnCurrentCallList(LPSTR pszParams, int cchParam)
{
    #define MAX_CALLS   8

    ONE_CALL    OneCallArray[MAX_CALLS];
    CALL_LIST   CallList, 
                *pCallList = &CallList;
    BOOL        bCallListObtained = FALSE; 
    
    pCallList->dwMaxCalls     = MAX_CALLS;
    pCallList->pOneCallArray  = OneCallArray;
    
    Lock();    
    
    DWORD cIterations = 0;
    while (cIterations < MAX_NETWORK_REQUEST_ITERATIONS) {
        DWORD cCalls = pCallList->dwMaxCalls;
        if (!BthAGNetworkExtRequest(NETWORK_EVENT_QUERY_CALL_LIST, 0, pCallList)) {
            if (pCallList->dwMaxCalls <= cCalls) {
                DEBUGMSG (ZONE_ERROR, (L"BTAGSVC: No calls queried.\r\n"));
                break;
            }
            if (pCallList->pOneCallArray != OneCallArray)
                delete[]pCallList->pOneCallArray;
            pCallList->pOneCallArray = new ONE_CALL[pCallList->dwMaxCalls];
            if (pCallList->pOneCallArray == NULL) {
                DEBUGMSG (ZONE_ERROR, (L"BTAGSVC:: OnCurrentCallList(): OOM.\r\n"));
                break;
            }
        } 
        else {
            bCallListObtained = TRUE;
            break;
        }
        
        cIterations++;        
    }    

    if (bCallListObtained && (pCallList->dwMaxCalls != 0)) {
        //  return format: <idx>,<dir>,<status>,<mode>,<mprty>[,<number>,<type>]
        //  with mode hard coded to voice (0)
        //  type hard coded to hardcoded to 128 (national or international format)
        
        CHAR     szCommand[MAX_SEND_BUF];
        ONE_CALL *pOneCall;
        DWORD    i = 0, dwCallStatus;
        HRESULT  hResult;

        while (pCallList->dwMaxCalls--) {
            pOneCall = &(pCallList->pOneCallArray[i++]);
            dwCallStatus = MapNetworkCallStateToCallStatus(pOneCall->dwStatus);
            
            hResult = StringCchPrintfA(
                            szCommand, 
                            MAX_SEND_BUF, 
                            "\r\n+CLCC: %d,%d,%d,0,%d,\"%s\",128\r\n", 
                            pOneCall->CallID,
                            pOneCall->bMO ? 0 : 1,
                            dwCallStatus,
                            pOneCall->bMultiparty ? 1 : 0,
                            pOneCall->Number);            

            if (S_OK == hResult) {                
                SendATCommand(szCommand);
            }
        }        
    }
        
    SendATCommand(AT_OK);
    
    if (pCallList->pOneCallArray != OneCallArray)
        delete[]pCallList->pOneCallArray;

    Unlock();

}


// This methods sets the method of network operator string.
void CAGEngine::OnOperatorSelectionFormat(LPSTR pszParams, int cchParam)
{
    Lock();
    m_AGProps.fOperatorFormatLongAlphaNumeric = TRUE;    
    SendATCommand(AT_OK);    
    Unlock();
}


// This methods gets current operator.
void CAGEngine::OnOperatorSelection(LPSTR pszParams, int cchParam)
{    
    CHAR            szCommand[MAX_SEND_BUF];
    HRESULT         hr;
 
    ONE_OPERATOR    op;   
    memset(&op, 0, sizeof(op));
    
    Lock();
    
    BOOL fResult = BthAGNetworkExtRequest(NETWORK_EVENT_QUERY_CURRENT_OPERATOR, 0, &op);
    if (!fResult || (op.Name[0] == 0)) {
        hr = StringCchPrintfA(szCommand, sizeof(szCommand), 
            "\r\n+COPS: %d\r\n",
            op.dwMode);
    }
    else {
        hr = StringCchPrintfA(szCommand, sizeof(szCommand), 
            "\r\n+COPS: %d,0,\"%s\"\r\n",
            op.dwMode, op.Name);
    }
    
    if (S_OK == hr) {
        SendATCommand(szCommand);        
        SendATCommand(AT_OK);
    }
    else {
        SendATCommand(AT_ERROR);
    }    
    
    Unlock();
}


// This method gets the device id of the A2DP driver
BOOL CAGEngine::GetA2DPDeviceID(UINT* pDeviceId)
{
    BOOL fRet = FALSE;

    WAVEOUTCAPS wc;
    UINT Size = sizeof(wc);

    UINT TotalDevs = waveOutGetNumDevs();
    
    for(UINT i = 0; i < TotalDevs; i++) {
        MMRESULT Res = waveOutGetDevCaps(i, &wc, Size);
        if(MMSYSERR_NOERROR == Res && 
           (0 ==_tcsncmp(TEXT("Bluetooth Advanced Audio Output"),  wc.szPname, min(MAXPNAMELEN, sizeof(TEXT("Bluetooth Advanced Audio Output")))))) {
            *pDeviceId = i;
            fRet = TRUE;
            break;
        }
    }
    
    return fRet;
}


// This method will request A2DP suspend the active stream
void CAGEngine::SendA2DPSuspend(void)
{
    if (! m_fA2DPSuspended) {
        UINT uiDeviceId;
        if (GetA2DPDeviceID(&uiDeviceId)) {
            MMRESULT mmr = waveOutMessage((HWAVEOUT)uiDeviceId, WODM_BT_A2DP_SUSPEND, 0, 0);
            if (MMSYSERR_NOERROR == mmr) {
                m_fA2DPSuspended = TRUE;
            }
        }
    }
}

// This method will request A2DP to start the stream
void CAGEngine::SendA2DPStart(void)
{
    if (m_fA2DPSuspended) {
        UINT uiDeviceId;
        if (GetA2DPDeviceID(&uiDeviceId)) {
            MMRESULT mmr = waveOutMessage((HWAVEOUT)uiDeviceId, WODM_BT_A2DP_START, 0, 0);
#ifdef DEBUG            
            if (MMSYSERR_NOERROR != mmr) {
                DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Failed to start A2DP stream 0x%08X (%d).\n", mmr, mmr));
            }
#endif // DEBUG            
        }

        m_fA2DPSuspended = FALSE;
    }   
}

// This method will unpark or unsniff a connection if necessary
void CAGEngine::EnsureActiveBaseband(void)
{
    unsigned char mode;

    if (m_AGProps.state >= AG_STATE_CONNECTING) { 
        if (ERROR_SUCCESS == BthGetCurrentMode(&m_AGProps.btAddrClient, &mode)) {
            if (mode == 0x02) {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: UnSniff.\n"));
                if (ERROR_SUCCESS != BthExitSniffMode(&m_AGProps.btAddrClient)) {
                    DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Warning - failed to exit sniff mode.\n"));
                }
            }
            else if (mode == 0x03) {
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: UnPark.\n"));
                if (ERROR_SUCCESS != BthExitParkMode(&m_AGProps.btAddrClient)) {
                    DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Warning - failed to exit park mode.\n"));
                }
            }
        }
    }
}


// This public method sends a custom AT command to the peer device
DWORD CAGEngine::ExternalSendATCommand(LPSTR pszCommand, unsigned int cbCommand)
{
    Lock();
    DWORD dwRetVal = SendATCommandWithLength (pszCommand, cbCommand);
    Unlock();

    return dwRetVal;
}


// This private method sends an AT command to the peer device
DWORD CAGEngine::SendATCommand(LPCSTR pszCommand)
{
    UINT    cbCommand = 0;
    
    if (pszCommand)
        cbCommand = strlen(pszCommand);

    return SendATCommandWithLength(pszCommand, cbCommand);
}

DWORD CAGEngine::SendATCommandWithLength(LPCSTR pszCommand, UINT cbCommand)
{
    DWORD   dwRetVal = ERROR_SUCCESS;
    DWORD   cdwBytesWritten = 0;

#ifdef DEBUG
    DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Data was sent: "));
    DbgPrintATCmd(ZONE_HANDLER, pszCommand, cbCommand);
    DEBUGMSG(ZONE_HANDLER, (L"\n"));
#endif // DEBUG

    ASSERT(IsLocked());
        
    //
    // This is the lowest layer in the send path for the AG.  At this point, we ensure
    // we are not in sniff or park mode, and exit this mode if we indeed are.  
    //

    EnsureActiveBaseband();

    //
    // Send down the packet to RFCOMM
    //

    cdwBytesWritten = send(m_sockClient, pszCommand, cbCommand, 0);
    if (SOCKET_ERROR == cdwBytesWritten) {
        dwRetVal = GetLastError();
        DEBUGMSG(ZONE_ERROR, (_T("BTAGSVC: Error writing AT command to the device: %d\n"), dwRetVal));
    }
    else if (cbCommand != cdwBytesWritten) {
        DEBUGMSG(ZONE_WARN, (_T("BTAGSVC: Warning --> Send only wrote %d of %d total bytes.\n"), cdwBytesWritten, cbCommand));
    }

    //
    // Reschedule a thread to put us in sniff mode later
    //

    if (m_AGProps.ulSniffDelay) {
        StopSniffModeTimer();

        if ((m_AGProps.state >= AG_STATE_CONNECTED) && (m_AGProps.state < AG_STATE_AUDIO_UP)) {
            StartSniffModeTimer();
        }
    }

    return dwRetVal;
}


// This method is used to call UI-related functions using a seperate thread.
void CAGEngine::AGUIThread_Int(void)
{
    BOOL fCallAG;

    Lock();

    BOOL fHandsfree = m_AGProps.fHandsfree;

    AddRef();
    
    if (AG_CALL_TYPE_IN == m_AGProps.usCallType) {
        Unlock();
        fCallAG = BthAGOverrideCallIn(fHandsfree);
        Lock();
    }
    else {
        Unlock();
        fCallAG = BthAGOverrideCallOut(fHandsfree);
        Lock();
    }

    DelRef();

    if (! fCallAG) {
        CloseAudioChannel();
        if (m_AGProps.fPowerSave || !m_AGProps.fHandsfree) {
            CloseControlChannel();
        }
    }

    CloseHandle(m_hUIThread);
    m_hUIThread = NULL;

    Unlock();
}


// This thread is created to call UI-related functions that may need to block
DWORD WINAPI CAGEngine::AGUIThread(LPVOID pv)
{
    CAGEngine* pInst = (CAGEngine*)pv;
    pInst->AGUIThread_Int();
    return 0;
}

void CAGEngine::TimeoutConnection_Int(void)
{
    Lock();

    if (m_fCancelCloseConnection) {
        goto exit;
    }
    
    CloseAudioChannel();

    m_AGProps.state = AG_STATE_CONNECTED;
    
    if (m_AGProps.fPowerSave || !m_AGProps.fHandsfree) {
        CloseControlChannel();
    }   

exit:
    Unlock();
    return;
}

DWORD WINAPI CAGEngine::TimeoutConnection(LPVOID pv)
{
    CAGEngine* pInst = (CAGEngine*)pv;
    pInst->TimeoutConnection_Int();
    return 0;
}

void CAGEngine::AllowSuspend(void)
{
    // Be paranoid and check fUnattendSet religiously.  REALLY need to ensure 
    // this refcount does get out of sync.
    if (m_AGProps.fUnattendedMode && m_AGProps.fUnattendSet) {
        m_AGProps.fUnattendSet = FALSE;
        PowerPolicyNotify(PPN_UNATTENDEDMODE, FALSE);
    }
}

void CAGEngine::PreventSuspend(void)
{
    // Be paranoid and check fUnattendSet religiously.  REALLY need to ensure 
    // this refcount does get out of sync.
    if (m_AGProps.fUnattendedMode && !m_AGProps.fUnattendSet) {        
        m_AGProps.fUnattendSet = TRUE;
        PowerPolicyNotify(PPN_UNATTENDEDMODE, TRUE);
    }
}

void CAGEngine::StartSniffModeTimer(void)
{
    // If we have not already scheduled sniff thread and sniff mode is enabled, 
    // schedule the sniff thread.
    if ((! m_SniffCookie) && m_AGProps.ulSniffDelay) {
        // We need to prevent device from going to sleep until after we try to enter
        // sniff mode.  After trying to enter sniff mode we will leave unattended mode.
        ASSERT(! m_AGProps.fUnattendSet); // be paranoid
        PreventSuspend();
       
        m_SniffCookie = m_pTP->ScheduleEvent(EnterSniffModeThread, (LPVOID)this, m_AGProps.ulSniffDelay);        
    }
}

void CAGEngine::StopSniffModeTimer(void)
{
    if (m_SniffCookie) {        
        // The AG has unscheduled the sniff event.  At this point we will allow
        // device to suspend
        ASSERT(m_AGProps.fUnattendSet); // be paranoid
        AllowSuspend();
        
        m_pTP->UnScheduleEvent(m_SniffCookie);
        m_SniffCookie = NULL;
    }
}


//  This method sends indicator event.
void CAGEngine::SendIndicatorEvent(DWORD dwEvent, DWORD dwValue)
{
   //  Heed the peer's request for unsolicited result code.
    if (m_AGProps.fIndicatorUpdates) {
        CHAR    szCommand[MAX_SEND_BUF]; 
        HRESULT hResult;

        hResult = StringCchPrintfA(
                        szCommand, 
                        MAX_SEND_BUF, 
                        "\r\n+CIEV: %u,%u\r\n",
                        dwEvent, 
                        dwValue);
        
        if (S_OK == hResult) {
            SendATCommand(szCommand);
        }
    }

}


void CAGEngine::SendIndicatorEvent(DWORD dwEvent, DWORD dwValue, DWORD dwMin, DWORD dwMax)
{
    //  filter out bad values.
    if (dwValue < dwMin || dwValue > dwMax) {        
        ASSERT(0);
        return;
    }        
    SendIndicatorEvent(dwEvent, dwValue);
}


void CAGEngine::EnterSniffModeThread_Int(void)
{
    Lock();

    if ((m_AGProps.state >= AG_STATE_CONNECTED) && (m_AGProps.state < AG_STATE_AUDIO_UP)) {
        unsigned char mode;
        
        if (ERROR_SUCCESS == BthGetCurrentMode(&m_AGProps.btAddrClient, &mode)) {
            if (mode < 0x02) { // check we are not already in deep enough sleep
                DEBUGMSG(ZONE_HANDLER, (L"BTAGSVC: Going into SNIFF.\n")); 
                unsigned short usInt = 0;
                if (ERROR_SUCCESS != BthEnterSniffMode(&m_AGProps.btAddrClient, 
                                        m_AGProps.usSniffMax, 
                                        m_AGProps.usSniffMin, 
                                        m_AGProps.usSniffAttempt, 
                                        m_AGProps.usSniffTimeout, 
                                        &usInt)) {
                    DEBUGMSG(ZONE_WARN, (L"BTAGSVC: !! Warning - failed to enter sniff mode.\n"));
                }
            }
        }
    }

    // The AG has tried to put the remote device in sniff.  At this point we
    // will allow the device to suspend.  Note: The sniff request could have 
    // failed but we should allow the device to suspend now anyway.
    AllowSuspend();

    m_SniffCookie = NULL;

    Unlock();
}

DWORD WINAPI CAGEngine::EnterSniffModeThread(LPVOID pv)
{
    CAGEngine* pInst = (CAGEngine*)pv;
    pInst->EnterSniffModeThread_Int();
    return 0;
}


//
// This function is called from the Network module
//

CAGEngine* g_pAGEngine;

void BthAGOnNetworkEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam)
{
    g_pAGEngine->OnNetworkEvent(dwEvent, lpvParam, cbParam);
}


void BthAGOnPhoneEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam)
{
    g_pAGEngine->OnPhoneEvent(dwEvent, lpvParam, cbParam);
}


//
// This function is called from the PhoneExt module
//

void PhoneExtServiceCallback(BOOL fHaveService)
{
    g_pAGEngine->OnServiceCallback(fHaveService);
}

