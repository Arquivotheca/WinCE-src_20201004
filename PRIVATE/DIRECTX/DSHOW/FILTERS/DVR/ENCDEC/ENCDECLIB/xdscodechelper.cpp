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


    Module Name:

        XDSCodecHelper.cpp

    Abstract:

        This module contains an XDSCodecHelper class derived from an
		XDSCodec filter implementation (part of the Encrypter/Tagger filter code).


--*/

// Suppress warning about long symbol names being truncated to 255 characters.
// This warning is disabled by default on VS.NET but enabled in our CE build.
#pragma warning(disable : 4786)

#include <DShow.h>
#include <Streams.h>
#include <tchar.h>
#include <Dvdmedia.h>
#include <EncDec.h>
#include <atlbase.h>

#include "XDSCodecHelper.h"
#ifndef UNDER_CE
// #include <bdaiface.h>
#endif // !UNDER_CE

#include "PackTvRat.h"

/* Defining VBI Data control codes for closed captioning and text data*/

enum VBI_Control_Packet_Non_XDS_Classes
{
    VBI_Non_XDS_Cls_Lower_Range                 = 0x10,
    VBI_Non_XDS_Cls_Upper_Range                 = 0x1F
};

//  ============================================================================

//  ============================================================================

CXDSCodecHelper::CXDSCodecHelper (IXDSCodecCallbacks *piXDSCodecCallbacks)
:
		 m_piXDSCodecCallbacks			(piXDSCodecCallbacks),
         m_cTvRatPktSeq                 (0),
         m_cTvRatCallSeq                (0),
         m_cXDSPktSeq                   (0),
         m_cXDSCallSeq                  (0),
         m_dwSubStreamMask              (KS_CC_SUBSTREAM_SERVICE_XDS),
         m_TvRating                     (0),
         m_XDSClassPkt                  (0),
         m_XDSTypePkt                   (0),
         m_hrXDSToRatCoCreateRetValue   (CLASS_E_CLASSNOTAVAILABLE),
         m_fJustDiscontinuous           (false),
         m_TimeStartRatPkt              (0),
         m_TimeEndRatPkt                (0),
         m_TimeStartXDSPkt              (0),
         m_TimeEndXDSPkt                (0),
         m_TimeStart_LastPkt            (0),
         m_TimeEnd_LastPkt              (0),
         m_fIsXDSParsing				(FALSE),
		 m_fXDSParsingPaused			(FALSE),
         m_cXDSLoc						(0),
         m_crcXDS                       (0),
		 m_cCritSec                     (),
		 m_eFilterState                 (State_Stopped),
		 m_pPinCCDecoder                (0),
		 m_piReferenceClock             (0),
		 m_cRestarts					(0)
{
	DbgLog((LOG_MEMORY, 2, _T("CXDSCodec(%p) constructor for %p\n"), piXDSCodecCallbacks, this));

    InitStats();

                // attempt to create the 3rd party ratings parser

#ifndef UNDER_CE
    try {
        m_hrXDSToRatCoCreateRetValue =
            CoCreateInstance(CLSID_XDSToRat,        // CLSID
                             NULL,                  // pUnkOut
                             CLSCTX_INPROC_SERVER,
                             IID_IXDSToRat,     // riid
                             (LPVOID *) &m_spXDSToRat);

    } catch (HRESULT hr) {
        m_hrXDSToRatCoCreateRetValue = hr;
    }
#else // !UNDER_CE
    m_hrXDSToRatCoCreateRetValue = CLASS_E_CLASSNOTAVAILABLE;
#endif // !UNDER_CE

    DbgLog((LOG_MEMORY, 2, _T("CXDSCodecHelper::CoCreate XDSToRat object - hr = 0x%08x"),m_hrXDSToRatCoCreateRetValue));

    return ;
}

CXDSCodecHelper::~CXDSCodecHelper ( )
{
	DbgLog((LOG_MEMORY, 2, _T("CXDSCodecHelper: destroying %p\n"), this));
}

void CXDSCodecHelper::OnPause ()
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnPause()\n"), this));

    CAutoLock  cLock(&m_cCritSec);          // grab the  lock...

    int start_state = m_eFilterState;
	m_eFilterState = State_Paused;

    if (start_state == State_Stopped) {
		DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnPause() -- prior state was Stop\n"), this));

      InitStats();
      m_fJustDiscontinuous = false;

    } else {
		DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnPause() -- prior state was Run\n"), this));

        DbgLog((LOG_TRACE, 4, _T("CXDSCodecHelper:: Stats: %d XDS samples %d Ratings, %d Parse Failures\n"),
            m_cPackets, m_cRatingsDetected, m_cRatingsFailures));

        DbgLog((LOG_TRACE, 4, _T("                   %d Changed %d Duplicate Ratings, %d Unrated, %d Data Pulls\n"),
            m_cRatingsChanged, m_cRatingsDuplicate, m_cUnratedChanged, m_cRatingsGets));

    }

    m_TimeStart_LastPkt = 0;        // just for safety, reinit these
    m_TimeEnd_LastPkt = 0;          //  (someone will probably find this isn't right)
}


void CXDSCodecHelper::OnStop()
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnStop()\n"), this));

	m_eFilterState = State_Stopped;
}

void CXDSCodecHelper::OnRun(REFERENCE_TIME rtStart)
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnRun(%I64d)\n"), this, rtStart));

	m_eFilterState = State_Running;
	m_rtStart = rtStart;
}

void CXDSCodecHelper::OnConnect(IPin *piVBIInputPin)
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnConnect(%p)\n"), this, piVBIInputPin));

	ASSERT(m_pPinCCDecoder == NULL);
	m_pPinCCDecoder = NULL;
	if (piVBIInputPin)
	{
		HRESULT hr = piVBIInputPin->ConnectedTo(&m_pPinCCDecoder);
		ASSERT(SUCCEEDED(hr));  // if this fails, we simply won't be able to change substreams.
	}
}

void CXDSCodecHelper::OnDisconnect()
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::OnDisconnect()\n"), this));

	m_pPinCCDecoder = NULL;
}

void CXDSCodecHelper::CacheClock(IReferenceClock *piReferenceClock)
{
	DbgLog((LOG_TRACE, 3, _T("CXDSCodecHelper(%p)::CacheClock(%p)\n"), this, piReferenceClock));

	m_piReferenceClock = piReferenceClock;
}

HRESULT
CXDSCodecHelper::Process (
                    IN  IMediaSample *  pIMediaSample
                    )
{
    HRESULT hr = S_OK;

    if(pIMediaSample == NULL)
        return E_POINTER;

    pIMediaSample->GetTime(&m_TimeStart_LastPkt, &m_TimeEnd_LastPkt);

    if(S_OK == pIMediaSample->IsDiscontinuity())
    {
        if(!m_fJustDiscontinuous)    // latch value to reduce effects of sequential Discontinuity samples
        {                            // (they occur on every sample when VBI even field doesn't contain CC data)

            DbgLog((LOG_TRACE,  3, _T("CXDSCodecHelper::Process - Discontinuity\n")));

            ResetToDontKnow(pIMediaSample);     // reset and kick off event
            m_fJustDiscontinuous = true;
        }
        return S_OK;
    } else {
        m_fJustDiscontinuous = false;
    }

    if(pIMediaSample->GetActualDataLength() == 0)       // no data
        return S_OK;

    // We expect 2 bytes of VBI data but it may be conveyed in a
    // structure that tacks on an additional 2 bytes after the VBI
    // data:

	size_t ccbMediaSample = pIMediaSample->GetActualDataLength();
    if((ccbMediaSample != 2) && (ccbMediaSample != 4))
    {
        DbgLog((LOG_ERROR,  2,
            _T("CXDSCodecHelper:: Unexpected Length of CC data (%d != 2 or 4 bytes\n)"),
            pIMediaSample->GetActualDataLength() ));
        return E_UNEXPECTED;
    }

    PBYTE pbData;
    hr = pIMediaSample->GetPointer(&pbData);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR,  3,   _T("CXDSCodecHelper:: Empty Buffer for CC data, hr = 0x%08x\n"),hr));
        return hr;
    }
	
//	if (ccbMediaSample == 4)
//		pbData += 2;  // the first two bytes are descriptive.
//  We always get the 4 bytes of data, this needs to be cast into the ksmdedia.h struct later

    BYTE byte0 = pbData[0];
    BYTE byte1 = pbData[1];
    DWORD dwType = 0;                   // todo - default to some useful value

    // todo - parse the data
    //        then send messages whe we get something interesting

#if xxxDEBUG
    static int cCalls = 0;
    TCHAR szBuff[256];
    _stprintf(szBuff, _T("0x%08x 0x%02x 0x%02x (%c %c)\n"),
        cCalls++, byte0&0x7f, byte1&0x7f,
        isprint(byte0&0x7f) ? byte0&0x7f : '?',
        isprint(byte1&0x7f) ? byte1&0x7f : '?' );
    OutputDebugString(szBuff);
#endif
    // strip off the high bit...
    ParseXDSBytePair(pIMediaSample, byte0, byte1);

    return hr ;
}

// ------------------------------------

#ifndef UNDER_CE
typedef struct
{
    KSPROPERTY                          m_ksThingy;
    VBICODECFILTERING_CC_SUBSTREAMS     ccSubStreamMask;
} KSPROPERTY_VBICODECFILTERING_CC_SUBSTREAMS;
#endif // !UNDER_CE

// dwFiltType is bitfield made up of:
//  KS_CC_SUBSTREAM_SERVICE_CC1, _CC2,_CC3, _CC4,  _T1, _T2, _T3 _T4 And/Or _XDS;

HRESULT
CXDSCodecHelper::SetSubstreamChannel(DWORD dwSubStreamChannels)
{
    DbgLog((LOG_TRACE,  3, _T("CXDSCodecHelper::SetSubstreamChannel\n")));
    HRESULT hr;

    // -- wonder if I should do this here:  CAutoLock  cLock(m_cCritSec);

    if(0 != (dwSubStreamChannels &
              ~(KS_CC_SUBSTREAM_SERVICE_CC1 |
                KS_CC_SUBSTREAM_SERVICE_CC2 |
                KS_CC_SUBSTREAM_SERVICE_CC3 |
                KS_CC_SUBSTREAM_SERVICE_CC4 |
                KS_CC_SUBSTREAM_SERVICE_T1 |
                KS_CC_SUBSTREAM_SERVICE_T2 |
                KS_CC_SUBSTREAM_SERVICE_T3 |
                KS_CC_SUBSTREAM_SERVICE_T4 |
                KS_CC_SUBSTREAM_SERVICE_XDS)))
        return E_INVALIDARG;

#ifndef UNDER_CE
    try {

        if(NULL == m_pPinCCDecoder.p)
            return S_FALSE;

        PIN_INFO pinInfo;
        hr = m_pPinCCDecoder->QueryPinInfo(&pinInfo);
        if (SUCCEEDED(hr)) {

            IBaseFilter *pFilt = pinInfo.pFilter;

            // Triggers are just on the T2 stream of closed captioning.
            //  Tell the nice CC filter to only give us that stream out of the 9 possible


            IKsPropertySet *pksPSet = NULL;

            HRESULT hr2 = m_pPinCCDecoder->QueryInterface(IID_IKsPropertySet, (void **) &pksPSet);
            if(!FAILED(hr2))
            {
                DWORD rgdwData[20];
                DWORD cbMax = sizeof(rgdwData);
                DWORD cbData;
                hr2 = pksPSet->Get(KSPROPSETID_VBICodecFiltering,
                                    KSPROPERTY_VBICODECFILTERING_SUBSTREAMS_DISCOVERED_BIT_ARRAY,
                                    NULL, 0,
                                    (BYTE *) rgdwData, cbMax, &cbData);
                if(FAILED(hr2))
                {
                    DbgLog((LOG_ERROR,  2,
                            _T("CXDSCodecHelper::SetSubstreamChannel  Error Getting CC Filtering, hr = 0x%08x\n"),hr2));
                    return hr2;
                }


                DbgLog((LOG_TRACE, 3,
                          _T("CXDSCodecHelper::SetSubstreamChannel  Setting CC Filtering to 0x%04x\n"),dwSubStreamChannels ));


                KSPROPERTY_VBICODECFILTERING_CC_SUBSTREAMS ksThing = {0};
                ksThing.ccSubStreamMask.SubstreamMask = dwSubStreamChannels;
                                                                        // ring3 to ring0 propset call
                hr2 = pksPSet->Set(KSPROPSETID_VBICodecFiltering,
                                     KSPROPERTY_VBICODECFILTERING_SUBSTREAMS_REQUESTED_BIT_ARRAY,
                                     &ksThing.ccSubStreamMask,
                                     sizeof(ksThing) - sizeof(KSPROPERTY),
                                     &ksThing,
                                     sizeof(ksThing));

                if(FAILED(hr2))
                {
                    DbgLog((LOG_ERROR,  2,
                            _T("CXDSCodecHelper:: Error Setting CC Filtering, hr = 0x%08x\n"),hr2));
                }

            } else {
                DbgLog((LOG_ERROR,  2,
                        _T("CXDSCodecHelper:: Error Getting CC's IKsPropertySet, hr = 0x%08x\n"),hr2));

            }
            if(pksPSet)
                pksPSet->Release();

            if(pinInfo.pFilter)
                pinInfo.pFilter->Release();
        }
    } catch (HRESULT hrCatch) {
        DbgLog((LOG_ERROR,  2,
                  _T("CXDSCodecHelper::SetSubstreamChannel Threw Badly - (hr=0x%08x) Giving Up\n"),hrCatch ));
        hr = hrCatch;
    } catch (...) {
        DbgLog((LOG_TRACE,  2,
                  _T("CXDSCodecHelper::SetSubstreamChannel Threw Badly - Giving Up\n") ));

        hr = E_FAIL;
    }
#else // !UNDER_CE
    hr = S_OK;
#endif // !UNDER_CE

    return hr;
}

//  -------------------------------------------------------------------
//  helper methods
//  -------------------------------------------------------------------

	// Saves state and sends events...
    //   assumes calling code is blocking duplicate packets

HRESULT
CXDSCodecHelper::GoNewXDSRatings(IMediaSample * pMediaSample,  PackedTvRating TvRating)
{

#ifdef DEBUG
    {
        //  4) store nice unpacked version of it too
        EnTvRat_System              enSystem;
        EnTvRat_GenericLevel        enLevel;
        LONG                        lbfEnAttrs;

        UnpackTvRating(TvRating, &enSystem, &enLevel, &lbfEnAttrs);

        TCHAR tbuff[128];
        tbuff[0] = 0;
        static int cCalls = 0;

        REFERENCE_TIME tStart=0, tEnd=0;
        if(NULL != pMediaSample)
            pMediaSample->GetTime(&tStart, &tEnd);

        RatingToString(enSystem, enLevel, lbfEnAttrs, tbuff, sizeof(tbuff)/sizeof(tbuff[0]));
        DbgLog((LOG_TRACE, 4,  _T("CXDSCodecHelper::GoNewXDSRatings. Rating %s, Seq (%d), time %d msec\n"),
            tbuff, m_cTvRatPktSeq+1, tStart));

                    // extra doc
//      _tcsncat(tbuff,L"\n",sizeof(tbuff)/sizeof(tbuff[0]) - _tcslen(tbuff));
//      OutputDebugString(tbuff);
    }
#endif

        // copy all the values
   {
        CAutoLock cLock(&m_cCritSec);    // lock these
        m_TvRating = TvRating;

            // increment our sequence counts
        m_cTvRatPktSeq++;
        m_cTvRatCallSeq = 0;

            // save the times (if can't get a media time due to Happauge bug, try our best to approximate it)
        if(NULL == pMediaSample ||
            S_OK != pMediaSample->GetTime(&m_TimeStartRatPkt, &m_TimeEndRatPkt))
        {
            // current time w.r.t. base time (m_rtStart)
            REFERENCE_TIME refTimeNow=0;
            HRESULT hr2 = m_piReferenceClock->GetTime(&refTimeNow);

            if(S_OK == hr2)
            {
                m_TimeStartRatPkt = refTimeNow - m_rtStart;          // fake a time
                m_TimeEndRatPkt   = m_TimeStartRatPkt + 10000;
            }
            else        // couldn't even fake it...
            {
                m_TimeStartRatPkt = m_TimeStart_LastPkt;
                m_TimeEndRatPkt   = m_TimeEnd_LastPkt;
            }
        }
   }

        // signal the broadcast events
    if (m_piXDSCodecCallbacks)
		m_piXDSCodecCallbacks->OnNewXDSRating();
    return S_OK;
}

HRESULT
CXDSCodecHelper::GoDuplicateXDSRatings(IMediaSample * pMediaSample,  PackedTvRating TvRating)
{

#ifdef DEBUG
    {
        //  4) store nice unpacked version of it too
        EnTvRat_System              enSystem;
        EnTvRat_GenericLevel        enLevel;
        LONG                        lbfEnAttrs;

        UnpackTvRating(TvRating, &enSystem, &enLevel, &lbfEnAttrs);

        TCHAR tbuff[128];
        tbuff[0] = 0;
        static int cCalls = 0;

        REFERENCE_TIME tStart=0, tEnd=0;
        if(NULL != pMediaSample)
            pMediaSample->GetTime(&tStart, &tEnd);

        RatingToString(enSystem, enLevel, lbfEnAttrs, tbuff, sizeof(tbuff)/sizeof(tbuff[0]));
        DbgLog((LOG_TRACE, 4,  _T("CXDSCodecHelper::GoDuplicateXDSRatings. Rating %s, Seq (%d), time %d msec\n"),
            tbuff, m_cTvRatPktSeq+1, tStart));

                    // extra doc
//      _tcsncat(tbuff,L"\n",sizeof(tbuff)/sizeof(tbuff[0]) - _tcslen(tbuff));
//      OutputDebugString(tbuff);
    }
#endif

        // copy all the values
   {
        CAutoLock cLock(&m_cCritSec);    // lock these
        ASSERT(m_TvRating == TvRating);      // assert it's really a duplicate

            // increment our sequence counts
//      m_cTvRatPktSeq++;
//      m_cTvRatCallSeq = 0;

        REFERENCE_TIME tStart;

            // save the end time (if can't get a media time due to Happauge bug, try our best to approximate it)
        if(NULL == pMediaSample ||
            S_OK != pMediaSample->GetTime(&tStart, &m_TimeEndRatPkt))
        {
            // current time w.r.t. base time (m_rtStart)
            REFERENCE_TIME refTimeNow=0;
            HRESULT hr2 = m_piReferenceClock->GetTime(&refTimeNow);

            if(S_OK == hr2)
            {
//                m_TimeStartRatPkt = refTimeNow - m_rtStart;          // fake a time
                m_TimeEndRatPkt   = m_TimeStartRatPkt + 10000;
            }
            else        // couldn't even fake it...
            {
//                m_TimeStartRatPkt = m_TimeStart_LastPkt;
                m_TimeEndRatPkt   = m_TimeEnd_LastPkt;
            }
        }
   }

        // signal the broadcast events
    if (m_piXDSCodecCallbacks)
		m_piXDSCodecCallbacks->OnDuplicateXDSRating();
    return S_OK;
}

HRESULT
CXDSCodecHelper::GoNewXDSPacket(IMediaSample * pMediaSample, long pktClass, long pktType, BSTR bstrXDSPkt)
{
        // copy all the values
    {
        CAutoLock cLock(&m_cCritSec);    // lock these
        m_XDSClassPkt = pktClass;
        m_XDSTypePkt = pktType;
        m_spbsXDSPkt = bstrXDSPkt;      // Question, should copy here?

            // increment our sequence counts
        m_cXDSPktSeq++;
        m_cXDSCallSeq = 0;

            // save the times (if can't get a media time due to Happauge bug, try our best to approximate it)
        if(NULL == pMediaSample ||
            S_OK != pMediaSample->GetTime(&m_TimeStartXDSPkt, &m_TimeEndXDSPkt))
        {
            // current time w.r.t. base time (m_rtStart)
            REFERENCE_TIME refTimeNow=0;
            HRESULT hr2 = m_piReferenceClock->GetTime(&refTimeNow);

            if(S_OK == hr2)
            {
                m_TimeStartXDSPkt = refTimeNow - m_rtStart;          // fake a time
                m_TimeEndXDSPkt   = m_TimeStartRatPkt + 10000;
            }
            else        // couldn't even fake it...
            {
                m_TimeStartXDSPkt = m_TimeStart_LastPkt;
                m_TimeEndXDSPkt   = m_TimeEnd_LastPkt;
            }
        }
    }


	DbgLog((LOG_TRACE, 4,  _T("CXDSCodecHelper::GoNewXDSPacket. CP (0x%04x), Seq (%d), time %d msec\n"),
            m_spbsXDSPkt[0],
			m_cXDSPktSeq,
			m_TimeStartXDSPkt/10000));


        // signal the broadcast events
    if (m_piXDSCodecCallbacks)
		m_piXDSCodecCallbacks->OnNewXDSPacket();

    return S_OK;
}

            // doesn't actually parse, calls the XDSToRat object to do the work.
HRESULT
CXDSCodecHelper::ParseXDSBytePair(IMediaSample *  mediaSample,
                            BYTE byte1,
                            BYTE byte2)
{
    HRESULT hr = S_OK;

    m_cPackets++;

        // call the 3rd party parser...
    if(m_spXDSToRat != NULL)
    {
        EnTvRat_System          enSystem;
        EnTvRat_GenericLevel    enLevel;
        LONG                    lbfAttrs; // BfEnTvRat_GenericAttributes

        BYTE byte1M = byte1 & 0x7f; // strip off parity bit (should we check it?)
        BYTE byte2M = byte2 & 0x7f;
        DbgLog((LOG_TRACE, 4,  _T("CXDSCodecHelper::ParseXDSBytePair : 0x%02x 0x%02x (%c %c)\n"),
            byte1M, byte2M,
            isprint(byte1M) ? byte1M : '?',
            isprint(byte2M) ? byte2M : '?'
            ));

									// look for ratings...
//      hr = m_spXDSToRat->ParseXDSBytePair(byte1, byte2, &enSystem, &enLevel, &lbfAttrs );

                                    // goofed, we need to strip the parity for the Parsers.
        hr = m_spXDSToRat->ParseXDSBytePair(byte1M, byte2M, &enSystem, &enLevel, &lbfAttrs );
        if(hr == S_OK)      // S_FALSE means it didn't find a new one
        {
            m_cRatingsDetected++;

            DbgLog((LOG_TRACE, 3,  _T("CXDSCodecHelper::ParseXDSBytePair- Rating (0x%02x 0x%02x) Sys:%d Lvl:%d Attr:0x%08x\n"),
                (DWORD) enSystem, (DWORD) enLevel, lbfAttrs));
            PackedTvRating TvRating;
            hr = PackTvRating(enSystem, enLevel, lbfAttrs, &TvRating);
			if (SUCCEEDED(hr))
			{
	            if(TvRating != m_TvRating)      // do I want this test here or in the Go method... Might as well have here?
		        {
			        m_cRatingsChanged++;
				    GoNewXDSRatings(mediaSample, TvRating);
				}
				else if (enSystem != TvRat_SystemDontKnow)
				{
					m_cRatingsDuplicate++;
					GoDuplicateXDSRatings(mediaSample, TvRating);     // found a duplicate.  Just send a broadcast event...
				}
            }
        }
        else if(hr != S_FALSE)
        {
            m_cRatingsFailures++;
        }

								// look for other stuff
    }

    // Generic XDS parsing code.

    {		
        long pktClass;
        long pktType;
        CComBSTR spbsPktData;

        hr = _ParseXDSBytePair(byte1, byte2, &pktClass, &pktType, &spbsPktData);

        if(S_OK == hr)
            GoNewXDSPacket(mediaSample, pktClass, pktType, spbsPktData);
    }

    return S_OK;
}

const BOOL parityArray[256] = {
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1
};

        // returns TRUE if parity is OK
static BOOL CheckParity(BYTE byte)
{
    BOOL fOK = parityArray[byte&0x7f] == (byte>>7);
    return fOK;
}

HRESULT
CXDSCodecHelper::InitXDSParser()		// call on channel changes to reset the state.
{
    m_fIsXDSParsing = FALSE;
	m_fXDSParsingPaused = FALSE;
    m_cXDSLoc		= 0;

    return S_OK;
}

// doesn't actually parse, calls the XDSToRat object to do the work.
HRESULT
CXDSCodecHelper::_ParseXDSBytePair(BYTE byte1,
                             BYTE byte2,
                             long *pPktClass,
                             long *pPktType,
                             BSTR *pBsPktData)
{
    if(NULL == pPktClass || NULL == pPktType || NULL == pBsPktData)
        return E_POINTER;

    // this code limited to just check the (0x1,0x8) - CGMS-A bit  Assume no continues allowed.

    BYTE byte1M = byte1 & 0x7f; // strip off parity bit (should we check it?)
    BYTE byte2M = byte2 & 0x7f;

    if(byte1 == 0x00 && byte2 == 0x00)      // NULL byte pair, simply return
        return S_FALSE;
	
            // parity error
    if(!CheckParity(byte1) || !CheckParity(byte2))
    {
        m_cBadParity++;
        InitXDSParser();
        return S_FALSE;
    } else {
        m_cGoodParity++;
    }



    //    TRACE_2 (LOG_AREA_XDSCODEC, 2, _T("[0x%02x 0x%02x]"),byte1,byte2);

    if ((byte1M == XDS_Cls_Current_Start) && (byte2M == XDS_1Type_CGMS_A))
    {
        m_fIsXDSParsing = TRUE;
		m_fXDSParsingPaused = FALSE;
        m_cXDSLoc		= 0;

        m_crcXDS = byte1;
        m_crcXDS += byte2;

        return S_FALSE;
    }
	if (m_fXDSParsingPaused && (byte1M == XDS_Cls_Current_Continue) && (byte2M == XDS_1Type_CGMS_A))
	{
		m_fXDSParsingPaused = FALSE;
		m_fIsXDSParsing = TRUE;

		// The continuation request does not get factored into the CRC.
		return S_FALSE;
	}

    if(m_fIsXDSParsing && (byte1M == XDS_Cls_End || m_cXDSLoc > 4))
    {
		// Note:  if byte1M is not XDS_Cls_End, then we don't want this
		// packet. In that case m_cXDSLoc > 4 and so certainly != 2 and
		// so will be rejected as a "bad CRC" below.

        // todo - add code to check CRC here in byte2;
        m_fIsXDSParsing = FALSE;
		m_fXDSParsingPaused = FALSE;

        *pPktClass = XDS_Cls_Current_Start;		//  0x1
        *pPktType  = XDS_1Type_CGMS_A;		    //  0x8 -- note CMGS-A ONLY here...

        m_crcXDS += byte1;
        m_crcXDS += byte2;
        m_crcXDS &= 0x7f;
                            // CRC Test, According to EIA/CEA 608-B, 7 bit value should be zero
                            // also test
                            //  bit5 of byte1 (reserved, should = 0)
                            //  bit6 of byte1,byte2 (binary value, should == 1
        if(m_crcXDS != 0 ||
            (m_rgXDSBuff[0] & (0x1<<5)) != 0 ||
            (m_rgXDSBuff[0] & (0x1<<6)) != (0x1<<6) ||
            (m_rgXDSBuff[1] & (0x1<<6)) != (0x1<<6) ||
            m_cXDSLoc != 2              // avoid non 6-byte packets (e.g. XBOX)
            )
        {
            m_cCGMSA_Bad_Packets++;
            return S_FALSE;         // bad CRC
        }

        m_cCGMSA_Packets++;     // remember, only CGMS_A packets are being parsed ehre

        CComBSTR bsData(2);
		if (!bsData)
		{
			// Yes, CComBSTR does not throw under Macallan
			return S_FALSE;
		}
        bsData[0] = m_rgXDSBuff[0];     // CGMS-A packet is only 2 bytes long (ignoring start and stop byte pairs...)
        bsData[1] = m_rgXDSBuff[1];;

        HRESULT hrCopy = bsData.CopyTo(pBsPktData);
		if (FAILED(hrCopy))
		{
			// Return an error because we're out of memory and so failed to
			// properly set *pBsPktData:
			return hrCopy;
		}
        return S_OK;
    }
    if (m_fIsXDSParsing)
	{
		// This takes care of all interleaving of XDS, non CGMS-A data and non XDS data
		if( (((byte1M >= XDS_Cls_Current_Start) && (byte1M <= XDS_Cls_PrivateData_Continue)) && byte2M != XDS_1Type_CGMS_A) ||
	        ((byte1M>= VBI_Non_XDS_Cls_Lower_Range) && (byte1M<= VBI_Non_XDS_Cls_Upper_Range)))
		{
			// Some other XDS packet is starting or continuing.  Put the CGMS-A packet
			// on the back burner:

			m_fIsXDSParsing = FALSE;
			m_fXDSParsingPaused = TRUE;
			return S_FALSE;
		}
	}
    if(m_fIsXDSParsing)
    {
         if(m_cXDSLoc + 2 > sizeof(m_rgXDSBuff))
        {
            m_fIsXDSParsing = FALSE;
			m_fXDSParsingPaused = FALSE;
            return S_FALSE;
        }
        m_rgXDSBuff[m_cXDSLoc++] = byte1M;
        m_rgXDSBuff[m_cXDSLoc++] = byte2M;

        m_crcXDS += byte1M;
        m_crcXDS += byte2M;
    }

    return S_FALSE;
}

        // mediaSample provided for timestamp, will work without it, but not a good idaa
HRESULT
CXDSCodecHelper::ResetToDontKnow(IMediaSample *  mediaSample)
{
					// clean up the state of the XDS Parser
    InitXDSParser();		// ??? should we kick of a don't know state here?

                    // clean up state in the decoder
    if(m_spXDSToRat)
        m_spXDSToRat->Init();

                    //
    PackedTvRating TvRatingDontKnow;
    HRESULT hr = PackTvRating(TvRat_SystemDontKnow, TvRat_LevelDontKnow, BfAttrNone, &TvRatingDontKnow);
	ASSERT(SUCCEEDED(hr));  // only reason for failures are bad args -- confirm none

            // store state, and kick off event
    if (SUCCEEDED(hr) && (TvRatingDontKnow != m_TvRating))
    {
        m_cUnratedChanged++;
        GoNewXDSRatings(mediaSample, TvRatingDontKnow);
    }

    if(true)
    {
                // treat channel change as CGMS-A off
        CComBSTR spBS(2);
		if (!spBS)
			return E_OUTOFMEMORY;

        spBS[0] = 0;            // 0,0,0,0 packet special to this code - indicates discontiuity
        spBS[1] = 0;
        GoNewXDSPacket(NULL, 0x0, 0x0, spBS);   // class/type (0,0) is our code for a discontinuity...
    }

    return S_OK;
}


        // sent on a broadcast TuneChanged event...
void
CXDSCodecHelper::DoTuneChanged()
{
                    // really want to do call ResetToDontKnow here,
                    // but don't have a media sample to give us a timestamp
                    // so instead, just clean up state in the decoder
    if(m_spXDSToRat)
        m_spXDSToRat->Init();

                    // for completeness, fire a broadcast event
                    //   saying we don't know the rating

    PackedTvRating TvRatingDontKnow;
    HRESULT hr = PackTvRating(TvRat_SystemDontKnow, TvRat_LevelDontKnow, BfAttrNone, &TvRatingDontKnow);
	ASSERT(SUCCEEDED(hr));  // only reason for failure are bad args so failure is a program bug
	if (SUCCEEDED(hr))
		GoNewXDSRatings(NULL, TvRatingDontKnow);

    // actually, probably get a discontinuity here which
    //  does same thing, but this is more fun (e.g. exact)
}



//  -------------------------------------------------------------------
//  IXDSCodec
//  -------------------------------------------------------------------

STDMETHODIMP
CXDSCodecHelper::get_XDSToRatObjOK(
    OUT HRESULT *pHrCoCreateRetVal
    )
{
    if(NULL == pHrCoCreateRetVal)
        return E_POINTER;

    {
        CAutoLock cLock(&m_cCritSec);    // lock these
        *pHrCoCreateRetVal = m_hrXDSToRatCoCreateRetValue;
    }
    return S_OK;
}


STDMETHODIMP
CXDSCodecHelper::put_CCSubstreamService(          // will return S_FALSE if unable to set
    IN long SubstreamMask
    )
{
    HRESULT hr = S_OK;

    if(!m_pPinCCDecoder)                   // can't change if connected
        return S_FALSE;

    if(0 != (SubstreamMask &
              ~(KS_CC_SUBSTREAM_SERVICE_CC1 |
                KS_CC_SUBSTREAM_SERVICE_CC2 |
                KS_CC_SUBSTREAM_SERVICE_CC3 |
                KS_CC_SUBSTREAM_SERVICE_CC4 |
                KS_CC_SUBSTREAM_SERVICE_T1 |
                KS_CC_SUBSTREAM_SERVICE_T2 |
                KS_CC_SUBSTREAM_SERVICE_T3 |
                KS_CC_SUBSTREAM_SERVICE_T4 |
                KS_CC_SUBSTREAM_SERVICE_XDS)))
        return E_INVALIDARG;

    return S_FALSE;             // can't change yet

    if(!FAILED(hr))
    {
        CAutoLock cLock(&m_cCritSec);    // lock these
        m_dwSubStreamMask = (DWORD) SubstreamMask;
    }

    DbgLog((LOG_TRACE, 3,  _T("CXDSCodecHelper:: put_CCSubstreamService : 0x%08x\n"),SubstreamMask));

    return hr;
}

STDMETHODIMP
CXDSCodecHelper::get_CCSubstreamService(
    OUT long *pSubstreamMask
    )
{
    if(NULL == pSubstreamMask)
        return E_POINTER;

  {
     CAutoLock cLock(&m_cCritSec);    // lock these
    *pSubstreamMask = m_dwSubStreamMask;
  }
    return S_OK;
}

            // TODO - need to add the TimeStamp here too

STDMETHODIMP
CXDSCodecHelper::GetContentAdvisoryRating(
    OUT PackedTvRating  *pRat,              // long
    OUT long            *pPktSeqID,
    OUT long            *pCallSeqID,
    OUT REFERENCE_TIME  *pTimeStart,        // time this sample started
    OUT REFERENCE_TIME  *pTimeEnd
    )
{
            // humm, should we allow NULL values here and simply not return data?
    if(NULL == pRat || NULL == pPktSeqID || NULL == pCallSeqID)
        return E_POINTER;

    if(NULL == pTimeStart || NULL == pTimeEnd)
        return E_POINTER;

    {
        CAutoLock cLock(&m_cCritSec);    // lock these
        *pRat       = m_TvRating;
        *pPktSeqID  = m_cTvRatPktSeq;
        *pCallSeqID = m_cTvRatCallSeq++;

        *pTimeStart = m_TimeStartRatPkt;
        *pTimeEnd   = m_TimeEndRatPkt;

        m_cRatingsGets++;
    }

    DbgLog((LOG_TRACE, 3,  _T("CXDSCodecHelper:: GetContentAdvisoryRating : Call %d, Seq %d/%d\n"),
        m_cRatingsGets, m_cTvRatPktSeq, m_cTvRatCallSeq-1 ));

    return S_OK;
}

        // GetXDSPacket is called via encrypter filters, usually after some event.
        //  It's up to the callee to detect stale packets
        //   (e.g. CGMS-A packet is older than 20 seconds.)
        //   if it ever makes this call in a non-callback manner.


STDMETHODIMP
CXDSCodecHelper::GetXDSPacket(
    OUT long           *pXDSClassPkt,       // ENUM EnXDSClass
    OUT long           *pXDSTypePkt,
    OUT BSTR           *pBstrXDSPkt,		
    OUT long           *pPktSeqID,
    OUT long           *pCallSeqID,
    OUT REFERENCE_TIME *pTimeStart,         // time this sample started
    OUT REFERENCE_TIME *pTimeEnd
    )
{
    HRESULT hr;
    if(NULL == pXDSClassPkt || NULL == pXDSTypePkt ||
       NULL == pBstrXDSPkt ||
       NULL == pPktSeqID || NULL == pCallSeqID)
        return E_POINTER;

    if(NULL == pTimeStart || NULL == pTimeEnd)
        return E_POINTER;

  {
        CAutoLock cLock(&m_cCritSec);    // lock these

        *pXDSClassPkt   = m_XDSClassPkt;
        *pXDSTypePkt    = m_XDSTypePkt;
        hr = m_spbsXDSPkt.CopyTo(pBstrXDSPkt);
        *pPktSeqID       = m_cXDSPktSeq;

        if(!FAILED(hr))
            *pCallSeqID = m_cXDSCallSeq++;
        else
            *pCallSeqID = -1;

        *pTimeStart = m_TimeStartXDSPkt;
        *pTimeEnd   = m_TimeEndXDSPkt;

        m_cXDSGets++;
  }

    DbgLog((LOG_TRACE, 3,  _T("CXDSCodecHelper:: GetXDSPacket : Call %d, Seq %d/%d\n"),
        m_cXDSGets, m_cXDSPktSeq, m_cXDSCallSeq-1 ));
    return hr;
}
