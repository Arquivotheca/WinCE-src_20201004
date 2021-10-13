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
// Header file private to AVDTP implementation

#include <windows.h>

#include <svsutil.hxx>

#include <list.hxx>
#include <block_allocator.hxx>

#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_debug.h>
#include <bt_api.h>
#include <bthapi.h>
#include <bt_sdp.h>

#ifndef BOOLIFY
#define BOOLIFY(e)      (!!(e))
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x)    (sizeof(x)/sizeof(x[0]))
#endif

#define FIXED_BUFFER_SIZE               128     // Avoid dynamic allocations smaller than this

#define AVDTP_L2CAP_MTU_IN              L2CAP_MTU

#define AVDTP_HCI_BUFF_THRESHOLD        20
#define AVDTP_HCI_BUFF_THRESHOLD_SLEEP  250

// AVDTP signal ids
#define AVDTP_SIGNAL_DISCOVER           0x01
#define AVDTP_SIGNAL_GET_CAPABILITIES   0x02
#define AVDTP_SIGNAL_SET_CONFIGURATION  0x03
#define AVDTP_SIGNAL_GET_CONFIGURATION  0x04
#define AVDTP_SIGNAL_RECONFIGURE        0x05
#define AVDTP_SIGNAL_OPEN               0x06
#define AVDTP_SIGNAL_START              0x07
#define AVDTP_SIGNAL_CLOSE              0x08
#define AVDTP_SIGNAL_SUSPEND            0x09
#define AVDTP_SIGNAL_ABORT              0x0A
#define AVDTP_SIGNAL_SECURITY_CONTROL   0x0B

// AVDTP channels
#define AVDTP_CHANNEL_NONE              0x00
#define AVDTP_CHANNEL_SIGNALING         0x01
#define AVDTP_CHANNEL_MEDIA             0x02

// AVDTP errors
#define AVDTP_ERROR_SUCCESS                     0x00
#define AVDTP_ERROR_BAD_HEADER_FORMAT           0x01
#define AVDTP_ERROR_BAD_LENGTH                  0x11
#define AVDTP_ERROR_BAD_SEID                    0x12
#define AVDTP_ERROR_SEP_IN_USE                  0x13
#define AVDTP_ERROR_SEP_NOT_IN_USE              0x14
#define AVDTP_ERROR_BAD_SERV_CATEGORY           0x17
#define AVDTP_ERROR_BAD_PAYLOAD_FORMAT          0x18
#define AVDTP_ERROR_NOT_SUPPORTED               0x19
#define AVDTP_ERROR_INVALID_CAPABILITIES        0x1A
#define AVDTP_ERROR_BAD_MEDIA_TRANSPORT_FORMAT  0x23
#define AVDTP_ERROR_BAD_STATE                   0x31
#define AVDTP_ERROR_UNDEFINED                   0xFF

// AVDTP session stages
#define NONE                0x00
#define CONNECTED           0x01
#define CONFIG_REQ_DONE     0x02
#define CONFIG_IND_DONE     0x04
#define UP                  0x07

#define RECV_BUFFER_SIZE    2048

namespace AVDT_NS 
{

struct DiscoverResponse {
    AVDTP_ENDPOINT_INFO EPInfo[8]; // limit the number of endpoints to 8 per extension
    DWORD cEndpoints;
    BYTE bError;
};

typedef ce::list<DiscoverResponse, ce::fixed_block_allocator<5> >  DiscoverResponseList;

//
// This structure is stored in a session.  It is necessary when a response is sent to AVDTP
// from the upper layer that references context from a previous request based solely on
// the transaction id.
//
struct TransactionContext {
    BYTE bTransaction;
    BYTE bSignalId;

    // This is for discover command/response
    DiscoverResponseList drl;    

    union {
        struct {
            BYTE bAcpSEID;
        } GetCapabilities;
    } u;

    TransactionContext(void)
    {
        bTransaction = 0;
        bSignalId = 0;    
    }
};

typedef ce::list<TransactionContext, ce::fixed_block_allocator<5> >  TransactionContextList;

//
// A session object abstracts all the data associated with the L2CAP channel.  Sessions can
// be using the signaling channel or media channel.  A fully open stream will have both a
// signaling session and a media session.  One signaling session may be linked to multiple
// streams.
//
struct Session {
    HANDLE          hContext;           // Unique context handle
        
    BD_ADDR         ba;                 // Peer address
    unsigned short  cid;                // channel id

    BYTE            bChannelType;       // Signaling || Media
    BOOL            fIncoming;          // Who initiated the connection
    UCHAR           l2cap_id;           // Id from l2cap connect

    unsigned short  usInMTU;
    unsigned short  usOutMTU;

    unsigned int    fStage;             // State of the session

    int             EnsureActiveCount;  // Ref count of ensure active calls

    DWORD           TimeoutCookie;      // Handle to timer for timing out session
    DWORD           SniffCookie;        // Handle to sniff timeout thread
    DWORD           ActiveModeCookie;   // Handle to active mode thread

    PBYTE           pRecvBuffer;        // Buffer to hold fragmented recv packets
    unsigned int    cbRecvBuffer;       // Length of the receive buffer
    unsigned int    cRemainingFrags;    // Number of continue packets remaining

    BYTE            bTransaction;       // Signaling transaction id

    TransactionContextList tcl;

    Session(BD_ADDR* pba, unsigned int Stage, unsigned short Cid, BYTE ChannelType, BOOL Incoming, UCHAR id) {
        ba = *pba;
        fStage = Stage;
        cid = Cid;
        bChannelType = ChannelType;
        fIncoming = Incoming;
        l2cap_id = id;

        hContext = NULL;
        bChannelType = AVDTP_CHANNEL_NONE;
        usInMTU = AVDTP_L2CAP_MTU_IN;
        usOutMTU = 0;

        pRecvBuffer = NULL;
        cbRecvBuffer = 0;
        cRemainingFrags = 0;
        
        bTransaction = 0;

        TimeoutCookie = 0;
        SniffCookie = 0;
        ActiveModeCookie = 0;

        EnsureActiveCount = 0;
    }

    ~Session(void) {
        delete[] pRecvBuffer;
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);      
};

enum StreamStateEnum {
    STREAM_STATE_IDLE,
    STREAM_STATE_CONFIGURED,
    STREAM_STATE_OPEN,
    STREAM_STATE_STREAMING,
    STREAM_STATE_CLOSING,
    STREAM_STATE_ABORTING
};

class StreamState {
public:
    StreamState(void)
    {
        Reset();
    }

    // 
    // The following methods transition an AVDTP stream from one
    // state to another.
    //

    inline void Idle(void) 
    {
        ASSERT((m_state == STREAM_STATE_ABORTING) || (m_state == STREAM_STATE_CLOSING));        
        m_state = STREAM_STATE_IDLE; 
    }
    
    inline void Configured(void) 
    { 
        ASSERT (m_state == STREAM_STATE_IDLE);        
        m_state = STREAM_STATE_CONFIGURED; 
    }
    
    inline void Open(void) 
    { 
        ASSERT ((m_state == STREAM_STATE_STREAMING) || (m_state == STREAM_STATE_CONFIGURED));
        m_state = STREAM_STATE_OPEN; 
    }
    
    inline void Streaming(void) 
    { 
        ASSERT(m_state == STREAM_STATE_OPEN);
        m_state = STREAM_STATE_STREAMING;         
    }
    
    inline void Closing(void) 
    { 
        m_state = STREAM_STATE_CLOSING; 
    }
    
    inline void Aborting(void) 
    { 
        m_state = STREAM_STATE_ABORTING; 
    }
    
    inline void Reset(void) 
    { 
        m_state = STREAM_STATE_IDLE; 
    }


    //
    // The following methods check the state of the AVDTP stream.
    //
    
    inline BOOL IsConfigured(void)
    {
        if ((m_state >= STREAM_STATE_CONFIGURED) && (m_state <= STREAM_STATE_STREAMING)) {
            return TRUE;
        }

        return FALSE;
    }

    inline BOOL IsOpen(void)
    {
        if ((m_state == STREAM_STATE_OPEN) || (m_state == STREAM_STATE_STREAMING)) {
            return TRUE;
        }

        return FALSE;
    }

    inline BOOL IsStreaming(void)
    {
        if (m_state == STREAM_STATE_STREAMING) {
            return TRUE;
        }

        return FALSE;
    }

    inline BOOL IsInState(DWORD dwState)
    {
        if (m_state == dwState) {
            return TRUE;
        }

        return FALSE;
    }
    
private:
    DWORD m_state;
};

//
// A stream contains all the data associated with an AVDTP stream.  It maintains a
// state variable and can refer to two sessions (signaling and media).  A stream
// is associated with a particular ACP SEID and INT SEID and is referenced by its
// hContext member when communicating with the upper layer.  This object is created
// on GetCapabilities when the upper layer takes ownership of a particular SEID.
//
struct Stream {
    StreamState             state;          // State of the stream

    HANDLE                  hOwner;         // Handle to owner context
    HANDLE                  hContext;       // Unique handle to reference stream

    HANDLE                  hSessSignal;    // Handle to the signaling session
    HANDLE                  hSessMedia;     // Handle to the media session

    DWORD                   TimeoutCookie;  // Handle to timer for timing out stream

    BOOL                    fInitiator;     // TRUE if initiator

    BYTE                    bAcpSeid;       // ACP SEID
    BYTE                    bIntSeid;       // INT SEID

    USHORT                  usSeqNum;       // RTP seq number
    DWORD                   dwBitsPerSec;   // Bit rate of this stream    
    DWORD                   cbCodecHeader;  // Codec header

    DWORD                   dwLastNotifiedEvent;

    void*                   pOpenCallContext;

    TransactionContextList  tcl;

    // Media flow control params
    DWORD                   dwPendingBytes;
    DWORD                   dwNextSlotStartTime;
    DWORD                   dwLowThresholdBytes;
    DWORD                   dwHighThresholdBytes;
    DWORD                   dwHciBuffThreshold;
    DWORD                   dwHciBuffThresholdSleep;
    
    BOOL                    fHciBuffOverflow;

    Stream(BYTE AcpSeid, HANDLE Owner, HANDLE SessSignal)
    {
        hOwner = Owner;
        hContext = NULL;
        hSessSignal = SessSignal;
        hSessMedia = NULL;

        TimeoutCookie = 0;
        
        dwBitsPerSec = 0;
        usSeqNum = 1;
        cbCodecHeader = 0;

        fInitiator = FALSE;
        bIntSeid = 0;        
        bAcpSeid = AcpSeid;

        pOpenCallContext = NULL;

        dwPendingBytes = 0;
        dwNextSlotStartTime = 0;
        dwLowThresholdBytes = 0;
        dwHighThresholdBytes = 0;
        dwHciBuffThreshold = AVDTP_HCI_BUFF_THRESHOLD;
        dwHciBuffThresholdSleep = AVDTP_HCI_BUFF_THRESHOLD_SLEEP;
        dwLastNotifiedEvent = 0;

        fHciBuffOverflow = FALSE;
    }  

    void *operator new (size_t iSize);
    void operator delete (void *ptr);      
};

}; // End namespace

// Everything in the AVDTP project should be using this namespace.  We need this
// so objects don't collide in other parts of the BT stack.
using namespace AVDT_NS;

class AVDTSignal {
public:
    AVDTSignal(void);

    int Init(void);
    void Deinit(void);

    int DiscoverRequest(
            Session* pSession, 
            LPVOID lpvCallContext, 
            BD_ADDR* pba);
            
    int DiscoverResponse(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_ENDPOINT_INFO pSEIDInfo,
            DWORD cSEIDInfo,
            BYTE bError);

    int GetCapabilitiesRequest(
            Session* pSession, 
            LPVOID lpvContext,
            BYTE bAcpSEID);

    int GetCapabilitiesResponse(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs,
            BYTE bError);

    int SetConfigurationRequest(
            Session* pSession, 
            LPVOID lpvCallContext, 
            BYTE bAcpSEID,
            BYTE bIntSEID,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs);

    int SetConfigurationResponse(
            Session* pSession,  
            BYTE bTransaction,
            BYTE bServiceCategory,
            BYTE bError);

    int GetConfigurationRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvCallContext);

    int GetConfigurationResponse(
            Session* pSession, 
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs,
            BYTE bError);

    int OpenStreamRequest(
            Stream* pStream, 
            Session* pSess,
            LPVOID lpvCallContext);

    int OpenStreamResponse(
            Session* pSess,
            BYTE bTransaction,
            BYTE bError);

    int CloseStreamRequest(
            Stream* pStream,
            Session* pSession, 
            LPVOID lpvCallContext);

    int CloseStreamResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bError);

    int StartStreamRequest(
            Session* pSession,
            LPVOID lpvCallContext, 
            Stream** ppStreamHandles,
            DWORD cStreamHandles);

    int StartStreamResponse(
            Session* pSession,
            BYTE bTransaction,
            Stream* hFirstFailedStream,
            BYTE bError);

    int SuspendRequest(
            Session* pSession,
            LPVOID lpvCallContext, 
            Stream** ppStreamHandles,
            DWORD cStreamHandles);

    int SuspendResponse(
            Session* pSession,
            BYTE bTransaction,
            Stream* hFirstFailedStream,
            BYTE bError);

    int ReconfigureRequest(
            Stream* pStream,
            Session* pSession,
            LPVOID lpvCallContext, 
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIEs);

    int ReconfigureResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bServiceCategory,
            BYTE bError);

    int SecurityControlRequest(
            Session* pSession, 
            LPVOID lpvCallContext, 
            PBYTE pSecurityControlData,
            USHORT cbSecurityControlData);

    int SecurityControlResponse(
            Session* pSession,
            BYTE bTransaction,
            PBYTE pSecurityControlData,
            USHORT cbSecurityControlData,
            BYTE bError);

    int AbortRequest(
            Stream* pStream,
            Session* pSession,
            LPVOID lpvCallContext);

    int AbortResponse(
            Session* pSession, 
            BYTE bTransaction,
            BYTE bError);

    int ProcessPacket(
            Session* pSession,
            BD_BUFFER* pBuffer);
    
private:
    BYTE GetNosp(
            DWORD cbPayload, 
            DWORD mtu);

    DWORD GetHeaderSize(
            BYTE bPacketType);
    
    BD_BUFFER* GetSignalBuffer(
            Session* pSession,
            BOOL fContinue,
            DWORD cbTotal, 
            BYTE bSignalId, 
            BYTE bMsgType,
            PBYTE pbPacketType);

    int CopyFragment(
            Session* pSession, 
            BD_BUFFER* pBuffer);

    DWORD GetCapabilityBufferSize(
            PAVDTP_CAPABILITY_IE pCapabilityIE, 
            DWORD cCapabilityIEs);

    int SerializeCapabilityBuffer(
            PAVDTP_CAPABILITY_IE pCapabilityIE, 
            DWORD cCapabilityIEs, 
            PBYTE pbConfig, 
            DWORD cbPayload);

    int SerializeDiscoverBuffer(
            PAVDTP_ENDPOINT_INFO pSEIDInfo, 
            DWORD cSEIDInfo, 
            PBYTE pbDisc, 
            DWORD cbPayload);

    int RecvServiceCapability(
            BD_BUFFER* pBuffer,
            PAVDTP_CAPABILITY_IE pCapability,
            PDWORD pcCapabilityIE,
            BOOL fAllowAllCategories,
            PBYTE bFailedCategory);

    int OnDiscover(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnGetCapabilities(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnSetConfiguration(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnGetConfiguration(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnReconfigure(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnOpenStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnCloseStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnStartStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnSuspendStream(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnSecurityControl(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnAbort(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);

    int OnUnknownSignalId(
            Session* pSession, 
            BD_BUFFER* pBuffer,
            BYTE bMsgType,
            BYTE bTransaction);        

};

class AVDTMedia {
public:
    AVDTMedia(void);

    int Init(DWORD dwLowThreshold, DWORD dwHighThreshold);
    void Deinit(void);

    DWORD PauseStream(
            HANDLE hStream, 
            HANDLE hSession, 
            DWORD dwMs);

    int WriteRequest(
            Stream* pStream,
            Session* pSession,
            BD_BUFFER* pBuffer,
            DWORD dwTime,
            BYTE bPayloadType,
            USHORT usMarker);

    int ProcessPacket(
            Session* pSession,
            BD_BUFFER* pBuffer);    

private:
    // AVDTP media flow control has the concept of thresholds.  The are being
    // expressed in milliseconds below but can also be expressed in bytes.
    // These values corresond to the amount of data (in ms) AVDTP will allow
    // itself to get ahead based on the bit rate.
    DWORD m_dwLowThresholdMs, m_dwHighThresholdMs;

};


BYTE DiscoverRequestUp(
            Session* pSession, 
            BYTE bTransaction);

int DiscoverResponseUp(
            Session* pSession, 
            BYTE bTransaction,
            PAVDTP_ENDPOINT_INFO pAcpEPInfo, 
            DWORD cAcpEPInfo, 
            BYTE bError);

BYTE GetCapabilitiesRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID);

int GetCapabilitiesResponseUp(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIE,
            BYTE bError);       

BYTE SetConfigurationRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID,
            BYTE bIntSEID,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIE,
            PBYTE pbFailedCategory);

int SetConfigurationResponseUp(
            Session* pSession, 
            BYTE bTransaction, 
            BYTE bServiceCategory, 
            BYTE bError);

BYTE GetConfigurationRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID);

int GetConfigurationResponseUp(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIE,
            BYTE bError);

BYTE OpenRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID);

int OpenResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError);

BYTE CloseRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID);

int CloseResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError);

BYTE StartRequestUp(
            Session* pSession,
            BYTE bTransaction,
            PBYTE pbSEIDList,
            DWORD cbSEIDList);

int StartResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bFirstFailingSEID,
            BYTE bError);

BYTE SuspendRequestUp(
            Session* pSession,
            BYTE bTransaction,
            PBYTE pbSEIDList,
            DWORD cbSEIDList);

int SuspendResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bFailedSEID,
            BYTE bError);

BYTE ReconfigureRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIE,
            PBYTE pbFailedCategory);

int ReconfigureResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bServiceCategory,
            BYTE bError);

BYTE SecurityControlRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID,
            USHORT cbSecurityControlData,
            PBYTE pSecurityControlData);

int SecurityControlResponseUp(
            Session* pSession,
            BYTE bTransaction,
            USHORT cbSecurityControlData,
            PBYTE pSecurityControlData,
            BYTE bError);

BYTE AbortRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID);

int AbortResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError);

int StreamDataUp(
            Session* pSession,
            BD_BUFFER* pBuffer,
            DWORD dwTimeInfo,
            BYTE bPayloadType,
            USHORT usMarker,
            USHORT usReliability);


void avdtp_lock(void);
void avdtp_unlock(void);
BOOL avdtp_IsLocked(void);
void avdtp_addref(void);
void avdtp_delref(void);

int L2CAPSend(LPVOID lpvContext, USHORT cid, BD_BUFFER* pBuffer, Session* pSession, BOOL fUnSniff, BOOL fScheduleSniff);
BD_BUFFER* GetAVDTPBuffer(DWORD cbTotal);
BYTE GetSEID(Stream* pStream, BOOL fInitiator);

Stream* VerifyStream(HANDLE hStream);
Session* VerifySession(HANDLE hSession);


