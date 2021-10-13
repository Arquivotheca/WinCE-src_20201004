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
/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Abstract:
    Windows CE Bluetooth A2DP_t layer 

**/

#include "btha2dp.h"
#include "BthA2dpCodec.h"
#include "Btha2dp_debug.h"
#include "hwctxt.h"
#include <winsock2.h>
#include "BtA2dpPair.h"
#include <windows.h>
#include <list.hxx>
#include <svsutil.hxx>



#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_debug.h>
#include <bt_api.h>
#include <bt_tdbg.h>
#include <bthapi.h>
#include <bt_sdp.h>
#include <pkfuncs.h>
#include <intsafe.h>


#include "FunctionTrace.h"


#if defined (SDK_BUILD)
#undef CP_UTF8
#endif

#define BT_NO_ERROR 0
#define BT_ERROR 1

#define STREAM_END_POINT_CAPS_COUNT 2
#define START_AFTER_OPEN_IND_DELAY  1000

DECLARE_DEBUG_VARS();


#ifndef ARRAYSIZE
#define ARRAYSIZE(x)    (sizeof(x)/sizeof(x[0]))
#endif

BYTE MAX_SUPPORTED_BIT_POOL = MAX_SUPPORTED_BIT_POOL_DEFAULT;
BYTE MIN_SUPPORTED_BIT_POOL = MIN_SUPPORTED_BIT_POOL_DEFAULT;
DWORD CHANNEL_MODE  = SBC_CHANNEL_MODE_STEREO;

//
// Need typedefs for AVDTP_EstablisDeviceContext and AVDTP_CloseDeviceContext because
// we do LoadLibrary on btd.dll to invoke those functions.
//

typedef int (*AVDTP_EstablisDeviceContext_t)
(
    void*                   pUserContext,   /* IN */
    PAVDTP_EVENT_INDICATION pInd,           /* IN */
    PAVDTP_CALLBACKS        pCall,          /* IN */
    PAVDTP_INTERFACE        pInt,           /* OUT */
    int*                    pcDataHeaders,  /* OUT */
    int*                    pcDataTrailers, /* OUT */
    HANDLE*                 pDeviceContext  /* OUT */
);

typedef int (*AVDTP_CloseDeviceContext_t)
(
    HANDLE DeviceContext      /* IN */
);


//
// Call definitions
//

//
// init/deinit functions
//
int a2dp_InitializeOnce      (void);
int a2dp_UninitializeOnce    (void);
int a2dp_CreateDriverInstance(void);
int a2dp_CloseDriverInstance (void);

//
// stack notification/init routines
//
static int          StackNotifyUser (int iEvent);
static DWORD WINAPI StackDisconnect (LPVOID lpVoid);
static DWORD WINAPI StackDown       (LPVOID lpVoid);
static DWORD WINAPI StackUp         (LPVOID lpVoid);


//helper
void AdaptAudioDMAPageSize();


//
// event handlers
//
static int a2dp_Discover_Ind         (void* pUserContext, BYTE Transaction, BD_ADDR* pba);
static int a2dp_GetCapabilities_Ind  (void* pUserContext, BYTE Transaction, BD_ADDR* pba, BYTE bAcpSEID);
static int a2dp_SetConfiguration_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream, BD_ADDR* pba, BYTE bAcpSEID, BYTE bIntSEID, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);
static int a2dp_GetConfiguration_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream);
static int a2dp_Open_Ind             (void* pUserContext, BYTE Transaction, HANDLE Stream);
static int a2dp_Close_Ind            (void* pUserContext, BYTE Transaction, HANDLE Stream);
static int a2dp_Start_Ind            (void* pUserContext, BYTE Transaction, HANDLE* pStreamHandles, DWORD cStreamHandles);
static int a2dp_Suspend_Ind          (void* pUserContext, BYTE Transaction, HANDLE* pStreamHandles, DWORD cStreamHandles);
static int a2dp_Reconfigure_Ind      (void* pUserContext, BYTE Transaction, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);
static int a2dp_Abort_Ind            (void* pUserContext, BYTE Transaction, HANDLE Stream);
static int a2dp_StackEvent           (void *pUserContext, int iEvent, void *pEventContext);
static int a2dp_StreamAbortedEvent   (void* pUserContext, HANDLE hStream, int Error);

//
// callbacks
//
static int a2dp_Discover_Out         (void* pCallContext, BD_ADDR* pba, PAVDTP_ENDPOINT_INFO pAcpSEIDInfo, DWORD cAcpSEIDInfo, BYTE Error);
static int a2dp_GetCapabilities_Out  (void* pCallContext, BD_ADDR* pba, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE, BYTE Error);
static int a2dp_SetConfiguration_Out (void* pCallContext, HANDLE Stream, BYTE bServiceCategory, BYTE Error);
static int a2dp_GetConfiguration_Out (void* pCallContext, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE, BYTE Error);
static int a2dp_Open_Out             (void* pCallContext, HANDLE Stream, BYTE Error);
static int a2dp_Close_Out            (void* pCallContext, HANDLE Stream, BYTE Error);
static int a2dp_Start_Out            (void* pCallContext, HANDLE hFirstFailingStream, BYTE Error);
static int a2dp_Suspend_Out          (void* pCallContext, HANDLE hFirstStream, HANDLE hFirstFailingStream, BYTE Error);
static int a2dp_Reconfigure_Out      (void* pCallContext, HANDLE Stream, BYTE bServiceCategory, BYTE Error);
static int a2dp_Abort_Out            (void* pCallContext, HANDLE Stream, BYTE Error);
static int a2dp_CallAborted          (void *pCallContext, int iError);


//
// interface
//
static int a2dp_DiscoverReq_In         (HANDLE DeviceContext, void* pCallContext, BD_ADDR* pba);
static int a2dp_DiscoverRsp_In         (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, BD_ADDR* pba, PAVDTP_ENDPOINT_INFO pEPInfo, DWORD cEndpoints, BYTE Error);
static int a2dp_GetCapabilitiesReq_In  (HANDLE DeviceContext, void* pCallContext, BD_ADDR* pba, BYTE bAcpSEID);
static int a2dp_GetCapabilitiesRsp_In  (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, BD_ADDR* pba, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE, BYTE Error);
static int a2dp_SetConfigurationReq_In (HANDLE DeviceContext, void* pCallContext, BD_ADDR* pba, BYTE bAcpSEID, BYTE bIntSEID, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);
static int a2dp_SetConfigurationRsp_In (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, BYTE bServiceCategory, BYTE Error);
static int a2dp_GetConfigurationReq_In (HANDLE DeviceContext, void* pCallContext, HANDLE Stream);
static int a2dp_GetConfigurationRsp_In (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE, BYTE Error);
static int a2dp_OpenReq_In             (HANDLE DeviceContext, void* pCallContext, HANDLE Stream);
static int a2dp_OpenRsp_In             (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, BYTE Error);
static int a2dp_CloseReq_In            (HANDLE DeviceContext, void* pCallContext, HANDLE Stream);
static int a2dp_CloseRsp_In            (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, BYTE Error);
static int a2dp_StartReq_In            (HANDLE DeviceContext, void* pCallContext, HANDLE* pStreamHandles, DWORD cStreamHandles);
static int a2dp_StartRsp_In            (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE hFirstFailingStream, BYTE Error);
static int a2dp_SuspendReq_In          (HANDLE DeviceContext, void* pCallContext, HANDLE* pStreamHandles, DWORD cStreamHandles);
static int a2dp_SuspendRsp_In          (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE hFirstStream, HANDLE hFirstFailingStream, BYTE Error);
static int a2dp_ReconfigureReq_In      (HANDLE DeviceContext, void* pCallContext, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);
static int a2dp_ReconfigureRsp_In      (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, BYTE bServiceCategory, BYTE Error);
static int a2dp_AbortReq_In            (HANDLE DeviceContext, void* pCallContext, HANDLE Stream);
static int a2dp_AbortRsp_In            (HANDLE DeviceContext, void* pCallContext, BYTE Transaction, HANDLE Stream, BYTE Error);
static int a2dp_WriteReq_In            (HANDLE DeviceContext, HANDLE Stream, USHORT cbData, PBYTE pData, DWORD dwTimeInfo, BYTE bPayloadType, USHORT usMarker);
static int a2dp_ReadStreamData         (HANDLE DeviceContext, HANDLE Stream, USHORT cbBuffer, PBYTE pData, PUSHORT pActualLen, PDWORD pdwTimeInfo, PBYTE bPayloadType, PUSHORT pusMarker, PUSHORT pusReliability);

static int a2dp_Discover              (A2DP_USER_CALL* pUserCall, BD_ADDR DiscoverBdAddr);
static int a2dp_GetCapabilites        (A2DP_USER_CALL* pUserCall);
static int a2dp_SetConfiguration      (A2DP_USER_CALL* pUserCall);
static int a2dp_Open                  (A2DP_USER_CALL* pUserCall);
static int a2dp_Start                 (A2DP_USER_CALL* pUserCall);
static int a2dp_Suspend               (A2DP_USER_CALL* pUserCall);
static int a2dp_Close                 (A2DP_USER_CALL* pUserCall);
static int a2dp_Abort                 (A2DP_USER_CALL* pUserCall);
static int a2dp_Write                 (A2DP_USER_CALL* pUserCall);





#define CALL_TYPE_DISCONNECT                    22
#define CALL_TYPE_READ_VOICE_SETTING            32
#define CALL_TYPE_WRITE_VOICE_SETTING           42
#define CALL_TYPE_WAIT_FOR_INCOMING_CONNECTION  52
#define CALL_TYPE_ACCEPT_INCOMING_CONNECTION    62
#define CALL_TYPE_DATA_PACKET_DOWN              82
#define CALL_TYPE_WRITE_A2DP_FLOW_CONTROL       92
#define CALL_TYPE_READ_A2DP_FLOW_CONTROL        102

#define A2DP_CALL_TYPE_DISCOVER                 1
#define A2DP_CALL_TYPE_GET_CAPABILITIES         2
#define A2DP_CALL_TYPE_CONFIGURE                3
#define A2DP_CALL_TYPE_GET_CONFIGURATION        4
#define A2DP_CALL_TYPE_OPEN                     5
#define A2DP_CALL_TYPE_START                    6
#define A2DP_CALL_TYPE_SUSPEND                  7
#define A2DP_CALL_TYPE_CLOSE                    8
#define A2DP_CALL_TYPE_WRITE                    9
#define A2DP_CALL_TYPE_ABORT                    10


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
sdp record data

if changes are needed, refer to a2dp.rec
-------------------------------------------------------------------*/
const int cSdpRecord = 0x00000035;
BYTE rgbSdpRecord[] = {
        0x35, 0x33, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19,
        0x11, 0x0a, 0x09, 0x00, 0x04, 0x35, 0x10, 0x35,
        0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x19, 0x35,
        0x06, 0x19, 0x00, 0x19, 0x09, 0x01, 0x00, 0x09,
        0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x11,
        0x0d, 0x09, 0x01, 0x00, 0x09, 0x01, 0x00, 0x25,
        0x04, 0x41, 0x32, 0x44, 0x50
};

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Class:          Call_t 

Description:    Keeps information about calls sent down to the
                avdtp layer                
-------------------------------------------------------------------*/
class Call_t {
public:

    int             Result;
    BYTE            Error;
    unsigned char   CallType;
    A2DP_USER_CALL  *pUserCall;
    Call_t (void) {
        Result = 0;
        Error = 0;
        CallType = 0;

    }
};

typedef ce::list<Call_t*, ce::free_list<10> >  CallList;

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Class:          Stream_t 

Description:    Maintains state about a particular stream
                between this end point and remote endpoint
-------------------------------------------------------------------*/
class Stream_t {
public:

    enum StreamState_e {
        STREAM_STATE_CLOSED,
        STREAM_STATE_CONFIGURING,
        STREAM_STATE_CONFIGURED,
        STREAM_STATE_OPENED,
        STREAM_STATE_STREAMING,
        STREAM_STATE_ABORTING,  
        STREAM_STATE_IDLE,  
    };

private:
    StreamState_e m_State;
    HANDLE m_StreamHandle;

public:
    Stream_t(
        void
        );

    void 
    Reset(
        void
        );

    void     
    Configuring(
        void
        );
        
    void 
    Configured(
        HANDLE StreamHandle
        );
        
    void
    Opened(
        void
        ); 
        
    void 
    Streaming(
        void
        ); 

    void
    Suspended(
        void
        ); 
    BOOL 
    IsClosed(
        void
        ) const; 
        
    BOOL 
    IsConfigured(
        void
        ) const;

    BOOL 
    HasBeenConfigured(
        void
        ) const;
        
    BOOL 
    IsOpened(
        void
        ) const;

    BOOL 
    IsStreaming(
        void
        ) const;

    BOOL 
    IsSuspended(
        void
        ) const;


    HANDLE 
    GetStreamHandle(
        void
        ) const;
    

};

Stream_t::
Stream_t(void)
{
    Reset();
}

inline void 
Stream_t::Reset(void)        
{
    m_State = STREAM_STATE_CLOSED; 
    DEBUGMSG(ZONE_A2DP_TRACE, (L"A2DP: Stream_t :: Reset :: Stream_t: [0x%08x] stream handle\n", this, m_StreamHandle));
    m_StreamHandle = NULL;
    DEBUGMSG(ZONE_A2DP_TRACE, (L"A2DP: Stream_t :: Reset :: Stream_t: [0x%08x] state STREAM_STATE_CLOSED\n", this));

    
}

inline void 
Stream_t::
Configuring(
    void
    )        
{ 
    SVSUTIL_ASSERT(STREAM_STATE_IDLE == m_State ||
                   STREAM_STATE_CLOSED == m_State);
    m_State = STREAM_STATE_CONFIGURING; 
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP: Stream_t :: Configuring :: Stream_t: [0x%08x] state STREAM_STATE_CONFIGURING\n", this));

}

inline void 
Stream_t::
Configured(
    HANDLE StreamHandle
    )        
{ 
    m_State = STREAM_STATE_CONFIGURED; 
    m_StreamHandle = StreamHandle;
    DEBUGMSG(ZONE_A2DP_TRACE, (L"A2DP: Stream_t :: Configured :: Stream_t: [0x%08x] stream handle\n", this, m_StreamHandle));
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP: Stream_t :: Configured :: Stream_t: [0x%08x] state STREAM_STATE_CONFIGURED\n", this));

}

inline void 
Stream_t::
Opened(
    void
    )        
{ 
    m_State = STREAM_STATE_OPENED; 
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP: Stream_t :: Opened :: Stream_t: [0x%08x] state STREAM_STATE_OPENED\n", this));

}


inline void 
Stream_t::
Streaming(
    void
    )        
{ 
    SVSUTIL_ASSERT(STREAM_STATE_OPENED == m_State);
    m_State = STREAM_STATE_STREAMING; 
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP: Stream_t :: Streaming :: Stream_t: [0x%08x] state STREAM_STATE_STREAMING\n", this));

}

inline void
Stream_t::
Suspended(
    void
    )
{
    SVSUTIL_ASSERT(STREAM_STATE_STREAMING == m_State);
    m_State = STREAM_STATE_OPENED;
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP: Stream_t :: Suspended :: Stream_t: [0x%08x] state STREAM_STATE_OPENED\n", this));

}

inline BOOL
Stream_t::
IsClosed(
    void
    ) const
{
    return (STREAM_STATE_IDLE == m_State ||
            STREAM_STATE_CLOSED == m_State);
}

inline BOOL
Stream_t::
IsConfigured(
    void
    ) const
{
    SVSUTIL_ASSERT(NULL != m_StreamHandle);
    return (STREAM_STATE_CONFIGURED == m_State);
}

inline BOOL
Stream_t::
HasBeenConfigured(
    void
    ) const
{
    return (NULL != m_StreamHandle);
}

inline BOOL
Stream_t::
IsOpened(
    void
    ) const
{
    return (STREAM_STATE_OPENED == m_State);
}

inline BOOL 
Stream_t::
IsStreaming(
    void
    ) const
{
    return (STREAM_STATE_STREAMING == m_State);
}

BOOL 
Stream_t::
IsSuspended(
    void
    ) const
{
    return (STREAM_STATE_OPENED == m_State);
}

        
inline HANDLE
Stream_t::
GetStreamHandle(
    void
    ) const
{
    return m_StreamHandle;
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Class:          StreamEndPoint_t 

Description:    Maintains end point SEID, end point information
                and capabilities
-------------------------------------------------------------------*/
class StreamEndPoint_t {

public:

    static const DWORD  MAX_NUM_CAPABILITIES = 16;
    static BYTE s_LocalSEID;
private:

    
    AVDTP_ENDPOINT_INFO m_AvdptEndPointInfo;
    AVDTP_CAPABILITY_IE m_pCapabilityIE[MAX_NUM_CAPABILITIES];
    DWORD               m_cCapabilityIE;

public:


    StreamEndPoint_t(
        );
    
    void 
    Zero(
        void
        );
    


    BYTE 
    GetSEID(
        void
        ) const;

    void
    GetCapabilitiesIEArrayCopy(
        AVDTP_CAPABILITY_IE pCapabilityIE[], 
        DWORD &cCapabilityIE
        ) const;

    AVDTP_CAPABILITY_IE*
    GetCapabilitiesIE(
        void
        );
        
    DWORD 
    GetCapabilitiesIECount(
        void
        ) const;

    AVDTP_ENDPOINT_INFO
    GetAvdptEndPointInfo(
        void
        ) const;

    DWORD 
    SetToLocalDefaultValues(
        BOOL Capabilities
        );
        
    DWORD 
    SetToLocalDefaultValuesCapabilities(
        void
        );

    DWORD 
    SetToLocalDefaultValuesConfiguration(
        void
        );        
    void 
    SetAvdptEndPointInfo(
        const AVDTP_ENDPOINT_INFO& AvdptEndPointInfo
        );

    void 
    InUse(
        void
        );
        
    void 
    SetCapabilitiesIE(
        const AVDTP_CAPABILITY_IE* const pCapabilityIE, 
        DWORD cCapabilityIE
        );

    void
    ClearCapabilitiesIE(
        void
        );

};

const DWORD  StreamEndPoint_t::MAX_NUM_CAPABILITIES;
BYTE StreamEndPoint_t::s_LocalSEID = 0;
#define TSEP_SOURCE 0
#define TSEP_SINK 1
    
void 
StreamEndPoint_t::
Zero()
{

    memset(&m_AvdptEndPointInfo, 0, sizeof(m_AvdptEndPointInfo));
    memset(m_pCapabilityIE, 0, sizeof(m_pCapabilityIE));
    m_cCapabilityIE = FALSE;
}

StreamEndPoint_t::    
StreamEndPoint_t(
    )
{
    Zero();
}

//#ifdef DEBUG

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       PrintCodecCaps ()

Description:    Print the information from CodecCaps in a (more) 
                human readable format

Arguments:      AVDTP_CAPABILITY_IE* CodecCaps
                    Pointer to an AVDTP_CAPABILITY_IE from which
                    we derive the information to print
-------------------------------------------------------------------*/
void 
PrintCodecCaps(AVDTP_CAPABILITY_IE* CodecCaps)
{

    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : printing raw codec capabilities\r\n"));

    
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: Byte 0 : [0x%x]\r\n", CodecCaps->MediaCodec.CodecIEs[0]));
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: Byte 1 : [0x%x]\r\n", CodecCaps->MediaCodec.CodecIEs[1]));
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: Byte 2 : [0x%x]\r\n", CodecCaps->MediaCodec.CodecIEs[2]));
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: Byte 3 : [0x%x]\r\n", CodecCaps->MediaCodec.CodecIEs[3]));

    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : translated codec capabilities \r\n"));

    BYTE Channels = CodecCaps->MediaCodec.CodecIEs[0]&0x0f;
    if(Channels & 0x01)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports JOINT STEREO\n\r"));
        }
    if(Channels & 0x02)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports STEREO\n\r"));
        }
    if(Channels & 0x04)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports DUAL CHANNEL\n\r"));
        }
    if(Channels & 0x08)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports MONO\n\r"));
        }
    
    BYTE SamplingFrequency = (CodecCaps->MediaCodec.CodecIEs[0])>>4;
    if(SamplingFrequency & 0x08)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Sampling Frequency16kHz\n\r"));
        }
    if(SamplingFrequency & 0x04)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Sampling Frequency 32kHz\n\r"));
        }
    if(SamplingFrequency & 0x02)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Sampling Frequency 44.1kHz\n\r"));
        }
    if(SamplingFrequency & 0x01)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Sampling Frequency 48kHz\n\r"));
        }    

    BYTE AllocationMethod = CodecCaps->MediaCodec.CodecIEs[1] & 0x03;
    if(AllocationMethod & 0x01)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Alocation Method Loudness\n\r"));
        }
    if(AllocationMethod & 0x02)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports Alocation Method SNR\n\r"));
        }

    BYTE Subbands = ((CodecCaps->MediaCodec.CodecIEs[1])>>2)&0x3;
    if(Subbands & 0x01)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # Subbands [%d]\n\r", 8));
        }
    if(Subbands & 0x02)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # Subbands [%d]\n\r", 4));
        }

    BYTE BlockLen = (CodecCaps->MediaCodec.CodecIEs[1])>>4;
    if(BlockLen & 0x01)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # BlockLen [%d]\n\r", 16));
        }
    if(BlockLen & 0x02)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # BlockLen [%d]\n\r", 12));
        }
    if(BlockLen & 0x04)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # BlockLen [%d]\n\r", 8));
        }
    if(BlockLen & 0x08)
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : supports # BlockLen [%d]\n\r", 4));
        }
    
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : max bitpool value [%d]\n\r",CodecCaps->MediaCodec.CodecIEs[2]));
    DEBUGMSG(ZONE_A2DP_VERBOSE, ( L"A2DP :: PrintCodecCaps() : min bitpool value [%d]\n\r",CodecCaps->MediaCodec.CodecIEs[3]));

}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SetDefaultCodecInfo ()

Description:    Resets the codec info in 
                AVDTP_CAPABILITY_IE * CodecCaps to the local 
                defaults (since it can be changed during the 
                configuration process with the remote end point)
                If Capabilities is set, MAX and MIN bitpool are 
                set to diffrent values (the max and min bitpool 
                we support), otherwise, set them both the the 
                default local bitpool

Arguments:      AVDTP_CAPABILITY_IE* CodecCaps
                    Pointer to an AVDTP_CAPABILITY_IE from which
                    we derive the information in a (more) human
                    readable format
                BOOL Capabilities
                    Set BitPool for Capabilites or Configuration
-------------------------------------------------------------------*/
void SetDefaultCodecInfo(AVDTP_CAPABILITY_IE * CodecCaps, BOOL Capabilities)
{

    BYTE Channels = (BYTE)OUT_CHANNELS;
    DWORD SamplingFrequency = (DWORD)SAMPLE_RATE;
    BYTE AllocationMethod = (BYTE)ALLOCATION_METHOD;
    BYTE Subbands = (BYTE)SUBBANDS;
    BYTE BlockLen = (BYTE)BLOCK_SIZE;
    
    SVSUTIL_ASSERT(Channels == 1 || Channels == 2);
    if(Channels == 1)
        {
        CodecCaps->MediaCodec.CodecIEs[0] = 0x08;       
        }
    if(Channels == 2)
        {
        if(SBC_CHANNEL_MODE_JOINT == CHANNEL_MODE)
            {
            CodecCaps->MediaCodec.CodecIEs[0] = 0x01;      
            }
        else
            {
            CodecCaps->MediaCodec.CodecIEs[0] = 0x02;      
            }
        }
    SVSUTIL_ASSERT(SamplingFrequency == 16000 || 
                   SamplingFrequency == 32000 ||
                   SamplingFrequency == 44100 ||
                   SamplingFrequency == 48000
                   );
    if(SamplingFrequency == 16000)
        {
        CodecCaps->MediaCodec.CodecIEs[0] |= 0x80;      
        }
    if(SamplingFrequency == 32000)
        {
        CodecCaps->MediaCodec.CodecIEs[0] |= 0x40;      
        }  
    if(SamplingFrequency == 44100)
        {
        CodecCaps->MediaCodec.CodecIEs[0] |= 0x20;      
        }  
    if(SamplingFrequency == 48000)
        {
        CodecCaps->MediaCodec.CodecIEs[0] |= 0x10;      
        }  

    SVSUTIL_ASSERT((AllocationMethod == SBC_ALLOCATION_LOUDNESS)||
                   (AllocationMethod == SBC_ALLOCATION_SNR)
                   );
                  
    if(AllocationMethod == SBC_ALLOCATION_LOUDNESS)
        {
        CodecCaps->MediaCodec.CodecIEs[1] = 0x01;      
        }
    if(AllocationMethod == SBC_ALLOCATION_SNR)
        {
        CodecCaps->MediaCodec.CodecIEs[1] = 0x02;      
        }
        
    SVSUTIL_ASSERT((Subbands == 4)||
                   (Subbands == 8)
                   );
                  
    if(Subbands == 4)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x08;
        }
    if(Subbands == 8)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x04;
        }

    SVSUTIL_ASSERT((BlockLen == 4) ||
                   (BlockLen == 8) ||
                   (BlockLen == 12) ||
                   (BlockLen == 16)
                   );
    
    if(BlockLen == 4)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x80;
        }
    if(BlockLen == 8)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x40;
        }
    if(BlockLen == 12)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x20;
        }
    if(BlockLen == 16)
        {
        CodecCaps->MediaCodec.CodecIEs[1] |= 0x10;
        }
    if(!Capabilities)//Configuration
        {
        CodecCaps->MediaCodec.CodecIEs[2] = BIT_POOL;
        CodecCaps->MediaCodec.CodecIEs[3] = BIT_POOL;
        }
    else 
        {
        CodecCaps->MediaCodec.CodecIEs[2] = MIN_SUPPORTED_BIT_POOL;
        CodecCaps->MediaCodec.CodecIEs[3] = MAX_SUPPORTED_BIT_POOL;
        }
        
}

DWORD
StreamEndPoint_t::
SetToLocalDefaultValuesCapabilities()
{
    return SetToLocalDefaultValues(TRUE);
}

DWORD
StreamEndPoint_t::
SetToLocalDefaultValuesConfiguration()
{
    return SetToLocalDefaultValues(FALSE);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SetToLocalDefaultValues ()

Description:    Resets the StreamEndPoint to the local 
                defaults (since it can be changed during the 
                configuration process with the remote end point)

Arguments:      BOOL Capabilities
                    Set CodecCaps for Capabilites or Configuration
-------------------------------------------------------------------*/
DWORD
StreamEndPoint_t::SetToLocalDefaultValues(BOOL Capabilities)
{

    if(s_LocalSEID == 0) //do not have a local SEID yet
        {
        return ERROR_RESOURCE_NOT_AVAILABLE;
        }
    memset(&m_AvdptEndPointInfo, 0, sizeof(m_AvdptEndPointInfo));
    m_AvdptEndPointInfo.dwSize = sizeof(m_AvdptEndPointInfo);
    m_AvdptEndPointInfo.bSEID = s_LocalSEID;
    m_AvdptEndPointInfo.fInUse = FALSE; 
    m_AvdptEndPointInfo.fMediaType = 0;  // Audio
    m_AvdptEndPointInfo.bTSEP = TSEP_SOURCE;       // SRC

    memset(m_pCapabilityIE, 0, sizeof(m_pCapabilityIE));
    
    m_pCapabilityIE[0].dwSize = sizeof(m_pCapabilityIE);
    m_pCapabilityIE[0].bServiceCategory = AVDTP_CAT_MEDIA_TRANSPORT;
    m_pCapabilityIE[0].bServiceCategoryLen = 0;

    m_pCapabilityIE[1].dwSize = sizeof(m_pCapabilityIE);
    m_pCapabilityIE[1].bServiceCategory = AVDTP_CAT_MEDIA_CODEC;
    m_pCapabilityIE[1].bServiceCategoryLen = 6;
    m_pCapabilityIE[1].MediaCodec.bMediaType = 0; // audio
    m_pCapabilityIE[1].MediaCodec.bMediaCodecType = 0; // SBC       


    SetDefaultCodecInfo(&(m_pCapabilityIE[1]), Capabilities);
    PrintCodecCaps(&(m_pCapabilityIE[1]));
    m_cCapabilityIE = STREAM_END_POINT_CAPS_COUNT;

    return ERROR_SUCCESS;
}

BYTE
StreamEndPoint_t::
GetSEID(
    void 
    ) const
{
    return m_AvdptEndPointInfo.bSEID;
}

void
StreamEndPoint_t::
GetCapabilitiesIEArrayCopy(
    AVDTP_CAPABILITY_IE pCapabilityIE[], 
    DWORD & cCapabilityIE
    ) const
{
    
    memset(pCapabilityIE, 0, sizeof(AVDTP_CAPABILITY_IE)*cCapabilityIE);
    cCapabilityIE = min(m_cCapabilityIE, cCapabilityIE);
    memcpy(pCapabilityIE, 
           m_pCapabilityIE, 
           cCapabilityIE * sizeof(AVDTP_CAPABILITY_IE));
}

AVDTP_CAPABILITY_IE*
StreamEndPoint_t::
GetCapabilitiesIE(
    void
    )
{
    return m_pCapabilityIE;
}


DWORD
StreamEndPoint_t::
GetCapabilitiesIECount(
    void
    ) const
{
    return m_cCapabilityIE;
}

AVDTP_ENDPOINT_INFO 
StreamEndPoint_t::
GetAvdptEndPointInfo(
    void
    ) const
{
    return m_AvdptEndPointInfo;
}


void
StreamEndPoint_t::
SetAvdptEndPointInfo(
    const AVDTP_ENDPOINT_INFO & AvdptEndPointInfo
    )
{
    memmove(&m_AvdptEndPointInfo, &AvdptEndPointInfo, sizeof(m_AvdptEndPointInfo));
}

void 
StreamEndPoint_t::
InUse(
    void
    )
{
    m_AvdptEndPointInfo.fInUse = TRUE;
}
        
void
StreamEndPoint_t::
SetCapabilitiesIE(
    const AVDTP_CAPABILITY_IE* const pCapabilityIE, 
    DWORD cCapabilityIE
    )
{
    memset(m_pCapabilityIE, 0, sizeof(m_pCapabilityIE));
    m_cCapabilityIE= min(cCapabilityIE, MAX_NUM_CAPABILITIES);
    memcpy(m_pCapabilityIE, 
           pCapabilityIE, 
           m_cCapabilityIE * sizeof(AVDTP_CAPABILITY_IE));

    if(cCapabilityIE == 2)
        {
        PrintCodecCaps(&(m_pCapabilityIE[1]));
        }
}

void
StreamEndPoint_t::
ClearCapabilitiesIE(
    void
    )
{
   memset(m_pCapabilityIE, 0, sizeof(m_pCapabilityIE));
   m_cCapabilityIE = 0;
}




    

//
// BTEndPoint_t definitions
//

class BTEndPoint_t {
public:
    BD_ADDR             BdAddress;                  //BD standard address for end point 
    AVDTP_ENDPOINT_INFO pAcpSEIDInfo[MAX_SEIDS];    //list of all stream endpoints at BT endpoint
    DWORD               cAcpSEIDInfo;               //number of stream endpoint in above list
    StreamEndPoint_t    CurrentSEInfo;              //information for the stream end point currently in use

    void Zero(void)
    {
        memset(&BdAddress, 0, sizeof(BdAddress));
        memset(pAcpSEIDInfo, 0, sizeof(pAcpSEIDInfo));
        cAcpSEIDInfo = 0;
    }
    BTEndPoint_t(void)
    {
        Zero();
    }
    void 
    ClearCurrentSEInfo(
        void
        )
    {
        CurrentSEInfo.Zero();
    }
        
};

class A2DPState_t {
public:

    enum A2DPStateEnum_e {
        A2DP_STATE_CLOSED,
        A2DP_STATE_INITIALIZING,
        A2DP_STATE_DOWN,
        A2DP_STATE_UP,
        A2DP_STATE_CLOSING   
    };

private:
    A2DPStateEnum_e m_State;

public:
    A2DPState_t(void)
    {
        Reset();
    }

    inline void Initializing(void) { m_State = A2DP_STATE_INITIALIZING; }
    inline void Initialized(void)  { m_State = A2DP_STATE_DOWN; }
    inline void StackUp(void)      { m_State = A2DP_STATE_UP; }
    inline void StackDown(void)    { m_State = A2DP_STATE_DOWN; }
    inline void Closing(void)      { m_State = A2DP_STATE_CLOSING; }
    inline void Closed(void)       { m_State = A2DP_STATE_CLOSED; }
    inline void Reset(void)        { m_State = A2DP_STATE_CLOSED; }

    inline BOOL IsRunning(void) const 
    {  
        return ((m_State == A2DP_STATE_UP) || (m_State == A2DP_STATE_DOWN)) ;
    }

    inline BOOL IsStackConnected(void) const
    {
        return (m_State == A2DP_STATE_UP);
    }

    inline 
    BOOL 
    IsStackDown(
        void
        ) const
    {
        return (m_State != A2DP_STATE_UP);
    }

        

};

static const DWORD MAX_SUBKEY_NUM = 4;
static const DWORD MIN_SUBKEY_NUM = 1;
static const size_t PATH = 256;

class A2DP_t : public A2DPState_t, public SVSSynch, public SVSRefObj 
{


friend int a2dp_SetConfiguration(A2DP_USER_CALL * pUserCall);
friend int a2dp_Discover (A2DP_USER_CALL* pUserCall, BD_ADDR DiscoverBdAddr);
friend int a2dp_Open (A2DP_USER_CALL* pUserCall);
friend int a2dp_Start(A2DP_USER_CALL * pUserCall);
friend int a2dp_Suspend(A2DP_USER_CALL * pUserCall);
friend int a2dp_GetCapabilites(A2DP_USER_CALL * pUserCall);
friend int a2dp_Close(A2DP_USER_CALL * pUserCall);
friend int a2dp_Write (A2DP_USER_CALL* pUserCall);
friend int a2dp_Abort(A2DP_USER_CALL * pUserCall);
friend BOOL a2dp_IsReadyToWrite();
friend BOOL a2dp_HasBeenOpened();
friend int a2dp_StreamAbortedEvent(void* pUserContext, HANDLE hStream, int Error);





friend int a2dp_Discover_Out (void* pCallContext, BD_ADDR* pba, PAVDTP_ENDPOINT_INFO pAcpSEIDInfo, DWORD cAcpSEIDInfo, BYTE Error);
friend int a2dp_SetConfiguration_Out (void* pCallContext, HANDLE Stream, BYTE bServiceCategory, BYTE Error);
friend int a2dp_Open_Out (void* pCallContext, HANDLE Stream, BYTE Error);
friend int a2dp_Start_Out (void* pCallContext, HANDLE hFirstFailingStream, BYTE Error);
friend int a2dp_Suspend_Out (void* pCallContext, HANDLE hFirstStream, HANDLE hFirstFailingStream, BYTE Error);

friend int a2dp_SetConfiguration_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream, BD_ADDR* pba, BYTE bAcpSEID, BYTE bIntSEID, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);

friend int a2dp_Start_Ind (void* pUserContext, BYTE Transaction, HANDLE* pStreamHandles, DWORD cStreamHandles);
friend int a2dp_Suspend_Ind (void* pUserContext, BYTE Transaction, HANDLE* pStreamHandles, DWORD cStreamHandles);

friend int a2dp_Reconfigure_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE);
friend int a2dp_Open_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream);
friend int a2dp_Close_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream);







public:
 
    CallList        ListCalls;
    BTEndPoint_t    RemoteBTEndPoint;

    unsigned int    fIsBinded  : 1; // whether SoundDriver has invoked a2dp_Bind

    HANDLE          AVDTP;
    HANDLE			hClosingEvent;
    AVDTP_INTERFACE avdtp_if;

    void            *pUserData;
    A2DP_CALLBACKS  a2dp_callbacks;

    int             cAvdtpHeaders;
    int             cAvdtpTrailers;

    // We want to keep track of the time open_ind happens since we
    // don't want to send a Start as ACP right away since some
    // headphones will expect to send us the Start first (e.g. HP headphones).
    DWORD           dwOpenIndTime;
    BOOL            fHaveOpenIndTime;

    static BYTE m_LocalSEID;

    BthA2dpCodec_t m_Codec; 
    unsigned long m_SdpRecord;
    StreamEndPoint_t m_CurrentLocalStreamEPInfo; 

    BD_ADDR pBdAdresses[MAX_SUBKEY_NUM];
    
private:

    Stream_t m_CurrentStream;

    
    void 
    SetRemoteBTSEIDs(
        const AVDTP_ENDPOINT_INFO* pAcpSEIDInfo,
        DWORD cAcpSEIDInfo
        );
    
public:
    A2DP_t (void);
    ~A2DP_t (void);

    BYTE 
    GetCurrentRemoteSEID(
        void
        ) const;
        
    HANDLE
    GetCurrentStreamHandle(
        void
        ) const;

    BD_ADDR
    GetRemoteBdAddress(
        void
        ) const;

    void
    SetDefaultLocalSEID(
        BYTE Seid
        );
    
    BYTE 
    SetRemoteBTSEIDsAndChooseCurrentSEID(
        BYTE StartSEID,
        const AVDTP_ENDPOINT_INFO*  pAcpSEIDInfo,
        DWORD cAcpSEIDInfo
        );

    void 
    SetRemoteEPCapabilities(
        const AVDTP_CAPABILITY_IE*  const pCapabilityIE,
        DWORD cCapabilityIE
        );

    void 
    SetRemoteBdAddress(
        const BD_ADDR* const  bd
        );

    BOOL 
    ProcessConfiguration(
        PAVDTP_CAPABILITY_IE pCapabilityIE, 
        DWORD cCapabilityIE,
        DWORD & FirstFailedConfiguration,
        BYTE & FirstError
        );

    BOOL
    ProcessCapabilities(
        PAVDTP_CAPABILITY_IE pCapabilityIE, 
        DWORD cCapabilityIE
        );
        
    void
    ResetToPreConfigurationValues(
        void
        );

    DWORD
    AddSdpRecord(
        void
        );

    DWORD
    RemoveSdpRecord(
        void
        );
    
    void
    ReadRegistrySettingsAndSetConfigurationValues(
        void
        );
    
};


//
// A2DP_t private functions
//




void
A2DP_t::
SetRemoteBTSEIDs(
    const AVDTP_ENDPOINT_INFO* pAcpSEIDInfo,
    DWORD cAcpSEIDInfo
    )
{
    RemoteBTEndPoint.cAcpSEIDInfo = min(MAX_SEIDS, cAcpSEIDInfo);
    memcpy(
        RemoteBTEndPoint.pAcpSEIDInfo, 
        pAcpSEIDInfo,
        RemoteBTEndPoint.cAcpSEIDInfo * sizeof(AVDTP_ENDPOINT_INFO)
        );
    return;
}

//
// A2DP_t public functions
//
A2DP_t::A2DP_t (void)
{
    AVDTP = NULL;
    hClosingEvent = NULL;

   // fIsRunning = FALSE;
   // fConnected = FALSE;
    fIsBinded  = FALSE;

    memset(&avdtp_if, 0, sizeof(avdtp_if));
    pUserData = NULL;
    memset (&a2dp_callbacks, 0, sizeof (a2dp_callbacks));

    cAvdtpHeaders = 0;
    cAvdtpTrailers = 0;
    m_SdpRecord = 0;
}

A2DP_t::
~A2DP_t (void) 
{
    if (hClosingEvent) 
        {
        CloseHandle(hClosingEvent);
        hClosingEvent = NULL;
        }
}

BYTE
A2DP_t::
GetCurrentRemoteSEID(
    void 
    ) const
{
    return RemoteBTEndPoint.CurrentSEInfo.GetSEID();
}

HANDLE
A2DP_t::
GetCurrentStreamHandle(
    void
    ) const
{
    return m_CurrentStream.GetStreamHandle();
}

BD_ADDR
A2DP_t::
GetRemoteBdAddress(
    void
    ) const
{
    return RemoteBTEndPoint.BdAddress;
}


void
A2DP_t::
SetDefaultLocalSEID(
    BYTE Seid
    )
{
    m_CurrentLocalStreamEPInfo.s_LocalSEID = Seid;
}


// this is for sink choice only
// will need to alter when a2dp is used for both
BYTE
A2DP_t::
SetRemoteBTSEIDsAndChooseCurrentSEID(
    BYTE StartSEID,
    const AVDTP_ENDPOINT_INFO*  pAcpSEIDInfo,
    DWORD cAcpSEIDInfo
    )
{
    const AVDTP_ENDPOINT_INFO* pAcpSEIDInfoLocal;
    DWORD cAcpSEIDInfoLocal;
    RemoteBTEndPoint.CurrentSEInfo.Zero();

    if(NULL == pAcpSEIDInfo)
        {
        //already copied the SEIDs, so use the ones stored by a2dp
        pAcpSEIDInfoLocal = RemoteBTEndPoint.pAcpSEIDInfo;
        cAcpSEIDInfoLocal = RemoteBTEndPoint.cAcpSEIDInfo;
        }
    else
        {
        pAcpSEIDInfoLocal = pAcpSEIDInfo;
        cAcpSEIDInfoLocal = cAcpSEIDInfo;
        }

    unsigned i=0;
    if(0 != StartSEID)
        {
        for(; i<cAcpSEIDInfoLocal; i++)
            {
            if(pAcpSEIDInfoLocal[i].bSEID == StartSEID)
                {
                i++;
                break;
                }
            }
        }

    for(; i<cAcpSEIDInfoLocal; i++)
        {
        if((!pAcpSEIDInfoLocal[i].fInUse) &&         //not in use //commenting out as work around naviplay
           (pAcpSEIDInfoLocal[i].fMediaType == 0) && //audio media type
           (pAcpSEIDInfoLocal[i].bTSEP == TSEP_SINK) && //remote end is expected to be sink
           (pAcpSEIDInfoLocal[i].bSEID != 0)
          )
            {
            break;
            }
        }

    if(i < cAcpSEIDInfoLocal)
        {
        if(NULL != pAcpSEIDInfo)
            {
            SetRemoteBTSEIDs(pAcpSEIDInfoLocal, cAcpSEIDInfoLocal);
            }
        RemoteBTEndPoint.CurrentSEInfo.SetAvdptEndPointInfo(pAcpSEIDInfoLocal[i]);
        }


    return RemoteBTEndPoint.CurrentSEInfo.GetSEID();
}

void 
A2DP_t::
SetRemoteBdAddress(
    const BD_ADDR* const bd
    )
{
    if(NULL == bd)
        {
        return;
        }
    memcpy(&RemoteBTEndPoint.BdAddress, bd, sizeof(RemoteBTEndPoint.BdAddress));
}

void 
A2DP_t::
SetRemoteEPCapabilities(
    const AVDTP_CAPABILITY_IE*  const pCapabilityIE,
    DWORD cCapabilityIE
    )
{
    if(NULL == pCapabilityIE)
        {
        return;
        }
    RemoteBTEndPoint.CurrentSEInfo.SetCapabilitiesIE(pCapabilityIE, cCapabilityIE);
    return;

}

//Returns all of A2DP_t state vars to 
//what they where before the call to
//initial values, or to post call
//to discover
//maintain all initialized values
void
A2DP_t::
ResetToPreConfigurationValues(
    void
    )
{

    //maintain information from discover
    //but clear the array currently assumed to be in use
    g_pHWContext->MakeNonPreferredDriver();
    RemoteBTEndPoint.ClearCurrentSEInfo();
    m_CurrentStream.Reset();
    //Go back to local defaults if had been changed by the configuration procedure
    m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();
    if(m_Codec.IsOpened())
        {
        m_Codec.CloseStream();
        }
    fHaveOpenIndTime = FALSE;
    dwOpenIndTime = 0;
}





BOOL OneBitSet(BYTE a)
{
    DWORD Res = ((a & 0x80)>>7) + 
                ((a & 0x40)>>6) +
                ((a & 0x20)>>5) +
                ((a & 0x10)>>4) +
                ((a & 0x08)>>3) +
                ((a & 0x04)>>2) +
                ((a & 0x02)>>1) +
                (a & 0x01);
    if(1 == Res)
        return TRUE;
    else
        return FALSE;
}

        
//returns true on sucess, false on failure
//on failure FirstFailedConfiguration has the 
//first bas pCapabilityIE
BOOL
A2DP_t::
ProcessConfiguration(
    PAVDTP_CAPABILITY_IE pCapabilityIE, 
    DWORD cCapabilityIE,
    DWORD & FirstFailedConfiguration,
    BYTE & FirstError
    )
{

    BOOL ValidCodecConfiguration = FALSE;
    BOOL ValidTransportConfiguration = FALSE;
    FirstError = 0;
    for(DWORD i = 0; i<cCapabilityIE; i++)
        {
        if(AVDTP_CAT_MEDIA_TRANSPORT == pCapabilityIE[i].bServiceCategory)
            {
            if(0 == pCapabilityIE[i].bServiceCategoryLen)
                {
                ValidTransportConfiguration = TRUE;
                }
            else
                {
                FirstError = INVALID_TRANSPORT_TYPE;
                FirstFailedConfiguration = i;
                break;
                }
            }
        else if(AVDTP_CAT_MEDIA_CODEC == pCapabilityIE[i].bServiceCategory)
            {
            if(6 != pCapabilityIE[i].bServiceCategoryLen)
                {
                FirstError = INVALID_CODEC_SERVICE_CATEGORY_LEN;
                FirstFailedConfiguration = i;
                break;
                }
            if(0 != pCapabilityIE[i].MediaCodec.bMediaType || //audio
               0 != pCapabilityIE[i].MediaCodec.bMediaCodecType) // sbc
            {
                FirstError = NOT_SUPPORTED_CODEC_TYPE;
                FirstFailedConfiguration = i;
                break;
            }
                
            BYTE Channels = pCapabilityIE[i].MediaCodec.CodecIEs[0]&0x0f;
            BYTE SamplingFrequency = pCapabilityIE[i].MediaCodec.CodecIEs[0]&0xf0;
            BYTE AllocationMethod = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0x03;
            BYTE Subbands = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0x0c;
            BYTE BlockLen = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0xf0;
            //first check for valid values of these parms
            if(!OneBitSet(Channels))
                {
                FirstError = INVALID_CHANNEL_MODE;
                }
            if(!OneBitSet(SamplingFrequency))
                {
                FirstError = INVALID_SAMPLING_FREQUENCY;
                }
            if(!OneBitSet(AllocationMethod))
                {
                FirstError = INVALID_ALLOCATION_METHOD;
                }
            if(!OneBitSet(Subbands))
                {
                FirstError = INVALID_SUBBANDS;
                }
            if(!OneBitSet(BlockLen))
                {
                FirstError = INVALID_BLOCK_LENGTH;
                }
            if(MIN_BIT_POOL > pCapabilityIE[i].MediaCodec.CodecIEs[2])
                {
                FirstError = INVALID_MINIMUM_BITPOOL_VALUE;
                }
            if(MAX_BIT_POOL < pCapabilityIE[i].MediaCodec.CodecIEs[3])
                {
                FirstError = INVALID_MAXIMUM_BITPOOL_VALUE;
                }
            if(0 == FirstError)
                {
                switch(OUT_CHANNELS)
                    {
                    case 2:
                        if(CHANNEL_MODE == SBC_CHANNEL_MODE_JOINT)
                            {
                            if(0x01 != Channels)
                                {
                                FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                                }
                            }
                        else
                            {
                            if(0x02 != Channels)
                                {
                                FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                                }
                            }
                        break;
                    case 1:
                        if(0x08 != Channels)
                            {
                            FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                            }
                        break;
                    }

                switch(SAMPLE_RATE)
                    {
                    case 16000:
                        if(0x80 != SamplingFrequency)
                            {                            
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 32000:
                        if(0x40 != SamplingFrequency)
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 44100:
                        if(0x20 != SamplingFrequency)
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 48000:
                        if(0x10 != SamplingFrequency)
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    }

                switch(ALLOCATION_METHOD)
                    {
                    case SBC_ALLOCATION_LOUDNESS:
                        if(0x01 != AllocationMethod)
                            {
                            FirstError = NOT_SUPPORTED_ALLOCATION_METHOD;
                            }
                        break;
                    case SBC_ALLOCATION_SNR:
                        if(0x02 != AllocationMethod)
                            {
                            FirstError = NOT_SUPPORTED_ALLOCATION_METHOD;
                            }
                        break;
                    }
                switch(SUBBANDS)
                    {
                    case 4:
                        if(0x08 != Subbands)
                            {
                            FirstError = NOT_SUPPORTED_SUBBANDS;
                            }
                        break;
                    case 8:
                        if(0x04 != Subbands)
                            {
                            FirstError = NOT_SUPPORTED_SUBBANDS;
                            }
                        break;
                    }
                switch(BLOCK_SIZE)
                    {
                    case 4:
                        if(0x80 != BlockLen)
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    case 8:
                        if(0x40 != BlockLen)
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;                
                    case 12:
                        if(0x20 != BlockLen)
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    case 16:
                        if(0x10 != BlockLen)
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    }

                if(pCapabilityIE[i].MediaCodec.CodecIEs[2] > MAX_SUPPORTED_BIT_POOL)
                    {
                    FirstError = NOT_SUPPORTED_MAXIMUM_BITPOOL_VALUE;
                    }
                else
                    {
                    if(pCapabilityIE[i].MediaCodec.CodecIEs[3] < MIN_SUPPORTED_BIT_POOL)
                        {
                        FirstError = NOT_SUPPORTED_MINIMUM_BITPOOL_VALUE;
                        }
                    else
                        {
                        if(BIT_POOL > pCapabilityIE[i].MediaCodec.CodecIEs[3])
                            {
                            BIT_POOL= pCapabilityIE[i].MediaCodec.CodecIEs[3];
                            AdaptAudioDMAPageSize();
                            m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();
                            }
                        else
                            {
                            if(BIT_POOL < pCapabilityIE[i].MediaCodec.CodecIEs[2])
                                {
                                BIT_POOL= pCapabilityIE[i].MediaCodec.CodecIEs[2];
                                AdaptAudioDMAPageSize();
                                m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();
                                }

                            }

                        }
                    }
                    
                }
            if(0 != FirstError)
                {
                FirstFailedConfiguration = i;
                break;
                }
            if(0 == FirstError)
                {
                ValidCodecConfiguration = TRUE;
                }

            }
        else
            {
            FirstError = INVALID_CODEC_SERVICE_CATEGORY_LEN;
            FirstFailedConfiguration = i;
            break;
            }
            

        }
    if((i == cCapabilityIE) &&
       (ValidTransportConfiguration == TRUE) &&
       (ValidCodecConfiguration == TRUE)
      )
        {
        return TRUE;
        }
    return FALSE;
}

//returns true on sucess, false on failure
//on failure FirstFailedConfiguration has the 
//first bas pCapabilityIE
BOOL
A2DP_t::
ProcessCapabilities(
    PAVDTP_CAPABILITY_IE pCapabilityIE, 
    DWORD cCapabilityIE
    )
{

    BOOL ValidCodecConfiguration = FALSE;
    BOOL ValidTransportConfiguration = FALSE;
    BYTE FirstError = 0;
    for(DWORD i = 0; i<cCapabilityIE; i++)
        {
        if(AVDTP_CAT_MEDIA_TRANSPORT == pCapabilityIE[i].bServiceCategory)
            {
            if(0 == pCapabilityIE[i].bServiceCategoryLen)
                {
                ValidTransportConfiguration = TRUE;
                }
            }
        else if(AVDTP_CAT_MEDIA_CODEC == pCapabilityIE[i].bServiceCategory)
            {
            FirstError = 0;
            if(6 != pCapabilityIE[i].bServiceCategoryLen)
                {
                continue;
                }
            if(0 != pCapabilityIE[i].MediaCodec.bMediaType || //audio
               0 != pCapabilityIE[i].MediaCodec.bMediaCodecType) // sbc
            {
                continue;
            }
                
            BYTE Channels = pCapabilityIE[i].MediaCodec.CodecIEs[0]&0x0f;
            BYTE SamplingFrequency = pCapabilityIE[i].MediaCodec.CodecIEs[0]&0xf0;
            BYTE AllocationMethod = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0x03;
            BYTE Subbands = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0x0c;
            BYTE BlockLen = pCapabilityIE[i].MediaCodec.CodecIEs[1]&0xf0;
            //first check for valid values of these parms

            if(pCapabilityIE[i].MediaCodec.CodecIEs[3] < pCapabilityIE[i].MediaCodec.CodecIEs[2])
                {
                    FirstError = INVALID_MAXIMUM_BITPOOL_VALUE;
                }
            else
                {
            
                if(BIT_POOL < pCapabilityIE[i].MediaCodec.CodecIEs[2])
                    {
                        
                    if(pCapabilityIE[i].MediaCodec.CodecIEs[2] <= MAX_BIT_POOL)
                        {
                        BIT_POOL = pCapabilityIE[i].MediaCodec.CodecIEs[2];
                        AdaptAudioDMAPageSize();
                        m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();
                        }
                    else
                        {
                        FirstError = NOT_SUPPORTED_MINIMUM_BITPOOL_VALUE;
                        }
                    }
                if(BIT_POOL > pCapabilityIE[i].MediaCodec.CodecIEs[3])
                    {
                    if(pCapabilityIE[i].MediaCodec.CodecIEs[3] >= MIN_BIT_POOL)
                        {
                        BIT_POOL = pCapabilityIE[i].MediaCodec.CodecIEs[3];
                        AdaptAudioDMAPageSize();
                        m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();

                        }
                    else
                        {
                        FirstError = NOT_SUPPORTED_MAXIMUM_BITPOOL_VALUE;
                        }
                    }
                }
            if(0 == FirstError)
                {
                switch(OUT_CHANNELS)
                    {                 
                    case 2:
                        if(SBC_CHANNEL_MODE_JOINT == CHANNEL_MODE)
                            {
                            if(!(0x01 & Channels))
                                {
                                FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                                }
                            }
                        else
                            {
                            if(!(0x02 & Channels))

                                {
                                FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                                }
                            }
                        break;
                    case 1:
                        if(!(0x08 & Channels))
                            {
                            FirstError = NOT_SUPPORTED_CHANNEL_MODE;
                            }
                        break;
                    }

                switch(SAMPLE_RATE)
                    {
                    case 16000:
                        if(!(0x80 & SamplingFrequency))
                            {                            
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 32000:
                        if(!(0x40 & SamplingFrequency))
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 44100:
                        if(!(0x20 & SamplingFrequency))
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    case 48000:
                        if(!(0x10 & SamplingFrequency))
                            {
                            FirstError = NOT_SUPPORTED_SAMPLING_FREQUENCY;
                            }
                        break;
                    }

                switch(ALLOCATION_METHOD)
                    {
                    case SBC_ALLOCATION_LOUDNESS:
                        if(!(0x01 & AllocationMethod))
                            {
                            FirstError = NOT_SUPPORTED_ALLOCATION_METHOD;
                            }
                        break;
                    case SBC_ALLOCATION_SNR:
                        if(!(0x02 & AllocationMethod))
                            {
                            FirstError = NOT_SUPPORTED_ALLOCATION_METHOD;
                            }
                        break;
                    }
                switch(SUBBANDS)
                    {
                    case 4:
                        if(!(0x08 & Subbands))
                            {
                            FirstError = NOT_SUPPORTED_SUBBANDS;
                            }
                        break;
                    case 8:
                        if(!(0x04 & Subbands))
                            {
                            FirstError = NOT_SUPPORTED_SUBBANDS;
                            }
                        break;
                    }
                switch(BLOCK_SIZE)
                    {
                    case 4:
                        if(!(0x80 & BlockLen))
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    case 8:
                        if(!(0x40 & BlockLen))
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;                
                    case 12:
                        if(!(0x20 & BlockLen))
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    case 16:
                        if(!(0x10 & BlockLen))
                            {
                            FirstError = INVALID_BLOCK_LENGTH;
                            }
                        break;
                    }   
                }

            if(0 == FirstError)
                {
                ValidCodecConfiguration = TRUE;
                }
            else
                {
                    WCHAR pSubKey[500];
                    StringCchPrintfW(pSubKey, PATH, TEXT("%x"), FirstError);
                 }

            }

            

        }
    if((i == cCapabilityIE) &&
       (ValidTransportConfiguration == TRUE) &&
       (ValidCodecConfiguration == TRUE)
      )
        {
        return TRUE;
        }
    return FALSE;
}

DWORD
A2DP_t::
AddSdpRecord(
    void
    )
{
 

    DWORD Ret  = ERROR_SUCCESS;
    DWORD SizeOut = 0;

    struct {
        BTHNS_SETBLOB   b;
        unsigned char   uca[cSdpRecord];
    } bigBlob;
    
    ULONG SdpVersion = BTH_SDP_VERSION;
    m_SdpRecord = 0;

    bigBlob.b.pRecordHandle   = &(m_SdpRecord);
    bigBlob.b.pSdpVersion     = &SdpVersion;
    bigBlob.b.fSecurity       = 0;
    bigBlob.b.fOptions        = 0;
    bigBlob.b.ulRecordLength  = cSdpRecord;

    memcpy(bigBlob.b.pRecord, rgbSdpRecord, cSdpRecord);

    BLOB blob;
    blob.cbSize    = sizeof(BTHNS_SETBLOB) + cSdpRecord - 1;
    blob.pBlobData = (PBYTE) &bigBlob;

    WSAQUERYSET Service;
    memset(&Service, 0, sizeof(Service));
    Service.dwSize = sizeof(Service);
    Service.lpBlob = &blob;
    Service.dwNameSpace = NS_BTH;

    Ret = BthNsSetService(&Service, RNRSERVICE_REGISTER, 0);
    if (ERROR_SUCCESS != Ret) 
        {
        DEBUGMSG(ZONE_A2DP_ERROR, (L"A2DP_t:: AddSdpRecord:Error adding SDP record.\n"));
        }
    else
        {
        DEBUGMSG(ZONE_A2DP_VERBOSE, (L"A2DP_t:: AddSdpRecord: Successfully added SDP record.\n"));
        }
    return Ret;
}

// This method removes an SDP record
DWORD
A2DP_t::
RemoveSdpRecord(
    void
    )
{
    DWORD Ret      = ERROR_SUCCESS;
    ULONG SdpVersion  = BTH_SDP_VERSION;
    BTHNS_SETBLOB delBlob;
    memset(&delBlob, 0, sizeof(delBlob));
    delBlob.pRecordHandle   = &m_SdpRecord;
    delBlob.pSdpVersion     = &SdpVersion;

    BLOB blob;
    blob.cbSize    = sizeof(BTHNS_SETBLOB);
    blob.pBlobData = (PBYTE) &delBlob;

    WSAQUERYSET Service;
    memset(&Service, 0, sizeof(Service));
    Service.dwSize = sizeof(Service);
    Service.lpBlob = &blob;
    Service.dwNameSpace = NS_BTH;

    Ret = BthNsSetService(&Service, RNRSERVICE_DELETE, 0);

    m_SdpRecord = 0;

    return Ret;
}



// registry settings
static const TCHAR pRegistryAddress[] = TEXT("SOFTWARE\\Microsoft\\Bluetooth\\A2DP\\Settings");

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ReadRegistrySettings()

Description:    Reads all the configurable registry settings
                In case they do not exist, simply use default
                values

Arguments:      void
Returns:        void
-------------------------------------------------------------------*/
void
A2DP_t::
ReadRegistrySettingsAndSetConfigurationValues(
    void
    )
{
    //build sub key name
    WCHAR pSubKey[PATH];
    StringCchPrintfW(pSubKey, PATH, TEXT("%s"), pRegistryAddress);


    HKEY    Hk;

    DWORD Size;
    DWORD Data;
    DWORD Type;
    // defaults
    BIT_POOL = BIT_POOL_DEFAULT;
    SAMPLE_RATE = SAMPLE_RATE_DEFAULT;

    //open key
    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, pSubKey, 0, KEY_READ, &Hk) == ERROR_SUCCESS) 
        {


        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"BitPool", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
            if((MIN_BIT_POOL <= Data)&&
               (MAX_BIT_POOL >= Data))
                {
                BIT_POOL = Data;
                }
            else
                {
                //in debug builds, make sure not doing something wrong in the registry
                assert(0);
                }
                
            }

        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"MaxSupportedBitPool", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
            if((MIN_BIT_POOL <= Data)&&
               (MAX_BIT_POOL >= Data))
                {
                MAX_SUPPORTED_BIT_POOL = Data;
                }
            else
                {
                //in debug builds, make sure not doing something wrong in the registry
                assert(0);
                }
                
            }


        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"MinSupportedBitPool", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
            if((MIN_BIT_POOL <= Data)&&
               (MAX_BIT_POOL >= Data))
                {
                MIN_SUPPORTED_BIT_POOL = Data;
                }
            else
                {
                //in debug builds, make sure not doing something wrong in the registry
                assert(0);
                }
                
            }


        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"SampleRate", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
                
            if((Data == 16000) || 
               (Data == 32000) ||
               (Data == 44100) ||
               (Data == 48000)
              )
              {
              SAMPLE_RATE = Data;
              INVSAMPLERATE = ((UINT32)(((1i64<<32)/SAMPLE_RATE)+1));
              }
        }


        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"EnableAutoConnect", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
                
            if(Data == 1)
                {
                g_pHWContext->Lock();
                g_pHWContext->EnableAutoConnect();
                g_pHWContext->Unlock();
                }
            else
                {
                g_pHWContext->Lock();
                g_pHWContext->DisableAutoConnect();
                g_pHWContext->Unlock();
                }
        }
        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"SuspendDelay", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
                
            if(Data < 600000) //10 min
                {
                SUSPEND_DELAY_MS = Data;
                }
            }

        Size = sizeof(Data);
        if ((RegQueryValueEx (Hk, L"UseJointStereo", NULL, &Type, (LPBYTE)&Data, &Size) == ERROR_SUCCESS) &&
            (Type == REG_DWORD) && (Size == sizeof(Data))) 
            {
            if(Data == 1)
                {
                CHANNEL_MODE = SBC_CHANNEL_MODE_JOINT;
                }
            else
                {
                CHANNEL_MODE = SBC_CHANNEL_MODE_STEREO;
                }
        }

    RegCloseKey (Hk);
    }
    //avoid bad values
    if(MIN_SUPPORTED_BIT_POOL > MAX_SUPPORTED_BIT_POOL)
        {
        MIN_SUPPORTED_BIT_POOL = MIN_SUPPORTED_BIT_POOL_DEFAULT;
        MAX_SUPPORTED_BIT_POOL = MAX_SUPPORTED_BIT_POOL_DEFAULT;
        }
    if(MIN_SUPPORTED_BIT_POOL > BIT_POOL)
        {
        BIT_POOL = MIN_SUPPORTED_BIT_POOL;
        }
    if(MAX_SUPPORTED_BIT_POOL < BIT_POOL)
        {
        BIT_POOL = MAX_SUPPORTED_BIT_POOL;
        }

    // Always make sure we adapt DMA page size based on bit pool
    AdaptAudioDMAPageSize();
    m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();

    return;
}


static int a2dp_AbortCall            (Call_t *pCall, BOOL bHciAbort);

// Call functions
static Call_t*    CallCreateNew       (unsigned char fCallType, A2DP_USER_CALL *pUserCall);
static void     CallDelete          (Call_t* pCall);
static Call_t*    CallVerify          (Call_t* pCall, unsigned char fCallType);
static void     CallRemove          (Call_t* pCall);
static void     CallSignal          (Call_t *pCall, int Result, BYTE Error=0);
static Call_t*    CallFindByUserCall  (A2DP_USER_CALL *pUserCall);

//
// interface
//
static int  a2dp_Call_In        (A2DP_USER_CALL* pUserCall);




//
// global variables
//
static A2DP_t*                         f_pLayerState                    = NULL;
static HMODULE                         f_ModBtd                        = NULL;
static AVDTP_EstablisDeviceContext_t  f_pAVDTP_EstablishDeviceContext = NULL;
static AVDTP_CloseDeviceContext_t      f_pAVDTP_CloseDeviceContext     = NULL;








//////////////////////////////////////////////////////////////////////
//Helper functions
/////////////////////////////////////////////////////////////////////
static 
BOOL 
GetAvdtpConnectionState (
    void
    ) 
{
    __try {
        DWORD Connected = 0;
        int pDataCount = 0;
        
        f_pLayerState->AddRef ();
        f_pLayerState->Unlock();
        f_pLayerState->avdtp_if.avdtp_ioctl (
            f_pLayerState->AVDTP, 
            BTH_STACK_IOCTL_GET_CONNECTED, 
            0, 
            NULL, 
            sizeof(Connected), 
            (char *)&Connected, 
            &pDataCount
            );
        f_pLayerState->Lock();
        f_pLayerState->DelRef ();
        if ((pDataCount == sizeof(Connected))&&
            (Connected != 0)
           )
            {
            return TRUE;
            }
        else
            {
            return FALSE;
            }
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP:: GetConnectionState : exception in avdtp_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
        return FALSE;
    }
}


static BOOL LockIfRunning(void) 
{
    FUNCTION_TRACE(LockIfConnected);
    if (! f_pLayerState) {
        return FALSE;
    }

    f_pLayerState->Lock();    

    if (! f_pLayerState->IsRunning()) {
        f_pLayerState->Unlock();
        return FALSE;
    }

    return TRUE;
}
static BOOL LockIfConnected(void) 
{
    FUNCTION_TRACE(LockIfConnected);
    if (! f_pLayerState) 
        {
        return FALSE;
        }

    f_pLayerState->Lock();    

    if (! f_pLayerState->IsStackConnected()) 
        {
        BOOL Connected = GetAvdtpConnectionState();
        if(Connected)
            {
            f_pLayerState->StackUp();
            }
        else
            {
            f_pLayerState->Unlock();
            return FALSE;
            }
        }

    return TRUE;
}

static int LockIfConnectedAndCallContextIsFound(void *pCallContext, unsigned char CallType) 
{
    FUNCTION_TRACE(LockIfConnectedAndCallContextIsFound);

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }


    Call_t *pCall = CallVerify ((Call_t*)pCallContext, CallType);
    if (!pCall) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: LockIfConnectedAndCallContextIsFound for call [0x%08x] :: no context\n",pCallContext));
        f_pLayerState->Unlock ();
        return ERROR_NOT_FOUND;
        }
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    The DWORD WINAPI Stack* functions are used to process 
    stack events in a separate thread from the main driver thread
    WaitForRefToOneAndLock is a helper function
-------------------------------------------------------------------*/

BOOL 
WaitForRefToOneAndLock(
    void
    )
{
    //
    // Wait for ref count to drop to 1
    //
    
    while (1) {
        if (! LockIfRunning()) {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: WaitForRefToOne : ERROR_SERVICE_DOES_NOT_EXIST\n"));
            return FALSE;
        }
        
        if (f_pLayerState->GetRefCount() == 1) {
            break;
        }

        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: WaitForRefToOne: Waiting for ref count to drop to 1\n"));
        
        f_pLayerState->Unlock ();
        Sleep(100);
    }

    return TRUE;
}




//must be locked
void AdaptAudioDMAPageSize()
{
    ASSERT(g_pHWContext);

    DWORD Ret = f_pLayerState->m_Codec.OpenStream(NULL);

    if(ERROR_SUCCESS == Ret)
        {
        DWORD AudioDmaPageSize = AUDIO_DMA_PAGE_SIZE_DEFAULT;
        Ret = f_pLayerState->m_Codec.GetPCMBufferSize(AudioDmaPageSize, AUDIO_DMA_PAGE_SIZE_DEFAULT);
        SVSUTIL_ASSERT(Ret == ERROR_SUCCESS);
        if(g_pHWContext)
            {
            g_pHWContext->SetAudioDmaPageSize(AudioDmaPageSize);
            }
        f_pLayerState->m_Codec.CloseStream();
        }
}   
    



static int StatusToError (unsigned char status, int iGeneric) 
{
    switch (status) 
    {
        case BT_ERROR_SUCCESS:
            return ERROR_SUCCESS;

        case BT_ERROR_COMMAND_DISALLOWED:
        case BT_ERROR_UNKNOWN_HCI_COMMAND:
        case BT_ERROR_UNSUPPORTED_FEATURE_OR_PARAMETER:
        case BT_ERROR_INVALID_HCI_PARAMETER:
        case BT_ERROR_UNSUPPORTED_REMOTE_FEATURE:
            return ERROR_CALL_NOT_IMPLEMENTED;

        case BT_ERROR_NO_CONNECTION:
            return ERROR_DEVICE_NOT_CONNECTED;

        case BT_ERROR_HARDWARE_FAILURE:
        case BT_ERROR_UNSPECIFIED_ERROR:
            return ERROR_ADAP_HDW_ERR;

        case BT_ERROR_PAGE_TIMEOUT:
        case BT_ERROR_CONNECTION_TIMEOUT:
        case BT_ERROR_HOST_TIMEOUT:
            return ERROR_TIMEOUT;

        case BT_ERROR_AUTHENTICATION_FAILURE:
            return ERROR_NOT_AUTHENTICATED;

        case BT_ERROR_KEY_MISSING:
            return ERROR_NO_USER_SESSION_KEY;

        case BT_ERROR_MEMORY_FULL:
            return ERROR_OUTOFMEMORY;

        case BT_ERROR_MAX_NUMBER_OF_CONNECTIONS:
        case BT_ERROR_MAX_NUMBER_OF_ACL_CONNECTIONS:
            return ERROR_NO_SYSTEM_RESOURCES;

        case BT_ERROR_HOST_REJECTED_LIMITED_RESOURCES:
        case BT_ERROR_HOST_REJECTED_SECURITY_REASONS:
        case BT_ERROR_HOST_REJECTED_PERSONAL_DEVICE:
        case BT_ERROR_PAIRING_NOT_ALLOWED:
            return ERROR_CONNECTION_REFUSED;

        case BT_ERROR_OETC_USER_ENDED:
        case BT_ERROR_OETC_LOW_RESOURCES:
        case BT_ERROR_OETC_POWERING_OFF:
            return ERROR_GRACEFUL_DISCONNECT;

        case BT_ERROR_CONNECTION_TERMINATED_BY_LOCAL_HOST:
            return ERROR_CONNECTION_ABORTED;

        case BT_ERROR_REPEATED_ATTEMPTS:
            return ERROR_CONNECTION_COUNT_LIMIT;
    }

    return iGeneric;
}




///////////////////////////////////////////////////
// Call functions
///////////////////////////////////////////////////

static Call_t* CallCreateNew(unsigned char fCallType, A2DP_USER_CALL *pUserCall)
{
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());
    Call_t *pCall = new Call_t;
    if (!pCall) 
        {
        return NULL;
        }

    pCall->CallType  = fCallType;
    pCall->pUserCall  = pUserCall;

    if (! f_pLayerState->ListCalls.push_front(pCall)) 
        {
        CallDelete(pCall);
        return NULL;
        }
    return pCall;
}

static void CallDelete(Call_t* pCall)
{

    SVSUTIL_ASSERT(f_pLayerState->IsLocked());
    for (CallList::iterator it = f_pLayerState->ListCalls.begin(), itEnd = f_pLayerState->ListCalls.end(); it != itEnd; ++it) {
        if (*it == pCall) {
            f_pLayerState->ListCalls.erase(it);
            break;
        }
    }

    delete pCall;
}

//
// return NULL     if pCall not found in list.
// return non-NULL if pCall     found in list
//
// if fCallType is zero, then it is not checked.
//
static Call_t* CallVerify(Call_t* pCall, unsigned char CallType)
{
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());

    Call_t* pCallReturn = NULL;
    for (CallList::iterator it = f_pLayerState->ListCalls.begin(), itEnd = f_pLayerState->ListCalls.end(); 
         it != itEnd; 
         ++it) 
        {
        if (*it == pCall) 
            { 
            pCallReturn = *it;
            break;
            }
        }
    if(pCallReturn && CallType)
        {
        if(pCallReturn->CallType != CallType)
            {
            pCallReturn = NULL;
            }
        }
        
    return pCallReturn;
}
/*
static void CallInsert (Call_t* pCall)
{
    if (!f_pLayerState || !pCall) {
        return;
    }

    SVSUTIL_ASSERT(f_pLayerState->IsLocked());

    // insert into head
    pCall->pPrev = NULL;
    if (f_pLayerState->pCalls) {
        pCall->pNext = f_pLayerState->pCalls;
        f_pLayerState->pCalls->pPrev = pCall;
    } else {
        pCall->pNext = NULL;
    }
    f_pLayerState->pCalls = pCall;
}
*/
static void CallRemove (Call_t* pCall)
{

    SVSUTIL_ASSERT(f_pLayerState->IsLocked());

    for (CallList::iterator it = f_pLayerState->ListCalls.begin(), itEnd = f_pLayerState->ListCalls.end(); it != itEnd; ++it) {
        if (*it == pCall) {
            f_pLayerState->ListCalls.erase(it);
            break;
        }
    }
    return;
}

static void CallListClear (int Result = 0)
{
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());
    Call_t *pCall;

    for (CallList::iterator it = f_pLayerState->ListCalls.begin(), itEnd = f_pLayerState->ListCalls.end(); it != itEnd;) 
        {
        pCall = *it;
        it++;
        CallSignal(pCall, Result);
        }
        
    return;
}
static void CallSignal (Call_t *pCall, int Result, BYTE Error)

{
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());
    if (!pCall) {
        return;
    }

    pCall->Result = Result;
    pCall->Error = Error;

    A2DP_USER_CALL *pUserCall = pCall->pUserCall;
    if (pUserCall) {
        pUserCall->iResult = Result;
        if (pUserCall->hEvent) {
            SetEvent(pUserCall->hEvent);
        }
    }
    CallDelete(pCall);
    
}

static Call_t* CallFindByUserCall(A2DP_USER_CALL *pUserCall)
{
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());

    Call_t* pCallReturn = NULL;
    for (CallList::iterator it = f_pLayerState->ListCalls.begin(), itEnd = f_pLayerState->ListCalls.end(); it != itEnd; ++it) {
        if ((*it)->pUserCall == pUserCall) { 
            pCallReturn = *it;
            break;
        }
    }

    return pCallReturn;
}





/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_InitializeOnce ()

Description:    Loads necessary dlls and initializes the a2dp
                main state variable

Returns:        int indicating status
-------------------------------------------------------------------*/
int 
a2dp_InitializeOnce(
    void
    )
{

    // Initialize helper classes
    DebugInit(); 
    svsutil_Initialize();
    
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_InitializeOnce() entered\n"));

    
    // Fail if file scope pointer has already been initialized
    if (f_pLayerState) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_InitializeOnce() return ERROR_ALREADY_EXISTS\n"));
        return ERROR_ALREADY_EXISTS;
        }
    
    // Load bluetooth stack library
    SVSUTIL_ASSERT(!f_ModBtd);
    f_ModBtd = LoadLibrary(L"btd.dll");
    if (!f_ModBtd) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_InitializeOnce() : LoadLibrary(btd.dll) failed\n"));
        return GetLastError();
        }
        
    // Call_t avdtp layer API to get context
    f_pAVDTP_EstablishDeviceContext = (AVDTP_EstablisDeviceContext_t) GetProcAddress(f_ModBtd, L"AVDTP_EstablishDeviceContext");
    f_pAVDTP_CloseDeviceContext    = (AVDTP_CloseDeviceContext_t)    GetProcAddress(f_ModBtd, L"AVDTP_CloseDeviceContext");
    if (!f_pAVDTP_EstablishDeviceContext || !f_pAVDTP_CloseDeviceContext)
        {
        FreeLibrary(f_ModBtd);
        f_ModBtd = NULL;
        f_pAVDTP_EstablishDeviceContext = NULL;
        f_pAVDTP_CloseDeviceContext     = NULL;
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_InitializeOnce() : btd.dll is not exporting required functions\n"));
        return ERROR_PROC_NOT_FOUND;
        }
        
    // Initialize file scope pointer 
    f_pLayerState = new A2DP_t;
    if (!f_pLayerState) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_InitializeOnce() return ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY;
        }

    // Add sdp record
    DWORD Res;
    Res = f_pLayerState->AddSdpRecord();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_ERROR, (L"A2DP: Error adding headphones SDP record.\n"));
        }
    else
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_InitializeOnce() return ERROR_SUCCESS\n"));
        }
        
    return Res;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_CreateDriverInstance ()

Description:    initiates the a2dp part of the driver

Returns:        int indicating status
-------------------------------------------------------------------*/
int 
a2dp_CreateDriverInstance 
    (
    void
    ) 
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"++A2DP :: a2dp_CreateDriverInstance\n"));
    
    if (! f_pLayerState) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    f_pLayerState->Lock();
    
    // Load default codec
    int Err = f_pLayerState->m_Codec.Load();
    if(Err != ERROR_SUCCESS)
        {
        f_pLayerState->Unlock();
        return Err;
        }
    if (f_pLayerState->hClosingEvent) 
        {
        CloseHandle(f_pLayerState->hClosingEvent);
        f_pLayerState->hClosingEvent = NULL;
        }

    f_pLayerState->hClosingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    f_pLayerState->fHaveOpenIndTime = FALSE;
    f_pLayerState->dwOpenIndTime = 0;

    //check if previously initialized
    if (f_pLayerState->IsRunning()) 
        {
        f_pLayerState->Unlock();
        return ERROR_ALREADY_EXISTS;
        }

    f_pLayerState->Initializing();
    
    AVDTP_EVENT_INDICATION AvdtpEventIndication;
    memset (&AvdtpEventIndication, 0, sizeof (AvdtpEventIndication));
    AvdtpEventIndication.avdtp_Discover_Ind = a2dp_Discover_Ind;
    AvdtpEventIndication.avdtp_GetCapabilities_Ind = a2dp_GetCapabilities_Ind;
    AvdtpEventIndication.avdtp_SetConfiguration_Ind = a2dp_SetConfiguration_Ind;
    AvdtpEventIndication.avdtp_GetConfiguration_Ind = a2dp_GetConfiguration_Ind;
    AvdtpEventIndication.avdtp_Open_Ind = a2dp_Open_Ind;
    AvdtpEventIndication.avdtp_Close_Ind = a2dp_Close_Ind;
    AvdtpEventIndication.avdtp_Start_Ind = a2dp_Start_Ind;
    AvdtpEventIndication.avdtp_Suspend_Ind = a2dp_Suspend_Ind;
    AvdtpEventIndication.avdtp_Reconfigure_Ind = a2dp_Reconfigure_Ind;
    AvdtpEventIndication.avdtp_Abort_Ind = a2dp_Abort_Ind;
    AvdtpEventIndication.avdtp_StreamAbortedEvent =  a2dp_StreamAbortedEvent;
    AvdtpEventIndication.avdtp_StackEvent = a2dp_StackEvent;
    
    AVDTP_CALLBACKS AvdptCallbacks;
    memset (&AvdptCallbacks, 0, sizeof (AvdptCallbacks));
    AvdptCallbacks.avdtp_Discover_Out = a2dp_Discover_Out;
    AvdptCallbacks.avdtp_GetCapabilities_Out = a2dp_GetCapabilities_Out;
    AvdptCallbacks.avdtp_SetConfiguration_Out = a2dp_SetConfiguration_Out;
    AvdptCallbacks.avdtp_GetConfiguration_Out = a2dp_GetConfiguration_Out;
    AvdptCallbacks.avdtp_Open_Out = a2dp_Open_Out;
    AvdptCallbacks.avdtp_Close_Out = a2dp_Close_Out;
    AvdptCallbacks.avdtp_Start_Out = a2dp_Start_Out;
    AvdptCallbacks.avdtp_Suspend_Out = a2dp_Suspend_Out;
    AvdptCallbacks.avdtp_Reconfigure_Out = a2dp_Reconfigure_Out;
    AvdptCallbacks.avdtp_Abort_Out = a2dp_Abort_Out;
    AvdptCallbacks.avdtp_CallAborted = a2dp_CallAborted;
    
    SVSUTIL_ASSERT(f_pAVDTP_EstablishDeviceContext);

    int Error = f_pAVDTP_EstablishDeviceContext( f_pLayerState, 
                                                &AvdtpEventIndication, 
                                                &AvdptCallbacks, 
                                                &f_pLayerState->avdtp_if, 
                                                &f_pLayerState->cAvdtpHeaders, 
                                                &f_pLayerState->cAvdtpTrailers, 
                                                &f_pLayerState->AVDTP
                                               );
    
    if (ERROR_SUCCESS != Error) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_CreateDriverInstance :: Could not create device context\n"));
        }
    else
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_CreateDriverInstance :: AVDTP_EstablisDeviceContext successful\n"));

        BT_LAYER_IO_CONTROL pIoctl = f_pLayerState->avdtp_if.avdtp_ioctl;
        int OutputSize = 0;
        HANDLE AVDTP = f_pLayerState->AVDTP;
        BYTE Seid = 0;
        f_pLayerState->AddRef();
        f_pLayerState->Unlock();

        __try {
            Error = pIoctl(
                        AVDTP, 
                        BTH_AVDTP_IOCTL_GET_UNIQUE_SEID, 
                        0, 
                        NULL, 
                        sizeof(Seid), 
                        (char*)(&Seid), 
                        &OutputSize);

        } __except (1) {
            DEBUGMSG(DEBUG_ERROR, ( L"Exception in avdtp_ioctl\n"));
        }
        f_pLayerState->DelRef();
        f_pLayerState->Lock();
        if (ERROR_SUCCESS != Error || OutputSize != sizeof(Seid)) 
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_CreateDriverInstance :: Could not get unique SEID\n"));
            }
         else
            {

            f_pLayerState->SetDefaultLocalSEID(Seid);
            f_pLayerState->m_CurrentLocalStreamEPInfo.SetToLocalDefaultValuesConfiguration();
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_CreateDriverInstance :: successful\n"));
            }            
        }


    //if everything is going well up to this point
    //read reg configurations
    if(Error == ERROR_SUCCESS)
        {
        f_pLayerState->ReadRegistrySettingsAndSetConfigurationValues();
        }
        

    f_pLayerState->Unlock ();

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"--A2DP :: a2dp_CreateDriverInstance\n"));
    return Error;
}




/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Bind ()

Description:    Binds the interface functions between mixer
                part of the driver and a2dp part of the driver
                
Returns:        int indicating status
-------------------------------------------------------------------*/    
int 
a2dp_Bind(
    void *pUserData,  
    A2DP_INTERFACE* pInterface
    )
{

	if (! f_pLayerState) 
	    {
		return ERROR_SERVICE_NOT_ACTIVE;
    	}

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"++A2DP :: a2dp_Bind\n"));

    if (!pInterface) 
        {
        return ERROR_INVALID_PARAMETER;
        }
    

    f_pLayerState->Lock();
    
    if (f_pLayerState->fIsBinded) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_Bind :: ERROR_ALREADY_INITIALIZED\n"));
        f_pLayerState->Unlock();
        return ERROR_ALREADY_INITIALIZED;
        }
        
    pInterface->Call = a2dp_Call_In;

    f_pLayerState->pUserData = pUserData;
    f_pLayerState->fIsBinded = TRUE;
    f_pLayerState->Initialized();

    BOOL Connected = GetAvdtpConnectionState();
    if(Connected)
        {
        f_pLayerState->StackUp();
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_Bind :: return ERROR_SUCCESS\n"));
    f_pLayerState->Unlock ();
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_UninitializeOnce ()

Description:    Unloads necessary dlls and uninitializes the a2dp
                main state variable
                
Note:           Since the driver is neve unloaded, this is never
                executed and thus is untested.
                Writing necessary code in case this is ever done
                
Returns:        int indicating status
-------------------------------------------------------------------*/
int a2dp_UninitializeOnce (void)
{
	if (!f_pLayerState) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_UninitializeOnce :: ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }
	
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_UninitializeOnce() entered\n"));

    f_pLayerState->Lock();

    // a2dp_CloseDriverInstance() has to be called before a2dp_UninitializeOnce()
    if (f_pLayerState->IsRunning()) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_UninitializeOnce ERROR_DEVICE_IN_USE\n"));
        f_pLayerState->Unlock();
        return ERROR_DEVICE_IN_USE;
    }
    
    f_pLayerState->RemoveSdpRecord();

    // want f_pLayerState to be NULL by the time we Unlock() so...
    A2DP_t *pTemp = f_pLayerState;
    f_pLayerState = NULL;
    pTemp->Unlock();
    delete pTemp;

    if (f_ModBtd) {
        FreeLibrary(f_ModBtd);
        f_ModBtd = NULL;
        f_pAVDTP_EstablishDeviceContext = NULL;
        f_pAVDTP_CloseDeviceContext     = NULL;
    }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_UninitializeOnce() return ERROR_SUCCESS\n"));
    DebugDeInit();
    svsutil_DeInitialize();
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_CloseDriverInstance ()

Description:    DeInitializes the a2dp part of the driver

Note:           Since the driver is neve unloaded, this is never
                executed and thus is untested.
                Writing necessary code in case this is ever done
                
Returns:        int indicating status
-------------------------------------------------------------------*/
int 
a2dp_CloseDriverInstance (
    void
    ) 
{

	if (! f_pLayerState) 
	    {
		return ERROR_SERVICE_NOT_ACTIVE;
    	}
	
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"++A2DP :: a2dp_CloseDriverInstance\n"));

    f_pLayerState->Lock ();
    if (! f_pLayerState->IsRunning()) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_CloseDriverInstance :: ERROR_SERVICE_NOT_ACTIVE\n"));
        f_pLayerState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
        }

	SetEvent(f_pLayerState->hClosingEvent);
    f_pLayerState->Closing();
    f_pLayerState->Unlock ();

    if (! WaitForRefToOneAndLock())
        {
        // On failure we are unlocked
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    CallListClear(ERROR_SHUTDOWN_IN_PROGRESS);
    if (f_pLayerState->AVDTP) {
        SVSUTIL_ASSERT(f_pAVDTP_CloseDeviceContext);
        f_pAVDTP_CloseDeviceContext (f_pLayerState->AVDTP);
    }
    f_pLayerState->ResetToPreConfigurationValues();
    f_pLayerState->Unlock();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"--A2DP :: a2dp_CloseDriverInstance :: ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Unbind ()

Description:    Unbinds the interface functions between mixer
                part of the driver and a2dp part of the driver

Note:           Since the driver is neve unloaded, this is never
                executed and thus is untested.
                Writing necessary code in case this is ever done
                
Returns:        int indicating status
-------------------------------------------------------------------*/    
int 
a2dp_Unbind()
{
	if (! f_pLayerState) 
	    {
		return ERROR_SERVICE_NOT_ACTIVE;
	    }
	
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"++A2DP :: a2dp_Unbind\n"));

    f_pLayerState->Lock ();

    if (!f_pLayerState->fIsBinded) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_Unbind :: ERROR_SERVICE_NOT_ACTIVE\n"));
        f_pLayerState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
        }
        
    // abort all current Calls
    CallListClear(ERROR_OPERATION_ABORTED);

    
    // abort all Links
    f_pLayerState->RemoteBTEndPoint.Zero();
    //should also abort the avtdp layer here

    memset(&f_pLayerState->a2dp_callbacks, 0, sizeof(A2DP_CALLBACKS));
    f_pLayerState->pUserData = NULL;
    f_pLayerState->fIsBinded = FALSE;

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Unbind :: return ERROR_SUCCESS\n"));
    f_pLayerState->Unlock ();
    return ERROR_SUCCESS;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_IsReadyToWrite ()

Description:    Queries the a2dp state for having a stream in 
                streaming state
                
Returns:        BOOL indicating ability to stream to a2dp
-------------------------------------------------------------------*/
BOOL a2dp_IsReadyToWrite()
{
    BOOL Ret = FALSE;
    f_pLayerState->Lock ();
    Ret = f_pLayerState->m_CurrentStream.IsStreaming();
    f_pLayerState->Unlock ();

    return Ret;
 }

BOOL a2dp_HasBeenOpened()
{
    BOOL Ret = FALSE;
    f_pLayerState->Lock ();
    Ret = (f_pLayerState->m_CurrentStream.IsStreaming()||f_pLayerState->m_CurrentStream.IsOpened());
    f_pLayerState->Unlock ();

    return Ret;
 }
////////////////////////////////////////////
// stack notification routines
// note: they are executed asynchronously (a thread is created for them)
//       (they do not execute on same thread as HCI callback)
//
//  StackInitDevice
//  StackNotifyUser
// The following 3 routines are ThreadProcs [see function a2dp_Stack_Event]
//  StackDisconnect
//  StackDown
//  StackUp
////////////////////////////////////////////

static 
int 
a2dp_Discover_Out (
    void* pCallContext, 
    BD_ADDR* pba, 
    PAVDTP_ENDPOINT_INFO pAcpSEIDInfo, 
    DWORD cAcpSEIDInfo, 
    BYTE Error
    )
{
    FUNCTION_TRACE(a2dp_Discover_Out);


    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_DISCOVER);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }


    if (BT_NO_ERROR == Error)
        {
        if(0 == f_pLayerState->SetRemoteBTSEIDsAndChooseCurrentSEID(0, pAcpSEIDInfo, cAcpSEIDInfo))
            {
            f_pLayerState->ResetToPreConfigurationValues();
            Ret = ERROR_INTERNAL_ERROR;
            }
        }
    else
        {
        f_pLayerState->ResetToPreConfigurationValues();
        Ret = ERROR_INTERNAL_ERROR;
        }


    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;

}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_GetCapabilities_Out ()

Description:    This function looks at the capabilities returned
                by the remote end point when we did a GetCapabilites
                on the first end point determined to be a relevant
                by a2dp_GetCapabilities (ie. sink,  not in use, 
                audio media type).
                If the capabilities are acceptable for this 
                implementation of a2dp, signal success to
                caller of a2dp_GetCapabilities and return 
                ERROR_SUCCESS to avdtp.

                This function signals different values to the caller
                of a2dp_GetCapabilities (in call signal) and the
                avdtp layer that actually called this function:
                Since avdtp will tear down the connection if we return 
                an error, this function needs to return success to avdtp
                if a2dp will try another end point (in case the 
                capabilities for the current end point are 
                not acceptable and there are other available end points).

                While avdtp only cares about having no fatal errors,
                the callee of a2dp_GetCapabilities needs to know if 
                one of the following  happened:
                1 - Current end point has acceptable capabilities:
                    signal  ERROR_SUCCESS
                2 - Current end point does not have acceptable 
                    capabilities, but there are more end points to 
                    look at:
                    signal ERROR_BAD_CONFIGURATION
                3 - Current end point does not have acceptable 
                    capabilities, and there are *no* more end points 
                    to look at
                    -OR-
                    some other internal fatal error happened in the
                    function or in bluetooth:
                    signal ERROR_INTERNAL_ERROR


Note:           This function picks the first acceptable end point,
                not necessarely the best one
                
Returns:        int indicating if any fatal errors happened
                (see above description for more details)
-------------------------------------------------------------------*/
static 
int 
a2dp_GetCapabilities_Out (
    void* pCallContext, 
    BD_ADDR* pba, 
    PAVDTP_CAPABILITY_IE pCapabilityIE, 
    DWORD cCapabilityIE, 
    BYTE Error)
{
    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_GET_CAPABILITIES);
    int Status = Ret;
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    SVSUTIL_ASSERT(pCall->CallType == A2DP_CALL_TYPE_GET_CAPABILITIES);
    if(0 == Error)
        {
        if(f_pLayerState->ProcessCapabilities(pCapabilityIE, cCapabilityIE))
            {
            //the capabilities in this end point were acceptable 
            //to a2dp, so set them to be the remote caps a2dp is using
            f_pLayerState->SetRemoteEPCapabilities(pCapabilityIE, cCapabilityIE);
            Ret = ERROR_SUCCESS;
            Status = Ret;
            }
        else
            {
            //this end point didn't have any acceptable capabilites,
            //so attempt to go the the next end point reported by remote device
            if(0 == f_pLayerState->SetRemoteBTSEIDsAndChooseCurrentSEID(f_pLayerState->GetCurrentRemoteSEID(), 
                                                                       NULL, 
                                                                       0))
                {
                //there aren't any other end points we haven't tried yet,
                //return ERROR_INTERNAL_ERROR so that avdtp knows to tear
                //down the connection
                // singal ERROR_INTERNAL_ERROR so that the callee 
                // of a2dp_GetCapabilities
                //can distinguish between the situation when we tried a 
                //bad end point, but still have other end points to try
                //or not
                f_pLayerState->ResetToPreConfigurationValues();
                Ret = ERROR_INTERNAL_ERROR;
                Status = Ret;
                }
            else
                {
                //there are end points we have not tried
                //but this particular one is not acceptable

                //return ERROR_SUCCESS to avoid avdtp tearing 
                //down the connection
                Ret = ERROR_SUCCESS;

                //signal ERROR_BAD_CONFIGURATION so that the callee
                //of a2dp_GetCapabilities knows that there are more 
                //end points to try
                Status = ERROR_BAD_CONFIGURATION; 
                }
            }
            
        }
    else
        {
        //some error happend in the lower layer, 
        //so do not attempt to configure
        f_pLayerState->ResetToPreConfigurationValues();
        Ret = ERROR_INTERNAL_ERROR;
        Status = Ret;
        }
    CallRemove (pCall);
    CallSignal (pCall, Status, Error);

    f_pLayerState->Unlock();  
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities_Out :: Completed call 0x%08x\n", pCall));

    return Ret;
}

static 
int 
a2dp_SetConfiguration_Out (
    void* pCallContext, 
    HANDLE Stream, 
    BYTE bServiceCategory, 
    BYTE Error
    )
{

    FUNCTION_TRACE(a2dp_SetConfiguration_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_CONFIGURE);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    if (BT_NO_ERROR == Error)
        {
        f_pLayerState->m_CurrentStream.Configured(Stream);
        f_pLayerState->m_CurrentLocalStreamEPInfo.InUse();
        SetBDAddrList(f_pLayerState->pBdAdresses, ARRAYSIZE(f_pLayerState->pBdAdresses), f_pLayerState->GetRemoteBdAddress());

        }
    else
        {
        Ret = ERROR_INTERNAL_ERROR;
        f_pLayerState->ResetToPreConfigurationValues();
        }


    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_SetConfiguration_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

static
int 
a2dp_GetConfiguration_Out (
    void* pCallContext, 
    HANDLE Stream, 
    PAVDTP_CAPABILITY_IE pCapabilityIE, 
    DWORD cCapabilityIE, 
    BYTE Error
    )
{
    FUNCTION_TRACE(a2dp_GetConfiguration_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_GET_CONFIGURATION);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetConfiguration_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

static 
int 
a2dp_Open_Out (
    void* pCallContext, 
    HANDLE Stream, 
    BYTE Error
    )
{

    FUNCTION_TRACE(a2dp_Open_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_OPEN);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }
    Ret = ERROR_INTERNAL_ERROR;
    if (BT_NO_ERROR == Error)
        {
        if(Stream == f_pLayerState->GetCurrentStreamHandle())
            {
            f_pLayerState->m_CurrentStream.Opened();
            Ret = f_pLayerState->m_Codec.OpenStream(NULL);
            }
        else
            {
            Ret = ERROR_BAD_ARGUMENTS;
            }
        }
    else
        {
        Ret = ERROR_INTERNAL_ERROR;
        }

    if(ERROR_SUCCESS != Ret)
        {
        f_pLayerState->ResetToPreConfigurationValues();
        } 

    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Abort_Out()

Description:    Callback from avdtp, from a abort request 
                When the abort callback is received, all the
                internal state is reset

Arguments:      void* pCallContext
                    CallCOntext created by the abortreq and
                    passed to avdtp
                HANDLE Stream
                    Stream handle for stream being aborted
                BYTE Error
                    Error status of the callback
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static 
int 
a2dp_Abort_Out (
    void* pCallContext, 
    HANDLE Stream, 
    BYTE Error
    )
{
    FUNCTION_TRACE(a2dp_Abort_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_ABORT);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }
    if(Stream == f_pLayerState->GetCurrentStreamHandle())
        { 
        f_pLayerState->ResetToPreConfigurationValues();
        }
    else
        {
        Ret =  ERROR_BAD_ARGUMENTS;
        f_pLayerState->ResetToPreConfigurationValues();
        }

    CallListClear(ERROR_OPERATION_ABORTED);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();
    return Ret;

}

static 
int 
a2dp_Close_Out (
    void* pCallContext, 
    HANDLE Stream, 
    BYTE Error
    )
{
    FUNCTION_TRACE(a2dp_Close_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_CLOSE);
    if(ERROR_SUCCESS != Ret)
        {
        //call already removed by streamAbortEvent or no longer connected
        return ERROR_SUCCESS;
        }
        
    SVSUTIL_ASSERT(Stream == f_pLayerState->GetCurrentStreamHandle());
    if(Stream == f_pLayerState->GetCurrentStreamHandle())
        { 
        f_pLayerState->ResetToPreConfigurationValues();
        }
    else
        {
        f_pLayerState->ResetToPreConfigurationValues();
        }

    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

static 
int 
a2dp_Start_Out (
    void* pCallContext, 
    HANDLE FirstFailingStream, 
    BYTE Error
    )
{

    FUNCTION_TRACE(a2dp_Start_Out);
    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_START);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    // Check if we are already in a streaming state and only process the response if not
    if (! f_pLayerState->m_CurrentStream.IsStreaming()) 
        {
        if ((BT_NO_ERROR == Error) &&
            (NULL == FirstFailingStream))
            {
            f_pLayerState->m_CurrentStream.Streaming();
            }
        else
            {
            Ret = ERROR_INTERNAL_ERROR;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

static 
int 
a2dp_Suspend_Out (
    void* pCallContext, 
    HANDLE FirstStream, 
    HANDLE FirstFailingStream, 
    BYTE Error
    )
{
    FUNCTION_TRACE(a2dp_Suspend_Out);

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, A2DP_CALL_TYPE_SUSPEND);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    if ((BT_NO_ERROR == Error) &&
        (f_pLayerState->m_CurrentStream.IsStreaming()) &&
        (FirstStream == f_pLayerState->GetCurrentStreamHandle()))
        {
        f_pLayerState->m_CurrentStream.Suspended();
        }

    // finish the call
    CallRemove (pCall);
    CallSignal (pCall, Ret, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Suspend_Out :: Completed call 0x%08x\n", pCall));

    f_pLayerState->Unlock ();

    return Ret;
}

static int a2dp_Reconfigure_Out (void* pCallContext, HANDLE Stream, BYTE bServiceCategory, BYTE Error)
{
    return ERROR_CALL_NOT_IMPLEMENTED;
}





static int a2dp_CallAborted (void *pCallContext, int Error) 
{
    FUNCTION_TRACE(a2dp_CallAborted);
    
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_CallAborted :: pCallContext: [0x%08x\n], Error: [0x%08x]\r\n", pCallContext, Error)); 

    Call_t *pCall = (Call_t*)pCallContext;
    int Ret = LockIfConnectedAndCallContextIsFound(pCall, 0);
    if(ERROR_SUCCESS != Ret)
        {
        return Ret;
        }

    if (! f_pLayerState->IsRunning()) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: Call Aborted :: system shutdown\n"));
        f_pLayerState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    CallRemove (pCall);
    CallSignal (pCall, Error);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: Call Abort :: aborted call 0x%08x type %d\n", pCall, pCall->CallType));

    f_pLayerState->Unlock ();

    return Ret;
}

//////////////////////////////////////////////////////////////////
//  Events
//////////////////////////////////////////////////////////////////

static 
int 
a2dp_Discover_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    BD_ADDR* pba)
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover_Ind :: pUserContext: [0x%08x], Transaction: [0x%08x], pba [0x%08x]\r\n", pUserContext, Transaction, pba)); 

    int Res = ERROR_SUCCESS;

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    f_pLayerState->ReadRegistrySettingsAndSetConfigurationValues();

    AVDTP_DiscoverRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_DiscoverRsp_In;
    HANDLE AVDPT = f_pLayerState->AVDTP;

    AVDTP_ENDPOINT_INFO AvdptEndPointInfo = f_pLayerState->m_CurrentLocalStreamEPInfo.GetAvdptEndPointInfo();


    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    
    __try {
        Res = pCallback(AVDPT, 
                        NULL, 
                        Transaction, 
                        pba, 
                        &AvdptEndPointInfo, 
                        1, 
                        BT_NO_ERROR);    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in a2dp_DiscoverRsp_In\n"));
    }

    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if (Res != ERROR_SUCCESS) {
        SVSUTIL_ASSERT(0);
    }

    f_pLayerState->Unlock();

    return Res;
}

static
int 
a2dp_GetCapabilities_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    BD_ADDR* pba, 
    BYTE AcpSEID
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], pba [0x%08x]\r\n", pUserContext, Transaction, pba)); 

    int Res = ERROR_SUCCESS;
    BYTE Error = BT_NO_ERROR;
    
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    AVDTP_GetCapabilitiesRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_GetCapabilitiesRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;
    StreamEndPoint_t DefaultLocalStreamEndpoint; 
    DWORD Ret = DefaultLocalStreamEndpoint.SetToLocalDefaultValuesCapabilities();

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();


    if(AcpSEID != DefaultLocalStreamEndpoint.GetSEID())
        {
        Error = BT_ERROR;
        //Unmatched SEID, so no caps 
        DefaultLocalStreamEndpoint.ClearCapabilitiesIE();
        }

    __try {
        Res = pCallback(AVDTP, 
                        NULL, 
                        Transaction, 
                        pba, 
                        DefaultLocalStreamEndpoint.GetCapabilitiesIE(), 
                        DefaultLocalStreamEndpoint.GetCapabilitiesIECount(), 
                        Error
                        );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_GetCapabilitiesRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if (Res != ERROR_SUCCESS) {
        SVSUTIL_ASSERT(0);
    }

    f_pLayerState->Unlock();

    return Res;

}

static 
int 
a2dp_SetConfiguration_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE Stream, 
    BD_ADDR* pba, 
    BYTE bAcpSEID, 
    BYTE bIntSEID, 
    PAVDTP_CAPABILITY_IE pCapabilityIE, 
    DWORD cCapabilityIE
    )
{

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_SetConfiguration_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], Stream [0x%08x]\r\n", pUserContext, Transaction, Stream)); 

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    BYTE Error = BT_NO_ERROR;
    AVDTP_SetConfigurationRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_SetConfigurationRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;
    DWORD FirstFailingConfiguration = 0;
    BYTE ServiceCategory = 0;


    if(!f_pLayerState->m_CurrentStream.IsClosed())
        {
        Error = BT_ERROR;
        f_pLayerState->Unlock();
        return ERROR_ALREADY_INITIALIZED;
        }
        
    if(!f_pLayerState->ProcessConfiguration(
                            pCapabilityIE,
                            cCapabilityIE, 
                            FirstFailingConfiguration,
                            Error
                            )
                                )
        {
        ServiceCategory = pCapabilityIE[FirstFailingConfiguration].bServiceCategory;
        }
    int Res = ERROR_SUCCESS;

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    

    __try {
        Res = pCallback(
                AVDTP, 
                NULL, 
                Transaction,
                Stream,
                ServiceCategory, 
                Error
                );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_SetConfigurationRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if (Res != ERROR_SUCCESS) 
        {
        SVSUTIL_ASSERT(0);
        }
    else
        {
        f_pLayerState->SetRemoteBdAddress(pba);
        SetBDAddrList(f_pLayerState->pBdAdresses, ARRAYSIZE(f_pLayerState->pBdAdresses), f_pLayerState->GetRemoteBdAddress());        
        f_pLayerState->m_CurrentStream.Configured(Stream);
        f_pLayerState->m_CurrentLocalStreamEPInfo.InUse();

        }


    f_pLayerState->Unlock();

    return Res;
}
static 
int 
a2dp_GetConfiguration_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE Stream
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetConfiguration_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], Stream [0x%08x]\r\n", pUserContext, Transaction, Stream)); 
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    BYTE Error = BT_NO_ERROR;
    AVDTP_GetConfigurationRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_GetConfigurationRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;

    AVDTP_CAPABILITY_IE pLocalConfigurationIE[StreamEndPoint_t::MAX_NUM_CAPABILITIES];
    DWORD cLocalConfigurationIE = StreamEndPoint_t::MAX_NUM_CAPABILITIES;
    
    if (Stream != f_pLayerState->GetCurrentStreamHandle())
        {
        Error = BT_ERROR;
        memset(pLocalConfigurationIE, 0, sizeof(pLocalConfigurationIE));
        cLocalConfigurationIE = 0;
        }
    else
        {
        f_pLayerState->m_CurrentLocalStreamEPInfo.GetCapabilitiesIEArrayCopy(pLocalConfigurationIE, cLocalConfigurationIE);
        }

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
            AVDTP, 
            NULL, 
            Transaction,
            Stream,
            pLocalConfigurationIE, 
            cLocalConfigurationIE, 
            Error
            );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_GetConfigurationRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if (Res != ERROR_SUCCESS) {
        SVSUTIL_ASSERT(0);
    }

    f_pLayerState->Unlock();

    return Res;
}

static 
int 
a2dp_Open_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE Stream
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], Stream [0x%08x]\r\n", pUserContext, Transaction, Stream)); 
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }


    BYTE Error = BT_NO_ERROR;
    AVDTP_OpenRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_OpenRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;

    if (Stream != f_pLayerState->GetCurrentStreamHandle())
        {
        //bad stream handle, send avdtp an error
        //lower layer should tear down connection
        Error = BT_ERROR;
        //since this is presumably a lower layer issue, 
        //do not change a2dp state
        }

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
            AVDTP, 
            NULL, 
            Transaction,
            Stream,
            Error
            );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in a2dp_Open_Ind\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if((Res == ERROR_SUCCESS) &&
       (Error == BT_NO_ERROR))
        {
        f_pLayerState->dwOpenIndTime = GetTickCount();
        f_pLayerState->fHaveOpenIndTime = TRUE;
        f_pLayerState->m_CurrentStream.Opened();
        DWORD Ret = f_pLayerState->m_Codec.OpenStream(NULL);
        SVSUTIL_ASSERT(Ret == ERROR_SUCCESS);
        //TODO: add reg setting here?
        g_pHWContext->MakePreferredDriver();
        }
    else
        {
        //something bad has happened, so reset state
        f_pLayerState->ResetToPreConfigurationValues();
        }

    f_pLayerState->Unlock();
    return Res;
}

static 
int 
a2dp_Close_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE Stream
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], Stream [0x%08x]\r\n", pUserContext, Transaction, Stream)); 
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }


    BYTE Error = BT_NO_ERROR;
    AVDTP_CloseRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_CloseRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;

    if (Stream != f_pLayerState->GetCurrentStreamHandle())
        {
        Error = BT_ERROR;
        }

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
            AVDTP, 
            NULL, 
            Transaction,
            Stream,
            Error
            );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_CloseRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    
    if (Res != ERROR_SUCCESS) 
        {
        SVSUTIL_ASSERT(0);
        f_pLayerState->ResetToPreConfigurationValues();
        }
    else
        {
        f_pLayerState->ResetToPreConfigurationValues();
        }
    
    g_pHWContext->MakeNonPreferredDriver(); //remote end is turning off
                                            //make sure no longer preferred

    f_pLayerState->Unlock();
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Abort_Ind()

Description:    Notification from avdtp, remote end requested
                an abort. Abort all internal state here, and 
                stop/return buffers if streaming

Arguments:      void* pUserContext
                    User Context created by the abortreq and
                    passed to avdtp
                BYTE Transaction
                    avdtp transaction identifier
                HANDLE Stream
                    Stream handle for stream being aborted

                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static
int 
a2dp_Abort_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE Stream
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort_Ind :: pUserContext: [0x%08x\n], Transaction: [0x%08x], Stream [0x%08x]\r\n", pUserContext, Transaction, Stream)); 
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    // Check if have the correct stream handle
    BYTE Error = BT_NO_ERROR;
    if(Stream != f_pLayerState->GetCurrentStreamHandle())
        { 
        Error = BT_ERROR;
        }

    // Create a2dp layer call for response
    AVDTP_AbortRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_AbortRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
            AVDTP, 
            NULL, 
            Transaction,
            Stream,
            Error
            );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_AbortRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    
    // Check if call successful. 
    // Reset state regardless of success
    if (Res != ERROR_SUCCESS) 
        {
        SVSUTIL_ASSERT(0);
        }
    g_pHWContext->MakeNonPreferredDriver();

    f_pLayerState->ResetToPreConfigurationValues();

    f_pLayerState->Unlock();
    return Res;
}

static 
int 
a2dp_Start_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE* pStreamHandles, 
    DWORD cStreamHandles
    )
{
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
        
    int Res = ERROR_SUCCESS;
    BYTE Error = BT_NO_ERROR;
    AVDTP_StartRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_StartRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE FirstFailingStream = NULL;
        
    if (0 == cStreamHandles)
        {
        Error = BT_ERROR;
        }
    else
        {
        if((cStreamHandles > 1) ||
           (pStreamHandles[0] != f_pLayerState->GetCurrentStreamHandle()))
            {
            Error = BT_ERROR;
            FirstFailingStream = pStreamHandles[0];
            f_pLayerState->ResetToPreConfigurationValues();
            }
        else
            {
            //everything from remote side is ok, now check local state
            if(f_pLayerState->m_CurrentStream.IsOpened())
                {            

                BT_LAYER_IO_CONTROL pIoctl = f_pLayerState->avdtp_if.avdtp_ioctl;

                //set flow control
                AVDTP_SET_BIT_RATE call;
                call.dwSize = sizeof(call);
                f_pLayerState->m_Codec.GetBitRate(call.dwBitRate);

                call.hStream = f_pLayerState->GetCurrentStreamHandle();
                f_pLayerState->m_Codec.GetHeaderSize(call.cbCodecHeader);
                f_pLayerState->AddRef();
                f_pLayerState->Unlock();

                __try {
                    Res = pIoctl(AVDTP, BTH_AVDTP_IOCTL_SET_BIT_RATE, sizeof(call), (char *)&call, 0, NULL, NULL);
                } __except (1) {
                    DEBUGMSG(DEBUG_ERROR, ( L"Exception in avdtp_ioctl\n"));
                }

                f_pLayerState->Lock  ();
                f_pLayerState->DelRef ();

                if (ERROR_SUCCESS != Res) 
                    {
                    SVSUTIL_ASSERT(0);
                    f_pLayerState->ResetToPreConfigurationValues();
                    }
                
                }
            else
                {
                if(!f_pLayerState->m_CurrentStream.IsStreaming())
                    {
                    Error = BT_ERROR;
                    FirstFailingStream = pStreamHandles[0];
                    f_pLayerState->ResetToPreConfigurationValues();
                    }
                }
            
            }
        }


    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    

    __try {
        Res = pCallback(
                AVDTP, 
                NULL, 
                Transaction,
                FirstFailingStream, 
                Error
                );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_StartRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if (Res != ERROR_SUCCESS) 
        {
        f_pLayerState->ResetToPreConfigurationValues();
        SVSUTIL_ASSERT(0);
        }
    else
        {
        f_pLayerState->m_CurrentStream.Streaming();
        }


    f_pLayerState->Unlock();

    return Res;
}

static 
int 
a2dp_Suspend_Ind (
    void* pUserContext, 
    BYTE Transaction, 
    HANDLE* pStreamHandles, 
    DWORD cStreamHandles
    )
{
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    BYTE Error = BT_NO_ERROR;
    AVDTP_SuspendRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_SuspendRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE FirstFailingStream = NULL;
    HANDLE FirstStream = NULL;
        
    if (0 == cStreamHandles)
        {
        Error = BT_ERROR;
        }
    else
        {
        FirstStream = pStreamHandles[0];
        if((cStreamHandles > 1) ||
           (pStreamHandles[0] != f_pLayerState->GetCurrentStreamHandle()))
            {
            Error = BT_ERROR;
            FirstFailingStream = pStreamHandles[0];
            }
        else
            {
            //everything from remote side is ok, now check local state
            if(f_pLayerState->m_CurrentStream.IsStreaming())
                {            
                f_pLayerState->m_CurrentStream.Suspended();
                }
            else
                {
                if(!f_pLayerState->m_CurrentStream.IsSuspended())
                    {
                    Error = BT_ERROR;
                    FirstFailingStream = pStreamHandles[0];
                    }
                }
            
            }
        }


    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
                AVDTP, 
                NULL, 
                Transaction,
                FirstStream,
                FirstFailingStream, 
                Error
                );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_SuspendRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if ((Res != ERROR_SUCCESS) ||
         (Error != BT_NO_ERROR)
        ) 
        {
        f_pLayerState->ResetToPreConfigurationValues();
        SVSUTIL_ASSERT(0);
        }

    f_pLayerState->Unlock();

    return Res;
}

static int a2dp_Reconfigure_Ind (void* pUserContext, BYTE Transaction, HANDLE Stream, PAVDTP_CAPABILITY_IE pCapabilityIE, DWORD cCapabilityIE)
{

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }

    BYTE Error = BT_NO_ERROR;
    AVDTP_ReconfigureRsp_In pCallback = f_pLayerState->avdtp_if.avdtp_ReconfigureRsp_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;
    DWORD FirstFailingConfiguration = 0;
    BYTE ServiceCategory = 0;


    if((Stream != f_pLayerState->GetCurrentStreamHandle()) ||
        (!f_pLayerState->m_CurrentStream.IsSuspended()))
        {
        Error = BT_ERROR;
        }
        
    if(!f_pLayerState->ProcessConfiguration(
                            pCapabilityIE,
                            cCapabilityIE, 
                            FirstFailingConfiguration,
                            Error
                            )
                                )
        {
            ServiceCategory = pCapabilityIE[FirstFailingConfiguration].bServiceCategory;
        }
                                

    f_pLayerState->AddRef();
    f_pLayerState->Unlock();
    
    int Res = ERROR_SUCCESS;

    __try {
        Res = pCallback(
                AVDTP, 
                NULL, 
                Transaction,
                Stream,
                ServiceCategory, 
                Error
                );    
    } __except(1) {
        DEBUGMSG(1, (L"A2DP: Exception in avdtp_ReconfigureRsp_In\n"));
    }
    f_pLayerState->Lock();
    f_pLayerState->DelRef();

    if ((Res != ERROR_SUCCESS) ||
         (Error != BT_NO_ERROR)
        ) 
        {
        f_pLayerState->ResetToPreConfigurationValues();
        SVSUTIL_ASSERT(0);
        }

    f_pLayerState->Unlock();

    return Res;
}






//assume holds
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StackNotifyUser()

Description:    Notifies upper layer (actual audio driver part
                of a2dp) of stack event. Assumes lock is held when 
                called.
Arguments:      int iEvent
                integer representing the stack event being notified
                    
Returns:        int indicating success
-------------------------------------------------------------------*/
static 
int 
StackNotifyUser(
    int iEvent
    )
{

/*    A2DP_CALLBACK_StackEvent pCallBack = f_pLayerState->a2dp_callbacks.StackEvent;
    if (pCallBack) {

        int iRes = ERROR_INTERNAL_ERROR;
        void *pUserData = f_pLayerState->pUserData;

        f_pLayerState->AddRef ();
        f_pLayerState->Unlock ();
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: StackNotifyUser :: going into callback\n"));

        __try {
            iRes = pCallBack (pUserData, iEvent);
        } __except (1) {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: StackNotifyUser :: exception in callback\n"));
        }

        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: StackNotifyUser :: came from callback\n"));
        f_pLayerState->Lock  ();
        f_pLayerState->DelRef ();
    }

    f_pLayerState->Unlock();*/
    //don't actually need this, since the upper layer will hit an 
    //error if the stack is down
    return ERROR_CALL_NOT_IMPLEMENTED;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StackUp()

Description:    Processes stack up event, mark stack as up
                allowing for connections to remote end points

Returns:        DWORD indicating status
-------------------------------------------------------------------*/
DWORD 
static 
WINAPI 
StackUp(
    LPVOID lpv
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::++StackUp\n"));
	if (! f_pLayerState) 
	    {
		return ERROR_SERVICE_NOT_ACTIVE;
	    }
	
    if (! WaitForRefToOneAndLock()) 
        {
        // On failure we are unlocked
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    DWORD Res = ERROR_SUCCESS;
    if (f_pLayerState->IsStackConnected()) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: Stack Up :: already up\n"));
        Res = ERROR_SERVICE_ALREADY_RUNNING;
        }

    if(ERROR_SUCCESS == Res)
        {
        f_pLayerState->StackUp();
        }
    f_pLayerState->AddRef();
    StackNotifyUser(BTH_STACK_UP);
    f_pLayerState->DelRef();
    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::--StackUp\n"));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StackDown()

Description:    Processes stack down event, abort current stream
                state

Returns:        DWORD indicating status
-------------------------------------------------------------------*/
DWORD 
static WINAPI StackDown(
    LPVOID lpv
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::++StackDown\n"));
    if (! f_pLayerState) 
        {
		return ERROR_SERVICE_NOT_ACTIVE;
	    }
	    

    if(WaitForSingleObject(f_pLayerState->hClosingEvent, 0) != WAIT_TIMEOUT) 
        {
        return 0;
        }
        
    if (! WaitForRefToOneAndLock())
        {
        // On failure we are unlocked
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
    
    if (f_pLayerState->IsStackDown()) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: Stack Down received :: already down\n"));
        f_pLayerState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    f_pLayerState->ResetToPreConfigurationValues();
    CallListClear(ERROR_SERVICE_DISABLED);
    f_pLayerState->StackDown();

    f_pLayerState->AddRef();
    StackNotifyUser(BTH_STACK_DOWN);
    f_pLayerState->DelRef();
    
    f_pLayerState->Unlock ();

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::--StackDown\n"));

    return 0;
}

static DWORD WINAPI StackReset(LPVOID lpv)
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::++StackReset\n"));
    
    StackDown (NULL);
    StackUp (NULL);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::--StackReset\n"));
    
    return 0;
}

static DWORD WINAPI StackDisconnect(LPVOID lpv)
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::++StackDisconnect\n"));
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP ::--StackDisconnect\n"));
    
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_StackEvent()

Description:    callback from avdtp with stack events, set the
                appropriate events to update driver state

Returns:        Boolean indicating status
-------------------------------------------------------------------*/

static 
int 
a2dp_StackEvent (
    void *pUserContext, 
    int iEvent, 
    void *pEventContext
    )
{
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"++a2dp_StackEvent 0x%08X iEvent:%d pUserContext:0x%08X\n", pUserContext, iEvent, pEventContext));

    SVSUTIL_ASSERT(pUserContext == f_pLayerState);

    switch (iEvent) {
        case BTH_STACK_RESET:
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_StackEvent : Stack reset\n"));
            CloseHandle (CreateThread (NULL, 0, StackReset, NULL, 0, NULL));
            break;

        case BTH_STACK_DOWN:
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_StackEvent : Stack down\n"));
            CloseHandle (CreateThread (NULL, 0, StackDown, NULL, 0, NULL));
            break;

        case BTH_STACK_UP:
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_StackEvent : Stack up\n"));
            CloseHandle (CreateThread (NULL, 0, StackUp, NULL, 0, NULL));
            break;

        case BTH_STACK_DISCONNECT:
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_StackEvent : Stack disconnect\n"));
            CloseHandle (CreateThread (NULL, 0, StackDisconnect, NULL, 0, NULL));
            break;

        default:
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_StackEvent : Unknown event, disconnect out of paranoia\n"));
            CloseHandle (CreateThread (NULL, 0, StackDown, NULL, 0, NULL));
            break;
    }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"--a2dp_StackEvent:: ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}

static
int
a2dp_StreamAbortedEvent(
    void* pUserContext, 
    HANDLE Stream, 
    int Error)
{
    FUNCTION_TRACE(a2dp_StreamAbortedEvent);
    int Ret = ERROR_SUCCESS;

    // This call will mark all audio buffers as done and stop the
    // audio driver streaming.
    g_pHWContext->Lock();
    g_pHWContext->ResetDeviceContext();
    g_pHWContext->Unlock();

    if (! f_pLayerState) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    f_pLayerState->Lock();

    //only switch out if stream aborted not expected
    if(!f_pLayerState->m_CurrentStream.IsClosed())
        {
        //TODO: add reg setting here?
        g_pHWContext->MakeNonPreferredDriver();
        }
        
    if(Stream == f_pLayerState->GetCurrentStreamHandle())
        { 
        f_pLayerState->ResetToPreConfigurationValues();
        }
    else
        {
        Ret =  ERROR_BAD_ARGUMENTS;
        f_pLayerState->ResetToPreConfigurationValues();
        }

    CallListClear(Error);

    f_pLayerState->Unlock ();
    return Ret;

}

///////////////////////////////////////////////////////////
// Interface functions
///////////////////////////////////////////////////////////

static int a2dp_AbortCall(Call_t *pCall, BOOL bHciAbort)
{
    if (!f_pLayerState) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
    
    SVSUTIL_ASSERT(pCall);
    SVSUTIL_ASSERT(f_pLayerState->IsLocked());

    if (!pCall) 
        {
        return ERROR_INVALID_PARAMETER;
        } 
    else if (!f_pLayerState) 
        {
        return ERROR_SHUTDOWN_IN_PROGRESS;
        } 
    else if (!f_pLayerState->IsLocked()) 
        {
        return ERROR_NOT_LOCKED;
        }
//need abort mechanism, implement later
/*    if (bHciAbort) {
        //
        // HCI was invoked using this pCall, so tell HCI to abort that call.
        // invoke hci_AbortCall.
        // This will induce a a2dp_CallAborted_Out callback
        //

        BT_LAYER_ABORT_CALL pFunction = f_pLayerState->hci_if.hci_AbortCall;
        HANDLE hAVDTP = f_pLayerState->hAVDTP;

        int iRes = ERROR_INTERNAL_ERROR;

        f_pLayerState->AddRef ();
        f_pLayerState->Unlock ();

        __try {
            iRes = pFunction (hAVDTP, pCall);
        } __except (1) {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_AbortCall :: exception in hci_AbortCall\n"));
        }

        f_pLayerState->Lock  ();
        f_pLayerState->DelRef ();

    } else {
        //
        // HCI has not been invoked using this pCall so remove it from pCalls and signal it
        //
        CallRemove(pCall);
        CallSignal(pCall, ERROR_OPERATION_ABORTED);
    }
*/
    return ERROR_SUCCESS;
}

static int a2dp_AbortUserCall_In (A2DP_USER_CALL* pUserCall)
{
/*	if (!f_pLayerState) {
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    SVSUTIL_ASSERT(pUserCall);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_AbortUserCall_In %#x\n", pUserCall));

    f_pLayerState->Lock ();
    if (! (f_pLayerState->IsRunning() && f_pLayerState->IsConnected())) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_AbortUserCall_In %#x : ERROR_SERVICE_NOT_ACTIVE\n", pUserCall));
        f_pLayerState->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    // find the call
    Call_t *pCall = CallFindByUserCall(pUserCall);
    if (!pCall) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_AbortUserCall_In %#x : cannot find Call_t\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_NOT_FOUND;
    }

    a2dp_AbortCall(pCall, pCall->fInvokedHCI);

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_AbortUserCall_In :: ERROR_SUCCESS\n"));
    f_pLayerState->Unlock ();
    */
    return ERROR_SUCCESS;
}

static int a2dp_Call_In (A2DP_USER_CALL* pUserCall)
{
    if (!f_pLayerState) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Call_In %#x\n", pUserCall));

    if (!pUserCall) 
        {
        return ERROR_INVALID_PARAMETER;
        }

    int Ret = ERROR_SUCCESS;
    pUserCall->iResult = ERROR_SUCCESS;
    
    if (pUserCall->dwUserCallType == UCT_WRITE) 
        {
        // Fast path for writes
        Ret = a2dp_Write(pUserCall);
        } 
    else 
        {
        // if user wants synchronous call we will create
        // an event and wait for completion

        HANDLE LocalEvent = NULL;
        if (!pUserCall->hEvent) 
            {
            LocalEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (!LocalEvent) 
                {
                DWORD dwErr = GetLastError();
                DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Call_In %#x :: event creation failure %d\n", pUserCall, dwErr));
                pUserCall->iResult = dwErr;
                return dwErr;
                }
            pUserCall->hEvent = LocalEvent;
            }

        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Call_In %#x type %#x\n", pUserCall, pUserCall->dwUserCallType));

        switch (pUserCall->dwUserCallType)
            {
            case UCT_DISCOVER:
                //must be a sync call
                Ret = ERROR_BAD_CONFIGURATION;
                if(NULL == LocalEvent)
                    {
                    Ret = ERROR_INVALID_PARAMETER;
                    }
                else
                    {
                    f_pLayerState->Lock();
                    DWORD NumDevices = GetBDAddrList(f_pLayerState->pBdAdresses, MAX_SUBKEY_NUM);
                    f_pLayerState->Unlock();
                    
                    for(UINT i=0; i< NumDevices; i++)
                        {
                        Ret = a2dp_Discover(pUserCall, f_pLayerState->pBdAdresses[i]);
                        if (ERROR_SUCCESS == Ret) 
                            {
                            
                            WaitForSingleObject(LocalEvent, INFINITE);
                            Ret = pUserCall->iResult;
                            if(ERROR_SUCCESS == Ret)
                                {
                                //discover succeded, don't look at anymore reg settings
                                break;
                                }
                                
                            }
                        }
                    }

                break;


            case UCT_GET_CAPABILITIES:
                {
                Ret = ERROR_BAD_CONFIGURATION;
                //must be a sync call
                if(NULL == LocalEvent)
                    {
                    Ret = ERROR_INVALID_PARAMETER;
                    }
                while(1)
                    {
                    Ret = a2dp_GetCapabilites(pUserCall);
                    if (ERROR_SUCCESS == Ret) 
                        {
                        
                        WaitForSingleObject(LocalEvent, INFINITE);
                        Ret = pUserCall->iResult;
                        if(ERROR_SUCCESS == Ret)
                            {
                            //get caps succeded, don't look at anymore end points
                            break;
                            }
                        else if (ERROR_BAD_CONFIGURATION == Ret)
                            {
                            //get caps failed, but still more end points to look at
                            continue;
                            }
                        else
                            {
                            //all stream end points have failed
                            //or had some other bad failure
                            break;
                            }
                            
                            
                        }
                    }

                break;
                }
            case UCT_CONFIGURE:             Ret = a2dp_SetConfiguration (pUserCall); break;
            case UCT_OPEN:                  Ret = a2dp_Open             (pUserCall); break;
            case UCT_START:                 Ret = a2dp_Start            (pUserCall); break;
            case UCT_SUSPEND:               Ret = a2dp_Suspend          (pUserCall); break;
            case UCT_CLOSE:                 Ret = a2dp_Close            (pUserCall); break;
            case UCT_ABORT:                 Ret = a2dp_Abort            (pUserCall); break;

            default:
                Ret = ERROR_CALL_NOT_IMPLEMENTED;
                break;
            }

        // if user wanted a synchronous call...
        if (LocalEvent) 
            {
            if (Ret == ERROR_SUCCESS && 
            UCT_DISCOVER != pUserCall->dwUserCallType &&
            UCT_GET_CAPABILITIES!= pUserCall->dwUserCallType) 
                {
                WaitForSingleObject(LocalEvent, INFINITE);
                Ret = pUserCall->iResult;
                }    
            else 
                {
                pUserCall->iResult = Ret;
                }
            CloseHandle(LocalEvent);
            pUserCall->hEvent = NULL;
            }
        }
        
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Call_In %#x returning %d\n", pUserCall, Ret));

    return Ret;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Discover()

Description:    Discover end points on remote devices

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static
int 
a2dp_Discover (
    A2DP_USER_CALL* pUserCall,
    BD_ADDR DiscoverBdAddr
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_DISCOVER);

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
    if(!f_pLayerState->m_CurrentStream.IsClosed())
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: Current Stream is already configured\n", pUserCall));
        if(pUserCall->hEvent)
            {
            SetEvent(pUserCall->hEvent);
            }
        f_pLayerState->Unlock ();
        return ERROR_SUCCESS;
        }

    f_pLayerState->ReadRegistrySettingsAndSetConfigurationValues();

    f_pLayerState->SetRemoteBdAddress(&DiscoverBdAddr);
    // create the Call_t
    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_DISCOVER, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));

    AVDTP_DiscoverReq_In pFunction = f_pLayerState->avdtp_if.avdtp_DiscoverReq_In;
    HANDLE AVDTP = f_pLayerState->AVDTP;


    
    BD_ADDR BdAddr = f_pLayerState->GetRemoteBdAddress();


    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    int Res = ERROR_INTERNAL_ERROR;

    __try 
        {
        Res = pFunction (AVDTP, pCall, &BdAddr);
        }
    __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Discover :: exception in avdtp_DiscoverReq_In\n"));
        }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: avdtp_DiscoverReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_DISCOVER)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        }
    else
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Discover pUserCall [%#x] :: call 0x%08x : call result [%d]\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_GetCapabilites()

Description:    Get the remote stream capabilities

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static 
int 
a2dp_GetCapabilites(
    A2DP_USER_CALL * pUserCall
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_GET_CAPABILITIES);
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
    if(!f_pLayerState->m_CurrentStream.IsClosed())
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_GetCapabilites pUserCall [%#x] :: Current Stream is already configured\n", pUserCall));
        if(pUserCall->hEvent)
            {
            SetEvent(pUserCall->hEvent);
            }
        f_pLayerState->Unlock ();
        return ERROR_SUCCESS;
        }
        
    // create the Call
    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_GET_CAPABILITIES, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_GetCapabilities pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }


    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_GetCapabilitiesReq_In pFunction = f_pLayerState->avdtp_if.avdtp_GetCapabilitiesReq_In;

    HANDLE AVDTP =   f_pLayerState->AVDTP;
    BD_ADDR BdAddr = f_pLayerState->GetRemoteBdAddress();
    BYTE AcpSEID =   f_pLayerState->RemoteBTEndPoint.CurrentSEInfo.GetSEID();
    

    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                          pCall, 
                          &BdAddr, 
                          AcpSEID
                          );
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_GetCapabilities :: exception in avdtp_GetCapabilitiesReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities pUserCall [%#x] :: avdtp_GetCapabilitiesReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_GET_CAPABILITIES)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_GetCapabilities pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_SetConfiguration()

Description:    Configure the remote stream appropriately

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/ 
static 
int 
a2dp_SetConfiguration(
    A2DP_USER_CALL * pUserCall
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_CONFIGURE);

    if (! LockIfConnected()) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_SetConfiguration pUserCall [%#x] :: ERROR_SERVICE_DOES_NOT_EXIST\n", pUserCall));
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    if(!f_pLayerState->m_CurrentStream.IsClosed())
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_SetConfiguration pUserCall [%#x] :: Current Stream is already configured\n", pUserCall));
        if(pUserCall->hEvent)
            {
            SetEvent(pUserCall->hEvent);
            }
        f_pLayerState->Unlock ();
        return ERROR_SUCCESS;
        }
        
    // create the Call_t
    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_CONFIGURE, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_SetConfiguration pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }


    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_SetConfiguration pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_SetConfigurationReq_In pFunction = f_pLayerState->avdtp_if.avdtp_SetConfigurationReq_In;

    HANDLE AVDTP =   f_pLayerState->AVDTP;
    BD_ADDR BdAddr = f_pLayerState->RemoteBTEndPoint.BdAddress;
    BYTE AcpSEID =   f_pLayerState->GetCurrentRemoteSEID();
    BYTE IntSEID =   f_pLayerState->m_CurrentLocalStreamEPInfo.GetSEID(); 
    AVDTP_CAPABILITY_IE pLocalCapabilityIE[StreamEndPoint_t::MAX_NUM_CAPABILITIES];
    DWORD cLocalCapabilityIE = StreamEndPoint_t::MAX_NUM_CAPABILITIES;
    f_pLayerState->m_CurrentLocalStreamEPInfo.GetCapabilitiesIEArrayCopy(pLocalCapabilityIE, cLocalCapabilityIE); 

    


    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                          pCall, 
                          &BdAddr, 
                          AcpSEID, 
                          IntSEID, 
                          pLocalCapabilityIE, 
                          cLocalCapabilityIE);
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP :: a2dp_SetConfiguration :: exception in avdtp_SetConfigurationReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_SetConfiguration pUserCall [%#x] :: avdtp_SetConfigurationReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_CONFIGURE)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP :: a2dp_SetConfiguration pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        //assume this will work, and tentatively set the remote layer caps
        f_pLayerState->SetRemoteEPCapabilities(pLocalCapabilityIE, cLocalCapabilityIE);

        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_SetConfiguration pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Open()

Description:    Open the current stream in a2dp, 

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/ 
static 
int 
a2dp_Open(
    A2DP_USER_CALL * pUserCall
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_OPEN);

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
        
    if(!f_pLayerState->m_CurrentStream.HasBeenConfigured())
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: Current Stream is not configured\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    else
        {
        if(f_pLayerState->m_CurrentStream.IsOpened())
            {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: Current Stream is opened already\n", pUserCall));
            if(pUserCall->hEvent)
                {
                SetEvent(pUserCall->hEvent);
                }
            f_pLayerState->Unlock ();
            return ERROR_SUCCESS;
            }
        if(f_pLayerState->m_CurrentStream.IsStreaming())
            {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: Current Stream is streaming already\n", pUserCall));
            if(pUserCall->hEvent)
                {
                SetEvent(pUserCall->hEvent);
                }
            f_pLayerState->Unlock ();
            return ERROR_SUCCESS;
            }
        }

    
    // create the Call_t
    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_OPEN, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }
     


    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_OpenReq_In pFunction = f_pLayerState->avdtp_if.avdtp_OpenReq_In;

    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->m_CurrentStream.GetStreamHandle();


    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                         pCall, 
                         StreamHandle);
                         
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Open :: exception in avdtp_OpenReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: avdtp_OpenReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_OPEN)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Open pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Start()

Description:    Start streaming in the current stream in a2dp, 
                going to streaming state

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/ 
static 
int 
a2dp_Start(
    A2DP_USER_CALL * pUserCall
    )
{

    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_START);
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
        
    int Res = ERROR_SERVICE_NOT_ACTIVE;

    // We don't want to send a Start as ACP right away since some
    // headphones will expect to send us the Start first as they
    // are the INT (e.g. HP headphones).
    //
    // (TODO): In the future we could make this an event such that
    // we would only block the full amount of time in cases where
    // we do not get the start.
    if (f_pLayerState->fHaveOpenIndTime) 
        {
        DWORD dwCurrTime = GetTickCount();
        DWORD dwDiffTime = dwCurrTime - f_pLayerState->dwOpenIndTime;        
        
        if (dwDiffTime < START_AFTER_OPEN_IND_DELAY) 
            {
            f_pLayerState->Unlock();    
            Sleep(START_AFTER_OPEN_IND_DELAY - dwDiffTime);            
            if (! LockIfConnected()) 
                {
                return ERROR_SERVICE_DOES_NOT_EXIST;
                }                
            }

            f_pLayerState->fHaveOpenIndTime = FALSE;
            f_pLayerState->dwOpenIndTime = 0;                
        }

    //check for current state
    if(!f_pLayerState->m_CurrentStream.IsOpened())
        {
        if(f_pLayerState->m_CurrentStream.IsStreaming())
            {
            //already streaming, so don't do anything
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: Current Stream is already streaming, do nothing\n", pUserCall));
            if(pUserCall->hEvent)
                {
                SetEvent(pUserCall->hEvent);
                }
            Res = ERROR_SUCCESS;
            }
        else
            {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: Current Stream is not opened\n", pUserCall));
            Res = ERROR_SERVICE_NOT_ACTIVE;
            }

        f_pLayerState->Unlock ();  
        return Res;
        }
     
    // create the Call_t
    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_START, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_StartReq_In pFunction = f_pLayerState->avdtp_if.avdtp_StartReq_In;

    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->GetCurrentStreamHandle();

    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                         pCall, 
                         &StreamHandle,
                         1);
                         
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Start :: exception in avdtp_StartReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: avdtp_StartReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_START)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }
    else 
        {

        //set up flow control
        BT_LAYER_IO_CONTROL pIoctl = f_pLayerState->avdtp_if.avdtp_ioctl;

        AVDTP_SET_BIT_RATE call;
        call.dwSize = sizeof(call);
        f_pLayerState->m_Codec.GetBitRate(call.dwBitRate);
        call.hStream = StreamHandle;
        f_pLayerState->m_Codec.GetHeaderSize(call.cbCodecHeader);
        f_pLayerState->AddRef();
        f_pLayerState->Unlock();

        __try {
            Res = pIoctl(AVDTP, BTH_AVDTP_IOCTL_SET_BIT_RATE, sizeof(call), (char *)&call, 0, NULL, NULL);
        } __except (1) {
            DEBUGMSG(DEBUG_ERROR, ( L"Exception in avdtp_ioctl\n"));
        }

        f_pLayerState->Lock  ();
        f_pLayerState->DelRef ();

        if (ERROR_SUCCESS != Res) 
            {
            SVSUTIL_ASSERT(0);
            f_pLayerState->ResetToPreConfigurationValues();
            }
            
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            f_pLayerState->ResetToPreConfigurationValues();
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Start pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Suspend()

Description:    Suspend current stream in a2dp, 
                returning to configured state

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/  
static 
int 
a2dp_Suspend(
    A2DP_USER_CALL * pUserCall
    )
{

    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_SUSPEND);

    //check if in correct state
    //if not, return appropriate error and do not initiate call
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    if(!f_pLayerState->m_CurrentStream.IsStreaming())
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: Current Stream is not streaming, can't suspend\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
        }
     

    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_SUSPEND, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }
     


    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_SuspendReq_In pFunction = f_pLayerState->avdtp_if.avdtp_SuspendReq_In;
    
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->m_CurrentStream.GetStreamHandle();

    //suspended state needs to be set before the 
    //suspend handshake is done to avoid sending data to the 
    //sink after starting the suspend handshake and in case
    //want to re-start stream (a2dp_Start), which only
    //sends command to sink if already in suspended state
    f_pLayerState->m_CurrentStream.Suspended();

    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                         pCall, 
                         &StreamHandle,
                         1);
                         
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Suspend :: exception in avdtp_SuspendReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: avdtp_SuspendReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_SUSPEND)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Suspend pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Close()

Description:    Close current stream in a2dp, 
                returning to idle state and no configuration
                Is used when the driver receives a close

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/   
static 
int 
a2dp_Close(
    A2DP_USER_CALL * pUserCall
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_CLOSE);

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }
     

    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_CLOSE, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Close pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));
    AVDTP_CloseReq_In pFunction = f_pLayerState->avdtp_if.avdtp_CloseReq_In;
    
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->GetCurrentStreamHandle();
    

    f_pLayerState->ResetToPreConfigurationValues();

    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    
    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                         pCall, 
                         StreamHandle
                         );
                         
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Close :: exception in avdtp_CloseReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close pUserCall [%#x] :: avdtp_CloseReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_CLOSE)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            //f_pLayerState->ResetToPreConfigurationValues();
            }
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            //f_pLayerState->ResetToPreConfigurationValues();
            }
        }
    //even if close fails, want to reset local state
    //f_pLayerState->ResetToPreConfigurationValues();
    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Close pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
}


#if 0
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_InternalAbort()

Description:    Aborts all the current connections in a2dp, 
                returning to idle state and no configuration
                Should be used when internal failures happen
                in the a2dp layer

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static 
int 
a2dp_InternalAbort(
    void
    )
{
    // BUGBUG: need more work on internal driver state!
    A2DP_USER_CALL InternalAbort;
    memset(&InternalAbort, 0, sizeof(InternalAbort));
    InternalAbort.dwUserCallType = UCT_ABORT;
    int Err = a2dp_Call_In(&InternalAbort);
    if (Err) {
        DEBUGMSG(ZONE_A2DP_ERROR,(TEXT("a2dp_InternalAbort: Call UCT_ABORT failed [0x%8x]"), Err));
        }
    return Err;
}
#endif
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Abort()

Description:    Aborts all the current connections in a2dp, 
                returning to idle state and no configuration
                Should be used when internal failures happen
                on the driver

Arguments:      A2DP_USER_CALL * pUserCall
                    User call context, used to return the 
                    callbacks to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static 
int 
a2dp_Abort(
    A2DP_USER_CALL * pUserCall
    )
{

    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_ABORT);

    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }


    // Check if in configured state, since can't abort a AVDTP stream without a 
    // stream handle

    if(!f_pLayerState->m_CurrentStream.HasBeenConfigured())
        {
        //just reset internal state
        f_pLayerState->ResetToPreConfigurationValues();

        //since return success and never calling avdtp (never
        //getting a call back), signal user call as done
        if(pUserCall->hEvent)
            {
            SetEvent(pUserCall->hEvent);
            }
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: ERROR_SUCCESS, no call to avdtp since no stream configured\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_SUCCESS;
        }

    // Create a2dp layer call, since making avdtp call

    Call_t *pCall = CallCreateNew (A2DP_CALL_TYPE_ABORT, pUserCall);
    if (!pCall) 
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: ERROR_OUTOFMEMORY\n", pUserCall));
        f_pLayerState->Unlock ();
        return ERROR_OUTOFMEMORY;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: created call [0x%08x]\n", pUserCall, pCall));


    // Get variables to make call before unlocking
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->GetCurrentStreamHandle();
    AVDTP_AbortReq_In pFunction = f_pLayerState->avdtp_if.avdtp_AbortReq_In;

                
    // Make avdtp call
    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();
    int Res = ERROR_INTERNAL_ERROR;
    __try {
        Res = pFunction (AVDTP, 
                         pCall, 
                         StreamHandle
                         );
                         
    } __except (1) {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Abort :: exception in avdtp_AbortReq_In\n"));
    }

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    // Check if abort call succeeded
    if (ERROR_SUCCESS != Res) 
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: avdtp_AbortReq_In failed [%d]\n", pUserCall, Res));
        if (CallVerify(pCall, A2DP_CALL_TYPE_ABORT)) 
            {
            CallRemove(pCall);
            CallSignal(pCall, Res);
            }
        // if abort is failing, something has gone very wrong, 
        // so clear state anyway
        f_pLayerState->ResetToPreConfigurationValues();
        }
    else 
        {
        if(! f_pLayerState->IsStackConnected())
            {
            DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: ERROR_SHUTDOWN_IN_PROGRESS\n", pUserCall));
            Res = ERROR_SHUTDOWN_IN_PROGRESS;
            // if stack is down, need to clear state
            f_pLayerState->ResetToPreConfigurationValues();
            }
        }

    f_pLayerState->Unlock ();
    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Abort pUserCall [%#x] :: call [0x%08x] : call result %d\n", pUserCall, pCall, Res));
    return Res;
    
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       a2dp_Write()

Description:    Writes buffer to the avdtp layer. 
                Unlike most a2dp function calls, this is a sync 
                call, so this function is responsible for 
                signaling done to the callee once the call into
                avdtp is done if returning success


Arguments:      A2DP_USER_CALL * pUserCall
                     User call context, used to signal write
                     completion to the user
                    
Returns:        int indicating status
-------------------------------------------------------------------*/
static 
int 
a2dp_Write (
    A2DP_USER_CALL* pUserCall
    )
{
    SVSUTIL_ASSERT(pUserCall);
    SVSUTIL_ASSERT(pUserCall->dwUserCallType == UCT_WRITE);
    int Res = ERROR_SUCCESS;

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write\n"));
    if (! LockIfConnected()) 
        {
        return ERROR_SERVICE_DOES_NOT_EXIST;
        }

    if(!f_pLayerState->m_CurrentStream.IsStreaming())
        {
        if(f_pLayerState->m_CurrentStream.IsOpened())
            {//suspended, just return with particular error value
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Write pUserCall [%#x] :: Current Stream is not streaming, but is opened, returning\n", pUserCall));
            f_pLayerState->Unlock ();
            return ERROR_NOT_READY;
            }
        else
            {
            DEBUGMSG(DEBUG_ERROR, ( L"A2DP_t :: a2dp_Write pUserCall [%#x] :: Current Stream is not streaming\n", pUserCall));
            f_pLayerState->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
            }
        }
        
    AVDTP_WriteReq_In pFunction = f_pLayerState->avdtp_if.avdtp_WriteReq_In;
    
    HANDLE AVDTP = f_pLayerState->AVDTP;
    HANDLE StreamHandle = f_pLayerState->GetCurrentStreamHandle();
    

    DWORD CodecMetaDataByteCount = 0;
    DWORD OuputDataBufferByteCount = 0;
    DWORD InputBufferByteCount = pUserCall->uParameters.Write.DataBufferByteCount;

    uint BufferSize;
    
    HRESULT IsSumValid = E_FAIL;
    if(f_pLayerState->cAvdtpHeaders >= 0 &&
       f_pLayerState->cAvdtpTrailers >= 0)
        {

        IsSumValid = UIntAdd(f_pLayerState->cAvdtpHeaders, 
                             f_pLayerState->cAvdtpTrailers, 
                             &BufferSize);
        }

    if(S_OK != IsSumValid)
        {
        DEBUGMSG(ZONE_A2DP_TRACE, 
                ( L"A2DP_t :: a2dp_Write :: Error in sum for buffer size, IsSumValid = [%d], BufferSize = [%d]", 
                IsSumValid, BufferSize));
                
        f_pLayerState->Unlock();
        return ERROR_INVALID_PARAMETER;
        }

    BD_BUFFER* pBuffer = f_pLayerState->m_Codec.AllocateOutputBDBuffer(
                                                    InputBufferByteCount, 
                                                     BufferSize,
                                                     OuputDataBufferByteCount,
                                                     CodecMetaDataByteCount
                                                     );
    if (! pBuffer) {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write :: Error allocating buffer"));
        f_pLayerState->Unlock();
        return ERROR_OUTOFMEMORY;
    }

    pBuffer->fMustCopy = FALSE; 
    pBuffer->cStart = f_pLayerState->cAvdtpHeaders;
    PBYTE InputBuffer = pUserCall->uParameters.Write.pDataBuffer;

    
    Res = f_pLayerState->m_Codec.EncodeBuffer(
                                    InputBuffer, 
                                    InputBufferByteCount, 
                                    pBuffer->pBuffer + pBuffer->cStart, 
                                    OuputDataBufferByteCount
                                    );

    if(Res != ERROR_SUCCESS)
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write :: Error encoding buffer [0x%08x]\r\n", Res));
        f_pLayerState->Unlock();
        f_pLayerState->m_Codec.FreeBDBuffer(pBuffer);
        return Res;
        }
    Res = f_pLayerState->m_Codec.WriteHeader(
                                    pBuffer->pBuffer + pBuffer->cStart, 
                                    OuputDataBufferByteCount
                                    );        
    if(Res != ERROR_SUCCESS)
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write :: Error writting header [0x%08x]\r\n", Res));
        f_pLayerState->Unlock();
        f_pLayerState->m_Codec.FreeBDBuffer(pBuffer);
        return Res;
        }
        
    f_pLayerState->AddRef ();
    f_pLayerState->Unlock ();

    Res = pFunction (AVDTP, 
                     StreamHandle,
                     pBuffer,
                     GetTickCount(),
                     0x01,
                     0);
                         

    f_pLayerState->Lock  ();
    f_pLayerState->DelRef ();

    if(! f_pLayerState->IsStackConnected())
        {
        DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write :: ERROR_SHUTDOWN_IN_PROGRESS\n"));
        f_pLayerState->Unlock();
        return ERROR_SHUTDOWN_IN_PROGRESS;
        }

    DEBUGMSG(ZONE_A2DP_TRACE, ( L"A2DP_t :: a2dp_Write :: [0x%08x]\n", Res));

    f_pLayerState->Unlock ();
    return Res;

}


