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

#ifndef __AUDIOGW_H__
#define __AUDIOGW_H__

#include <windows.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <service.h>

#include <svsutil.hxx>

#include <bt_sdp.h>
#include <bthapi.h>
#include <bt_api.h>
#include <bt_buffer.h>
#include <bt_ddi.h>

#include "btagpub.h"
#include "btagdbg.h"
#include "btagutil.h"

#define AG_CAP_3WAY_CALL                0x0001
#define AG_CAP_EC_NR                    0x0002
#define AG_CAP_VOICE_RECOG              0x0004
#define AG_CAP_INBAND_RING              0x0008
#define AG_CAP_VOICE_TAG                0x0010
#define AG_CAP_REJECT_CALL              0x0020
#define AG_CAP_ENHANCED_CALL_STATUS     0x0040
#define AG_CAP_ENHANCED_CALL_CONTROL    0x0080
#define AG_CAP_EXTENDED_ERROR_CODES     0x0100

#define DEFAULT_AG_CAPABILITY   (AG_CAP_REJECT_CALL|AG_CAP_3WAY_CALL|AG_CAP_ENHANCED_CALL_STATUS)

#define HF_CAP_EC_NR            0x01
#define HF_CAP_3WAY_CALL        0x02
#define HF_CAP_CLI              0x04
#define HF_CAP_VOICE_RECOG      0x08
#define HF_CAP_VOLUME           0x10

#define HANGUP_DELAY_MS         0
#define HANGUP_DELAY_BUSY_MS    4000    // 4 sec busy delay

#define DEFAULT_PAGE_TIMEOUT    0x2500  // approx 6 seconds

#define AG_MTU                  127     // RFCOMM MTU to be used by AG

#define DEFAULT_SNIFF_DELAY     7000  // 7 secs
#define DEFAULT_SNIFF_MAX       0x640 // 1 sec max
#define DEFAULT_SNIFF_MIN       0x4B0 // .75 sec min
#define DEFAULT_SNIFF_ATTEMPT   0x20
#define DEFAULT_SNIFF_TO        0x20

#define DEFAULT_ESCO_TX_BANDWIDTH   0x1F40  // octets per second
#define DEFAULT_ESCO_RX_BANDWIDTH   0x1F40  // octets per second
#define DEFAULT_ESCO_MAX_LATENCY    0xFFFF  // don't care
#define DEFAULT_ESCO_VOICE_SETTING  0x0060  // Linear Input Coding, 2's complement, 16-bit, CVSD air coding format
#define DEFAULT_ESCO_RETRANSMISSION 0xFF    // don't care

#define AG_COMMAND_COMPLETE_TO  4000    // 4 seconds

#define DEFAULT_BTEXTHANDLER_MODULE L"btagext.dll"
#define BTATHANDLER_SETCALLBACK_API L"BthAGATSetCallback"
#define BTATHANDLER_HANDLER_API     L"BthAGATHandler"
#define BTAGEXT_ON_VOICETAG         L"BthAGOnVoiceTag"

//  Match indicators reported in CIND.
#define SERVICE_EVENT           0x01
#define CALL_EVENT              0x02
#define CALL_SETUP_EVENT        0x03
#define CALL_HELD_EVENT         0x04
#define SIGNAL_STRENGTH_EVENT   0x05
#define ROAM_EVENT              0x06
#define BATT_STRENGTH_EVENT     0x07

typedef enum _AG_STATE {
    AG_STATE_DISCONNECTED = 0x00,
    AG_STATE_CONNECTING = 0x01,
    AG_STATE_CONNECTED = 0x02,
    AG_STATE_RINGING = 0x03,
    AG_STATE_AUDIO_UP = 0x04,
    AG_STATE_RINGING_AUDIO_UP = 0x5
} AG_STATE;

typedef enum _AG_CALL_TYPE {
    AG_CALL_TYPE_NONE = 0x00,
    AG_CALL_TYPE_IN = 0x01,
    AG_CALL_TYPE_OUT = 0x02
} AG_CALL_TYPE;

typedef struct _AG_PROPS {
    DWORD state;
    USHORT usCallType;

    AG_DEVICE DeviceList[MAX_PAIRINGS];
    BT_ADDR btAddrClient;
    
    USHORT usSpeakerVolume;
    USHORT usMicVolume;
    USHORT usRemoteFeatures;
    USHORT usCallSetup;
    USHORT usHFCapability;
    USHORT usPageTimeout;  

    // Sniff parameters
    ULONG ulSniffDelay;
    USHORT usSniffMax;
    USHORT usSniffMin;
    USHORT usSniffAttempt;
    USHORT usSniffTimeout;
    
    // eSCO params
    unsigned int txBandwidth;
    unsigned int rxBandwidth;
    unsigned short maxLatency;
    unsigned short voiceSetting;
    unsigned char retransmission;    

    // AG Flags
    BOOL fInCall : 1;
    BOOL fPCMMode : 1;
    BOOL fAuth : 1;
    BOOL fEncrypt : 1;
    BOOL fNotifyCallWait : 1;
    BOOL fNotifyCLI : 1;
    BOOL fIndicatorUpdates : 1;
    BOOL fHandsfree : 1;  
    BOOL fNoHandsfree : 1;
    BOOL fHaveService : 1;
    BOOL fPowerSave : 1;
    BOOL fMuteRings : 1;
    BOOL fUseHFAudio : 1;
    BOOL fNoRoleSwitch : 1;
    BOOL fUnattendedMode : 1;
    BOOL fUnattendSet : 1;
    BOOL fUseHFAudioAlways : 1;
    BOOL fConnectHFOnCall : 1;
    BOOL fESCOSupport : 1;
    BOOL fUseESCO : 1;
    BOOL fRoam : 1;
    BOOL fOperatorFormatLongAlphaNumeric : 1;   
} AG_PROPS, *PAG_PROPS;


extern HINSTANCE g_hInstance;
extern CRITICAL_SECTION g_csLock;
extern DWORD g_dwState;

typedef void (*PFN_BthAGATSetCallback) (PFN_SendATCommand pfn);
typedef BOOL (*PFN_BthAGATHandler) (LPSTR szCommand, DWORD cbCommand);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof(a[0]))
#endif

class CATParser;

class CAGEngine : public SVSSynch, public SVSRefObj {
public:
    CAGEngine(void);

    DWORD Init(CATParser* pParser);
    void Deinit(void);
    
    void GetAGProps(PAG_PROPS pAGProps);
    void SetAGProps(PAG_PROPS pAGProps);

    DWORD OpenAGConnection(BOOL fOpenAudio, BOOL fFirstOnly);
    void CloseAGConnection(BOOL fCloseControl);
    void DisconnectionEvent(unsigned short connection_handle);
    DWORD NotifySCOConnect(unsigned short connection_handle);
    DWORD NotifyConnect(SOCKET sockClient, BOOL fHFSupport);
    BOOL FindBTAddrInList(const BT_ADDR btaSearch);

    void SetSpeakerVolume(unsigned short usVolume);
    void SetUseHFAudio(BOOL fUseHFAudio);
    void SetUseInbandRing(BOOL fUseInbandRing);
    void SetMicVolume(unsigned short usVolume);
    DWORD SetBTAddrList(BT_ADDR btAddr, BOOL fHFSupport);

    // General handlers
    void OnUnknownCommand(void);
    void OnSpeakerVol(LPSTR pszParams, int cchParam);
    void OnMicVol(LPSTR pszParams, int cchParam);
    
    // Headset handlers
    void OnHeadsetButton(LPSTR pszParams, int cchParam);    

    // Hands-Free handlers
    void OnDial(LPSTR pszParams, int cchParam);
    void OnDialLast(LPSTR pszParams, int cchParam);
    void OnDialMemory(LPSTR pszParams, int cchParam);
    void OnAnswerCall(LPSTR pszParams, int cchParam);
    void OnHangupCall(LPSTR pszParam, int cchParam);
    void OnDTMF(LPSTR pszParams, int cchParam);
    void OnCallHold(LPSTR pszParams, int cchParam);
    void OnSupportedFeatures(LPSTR pszParams, int cchParam);
    void OnTestIndicators(LPSTR pszParams, int cchParam);
    void OnReadIndicators(LPSTR pszParams, int cchParam);
    void OnRegisterIndicatorUpdates(LPSTR pszParams, int cchParam);
    void OnEnableCallWaiting(LPSTR pszParams, int cchParam);
    void OnEnableCLI(LPSTR pszParams, int cchParam);
    void OnVoiceRecognition(LPSTR pszParams, int cchParam);
    void OnRespondAndHold (LPSTR pszParams, int cchParam);
    void OnSubscriberNumberInformation (LPSTR pszParams, int cchParam);
    void OnCurrentCallList (LPSTR pszParams, int cchParam);
    void OnOperatorSelectionFormat (LPSTR pszParams, int cchParam);
    void OnOperatorSelection (LPSTR pszParams, int cchParam);
    void OnOK(void);
    void OnError(void);

    // Called from Network Module
    void OnNetworkEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam);

    // Called from PhoneExt Module
    void OnServiceCallback(BOOL fHaveService);
    void OnPhoneEvent(DWORD dwEvent, LPVOID lpvParam, DWORD cbParam);
    
    static DWORD WINAPI AGUIThread(LPVOID pv);
    void AGUIThread_Int(void);

    static DWORD WINAPI TimeoutConnection(LPVOID pv);
    void TimeoutConnection_Int(void);

    static DWORD WINAPI EnterSniffModeThread(LPVOID pv);
    void EnterSniffModeThread_Int(void);

    DWORD ExternalSendATCommand(LPSTR pszCommand, unsigned int cbCommand);
        
private:
    DWORD SendATCommand(LPCSTR pszCommand);
    DWORD SendATCommandWithLength(LPCSTR pszCommand, unsigned int cbCommand);
    DWORD OpenControlChannel(BOOL fFirstOnly, BOOL fConnectHSOnly);
    DWORD OpenAudioChannel(void);
    void CloseControlChannel(void);
    DWORD CloseAudioChannel(void); 
    void ServiceConnectionUp();
    void EnsureActiveBaseband(void);    
    void ClearSCO(void);
    void SendA2DPStart(void);
    void SendA2DPSuspend(void);
    BOOL GetA2DPDeviceID(UINT* pDeviceId);
    DWORD MapNetworkCallStateToCallStatus(DWORD dwNetworkCallState);

    void AllowSuspend(void);
    void PreventSuspend(void);
    void StartSniffModeTimer(void);
    void StopSniffModeTimer(void);    

    void OnNetworkAnswerCall(void);
    void OnNetworkHangupCall(DWORD dwRemainingCalls, DWORD dwHangUpDelayMS);
    void OnNetworkRejectCall(void);
    void OnNetworkCallIn(LPSTR pszNumber);
    void OnNetworkInfo(LPSTR pszNumber);
    void OnNetworkCallOut(void);
    void OnNetworkRing(void);
    void OnNetworkHeldCall(DWORD dwState);
    void OnNetworkCallFailed(USHORT usCallType, DWORD dwStatus);

    void SendIndicatorEvent(DWORD dwEvent, DWORD dwValue, DWORD dwMin, DWORD dwMax);
    void SendIndicatorEvent(DWORD dwEvent, DWORD dwValue);

    PFN_BthAGOnVoiceTag GetVoiceTagHandler(void);

    SVSThreadPool* m_pTP;
    SVSCookie m_CloseCookie;
    SVSCookie m_SniffCookie;
    AG_PROPS m_AGProps; 
    HANDLE m_hUIThread;
    HANDLE m_hHangupEvent;
    SOCKET m_sockClient;
    unsigned short m_hSco;
    CATParser* m_pParser;
    CHAR m_szCLI[MAX_PHONE_NUMBER];
    BOOL m_fCancelCloseConnection;
    BOOL m_fExpectHeadsetButton;
    BOOL m_fAudioConnect;
    BOOL m_fNetworkInited;
    BOOL m_fPhoneExtInited;
    BOOL m_fA2DPSuspended;
};

extern CAGEngine* g_pAGEngine;

class CATParser : public SVSSynch {
public:
    CATParser();

    static DWORD WINAPI ATParserThread(LPVOID pv);
    void ATParserThread_Int(void);

    DWORD Init(void);
    void Deinit(void);        

    DWORD Start(CAGEngine* pHandler, SOCKET sockClient);
    void Stop(void);

    PFN_BthAGOnVoiceTag GetVoiceTagHandler() { return m_pfnBthAGOnVoiceTag; }
    
private:
    DWORD ReadCommand(SOCKET s, CBuffer& buffCommand);
        
    SOCKET m_sockClient;
    CAGEngine* m_pHandler;
    BOOL m_fShutdown;
    HANDLE m_hThread;
    CBuffer m_buffRecv;
    int m_cbUnRead;

    // AG extension
    HINSTANCE m_hBtExtHandler;
    PFN_BthAGATSetCallback m_pfnBthAGExtATSetCallback;
    PFN_BthAGATHandler m_pfnBthAGExtATHandler;
    PFN_BthAGOnVoiceTag m_pfnBthAGOnVoiceTag;

    // Phone book download
    HINSTANCE m_hPBHandler;
    PFN_BthAGATSetCallback m_pfnBthAGPBSetCallback;
    PFN_BthAGATHandler m_pfnBthAGPBHandler;    
};


class CAGService : public SVSSynch {
public:
    CAGService();

    DWORD Init(void);
    void Deinit(void);
    DWORD Start(void);
    void Stop(void);
    DWORD OpenAudio(void);
    DWORD CloseAudio(void);
    DWORD OpenControlConnection(BOOL fFirstOnly);
    DWORD CloseControlConnection(void);
    DWORD SetSpeakerVolume(unsigned short usVolume);
    DWORD SetMicVolume(unsigned short usVolume);
    DWORD GetSpeakerVolume(unsigned short* pusVolume);
    DWORD GetMicVolume(unsigned short* pusVolume);
    DWORD GetPowerMode(BOOL* pfPowerSave);
    DWORD SetPowerMode(BOOL fPowerSave);
    DWORD SetUseHFAudio(BOOL fUseHFAudio);
    DWORD SetUseInbandRing(BOOL fUseInbandRing);
        
    static DWORD WINAPI ListenThread(LPVOID pv);
    void ListenThread_Int(void);
    
    static DWORD WINAPI NotifyListenThread(LPVOID pv);
    void NotifyListenThread_Int(void);

private:
    DWORD LoadAGState(void);
    DWORD SaveAGState(void);
    DWORD AddSDPRecord(PBYTE rbgSdpRecord, DWORD cbSdpRecord, DWORD dwChannelOffset, unsigned long ulPort, unsigned long* pSdpRecord);
    DWORD RemoveSDPRecord(unsigned long* pSdpRecord);
   
    CATParser m_ATParser;
    CAGEngine m_AGEngine;
    HANDLE m_hThread;
    HANDLE m_hScoThread;
    HANDLE m_hCloseEvent;
    SOCKET m_sockServer[2];
    BOOL m_fShutdown;
    ULONG m_SDPRecordHS;
    ULONG m_SDPRecordHF;
    DWORD m_dwHFCapability;
};


#endif // __AUDIOGW_H__

